#pragma once

#define ARDUINOJSON_ENABLE_STD_STREAM 1
#define ARDUINOJSON_ENABLE_ARDUINO_STRING 1
#include "ArduinoJson-v6.12.0.hpp"
//#include "deserialize.hpp"
//#include "StdStreamReader.hpp"
static const char *TL_TAG = "TimeLapseCamera";
#undef LOG_LOCAL_LEVEL
#define LOG_LOCAL_LEVEL ESP_LOG_VERBOSE
#include "esp_log.h"

#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>
#include <freertos/queue.h>
#include "esp_camera.h"
#include <FS.h>
#include "BBEsp32Lib.h"


#define DEFAULT_CHG_PIN 3

class FTPUploader;
class TimeLapseWebServer;
class TimeLapseWebSocket;

using namespace ArduinoJson;
using namespace bbesp32lib;

/*
    QVGA|CIF|VGA|SVGA|XGA|SXGA|UXGA
            |640|800|1024|1400|1600
            |480|600|768 |960 |1200

*/
// typedef struct {
//  uint8_t * buf;              /*!< Pointer to the pixel data */
// size_t len;                 /*!< Length of the buffer in bytes */
// size_t width;               /*!< Width of the buffer in pixels */
// size_t height;              /*!< Height of the buffer in pixels */
// pixformat_t format;         /*!< Format of the pixel data */
//} camera_fb_t;

// #define __SIZE_TYPE__ long unsigned int

extern RTC_DATA_ATTR int TimeLapseCameraFileNumber;

class TimeLapseCamera {


  public:

    enum OffOnAuto {
      Off = 0,
      On = 1,
      Auto = 2
    };

    TimeLapseCamera(fs::FS &filesys, String saveFolder = "/tlc/", int takePhotoPeriodS = 10, int uploadPhotosMaxS = 600);

    void startTakingPhotos();
    void stopTakingPhotos();

    uint32_t sleepy(); // seconds to sleep for (0 if not required)
    void sleep();      // deepsleep if required

    bool save();
    bool load();

    

    void taskify(int takePriority=3,int uploadPriority=2);
    void configFTP(const String& host, const String& user, const String& password);
    bool testFTPConnection();
    void takeSinglePhotoAndDiscard();
    void takeSinglePhoto();
    // true if all photos uploaded
    bool uploadPhotos(int maxFiles = 500, int maxSeconds = 600);
    String toString();
    void taskStates();
    bool isCharged();
    bool _sleepHours[24];
    OffOnAuto _uploadMode = Auto;
    String _wifiSSID;
    String _wifiPassword;

    String DefaultLogFile = "/timelapse.log";

  

    friend void TakePhotosTask (void *timeLapseCamera);
    friend void UploadPhotosTask (void *timeLapseCamera);
    friend void MonitorTask (void *timeLapseCamera);
    friend TimeLapseWebServer;
    friend TimeLapseWebSocket;

  private:

    //
// Saves the configuration to a file
//
bool saveConfigToFile(const DynamicJsonDocument &doc) {

  // Delete existing file, otherwise the configuration is appended to the file
  _filesys.remove(_configFilename);

  // Open file for writing
  File file = _filesys.open(_configFilename, FILE_WRITE);
  if (!file) {
    ESP_LOGE(TL_TAG, "Failed to open config file: %s", _configFilename.c_str());
    return false;
  }
printf("WARNNING AT %s\n",__FILE__);
/*
  if (serializeJson(doc, file) == 0) {
    ESP_LOGE(TL_TAG, "Failed to serialize to config file: %s", _configFilename.c_str());
    return false;
  }
  */

  // Close the file
  file.close();
  return true;
}

DynamicJsonDocument status() {
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

bool setIfValid(String &value, const char *key, int minLength,
                                 const DynamicJsonDocument &data) {
  if (data.containsKey(key)) {
    String proposedValue = data[key].as<String>();
    if (proposedValue.length() >= minLength) {
      if (proposedValue != value) {
        value = proposedValue;
        return true;
      }
    } else {
      ESP_LOGW(TL_TAG, "%s: %s is invalid. Ignored", key, value.c_str());
    }
  }
  return false;
}
bool setIfValid(bool &value, const char *key,
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
      ESP_LOGW(TL_TAG, "%s: %d is invalid. Ignored", key, value);
    }
  }
  return false;
}

