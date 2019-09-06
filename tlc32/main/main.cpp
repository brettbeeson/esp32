#include "BBEsp32Lib.h"
#include "TimeLapseCamera.h"
#include "TimeLapseWebServer.h"
#include "TimeLapseWebSocket.h"

// Arduino
#include "Arduino.h"
#include "FS.h"
#include "SD_MMC.h"
// Time
#include "apps/sntp/sntp.h"
#include "lwip/apps/sntp.h"

// ESP
#include "driver/sdmmc_host.h"
#include "driver/sdspi_host.h"
#include "esp_camera.h"
#include "esp_event.h" // default event loop
#include "esp_pm.h"
#include "esp_vfs_fat.h"
#include "nvs_flash.h"
#include "rom/rtc.h"
#include "sdmmc_cmd.h"

// System
#include <sys/time.h>
#include <time.h>
#include <vector>

//#include "camera.h" // testing only

#include "wifi_prov.h"
#include <esp_wifi.h>
#include <wifi_provisioning/manager.h>
#include <wifi_provisioning/scheme_softap.h>

#ifdef CONFIG_FATFS_LFN_NONE
//#ifndef CONFIG_FATFS_LONG_FILENAMES
#warning Enable FATFS_LONG_FILENAMES in menuconfig
#endif

using namespace std;
using namespace bbesp32lib;

TimeLapseCamera cam(SD_MMC);
TimeLapseWebServer webServer(80, cam);
//AsyncWebServer webServer(80);
TimeLapseWebSocket socketServer("/ws", cam);

const char *NTP_SERVER = "time.google.com";

const char *TAG = "tlc32";
#undef LOG_LOCAL_LEVEL
#define LOG_LOCAL_LEVEL ESP_LOG_VERBOSE
#include "esp_log.h"

#define TRACE printf("TRACE %s:%d\n", __FILE__, __LINE__);

#define BASIC_CAM_ONLY 0

RTC_DATA_ATTR static int boot_count = 0;

bool SDMMC_init() {

  ESP_LOGI(TAG, "Starting SDMMC peripheral");

  if (!SD_MMC.begin("")) // mount at root to match FILE* and fs::FS
  {
    Serial.println("Card Mount Failed");
    return false;
  }

  uint64_t cardSize = SD_MMC.cardSize() / (1024 * 1024);
  ESP_LOGI(TAG, "SD_MMC Card Size: %lluMB", cardSize);

  return true;
}

bool setSystemTime() {
  time_t now;
  struct tm timeinfo;
  char strftime_buf[128];

  time(&now);
  localtime_r(&now, &timeinfo);

  if (bbesp32lib::timeIsValid()) {
    ESP_LOGI(TAG, "System time is not set.");
  } else {
    ESP_LOGI(TAG, "System time is %s", bbesp32lib::timestamp().c_str());
  }

  //    initialize_sntp - 1 hour check is default
  ESP_LOGI(TAG, "Initializing SNTP");
  sntp_setoperatingmode(SNTP_OPMODE_POLL);
  sntp_setservername(0, (char *)NTP_SERVER);
  sntp_init();

  // timezone
  // todo: config via timelapse camera
  ESP_LOGI(TAG, "Setting timezone");
  setenv("TZ", "AEST-10", 1);
  tzset();

  localtime_r(&now, &timeinfo);
  strftime(strftime_buf, sizeof(strftime_buf), "%c", &timeinfo);

  // wait for time to be set
  int retry = 0;
  const int retry_count = 10;
  while (timeinfo.tm_year < (2016 - 1900) && ++retry < retry_count) {
    // ESP_LOGI(TAG, "Waiting for system time to be set... (%d/%d)", retry,
    // retry_count);
    vTaskDelay(2000 / portTICK_PERIOD_MS);
    time(&now);
    localtime_r(&now, &timeinfo);
  }
  if (!bbesp32lib::timeIsValid()) {
    ESP_LOGW(TAG, "System time could not be set");
    return false;
  }
  ESP_LOGI(TAG, "System time set: %s", bbesp32lib::timestamp().c_str());
  return true;
}

