static const char *TAG = "TimeLapseCamera";
#undef LOG_LOCAL_LEVEL
#define LOG_LOCAL_LEVEL ESP_LOG_VERBOSE
#define INCLUDE_pcGetTaskName 1
#include "TimeLapseCamera.h"
#include "BBEsp32Lib.h"
#include "FS.h"
#include "FTPUploader.h"
#include "SD_MMC.h"
#include "TimeLapseWebServer.h"
#include "esp_camera.h"
#include "esp_log.h"
#include <freertos/task.h>
#include <fstream>
#include <sys/stat.h>

#include "esp_pm.h"

// CAMERA_MODEL_AI_THINKER
#define PWDN_GPIO_NUM 32
#define RESET_GPIO_NUM -1
#define XCLK_GPIO_NUM 0
#define SIOD_GPIO_NUM 26
#define SIOC_GPIO_NUM 27
#define Y9_GPIO_NUM 35
#define Y8_GPIO_NUM 34
#define Y7_GPIO_NUM 39
#define Y6_GPIO_NUM 36
#define Y5_GPIO_NUM 21
#define Y4_GPIO_NUM 19
#define Y3_GPIO_NUM 18
#define Y2_GPIO_NUM 5
#define VSYNC_GPIO_NUM 25
#define HREF_GPIO_NUM 23
#define PCLK_GPIO_NUM 22

//                                   M  S  MS
#define BATTERY_IS_CHARGED_PERIOD_MS 1 * 60 * 1000

RTC_DATA_ATTR int TimeLapseCameraFileNumber = 0;

// Local function for RTOS - must be non-class

using namespace bbesp32lib;

void TakePhotosTask(void *timeLapseCamera);
void UploadPhotosTask(void *timeLapseCamera);
void MonitorTask(void *timeLapseCamera);

TimeLapseCamera::TimeLapseCamera(fs::FS &filesys, String saveFolder,
                                 int takePhotoPeriodS, int uploadPhotosMaxS)
    : _filesys(filesys), _saveFolder(saveFolder),
      _takePhotoPeriodS(takePhotoPeriodS), _uploadPhotosMaxS(uploadPhotosMaxS) {
  _saveFolder = slash(_saveFolder, '/');

  for (int i = 0; i < 24; i++) {
    _sleepHours[i] = false;
  }
}

void TimeLapseCamera::configFTP(const String &host, const String &user,
                                const String &password) {
  if (_ftp) {
    if (_ftp->_host == host && _ftp->_user == user &&
        _ftp->_password == password) {
      return; // no change
    }
    delete (_ftp);
    _ftp = NULL;
  }
  _ftp = new FTPUploader(host, user, password);
}

uint32_t TimeLapseCamera::sleepy() {
  time_t now;
  struct tm timeinfo;
  struct tm nextHour;
  char strftime_buf[64];
  uint32_t msToSleep;

  time(&now);
  localtime_r(&now, &timeinfo);
  strftime(strftime_buf, sizeof(strftime_buf), "%Y-%m-%dT%H-%M-%S", &timeinfo);

  if (!_sleepEnabled)
    return false;
  if (timeinfo.tm_year < (2016 - 1900)) {
    ESP_LOGI(TAG, "System time not set.");
  }

  // Find seconds until next o'clock
  nextHour = timeinfo;
  nextHour.tm_hour++;
  nextHour.tm_sec = 0;
  nextHour.tm_min = 0;
  msToSleep = (uint32_t)(difftime(now, mktime(&nextHour)) * 1000.0f);

  if (_sleepHours[timeinfo.tm_hour]) {
    ESP_LOGI(TAG, "Sleepy. hourNow: %d now: %s secondsToSleep: %ds", timeinfo.tm_hour,
             strftime_buf, (int)msToSleep);
    return msToSleep;
  } else {
    ESP_LOGI(TAG, "Not Sleepy. hourNow: %d now: %s secondsToSleep: %ds",
             timeinfo.tm_hour, strftime_buf, (int)msToSleep);
    return 0;
  }
}

bool TimeLapseCamera::testFTPConnection() {
  assert(_ftp);
  return _ftp->testConnection();
}

