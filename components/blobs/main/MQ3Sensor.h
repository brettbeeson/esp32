#pragma once

#include "Sensor.h"

class MQ3Sensor : public Sensor {

  public:
    MQ3Sensor(Blob* blob, int pin);
    ~MQ3Sensor();
    void begin();
    void read();

  private:
    int _pin;
    float rawReading;
    float percentReading;
    

};
