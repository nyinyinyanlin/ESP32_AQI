#define SCREEN_WIDTH 128 // OLED display width, in pixels
#define SCREEN_HEIGHT 32 // OLED display height, in pixels

// Declaration for an SSD1306 display connected to I2C (SDA, SCL pins)
#define OLED_RESET     4 // Reset pin # (or -1 if sharing Arduino reset pin)

#define SEALEVELPRESSURE_HPA (1013.25)
#define MOV_AVG_WDW 20
#define RECORD_SIZE 288
#define SPLIT_SIZE 5242880
#define LOG_DIR "/log"

// Log File Size Modes
// 0 - for sizing the log directory only. This involves iterating every file in the log directory and consumes time proportionally to total number of files. But this is accurate.
// 1 - for sizing the log by substracting the total SD Card's size and used space across the entire SD Card. This is a lot quicker than above method but if there are files other than logs on SD Card, total log file size will be flawed.
#define LOG_SIZE_SD 1

#define RESET_PIN 27
#define LED_PIN 13
#define BAT_PIN 33
#define CHARGE_PIN 32

// LoRa Configurations

#define LORA_SS 15
#define LORA_RST 5
#define LORA_DIO0 2

#define LORA_FREQ 433E6 // legal frequency for asia, 433kHz
#define LORA_SYNCWORD 0x03 // can be any value from 0x00 to 0xFF

// Wifi Mode

#define WIFIAP_MODE 0
#define WIFISTA_MODE 1

// Multitask Timeouts & Intervals

#define NDIR_TIMEOUT 1000
#define SDS011_TIMEOUT 2000
#define WIFI_TIMEOUT 10000
#define SNTP_OK_TIMEOUT 360000
#define SNTP_NOT_OK_TIMEOUT 150002000
#define SD_LOCK_TIMEOUT 5000
#define READ_INTERVAL 2000
#define SEND_INTERVAL 30000
#define WARMUP_INTERVAL 3000
#define RECORD_INTERVAL 10000
#define SD_LOG_INTERVAL 10000

// System Status

#define LED_STATUS_OK 1
#define LED_STATUS_WARMUP 2
#define LED_STATUS_SENSOR 3
#define LED_STATUS_WIFI 4
#define LED_STATUS_UPLOAD 5

// Status LED Intervals

#define LED_WARMUP_ON 1000
#define LED_WARMUP_OFF 1000
#define LED_OK_ON 3000
#define LED_OK_OFF 1000
#define LED_WIFI_ON 750
#define LED_WIFI_OFF 750
#define LED_UPLOAD_ON 500
#define LED_UPLOAD_OFF 500
#define LED_SENSOR_ON 250
#define LED_SENSOR_OFF 250
