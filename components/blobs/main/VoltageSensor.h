#pragma once
//#include <Arduino.h>
#include "Sensor.h"

//
//   vTotal -|      <--- connected voltage to read (>3.3v)
//           | r2
//   vPin ---|      <--- pin
//           | r1
//          -|      <--- ground
//
class Blob;

class VoltageSensor : public Sensor {

  public:

    VoltageSensor(Blob* blob, int pin, float r1, float r2, float cali = 1.0);
    ~VoltageSensor();
    void begin();
    void read();
    
  private:

    float vPin();
    float vTotal();

    int _pin;
    float _r1, _rt, _cali;
};
