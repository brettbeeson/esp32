#include "VoltageSensor.h"
#include "Blob.h"

#include "esp_log.h"
#undef LOG_LOCAL_LEVEL
#define LOG_LOCAL_LEVEL ESP_LOG_VERBOSE
static const char *TAG = "VoltageSensor";
#define TRACE printf("TRACE %s:%d\n", __FILE__, __LINE__);
VoltageSensor::VoltageSensor(Blob *blob, int pin, float r1, float r2,
                             float cali)
    : Sensor(blob), _pin(pin), _r1(r1), _rt(r1 + r2), _cali(cali) {}

VoltageSensor ::~VoltageSensor() {
  ESP_LOGV(TAG, "Destructing.");
  Sensor::end();
  
}

void VoltageSensor::begin() {
  Sensor::begin(1);
  readings[0]->metric = String("battery");
  readings[0]->units = "V";
  readings[0]->id = _blob->id + "-battery";
}

void VoltageSensor::read() {
  float p, t;
  p = vPin();
  t = vTotal();
  ESP_LOGD(TAG, "vPin=%f vBatt=%f", p, t);
  readings[0]->setValue(t);
}

float VoltageSensor::vPin() {
  return ((float)analogRead(_pin) / 4096.0 * 3.3 * _cali);
}

float VoltageSensor::vTotal() { return vPin() * _rt / _r1; }
