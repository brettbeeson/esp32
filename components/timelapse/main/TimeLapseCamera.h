#pragma once

#define ARDUINOJSON_ENABLE_STD_STREAM 1
#include "ArduinoJson.h"
//#include "deserialize.hpp"
//#include "StdStreamReader.hpp"

#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>
#include <freertos/queue.h>
#include "esp_camera.h"
#include <FS.h>


#define DEFAULT_CHG_PIN 3

class FTPUploader;
class TimeLapseWebServer;
class TimeLapseWebSocket;

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

    DynamicJsonDocument status();   // emphemeral values, unlike config

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

    // Can't figure templates...
    bool setIfValid(OffOnAuto& value, const char*key, const OffOnAuto& minimum, const OffOnAuto& maximum, const DynamicJsonDocument & data);
    bool setIfValid(int& value, const char*key, const int& minimum, const int& maximum, const DynamicJsonDocument & data);
    bool setIfValid(bool& value, const char*key, const int& minimum, const int& maximum, const DynamicJsonDocument & data) ;
    bool setIfValid(String& value, const char*key, int minLength, const DynamicJsonDocument & data) ;
    bool setIfValid(framesize_t& value, const char*key, const framesize_t minimum, const framesize_t maximum, const DynamicJsonDocument & data);

    DynamicJsonDocument readConfigFromFile();             // read from file
    void applyConfig(const DynamicJsonDocument& doc);     // write to state
    DynamicJsonDocument getConfig();                      // read from state
    bool saveConfigToFile(const DynamicJsonDocument& doc);// write to file

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
