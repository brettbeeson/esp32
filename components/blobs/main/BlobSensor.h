#pragma once
#include <Arduino.h>
#include "Sensor.h"


/**
 * Monitor internal parts of ESP32 (memory, uptime, etc.)
 */
class BlobSensor : public Sensor {
  public:
    BlobSensor(Blob* blob,int *bootCountRTCPtr=0);
    ~BlobSensor();
    void begin();
    void read();
    
      
  private:
    int *_bootCountRTCPtr=0;
    
    
};