// quality: 0-63, lower means higher quality
void TimeLapseCamera::initCam() {

  if (_camInitialised) {
    return;
  }
  struct stat sb;
  String saveFolderBare = slash("/", _saveFolder, "").c_str();
  ESP_LOGV(TAG, "initCam saveFolder:%s", saveFolderBare.c_str());
  if (!(stat(saveFolderBare.c_str(), &sb) == 0 && S_ISDIR(sb.st_mode))) {
    if (mkdir(saveFolderBare.c_str(), S_IRWXU | S_IRWXG | S_IRWXO) != 0) {
      String e = String("Couldn't create folder at ") + saveFolderBare;
      throw std::runtime_error(e.c_str());
    }
  } else {
    String e = String("Folder exists at ") + saveFolderBare;
    ESP_LOGV(TAG, "%s", e.c_str());
  }
  // power up the camera if PWDN pin is defined
  if (PWDN_GPIO_NUM != -1) {
    pinMode(PWDN_GPIO_NUM, OUTPUT);
    digitalWrite(PWDN_GPIO_NUM, LOW);
  }

  camera_config_t config;
  esp_err_t cam_err;
  int psramStartkB = ESP.getFreePsram() / 1024;

  config.ledc_channel = LEDC_CHANNEL_0;
  config.ledc_timer = LEDC_TIMER_0;
  config.pin_d0 = Y2_GPIO_NUM;
  config.pin_d1 = Y3_GPIO_NUM;
  config.pin_d2 = Y4_GPIO_NUM;
  config.pin_d3 = Y5_GPIO_NUM;
  config.pin_d4 = Y6_GPIO_NUM;
  config.pin_d5 = Y7_GPIO_NUM;
  config.pin_d6 = Y8_GPIO_NUM;
  config.pin_d7 = Y9_GPIO_NUM;
  config.pin_xclk = XCLK_GPIO_NUM;
  config.pin_pclk = PCLK_GPIO_NUM;
  config.pin_vsync = VSYNC_GPIO_NUM;
  config.pin_href = HREF_GPIO_NUM;
  config.pin_sscb_sda = SIOD_GPIO_NUM;
  config.pin_sscb_scl = SIOC_GPIO_NUM;
  config.pin_pwdn = PWDN_GPIO_NUM;
  config.pin_reset = RESET_GPIO_NUM;
  config.xclk_freq_hz = 20000000;
  config.pixel_format = PIXFORMAT_JPEG;
  // init with high specs to pre-allocate larger buffers

  config.frame_size = _frameSize;
  config.jpeg_quality = _quality; // 0-63 lower means higher quality
  config.fb_count = 1;

  /* if (!psramFound()) {
    // oh no, we're too littlely
    // throw std::runtime_error("No psramFound. Not tested without it!");
    ESP_LOGW(TAG,
             "No PSRAM found. Might fail. Check menuconfig and enable PSRAM.");
  }
  */

  int initTries = 0;
  while ((cam_err = esp_camera_init(&config) != ESP_OK) && initTries++ < 10) {
    delay(6000);
  }

  if (cam_err != ESP_OK) {
    String err = "Camera init failed with error: " + String(cam_err, HEX);
    throw std::runtime_error(err.c_str());
  }

  ESP_LOGI(TAG, "esp_camera_init ok. Size: %d Used %dkB Psram", _frameSize,
           psramStartkB - (ESP.getFreePsram() / 1024));
  _camInitialised = true;
}

void TimeLapseCamera::deinitCam() {
  esp_err_t camErr;
  if (!_camInitialised)
    return;
  if ((camErr = esp_camera_deinit()) != ESP_OK) {
    ESP_LOGE(TAG, "esp_camera_deinit() failed: %d", camErr); // esp_err_t type?
  }
  _camInitialised = false;
}

void TimeLapseCamera ::stopTakingPhotos() { _camTakePhotos = false; }
void TimeLapseCamera ::startTakingPhotos() {
  if (!_camTakePhotos) {
    ESP_LOGV(TAG, "startTakingPhotos");
    _camTakePhotos = true;
  }
}

void TimeLapseCamera::taskify(int takePriority, int uploadPriority) {
  // use angel! todo: check
#if 0
    xTaskCreatePinnedToCore(MonitorTask, "MonitorTask",
                            3000 /* stack depth */,
                            (void *)this, /* parameters */
                            2,            /* just above idle */
                            NULL /* task handle */,
                            1 /* core */);
#endif
  xTaskCreatePinnedToCore(
      TakePhotosTask, "TakePhotosTask", 5000 /* stack depth */,
      (void *)this, /* parameters */
      takePriority, &_takePhotosTaskHandle /* task handle */, 1 /* core */);

  xTaskCreatePinnedToCore(UploadPhotosTask, "UploadPhotosTask",
                          7500 /* stack depth */, (void *)this, /* parameters */
                          uploadPriority,                       /* less important than taking photo */
                          &_uploadPhotosTaskHandle /* task handle */,
                          1 /* core */);
}

