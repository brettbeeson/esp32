#include "CCS811Sensor.h"
#include "Blob.h"
#include "OLEDUI.h"
#include "esp_log.h"

static const char *TAG = __FILE__;

CCS811Sensor::CCS811Sensor(Blob *blob) : Sensor(blob) {}

CCS811Sensor::~CCS811Sensor() {
 Sensor::end();
}

void CCS811Sensor::hardReset(int pin) {
  ESP_LOGI(TAG,"Forcing I2C CCS811 reset of with pin %d", pin);
  pinMode(pin, OUTPUT);
  digitalWrite(pin, LOW); // reset via pull down
  delay(100);
  digitalWrite(pin, HIGH); // ready
  delay(250);
}

void CCS811Sensor::begin() {
  Sensor::begin(3);

  readings[ECO2]->units = "ppm";
  readings[TVOC]->units = "ppb";
  readings[TEMP]->units = "C";
  readings[ECO2]->metric = "eco2";
  readings[TVOC]->metric = "tvoc";
  readings[TEMP]->metric = "temp";
  readings[ECO2]->id = _blob->id + "-" + readings[ECO2]->metric;
  readings[TVOC]->id = _blob->id + "-" + readings[TVOC]->metric;
  readings[TEMP]->id = _blob->id + "-" + readings[TEMP]->metric;

  this->ccs = new Adafruit_CCS811();

  if (!this->ccs->begin()) {
    ESP_LOGE(TAG,"Failed to start CO2 sensor! Please check your wiring.");
    // OLEDUI.message("CCS811Sensor fail");
    while (1)
      ;
  }

  while (!this->ccs->available()) {
    delay(500);
    ESP_LOGI(TAG,"Waiting for CCS811Sensor");
    // OLEDUI.message("Waiting for CCS811Sensor");
  }
  // OLEDUI.message("CCS811Sensor ok");
  this->temp = this->ccs->calculateTemperature();
  this->temp = 25;
  ESP_LOGV(TAG,"CCS811Sensor::temp=%.2f", temp);
  if ((this->temp) < -100 || (this->temp) > 100) {
    throw std::runtime_error("CCS811Sensor:: invalid temperature");
  }
}

void CCS811Sensor::read() {
  int retval;

  Sensor::clearReadings();

  if (ccs->available()) {
    temp = ccs->calculateTemperature();
    temp = 25; // forced \todo
    if (!(retval = ccs->readData())) {
      readings[ECO2]->setValue(ccs->geteCO2());
      readings[TVOC]->setValue(ccs->getTVOC());
      readings[TEMP]->setValue(temp);
    } else {
      throw std::runtime_error("Could not readData() CCS811 sensor");
    }
    ESP_LOGV(TAG,"CCS811Sensor::readHardware eco2 %.2f temp %.2f tvoc %.2f",
           readings[ECO2]->getValue(), temp, readings[TVOC]->getValue());
  } else {
    throw std::runtime_error("Could not available() CCS811 sensor");
  }
}


