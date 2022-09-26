/**
 * Sadi Wali August 2022
 * Last modified: September 07 2022
 * 
 * This open-source program is written for the Nordic nRF52840, and the NanoLambda NSP32m W1
 * to create a lightweight wearable spectral sensor that record data at a set interval on battery.
 * 
 * Pin layout used:
 * ------------------------------------
 *               NSP32      Sparkfun       
 * SPI                      nRF52840     
 * Signal        Pin          Pin        
 * ------------------------------------
 * Wakeup/Reset  RST          28    
 * SPI SSEL      SS           5    
 * SPI MOSI      MOSI         3    
 * SPI MISO      MISO         31  
 * SPI SCK       SCK          30   
 * Ready         Ready        29
 * ------------------------------------
 *               SD Card
 *               Pin
 * ------------------------------------
 * SPI SSEL      CS           4
 * SPI MOSI      DI           3
 * SPI MISO      DO           31
 * SPI SCK       SCK          30
 * 
 * 
 * 
 * Error codes:
 * ------------------------------------
 * 
 * Error         Number of
 *               flashes
 * ------------------------------------
 * SD card       1
 * Internal FS   2
 * 
 * 
 **/

#include "Adafruit_TinyUSB.h"                       // for serial communication on nrf52840

#include <SPI.h>                                    // Arduino SPI

#include <TimeLib.h>                                // Date and time

#include <ArduinoAdaptor.h>                         // for low-level interfacing with the NSP via Arduino

#include <NSP32.h>                                  // for high-level interfacing with the NSP module

#include "DataStorage.h"                            // SD Data storage module

#include "Constants.h"                              // constants

#include <Adafruit_LittleFS.h>                      // persistent data storage

#include <InternalFileSystem.h>                     // persistent data storage

using namespace NanoLambdaNSP32;
using namespace Adafruit_LittleFS_Namespace;

const unsigned int PinRst = NSP_RESET; // pin Reset
const unsigned int PinSS = NSP_CS_PIN; // NSP chip select pin
const unsigned int PinReady = NSP_READY; // pin Ready

// VARIABLES
int logging_interval = DEF_CAPTURE_INTERVAL; // the data logging interval
unsigned long last_collection_time = 0; // remember the last collection interval
unsigned long time_offset = 0; // how much time to offset by if device hung for long operations
bool recording = false; // is data capture paused?
String device_name = DEV_NAME_PREFIX; // the device name for easier identification
double calibration_factor = DEF_CALIBRATION_FACTOR; // the calibration factor
int int_time = 500; // default integration time for sensor           
int frame_avg = DEF_FRAME_AVG; // how many frames to average
bool ae = true; // use autoexposure?
unsigned int data_counter = 0; // how many data points collected and stored
int s_dark_readings = 0; // how many sequential dark readings?

char ser_buffer[32]; // the serial buffer
int read_index = 0; // the serial buffer read index

// OBJECTS
ArduinoAdaptor adaptor(PinRst, PinSS); // master MCU adaptor
NSP32 nsp32( & adaptor, NSP32::ChannelSpi); // NSP32 (using SPI channel)
Storage st(SD_CS_PIN, LOG_FILENAME); // the data storage object
Adafruit_LittleFS_Namespace::File file(InternalFS); // the metadata file stored in persistent flash

/* Get a reading from the NSP32 sensor */
void read_sensor(SpectrumInfo * info, int use_int_time = 0, int use_frame_avg = 3, bool use_ae = true) {
  // wakeup the sensor if sleeping
  nsp32.Wakeup();
  // get the spectrum data. Note that if ae is true, int_time is ignored
  nsp32.AcqSpectrum(0, use_int_time, use_frame_avg, use_ae);
  // wait until spectrum data completely collected
  while (nsp32.GetReturnPacketSize() <= 0) nsp32.UpdateStatus();
  // data has been collected, extract into info
  nsp32.ExtractSpectrumInfo(info);
}

/* Check if datapoint is too dark, okay, or too bright indicated by -1, 0, 1 respectively */
int validate_reading(SpectrumInfo infoS) {
  if (infoS.IntegrationTime == 1 || infoS.Y <= MIN_ACCEPTABLE_Y) return -1;
  if (infoS.IsSaturated == 1) return 1;
  return 0;
}