/*
   Take one photo and save to SD Card
*/
void TimeLapseCamera::takeSinglePhoto() {

  camera_fb_t *f = NULL;
  String ts;
  String filename;

  static esp_pm_lock_handle_t nosleep_handle = 0;
  static esp_pm_lock_handle_t freq_handle = 0;
  static esp_pm_lock_handle_t apb_handle = 0;
  if (!nosleep_handle) {
    ESP_LOGE(TAG, "Creating Lock");
    ESP_ERROR_CHECK(esp_pm_lock_create(ESP_PM_CPU_FREQ_MAX, 0, "cam1", &freq_handle));
    ESP_ERROR_CHECK(esp_pm_lock_create(ESP_PM_APB_FREQ_MAX, 0, "cam2", &apb_handle));
    ESP_ERROR_CHECK(esp_pm_lock_create(ESP_PM_NO_LIGHT_SLEEP, 0, "cam3", &nosleep_handle));
  }
  //ESP_LOGE(TAG, "TEMP: TURN OFF PS BEFORE CAMERA");
  //ESP_ERROR_CHECK(esp_pm_lock_acquire(nosleep_handle));
  //ESP_ERROR_CHECK(esp_pm_lock_acquire(apb_handle));
  //ESP_ERROR_CHECK(esp_pm_lock_acquire(freq_handle));
  //esp_pm_config_esp32_t pm_config;
  //pm_config.max_freq_mhz = 240;
  //pm_config.min_freq_mhz = 240;
  //pm_config.light_sleep_enable = false;
  
//  ESP_ERROR_CHECK(esp_pm_configure(&pm_config));

  initCam();

  ESP_LOGV(TAG, "Taking a photo...");
  f = esp_camera_fb_get(); // 300ms
  ESP_LOGV(TAG, "Got a photo. Saving...");
  if (!f) {
    assert(0);
    
    throw std::runtime_error("Failed to get fb with esp_camera_fb_get()");
  }

  if (timeIsValid()) {
    // Timestamped
    // Round to nearest interval of photo taking
    time_t now;
    time(&now);
    time_t nowRounded = mround_time_t(now, _takePhotoPeriodS);
    ts = timestamp(nowRounded);
    // Careful! filesystem doesn't like weird names (fails to write)
    filename = _saveFolder + ts + String(".jpg");
  } else {
    // Sequence. No time is available
    char ibuf[8 + 1]; // 9, 999, 999 + 1 for \0 is 3 years @ 10s
    snprintf(ibuf, 8, "%06d", ++TimeLapseCameraFileNumber);
    filename = _saveFolder + _series + String(ibuf) + ".jpg";
    // Prepend (aa,ab,etc) to avoid overwriting sequences
    // If we get to "zz", start overwriting
    // Check if destination file exists before renaming
    struct stat st;

    // ESP_LOGV(TAG,"stat = %d",stat(filename.c_str(), &st));

    while ((stat(filename.c_str(), &st) == 0) && (_series != "zz")) {
      filename = _saveFolder + nextSeries() + String(ibuf) + ".jpg";
    }
  }
  // To avoid overwriting, add a prefix (A-Z). This is easily stripped out
  // in post-processing, cos its a non-digit

  ESP_LOGV(TAG, "Saving %s (%dkB)", filename.c_str(), f->len / 1024);

  // \todo Prevent other tasks readings a half-written file

  // FILE*.open takes 900ms, but much better than 2500ms for fs::File.open
  FILE *file;
  int bytesWritten = 0;
  _lastImage =
      filename; // lastImage will need  /sdcard/ removed for retrival via URL

  

  if ((file = fopen(filename.c_str(), "w"))) { // 900ms
    ESP_LOGI(TAG, "Saving: fn:%s (%dkB) (%dx%d)", filename.c_str(),
             (int)(f->len / 1000), f->width, f->height);
    bytesWritten = fwrite(f->buf, 1, f->len, file);
    // ESP_LOGV(TAG,"T2.2 Wrote");
    if (bytesWritten != f->len) {
      ESP_LOGW(TAG, "Only wrote: %dB of %dB to %s", (int)bytesWritten, (int)f->len,
               filename.c_str());
    }
  } else {
    esp_camera_fb_return(f);
    assert(0);
   
    throw std::runtime_error(String("Could not save file:" + filename).c_str());
  }
  fclose(file);
  file = NULL;
  esp_camera_fb_return(f);
  ESP_LOGE(TAG, "TEMP: TURN ON PS AFTER CAMERA");
 // ESP_ERROR_CHECK(esp_pm_lock_release(nosleep_handle));
  //ESP_ERROR_CHECK(esp_pm_lock_release(apb_handle));
  //ESP_ERROR_CHECK(esp_pm_lock_release(freq_handle));
  //pm_config.max_freq_mhz = 240;
  //pm_config.min_freq_mhz = 80;
  //pm_config.light_sleep_enable = true;
  //ESP_ERROR_CHECK(esp_pm_configure(&pm_config));

}