void init_power_save() {

  // Configure dynamic frequency scaling:
  // maximum and minimum frequencies are set in sdkconfig,
  // automatic light sleep is enabled if tickless idle support is enabled.
  esp_pm_config_esp32_t pm_config;
  
  pm_config.min_freq_mhz = CONFIG_TLC32_MIN_CPU_FREQ_MHZ;
  /*
  */
//  pm_config.min_freq_mhz = CONFIG_TLC32_MAX_CPU_FREQ_MHZ;
  pm_config.max_freq_mhz = CONFIG_TLC32_MAX_CPU_FREQ_MHZ;
  
#if CONFIG_FREERTOS_USE_TICKLESS_IDLE
  pm_config.light_sleep_enable = true;
  ESP_LOGE(TAG, " Using light_sleep_enable: camera won't work. Try alternative powersave method.");
#else
  pm_config.light_sleep_enable = false;
  ESP_LOGI(TAG, "Not using light_sleep_enable");
#endif
ESP_LOGE(TAG, "TEMP LIGHT SLEEP OVERRIDE AT %d",__LINE__);
pm_config.light_sleep_enable = false;

  ESP_LOGV(TAG, "Initializing DFS: %d to %d Mhz", pm_config.min_freq_mhz, pm_config.max_freq_mhz);

  ESP_ERROR_CHECK(esp_pm_configure(&pm_config));

  // WiFi Modem Power
  ESP_ERROR_CHECK(esp_wifi_set_ps(WIFI_PS_MIN_MODEM)); // default
  // ESP_ERROR_CHECK(esp_wifi_set_ps(WIFI_PS_MAX_MODEM)); // lower power
}

