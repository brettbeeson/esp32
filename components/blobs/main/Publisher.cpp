
#define INCLUDE_vTaskSuspend 1

#include "Publisher.h"
#include "Reading.h"
#include <Arduino.h>

#undef LOG_LOCAL_LEVEL
#define LOG_LOCAL_LEVEL ESP_LOG_VERBOSE
#include "esp_log.h"
static const char *TAG = "Publisher";

void PublisherTask(void *_p) {
  Publisher *p = (Publisher *)_p;

  for (;;) {
    try {
      Reading *readingPtr = NULL;
      BaseType_t xStatus;
      // Block until something in the queue
      xStatus = xQueuePeek(p->_blob->readingsQueue(0), &(readingPtr), portMAX_DELAY);
      assert(xStatus == pdTRUE);
        xEventGroupClearBits(p->_state, Publisher::STATE_IDLE);
        p->publishReadings();
        xEventGroupSetBits(p->_state, Publisher::STATE_IDLE);
    } catch (const exception &e) {
      ESP_LOGE(TAG, "PublisherTask exception: %s", e.what());
      vTaskDelay(pdMS_TO_TICKS(60000));
    }
  }
}

Publisher::Publisher(Blob *blob) : _blob(blob) {
  blob->add(this);
  _state = xEventGroupCreate();
  assert(_state);
}

Publisher::~Publisher() {
  assert(!_publishTask); // must call ::end before destruction
}

void Publisher::taskify(int priority) {
  int core = 1; // run on this core
  ESP_LOGI(TAG, "Starting Publisher task in core %d", core);
   xTaskCreatePinnedToCore(PublisherTask, "PublisherTask", 10000, (void *)this,
                        priority, &_publishTask /* handle */, core);
  //xTaskCreate(PublisherTask, "Publisher", 10000, (void *)this, priority,
  //            &_publishTask /* handle */);
}

void Publisher::waitUntilPublished() {
  
  EventBits_t uxBits;
  for (;;) {
    // block until we are IDLE ...
    uxBits = xEventGroupWaitBits(_state, STATE_IDLE, false, true, portMAX_DELAY);
    assert(uxBits & STATE_IDLE);
    // if not more items to publish ...
    if (uxQueueMessagesWaiting(_blob->readingsQueue(0)) == 0) {
      // if  PublisherTask didn't *just* empty the queue, we're finished
      if (xEventGroupWaitBits(_state, STATE_IDLE, false, true, 0) & STATE_IDLE) {
        return;
      }
    }
  }
}

void Publisher::end() {
  if (_publishTask) {
    while (eTaskGetState(_publishTask) == eRunning) {
      vTaskDelay(pdMS_TO_TICKS(1000));
      ESP_LOGV(TAG, "Waiting for task to idle.");
    }
    vTaskDelete(_publishTask);
    _publishTask = 0;
    vTaskDelay(pdMS_TO_TICKS(10));
    ESP_LOGV(TAG, "Stopped task.");
  }
}

eTaskState Publisher::taskState() {

  assert(_publishTask);
  return eTaskGetState(_publishTask);
}

/*
 * Publish all available
 */
int Publisher::publishReadings() {
  BaseType_t xStatus = pdPASS;
  Reading *r = NULL;
  TickType_t xTicksToWait;
  int nPublished = 0;

  while (xStatus == pdPASS) {
    xStatus = xQueueReceive(_readingsQueue, &r, 0);
    if (xStatus == pdPASS) {
      ESP_LOGV(TAG, "Publishing reading:%s", r->tostr().c_str());
      if (publishReading(r)) {
        nPublished++;
        _blob->raiseEvent(Blob::PublishEvent);
        if (r) {
          delete r;
          r = NULL;
        }
      } else {
        ESP_LOGW(TAG, "Can't publish reading:%s", r->tostr().c_str());
        if (r) {
          delete r;
          r = NULL;
        }
        // retain reading - should push_back
        // todo
      }
    } else {
      ESP_LOGV(TAG, "Nothing to publish. xStatus = %d", xStatus);
    }
  }
  if (nPublished)
    ESP_LOGV(TAG, "Published %d", nPublished);
  return nPublished;
}