void TimeLapseCamera::takeSinglePhotoAndDiscard() {
  camera_fb_t *fb = NULL;

  ESP_LOGE(TAG, "TEMP RETURN");

  initCam();

  fb = esp_camera_fb_get();
  if (fb) {
    esp_camera_fb_return(fb);
  }
}

/**
   Return true if all possible photos uploaded
   throws
*/
bool TimeLapseCamera::uploadPhotos(int maxFiles, int maxSeconds) {

  String localDirSlashed;
  String localDir;
  String remoteDir;
  std::vector<String> filesToUpload;
  int nFiles = 0;
  int endSeconds = millis() / 1000 + maxSeconds;
  uint32_t s;

  if (!_ftp)
    return true; // all possible photos uploaded

  assert(maxFiles > 0);

  localDirSlashed = slash("/", _saveFolder, "/");
  localDir = slash("/", _saveFolder, "");
  remoteDir = slash("", _saveFolder, "/");
  // remoteDir = "tlc/"; // could make configurable or use last folder in
  // localDir

  s = millis();

  // will not work for file called localDir
  // todo add setSaveFolder(asdf) to do this
  //
  if (!_filesys.exists(localDir)) {
    ESP_LOGI(TAG, "mkdir %s", localDir.c_str());
    _filesys.mkdir(localDir);
  }
  // Get all to enable sort and get eariliest, even if maxFiles is set - do that
  // later.
  filesToUpload = ls(localDir, maxFiles, ".jpg");
  /*
  Serial.print(localDir); Serial.print(",");
  Serial.print(millis() - s); Serial.print(",");
  Serial.print((long) filesToUpload.size()); Serial.print(",");
  Serial.print(remoteDir); Serial.println("");
  Serial.flush();
*/

  ESP_LOGI(TAG, "uploadPhotos: ls:%ums localDir:%s (%u files) remoteDir:%s",
           (uint32_t)(millis() - s), localDir.c_str(),
           (uint32_t)filesToUpload.size(), remoteDir.c_str());

  for (size_t i = 0; i < filesToUpload.size(); ++i) {
    // Serial.println(filesToUpload[i]);
  }

  if (filesToUpload.size() < 2)
    return true; // all possible photos uploaded (leave one file)

  std::sort(filesToUpload.begin(), filesToUpload.end());

  filesToUpload.pop_back(); // Never upload the last photo - use for a preview

  _wiFiUsers++;

  _ftp->makeDir(remoteDir);

  for (size_t i = 0; i < filesToUpload.size(); ++i) {
    s = millis();
    _ftp->uploadFile(localDirSlashed + filesToUpload[i],
                     remoteDir + filesToUpload[i]);
    // ESP_LOGV(TAG,"upload took %lums", millis() - s);
    _lastImageUploaded = filesToUpload[i];
    s = millis();
    if (_deleteOnUpload) {
      String fn = localDirSlashed + filesToUpload[i];
      if (remove(fn.c_str()) != 0) {
        perror("Error deleting file");
        ESP_LOGW(TAG, "Couldn't remove: %s", fn.c_str());
      }
    }
    // ESP_LOGV(TAG,"delete took %lums", millis() - s);
    if (++nFiles > maxFiles || millis() / 1000 > endSeconds) {
      ESP_LOGI(TAG, "uploadPhotos: returning early _upload:%d nFiles:%d of %d",
               _uploadMode, nFiles, maxFiles);
      _wiFiUsers--;
      return false; // more possible photos to upload
    }
  }
  _wiFiUsers--;
  return true; // all possible photos uploaded
}

bool TimeLapseCamera::isCharged() {
  bool bcAtm; // charging at the moment

  if (!_lastBatteryCheckMs) {
    pinMode(_chargePin, INPUT_PULLUP);
  }

  // active low for open drain to adafruit solar charge
  bcAtm = digitalRead(_chargePin) == LOW;

  // aka  _chargingForMs = bcAtm * (_chargingForMs+millis() -
  // _lastBatteryCheck);
  if (bcAtm) {
    _chargingForMs += millis() - _lastBatteryCheckMs;
  } else {
    _chargingForMs = 0;
  }

  _lastBatteryCheckMs = millis();

  // ESP_LOGI(TAG,"chargingForMs: %juminutes, atmBatteryCharging: %ju", (uintmax_t)
  // _chargingForMs, (uintmax_t)bcAtm);
  return (_chargingForMs > BATTERY_IS_CHARGED_PERIOD_MS);
}

