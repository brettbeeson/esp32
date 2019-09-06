#include "OLEDButtons.h"

#undef LOG_LOCAL_LEVEL
#define LOG_LOCAL_LEVEL ESP_LOG_VERBOSE
#include "esp_log.h"
static const char *TAG = "OLEDButtons";


/**
   OledFramesButton
*/

OledFramesButton::OledFramesButton( OLEDUIClass &oled, gpio_num_t pin, int threshold) :
  CapButton(pin, threshold),
  _oled(oled)
{}

void OledFramesButton::keypressEvent() {
  ESP_LOGV(TAG,"OledFramesButton KeyPress on pin %d", (int) _pin);
  //digitalWrite(LED_BUILTIN, 1);
  _oled.nextFrame();
  //_oled.refresh();

}
void OledFramesButton::keyupEvent() {
  ESP_LOGV(TAG,"OledFramesButton Keyup on pin %d", (int) _pin);
  //digitalWrite(LED_BUILTIN, 0);
}
