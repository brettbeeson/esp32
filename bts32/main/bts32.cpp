#include "Arduino.h"
#include "BBEsp32Lib.h"
#include "BME280Sensor.h"
#include "Blob.h"
#include "BlobSensor.h"
#include "DSTempSensors.h"
#include "InfluxPublisher.h"
#include "MemoryReader.h"
#include "MemorySaver.h"
#include "Reading.h"
#include "VoltageSensor.h"
#include "WiFiAngel.h"

#include "esp_pm.h"
#include "esp_spi_flash.h"
#include "esp_spiffs.h"
#include "esp_system.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "lwip/apps/sntp.h"
#include "nvs.h"
#include "nvs_flash.h"
#include "soc/rtc.h"
#include <inttypes.h>
#include <stdexcept>
#include <stdio.h>
#include <sys/time.h>

#undef LOG_LOCAL_LEVEL
#define LOG_LOCAL_LEVEL ESP_LOG_VERBOSE
#include "esp_log.h"
static const char *TAG = "bts32";


#define DEBUG 0
#if DEBUG
#define SAMPLE_PERIOD_SEC 60
#define BULK_SEND 10
#define DEFAULT_SSID "NetComm 0405"
#define DEFAULT_PWD "wimepuderi"
#else
#define SAMPLE_PERIOD_SEC CONFIG_BTS_SAMPLE_PERIOD_SEC
#define BULK_SEND CONFIG_BTS_BULK_SEND
#define DEFAULT_SSID CONFIG_BTS_WIFI_SSID
#define DEFAULT_PWD CONFIG_BTS_WIFI_PASSWORD
#endif
#define BTS_MAX_CPU_FREQ_MHZ CONFIG_BTS_MAX_CPU_FREQ_MHZ
#define BTS_MIN_CPU_FREQ_MHZ CONFIG_BTS_MIN_CPU_FREQ_MHZ




const int VoltageSensorPin = 39;
const int TempSensorPin = 25;
const gpio_num_t LEDPin = GPIO_NUM_2; // GPIO_NUM_2 (D9) is LED BUILTIN

RTC_DATA_ATTR static int boot_count = 0;
RTC_DATA_ATTR static time_t last_ntp_sync = 0;

#define TRACE printf("TRACE %s:%d\n", __FILE__, __LINE__);

using namespace bbesp32lib;

void initSPIFFS() {

  esp_vfs_spiffs_conf_t conf = {
      .base_path = "", //"/spiffs",
      .partition_label = NULL,
      .max_files = 5,
      .format_if_mount_failed = true};

  // Use settings defined above to initialize and mount SPIFFS filesystem.
  // Note: esp_vfs_spiffs_register is an all-in-one convenience function.
  esp_err_t ret = esp_vfs_spiffs_register(&conf);

  if (ret != ESP_OK) {
    if (ret == ESP_FAIL) {
      ESP_LOGE(TAG, "Failed to mount or format filesystem");
    } else if (ret == ESP_ERR_NOT_FOUND) {
      ESP_LOGE(TAG, "Failed to find SPIFFS partition");
    } else {
      ESP_LOGE(TAG, "Failed to initialize SPIFFS (%s)", esp_err_to_name(ret));
    }
    return;
  }

  size_t total = 0, used = 0;
  ret = esp_spiffs_info(NULL, &total, &used);
  if (ret != ESP_OK) {
    ESP_LOGE(TAG, "Failed to get SPIFFS partition information (%s)", esp_err_to_name(ret));
  } else {
    ESP_LOGI(TAG, "SPIIFS Partition size: total: %dkB, used: %dkB", total / 1024, used / 1024);
  }
}

void countSensors() {
  // untested
  DSTempSensors ts(NULL, TempSensorPin);
  ts.begin();

  bbesp32lib::Blink.flash(ts.nReadings);
  if (ts.nReadings == 0) {
    bbesp32lib::Blink.flash(1, BlinkClass::HARE, BlinkClass::LONG);
  }
}

void readSensors() {

  Blob sensorBlob;
  BlobSensor bs(&sensorBlob, &boot_count);
  // BME280Sensor bme(&sensorBlob) ; // need to change i2c to ESP-ISF
  VoltageSensor vs(&sensorBlob, VoltageSensorPin, 1600000, 1600000, 1.0);
  DSTempSensors ts(&sensorBlob, TempSensorPin);
  MemorySaver saveToMemory(&sensorBlob);

  bbesp32lib::Blink.set(2, 100);

  sensorBlob.begin();

  try {
    saveToMemory.taskify();
    sensorBlob.readSensors(2000);
    saveToMemory.waitUntilPublished();

  } catch (const std::exception &e) {
    ESP_LOGE(TAG, "Exception reading sensors: %s", e.what());
  }
  if (sensorBlob.readingsInQueue() != 0) {
    ESP_LOGE(TAG, "There are %d unpublished readings!",sensorBlob.readingsInQueue());
  }
  /*
    ESP_LOGV(TAG, "Done publish to Memory: Measurements: %d Samples:%d\n",
             saveToMemory.nMeasurements(), saveToMemory.nTotalSamples());
             */
  // saveToMemory.print();
}

