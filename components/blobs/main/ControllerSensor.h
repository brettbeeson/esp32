#pragma once
#include <Arduino.h>
#include "Controller.h"

/**
 * 
 */
class ControllerSensor : public Sensor {
  public:
    ControllerSensor(Blob* blob,Controller &controller,String id="");
    ~ControllerSensor();
    void begin();
    void read();

  
  private:
    Controller & _controller;
    
    
};
