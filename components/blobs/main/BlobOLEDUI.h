#pragma once

#include "OLEDUI.h"
#include "Reading.h"
#include "Sensor.h"

class ReadingsFrame : public OLEDFrame {
public:
  ReadingsFrame(gpio_num_t pin = GPIO_NUM_0, int threshold = -1);
  ReadingsFrame(OLEDUIClass &oled, gpio_num_t pin = GPIO_NUM_0,
                int threshold = -1);
  ~ReadingsFrame();
  void draw();
  void add(const String &message){};
  void add(Sensor& s);
  void clearReadings();

  void enableButtons();
  void disableButtons();
  // void setButtons(gpio_num_t framesPin, gpio_num_t readingsPin, int
  // threshold);

  CapButton *_readingsButton;

  friend class OledReadingsButton;
  friend class OledFramesButton;

private:
  vector<Reading *> _readings;
  size_t _iReading = 0;
  long _lastReadingChangeMs = 0; // or via a press button
  void nextReading();

  
};


class OledReadingsButton : public CapButton {
public:
  OledReadingsButton( OLEDUIClass &oled, ReadingsFrame *readingsFrame,
                     gpio_num_t pin, int threshold = 40);
  void keypressEvent();
  void keyupEvent();

private:
   OLEDUIClass &_oled;
  ReadingsFrame *_readingsFrame;
};