void sleep() {
  uint64_t nsSleep = bbesp32lib::uSecToNextMultiple(SAMPLE_PERIOD_SEC);
  ESP_LOGV(TAG, "Ready for bed at %s", bbesp32lib::timestamp().c_str());
  ESP_LOGI(TAG, "Alarm clock set for for %fs", (nsSleep / 1000000.0));
  fflush(NULL);
  bbesp32lib::Blink.end();
  assert(nsSleep / 1000000.0 < SAMPLE_PERIOD_SEC * 2); // if sleepDuration -ve, would overflow and cause epic sleep time
  esp_sleep_enable_timer_wakeup(nsSleep);
  esp_deep_sleep_start();
  assert(0);
}

/**
 * Usually, the sntp sub-system will automatically sync every hour.
 * However, since we deepsleep/reboot, this needs to be done manually. Otherwise every reboot would sync.
 * When resyncing, it's unclear how to guarantee an update. Can't find API reference.
*/

void sync_ntp_if_needed() {

  time_t now;
  int retry = 0;
  const int RETRIES = 10;
  const int ONE_DAY_S = 3600 * 24;
  const char *ntp = "time.google.com";

  now = time(NULL);

  if (bbesp32lib::timeIsValid() && now - last_ntp_sync < ONE_DAY_S) {
    ESP_LOGV(TAG, "No SNTP sync required. Last sync was %.2fh ago.", float(now - last_ntp_sync) / 3600);
    return;
  }

  ESP_LOGI(TAG, "Initializing SNTP");
  sntp_setoperatingmode(SNTP_OPMODE_POLL);
  sntp_setservername(0, (char *)ntp);
  sntp_init();

  while (!bbesp32lib::timeIsValid() && ++retry < RETRIES) {
    ESP_LOGV(TAG, "Waiting for system time to be set... (%d/%d)", retry, RETRIES);
    vTaskDelay(1000 / portTICK_PERIOD_MS);
  }

  if (!bbesp32lib::timeIsValid()) {
    ESP_LOGE(TAG, "Unable to set time from NTP");
    // throw new runtime_error("Unable to set time from NTP");
  } else {
    ESP_LOGI(TAG, "NTP sync'd. Time: %s", bbesp32lib::timestamp().c_str());
    last_ntp_sync = now;
  }
}

void init_power_save() {

  // Configure dynamic frequency scaling:
  // maximum and minimum frequencies are set in sdkconfig,
  // automatic light sleep is enabled if tickless idle support is enabled.
  esp_pm_config_esp32_t pm_config;
  pm_config.max_freq_mhz = BTS_MAX_CPU_FREQ_MHZ;
  pm_config.min_freq_mhz = BTS_MIN_CPU_FREQ_MHZ;
#if CONFIG_FREERTOS_USE_TICKLESS_IDLE
  pm_config.light_sleep_enable = true;
  ESP_LOGI(TAG, "Using light_sleep_enable");
#else
  pm_config.light_sleep_enable = false;
  ESP_LOGI(TAG, "Not using light_sleep_enable");
#endif

  ESP_LOGV(TAG, "Initializing DFS: %d to %d Mhz", pm_config.min_freq_mhz,
           pm_config.max_freq_mhz);

  ESP_ERROR_CHECK(esp_pm_configure(&pm_config));
}

/*
Check some power-saving methods
 */
void power_test() {

  bbesp32lib::Blink.begin(LEDPin); // 14MA
  bbesp32lib::Blink.flash(1);
  printf("Begin\n");
  vTaskDelay(pdMS_TO_TICKS(10000));

  bbesp32lib::Blink.flash(2);
  printf("init_power_save\n");
  init_power_save(); // 1 mA
  vTaskDelay(pdMS_TO_TICKS(10000));

  bbesp32lib::Blink.flash(4);
  WiFiAngel.configSTA(DEFAULT_SSID, DEFAULT_PWD);
  WiFiAngel.begin(60000);
  printf("wifi start\n"); // 100mA
  vTaskDelay(pdMS_TO_TICKS(10000));

  bbesp32lib::Blink.flash(2); // 74mA
  ESP_ERROR_CHECK(esp_wifi_set_ps(WIFI_PS_NONE));
  vTaskDelay(pdMS_TO_TICKS(10000));

  bbesp32lib::Blink.flash(2); // 7mA
  ESP_ERROR_CHECK(esp_wifi_set_ps(WIFI_PS_MIN_MODEM));
  vTaskDelay(pdMS_TO_TICKS(10000));

  bbesp32lib::Blink.flash(2); // 7mA
  ESP_ERROR_CHECK(esp_wifi_set_ps(WIFI_PS_MAX_MODEM));
  vTaskDelay(pdMS_TO_TICKS(10000));

  bbesp32lib::Blink.flash(6);
  WiFiAngel.end();
  printf("wifi end\n"); // 2
  vTaskDelay(pdMS_TO_TICKS(10000));

  bbesp32lib::Blink.flash(10); // .1mA
  esp_sleep_enable_timer_wakeup(10 * 1000000);
  printf("esp_deep_sleep_start\n");
  esp_deep_sleep_start();
}