/* Take a manual or automatic measurement, and return the formatted datapoint line */
String take_measurement(bool manual_measurement = false) {
  SpectrumInfo infoS; // variable for storing the spectrum data

  bool dark_flag = true;
  bool too_dark = false;

  // modified settings to use in this session
  int mod_int_time = int_time;
  int mod_frame_avg = frame_avg;
  bool mod_ae = ae;

  while (true) {

    read_sensor( & infoS, mod_int_time, mod_frame_avg, mod_ae);
    // validate the datapoint
    int validation_res = validate_reading(infoS);

    if (validation_res == 1) {
      // too bright, nothing to do
      s_dark_readings = 0;
      break;

    } else if (validation_res == -1) {
      // too dark
      
      if (ENABLE_NIGHTMODE && s_dark_readings >= MAX_DARK_READINGS) {
        // too many dark readings in a series, accept this reading as-is
        too_dark = true;
        break;
      }

      // too dark even with int_time maxed
      if (mod_int_time >= MAX_INT_TIME) {
        too_dark = true;
        s_dark_readings++;
        break;
      }

      // find the correct exposure time, use half the frames to save time and power
      mod_ae = false;
      mod_frame_avg = frame_avg / 2;

      if (mod_frame_avg < 1) mod_frame_avg = 1;

      if (dark_flag) {
        // start with integration time set to 500
        dark_flag = false;
        mod_int_time = STARTING_DARK_INT_TIME;
        continue;
      }

      mod_int_time += 50;

    } else {
      // no issues, break out of loop
      break;
      s_dark_readings = 0;
    }
  }

  // format each line of CSV file into this line
  String line = format_line(infoS, manual_measurement, too_dark, mod_frame_avg, mod_ae);

  // Standby seems to clear SpectrumInfo, therefore call it after processing the data
  nsp32.Standby(0);

  // write the data to the SD card
  if (!st.write_line( & line)) error_state(1);

  data_counter++;

  update_memory();

  return line;
}

String format_line(SpectrumInfo infoS, bool manual_measurement, bool too_dark, int mod_frame_avg, int mod_ae) {
  String line = "";

  // 1. Which date was this data point collected on?
  line += String(day());
  line += "/";
  line += String(month());
  line += "/";
  line += String(year());
  line += ",";

  // 2. When was this data point collected?  
  line += String(hour());
  line += ":";
  line += String(minute());
  line += ":";
  line += String(second());
  line += ",";

  // 3. Was this recording triggered manually?
  line += String(manual_measurement);
  line += ",";

  // 4. What integration time was used?
  line += String(infoS.IntegrationTime);
  line += ",";

  // 5. What frame average number was used?
  line += String(mod_frame_avg);
  line += ",";

  // 6. Was auto-exposure used?
  line += String(mod_ae);
  line += ",";

  // 7. Was the reading saturated? A reading should not be saturated if ae is used.
  line += String(infoS.IsSaturated);
  line += ",";

  // 8. Was the lighting too dark when this reading took place? This reading should not be used.
  line += String(too_dark);
  line += ",";

  // 9. Then put in the CIE1931 values
  line += String(infoS.X);
  line += ",";
  line += String(infoS.Y);
  line += ",";
  line += String(infoS.Z);
  line += ",";

  // 10. Then put in the spectrum data. Sensor reads 340-1010 nm (inclusive) in 5 nm increments
  for (int i = ((MIN_WAVELENGTH - SENSOR_MIN_WAVELENGTH) / WAVELENGTH_STEPSIZE); i <= ((MAX_WAVELENGTH - SENSOR_MIN_WAVELENGTH) / WAVELENGTH_STEPSIZE); i++) {
    line += String(infoS.Spectrum[i] * calibration_factor, CAPTURE_PRECISION);
    line += ",";
  }

  return line;
}

/* Update the persistent storage */
void update_memory() {
  // overwrite the file
  String filename = "/" + String(METADATA_FILENAME) + String(FILE_EXT);
  InternalFS.remove(filename.c_str());
  if (file.open(filename.c_str(), FILE_O_WRITE)) {
    String writeline = device_name + "," + String(logging_interval) + "," + String(data_counter) + "," + String(calibration_factor);
    file.write(writeline.c_str(), strlen(writeline.c_str()));
    file.close();

  } else {
    // could not create file
    error_state(2);
  }
}

