#include "Blob.h"
#include "Controller.h"
#include <stdexcept>
#include "esp_log.h"

static const char* TAG = "Controller";

void ControllerAdjustTask(void *controllerPtr) {
  TickType_t xLastWakeTime;

  Controller* c = (Controller *) controllerPtr;
  // todo: wait until a multiple of the period
  xLastWakeTime = xTaskGetTickCount();

  for (;;) {
    vTaskDelayUntil(&xLastWakeTime, pdMS_TO_TICKS(c->_taskPeriodMs));
    ESP_LOGV(TAG,"ControllerAdjustTask");
    try {
      c->adjust();
    } catch (const std::runtime_error& e) {
      ESP_LOGE(TAG,"Failed ControllerAdjustTask. Exception: %s",e.what());
    }
  }
}

Controller ::Controller (Blob* blob) : _blob(blob) {
  _blob->add(this);
}


Controller ::~Controller () {
  assert(!_taskHandle);
}

String Controller:: toString() {
  return String("Controller");
}

void Controller::taskify(int taskPeriodMs, int priority, int stackDepth) {
  _taskPeriodMs = taskPeriodMs;
  int core = xPortGetCoreID();
  ESP_LOGV(TAG,"Controller executing on core %d", core);
  xTaskCreatePinnedToCore(ControllerAdjustTask, "Controller", stackDepth, (void *) this, priority, NULL /* task handle */, core);
}

void Controller::end() {
  vTaskDelete(_taskHandle);
  _taskHandle=NULL;
}