#include "MemoryReader.h"

#undef LOG_LOCAL_LEVEL
#define LOG_LOCAL_LEVEL ESP_LOG_VERBOSE
#include "esp_log.h"
static const char *TAG = "MemoryReader";

// Allows task to wait on queue forever
#define INCLUDE_vTaskSuspend 1

MemoryReader::MemoryReader(Blob *blob) : Reader(blob) {}

void MemoryReader::begin() {}

int MemoryReader::read() {
  BaseType_t xStatus = 0;
  sample_t *samples = NULL;
  Reading *r = NULL;
  int s, m;
  int nSent;
  int nRead = 0;

  for (m = 0; m < N_MEASUREMENTS; m++) {
    nSent = 0;
    samples = rtcmem::measurements[m].samples;

    // todo set #samples in measurement?
    if (rtcmem::measurements[m].id[0] != 0) {
      ESP_LOGV(TAG, "MemoryReader::read(): rtcmem::measurements[%d].id = %s", m, rtcmem::measurements[m].id);
      for (s = 0; s < N_SAMPLES; s++) {
        if (samples[s].set) {
          r = new Reading(&rtcmem::measurements[m], samples[s]);
          nRead++;
          // dequeuer will delete
          // don't use block - return to caller instead
          xStatus = xQueueSendToBack(_readingsQueue, &r, pdMS_TO_TICKS(1000));

          if (xStatus == pdPASS) {
            // ok - remove
            samples[s].set = false;
            samples[s].t = 0;
            samples[s].v = 0;
            nSent++;
          } else if (xStatus == errQUEUE_FULL) {
            // queue full - leave in RTC and return
            delete (r);
            r = NULL;
            throw Blob::queue_full("MemoryReader cannot not send to the queue as it is full.");
          } else {
            // odd error
            String m = "MemoryReader cannot not send to the queue. Error:" + String(xStatus);
            throw Blob::queue_full(m.c_str());
          }
        }
      }
    } 
  }
  return nSent;
}

int MemoryReader::nTotalSamples() {

  int nSamples = 0;
  sample_t *samples;

  for (int m = 0; m < N_MEASUREMENTS; m++) {
    samples = rtcmem::measurements[m].samples;
    assert(samples);
    if (rtcmem::measurements[m].id[0] != 0) {
      for (int s = 0; s < N_SAMPLES; s++) {
        if (samples[s].set) {
          nSamples++;
        }
      }
    }
  }
  return nSamples;
}

MemoryReader::~MemoryReader() {
  Reader::end();
}