void read_memory() {
  String filename = "/" + String(METADATA_FILENAME) + String(FILE_EXT);
  file.open(filename.c_str(), FILE_O_READ);

  if (file) {
    uint32_t readlen;
    char sbuffer[64] = {
      0
    };
    readlen = file.read(sbuffer, sizeof(sbuffer));
    sbuffer[readlen] = '\0';
    String readline = String(sbuffer);

    int delimiters[3];
    int d_count = 0;

    for (int i = 0; i < readlen; i++) {
      // loop through the character array
      if (sbuffer[i] == ',') {
        delimiters[d_count] = i;
        d_count++;
      }
    }

    if (d_count != 3) {
      // the memory file is not as expected, re-create it
      file.close();
      update_memory();
      return;
    }

    device_name = readline.substring(0, delimiters[0]);
    logging_interval = readline.substring(delimiters[0] + 1, delimiters[1]).toInt();
    data_counter = readline.substring(delimiters[1] + 1, delimiters[2]).toInt();
    calibration_factor = readline.substring(delimiters[2] + 1).toFloat();

    file.close();
  } else {
    update_memory();
  }

}

/* Infinite loop LED to indicate an error. */
void error_state(int code) {
  while (true) {
    for (int i = 0; i < code; i++) {
      digitalWrite(7, HIGH);
      delay(250);
      digitalWrite(7, LOW);
      delay(250);
    }
    delay(500);
  }

}

/* Arduino setup function. */
void setup() {
  pinMode(PinReady, INPUT_PULLUP); // use pull-up for ready pin
  pinMode(7, OUTPUT); // onboard LED output
  digitalWrite(7, HIGH); // turn LED ON
  attachInterrupt(digitalPinToInterrupt(PinReady),
    PinReadyTriggerISR, FALLING); // enable interrupt for NSP READY
  // initialize serial port
  Serial.begin(BAUDRATE);
  while (!Serial) delay(10);

  // initialize the persistent storage
  InternalFS.begin();
  //  
  //  delay(1000);
  //  InternalFS.format();
  //  Serial.println("FORMAT COMPLETE");
  //  return;

  // read persistent flash storage into memory
  read_memory();

  // if the SD storage object is errored, go into error state
  if (!st.init()) error_state(1);

  // initialize NSP32
  nsp32.Init();
  nsp32.Standby(0);

  pause(true);
}

unsigned long paused_time = 0;
unsigned long pause_duration = 0;

void pause(bool do_pause) {
  if (do_pause) {
    // remember when we paused
    paused_time = millis();
    recording = false;
  } else {
    pause_duration = millis() - paused_time;
    recording = true;
  }
}

void sleep_until_capture() {
  //  if (!recording || timeStatus() == timeNotSet){
  if (!recording) {
    if (!Serial) delay(SLEEP_DURATION);
    return;
  }

  long time_diff = min((millis() - (last_collection_time + pause_duration)), logging_interval);

  unsigned long time_remaining = logging_interval - time_diff;

  if (!Serial) delay(min(time_remaining, SLEEP_DURATION));

  if (time_remaining <= 0) {
    last_collection_time = millis();
    pause_duration = 0;
    take_measurement(false);
  }

}

