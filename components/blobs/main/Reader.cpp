#include "Reader.h"
#include "Blob.h"
#include <Arduino.h>
#include "esp_log.h"

static const char*TAG = "Reader";


void ReaderTask(void *_p)
{
  TickType_t xLastWakeTime;

  Reader* p = (Reader *) _p;
  
  xLastWakeTime = xTaskGetTickCount();

  for (;;) {
    vTaskDelayUntil(&xLastWakeTime, pdMS_TO_TICKS(p->_readPeriodMs));
    try {
      p->read();
    } catch (const std::exception& e) {
      ESP_LOGV(TAG,"%s",e.what());
      //OLEDUI.message(e.what());
    }
  }
}


Reader::Reader(Blob* blob)
  : _blob(blob) {
  
  blob->add(this);
}


void Reader::taskify(int readPeriodMs, int priority) {
  int core = 0;   // run on this core
  ESP_LOGV(TAG,"Starting Reader task");
  _readPeriodMs = readPeriodMs;
  xTaskCreatePinnedToCore(ReaderTask, "Reader", 10000, (void *) this, priority, &_taskHandle /* no handle */, core);
}

void Reader::end() {
  if (_taskHandle) {
    vTaskDelete(_taskHandle);
    _taskHandle=0;
  }
}