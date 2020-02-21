////////////////// SD ////////////////////

#ifndef sd_util_h
#define sd_util_h

#include "Arduino.h"
#include <SD.h>
#include "FS.h"
#include "config.h"
#include "time.h"
#include "dirent.h"
#include "sys/stat.h"

extern bool sd_lock;
extern bool sd_ok;
extern bool sd_mount;
extern bool spi_lock;

extern String timeToString(struct tm * timestruct);

bool initSD();
bool insertSD();
bool ejectSD();
bool waitSDLock();
void lockSD();
void unlockSD();
float cardSize();
float totalSD();
float availableSD();
float logSizeSD();
uint64_t sizeDir(fs::FS &fs, const char * dirname, uint8_t levels);
uint64_t sizeFile(fs::FS &fs, const char * filename);
uint64_t sizeFile(fs::FS &fs, String filename);
String listLogDir(fs::FS &fs);
void writeFile(fs::FS &fs, const char * path, const char * message);
void writeFile(fs::FS &fs, String path, String message);
bool appendFile(fs::FS &fs, const char * path, const char * message);
bool appendFile(fs::FS &fs, String path, String message);
bool deleteFile(fs::FS &fs, const char * path);
bool deleteFile(fs::FS &fs, String path);

#endif
