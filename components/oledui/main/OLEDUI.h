#pragma once

#include "ssd1306.h"
#include "fonts.h"
#include "Arduino.h"
#include "CapButton.h"

#include <vector>

using namespace std;

// class OledReadingsButton;
// class OledFramesButton;
class OLEDFrame;

class OLEDUIClass {
public:
  OLEDUIClass();
  ~OLEDUIClass();
  void begin(uint8_t address);
  void setFramesButton(gpio_num_t pin, int threshold = 40);
  int refresh();
  void message(const String &m); // Put message in first available frame
  void addFrame(OLEDFrame *frame);
  void addOverlay(OLEDFrame *frame);
  void setFPS(int fps);
  void nextFrame();
  void gotoFrame(int i);
  void disableButtons();
  void enableButtons();
  vector<OLEDFrame *> frames;
  friend void OLEDTask(void *OLEDPtr);

private:
  CapButton *_framesButton = NULL;
  int _fps = 10;
  uint32_t _lastRefresh = 0;
  int _iFrame = -1; // could be an iterator
  OLEDFrame *_overlay = NULL;
  int _rst = 0; // todo
  long _lastFrameChangeMs = 0;
};

extern OLEDUIClass OLEDUI;

class OLEDFrame {
public:
  OLEDFrame(); // use default
  OLEDFrame(OLEDUIClass &oled);
  virtual ~OLEDFrame(){};
  virtual void draw() = 0;
  virtual void add(const String &message) = 0;

protected:
  OLEDUIClass &_oled;
};

class MessagesFrame : public OLEDFrame {
public:
  MessagesFrame();
  MessagesFrame(OLEDUIClass &oled);
  void draw();
  void add(const String &m);

private:
  vector<String> _messages; // need to make thread safe access?
  int _maxMessages = 4;
  gpio_num_t _readingsButtonPin = GPIO_NUM_0;
};


class OverlayFrame : public OLEDFrame {
public:
  OverlayFrame();
  OverlayFrame(OLEDUIClass &oled);
  void add(const String &message){};
  void draw();

private:
};

