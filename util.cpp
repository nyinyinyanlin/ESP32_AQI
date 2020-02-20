/* UTILITY FUNCTIONS */

#include "util.h"

float mapf(float val, float in_min, float in_max, float out_min, float out_max) {
  return (val - in_min) * (out_max - out_min) / (in_max - in_min) + out_min;
}

void popArray(float arr[], int n) {
  for (int i = 0; i < n; i++) {
    arr[i] = 0.0;
  }
}

void popArray(int arr[], int n) {
  for (int i = 0; i < n; i++) {
    arr[i] = 0;
  }
}

void pushArray(float val, float arr[], int n) {
  for (int i = 0; i < n - 1; i++) {
    arr[i] = arr[i + 1];
  }
  arr[n - 1] = val;
}

void pushArray(int val, int arr[], int n) {
  for (int i = 0; i < n - 1; i++) {
    arr[i] = arr[i + 1];
  }
  arr[n - 1] = val;
}

String arrToStr(int arr[], int n) {
  String str = "";
  for (int i = 0; i < n - 1; i++) {
    str = str + arr[i] + ",";
  }
  return str + arr[n - 1];
}

String arrToStr(float arr[], int n) {
  String str = "";
  for (int i = 0; i < n - 1; i++) {
    str = str + String(arr[i], 2) + ",";
  }
  return str + String(arr[n - 1], 2);
}

float getAvg(int arr[], int n) {
  float avg = 0;
  for (int i = 0; i < n; i++) {
    avg += arr[i];
  }
  return (avg / n);
}

float getAvg(float arr[], int n) {
  float avg = 0;
  for (int i = 0; i < n; i++) {
    avg += arr[i];
  }
  return (avg / n);
}

int getLast(int arr[], int n) {
  return arr[n - 1];
}

float getLast(float arr[], int n) {
  return arr[n - 1];
}

///////////////////////// * PREFERENCES * ///////////////////


// refector "get" functions to include default value

Preferences preferences;

void putPreference(char* key, String value) {
  preferences.begin("setting", false);
  preferences.putString(key, value);
  preferences.end();
}

void putPreference(char* key, bool value) {
  preferences.begin("setting", false);
  preferences.putBool(key, value);
  preferences.end();
}

void putPreference(char* key, unsigned long value) {
  preferences.begin("setting", false);
  preferences.putULong(key, value);
  preferences.end();
}

void putPreference(char* key, unsigned int value) {
  preferences.begin("setting", false);
  preferences.putUInt(key, value);
  preferences.end();
}

String getPreferenceString(char* key) {
  preferences.begin("setting", false);
  String value = preferences.getString(key, "");
  preferences.end();
  return value;
}

bool getPreferenceBool(char* key) {
  preferences.begin("setting", false);
  bool value = preferences.getBool(key, false);
  preferences.end();
  return value;
}

unsigned long getPreferenceULong(char* key) {
  preferences.begin("setting", false);
  unsigned long value = preferences.getULong(key, 0);
  preferences.end();
  return value;
}

unsigned int getPreferenceUInt(char* key) {
  preferences.begin("setting", false);
  unsigned int value = preferences.getUInt(key, 0);
  preferences.end();
  return value;
}

void removePreference(char* key) {
  preferences.begin("setting", false);
  preferences.remove(key);
  preferences.end();
}
