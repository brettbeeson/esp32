
#include "BlobSensor.h"
#include "Blob.h"

BlobSensor::BlobSensor(Blob *blob, int *bootCountRTCPtr) : Sensor(blob), _bootCountRTCPtr(bootCountRTCPtr) {
}

BlobSensor::~BlobSensor() {
  Sensor::end();
}

void BlobSensor::begin() {

  int n;
  if (_bootCountRTCPtr) {
    n = 3;
  } else {
    n = 2;
  }

  Sensor::begin(n);
  assert(_blob);
  readings[0]->metric = String("freeheap");
  readings[0]->units = "B";
  readings[0]->id = _blob->id + "-" + readings[0]->metric;

  readings[1]->metric = String("uptime");
  readings[1]->units = "s";
  readings[1]->id = _blob->id + "-" + readings[1]->metric;
  if (_bootCountRTCPtr) {
    readings[2]->metric = String("boots");
    readings[2]->units = "N";
    readings[2]->id = _blob->id + "-" + readings[2]->metric;
  }
}

void BlobSensor::read() {
  readings[0]->setValue(esp_get_free_heap_size()); // esp_get_minimum_free_heap_size() for contiguous;
  readings[1]->setValue(millis() / 1000);          // will overflow!
  if (_bootCountRTCPtr) {
    readings[2]->setValue(*_bootCountRTCPtr); //
  } else {
    readings[2]->setValue(-1); //
  }
}
