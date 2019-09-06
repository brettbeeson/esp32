#pragma once

#include "driver/i2c.h"
#include <freertos/FreeRTOS.h>
#include "Arduino.h"

class I2CManagerClass {
public:

  I2CManagerClass(gpio_num_t sda = GPIO_NUM_21, gpio_num_t scl = GPIO_NUM_22);
  ~I2CManagerClass();
  String str();
  const char* c_str();
  void begin();
  void end();
  void take();
  void give();
  
  gpio_num_t _sclPin = GPIO_NUM_0;
  gpio_num_t _sdaPin = GPIO_NUM_0;
   int scan();
   SemaphoreHandle_t _access;
   
   private:
   bool _ready = false;
};

extern I2CManagerClass I2CManager;