void TimeLapseCamera::taskStates() {

  // TaskHandle_t cth = xTaskGetCurrentTaskHandle();
  // UBaseType_t nt = uxTaskGetNumberOfTasks( );
  // char tl[1000];
  // char rts[1000];
  // vTaskList(tl);
  // vTaskGetRunTimeStats(rts);
  // ESP_LOGV(TAG,"cth: %d nt:%d", (int) cth, (int) nt);
  // ESP_LOGV(TAG,"tl: %s", tl);
  // Serial.printf("**********RTS**************\n%s", rts);
  // Serial.printf("**********TL**************\n%s", tl);
  vTaskPrintRunTimeStats();
  /*
  if (_uploadPhotosTaskHandle) {
  taskState(_uploadPhotosTaskHandle);
  }
  if (_takePhotosTaskHandle) {
   taskState(_uploadPhotosTaskHandle);
  }
*/
}
// TaskHandle_t _takePhotosTaskHandle = NULL;

bool TimeLapseCamera::save() {
  DynamicJsonDocument doc = getConfig();
  if (doc.size() == 0)
    return false;
  return saveConfigToFile(doc);
}

String TimeLapseCamera::toString() {
  DynamicJsonDocument doc = getConfig();
  String jsonString;
  if (doc.size() == 0)
    return "";
  if (serializeJson(doc, jsonString) == 0) {
    ESP_LOGE(TAG, "Failed to serialize");
    return "";
  } else {
    return jsonString;
  }
}

bool TimeLapseCamera::load() {
  DynamicJsonDocument doc = readConfigFromFile();
  if (doc.size() == 0)
    return false;
  applyConfig(doc);
  return true;
}

void TimeLapseCamera::applyConfig(const DynamicJsonDocument &data) {
  // Must use containsKey, as (bool) data["saveFolder"] == false (why?)
  // Serial.println("applyConfig with:");
  // serializeJson(data, Serial);

  if (data.containsKey("saveFolder")) {
    _saveFolder = data["saveFolder"].as<String>();
    _saveFolder = slash(_saveFolder, '/');
  }

  setIfValid(_uploadMode, "uploadMode", Off, Auto, data);

  bool sleepChanged = setIfValid(_sleepAt, "sleepAt", 0, 24, data);
  bool wakeChanged = setIfValid(_wakeAt, "wakeAt", 0, 24, data);
  if (sleepChanged || wakeChanged) {
    for (int h = 0; h < 24; h++) {
      _sleepHours[h] = (h < _wakeAt) || (h >= _sleepAt);
      // ESP_LOGV(TAG,"h: %d sleep:%d", h, _sleepHours[h]);
    }
  }
  setIfValid(_takePhotoPeriodS, "takePhotoPeriodS", 1, 3600, data);
  setIfValid(_sleepEnabled, "sleepEnabled", 0, 1, data);
  setIfValid(_camTakePhotos, "camTakePhotos", false, true, data);
  setIfValid(_wifiSSID, "wifiSSID", 1, data);
  setIfValid(_wifiPassword, "wifiPassword", 0, data);

  if (setIfValid(_frameSize, "frameSize", FRAMESIZE_QQVGA, FRAMESIZE_UXGA,
                 data)) {
    ESP_LOGV(TAG, "Setting cam with framesize %d _camTakePhotos: %d", (int)_frameSize,
             _camTakePhotos);
    bool takingPhotos = _camTakePhotos;
    _camTakePhotos = false; // stop to avoid re-initing
    vTaskDelay(
        pdMS_TO_TICKS(2000)); // maximum take-photo time to allow finishment
    deinitCam();              // next "takeSinglePhoto" will re-init
    _camTakePhotos = takingPhotos;
  }

  if (data.containsKey("ftpServer")) {
    configFTP(data["ftpServer"].as<String>(), data["ftpUsername"].as<String>(),
              data["ftpPassword"].as<String>());
  }
}

DynamicJsonDocument TimeLapseCamera::getConfig() {

  DynamicJsonDocument data(JSON_OBJECT_SIZE(30));

  data["saveFolder"] = _saveFolder.c_str();

  data["uploadMode"] = (int)_uploadMode;
  data["sleepAt"] = _sleepAt;
  data["wakeAt"] = _wakeAt;
  data["sleepEnabled"] = (int)_sleepEnabled;
  data["takePhotoPeriodS"] = _takePhotoPeriodS;
  if (_ftp) {
    data["ftpServer"] = _ftp->_host.c_str();
    data["ftpPort"] = _ftp->_port;
    data["ftpUsername"] = _ftp->_user.c_str();
    data["ftpPassword"] = _ftp->_password.c_str(); // secure as a beach
  }
  data["camTakePhotos"] = (int)_camTakePhotos;
  data["wifiPassword"] = _wifiPassword.c_str();
  data["wifiSSID"] = _wifiSSID.c_str();
  data["frameSize"] = (int)_frameSize;
  Serial.print("getConfig:");
  // serializeJsonPretty(data, Serial);
  return data; // copies
}

//
// Read the configuration from a file. Doesn't apply it - use applyConfig().
//

