#include <Arduino.h>
#include <WiFi.h>
#include <AsyncTCP.h>
#include <WiFiUdp.h>
#include <ArduinoOTA.h>
#include <ESPmDNS.h>
#include <ESPAsyncWebServer.h>
#include <Wire.h>
#include <SPI.h>
#include <LoRa.h>
#include <Adafruit_Sensor.h>
#include "driver/ledc.h"
#include "Adafruit_BME680.h"
#include "SPIFFS.h"
#include <RTClib.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include "config.h"
#include "graphic.h"
#include "util.h"
#include "sd_util.h"
#include "soc/soc.h"
#include "soc/rtc_cntl_reg.h"

const char* ntpServer = "pool.ntp.org";
const long  gmtOffset_sec = 23400;
const int   daylightOffset_sec = 0;
struct tm timeinfo;

/*
  struct tm
  {
  int    tm_sec;   //   Seconds [0,60].
  int    tm_min;   //   Minutes [0,59].
  int    tm_hour;  //   Hour [0,23].
  int    tm_mday;  //   Day of month [1,31].
  int    tm_mon;   //   Month of year [0,11].
  int    tm_year;  //   Years since 1900.
  int    tm_wday;  //   Day of week [0,6] (Sunday =0).
  int    tm_yday;  //   Day of year [0,365].
  int    tm_isdst; //   Daylight Savings flag.
  }
*/



//define the pins used by the transceiver module
//change the pin numbers
/*
#define ss 15
#define rst 5
#define dio0 2
*/




const uint8_t channel         = 0;
const uint8_t numberOfBits    = 8; //LEDC_TIMER_15_BIT;
const uint32_t frequency      = 1000;
const uint32_t fadeTimeSeconds = 86400;
const ledc_mode_t speed_mode  = LEDC_HIGH_SPEED_MODE;

byte led_status_mode = LED_STATUS_WARMUP;
bool led_phase = false;
long led_tick = 0;

long sntp_start = 0;
bool warmup_over = false;
bool MDNS_STATUS = false;
bool sntp_ok = false;
bool sd_ok = false;
bool sd_lock = false;
bool sd_mount = true;
bool display_ok = false;
bool i2c_lock = false;
bool spi_lock = false;
bool wifi_mode = WIFISTA_MODE;

HardwareSerial ndir(1);
HardwareSerial sds011(2);
Adafruit_BME680 bme;

TaskHandle_t statusLedTask;
TaskHandle_t oledDisplayTask;
TaskHandle_t rtcTask;
TaskHandle_t dataRecordTask;
TaskHandle_t uploadTask;
TaskHandle_t logTask;
TaskHandle_t bme680Task;
TaskHandle_t ndirTask;
TaskHandle_t sds011Task;

AsyncWebServer server(80);
RTC_DS3231 rtc;
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

char daysOfTheWeek[7][12] = {"Sunday", "Monday", "Tuesday", "Wednesday", "Thursday", "Friday", "Saturday"};

const char* default_sta_ssid    = "Nyi Nyi Nyan Tun";
const char* default_ap_ssid    = "nyinyitun";

const String deviceId = "aq_sense01";

byte cmd[9] = {0xFF, 0x01, 0x86, 0x00, 0x00, 0x00, 0x00, 0x00, 0x79};
byte co2res[7];
byte ppmres[8];

float hum_weighting = 0.25; // so hum effect is 25% of the total air quality score
float gas_weighting = 0.75; // so gas effect is 75% of the total air quality score

float hum_score, gas_score;
float gas_reference = 250000;
float hum_reference = 40;
int getgasreference_count = 0;

float co2_avg = 0;
float pm25_avg = 0;
float pm10_avg = 0;
float hum_avg = 0;
float temp_avg = 0;
float pres_avg = 0;
float iaq_avg = 0;
float vbat_avg = 0;

int co2[MOV_AVG_WDW];
int pm25[MOV_AVG_WDW];
int pm10[MOV_AVG_WDW];
int hum[MOV_AVG_WDW];
int temp[MOV_AVG_WDW];
int pres[MOV_AVG_WDW];
int iaq[MOV_AVG_WDW];
float vbat[MOV_AVG_WDW];

int * pm10Rec;
int * pm25Rec;
int * humRec;
int * tempRec;
int * presRec;
int * iaqRec;
int * co2Rec;
float * vbatRec;

////////////////////////////////// * TEMPLATE PROCESSORS * /////////////////////////////////

String device_processor(const String& var) {
  if (var == "DEVICE_ID") {
    return deviceId;
  } else if (var == "FIRMWARE_VERSION") {
    return "FGHGK.153";
  } else if (var == "CONNECTIVITY_TYPE") {
    return "Wifi";
  } else if (var == "WIFI_MODE") {
    if ((wifi_mode == WIFISTA_MODE) && (WiFi.status() == WL_CONNECTED)) {
      return "Station";
    } else if (wifi_mode == WIFIAP_MODE) {
      return "Access Point";
    } else {
      return "Not Connected";
    }
  } else if (var == "IP_ADDRESS") {
    if ((wifi_mode == WIFISTA_MODE) && (WiFi.status() == WL_CONNECTED)) {
      return WiFi.localIP().toString();
    } else if (wifi_mode == WIFIAP_MODE) {
      return WiFi.softAPIP().toString();
    } else {
      return "0.0.0.0";
    }
  } else if (var == "MAC_ADDRESS") {
    return WiFi.macAddress();
  } else if (var == "MDNS_ADDRESS") {
    return "esp32.local";
  } else if (var == "PM25_STATUS") {
    if (getLast(pm25, MOV_AVG_WDW) == 0) {
      return "false";
    } else {
      return "true";
    }
  } else if (var == "PM10_STATUS") {
    if (getLast(pm10, MOV_AVG_WDW) == 0) {
      return "false";
    } else {
      return "true";
    }
  } else if (var == "TEMPERATURE_STATUS") {
    if (getLast(temp, MOV_AVG_WDW) == 0) {
      return "false";
    } else {
      return "true";
    }
  } else if (var == "HUMIDITY_STATUS") {
    if (getLast(hum, MOV_AVG_WDW) == 0) {
      return "false";
    } else {
      return "true";
    }
  } else if (var == "PRESSURE_STATUS") {
    if (getLast(pres, MOV_AVG_WDW) == 0) {
      return "false";
    } else {
      return "true";
    }
  } else if (var == "VOC_STATUS") {
    if (getLast(iaq, MOV_AVG_WDW) == 0) {
      return "false";
    } else {
      return "true";
    }
  } else if (var == "CO2_STATUS") {
    if (getLast(co2, MOV_AVG_WDW) == 0) {
      return "false";
    } else {
      return "true";
    }
  } else if (var == "BATTERY_STATUS") {
    return getChargeStatus();
  } else if (var == "BATTERY_PERCENTAGE") {
    float bat = getAvg(vbat, MOV_AVG_WDW);
    return String(getBatteryPercentage(bat, 2.9, 4.1));
  } else if (var == "BATTERY_VOLTAGE") {
    return String(getAvg(vbat, MOV_AVG_WDW), 1);
  } else if (var == "TOTAL_HEAP") {
    return String(ESP.getHeapSize());
  } else if (var == "AVAILABLE_HEAP") {
    return String(ESP.getFreeHeap());
  } else if (var == "SNTP_TIME") {
    if (handleSNTP()) {
      String t = getSntpIsoTime();
      Serial.println(t);
      return t;
    } else {
      return "Unavailable";
    }
  } else if (var == "SD_LOG") {
    if (getPreferenceBool("sd_log")) {
      return "true";
    } else {
      return "false";
    }
  } else if (var == "FILE_PREFIX") {
    String prefix = getPreferenceString("file_prefix");
    if (prefix.length() > 0) {
      return prefix;
    } else {
      return deviceId;
    }
  } else if (var == "CARD_SIZE") {
    if (sd_ok) {
      return String(cardSize());
    } else {
      return String(0);
    }
  } else if (var == "TOTAL_SD") {
    if (sd_ok) {
      return String(totalSD());
    } else {
      return String(0);
    }
  } else if (var == "AVAILABLE_SD") {
    if (sd_ok) {
      return String(availableSD());
    } else {
      return String(0);
    }
  } else if (var == "LOG_SIZE_SD") {
    if (sd_ok) {
      return String(logSizeSD());
    } else {
      return String(0);
    }
  } else if (var == "LOG_SPLIT_SIZE") {
    unsigned long split = getPreferenceULong("log_split_size");
    if (!split) split = SPLIT_SIZE;
    return String(split);
  } else if (var == "SD_INSERTED") {
    if (sd_ok) {
      return "true";
    } else {
      return "false";
    }
  } else if (var == "LOG_INTERVAL") {
    return String(SD_LOG_INTERVAL / 1000.0);
  }
  return String();
}

