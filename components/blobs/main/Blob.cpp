#include <Arduino.h>
#include <ArduinoJson.h> //https://github.com/bblanchon/ArduinoJson
#include <SD.h>
#include <SPIFFS.h>
#include <WiFi.h>
#include <Wire.h>
#include <exception>
//#include <rom/rtc.h>

#include "Blob.h"
#include "BlobWebServer.h"
#include "Controller.h"
#include "LoRa.h"
#include "Publisher.h"
#include "Reader.h"
#include "Reading.h"
#include "Sensor.h"

#include "esp_log.h"
#undef LOG_LOCAL_LEVEL
#define LOG_LOCAL_LEVEL ESP_LOG_VERBOSE
static const char *TAG = "Blob";

using namespace std;

#define DEBUG 0
#if DEBUG
#warning Debugging Blob with single Reading queue length!
#define BLOB_QUEUE_0_LENGTH 1
#define BLOB_QUEUE_1_LENGTH 1
#else
#define BLOB_QUEUE_0_LENGTH CONFIG_BLOB_QUEUE_0_LENGTH
#define BLOB_QUEUE_1_LENGTH CONFIG_BLOB_QUEUE_1_LENGTH

#endif

void ReadSensorsTask(void *);

void BlobTask(void *_b) {
  Blob *b = (Blob *)_b;

  for (;;) {
    b->refresh();
  }
}

Blob::Blob() {
  _readingsQueue0 = xQueueCreate(BLOB_QUEUE_0_LENGTH, sizeof(Reading *));
  _readingsQueue1 = xQueueCreate(BLOB_QUEUE_1_LENGTH, sizeof(Reading *));
  assert(_readingsQueue0);
  assert(_readingsQueue1);
  ESP_LOGV(TAG, "ReadingsQueue0: %d ReadingsQueue1: %d",BLOB_QUEUE_0_LENGTH,BLOB_QUEUE_1_LENGTH);
  id = getMacStr();
}

void Blob::add(Publisher *p) {
  ESP_LOGV(TAG, "Adding a publisher: %s", p->_id.c_str());
  assert(publisher == NULL);
  publisher = p;
  publisher->_readingsQueue = this->readingsQueue(0);
}

void Blob::remove(Publisher *p) {
  assert(publisher == p);
  publisher = NULL;
}

void Blob::add(Reader *r) {
  ESP_LOGV(TAG, "Adding a reader: %s", r->_id.c_str());
  assert(_reader == NULL);
  _reader = r;
  _reader->_readingsQueue = this->readingsQueue(0);
}

void Blob::remove(Reader *r) {
  assert(_reader == r);
  _reader = NULL;
}

/*
   Add reading to queue. When used, it will be deleted. Caller: don't delete.
*/
void Blob::addReading(Reading *r, int iReadingsQueue) {
  BaseType_t xStatus;
  assert(r);
  xStatus = xQueueSendToBack(this->readingsQueue(iReadingsQueue), &r,
                             pdMS_TO_TICKS(100) /* timeout */);
  if (xStatus != pdPASS) {
    delete (r);
    r = NULL; // todo: maybe don't?
    ESP_LOGE(TAG, "%s: xQueueSendToBack fail with status = %d", __FUNCTION__,
             xStatus);
    throw runtime_error("addReading: xQueueSendToBack fail");
  }
}

void Blob::begin() { begin(0); }

void Blob::begin(int useServicesMask) {

  Reading *r;

  if (useServicesMask & USE_SD)
    setupSDCard();
  if (useServicesMask & USE_LORA)
    setupLora();
  if (useServicesMask & USE_HTTPD)
    setupWebServer();

  for (std::size_t i = 0; i < sensors.size(); ++i) {
    sensors[i]->begin();
    if ((r = sensors[i]->getReadingPtr(0, false /* don't throw */))) {
      if (r->location == "")
        sensors[i]->setLocation(location);
    }
    if ((r = sensors[i]->getReadingPtr(0, false /* don't throw */))) {
      if (r->id == "")
        sensors[i]->setID(id);
    }
  }
  if (publisher)
    publisher->begin();
  if (_reader)
    _reader->begin();
  for (std::size_t i = 0; i < controllers.size(); ++i) {
    controllers[i]->begin();
  }
}

void Blob::setupLora() {

  SPIClass *spiLora;

  spiLora = new SPIClass();
  LoRa.setSPI(*spiLora);
  //       SCLK , MISO , MOSI , SS
  spiLora->begin(5, 19, 27, 18);
  // void setPins(int ss = LORA_DEFAULT_SS_PIN, int reset =
  // LORA_DEFAULT_RESET_PIN, int dio0 = LORA_DEFAULT_DIO0_PIN);
  LoRa.setPins(18, 23, 26);
  if (!LoRa.begin(915E6)) {
    throw std::runtime_error("Couldn't start Lora");
  }
  LoRa.setSyncWord(0xBB); // ranges from 0-0xFF, default 0x34, see API docs
  LoRa.enableCrc();
}

void Blob::setupSDCard() {

  int sdCS;
  SPIClass *spiSD;

  assert(0); // not working yet
  sdCS = 13; // SS
  spiSD = new SPIClass();
  // SCLK , MISO , MOSI , SS
  spiSD->begin(14, 2, 15, sdCS);
  // bool begin(uint8_t ssPin=SS, SPIClass &spi=SPI, uint32_t frequency=4000000,
  // const char * mountpoint="/sd")
  Serial.println(sdCS);
  if (!SD.begin(sdCS, *spiSD))
    throw std::runtime_error("Couldn't start SD");
}

