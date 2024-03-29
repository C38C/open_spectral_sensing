/**
 * Sadi Wali August 2022
 * Last modified: December 16 2023
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

const unsigned int PinRst = NSP_RESET;              // pin Reset
const unsigned int PinSS = NSP_CS_PIN;              // NSP chip select pin
const unsigned int PinReady = NSP_READY;            // pin Ready

// VARIABLES
int logging_interval = DEF_CAPTURE_INTERVAL;        // the data logging interval
unsigned long last_collection_time = 0;             // remember the last collection interval
unsigned long time_offset = 0;                      // how much time to offset by if device hung for long operations
bool recording = false;                             // is data capture recording?
String device_name = DEV_NAME_PREFIX;               // the device name for easier identification
double calibration_factor = DEF_CALIBRATION_FACTOR; // the calibration factor
int int_time = 500;                                 // default integration time for sensor           
int frame_avg = DEF_FRAME_AVG;                      // how many frames to average
bool ae = true;                                     // use autoexposure?
unsigned int data_counter = 0;                      // how many data points collected and stored
int s_dark_readings = 0;                            // how many sequential dark readings?
unsigned long paused_time = 0;                      // when was the sensor paused
unsigned long pause_duration = 0;                   // how long was the sensor paused for?

char ser_buffer[32];                                // the serial buffer
int read_index = 0;                                 // the serial buffer read index

// OBJECTS
ArduinoAdaptor adaptor(PinRst, PinSS);              // master MCU adaptor
NSP32 nsp32( & adaptor, NSP32::ChannelSpi);         // NSP32 (using SPI channel)
Storage st(SD_CS_PIN, LOG_FILENAME);                // the data storage object
Adafruit_LittleFS_Namespace::File file(InternalFS); // the metadata file stored in persistent flash


/* Arduino setup function. */
void setup() {
  pinMode(PinReady, INPUT_PULLUP); // use pull-up for ready pin
  pinMode(7, OUTPUT); // onboard LED output
  digitalWrite(7, HIGH); // turn LED ON
  attachInterrupt(digitalPinToInterrupt(PinReady),
    PinReadyTriggerISR, FALLING); // enable interrupt for NSP READY
  // initialize serial port
  Serial.begin(BAUDRATE);

  // initialize the persistent storage
  InternalFS.begin();

  // read persistent flash storage into memory
  read_memory();

  // if the SD storage object is errored, go into error state
  if (!st.init()) error_state(1);

  // initialize NSP32
  nsp32.Init();
  nsp32.Standby(0);

  // if device is already recording on startup, that means power was lost
  if (recording) {
    String error_line = "POWER LOSS DETECTED";
    st.write_line(& error_line);
  }
  
}

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

  // count the number of dips to zero. A dark reading will have multiple dips
  int dip_count = 0;
  bool is_zero = false;
  
  // go through 400 - 700 nm, and count how many times the graph dips to 0
  for (int i = (400 - MIN_WAVELENGTH) / 5; i < (700 - MIN_WAVELENGTH) / 5; i++) {
     if (infoS.Spectrum[i] == 0) {
      if (!is_zero) {
        dip_count++;
        is_zero = true;
      }
     } else {
      is_zero = false;
     }
  }

  // combine the dip count with autoexposure malfunction with low CIE1931 Y value 
  if (infoS.IntegrationTime == 1 || infoS.Y <= MIN_ACCEPTABLE_Y || dip_count >= 2) {
    // too dark
    return -1;
  } else if (infoS.IsSaturated == 1) {
    // too bright
    return 1;
  } else {
    // just right
    return 0;
  }
}

