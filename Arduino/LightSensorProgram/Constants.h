#ifndef CONSTANTS_H
#define CONSTANTS_H

// STORAGE
#define LOG_FILENAME "LOG1" // the log filename to write to
#define METADATA_FILENAME "METADATA" // the metadata file name
#define FILE_EXT ".CSV" // the log file extension
#define MAX_LINE_LENGTH 3000 // the maximum line length in file

// SERIAL COMMUNICATIONS
#define DEV_NAME_PREFIX "NSP" // the device name prefix
#define BAUDRATE 921600 // the device baudrate for serial communication

// DEVICE SETTINGS
#define DEF_CAPTURE_INTERVAL 60000 // how frequently to capture data in ms
#define SLEEP_DURATION 1000 // how many ms to go into deep sleep for in between wake checks
#define MIN_WAVELENGTH 340 // the minimum wavelength we are interested in
#define MAX_WAVELENGTH 1010 // the maximum wavelength we are interested in
#define DEF_FRAME_AVG 3 // how many frames to average into single reading
#define CAPTURE_PRECISION 18 // how many digits of precision to write (18 digits)
#define DEF_CALIBRATION_FACTOR 1 // the calibration factor for the particular sensor
#define MIN_ACCEPTABLE_Y 5 // the minimum acceptable Y value for reading (CIE1931)
#define STARTING_DARK_INT_TIME 500 // the starting integration time for correcting dark readings
#define MAX_INT_TIME 800 // the maximm integration time to use
// NIGHT MODE
#define ENABLE_NIGHTMODE true // night mode attempts to save battery by reducing readings when dark
#define MAX_DARK_READINGS 3 // how many sequential dark readings before nightmode engages?

// NSP SENSOR SETUP
#define SENSOR_MIN_WAVELENGTH 340 // the minimum sensing wavelength (for W1 sensor)
#define SENSOR_MAX_WAVELENGTH 1010 // the maximum sensing wavelength  (for W1 sensor)
#define WAVELENGTH_STEPSIZE 5 // the sensor wavelength resolution (5 nm for W1 sensor)

// PINS
#define SD_CS_PIN 4 // pin connected to SD CS
#define NSP_CS_PIN 5 // pin connected to NSP CS
#define NSP_RESET 28 // NSP reset pin
#define NSP_READY 29 // NSP ready pin

#endif
