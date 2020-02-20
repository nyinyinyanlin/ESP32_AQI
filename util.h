/* UTILITY FUNCTIONS */

#ifndef util_h
#define util_h

#include "Arduino.h"
#include <Preferences.h>

// basic utils
float mapf(float val, float in_min, float in_max, float out_min, float out_max);
void popArray(float arr[], int n);
void popArray(int arr[], int n);
void pushArray(float val, float arr[], int n);
void pushArray(int val, int arr[], int n);
String arrToStr(int arr[], int n);
String arrToStr(float arr[], int n);
float getAvg(int arr[], int n);
float getAvg(float arr[], int n);
int getLast(int arr[], int n);
float getLast(float arr[], int n);

// preference utils
// refector "get" functions to include default value
extern Preferences preferences;
void putPreference(char* key, String value);
void putPreference(char* key, bool value);
void putPreference(char* key, unsigned long value);
void putPreference(char* key, unsigned int value);
String getPreferenceString(char* key);
bool getPreferenceBool(char* key);
unsigned long getPreferenceULong(char* key);
unsigned int getPreferenceUInt(char* key);
void removePreference(char* key);

#endif
