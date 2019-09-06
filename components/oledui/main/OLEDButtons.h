#pragma once

#include "CapButton.h"
#include "OLEDUI.h"

class OledFramesButton : public CapButton {
public:
  OledFramesButton( OLEDUIClass &oled, gpio_num_t pin, int threshold = 40);
  void keypressEvent();
  void keyupEvent();

private:
   OLEDUIClass &_oled;
};

