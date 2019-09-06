#include "Arduino.h"
#include "BBEsp32Lib.h"
#include "Blob.h"
#include "BlobOLEDUI.h"
#include "BlobSensor.h"
#include "CoolingRelay.h"
#include "DSTempSensors.h"
#include "ControllerSensor.h"
#include "HeatingRelay.h"
#include "I2CManager.h"
#include "LoRa.h" // custom
#include "LoraPublisher.h"
#include "LoraReader.h"
#include "OLEDUI.h"
#include "esp_task_wdt.h"
#include "rom/rtc.h"
#undef LOG_LOCAL_LEVEL
#define LOG_LOCAL_LEVEL ESP_LOG_VERBOSE
#include "esp_log.h"
static const char *TAG = "brew32";

//#define MQ3PIN GPIO_NUM_4
#define OLED_FRAME_PIN GPIO_NUM_12
#define OLED_READING_PIN GPIO_NUM_14
#define RELAY_PIN_1 GPIO_NUM_2
// Todo: complete wiring
#define RELAY_PIN_2 GPIO_NUM_4
#define DS_PIN GPIO_NUM_15
#define SDA GPIO_NUM_21
#define SCL GPIO_NUM_22
#define OLEDPIN GPIO_NUM_16

//
// 17 - 18 - 19 - 20 - 21 - 22 - 23
//            |1.0|                 HEAT
//                |1.0 |            COOL


float CoolingSetpoint = 26.5;
float CoolingDeadband = 1.0;
float HeatingSetpoint = 24.5;
float HeatingDeadband = 1.0;
/*
float CoolingSetpoint = 20.5;
float CoolingDeadband = 1.0;
float HeatingSetpoint = 19.5;
float HeatingDeadband = 1.0;
*/

RTC_DATA_ATTR static int bootCount = 0;
long startTime = 0;
const long periodMs = 10000;

Blob blob;
DSTempSensors ds(&blob, DS_PIN);
//BlobSensor ts(&blob);
// MQ3Sensor booze(blob.readingsQueue, MQ3PIN);
CoolingRelay fridge(&blob,&ds, RELAY_PIN_1, CoolingSetpoint, CoolingDeadband);
HeatingRelay heater(&blob,&ds, RELAY_PIN_2, HeatingSetpoint, HeatingDeadband);
ControllerSensor heaterState(&blob,heater,"heating");
ControllerSensor coolerState(&blob,fridge,"cooling");
LoraPublisher loraPub(&blob);

using namespace std;

extern "C" {
void i2c_test();
void i2c_example_master_init();
}

void test_main();



void setup() {
  Serial.begin(115200);
  bootCount++;
  while (!Serial) {
  };

  esp_log_level_set("*", ESP_LOG_WARN);
  esp_log_level_set(TAG, ESP_LOG_VERBOSE);
  
  esp_log_level_set("BlobOLEDUI", ESP_LOG_INFO);
  esp_log_level_set("LoraPublisher", ESP_LOG_WARN);
  esp_log_level_set("bbesp32lib", ESP_LOG_INFO);
  esp_log_level_set("Blob", ESP_LOG_INFO);
  esp_log_level_set("ESPinfluxdb", ESP_LOG_VERBOSE);
  esp_log_level_set("CoolingRelay", ESP_LOG_VERBOSE);
  esp_log_level_set("HeatingRelay", ESP_LOG_VERBOSE);

  ESP_LOGI(TAG,
           "Setup executing on core %d, bootCount=%d reset reason 0,1=%s,%s",
           xPortGetCoreID(), bootCount,
           bbesp32lib::resetReason(0).c_str(),
           bbesp32lib::resetReason(1).c_str());
           

  try {

    I2CManager.begin();
    blob.begin(Blob::USE_LORA);
    blob.readSensors();
    assert(CoolingSetpoint>HeatingSetpoint);
    OLEDUI.setFPS(10);
    OLEDUI.setFramesButton(OLED_FRAME_PIN, 20);
    OLEDUI.addOverlay(new OverlayFrame());
    MessagesFrame *runLog = new MessagesFrame();
    MessagesFrame *setupLog = new MessagesFrame();
    ReadingsFrame *readingsFrame = new ReadingsFrame(OLED_READING_PIN, 20);
    OLEDUI.addFrame(runLog);
    OLEDUI.addFrame(setupLog);
    OLEDUI.addFrame(readingsFrame);
    setupLog->add("Brew Blob Setup:");
    setupLog->add(fridge.toString());
    setupLog->add(heater.toString());
    setupLog->add(ds.toString(true));
    readingsFrame->add(ds);
    readingsFrame->add(heaterState);
    readingsFrame->add(coolerState);
    OLEDUI.begin(0x3C);
    OLEDUI.gotoFrame(2);
    
    ESP_LOGI(TAG, "Blob: %s", blob.toString().c_str());

  } catch (const runtime_error &e) {
    ESP_LOGE(TAG, "Failed setup. Exception: %s", e.what());
    OLEDUI.message("Setup failed:");
    OLEDUI.message(e.what());
    delay(10000);
    ESP.restart();
  }
}

void loopForever(void) {

  TickType_t xLastWakeTime = xTaskGetTickCount();

  for (;;) {
    vTaskDelayUntil(&xLastWakeTime, pdMS_TO_TICKS(periodMs));
    try {
      blob.readSensors(); 
      blob.adjustControllers();
      //assert(!(heater.on() && fridge.on()));  // cos that would be dumb to cool and heat simulateounsly
      blob.publish();
      //bbesp32lib::vTaskPrintRunTimeStats();
      //ESP_LOGV(TAG, "Temp: %s\n", ds.toString().c_str());

    } catch (const std::runtime_error &e) {
      ESP_LOGE(TAG, "Loop Exception: %s", e.what());
      OLEDUI.message(e.what());
      delay(60000);
      ESP.restart();
    }
  }
}

extern "C" {
void app_main() {
  initArduino();
  setup();
  loopForever();
}
}
