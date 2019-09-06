#include "Arduino.h"
#include "Sensor.h"
#include "Blob.h"
#include "OLEDUI.h"
#include "freertos/FreeRTOS.h"
#include <ArduinoJson.h>

#undef LOG_LOCAL_LEVEL
#define LOG_LOCAL_LEVEL ESP_LOG_VERBOSE
#include "esp_log.h"
static const char *TAG = "Sensor";
#define TRACE printf("TRACE %s:%d\n", __FILE__, __LINE__);

void SensorReadTask(void *SensorSensor) {
  TickType_t xLastWakeTime;

  Sensor *s = (Sensor *)SensorSensor;
  // todo: wait until a multiple of the period
  xLastWakeTime = xTaskGetTickCount();

  for (;;) {
    vTaskDelayUntil(&xLastWakeTime, pdMS_TO_TICKS(s->_taskPeriodMs));
    // ESP_LOGV(TAG,"SensorReadTask");
    try {
      s->read();
    } catch (const std::runtime_error &e) {
      ESP_LOGE(TAG, "Exception: %s", e.what());
    }
    s->copyReadingsToQueue();
  }
}

Sensor::Sensor(Blob *blob, String id)
    : _blob(blob), _id(id), _taskPeriodMs(10000) {
  blob->add(this);
}

// todo: String location, String id
void Sensor::begin(int _nreadings) {

  nReadings = _nreadings;
  if (nReadings > 0) {
    this->readings = (Reading **)malloc((sizeof(Reading *)) * nReadings);
    for (int i = 0; i < nReadings; i++) {
      this->readings[i] = new Reading();
      // for influx, it's important these are not empty. todo: omit in influx if
      // empty.
      this->readings[i]->units = "";
      this->readings[i]->id = "";
      this->readings[i]->location = "";
      this->readings[i]->metric = "";
      // if (showOnOLED) OLEDUI.addReading(this->readings[i]);
    }
  }
}
void Sensor::setLocation(String location) {

  for (int i = 0; i < nReadings; i++) {
    this->readings[i]->location = location;
  }
}

void Sensor::setID(String id) {

  for (int i = 0; i < nReadings; i++) {
    this->readings[i]->id = id;
  }
}

String Sensor::toString(bool brief) {

  String r;

  if (brief) {
    r += String("R:") + String(nReadings) + ",";
    for (int i = 0; i < nReadings; i++) {
      r += String("ID:") + String(this->readings[i]->id).substring(0, 5) +
           String("; ");
    }
  } else {
    r = String("Sensor: nReadings=") + String(nReadings) + String(": ");
    for (int i = 0; i < nReadings; i++) {
      r += String("units:") + String(this->readings[i]->units) + String("; ");
      r += String("id:") + String(this->readings[i]->id) + String("; ");
      r += String("location:") + String(this->readings[i]->location) +
           String("; ");
      r += String("metric:") + String(this->readings[i]->metric) + String("; ");
    }
  }
  return r;
}

void Sensor::taskify(int taskPeriodMs, int priority) {
  _taskPeriodMs = taskPeriodMs;
  //int core = xPortGetCoreID();
  int core = 1;
  // ESP_LOGV(TAG,"Sensor executing on core %d", core);
  xTaskCreatePinnedToCore(SensorReadTask, "Sensor", 10000 /* Stack depth */,
                          (void *)this, priority,
                          &_taskHandle /* task handle */, core);
}

// write a copy to queue
void  Sensor::copyReadingsToQueue(int timeoutMs) {
  BaseType_t xStatus;
  Reading *r = NULL;

  assert(_readingsQueue);
  for (int i = 0; i < nReadings; i++) {
    r = new Reading(*(readings[i]));                                          // copy it. receiver to delete
    xStatus = xQueueSendToBack(_readingsQueue, &r, pdMS_TO_TICKS(timeoutMs)); // will copy the pointer
    if (xStatus == errQUEUE_FULL) {
      delete (r);
      r = NULL;
      throw(Blob::queue_full("Queue is full"));
    } else if (xStatus != pdPASS) {
      String m = String("Unknown queue error:") + String(xStatus);
      throw runtime_error(m.c_str());
    }
  }
}

const Reading Sensor::getReading(int channel) {
  return (*getReadingPtr(channel)); // returns a copy
}

Reading *Sensor::getReadingPtr(int channel, boolean throwIfNull) {
  if (channel >= nReadings || channel < 0) {
    if (throwIfNull) {
      throw std::range_error("No reading available on that channel");
    } else {
      return NULL;
    }
  }
  return (readings[channel]); // returns a ptr
}

void Sensor::clearReadings() {
  for (int i = 0; i < nReadings; i++) {
    readings[i]->setValue(NAN);
    readings[i]->timestamp = 0;
  }
}

void Sensor::end() {

  if (_taskHandle) {
    while (eTaskGetState(_taskHandle) == eRunning) {
      vTaskDelay(pdMS_TO_TICKS(1000));
      ESP_LOGW(TAG, "Waiting for task to stop running");
    }
    vTaskDelete(_taskHandle);
    _taskHandle = NULL;
  }

  //ESP_LOGV(TAG, "deleting nReadings: %d", nReadings);
  for (int i = 0; i < nReadings; i++) {
    if (this->readings[i]) {
      delete (this->readings[i]);
    }
    this->readings[i] = NULL;
  }
  nReadings = 0; // mofo don't forget
  if (this->readings) {
    free(this->readings);
    this->readings = NULL;
  }
}

Sensor::~Sensor() {
  //ESP_LOGV(TAG, "Destructing (superclass)");
  assert(_taskHandle == 0); // end should be called before destruct
  end();
}