void setupWifi() {
  ESP_LOGI(TAG, "Setting up WiFi...");
  WiFiAngel.configSTA(DEFAULT_SSID, DEFAULT_PWD);

  if (WiFiAngel.begin(60 * 1000)) {
    ESP_LOGI(TAG, "WiFi connected. Setting time...");
    bbesp32lib::Blink.set(5, 50);

  } else {
    throw std::runtime_error("Couldn't connect to Wifi.");
  }
}

/**
 * todo: can throw outside of function
 * */
void uploadReadings() {
  Blob uploadBlob;
  InfluxPublisher influx(&uploadBlob, "monitor.phisaver.com", 8086, "test", "bts32", "bbeeson", "imagine");
  MemoryReader reader(&uploadBlob);
  int timeoutS=0;

  try {
    uploadBlob.begin();
    influx.taskify();           // publish continuously
    uploadBlob.readReaders();   // read all. Blocks for 1s on sendToQueue, to allow publishing
    influx.waitUntilPublished();
    
  } catch (const std::exception &e) {
    ESP_LOGE(TAG, "Exception uploading readers: %s", e.what());
  }
  if (uploadBlob.readingsInQueue() > 0) {
      ESP_LOGE(TAG, "There are un-uploaded readings!");
    }
  
  
}

void setup() {

#if DEBUG
  setvbuf(stdout, NULL, _IONBF, 0);
  esp_log_level_set("*", ESP_LOG_INFO);
  esp_log_level_set(TAG, ESP_LOG_VERBOSE);
  esp_log_level_set("DSTempSensor", ESP_LOG_VERBOSE);
  //esp_log_level_set("InfluxPublisher", ESP_LOG_VERBOSE);
  //esp_log_level_set("bbesp32lib", ESP_LOG_INFO);
  //esp_log_level_set("MemorySaver", ESP_LOG_VERBOSE);
  //esp_log_level_set("Blob", ESP_LOG_VERBOSE);
  //esp_log_level_set("Publisher", ESP_LOG_VERBOSE);
  esp_log_level_set("wifi", ESP_LOG_WARN);

#else
  esp_log_level_set("*", ESP_LOG_WARN);
#endif

  ESP_ERROR_CHECK(nvs_flash_init());

  init_power_save();

  bbesp32lib::Blink.set(1, 50);
  bbesp32lib::Blink.begin(LEDPin);

  setenv("TZ", "AEST-10", 1);
  tzset();

  ESP_LOGV(TAG, "Sampling @%ds (x%d). Connecting to %s", SAMPLE_PERIOD_SEC, BULK_SEND, DEFAULT_SSID);

  //initSPIFFS();
  // bbesp32lib::LogFile.setFilename("btc32.log");
  //bbesp32lib::LogFile.captureESPLog(true);
  ESP_LOGI(TAG, "Setup complete");
}

extern "C" {

void app_main() {

  try {

    // power_test();
    boot_count++;
    //initArduino(); // required?
    setup();

    ESP_LOGI(TAG, "Upon wake, time is: %s boot_count:%d", bbesp32lib::timestamp().c_str(), boot_count);

    if (boot_count == 1) {

      // segfault
      //LogFile.read(true);
      //LogFile.tail(stdout,10,true);
      setupWifi();
      sync_ntp_if_needed();

    } else if (boot_count == 2 || boot_count % BULK_SEND == 0) {
      readSensors();
      setupWifi();
      sync_ntp_if_needed();
      uploadReadings();
    } else {
      readSensors();
    }

    sleep();

  } catch (std::exception &e) {
    ESP_LOGW(TAG, "Run Exception: %s. Restarting in 60s.", e.what());
    bbesp32lib::Blink.set(20, 50);
    vTaskDelay(pdMS_TO_TICKS(60000));
    ESP.restart();
  }
}
} // extern "C"