/* Take a manual or automatic measurement, and return the formatted datapoint line */
String take_measurement(bool manual_measurement = false) {
  SpectrumInfo infoS; // variable for storing the spectrum data

  bool dark_flag = true;
  bool too_dark = false;
  bool second_attempt = false;

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
        // too many dark readings in a series
        
        if (!second_attempt) {
          // this was the first attempt, set int_time to 500, and try again
          mod_ae = false;
          second_attempt = true;
          mod_int_time = STARTING_DARK_INT_TIME;
          continue;
          
        } else {
          // this was the second attempt, environment still too dark so don't continue
          too_dark = true;
          break;
          
        }
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
        // this flag is used to set integration time to 500 only once
        dark_flag = false;
        mod_int_time = STARTING_DARK_INT_TIME;
        continue;
      }

      mod_int_time += 100;

    } else {
      // no issues, break out of loop 
      s_dark_readings = 0;
      break;
    }
  }

  // format each line of CSV file into this line
  String line = format_line(infoS, manual_measurement, too_dark, mod_frame_avg, mod_ae);

  // Standby seems to clear SpectrumInfo, therefore call it after processing the data
  nsp32.Standby(0);

  // write the data to the SD card, if there is an error, try to restart SD
  while (!st.write_line( & line)) {
    digitalWrite(7, HIGH);
    // wait 1 second, and try initialize SD again
    delay(1000);

    if (st.init()) {
      // write the line
      st.write_line(& line);
      // write that there was an error
      String error_line = "DATA LOGGING ERROR";
      st.write_line(& error_line);
      digitalWrite(7, LOW);
      // break out of the loop
      break;
    }
  }

  data_counter++;
  update_memory();

  return line;
}

String format_line(SpectrumInfo infoS, bool manual_measurement, bool too_dark, int mod_frame_avg, int mod_ae) {
  String line = "";

  // 1. Which date was this data point collected on?
  if (day() < 10) line += "0";
  line += String(day());
  line += "/";
  if (month() < 10) line += "0";
  line += String(month());
  line += "/";
  line += String(year());
  line += ",";

  // 2. When was this data point collected?  
  if (hour() < 10) line += "0";
  line += String(hour());
  line += ":";
  if (minute() < 10) line += "0";
  line += String(minute());
  line += ":";
  if (second() < 10) line += "0";
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

  // 7. What was the light quality? A dark reading will be -1, a good reading is 0, and a saturated reading is 1'
  if (infoS.IsSaturated) {
    line += "1";
  } else if (!infoS.IsSaturated && too_dark) {
    line += "-1";
  } else if (!infoS.IsSaturated && !too_dark) {
    line += "0";
  } else {
    line += "-2";
  }
  line += ",";

  // 8. Then put in the CIE1931 values
  line += String(infoS.X, CIE_PRECISION);
  line += ",";
  line += String(infoS.Y, CIE_PRECISION);
  line += ",";
  line += String(infoS.Z, CIE_PRECISION);
  line += ",";

  // 9. Then put in the spectrum data. Sensor reads 340-1010 nm (inclusive) in 5 nm increments
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
    String writeline = device_name + "," + String(logging_interval) + "," + String(data_counter) + "," + String(calibration_factor) + "," + String(recording);
    file.write(writeline.c_str(), strlen(writeline.c_str()));
    file.close();

  } else {
    // could not create file
    error_state(2);
  }
}

/* Read the persistent data in the nRF52840's internal flash */
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

    int delimiters[4];
    int d_count = 0;

    for (int i = 0; i < readlen; i++) {
      // loop through the character array
      if (sbuffer[i] == ',') {
        delimiters[d_count] = i;
        d_count++;
      }
    }

    if (d_count != 4) {
      // the memory file is not as expected, re-create it
      file.close();
      update_memory();
      return;
    }

    device_name = readline.substring(0, delimiters[0]);
    logging_interval = readline.substring(delimiters[0] + 1, delimiters[1]).toInt();
    data_counter = readline.substring(delimiters[1] + 1, delimiters[2]).toInt();
    calibration_factor = readline.substring(delimiters[2] + 1, delimiters[3]).toFloat();
    recording = readline.substring(delimiters[3] + 1) == "1";

    file.close();
  } else {
    update_memory();
  }

}

/* Infinite loop LED to indicate an error. */
void error_state(int code) {
  while (true) {
    Serial.println("Error " + String(code));
    for (int i = 0; i < code; i++) {
      digitalWrite(7, HIGH);
      delay(250);
      digitalWrite(7, LOW);
      delay(250);
    }
    delay(500);
  }
}

/* Pause the sensor's logging if provided parameter is True, else resume */
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

