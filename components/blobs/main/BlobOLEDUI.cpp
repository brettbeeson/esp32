#include "BlobOLEDUI.h"
#undef LOG_LOCAL_LEVEL
#define LOG_LOCAL_LEVEL ESP_LOG_VERBOSE
#include "esp_log.h"
static const char *TAG = "BlobOLEDUI";

ReadingsFrame::ReadingsFrame(gpio_num_t readingsButtonPin, int threshold)
    : OLEDFrame(OLEDUI) {
  if (readingsButtonPin != GPIO_NUM_0 && threshold > 0) {
    _readingsButton =
        new OledReadingsButton(_oled, this, readingsButtonPin, threshold);
    _readingsButton->begin();
  }
}

ReadingsFrame::ReadingsFrame(OLEDUIClass &oled, gpio_num_t readingsButtonPin,
                             int threshold)
    : OLEDFrame(oled) {
  if (readingsButtonPin != GPIO_NUM_0 && threshold > 0) {
    _readingsButton =
        new OledReadingsButton(_oled, this, readingsButtonPin, threshold);
    _readingsButton->begin();
  }
}

ReadingsFrame::~ReadingsFrame() {
  if (_readingsButton) {
    delete _readingsButton;
    _readingsButton = NULL;
  }
}

void ReadingsFrame::nextReading() {
  if (_readings.size() == 0)
    return;
  _iReading++;
  if (_iReading == _readings.size())
    _iReading = 0;
}

void ReadingsFrame::draw() {
  SSD1306_GotoXY(0, 15);
  if (_readings.size() == 0) {
    SSD1306_Puts("No readings", &Font_7x10, SSD1306_COLOR_WHITE);
    SSD1306_GotoXY(40, 4);
  } else {
    if (!_readingsButton && millis() - _lastReadingChangeMs > 3000) {
      _lastReadingChangeMs = millis();
      nextReading();
    }
    Reading *r = _readings[_iReading];
    char val[6];
    SSD1306_GotoXY(0, 14);
    String name = r->metric.substring(0, 4);
    if (_iReading == 0) {
      name += "*"; // first one is the controller
    }
    SSD1306_Puts(name.c_str(), &Font_11x18, SSD1306_COLOR_WHITE);

    SSD1306_GotoXY(64, 14);
    snprintf(val, sizeof(val), "%6.2f", r->getValue());
    SSD1306_Puts(val, &Font_11x18, SSD1306_COLOR_WHITE);
    SSD1306_GotoXY(0, 40);
    String id = r->id;
    if (id.length()>16){
      id = id.substring(id.length()-16);
    }
    SSD1306_Puts(id.c_str(), &Font_7x10, SSD1306_COLOR_WHITE);
    SSD1306_GotoXY(0, 50);
    SSD1306_Puts(r->location.c_str(), &Font_7x10, SSD1306_COLOR_WHITE);
  }
}

/*
   readings is an array of pointers owned by a Sensor so ... don't fuck with
   them. The caller should call clearReadings() before deleting them on their
   side
*/
void ReadingsFrame::add(Sensor &s) {
  ESP_LOGI(TAG, "Adding %d readings from %s to OLED", s.nReadings,
           s.toString(true).c_str());
  for (int i = 0; i < s.nReadings; i++) {
    assert(s.getReadingPtr(i));
    _readings.push_back(s.getReadingPtr(i));
  }
  _iReading = 0;
}

void ReadingsFrame::clearReadings() {
  _readings.clear();
  _iReading = -1;
}

OledReadingsButton::OledReadingsButton(OLEDUIClass &oled,
                                       ReadingsFrame *readingsFrame,
                                       gpio_num_t pin, int threshold)
    : CapButton(pin, threshold), _oled(oled), _readingsFrame(readingsFrame)

{}

void OledReadingsButton::keypressEvent() {
  ESP_LOGV(TAG, "OledReadingsButton KeyPress on pin %d", (int)_pin);
  // digitalWrite(LED_BUILTIN, HIGH);
  _readingsFrame->nextReading();
  //_readingsFrame->refresh();
}
void OledReadingsButton::keyupEvent() {
  ESP_LOGV(TAG, "OledReadingsButton Keyup on pin %d", (int)_pin);
  // digitalWrite(LED_BUILTIN, LOW);
}