/* Arduino loop function */
void loop() {
  if (Serial) {
    // cable plugged in   
    digitalWrite(7, HIGH);

    bool end_of_line = false;

    while (Serial.available()) {
      char c = Serial.read();
      if (c == '\n') {
        end_of_line = true;
        ser_buffer[read_index] = '\0';
        read_index = 0;
        Serial.flush();
        break;
      } else {
        ser_buffer[read_index] = c;
        read_index++;
      }
    }

    if (end_of_line) {

      if (ser_buffer[0] == '0' && ser_buffer[1] == '0') {
        // 00: Toggle data capture while plugged in
        // TODO implement
        if (recording) {
          pause(true);
        } else {
          pause(false);
        }
        Serial.println("DATA");
        Serial.println(recording);
        Serial.println("OK");

      } else if (ser_buffer[0] == '0' && ser_buffer[1] == '1') {
        // 01: collect a data point manually
        String manual_data = take_measurement(true);
        Serial.println("DATA");
        Serial.println(manual_data);
        Serial.println("OK");

      } else if (ser_buffer[0] == '0' && ser_buffer[1] == '2') {
        // 02: export all data line by line

        // pause recording because this is a lengthy command
        pause(true);

        Serial.println("DATA");

        st.open_file();

        for (int i = 0; i < data_counter + 1; i++) {
          String line = st.read_line(i, MAX_LINE_LENGTH);
          Serial.println(line);
        }

        pause(false);

        Serial.println("OK");

        st.close_file();
      } else if (ser_buffer[0] == '0' && ser_buffer[1] == '3') {
        // 03: Delete the data logging file
        st.delete_file();

        device_name = DEV_NAME_PREFIX;
        logging_interval = DEF_CAPTURE_INTERVAL;
        data_counter = 0;
        calibration_factor = DEF_CALIBRATION_FACTOR;

        update_memory();

        Serial.println("OK");

      } else if (ser_buffer[0] == '0' && ser_buffer[1] == '4') {
        // 04: Set new collection interval

        String s_buf = String(ser_buffer);
        int new_logging_interval = s_buf.substring(s_buf.indexOf("_") + 1).toInt();
        logging_interval = new_logging_interval;
        update_memory();
        Serial.println("OK");

      } else if (ser_buffer[0] == '0' && ser_buffer[1] == '5') {
        // 05: Set date and time  
        String s_buf = String(ser_buffer);
        int y = s_buf.substring(2, 6).toInt();
        int mth = s_buf.substring(6, 8).toInt();
        int d = s_buf.substring(8, 10).toInt();

        int h = s_buf.substring(10, 12).toInt();
        int m = s_buf.substring(12, 14).toInt();
        int s = s_buf.substring(14, 16).toInt();

        setTime(h, m, s, d, mth, y);
        Serial.println("OK");

      } else if (ser_buffer[0] == '0' && ser_buffer[1] == '7') {
        // 07: Hello
        Serial.println("DATA");
        // send name
        Serial.println(device_name);
        // send interval
        Serial.println(logging_interval);
        // send status
        Serial.println(recording);
        // send # of entries
        Serial.println(data_counter);
        Serial.println("OK");

      } else if (ser_buffer[0] == '0' && ser_buffer[1] == '8') {
        // 08: Set device name
        String s_buf = String(ser_buffer);
        device_name = String(DEV_NAME_PREFIX) + String(s_buf.substring(s_buf.indexOf("_") + 1));
        update_memory();
        Serial.println("OK");

      } else if (ser_buffer[0] == '0' && ser_buffer[1] == '9') {
        // 09: Get device information
        Serial.println("DATA");
        String to_ret = "device_name: " + device_name + " data_points: " +
          String(data_counter) +
          " uptime: " + String(millis() / 60000, 8) +
          "m Logging interval: " +
          String(logging_interval) + "ms";
        Serial.println(to_ret);
        Serial.println("OK");

      } else if (ser_buffer[0] == '1' && ser_buffer[1] == '0') {
        // 10: Set NSP settings
        // they are sent like this: 10[ae:1 or 0][frame_avg:1 to 999][int_time:1 to 1000]
        String s_buf = String(ser_buffer);
        ae = (bool) s_buf.substring(2, 3).toInt();
        frame_avg = s_buf.substring(3, 6).toInt();
        int_time = s_buf.substring(6, 10).toInt();

        Serial.println("OK");

      } else if (ser_buffer[0] == '1' && ser_buffer[1] == '1') {
        // 11: Set calibration factor
        String s_buf = String(ser_buffer);
        double new_calibration_factor = s_buf.substring(s_buf.indexOf("_") + 1).toFloat();
        calibration_factor = new_calibration_factor;
        update_memory();
        Serial.println("OK");

      } else {
        Serial.println("Err '" + String(ser_buffer) + "'");
      }

    }

  }

  // hang for 15s or do a data capture when it is time
  sleep_until_capture();

}

/* NSP32m ready interrupt */
void PinReadyTriggerISR() {
  // make sure to call this function when receiving a ready trigger from NSP32
  nsp32.OnPinReadyTriggered();
}