void Blob::setupWebServer() {}

bool Blob::configFromJSON(Stream &s) {
  return false; // not implemented. read a JSON string and configure myself
}

/*
 * Publish current readings and return
 */
int Blob::publish() {
  if (publisher) {
    return publisher->publishReadings();
  } else {
    return 0;
  }
}

void Blob::testBasicFunctions() {

  if (!testLora())
    throw std::runtime_error("Lora Check Failed");
  if (!testSDCard())
    throw std::runtime_error("SD Check Failed");
}

bool Blob::testSDCard() { return SD.exists("/index.htm"); }

bool Blob::testLora() {

  bool retval;
  retval = LoRa.beginPacket();
  retval = retval && (LoRa.write(1) == 1);
  retval = retval && LoRa.endPacket();
  return retval;
}

void Blob::readSensorsTaskify(int priority, int periodMs) {
  int core = 1;
  _readSensorsTaskPeriod = periodMs;
  ESP_LOGV(TAG, "NOT PINNING taskifySensors executing on core %d", core);
  //xTaskCreatePinnedToCore(ReadSensorsTask, "ReadSensorsTask", 2000,
  //                      (void *)this, priority, NULL /* task handle */, core);
  xTaskCreate(ReadSensorsTask, "ReadSensorsTask", 2000,
              (void *)this, priority, NULL /* task handle */);
}

Blob::~Blob() {
  end();
}
/**
 * Warning: careful not to call subclass methods
 * Stop all components
 * User must delete components
 */
void Blob::end() {
  ESP_LOGV(TAG, "Ending blob.");

  for (std::size_t i = 0; i < sensors.size(); ++i) {
    if (sensors[i]) {
      sensors[i]->end();
      sensors[i] = NULL; // user to delete
    }
  }
  for (std::size_t i = 0; i < controllers.size(); ++i) {
    if (controllers[i]) {
      controllers[i]->end();
    }
  }
  if (publisher) {
    publisher->end();
    publisher = NULL;
  }
  if (_reader) {
    _reader->end();
    _reader = NULL;
  }
  vQueueDelete(_readingsQueue0);
  vQueueDelete(_readingsQueue1);
  _readingsQueue0 = NULL;
  _readingsQueue1 = NULL;
}

String Blob::getMacStr() {
  const uint8_t MACLEN = 6;
  uint8_t mac[MACLEN];
  String s;

  memset(mac, 0, MACLEN);
  ESP_ERROR_CHECK(esp_efuse_mac_get_default(mac));
  for (int i = 0; i < MACLEN; i++) {
    if (s != "")
      s += ".";
    s += String(mac[i]);
  }

  return s;
}

void Blob::add(Controller *c) {
  ESP_LOGV(TAG, "Adding a controller: %s", c->_id.c_str());
  controllers.push_back(c);
}

void Blob::add(Sensor *s) {
  ESP_LOGV(TAG, "Adding a sensor: %s", s->_id.c_str());
  s->_readingsQueue = this->readingsQueue(0);
  sensors.push_back(s);
}

void Blob::readSensors(int timeoutMs) {
  ESP_LOGV(TAG, "Blob::readSensors() size:%d", sensors.size());
  for (std::size_t i = 0; i < sensors.size(); ++i) {
    ESP_LOGV(TAG, "Blob::readSensors(%d) nReadings:%d", i, sensors[i]->nReadings);
    if (sensors[i]->nReadings > 0) {
      sensors[i]->read();
      sensors[i]->copyReadingsToQueue(timeoutMs);
    }
  }
}

void Blob::refresh() {
  readSensors();
  readReaders();
  adjustControllers();
  publish();
}

void Blob::readReaders() {
  if (_reader) {
    _reader->read();
  }
}

void Blob::adjustControllers() {
  ESP_LOGV(TAG, "Adjusting %d controllers", controllers.size());
  for (auto &c : controllers) {
    c->adjust();

    // for (std::size_t i = 0; i < controllers.size(); ++i) {
    // controllers[i]->adjust();
  }
}

String Blob::toString() {
  String r;
  for (std::size_t i = 0; i < controllers.size(); ++i) {
    r += controllers[i]->toString();
  }
  for (std::size_t i = 0; i < sensors.size(); ++i) {
    r += sensors[i]->toString();
  }
  //  if (_reader) r+= _reader->toString();
  //  if (publisher) r+= publisher->toString();
  return r;
}

void Blob::printReadingsQueue(int i) {
  ESP_LOGI(TAG, "readingsQueue%d: %d items", i,
           uxQueueMessagesWaiting(this->readingsQueue(i)));
}

QueueHandle_t Blob::readingsQueue(int i) {
  if (i == 0) {
    return _readingsQueue0;
  } else if (i == 1) {
    return _readingsQueue1;
  } else {
    assert(0);
  }
}

UBaseType_t Blob::readingsInQueue(int i) {
  return uxQueueMessagesWaiting(readingsQueue(i));
}
//////////////////////////////////

void ReadSensorsTask(void *blobPtr) {
  TickType_t xLastWakeTime = xTaskGetTickCount();
  ;

  Blob *b = (Blob *)blobPtr;
  ESP_LOGI(TAG, "ReadSensors period: %dms", b->_readSensorsTaskPeriod);
  for (;;) {
    vTaskDelayUntil(&xLastWakeTime, pdMS_TO_TICKS(b->_readSensorsTaskPeriod));
    try {
      b->readSensors();
    } catch (const std::runtime_error &e) {
      ESP_LOGE(TAG, "Failed ReadSensorsTask. Exception: %s", e.what());
    }
  }
}