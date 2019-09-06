#pragma once

#include <Arduino.h>
#include "Sensor.h"
#include <BME280I2C.h>

class BME280Sensor : public Sensor {

  public:
    BME280Sensor(Blob* blob);
    ~BME280Sensor();
    void begin();
    void read();
    const byte PRES = 0;
    const byte TEMP = 1;
    const byte HUM = 2;

  protected:
    
  private:
    BME280I2C bme;    // Default : forced mode, standby time = 1000 ms; versampling = pressure ×1, temperature ×1, humidity ×1, filter off,
    
};
