#include "sd_util.h"

bool initSD() {
  if (sd_lock) return false;
  SD.end();
  sd_ok = false;
  if (sd_mount && SD.begin()) sd_ok = true;
  return sd_ok;
}

bool insertSD() {
  sd_mount = true;
  waitSDLock();
  initSD();
  return sd_ok;
}

bool ejectSD() {
  if (!waitSDLock()) return false;
  SD.end();
  sd_mount = false;
  sd_ok = false;
  sd_lock = false;
  return true;
}

bool waitSDLock() {
  unsigned long start = millis();
  while (sd_lock && ((millis() - start) < SD_LOCK_TIMEOUT)) vTaskDelay(100);
  return !sd_lock;
}

void lockSD() {
  sd_lock = true;
}

void unlockSD() {
  sd_lock = false;
}

float cardSize() {
  return SD.cardSize() / (1024.0 * 1024.0 * 1024.0);
}

float totalSD() {
  return SD.totalBytes() / (1024.0 * 1024.0 * 1024.0);
}

float availableSD() {
  return (SD.totalBytes() - SD.usedBytes()) / (1024.0 * 1024.0 * 1024.0);
}

float logSizeSD() {
  if (LOG_SIZE_SD) return (SD.usedBytes() / (1024.0 * 1024.0));
  else return (sizeDir(SD, LOG_DIR, 0) / (1024.0 * 1024.0));
}

uint64_t sizeDir(fs::FS &fs, const char * dirname, uint8_t levels) {
  Serial.printf("Sizing directory: %s\n\r", dirname);
  File root = fs.open(dirname);
  if (!root) {
    Serial.println("Failed to open directory");
    return 0;
  }
  if (!root.isDirectory()) {
    Serial.println("Not a directory");
    return 0;
  }
  uint64_t totalSize = 0;
  File file = root.openNextFile();
  unsigned long start = millis();
  while (file) {
    if (!file.isDirectory()) {
      totalSize += file.size();
    }
    file = root.openNextFile();
  }
  Serial.println("Sizing took: " + String((millis() - start) / 1000));
  return totalSize;
}

uint64_t sizeFile(fs::FS &fs, const char * filename) {
  File file = fs.open(filename);
  if (!file) {
    Serial.println("Failed to open file for reading");
    return 0;
  }
  uint64_t file_size = file.size();
  file.close();
  return file_size;
}

uint64_t sizeFile(fs::FS &fs, String filename) {
  char filename_c[filename.length()];
  filename.toCharArray(filename_c, filename.length() + 1);
  return sizeFile(fs, filename_c);
}

String listLogDir(fs::FS &fs) {
  File root = fs.open(LOG_DIR);
  if (!root) {
    return "";
  }
  if (!root.isDirectory()) {
    return "";
  }

  File file = root.openNextFile();
  String dir = "";
  String comma = "";
  long start = millis();
  while (file) {
    if (!file.isDirectory()) {
      time_t t = file.getLastWrite();
      struct tm * tmstruct = localtime(&t);
      dir += comma + "[\"" + String(file.name()) + "\"," + String(file.size()) + ",\"" + timeToString(tmstruct) + "\"]";
      comma = ",";
    }
    file = root.openNextFile();
  }
  Serial.println("Log Took: " + String((millis() - start) / 1000.0));
  return dir;
}

void writeFile(fs::FS &fs, const char * path, const char * message) {
  Serial.printf("Writing file: %s\n", path);

  File file = fs.open(path, FILE_WRITE);
  if (!file) {
    Serial.println("Failed to open file for writing");
    return;
  }
  if (file.print(message)) {
    Serial.println("File written");
  } else {
    Serial.println("Write failed");
  }
  file.close();
}

void writeFile(fs::FS &fs, String path, String message) {
  char path_c[path.length()];
  char msg_c[message.length()];
  path.toCharArray(path_c, path.length() + 1);
  message.toCharArray(msg_c, message.length() + 1);
  writeFile(fs, path_c, msg_c);
}

bool appendFile(fs::FS &fs, const char * path, const char * message) {
  File file = fs.open(path, FILE_APPEND);
  if (!file) {
    return false;
  }
  if (file.print(message)) {
    file.close();
    return true;
  } else {
    return false;
  }
}

bool appendFile(fs::FS &fs, String path, String message) {
  char path_c[path.length()];
  char msg_c[message.length()];
  path.toCharArray(path_c, path.length() + 1);
  message.toCharArray(msg_c, message.length() + 1);
  return appendFile(fs, path_c, msg_c);
}

bool deleteFile(fs::FS &fs, const char * path) {
  if (fs.remove(path)) {
    return true;
  } else {
    return false;
  }
}

bool deleteFile(fs::FS &fs, String path) {
  char path_c[path.length()];
  path.toCharArray(path_c, path.length() + 1);
  return fs.remove(path_c);
}