String wifi_processor(const String & var) {
  if (var == "STA_SSID") {
    String ssid = getPreferenceString("sta_ssid");
    if (ssid.length() > 0) {
      return ssid;
    } else {
      return default_sta_ssid;
    }
  } else if (var == "STA_PASSWORD") {
    return getPreferenceString("sta_password");
  } else if (var == "AP_SSID") {
    String ssid = getPreferenceString("ap_ssid");
    if (ssid.length() > 0) {
      return ssid;
    } else {
      return default_ap_ssid;
    }
  } else if (var == "AP_PASSWORD") {
    return getPreferenceString("ap_password");
  } else if (var == "") {
    return String();
  }
  return String();
}

String api_processor(const String & var)
{
  if (var == "API_URL") {
    String api_url = getPreferenceString("api_url");
    return api_url;
    /*
      if (api_url.length()>0) {
      return api_url;
      } else {
      return default_api_url;
      }*/
  } else if (var == "API_KEY") {
    String api_key = getPreferenceString("api_key");
    return api_key;
    /*if (api_key.length()>0) {
      return api_key;
      } else {
      return default_api_key;
      }*/
  }
  return String();
}

String data_processor(const String & var) {
  if (var == "PM10_CURRENT") {
    return String(getAvg(pm10, MOV_AVG_WDW));
  } else if (var == "PM10_RECORD") {
    return arrToStr(pm10Rec, RECORD_SIZE);
  } else  if (var == "PM25_CURRENT") {
    return String(getAvg(pm25, MOV_AVG_WDW));
  } else if (var == "PM25_RECORD") {
    return arrToStr(pm25Rec, RECORD_SIZE);
  } else  if (var == "TEMPERATURE_CURRENT") {
    return String(getAvg(temp, MOV_AVG_WDW));
  } else if (var == "TEMPERATURE_RECORD") {
    return arrToStr(tempRec, RECORD_SIZE);
  } else  if (var == "PRESSURE_CURRENT") {
    return String(getAvg(pres, MOV_AVG_WDW) / 100.0);
  } else if (var == "PRESSURE_RECORD") {
    return arrToStr(presRec, RECORD_SIZE);
  } else  if (var == "HUMIDITY_CURRENT") {
    return String(getAvg(hum, MOV_AVG_WDW));
  } else if (var == "HUMIDITY_RECORD") {
    return arrToStr(humRec, RECORD_SIZE);
  } else  if (var == "VOC_CURRENT") {
    return String(getAvg(iaq, MOV_AVG_WDW));
  } else if (var == "VOC_RECORD") {
    return arrToStr(iaqRec, RECORD_SIZE);
  } else  if (var == "CO2_CURRENT") {
    return String(getAvg(co2, MOV_AVG_WDW));
  } else if (var == "CO2_RECORD") {
    return arrToStr(co2Rec, RECORD_SIZE);
  } else  if (var == "BATTERY_CURRENT") {
    return String(getAvg(vbat, MOV_AVG_WDW));
  } else if (var == "BATTERY_RECORD") {
    return arrToStr(vbat, RECORD_SIZE);
  }
}

String log_processor(const String & var) {
  if (var == "LOG_FILES" && sd_ok) {
    Serial.println("Log Processor");
    return listLogDir(SD);
  }
  return String();
}

////////////////////////////// * FUNCTIONS * ///////////////////////////////////

void logCode(void * parameter) {
  Serial.print("Log Code running on core ");
  Serial.println(xPortGetCoreID());
  for (;;) {
    vTaskDelay(SD_LOG_INTERVAL);
    if (warmup_over) {
      Serial.println("[SD LOG]");
      Serial.println("==========");
      String timestamp = "";
      // timestamp,pm10,pm25,temp,pres,rhum,iaq,co2
      String log_data = "," + String(getAvg(pm10, MOV_AVG_WDW), 2) + "," + String(getAvg(pm25, MOV_AVG_WDW), 2) + "," + String(getAvg(temp, MOV_AVG_WDW), 2) + "," + String(getAvg(pres, MOV_AVG_WDW) / 100.0, 2) + "," + String(getAvg(hum, MOV_AVG_WDW), 2) + "," + String(getAvg(iaq, MOV_AVG_WDW), 2) + "," + String(getAvg(co2, MOV_AVG_WDW), 2);
      if (sntp_ok) timestamp = getSntpIsoTime();
      if (getPreferenceBool("sd_log") && waitSDLock() && initSD()) {
        lockSD();
        Serial.println("LOG: Logging");
        String prefix = getPreferenceString("file_prefix");
        if (prefix.length() == 0) {
          prefix = deviceId;
        }
        unsigned int count = 0;
        String postfix;
        if (sntp_ok) {
          postfix = getFileTimestamp();
          if (postfix.length() == 0) {
            postfix = getSntpFilename();
            setFileTimestamp(postfix);
          }
          log_data = timestamp + log_data;
        } else {
          count = getFileCounter(); // *add default value
          postfix = String(count);
        }
        String filename = String(LOG_DIR)  + "/" + String(prefix) + "_" + String(postfix) + ".csv";
        Serial.println("File Name: " + filename);
        Serial.println("Data: " + log_data);
        while (spi_lock)vTaskDelay(100);
        spi_lock = true;
        if (SD.exists(filename)) {
          spi_lock = false;
          if (appendFile(SD, filename, log_data + "\n")) {
            Serial.println("LOG: Append logged");
            uint64_t file_size = sizeFile(SD, filename);
            if (file_size && (file_size >= SPLIT_SIZE)) {
              Serial.println("LOG: File Split");
              if (sntp_ok) setFileTimestamp(getSntpFilename());
              else setFileCounter(count + 1);
            }
          } else {
            Serial.println("LOG: Log failed");
          }
          spi_lock = false;
        } else {
          spi_lock = false;
          Serial.println("LOG: New file logged");
          String header = "time,pm10,pm25,temperature,pressure,relative humidity,iaq,co2\n";
          writeFile(SD, filename, header + log_data + "\n");
        }
        unlockSD();
        spi_lock = false;
      } else {
        Serial.println("LOG: Not logging");
        spi_lock = false;
      }
    }
  }
}

