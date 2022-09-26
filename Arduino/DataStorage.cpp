/**
 * Sadi Wali September 2022
 * Last modified: September 25 2022
 * 
 * This class provides the means to store and read data stored on the microSD card. 
 * 
 **/

#include "DataStorage.h"

/* Initialize the class with chip select pin and file name. */
Storage::Storage(int CS, String filename): CS_PIN(CS), log_file_name(filename + FILE_EXT) {}

/* Attempt to initialize the SD card reader. */
bool Storage::init() {
  if (!SD.begin(CS_PIN)) return false;
  return true;
}

/* Once the SD card reader is initialized, attempt to open the log file. */
bool Storage::open_file() {
  // log file already open, do nothing
  if (log_file) return true;

  log_file = SD.open(log_file_name, FILE_WRITE);
  // could not open log file
  if (!log_file) return false;

  // file opened, check if new file
  if (log_file.size() == 0) {
    // brand new file, add headers
    String line = "DATE,TIME,MANUAL,INT_TIME,FRAME_AVG,AE,IS_SATURATED,IS_DARK,X,Y,Z,";

    // add the wavelengths to the header
    for (int i = MIN_WAVELENGTH; i <= MAX_WAVELENGTH; i += WAVELENGTH_STEPSIZE) {
      line.concat(String(i));
      line.concat(",");
    }
    // finally write the header string to file
    log_file.println(line);
  }

  return true;
}

/* Close the SD file. */
void Storage::close_file() {
  log_file.close();
}

/* Delete the SD file. */
bool Storage::delete_file() {
  if (SD.remove(String(LOG_FILENAME) + String(FILE_EXT))) {
    return true;
  } else {
    return false;
  }
}

/* Open the file, Write a line, then close the file. */
bool Storage::write_line(String * line) {
  if (!open_file()) return false;

  log_file.println( * line);

  close_file();

  return true;
}

/* Read a line given line number from the SD file. */
String Storage::get_line(unsigned int line) {
  if (!open_file()) return "";

  String line_to_ret = "";

  log_file.seek(0);
  char c;

  // read file until at start of chosen line
  for (unsigned int i = 0; i < (line - 1); i++) {
    c = log_file.read();
    if (c == '\n') {
      i++;
    }
  }

  // read that line, with a max line length
  for (unsigned int i = 0; i < MAX_LINE_LENGTH; i++) {
    c = log_file.read();
    line_to_ret += c;
    if (c == '\n') {
      break;
    }
  }

  close_file();

  return line_to_ret;
}

/* Return a new line every time this function is called.
 *  The line variable is only to set the point to read from.
 */
String Storage::read_line(unsigned int line, unsigned int buf_size) {
  String line_to_ret = "";
  char c;

  if (line == 0) log_file.seek(0);

  for (unsigned int i = 0; i < buf_size; i++) {
    c = log_file.read();
    if (c == '\n') {
      break;
    }
    line_to_ret += c;
  }

  return line_to_ret;
}
