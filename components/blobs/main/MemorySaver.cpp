#include "MemorySaver.h"
#include <Arduino.h>
#include <math.h>
#include <string.h>

#undef LOG_LOCAL_LEVEL
#define LOG_LOCAL_LEVEL ESP_LOG_VERBOSE
#include "esp_log.h"
static const char *TAG = "MemorySaver";

namespace rtcmem {
RTC_DATA_ATTR measurement_t measurements[N_MEASUREMENTS];
RTC_DATA_ATTR sample_t samples[N_MEASUREMENTS][N_SAMPLES];
RTC_DATA_ATTR bool inited = false;
} // namespace rtcmem

MemorySaver::MemorySaver(Blob *blob) : Publisher(blob) {
  if (!rtcmem::inited) {
    init();
  }
}

void MemorySaver::init() {
  ESP_LOGI(TAG, "MemorySaver::init(): inited: %d measurements[%d].samples[%d]",
           (int)rtcmem::inited, N_MEASUREMENTS, N_SAMPLES);

  for (int m = 0; m < N_MEASUREMENTS; m++) {
    memset(rtcmem::measurements[m].id, 0, READING_STR);
    memset(rtcmem::measurements[m].units, 0, READING_STR);
    memset(rtcmem::measurements[m].metric, 0, READING_STR);
    memset(rtcmem::measurements[m].location, 0, READING_STR);

    rtcmem::measurements[m].samples = rtcmem::samples[m];
    for (int s = 0; s < N_SAMPLES; s++) {
      rtcmem::measurements[m].samples[s].v = NAN;
      rtcmem::measurements[m].samples[s].t = 0;
      rtcmem::measurements[m].samples[s].set = false;
    }
  }
  rtcmem::inited = true;
}

void MemorySaver::begin() {}

void MemorySaver::print() {
  // char timeStr[256];
  ESP_LOGD(TAG, "MemorySaver #m: %d #s:%d", N_MEASUREMENTS, N_SAMPLES);
  for (int m = 0; m < N_MEASUREMENTS; m++) {
    ESP_LOGD(TAG, "MemorySaver::print(): measurements[%d].samples=%d", m,
             (int)rtcmem::measurements[m].samples);
    assert(rtcmem::measurements[m].samples != NULL);
    if (rtcmem::measurements[m].id[0] != 0) {
      for (int s = 0; s < N_SAMPLES; s++) {
        if (rtcmem::measurements[m].samples[s].set == true) {
          // strncpy(timeStr, ctime(&rtcmem::measurements[m].samples[s].t),
          // 255); timeStr[strlen(timeStr) - 1] = '\0';
          printf("m:%d s:%d : t:%lu v:%f", m, s,
                 rtcmem::measurements[m].samples[s].t,
                 rtcmem::measurements[m].samples[s].v);
        } else {
          printf("No sample at:%d", s);
        }
      }
    } else {
      Serial.printf("No ID for measurement:%d", m);
    }
  }
}

measurement_t *MemorySaver::findMeasurement(String id) {
  for (int m = 0; m < N_MEASUREMENTS; m++) {
    // ESP_LOGV(TAG,"Finding at measurement %d id: %s, looking for id:%s", m,
    // rtcmem::measurements[m].id, id.c_str());
    if (strcmp(id.c_str(), rtcmem::measurements[m].id) == 0) {
      // ESP_LOGV(TAG,"Found measurement %d for id:%s at %d", m, id.c_str(),
      // &(rtcmem::measurements[m]));
      return &(rtcmem::measurements[m]);
    }
  }
  return NULL;
}

measurement_t *MemorySaver::addMeasurement(String id) {

  // assert(findMeasurement(id) == false);
  for (int m = 0; m < N_MEASUREMENTS; m++) {
    if (rtcmem::measurements[m].id[0] == 0) {
      strncpy(rtcmem::measurements[m].id, id.c_str(), READING_STR - 1);
       ESP_LOGV(TAG,"AddedMeasurement %d for id:%s", m, id.c_str());
      return &(rtcmem::measurements[m]);
    }
  }
  return NULL;
}

bool MemorySaver::publishReading(Reading *r) {

  measurement_t *m;
   ESP_LOGV(TAG,"Publishing (to memory)  reading %s", r->tostr().c_str());
  if (!(m = findMeasurement(r->id))) {
    if (!(m = addMeasurement(r->id))) {
      ESP_LOGW(TAG, "No space for adding measurement %s in RTC_DATA. N_MEASUREMENTS:%d N_SAMPLES:%d",r->id.c_str(),N_MEASUREMENTS,N_SAMPLES);
      return false;
    }
  }

  // todo move to readings.h
  // r = m;
  // todo : not "-1"?

  // todo: only required on "addMeasurement?"
  strncpy(m->id, r->id.c_str(), READING_STR - 1);
  strncpy(m->location, r->location.c_str(), READING_STR - 1);
  strncpy(m->metric, r->metric.c_str(), READING_STR - 1);
  strncpy(m->units, r->units.c_str(), READING_STR - 1);

  // sample_t *samples;

  // Add the sample to list
  // Search list for null value (.set == 0)
  for (int s = 0; s < N_SAMPLES; s++) {
    if (m->samples[s].set == false) {
      m->samples[s].v = r->getValue();
      m->samples[s].t = r->timestamp;
      m->samples[s].set = true;
      return true;
    }
  }
  // could find the oldest and add there...
  ESP_LOGW(TAG, "No space for more measurement samples in RTC_DATA");
  return false;
}

int MemorySaver::nTotalSamples() {

  int nSamples = 0;
  sample_t *samples;

  for (int m = 0; m < N_MEASUREMENTS; m++) {
    samples = rtcmem::measurements[m].samples;
    assert(samples);
    if (rtcmem::measurements[m].id[0] != 0) {
      for (int s = 0; s < N_SAMPLES; s++) {
        if (samples[s].set == true) {
          nSamples++;
        }
      }
    }
  }
  return nSamples;
}

int MemorySaver::nMeasurements() {

  int nMeasurements = 0;

  for (int m = 0; m < N_MEASUREMENTS; m++) {
    if (rtcmem::measurements[m].id[0] != 0) {
      nMeasurements++;
    }
  }
  return nMeasurements;
}

MemorySaver::~MemorySaver() {
  //ESP_LOGV(TAG, "Destructing.");
  Publisher::end();
}