DynamicJsonDocument TimeLapseCamera::readConfigFromFile() {

  std::ifstream file(_configFilename.c_str());

  // ArduinoJson::StdStreamReader::makeReader();

  ARDUINOJSON_NAMESPACE::StdStreamReader ssr(file);
  // FILE* file = _filesys.open(_configFilename);

  DynamicJsonDocument doc(JSON_OBJECT_SIZE(30));
  DeserializationError error = deserializeJson(doc, file);
  file.close(); // ~File does not close
  if (error) {
    ESP_LOGE(TAG, "Failed to read config file: %s error: %s", _configFilename.c_str(),
             error.c_str());
  }
  return doc; // empty doc if error
}

//
// Saves the configuration to a file
//
bool TimeLapseCamera::saveConfigToFile(const DynamicJsonDocument &doc) {

  // Delete existing file, otherwise the configuration is appended to the file
  _filesys.remove(_configFilename);

  // Open file for writing
  File file = _filesys.open(_configFilename, FILE_WRITE);
  if (!file) {
    ESP_LOGE(TAG, "Failed to open config file: %s", _configFilename.c_str());
    return false;
  }

  if (serializeJson(doc, file) == 0) {
    ESP_LOGE(TAG, "Failed to serialize to config file: %s", _configFilename.c_str());
    return false;
  }

  // Close the file
  file.close();
  return true;
}

DynamicJsonDocument TimeLapseCamera::status() {
  DynamicJsonDocument doc(JSON_OBJECT_SIZE(1) + JSON_OBJECT_SIZE(30));
  String json;
  doc["event"] = "status";
  JsonObject data = doc.createNestedObject("data");
  data["freeheap"] = ESP.getFreeHeap();
  data["datetime"] = timestamp();
  data["uptime"] = millis();
  data["charging"] = (digitalRead(_chargePin) == LOW);
  data["chargingForMs"] = _chargingForMs;
  data["nImages"] = _nImages;
  // Use c_str for JSON to avoid copy (also, String doesn't work!)
  data["firstImage"] = _firstImage.c_str();
  data["lastImage"] = _lastImage.c_str();
  data["lastImageUploaded"] = _lastImageUploaded.c_str();
  data["overWorked"] = _overWorked;

  return doc; // copies
}

bool TimeLapseCamera::setIfValid(String &value, const char *key, int minLength,
                                 const DynamicJsonDocument &data) {
  if (data.containsKey(key)) {
    String proposedValue = data[key].as<String>();
    if (proposedValue.length() >= minLength) {
      if (proposedValue != value) {
        value = proposedValue;
        return true;
      }
    } else {
      ESP_LOGW(TAG, "%s: %s is invalid. Ignored", key, value.c_str());
    }
  }
  return false;
}
bool TimeLapseCamera::setIfValid(bool &value, const char *key,
                                 const int &minimum, const int &maximum,
                                 const DynamicJsonDocument &data) {

  if (data.containsKey(key)) {
    int proposedValue = data[key].as<int>();
    if (proposedValue == 0 || proposedValue == 1) {
      if (proposedValue != value) {
        value = proposedValue;
        return true;
      }
    } else {
      ESP_LOGW(TAG, "%s: %d is invalid. Ignored", key, value);
    }
  }
  return false;
}

bool TimeLapseCamera::setIfValid(int &value, const char *key,
                                 const int &minimum, const int &maximum,
                                 const DynamicJsonDocument &data) {
  if (data.containsKey(key)) {
    int proposedValue = data[key].as<int>();
    if (proposedValue >= minimum && proposedValue <= maximum) {
      if (proposedValue != value) {
        value = (framesize_t)proposedValue;
        return true;
      }
    } else {
      ESP_LOGW(TAG, "%s: %d is invalid. Ignored", key, value);
    }
  }

  return false;
}

bool TimeLapseCamera::setIfValid(OffOnAuto &value, const char *key,
                                 const OffOnAuto &minimum,
                                 const OffOnAuto &maximum,
                                 const DynamicJsonDocument &data) {
  if (data.containsKey(key)) {
    TimeLapseCamera::OffOnAuto proposedValue =
        (TimeLapseCamera::OffOnAuto)data[key].as<int>();
    if (proposedValue >= minimum && proposedValue <= maximum) {
      if (proposedValue != value) {
        value = proposedValue;
        return true;
      }
    } else {
      ESP_LOGW(TAG, "%s: %d is invalid. Ignored", key, value);
    }
  }

  return false;
}