bool setIfValid(int &value, const char *key,
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
      ESP_LOGW(TL_TAG, "%s: %d is invalid. Ignored", key, value);
    }
  }

  return false;
}

bool setIfValid(OffOnAuto &value, const char *key,
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
      ESP_LOGW(TL_TAG, "%s: %d is invalid. Ignored", key, value);
    }
  }

  return false;
}

bool setIfValid(framesize_t &value, const char *key,
                                 const framesize_t minimum,
                                 const framesize_t maximum,
                                 const DynamicJsonDocument &data) {

  if (data.containsKey(key)) {
    int proposedValue = data[key].as<int>();
    // ESP_LOGV(TL_TAG,"Frame Set %d key:%s pv:%d min:%d max:%d", (int)value, key,
    // proposedValue, (int) minimum, (int) maximum);
    if (proposedValue >= (int)minimum && proposedValue <= (int)maximum) {
      if ((int)proposedValue != (int)value) {
        // No change required
        value = (framesize_t)proposedValue;
        return true;
      }
    } else {
      ESP_LOGW(TL_TAG, "Frame Set %s: %d is invalid. Ignored", key, value);
    }
  }
  return false;
}
    DynamicJsonDocument readConfigFromFile();             // read from file
    //void applyConfig(const DynamicJsonDocument& doc);     // write to state
    inline void applyConfig(const DynamicJsonDocument &data) {
  // Must use containsKey, as (bool) data["saveFolder"] == false (why?)
  // Serial.println("applyConfig with:");
  // serializeJson(data, Serial);

  if (data.containsKey("saveFolder")) {
    _saveFolder = data["saveFolder"].as<String>();
    _saveFolder = bbesp32lib::slash(_saveFolder, '/');
  }

  setIfValid(_uploadMode, "uploadMode", Off, Auto, data);

  bool sleepChanged = setIfValid(_sleepAt, "sleepAt", 0, 24, data);
  bool wakeChanged = setIfValid(_wakeAt, "wakeAt", 0, 24, data);
  if (sleepChanged || wakeChanged) {
    for (int h = 0; h < 24; h++) {
      _sleepHours[h] = (h < _wakeAt) || (h >= _sleepAt);
      // ESP_LOGV(TL_TAG,"h: %d sleep:%d", h, _sleepHours[h]);
    }
  }
  setIfValid(_takePhotoPeriodS, "takePhotoPeriodS", 1, 3600, data);
  setIfValid(_sleepEnabled, "sleepEnabled", 0, 1, data);
  setIfValid(_camTakePhotos, "camTakePhotos", false, true, data);
  setIfValid(_wifiSSID, "wifiSSID", 1, data);
  setIfValid(_wifiPassword, "wifiPassword", 0, data);

  if (setIfValid(_frameSize, "frameSize", FRAMESIZE_QQVGA, FRAMESIZE_UXGA,
                 data)) {
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

    DynamicJsonDocument getConfig();                      // read from state
 
    String nextSeries();
    void initCam();
    void deinitCam();
    void updateFileStats();

    String _configFilename = "/timelapse-config.json";
    uint32_t _lastBatteryCheckMs = 0;  // millis
    uint32_t _chargingForMs = 0;

    bool _camInitialised = false;      // memory allocated and cam init'd
    bool _camTakePhotos = false;       // we want to take photos
    int _chargePin = DEFAULT_CHG_PIN;
    bool _deleteOnUpload = true;
    fs::FS &_filesys;
    framesize_t _frameSize = FRAMESIZE_XGA;
    FTPUploader *_ftp = NULL;
    
    String _lastImage;
    String _firstImage;
    int _nImages=-1;
    String _lastImageUploaded;
    int _overWorked = 0;
    String _saveFolder;
    String _series = "aa";
    bool _sleepEnabled = false;
    int _sleepAt = 0;
    int _takePhotoPeriodS = 10;
    TaskHandle_t _takePhotosTaskHandle = NULL;
    int _quality = 8;
    int _uploadPhotosMaxS;
    TaskHandle_t _uploadPhotosTaskHandle = NULL;
    int _wakeAt = 0;  
    int _wiFiUsers = 0; // remove?



};
