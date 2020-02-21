#include "sd_util.h"

bool initSD() {
  if (sd_lock) return false;
  Serial.print("SD Init: ");
  Serial.println(spi_lock);
  while (spi_lock)vTaskDelay(100);
  spi_lock = true;
  SD.end();
  sd_ok = false;
  if (sd_mount && SD.begin()) sd_ok = true;
  spi_lock = false;
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
  while (spi_lock)vTaskDelay(10);
  spi_lock = true;
  SD.end();
  spi_lock = false;
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
  while (spi_lock)vTaskDelay(100);
  spi_lock = true;
  float cardSize = SD.cardSize() / (1024.0 * 1024.0 * 1024.0);
  spi_lock = false;
  return cardSize;
}

float totalSD() {
  while (spi_lock)vTaskDelay(10);
  spi_lock = true;
  float totalSD = SD.totalBytes() / (1024.0 * 1024.0 * 1024.0);
  spi_lock = false;
  return totalSD;
}

float availableSD() {
  while (spi_lock)vTaskDelay(100);
  spi_lock = true;
  float availableSD = (SD.totalBytes() - SD.usedBytes()) / (1024.0 * 1024.0 * 1024.0);
  spi_lock = false;
  return availableSD;
}

float logSizeSD() {
  while (spi_lock)vTaskDelay(10);
  spi_lock = true;
  float logSize;
  if (LOG_SIZE_SD) logSize = (SD.usedBytes() / (1024.0 * 1024.0));
  else logSize = (sizeDir(SD, LOG_DIR, 0) / (1024.0 * 1024.0));
  spi_lock = false;
  return logSize;
}

uint64_t sizeDir(fs::FS &fs, const char * dirname, uint8_t levels) {
  Serial.printf("Sizing directory: %s\n\r", dirname);
  while (spi_lock)vTaskDelay(10);
  spi_lock = true;
  File root = fs.open(dirname);
  if (!root) {
    Serial.println("Failed to open directory");
    spi_lock = false;
    return 0;
  }
  if (!root.isDirectory()) {
    Serial.println("Not a directory");
    spi_lock = false;
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
  spi_lock = false;
  return totalSize;
}

uint64_t sizeFile(fs::FS &fs, const char * filename) {
  while (spi_lock)vTaskDelay(10);
  spi_lock = true;
  File file = fs.open(filename);
  if (!file) {
    Serial.println("Failed to open file for reading");
    spi_lock = false;
    return 0;
  }
  uint64_t file_size = file.size();
  file.close();
  spi_lock = false;
  return file_size;
}

uint64_t sizeFile(fs::FS &fs, String filename) {
  char filename_c[filename.length()];
  filename.toCharArray(filename_c, filename.length() + 1);
  return sizeFile(fs, filename_c);
}

String listLogDir(fs::FS &fs) {
  struct dirent *log_dir;
  DIR *pdir;
  struct stat filestat;
  const char * mountpoint = "/sd";
  const char * slash = "/";

  while (spi_lock)vTaskDelay(10);
  spi_lock = true;
  char * log_dir_temp = (char *)malloc(strlen(LOG_DIR) + strlen(mountpoint) + 1);
  if (!log_dir_temp) {
    spi_lock = false;
    return "";
  }
  sprintf(log_dir_temp, "%s%s", mountpoint, LOG_DIR);
  if ((pdir = opendir(log_dir_temp)) == NULL) {
    spi_lock = false;
    return "";
  }
  free(log_dir_temp);
  String dir = "";
  String comma = "";
  while ((log_dir = readdir(pdir)) != NULL) {
    char * file_temp = (char *)malloc(strlen(log_dir->d_name) + strlen(LOG_DIR) + strlen(mountpoint) + strlen(slash) + 1);
    if (!file_temp) {
      spi_lock = false;
      return "";
    }
    sprintf(file_temp, "%s%s%s%s", mountpoint, LOG_DIR, slash, log_dir->d_name);
    if (!stat(file_temp, &filestat)) {
      if (S_ISREG(filestat.st_mode)) {
        time_t t = filestat.st_mtime;
        struct tm * tmstruct = localtime(&t);
        dir += comma + "[\"" + String(log_dir->d_name) + "\"," + String(filestat.st_size) + ",\"" + timeToString(tmstruct) + "\"]";
        comma = ",";
      }
    }
    free(file_temp);
  }
  closedir(pdir);
  spi_lock = false;
  return dir;
}

void writeFile(fs::FS &fs, const char * path, const char * message) {
  Serial.printf("Writing file: %s\n", path);
  while (spi_lock)vTaskDelay(100);
  spi_lock = true;

  File file = fs.open(path, FILE_WRITE);
  if (!file) {
    Serial.println("Failed to open file for writing");
    spi_lock = false;
    return;
  }
  if (file.print(message)) {
    Serial.println("File written");
  } else {
    Serial.println("Write failed");
  }
  file.close();
  spi_lock = false;
}

void writeFile(fs::FS &fs, String path, String message) {
  char path_c[path.length()];
  char msg_c[message.length()];
  path.toCharArray(path_c, path.length() + 1);
  message.toCharArray(msg_c, message.length() + 1);
  writeFile(fs, path_c, msg_c);
}

bool appendFile(fs::FS &fs, const char * path, const char * message) {
  while (spi_lock)vTaskDelay(100);
  spi_lock = true;
  File file = fs.open(path, FILE_APPEND);
  if (!file) {
    spi_lock = false;
    return false;
  }
  if (file.print(message)) {
    file.close();
    spi_lock = false;
    return true;
  } else {
    spi_lock = false;
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
  while (spi_lock)vTaskDelay(10);
  spi_lock = true;
  if (fs.remove(path)) {
    spi_lock = false;
    return true;
  } else {
    spi_lock = false;
    return false;
  }
}

bool deleteFile(fs::FS &fs, String path) {
  char path_c[path.length()];
  path.toCharArray(path_c, path.length() + 1);
  return fs.remove(path_c);
}