bool TimeLapseCamera::setIfValid(framesize_t &value, const char *key,
                                 const framesize_t minimum,
                                 const framesize_t maximum,
                                 const DynamicJsonDocument &data) {

  if (data.containsKey(key)) {
    int proposedValue = data[key].as<int>();
    // ESP_LOGV(TAG,"Frame Set %d key:%s pv:%d min:%d max:%d", (int)value, key,
    // proposedValue, (int) minimum, (int) maximum);
    if (proposedValue >= (int)minimum && proposedValue <= (int)maximum) {
      if ((int)proposedValue != (int)value) {
        // No change required
        value = (framesize_t)proposedValue;
        return true;
      }
    } else {
      ESP_LOGW(TAG, "Frame Set %s: %d is invalid. Ignored", key, value);
    }
  }
  return false;
}

void TimeLapseCamera::sleep() {
  uint32_t ms = sleepy();

  if (ms < 1000)
    return;
  String m = "Deepsleep for " + String(ms) + "ms";
  ESP_LOGI(TAG, "%s", m.c_str());
  stopTakingPhotos();
  _uploadMode = TimeLapseCamera::Off;
  ESP_LOGW(TAG, "Close wifi and any other systems! Todo!");
  vTaskDelay(pdMS_TO_TICKS(10000)); // let finish up photos and uploading
  ESP.deepSleep(max((uint32_t)1000, (ms - 10000)) * 1000);
}

void TimeLapseCamera::updateFileStats() {
  String localDir = slash("/", _saveFolder, "");

  if (!lsStats(localDir, &_nImages, &_firstImage, &_lastImage, ".jpg")) {
    ESP_LOGE(TAG, "Failed to get lsStats");
  }
}
/******************************************************************
   RTOS Task Functions
*******************************************************************/

/**
   Take a photo every _takePhotoPeriodS seconds.
   Try to take on a multiple of seconds (ie time_t = 0 + period * t)
   Take immediately if overrun, or wait until exact time if underrun.

*/

// time_t unsigned long
void TakePhotosTask(void *timeLapseCamera) {
  time_t tickZero_Secs; // time (in epoch seconds) when ticks=0
  time_t now_Secs;      // eg. 342342344
  TickType_t now_Tcks;
  signed long long lastWakeAgo_Tcks; // can be -ve
  time_t lastWakeTime_Secs;
  TickType_t lastWakeTime_Tcks;
  TimeLapseCamera *tlc = (TimeLapseCamera *)timeLapseCamera;

  // As TickType_t is unsigned, and we have to specify the *previous* waketime,
  // we must wait until the previous waketime is positive (ie. timenow > period)
  // what a sneeky

  while (xTaskGetTickCount() < (tlc->_takePhotoPeriodS * configTICK_RATE_HZ)) {
    delay(100);
  }
  delay(500);
  now_Tcks = xTaskGetTickCount(); // 13889
  now_Secs = time(NULL);          // 1559617105
  assert(tlc);
  assert(configTICK_RATE_HZ);
  assert(tlc->_takePhotoPeriodS);

  tickZero_Secs = now_Secs - (now_Tcks / configTICK_RATE_HZ); // 1559617102
  lastWakeTime_Secs =
      ((time_t)now_Secs / tlc->_takePhotoPeriodS) * tlc->_takePhotoPeriodS;
  lastWakeAgo_Tcks =
      now_Tcks - (lastWakeTime_Secs - tickZero_Secs) * configTICK_RATE_HZ;
  lastWakeTime_Tcks = now_Tcks - lastWakeAgo_Tcks;
  /*
  Serial.print("now_Tcks:"); Serial.println(now_Tcks);
  Serial.print("now_Secs:");  Serial.println(now_Secs);                 //
  Serial.print("tickZero_Secs:"); Serial.println(tickZero_Secs);
  Serial.print("lastWakeAgo_Tcks:"); Serial.println((signed long int)
  lastWakeAgo_Tcks); Serial.print("lastWakeTime_Secs:");
  Serial.println(lastWakeTime_Secs); Serial.print("lastWakeTime_Tcks:");
  Serial.println(lastWakeTime_Tcks);
*/
  ESP_LOGI(TAG, "TakePhotosTask has begun! Period = %ds", tlc->_takePhotoPeriodS);

  int nExceptions = 0;
  for (;;) {
    vTaskDelayUntil(&lastWakeTime_Tcks,
                    pdMS_TO_TICKS(tlc->_takePhotoPeriodS * 1000));
    try {
      if (tlc->_camTakePhotos && !tlc->sleepy()) {

        tlc->takeSinglePhoto();
        nExceptions = max(0, nExceptions - 1);
      }
    } catch (const std::runtime_error &e) {
      if (++nExceptions > 10) {
        ESP_LOGE(TAG, "Too many exceptions. Restarting.");
        LogFile.print("TakePhotosTask: Too many exceptions. Restarting.");
        ESP.restart();
      } else {
        ESP_LOGE(TAG, "Exception[%dth]: %s. Retrying.", nExceptions, e.what());
      }
    }
    if (xTaskGetTickCount() >
        (lastWakeTime_Tcks + pdMS_TO_TICKS(tlc->_takePhotoPeriodS * 1000))) {
      tlc->_overWorked++;
    }
  }
}