void uploadCode(void * parameter) {
  Serial.print("Upload Code running on core ");
  Serial.println(xPortGetCoreID());
  for (;;) {
    vTaskDelay(SEND_INTERVAL);
    Serial.println("[INFORMATION]");
    Serial.println("==========");
    Serial.print("PM10: ");
    Serial.print(getAvg(pm10, MOV_AVG_WDW));
    Serial.println("ug/m ^ 3");
    Serial.print("PM25: ");
    Serial.print(getAvg(pm25, MOV_AVG_WDW));
    Serial.println("ug/m ^ 3");
    Serial.print("Temperature: ");
    Serial.print(getAvg(temp, MOV_AVG_WDW));
    Serial.println("*C");
    Serial.print("Pressure: ");
    Serial.print(getAvg(pres, MOV_AVG_WDW) / 100.0);
    Serial.println("hPa");
    Serial.print("Humidity: ");
    Serial.print(getAvg(hum, MOV_AVG_WDW));
    Serial.println(" % ");
    Serial.print("CO2: ");
    Serial.print(getAvg(co2, MOV_AVG_WDW));
    Serial.println("PPM");
    Serial.print("IAQ(VOC): ");
    Serial.print(getAvg(iaq, MOV_AVG_WDW));
    Serial.println(" % ");
    Serial.print("Battery: ");
    Serial.print(getAvg(vbat, MOV_AVG_WDW));
    Serial.println("V");
    Serial.println();
    if (!warmup_over) {
      if (millis() > WARMUP_INTERVAL) {
        Serial.println("[WARM UP]");
        Serial.println("==========");
        Serial.println("Status: Warm Up Finished");
        Serial.println();
        warmup_over = true;
        setLedStatus(LED_STATUS_OK);
      }
    }
    if (warmup_over && wifi_mode == WIFISTA_MODE && WiFi.status() == WL_CONNECTED) {
      sendData(pm25, pm10, co2, temp, pres, hum, iaq, vbat, MOV_AVG_WDW);
    }
  }
}

void statusLedCode(void * parameter) {
  Serial.print("Status LED running on core ");
  Serial.println(xPortGetCoreID());
  ledcSetup(channel, frequency, numberOfBits);
  ledcAttachPin(LED_PIN, channel);
  byte vdelay;
  for (;;) {
    if (wifi_mode == WIFIAP_MODE) vdelay = 50;
    else vdelay = 10;
    for (byte i = 0; i < 255; i = i + 5) {
      ledcWrite(channel, i);
      vTaskDelay(vdelay);
    }
    for (byte i = 255; i > 0; i = i - 5) {
      ledcWrite(channel, i);
      vTaskDelay(vdelay);
    }
  }
}

void dataRecordCode(void * parameter) {
  Serial.print("Data Record running on core ");
  Serial.println(xPortGetCoreID());
  for (;;) {
    vTaskDelay(RECORD_INTERVAL);
    if (warmup_over) {
      Serial.println("[GRAPH DATA RECORD]");
      Serial.println("===================");
      Serial.println();
      pushArray(getAvg(pm10, MOV_AVG_WDW), pm10Rec, RECORD_SIZE);
      pushArray(getAvg(pm25, MOV_AVG_WDW), pm25Rec, RECORD_SIZE);
      pushArray(getAvg(hum, MOV_AVG_WDW), humRec, RECORD_SIZE);
      pushArray(getAvg(temp, MOV_AVG_WDW), tempRec, RECORD_SIZE);
      pushArray(getAvg(pres, MOV_AVG_WDW) / 100.0, presRec, RECORD_SIZE);
      pushArray(getAvg(iaq, MOV_AVG_WDW), iaqRec, RECORD_SIZE);
      pushArray(getAvg(co2, MOV_AVG_WDW), co2Rec, RECORD_SIZE);
      pushArray(getAvg(vbat, MOV_AVG_WDW), vbatRec, RECORD_SIZE);
    }
  }
}

void rtcCode(void * parameter) {
  Serial.print("RTC Init runnning on core ");
  Serial.println(xPortGetCoreID());
  while (i2c_lock)vTaskDelay(100);
  i2c_lock = true;
  if (! rtc.begin()) {
    Serial.println("Couldn't find RTC");
    i2c_lock = false;
    for (;;) {
      vTaskDelay(10000);
    }
  } else {
    if (rtc.lostPower()) {
      Serial.println("RTC lost power, lets set the time!");
      // following line sets the RTC to the date & time this sketch was compiled
      // rtc.adjust(DateTime(F(__DATE__), F(__TIME__)));
      // This line sets the RTC with an explicit date & time, for example to set
      // January 21, 2014 at 3am you would call:
      // rtc.adjust(DateTime(2014, 1, 21, 3, 0, 0));
    }
    i2c_lock = false;
    for (;;) {
      vTaskDelay(10000);
      while (i2c_lock) vTaskDelay(100);
      i2c_lock = true;
      printRTC();
      i2c_lock = false;
    }
  }
}

