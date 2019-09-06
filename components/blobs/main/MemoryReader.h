#pragma once

#include <freertos/FreeRTOS.h>
#include <Arduino.h>
#include "Reader.h"
#include "MemorySaver.h"

class Blob;

class MemoryReader : public Reader {

  public:
  
    MemoryReader(Blob* blob);
    ~MemoryReader();
    int read();
    void begin();
    int nTotalSamples();
    
  private:
    
    
};