/**
   Upload photos continuously whilst the "upload" flag is set
   Poll for it
*/
void UploadPhotosTask(void *timeLapseCamera) {

  TimeLapseCamera *tlc = (TimeLapseCamera *)timeLapseCamera;
  TickType_t lastWakeTime_Tcks = xTaskGetTickCount();
  String localDirSlashed;
  String remoteDir;

  ESP_LOGV(TAG, "UploadPhotosTask has begun!");
  int nExceptions = 0;
  for (;;) {
    //
    // Upload if required
    //
    // ESP_LOGV(TAG,"Check uploading: charged:%d _uploadMode:%d WiFi:%d",
    // tlc->isCharged(), tlc->_uploadMode, WiFi.status() == WL_CONNECTED);
    try {
      if ((tlc->_uploadMode == TimeLapseCamera::Auto && tlc->isCharged()) ||
          tlc->_uploadMode == TimeLapseCamera::On) {

        // check for wifi connection? if (WiFiAngel.status() == WIFI_MODE_STA) {
        while (!tlc->uploadPhotos() &&
               ((xTaskGetTickCount() - lastWakeTime_Tcks) /
                configTICK_RATE_HZ) < tlc->_uploadPhotosMaxS) {
          // uploads in batches
          // tlc->uploadPhotos() returns true when complete
        }
        nExceptions = 0;
        // Upload the log file
        localDirSlashed = slash("/", tlc->_saveFolder, "/");
        remoteDir = slash("", tlc->_saveFolder, "/");
        tlc->_ftp->uploadFile(localDirSlashed + LogFile.getFilename(),
                              remoteDir + LogFile.getFilename());
      }
    } catch (const std::runtime_error &e) {
      nExceptions++;
      if (nExceptions >= 3) {
        ESP_LOGE(TAG, "Upload failed 3 times. Stopping. %s", e.what());
        LogFile.print("Upload failed 3 times. Stopping. " + String(e.what()));
        tlc->_uploadMode = TimeLapseCamera::Off;
      } else {
        vTaskDelay(
            pdMS_TO_TICKS(60 * 1000)); // wait a bit before checking again
      }
    }
    vTaskDelay(pdMS_TO_TICKS(5 * 1000)); // wait a bit before checking again
  }
}

/**
   Manage wifi, sleep, events, etc.
*/
void MonitorTask(void *timeLapseCamera) {

  TimeLapseCamera *tlc = (TimeLapseCamera *)timeLapseCamera;

  uint32_t updateFileStatsTimer = millis();

  // Wait a minute before adjusting anything
  // to allow users to connect before Wifi.Off or Sleep
  vTaskDelay(60000 / portTICK_PERIOD_MS);

  ESP_LOGV(TAG, "MonitorTask has begun!");

  for (;;) {
    try {
      //
      // Update filesystem stats (first, last, #) once a minute if WiFi on
      //
      if (millis() - updateFileStatsTimer > 60000) {
        //WiFiAngel.status() == WIFI_MODE_STA) {
        tlc->updateFileStats();
        updateFileStatsTimer = millis();
      }
      //
      // Send a ping
      //

      //
      // Sleep
      //
      // Wait 1 minute before thinking of sleeping to allow connect on hard
      // reset
      uint32_t sleepS;
      if (millis() > 60 * 1000) {
        // Sleep if bedtime and not on charge
        // Awake if bedtime and on charge, to allow upload
        if ((sleepS = tlc->sleepy()) && !tlc->isCharged()) {
          tlc->sleep();
        }
      }
      vTaskDelay(pdMS_TO_TICKS(5 * 1000)); // wait a bit before checking again

    } catch (const std::runtime_error &e) {
      ESP_LOGE(TAG, "Exception: %s", e.what());
      LogFile.print("Exception: " + String(e.what()));
    }

  } // for(ever)
}

/**
   base 24 (a-z)
   aa, ab, ac ... zz, aa
*/
String TimeLapseCamera::nextSeries() {

  // c1 | c2
  char c1;
  char c2;
  // asc
  const char a = 97;

  assert(_series.length() == 2);
  // Convert to numbers 0-26,0-26
  c1 = char(_series[0]) - a;
  c2 = char(_series[1]) - a;

  // Base 24 increment
  c2++;
  if (c2 >= 26) {
    c2 = 0;
    c1++;
    if (c1 >= 26) {
      c1 = 0;
    }
  }
  // Back to letters
  _series[0] = char(c1 + a);
  _series[1] = char(c2 + a);
  return _series;
}