void oledDisplayCode(void * parameter) {
  Serial.print("OLED Display running on core ");
  Serial.println(xPortGetCoreID());
  while (i2c_lock)vTaskDelay(100);
  i2c_lock = true;
  if (!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) { // Address 0x3C for 128x32
    Serial.println(F("SSD1306 allocation failed"));
    i2c_lock = false;
    for (;;) {
      vTaskDelay(10000);
    }
  } else {
    display.display();
    i2c_lock = false;
    vTaskDelay(3000);
    for (;;) {
      vTaskDelay(4000);
      while (i2c_lock) vTaskDelay(100);
      i2c_lock = true;
      display.clearDisplay();
      display.setTextSize(1);      // Normal 1:1 pixel scale
      display.setTextColor(SSD1306_WHITE); // Draw white text
      display.setCursor(0, 0);

      display.print("PM10:  ");
      display.print(getAvg(pm10, MOV_AVG_WDW));
      display.println("ug^m3");

      display.print("PM2.5:  ");
      display.print(getAvg(pm25, MOV_AVG_WDW));
      display.println("ug^m3");

      display.print("Humidity:  ");
      display.print(getAvg(hum, MOV_AVG_WDW));
      display.println("%");

      display.print("Pressure:  ");
      display.print(getAvg(pres, MOV_AVG_WDW) / 100);
      display.print("hPa");
      display.display();
      i2c_lock = false;

      vTaskDelay(4000);
      while (i2c_lock) vTaskDelay(100);
      i2c_lock = true;
      display.clearDisplay();
      display.setTextSize(1);      // Normal 1:1 pixel scale
      display.setTextColor(SSD1306_WHITE); // Draw white text
      display.setCursor(0, 0);

      display.print("Temperature:  ");
      display.print(getAvg(temp, MOV_AVG_WDW));
      display.println("*C");

      display.print("CO2:  ");
      display.print(getAvg(co2, MOV_AVG_WDW));
      display.println("PPM");


      display.print("IAQ:  ");
      display.println(getAvg(iaq, MOV_AVG_WDW));
      display.display();
      i2c_lock = false;

      vTaskDelay(4000);
      while (i2c_lock) vTaskDelay(100);
      i2c_lock = true;
      display.clearDisplay();
      display.setTextSize(1);      // Normal 1:1 pixel scale
      display.setTextColor(SSD1306_WHITE); // Draw white text
      display.setCursor(0, 0);

      display.print("WiFi: ");
      if (wifi_mode == WIFIAP_MODE) display.println("Station");
      else display.println("Access Point");

      display.println("SSID:");
      if (wifi_mode == WIFIAP_MODE) display.println("  " + wifi_processor(String("AP_SSID")));
      else display.println("  " + wifi_processor(String("STA_SSID")));

      display.print("Status: ");
      if (WiFi.status() == WL_CONNECTED) display.println("Connected");
      else display.println("Not Connected");

      display.display();
      i2c_lock = false;

      vTaskDelay(4000);
      while (i2c_lock) vTaskDelay(100);
      i2c_lock = true;
      display.clearDisplay();
      display.setTextSize(1);      // Normal 1:1 pixel scale
      display.setTextColor(SSD1306_WHITE); // Draw white text
      display.setCursor(0, 0);

      display.println("IP Address:");
      if (wifi_mode == WIFIAP_MODE) display.println("  " + WiFi.softAPIP().toString());
      else display.println("  " + WiFi.localIP().toString());

      display.println("MDNS:");
      display.println("  esp32.local");

      display.display();
      i2c_lock = false;

      vTaskDelay(4000);
      while (i2c_lock) vTaskDelay(100);
      i2c_lock = true;
      display.clearDisplay();
      display.setTextSize(1);      // Normal 1:1 pixel scale
      display.setTextColor(SSD1306_WHITE); // Draw white text
      display.setCursor(0, 0);
      float voltage = analogRead(CHARGE_PIN) * 0.00172;
      float bat = getAvg(vbat, MOV_AVG_WDW);
      if (voltage >= 2) {
        display.println("Status:");
        display.print("  Plugged-in|");
        display.println("Charged");
      } else if (voltage >= 1) {
        display.println("Status:");
        display.println("  Plugged-in|");
        display.println("Charging");
      } else {
        display.print("Status: ");
        display.println("On Battery");
        display.println();
      }

      display.print("Battery: ");
      display.print(bat);
      display.println("V");

      display.print("Charge: ");
      display.print(getBatteryPercentage(bat, 2.9, 4.1));
      display.println("%");

      display.display();
      i2c_lock = false;

      vTaskDelay(4000);
      while (i2c_lock) vTaskDelay(100);
      i2c_lock = true;
      display.clearDisplay();
      display.setTextSize(1);      // Normal 1:1 pixel scale
      display.setTextColor(SSD1306_WHITE); // Draw white text
      display.setCursor(0, 0);

      display.print("SD Card: ");
      initSD();
      if (sd_ok) {
        display.println("Inserted");
        display.print("Size: ");
        display.print(availableSD());
        display.print("/");
        display.print(cardSize());
        display.println("GB");
      } else {
        display.println("Ejected");
      }

      display.print("Logging: ");
      if (getPreferenceBool("sd_log"))display.println("Enabled");
      else display.println("Disabled");

      display.print("Interval:");
      display.print(SD_LOG_INTERVAL / 1000);
      display.println("s");

      display.display();
      i2c_lock = false;

      vTaskDelay(4000);
      while (i2c_lock) vTaskDelay(100);
      i2c_lock = true;
      DateTime now = rtc.now();
      display.clearDisplay();
      display.setTextSize(1);      // Normal 1:1 pixel scale
      display.setTextColor(SSD1306_WHITE); // Draw white text
      display.setCursor(0, 0);

      display.print("Dev.ID: ");
      display.println("aq_sense01");

      display.print("Firmware: ");
      display.println("FGHGK.153");

      display.println("Time:");
      display.print(now.year(), DEC);
      display.print('/');
      display.print(now.month(), DEC);
      display.print('/');
      display.print(now.day(), DEC);
      display.print(" ");
      display.print(now.hour(), DEC);
      display.print(':');
      display.print(now.minute(), DEC);
      display.print(':');
      display.print(now.second(), DEC);

      display.display();
      i2c_lock = false;

      vTaskDelay(4000);
      while (i2c_lock) vTaskDelay(100);
      i2c_lock = true;
      drawBitmap();
      i2c_lock = false;
    }
  }
}

void bme680Code(void * parameter) {
  Serial.print("BME680 Code running on core ");
  Serial.println(xPortGetCoreID());
  while (i2c_lock)vTaskDelay(100);
  i2c_lock = true;
  if (!bme.begin()) {
    i2c_lock = false;
    Serial.println("Could not find a valid BME680 sensor, check wiring!");
    setLedStatus(LED_STATUS_SENSOR);
    for (;;) {
      vTaskDelay(10000);
    }
  } else {
    bme.setTemperatureOversampling(BME680_OS_8X);
    bme.setHumidityOversampling(BME680_OS_2X);
    bme.setPressureOversampling(BME680_OS_4X);
    bme.setIIRFilterSize(BME680_FILTER_SIZE_3);
    bme.setGasHeater(320, 150);
    i2c_lock = false;
    for (;;) {
      vTaskDelay(READ_INTERVAL);
      while (i2c_lock)vTaskDelay(100);
      i2c_lock = true;
      Serial.println("[BME680]");
      Serial.println("==========");
      if (! bme.performReading()) {
        i2c_lock = false;
        Serial.println("Failed to perform reading :(");
        setLedStatus(LED_STATUS_SENSOR);
        pushArray(0, temp, MOV_AVG_WDW);
        pushArray(0, pres, MOV_AVG_WDW);
        pushArray(0, hum, MOV_AVG_WDW);
        pushArray(0, iaq, MOV_AVG_WDW);
      } else {
        setLedStatus(LED_STATUS_OK);
        pushArray(bme.temperature, temp, MOV_AVG_WDW);
        pushArray(bme.pressure, pres, MOV_AVG_WDW);
        pushArray(bme.humidity, hum, MOV_AVG_WDW);

        float current_humidity = bme.humidity;
        i2c_lock = false;
        if (current_humidity >= 38 && current_humidity <= 42)
          hum_score = 0.25 * 100; // Humidity +/-5% around optimum
        else
        { //sub-optimal
          if (current_humidity < 38)
            hum_score = 0.25 / hum_reference * current_humidity * 100;
          else
          {
            hum_score = ((-0.25 / (100 - hum_reference) * current_humidity) + 0.416666) * 100;
          }
        }

        //Calculate gas contribution to IAQ index
        float gas_lower_limit = 5000; // Bad air quality limit
        float gas_upper_limit = 50000; // Good air quality limit
        if (gas_reference > gas_upper_limit) gas_reference = gas_upper_limit;
        if (gas_reference < gas_lower_limit) gas_reference = gas_lower_limit;
        gas_score = (0.75 / (gas_upper_limit - gas_lower_limit) * gas_reference - (gas_lower_limit * (0.75 / (gas_upper_limit - gas_lower_limit)))) * 100;

        //Combine results for the final IAQ index value (0-100% where 100% is good quality air)
        float air_quality_score = hum_score + gas_score;
        /*
          Serial.println("Air Quality=" + String(air_quality_score, 1) + " % derived from 25 % of Humidity reading and 75 % of Gas reading - 100 % is good quality air");
            Serial.println("Humidity element was : " + String(hum_score/100) + " of 0.25");
            Serial.println("     Gas element was : " + String(gas_score/100) + " of 0.75");
            if (bme.readGas() < 120000) Serial.println("***** Poor air quality *****");
            Serial.println();
        */
        if ((getgasreference_count++) % 10 == 0) GetGasReference();
        /*
          Serial.println(CalculateIAQ(air_quality_score));
          Serial.println("->->->->->->->->->->->->->->->->->->->->->->->->");
          vTaskDelay(2000);
        */
        pushArray(air_quality_score, iaq, MOV_AVG_WDW);
      }
      Serial.println();
    }
  }
}

