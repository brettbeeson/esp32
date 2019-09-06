#include "BME280Sensor.h"

#include "Blob.h"
#include "I2CManager.h"

#include <stdexcept>

#undef LOG_LOCAL_LEVEL
#define LOG_LOCAL_LEVEL ESP_LOG_VERBOSE
#include "esp_log.h"
static const char *TAG = "BMS280Sensor";

BME280Sensor::BME280Sensor(Blob* blob) :
  Sensor (blob) {
}

void BME280Sensor::begin() {
  Sensor::begin(3);
  ESP_LOGV(TAG,"BME280Sensor::begin");

  int i;
  for (i = 0; i < 10 && !bme.begin(); i++) {
    delay(250);
  }

  if (i == 10) {
    throw std::runtime_error("Could not find BME280 sensor");
  }

  switch (bme.chipModel())  {
    case BME280::ChipModel_BME280:
      ESP_LOGI(TAG,"BME280");
      break;
    case BME280::ChipModel_BMP280:
      ESP_LOGI(TAG,"BMP280 without humidity.");
      break;
    default:
      throw std::runtime_error("BMXXXXX unknown sensor");
  }
  // calibrate? pressure?
  //OLEDUI.message("BME280Sensor:ok");
  readings[PRES]->id = _blob->id + "-pressure";
  readings[PRES]->metric = String("pressure");
  readings[PRES]->units = String("Pa");
  readings[HUM]->metric = String("humidity");
  readings[HUM]->units = String("PC");
  readings[HUM]->id = _blob->id + "-humidity";
  readings[TEMP]->metric  = String("temperature");
  readings[TEMP]->units  = String("C");
  readings[TEMP]->id = _blob->id + "-temperature";
}

void BME280Sensor::read() {
  BME280::TempUnit tempUnit(BME280::TempUnit_Celsius);
  BME280::PresUnit presUnit(BME280::PresUnit_Pa);
  float temp = NAN;
  float hum = NAN;
  float pres = NAN;

  I2CManager.take();
  bme.read(pres, temp, hum, tempUnit, presUnit);
  I2CManager.give();
  ESP_LOGV(TAG,"BME280Sensor::read pres %.2f temp %.2f hum %.2f", pres, temp, hum);

  readings[PRES]->setValue(  pres);
  readings[HUM]->setValue ( hum);
  readings[TEMP]->setValue  (temp);

}

BME280Sensor::~BME280Sensor() {
  Sensor::end();
  }