/* Sleep the device until it is time to capture next data point. 
 * If time to next data point is greater than SLEEP_DURATION, sleep until SLEEP_DURATION.
*/
void sleep_until_capture() {

  // check if timed start or stop reached

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

        if (recording) {
          pause(true);
        } else {
          pause(false);
        }

        update_memory();
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
        // 02: stream entire log file (export)

        // pause recording because this is a lengthy command
        bool was_recording = recording;
        if (was_recording) pause(true);

        Serial.println("DATA");
        Serial.println(st.get_size());

        if (st.open_file()) {
          st.seek_to(0);
          while (st.file_available() && Serial) {
            byte b = st.read_byte();
            Serial.write(b);
          }
        } else {
          Serial.println("ERR");
        }
        
        if (was_recording) pause(false);
        
        st.close_file();
        
        // do not send \r\n when streaming
        Serial.write("OK");

        
      } else if (ser_buffer[0] == '0' && ser_buffer[1] == '3') {
        // 03: Reset the device
        st.delete_file();

        device_name = DEV_NAME_PREFIX;
        logging_interval = DEF_CAPTURE_INTERVAL;
        data_counter = 0;
        calibration_factor = DEF_CALIBRATION_FACTOR;
        recording = false;

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
        device_name = String(DEV_NAME_PREFIX) + "_" + String(s_buf.substring(s_buf.indexOf("_") + 1));
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
        // they are sent like this: 10[ae:1 or 0][frame_avg:1 to 9999][int_time:1 to 1000]
        String s_buf = String(ser_buffer);
        ae = (bool) s_buf.substring(2, 3).toInt();
        frame_avg = s_buf.substring(3, 6).toInt();
        int_time = s_buf.substring(6).toInt();

        Serial.println("OK");

      } else if (ser_buffer[0] == '1' && ser_buffer[1] == '1') {
        // 11: Set calibration factor
        String s_buf = String(ser_buffer);
        double new_calibration_factor = s_buf.substring(s_buf.indexOf("_") + 1).toFloat();
        calibration_factor = new_calibration_factor;
        update_memory();
        Serial.println("OK");
        
      } else if (ser_buffer[0] == '1' && ser_buffer[1] == '2') {
      // 12: Start recording
        pause(false);
        Serial.println("OK");
        
      } else if (ser_buffer[0] == '1' && ser_buffer[1] == '3') {
        // 12: Stop recording
        pause(true);
        Serial.println("OK");
        
      } else if (ser_buffer[0] == '1' && ser_buffer[1] == '4') { 
        // 14: Delete storage only
        st.delete_file();
        data_counter = 0;
        update_memory();
        Serial.println("OK");
        
      } else if (ser_buffer[0] == '1' && ser_buffer[1] == '5') {
        // 15: sync data
        
        // which byte do we start with
        String s_buf = String(ser_buffer);
        unsigned long start_pointer = atol(s_buf.substring(s_buf.indexOf("_") + 1).c_str());

        if (st.open_file()) {
          // open the file
          
          Serial.println("DATA");
    
          unsigned long file_size = st.get_size();

          if (file_size < start_pointer) {
            // computer sync file larger than device
            Serial.println(-1);
            return;
          }
          
          Serial.println(file_size - start_pointer);
          
          st.seek_to(start_pointer);
          
          while (st.file_available()) {
            byte b = st.read_byte();
            Serial.write(b);
          }

          st.close_file();
        } else {
          Serial.println("ERR");
        }

        Serial.println("OK");      
        
      } else if (ser_buffer[0] == '1' && ser_buffer[1] == '7') {
        // 17: Set start time

        // what date and time to start at
        String s_buf = String(ser_buffer);
        String start_date = s_buf.substring(s_buf.indexOf("_") + 1);
 
      } else if (ser_buffer[0] == '1' && ser_buffer[1] == '8') {
        // 18: Set stop time

      } else {
        Serial.println("Err '" + String(ser_buffer) + "'");
      }
    }
  }
  
  // if time is not set, or is not recording, do not go to sleep and (do not capture)
  if (timeStatus() == timeNotSet || !recording) {
    delay(10);
  } else {
    // hang for set duration or do a data capture when it is time
    digitalWrite(7, LOW);
    sleep_until_capture();
  }

}

/* NSP32m ready interrupt */
void PinReadyTriggerISR() {
  // make sure to call this function when receiving a ready trigger from NSP32
  nsp32.OnPinReadyTriggered();
}