void ndirCode(void * parameter) {
  Serial.println("NDIR CO2 Code running on core ");
  Serial.println(xPortGetCoreID());
  ndir.begin(9600, SERIAL_8N1, 16, 17);
  for (;;) {
    vTaskDelay(READ_INTERVAL);
    Serial.println("[NDIR]");
    Serial.println("==========");
    memset(co2res, 0, 7);
    ndir.write(cmd, 9);
    long ndir_start = millis();
    bool timeout = false;
    bool ndir_avail = false;
    while ((!timeout) && (!ndir_avail)) {
      if ((millis() - ndir_start) >= NDIR_TIMEOUT) timeout = true;
      if (ndir.available()) ndir_avail = true;
    }
    if (ndir_avail) {
      while (ndir.available()) {
        if ((ndir.read() == 0xff) && (ndir.read() == 0x86)) {
          ndir.readBytes(co2res, 7);
          Serial.print("RAW BYTES: ");
          for (int i = 0; i < 7; i++) {
            Serial.print(co2res[i], HEX);
          }
          Serial.println();
          int co2buf = (co2res[0] * 256) + co2res[1];
          Serial.print("CO2: ");
          Serial.print(co2buf);
          Serial.println("PPM");
          pushArray(co2buf, co2, MOV_AVG_WDW);
        }
        setLedStatus(LED_STATUS_OK);
      }
    } else if (timeout) {
      setLedStatus(LED_STATUS_SENSOR);
      pushArray(0, co2, MOV_AVG_WDW);
    }
    Serial.println();
  }
}

void sds011Code(void * parameter) {
  Serial.print("SDS011 Code running on core ");
  Serial.println(xPortGetCoreID());
  sds011.begin(9600, SERIAL_8N1, 2, 4);
  for (;;) {
    vTaskDelay(READ_INTERVAL);
    Serial.println("[SDS011]");
    Serial.println("==========");
    memset(ppmres, 0, 8);
    while (sds011.available()) {
      sds011.read();
    }
    long sds011_start = millis();
    bool timeout = false;
    bool sds011_avail = false;
    while ((!timeout) && (!sds011_avail)) {
      if ((millis() - sds011_start) >= SDS011_TIMEOUT) timeout = true;
      if (sds011.available()) sds011_avail = true;
      vTaskDelay(100);
    }
    if (sds011_avail) {
      while (sds011.available())  {
        byte f_b = 0x00;
        while (f_b != 0xaa) {
          f_b = sds011.read();
        }
        if (f_b == 0xaa) {
          byte s_b = sds011.peek();
          if (s_b == 0xc0) {
            Serial.print("RAW BYTES: ");
            Serial.print(f_b, HEX);
            Serial.print(s_b, HEX);
            sds011.read();
            while (sds011.available() < 8) {
              vTaskDelay(50);
            }
            sds011.readBytes(ppmres, 8);
            for (int i = 0; i < 8; i++) {
              Serial.print(ppmres[i], HEX);
            }
            Serial.println();
            if (ppmres[7] == 0xab) {
              int pm25buf = (int(ppmres[1]) * 256 + int(ppmres[0])) / 10;
              int pm10buf = (int(ppmres[3]) * 256 + int(ppmres[2])) / 10;
              Serial.print("PM2.5: ");
              Serial.print(pm25buf);
              Serial.println("ug/m ^ 3");
              Serial.print("PM10: ");
              Serial.print(pm10buf);
              Serial.println("ug/m ^ 3");
              pushArray(pm25buf, pm25, MOV_AVG_WDW);
              pushArray(pm10buf, pm10, MOV_AVG_WDW);
              setLedStatus(LED_STATUS_OK);
            } else {
              pushArray(0, pm25, MOV_AVG_WDW);
              pushArray(0, pm10, MOV_AVG_WDW);
              setLedStatus(LED_STATUS_SENSOR);
            }
          }
        } else {
          Serial.print("Invalid Start Byte: ");
          Serial.print(f_b, HEX);
          setLedStatus(LED_STATUS_SENSOR);
        }
      }
    } else if (timeout) {
      pushArray(0, pm25, MOV_AVG_WDW);
      pushArray(0, pm10, MOV_AVG_WDW);
    }
    Serial.println();
  }
}

void drawBitmap() {
  display.clearDisplay();
  display.drawBitmap(
    (display.width()  - LOGO_WIDTH ) / 2,
    (display.height() - LOGO_HEIGHT) / 2,
    logo_bmp, LOGO_WIDTH, LOGO_HEIGHT, 1);
  display.display();
}

unsigned int getFileCounter() {
  return getPreferenceUInt("log_file_count");
}

void setFileCounter(unsigned int count) {
  putPreference("log_file_count", count);
}

String getFileTimestamp() {
  return getPreferenceString("log_file_time");
}

void setFileTimestamp(String timestamp) {
  putPreference("log_file_time", timestamp);
}

float getBattery() {
  return (analogRead(BAT_PIN) * 0.00172);
}

String getChargeStatus() {
  float voltage = analogRead(CHARGE_PIN) * 0.00172;
  if (voltage >= 2) {
    return "Plugged-in | Charged";
  } else if (voltage >= 1) {
    return "Plugged-in | Charging";
  }
  return "On Battery";
}

float getBatteryPercentage(float batteryVoltage, float cutOffVoltage, float maxVoltage) {
  return constrain(mapf(batteryVoltage, cutOffVoltage, maxVoltage, 0.0, 100.0), 0, 100);
}

void GetGasReference() {
  // Now run the sensor for a burn-in period, then use combination of relative humidity and gas resistance to estimate indoor air quality as a percentage.
  Serial.println("Getting a new gas reference value");
  int readings = 10;
  for (int i = 1; i <= readings; i++) { // read gas for 10 x 0.150mS=1.5secs
    gas_reference += bme.readGas();
  }
  gas_reference = gas_reference / readings;
}

String CalculateIAQ(float score) {
  String IAQ_text = "Air quality is ";
  score = (100 - score) * 5;
  if      (score <= 301)                  IAQ_text += "Hazardous";
  else if (score <= 201 && score <= 300 ) IAQ_text += "Very Unhealthy";
  else if (score <= 176 && score <= 200 ) IAQ_text += "Unhealthy";
  else if (score <= 151 && score <= 175 ) IAQ_text += "Unhealthy for Sensitive Groups";
  else if (score <=  51 && score <= 150 ) IAQ_text += "Moderate";
  else if (score <=  00 && score <=  50 ) IAQ_text += "Good";
  return IAQ_text;
}

void espReboot() {
  Serial.println("Restarting ESP32...");
  ESP.restart();
}

void printRTC() {
  DateTime now = rtc.now();

  Serial.print(now.year(), DEC);
  Serial.print('/');
  Serial.print(now.month(), DEC);
  Serial.print('/');
  Serial.print(now.day(), DEC);
  Serial.print(" (");
  Serial.print(daysOfTheWeek[now.dayOfTheWeek()]);
  Serial.print(") ");
  Serial.print(now.hour(), DEC);
  Serial.print(':');
  Serial.print(now.minute(), DEC);
  Serial.print(':');
  Serial.print(now.second(), DEC);
  Serial.println();
}

void setLedStatus(byte set_status) {
  if (set_status > led_status_mode) {
    led_status_mode = set_status;
  }
}

unsigned int getLedTimeout() {
  if (led_phase) {
    switch (led_status_mode) {
      case LED_STATUS_OK: return LED_OK_ON;
      case LED_STATUS_WARMUP: return LED_WARMUP_ON;
      case LED_STATUS_SENSOR: return LED_SENSOR_ON;
      case LED_STATUS_WIFI: return LED_WIFI_ON;
      case LED_STATUS_UPLOAD: return LED_UPLOAD_ON;
    }
  } else {
    switch (led_status_mode) {
      case LED_STATUS_OK: return LED_OK_OFF;
      case LED_STATUS_WARMUP: return LED_WARMUP_OFF;
      case LED_STATUS_SENSOR: return LED_SENSOR_OFF;
      case LED_STATUS_WIFI: return LED_WIFI_OFF;
      case LED_STATUS_UPLOAD: return LED_UPLOAD_OFF;
    }
  }
}