void setup() {
  try {
    ++boot_count;

    //
    // Logging
    //
    esp_log_level_set("*", ESP_LOG_INFO);
    esp_log_level_set("tlc32", ESP_LOG_VERBOSE);
    esp_log_level_set("wifiangel", ESP_LOG_VERBOSE);
    esp_log_level_set("bbesp32lib", ESP_LOG_VERBOSE);
    esp_log_level_set("camera", ESP_LOG_INFO);
    esp_log_level_set("gpio", ESP_LOG_WARN);
    esp_log_level_set("wifi", ESP_LOG_WARN);
    esp_log_level_set("pm-esp32", ESP_LOG_VERBOSE);
    esp_log_level_set("TimeLapseCamera", ESP_LOG_INFO);
    ESP_LOGI(TAG, "tlc32 boot:%d", boot_count);

    //
    // Miscellaneous
    //

    //Serial.begin(115200);
    //Serial.flush();

    //
    //  Sleep to prevent rapid boot-crash-reboot on brownout
    //
    if (rtc_get_reset_reason(0) == 12 /* CPU_SW_RESET */ ||
        rtc_get_reset_reason(0) == 15 /* brownout */) {
      //
      ESP_LOGW(TAG, "Back to sleep... I'm out of it\n");
      ESP.deepSleep(10 /*minutes*/ * 60 * 1000000);
    }

    //
    //  NVS
    //
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
      ESP_LOGW(TAG, "Erasing NVS");
      ESP_ERROR_CHECK(nvs_flash_erase());
      ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);
    //
    // SD Card
    //
    if (SDMMC_init()) {
      LogFile.print(String("Starting. Reset reason: ") + resetReason(0));
      if (0 && LOG_LOCAL_LEVEL >= ESP_LOG_VERBOSE) {
        vector<String> rootFiles = ls("/", 20);
        for (size_t i = 0; i < rootFiles.size(); ++i) {
          printf("%s\n", rootFiles[i].c_str());
        }
        ESP_LOGV(TAG, "--- SD LOG TAIL: ---\n%s------------\n", LogFile.tail(10).c_str());
      }
    } else {
      ESP_LOGE(TAG, "Failed to start SD_MMC filesystem. Dat bad.");
      throw std::runtime_error("Failed to start SD_MMC filesystem. Dat bad.");
    }

    /*
    if (BASIC_CAM_ONLY) {
      ESP_LOGI(TAG, "BASIC_CAM_ONLY active");
      ESP_ERROR_CHECK(my_camera_init());
      while (1) {
        //cam.takeSinglePhoto();
        ESP_ERROR_CHECK(camera_capture());
        vTaskDelay(pdMS_TO_TICKS(10000));
      }
    }
    */

    //
    // Configure cam
    //
    if (cam.load()) {
      ESP_LOGI(TAG, "Loaded from config file");
    } else {
      ESP_LOGI(TAG, "No config file - using defaults");
      // cam._wifiPassword = "wimepuderi";
      cam.configFTP("13.236.175.255" /*monitor.phisaver.com*/, "timelapse", "U88magine!");

      cam.startTakingPhotos();
       cam.configFTP("10.1.1.15", "bbeeson", "imagine");
    }

    ESP_LOGE(TAG, "Temp Override Settings");
    //cam.configFTP("monitor.phisaver.com", "timelapse", "U88magine!");
    cam.configFTP("13.236.175.255", "timelapse", "U88magine!");
    cam._wifiSSID = "NetComm 0405";
    cam._wifiPassword = "wimepuderi";
    cam._uploadMode = TimeLapseCamera::Off;
    //cam.setPsMode(true);
    ////cam.setTakingPhotos(true);

    //
    // Back to sleep (must be after cam start)
    //
    if (rtc_get_reset_reason(0) == 5 /* DEEP_SLEEP */ && cam.sleepy()) {
      cam.sleep();
    }

    //
    // Wifi
    //
    ESP_LOGI(TAG, "Connecting to WiFi...");
    wifi_event_group = xEventGroupCreate();
    assert(wifi_event_group);
    tcpip_adapter_init();
    ESP_ERROR_CHECK(esp_event_loop_init(event_handler, NULL));
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));
    /* Configuration for the provisioning manager */
    wifi_prov_mgr_config_t config = {
        .scheme = wifi_prov_scheme_softap,
        // This can be set to WIFI_PROV_EVENT_HANDLER_NONE when using wifi_prov_scheme_softap*/
        .scheme_event_handler = WIFI_PROV_EVENT_HANDLER_NONE,
        /* Do we want an application specific handler be executed on various provisioning related events */
        .app_event_handler = {
            .event_cb = prov_event_handler,
            .user_data = NULL}};

    /* Initialize provisioning manager with theconfiguration parameters set above */
    ESP_ERROR_CHECK(wifi_prov_mgr_init(config));

    bool provisioned = false;
    ESP_ERROR_CHECK(wifi_prov_mgr_is_provisioned(&provisioned));

    /* If device is not yet provisioned start provisioning service */
    if (!provisioned) {
      ESP_LOGI(TAG, "Starting provisioning");
      const char *service_key = "password";
      /* Start provisioning service */
      
      ESP_ERROR_CHECK(wifi_prov_mgr_start_provisioning(WIFI_PROV_SECURITY_0, 0 /* pop */, "tlc32", service_key));
      
      // Wait for connection
      wifi_prov_mgr_wait();
      
        // wifi_prov_mgr_deinit(); // handled in event handler
    } else {
      ESP_LOGI(TAG, "Already provisioned, starting Wi-Fi as STA");
      /* We don't need the manager as device is already provisioned, so let's release it's resources */
      wifi_prov_mgr_deinit();
      /* Start Wi-Fi station */
      ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
      ESP_ERROR_CHECK(esp_wifi_start());
    }
    // Wait for Wi-Fi connection
    

    xEventGroupWaitBits(wifi_event_group, WIFI_CONNECTED_BIT, false, true, portMAX_DELAY);
    
    // deinit is handled by the event handler.
    ESP_LOGI(TAG, "Connected to Wi-Fi as STA");

    setSystemTime();

    //
    // Begin Cam
    //
    ESP_LOGV(TAG, "%s", cam.toString().c_str());
    LogFile.print(cam.toString());
    cam.taskify();

    //
    // Webserver. FreeRTOS priority 3.
    //
    
    webServer.addHandler(&socketServer);
    webServer.begin();
    ESP_LOGE(TAG, "NO power save");
    //init_power_save();
    ESP_LOGI(TAG, "Setup complete.");
    // LogFile.print(String("Setup complete. WiFi: ") + String(WiFi.status() ==
    // WL_CONNECTED));
  } catch (std::exception &e) {
    ESP_LOGE(TAG, "Setup Exception: %s", e.what());
    LogFile.print(e.what());
    ESP.deepSleep(10 /*minutes*/ * 60 * 1000000);
  }
}

void loop() {
  try {
    vTaskDelay(pdMS_TO_TICKS(60000));
    //vTaskPrintRunTimeStats();
  } catch (std::exception &e) {
    ESP_LOGE(TAG, "Loop Exception: %s", e.what());
    LogFile.print(e.what());
    ESP.deepSleep(10 /*minutes*/ * 60 * 1000000);
  }
}

extern "C" {
void app_main() {
  setup();
  while (1) {
    loop();
  }
}
}