bool handleSNTP() {
  if (getLocalTime(&timeinfo)) {
    sntp_ok = true;
  } else {
    sntp_ok = false;
  }
  return sntp_ok;
}

String twoDigits(int digit) {
  String digits = "";
  if (digit < 10) {
    digits = "0";
  }
  return digits + String(digit);
}

//replace with snprintf
String getSntpIsoTime() {
  char iso[26];
  snprintf(iso, 26, "%04d-%02d-%02dT%02d:%02d:%02d+%02d:%02d", timeinfo.tm_year + 1900, timeinfo.tm_mon + 1, timeinfo.tm_mday, timeinfo.tm_hour, timeinfo.tm_min, timeinfo.tm_sec, gmtOffset_sec / 3600, (gmtOffset_sec % 3600) / 60);
  return iso;
}

String timeToString(struct tm * timestruct) {
  char iso[26];
  snprintf(iso, 26, "%04d-%02d-%02dT%02d:%02d:%02d+%02d:%02d", timestruct->tm_year + 1900, timestruct->tm_mon + 1, timestruct->tm_mday, timestruct->tm_hour, timestruct->tm_min, timestruct->tm_sec, gmtOffset_sec / 3600, (gmtOffset_sec % 3600) / 60);
  return iso;
}

String getSntpFilename() {
  char filename[15];
  snprintf(filename, 15, "%04d%02d%02d%02d%02d%02d", timeinfo.tm_year + 1900, timeinfo.tm_mon + 1, timeinfo.tm_mday, timeinfo.tm_hour, timeinfo.tm_min, timeinfo.tm_sec);
  return filename;
}

void notFound(AsyncWebServerRequest *request) {
  request->send(404, "text/plain", "Not found");
}

bool handleWifi() {
  if (!digitalRead(RESET_PIN)) {
    if (wifi_mode == WIFIAP_MODE) {
      //Serial.println("Status: Wifi is in AP mode");
    } else {
      wifi_mode = WIFIAP_MODE;
      server.end();
      Serial.println("[Wifi AP]");
      Serial.println("===========");
      Serial.println("Web Server: Stopped");
      //MDNS.end();
      Serial.println("MDNS: Stopped");
      WiFi.disconnect();
      Serial.println("Status: Wifi changing to AP mode");
      String tmp_ssid = wifi_processor(String("AP_SSID"));
      String tmp_password = wifi_processor(String("AP_PASSWORD"));
      char ssid[tmp_ssid.length()];
      tmp_ssid.toCharArray(ssid, tmp_ssid.length() + 1);
      if (tmp_password.length() == 0) {
        WiFi.softAP(ssid);
        Serial.print("SSID: ");
        Serial.println(ssid);
      } else {
        char password[tmp_password.length()];
        tmp_password.toCharArray(password, tmp_password.length() + 1);
        WiFi.softAP(ssid, password);
        Serial.print("SSID: ");
        Serial.println(ssid);
        Serial.print("PWD: ");
        Serial.println(password);
      }
      Serial.print("Gateway Address: ");
      Serial.println(WiFi.softAPIP());
      server.begin();
      Serial.println("Web Server: Started");
      ArduinoOTA.begin();
      MDNS.addService("http", "tcp", 80);
      vTaskDelay(100);
    }
  } else {
    if (wifi_mode == WIFISTA_MODE) {
      //Serial.println("Status: Wifi is in STA mode");
    } else {
      wifi_mode = WIFISTA_MODE;
      server.end();
      Serial.println("[Wifi STA]");
      Serial.println("==========");
      Serial.println("Web Server: Stopped");
      //MDNS.end();
      Serial.println("MDNS: Stopped");
      WiFi.softAPdisconnect(true);
      Serial.println("Status: Wifi changing to STA mode");
      vTaskDelay(100);
    }
    if (WiFi.status() != WL_CONNECTED) {
      Serial.println("Status: Disconnected");
      String tmp_ssid = "Nyi Nyi Nyan Tun"; //wifi_processor(String("STA_SSID"));
      String tmp_password = "nyinyitun"; //wifi_processor(String("STA_PASSWORD"));
      char ssid[tmp_ssid.length()];
      tmp_ssid.toCharArray(ssid, tmp_ssid.length() + 1);
      Serial.println(tmp_ssid);
      Serial.println(tmp_ssid.length());
      Serial.println(ssid);
      if (tmp_password.length() == 0) {
        Serial.print("Connecting SSID: ");
        Serial.println(ssid);
        WiFi.begin(ssid);
        WiFi.setSleep(false);
      } else {
        char password[tmp_password.length()];
        tmp_password.toCharArray(password, tmp_password.length() + 1);
        Serial.print("Connecting SSID: ");
        Serial.println(ssid);
        Serial.print("PWD: ");
        Serial.println(password);
        WiFi.begin(ssid, password);
        WiFi.setSleep(false);
      }
      long wifi_start = millis();
      bool timeout = false;
      while ((WiFi.status() != WL_CONNECTED) && (!timeout)) {
        vTaskDelay(500);
        Serial.print("=");
        if ((millis() - wifi_start) > WIFI_TIMEOUT) timeout = true;
      }
      Serial.println();
      if (WiFi.status() == WL_CONNECTED) {
        Serial.println("Status: Connected");
        Serial.print("IP address: ");
        Serial.println(WiFi.localIP());
        server.begin();
        Serial.println("Web Server: Started");
        Serial.println("OTA: Initialized");
        ArduinoOTA.begin();
        MDNS.addService("http", "tcp", 80);
      } else if (timeout == true) {
        Serial.println("Status: Connection Timeout");
      }
    }
  }
}

// loRa transmit
void loRaSend(String data){
  LoRa.beginPacket();
  LoRa.println(data);
  LoRa.endPacket();
}

//////////////////////////////////////// * MAIN * ///////////////////////////////

void setup() {
  WRITE_PERI_REG(RTC_CNTL_BROWN_OUT_REG, 0); //disable brownout detector
  Serial.begin(9600);
  Serial.println("Device Booting...");
  pinMode(RESET_PIN, INPUT_PULLUP);
  pinMode(LED_PIN, OUTPUT);
  if (!SPIFFS.begin(true)) {
    Serial.println("An Error has occurred while mounting SPIFFS");
    return;
  }


 // LoRa initialization
  LoRa.setPins(ss, rst, dio0);
  LoRa.begin(433E6); // legal frequency for asia, 433kHz
  LoRa.setSyncWord(0x03); // can be any value from 0x00 to 0xFF
  
  // Core 0 Tasks
  xTaskCreatePinnedToCore(statusLedCode, "Status LED", 10000, NULL, 0, &statusLedTask, 0);
  xTaskCreatePinnedToCore(dataRecordCode, "Data Record Task", 10000, NULL, 1, &dataRecordTask, 0);
  xTaskCreatePinnedToCore(sds011Code, "SDS011 Task", 10000, NULL, 1, &sds011Task, 0);

  // Core 1 Tasks
  xTaskCreatePinnedToCore(oledDisplayCode, "OLED Display", 10000, NULL, 1, &oledDisplayTask, 1);
  xTaskCreatePinnedToCore(uploadCode, "Upload Task", 10000, NULL, 3, &uploadTask, 1);
  xTaskCreatePinnedToCore(bme680Code, "BME680 Task", 10000, NULL, 2, &bme680Task, 1);
  xTaskCreatePinnedToCore(ndirCode, "NDIR CO2 Task", 10000, NULL, 3, &ndirTask, 1);
  xTaskCreatePinnedToCore(logCode, "Log Task", 10000, NULL, 4, &logTask, 1);

  //xTaskCreatePinnedToCore(rtcCode, "RTC", 10000, NULL, 1, &rtcTask, 0);

  // Initialize, memory allocate and populate arrays
  popArray(co2, MOV_AVG_WDW);
  popArray(pm25, MOV_AVG_WDW);
  popArray(pm10, MOV_AVG_WDW);
  popArray(hum, MOV_AVG_WDW);
  popArray(temp, MOV_AVG_WDW);
  popArray(pres, MOV_AVG_WDW);
  popArray(iaq, MOV_AVG_WDW);
  popArray(vbat, MOV_AVG_WDW);
  pm10Rec = (int *) malloc(RECORD_SIZE * sizeof(int));
  pm25Rec = (int *) malloc(RECORD_SIZE * sizeof(int));
  humRec = (int *) malloc(RECORD_SIZE * sizeof(int));
  tempRec = (int *) malloc(RECORD_SIZE * sizeof(int));
  presRec = (int *) malloc(RECORD_SIZE * sizeof(int));
  iaqRec = (int *) malloc(RECORD_SIZE * sizeof(int));
  co2Rec = (int *) malloc(RECORD_SIZE * sizeof(int));
  vbatRec = (float *) malloc(RECORD_SIZE * sizeof(float));
  popArray(co2Rec, RECORD_SIZE);
  popArray(pm10Rec, RECORD_SIZE);
  popArray(pm25Rec, RECORD_SIZE);
  popArray(humRec, RECORD_SIZE);
  popArray(presRec, RECORD_SIZE);
  popArray(tempRec, RECORD_SIZE);
  popArray(iaqRec, RECORD_SIZE);
  popArray(vbatRec, RECORD_SIZE);

  vTaskDelay(2000);
  WiFi.disconnect();

  ////////////////////// ***** OTA SECTION ***** /////////////////////////////////

  // Port defaults to 3232
  // ArduinoOTA.setPort(3232);

  // Hostname defaults to esp3232-[MAC]
  ArduinoOTA.setHostname("esp32");

  // No authentication by default
  // ArduinoOTA.setPassword("admin");

  // Password can be set with it's md5 value as well
  // MD5(admin)=21232f297a57a5a743894a0e4a801fc3
  // ArduinoOTA.setPasswordHash("21232f297a57a5a743894a0e4a801fc3");

  //ArduinoOTA.begin();
  ArduinoOTA
  .onStart([]() {
    String type;
    if (ArduinoOTA.getCommand() == U_FLASH)
      type = "sketch";
    else // U_SPIFFS
      type = "filesystem";

    // NOTE: if updating SPIFFS this would be the place to unmount SPIFFS using SPIFFS.end()
    Serial.println("Start updating " + type);
  })
  .onEnd([]() {
    Serial.println("\nEnd");
  })
  .onProgress([](unsigned int progress, unsigned int total) {
    Serial.printf("Progress: %u%%\r", (progress / (total / 100)));
  })
  .onError([](ota_error_t error) {
    Serial.printf("Error[%u]: ", error);
    if (error == OTA_AUTH_ERROR) Serial.println("Auth Failed");
    else if (error == OTA_BEGIN_ERROR) Serial.println("Begin Failed");
    else if (error == OTA_CONNECT_ERROR) Serial.println("Connect Failed");
    else if (error == OTA_RECEIVE_ERROR) Serial.println("Receive Failed");
    else if (error == OTA_END_ERROR) Serial.println("End Failed");
  });

  //////////////////// ***** SERVER REQUEST PATHS ***** ////////////////////////////

  // Send a GET request to <ESP_IP-/get?input1=<inputMessage-
  DefaultHeaders::Instance().addHeader("Access-Control-Allow-Origin", "*");

  server.on("/", HTTP_GET, [](AsyncWebServerRequest * request) {
    AsyncWebServerResponse *response = request->beginResponse(SPIFFS, "/index.html.gz", "text/html");
    response->addHeader("Content-Encoding", "gzip");
    request->send(response);
  });

  server.on("/get", HTTP_GET, [](AsyncWebServerRequest * request) {
    if (request->hasParam("settings")) {
      String type = request->getParam("settings")->value();
      if (type == "sta") {
        request->send(SPIFFS, "/sta.json", String(), false, wifi_processor);
      } else if (type == "ap") {
        request->send(SPIFFS, "/ap.json", String(), false, wifi_processor);
      } else if (type == "api") {
        request->send(SPIFFS, "/api.json", String(), false, api_processor);
      } else if (type == "device") {
        //waitSDLock();
        initSD();
        //lockSD();
        request->send(SPIFFS, "/device.json", String(), false, device_processor);
        //unlockSD();
      }
    } else if (request->hasParam("data")) {
      String type = request->getParam("data")->value();
      if (type == "pm10") {
        request->send(SPIFFS, "/pm10.json", String(), false, data_processor);
        Serial.println("REQ: /pm10.json");
      } else if (type == "pm25") {
        request->send(SPIFFS, "/pm25.json", String(), false, data_processor);
        Serial.println("REQ: /pm25.json");
      } else if (type == "humidity") {
        request->send(SPIFFS, "/humidity.json", String(), false, data_processor);
        Serial.println("REQ: /humidity.json");
      } else if (type == "temperature") {
        request->send(SPIFFS, "/temperature.json", String(), false, data_processor);
        Serial.println("REQ: /temperature.json");
      } else if (type == "pressure") {
        request->send(SPIFFS, "/pressure.json", String(), false, data_processor);
        Serial.println("REQ: /pressure.json");
      } else if (type == "voc") {
        request->send(SPIFFS, "/voc.json", String(), false, data_processor);
        Serial.println("REQ: /voc.json");
      } else if (type == "co2") {
        request->send(SPIFFS, "/co2.json", String(), false, data_processor);
        Serial.println("REQ: /co2.json");
      } else if (type == "battery") {
        request->send(SPIFFS, "/battery.json", String(), false, data_processor);
        Serial.println("REQ: /battery.json");
      } else if (type == "current") {
        request->send(SPIFFS, "/current.json", String(), false, data_processor);
        Serial.println("REQ: /current.json");
      } else if (type == "logs") {
        waitSDLock();
        initSD();
        lockSD();
        request->send(SPIFFS, "/logs.json", String(), false, log_processor);
        unlockSD();
        Serial.println("REQ: /logs.json");
      }
    }
  });

  server.on("/set", HTTP_GET, [](AsyncWebServerRequest * request) {
    if (request->hasParam("settings")) {
      String type = request->getParam("settings")->value();
      if (type == "sta") {
        if (request->hasParam("ssid")) {
          String ssid = request->getParam("ssid")->value();
          if (ssid.length() > 0) {
            putPreference("sta_ssid", ssid);
          } else {
            removePreference("sta_ssid");
          }
          if (request->hasParam("password")) {
            String password = request->getParam("password")->value();
            if (password.length() >= 8) {
              putPreference("sta_password", password);
            } else {
              removePreference("sta_password");
            }
          }
          request->send(200, "text/plain", "OK");
        }
      } else if (type == "ap") {
        if (request->hasParam("ssid")) {
          String ssid = request->getParam("ssid")->value();
          if (ssid.length() > 0) {
            putPreference("ap_ssid", ssid);
          } else {
            removePreference("ap_ssid");
          }
          if (request->hasParam("password")) {
            String password = request->getParam("password")->value();
            if (password.length() >= 8) {
              putPreference("ap_password", password);
            } else {
              removePreference("ap_password");
            }
          }
          request->send(200, "text/plain", "OK");
        }
      } else if (type == "api") {
        if (request->hasParam("url")) {
          String url = request->getParam("url")->value();
          if (url.length() > 0) {
            putPreference("api_url", url);
          } else {
            removePreference("api_url");
          }
        }
        if (request->hasParam("key")) {
          String key = request->getParam("key")->value();
          if (key.length() > 0) {
            putPreference("api_key", key);
          } else {
            removePreference("api_key");
          }
        }
        request->send(200, "text/plain", "OK");
      } else if (type == "sd") {
        if (request->hasParam("log")) {
          String sd_log = request->getParam("log")->value();
          bool result = (sd_log == "true");
          putPreference("sd_log", result);
          request->send(200, "text/plain", "OK");
        }
        if (request->hasParam("prefix")) {
          String prefix = request->getParam("prefix")->value();
          if (prefix.length() > 0) {
            putPreference("file_prefix", prefix);
          } else {
            putPreference("file_prefix", deviceId);
          }
          request->send(200, "text/plain", "OK");
        }
      }

    } else if (request->hasParam("control")) {
      String type = request->getParam("control")->value();
      if (type == "reboot") {
        request->send(200, "text/plain", "OK");
        espReboot();
      } else if (type == "sd_insert") {
        String result = "Cannot insert";
        if (insertSD()) {
          result = "inserted";
        }
        request->send(200, "text/plain", result);
      } else if (type == "sd_eject") {
        String result = "Cannot eject";
        if (ejectSD()) {
          result = "ejected";
        }
        request->send(200, "text/plain", result);
      } else if (type == "sd_lock") {
        lockSD();
        request->send(200, "text/plain", "OK");
      } else if (type == "sd_unlock") {
        unlockSD();
        request->send(200, "text/plain", "OK");
      }
    }
  });

  server.on("/records", HTTP_GET, [](AsyncWebServerRequest * request) {
    if (request->hasParam("file") && request->hasParam("type")) {
      String filename = request->getParam("file")->value();
      String type = request->getParam("type")->value();
      waitSDLock();
      initSD();
      if (type == "download") {
        if (sd_ok && !sd_lock) {
          lockSD();
          while (spi_lock)vTaskDelay(10);
          spi_lock = true;
          request->send(SD, "/log/" + filename, "text/plain", true);
          spi_lock = false;
          unlockSD();
        } else {
          request->send(404, "text/plain", "File Not Found");
        }
      } else if (type == "delete") {
        if (sd_ok && !sd_lock) {
          while (spi_lock)vTaskDelay(10);
          spi_lock = true;
          if (deleteFile(SD, "/log/" + filename)) {
            spi_lock = false;
            lockSD();
            request->send(200, "text/plain", "File: " + filename + " is deleted");
            unlockSD();
          }
          spi_lock = false;
        } else {
          request->send(404, "text/plain", "File Not Found");
        }
      }
    } else {
      AsyncWebServerResponse *response = request->beginResponse(SPIFFS, "/records.html.gz", "text/html");
      response->addHeader("Content-Encoding", "gzip");
      request->send(response);
    }
  });

  server.on("/settings", HTTP_GET, [](AsyncWebServerRequest * request) {
    AsyncWebServerResponse *response = request->beginResponse(SPIFFS, "/settings.html.gz", "text/html");
    response->addHeader("Content-Encoding", "gzip");
    request->send(response);
  });

  server.on("/bundle.js", HTTP_GET, [](AsyncWebServerRequest * request) {
    AsyncWebServerResponse *response = request->beginResponse(SPIFFS, "/bundle.js.gz", "text/javascript");
    response->addHeader("Content-Encoding", "gzip");
    request->send(response);
    Serial.println("REQ:/bundle.js");
  });

  server.on("/bundle.css", HTTP_GET, [](AsyncWebServerRequest * request) {
    AsyncWebServerResponse *response = request->beginResponse(SPIFFS, "/bundle.css.gz", "text/css");
    response->addHeader("Content-Encoding", "gzip");
    request->send(response);
    Serial.println("REQ:/bundle.css");
  });

  server.on("/fa-solid-900.woff2", HTTP_GET, [](AsyncWebServerRequest * request) {
    AsyncWebServerResponse *response = request->beginResponse(SPIFFS, "/fa-solid-900.woff2.gz", "font/woff2");
    response->addHeader("Content-Encoding", "gzip");
    request->send(response);
    Serial.println("REQ:/fa-solid-900.woff2");
  });

  server.on("/ygnbinhaus.png", HTTP_GET, [](AsyncWebServerRequest * request) {
    AsyncWebServerResponse *response = request->beginResponse(SPIFFS, "/ygnbinhaus.png.gz", "image/png");
    response->addHeader("Content-Encoding", "gzip");
    request->send(response);
    Serial.println("REQ:/ygnbinhaus.png");
  });

  server.onNotFound(notFound);

  sntp_start =  millis();
  setLedStatus(LED_STATUS_WARMUP);
  Serial.println();
  Serial.println("[WARM UP]");
  Serial.println("==========");
  Serial.println("Status: Warm Up Started");
  Serial.println();

  handleWifi();
  configTime(gmtOffset_sec, daylightOffset_sec, ntpServer);
  handleSNTP();
  insertSD();
  vTaskDelay(1000);
}

void loop() {
  Serial.print("Main Loop running on core ");
  Serial.println(xPortGetCoreID());
  pushArray(getBattery(), vbat, MOV_AVG_WDW);
  handleWifi();
  ArduinoOTA.handle();
  if ((WiFi.status() == WL_CONNECTED) && ((sntp_ok && (sntp_start - millis() > SNTP_OK_TIMEOUT)) || (!sntp_ok && (sntp_start - millis() > SNTP_NOT_OK_TIMEOUT)))) {
    configTime(gmtOffset_sec, daylightOffset_sec, ntpServer);
    if (handleSNTP()) {
      sntp_start = millis();
    }
  }
  String dummyData = "DummyData";
  loRaSend(dummyData);
  vTaskDelay(2000);
}

void sendData(int pm25[], int pm10[], int co2[], int temp[], int pres[], int hum[], int iaq[], float bat[], int window) {
  String api_url = api_processor(String("API_URL"));
  Serial.println(api_url);
  if (api_url.length() == 0) {
    return;
  }

  Serial.println("[UPLOAD]");
  Serial.println("==========");

  WiFiClient client;
  const int httpPort = 80;
  char url_char[api_url.length()];
  api_url.toCharArray(url_char, api_url.length() + 1);
  if (!client.connect(url_char, httpPort)) {
    setLedStatus(LED_STATUS_UPLOAD);
    Serial.println("Error: Connection Failed");
    return;
  }

  String url = "/update?api_key=" + api_processor(String("API_KEY")) +
               "&field1=" + String(getAvg(pm25, window)) +
               "&field2=" + String(getAvg(pm10, window)) +
               "&field3=" + String(getAvg(co2, window)) +
               "&field4=" + String(getAvg(temp, window)) +
               "&field5=" + String(getAvg(pres, window) / 100.0) +
               "&field6=" + String(getAvg(hum, window)) +
               "&field7=" + String(getAvg(iaq, window)) +
               "&field8=" + String(getAvg(bat, window));
  // + "&id=" + deviceId;

  Serial.print("Request URL: ");
  Serial.println(url);

  client.print(String("GET ") + url + " HTTP/1.1\r\n" +
               "Host: " + api_url + "\r\n" +
               "Connection: close\r\n\r\n");
  unsigned long timeout = millis();
  while (client.available() == 0) {
    if (millis() - timeout > 5000) {
      setLedStatus(LED_STATUS_UPLOAD);
      Serial.println("Error: Client Timeout !");
      client.stop();
      return;
    }
  }

  while (client.available()) {
    String line = client.readStringUntil('\r');
    Serial.print(line);
  }

  Serial.println();
  Serial.println("Status : Connection Closed");
  Serial.println();
  setLedStatus(LED_STATUS_OK);
}
