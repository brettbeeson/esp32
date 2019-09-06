#include "OLEDUI.h"
#include "Blob.h"
#include "I2CManager.h"
#include "OLEDButtons.h"
#include "Reading.h"

#undef LOG_LOCAL_LEVEL
#define LOG_LOCAL_LEVEL ESP_LOG_VERBOSE
#include "esp_log.h"

static const char *TAG = "OLED";

OLEDUIClass OLEDUI; // global singleton

void OLEDTask(void *) {

  TickType_t xLastWakeTime;
  int waitForMs;
  xLastWakeTime = xTaskGetTickCount();

  for (;;) {
    waitForMs = OLEDUI.refresh();
    if (waitForMs > 0) {
      vTaskDelayUntil(&xLastWakeTime, pdMS_TO_TICKS(waitForMs));
    }
  }
}

OLEDUIClass::OLEDUIClass() {
  static int i = 0;
  assert(++i == 1); // singleton
}

void OLEDUIClass::begin(uint8_t address) {

  ESP_LOGV(TAG, "begin: addr:0x%04X frames:%d i2c:%s", address, frames.size(),
           I2CManager.c_str());

  I2CManager.take();
  SSD1306_Init();
  SSD1306_Fill(SSD1306_COLOR_BLACK); // clear screen
  SSD1306_UpdateScreen();
  I2CManager.give();

  int core = 1; // xPortGetCoreID();

  ESP_LOGI(TAG, "NOT PINNED begin task on core %d currently running on core %d", core,
           xPortGetCoreID());
  //xTaskCreatePinnedToCore(OLEDTask, "OLED", 2000 /*stack*/, (void *)this,
    //                      1 /* priority */, NULL /* return task handle */,
      //                    core);
                           xTaskCreate(OLEDTask, "OLED", 2000 /*stack*/, (void *)this,
                          1 /* priority */, NULL /* return task handle */);
}

OLEDUIClass::~OLEDUIClass() {
  // off();
  if (_framesButton) {
    delete _framesButton;
    _framesButton = NULL;
  }
  for (int i = 0; i < frames.size(); i++) {
    if (frames[i]) {
      delete (frames[i]);
      frames[i] = 0;
    }
  }
  frames.clear();
}

void OLEDUIClass::message(const String &m) {
  // add to first available frame
  if (frames.size() > 0) {
    (*frames.begin())->add(m);
  }
}

void OLEDUIClass::addFrame(OLEDFrame *frame) {
  assert(frame);
  frames.push_back(frame);
  gotoFrame(0);
}

void OLEDUIClass::gotoFrame(int i) {
  if (i >= 0 && i < frames.size()) {
    _iFrame = i;
  } else {
    ESP_LOGW(TAG, "Tried to set iFrame to %d", i);
  }
}
void OLEDUIClass::setFPS(int fps) {
  assert(fps > 0 && fps < 100);
  _fps = fps;
}

/*
   Returns ms to next redraw to meet internal FPS
*/

int OLEDUIClass::refresh() {

  // Slideshow frames if no button
  if (!_framesButton && millis() - _lastFrameChangeMs > 4000) {
    _lastFrameChangeMs = millis();
    nextFrame();
  }
  SSD1306_Fill(SSD1306_COLOR_BLACK);
  if (_overlay) {
    SSD1306_GotoXY(10, 0);
    _overlay->draw();
  }
  if (_iFrame >= 0 && _iFrame < frames.size()) {
    SSD1306_GotoXY(10, 0);
    frames[_iFrame]->draw();
  }
  SSD1306_UpdateScreen();
  return 100; // improve
  // return _ui->update();
}

void OLEDUIClass::addOverlay(OLEDFrame *frame) {
  if (_overlay) {
    delete (_overlay);
  }
  _overlay = frame;
}
/**
 */
void OLEDUIClass::nextFrame() {
  if (frames.size() == -1)
    return;
  _iFrame++;
  if (_iFrame >= frames.size()) {
    _iFrame = 0;
  }
}

void OLEDUIClass::disableButtons() { CapButton::pollingEnabled = false; }

void OLEDUIClass::enableButtons() { CapButton::pollingEnabled = true; }

void OLEDUIClass::setFramesButton(gpio_num_t framesPin, int threshold) {
  if (_framesButton) {
    delete _framesButton;
  }
  _framesButton = new OledFramesButton(*this, framesPin, threshold);
  _framesButton->begin();
}

///////
///////////
///////////
////////////
OLEDFrame::OLEDFrame(OLEDUIClass &oled) : _oled(oled) {}
OLEDFrame::OLEDFrame() : _oled(OLEDUI) {}

MessagesFrame::MessagesFrame() : OLEDFrame() {}

MessagesFrame::MessagesFrame(OLEDUIClass &oled) : OLEDFrame(oled) {}

void MessagesFrame::draw() {


  int lineHeight = 10 + 1;
//  const int BannerHeight = 20;
  for (int i = 0; i < _messages.size(); i++) {
    SSD1306_GotoXY(0, SSD1306_HEIGHT - i * lineHeight - lineHeight);
    SSD1306_Puts(_messages[i].c_str(), &Font_7x10, SSD1306_COLOR_WHITE);
  }
}

void MessagesFrame::add(const String &m) {
  // FIRST                LAST
  // newest               oldest
  //                      oldest is pop'd and removed
  //[,'b','c','d','e','f']
  // Copy the message to store in query. Caller can delete their instance
  if (_messages.size() > _maxMessages - 1) {
    _messages.pop_back();
  }
  _messages.insert(_messages.begin(), m); // copies
}


OverlayFrame::OverlayFrame() : OLEDFrame() {}

OverlayFrame::OverlayFrame(OLEDUIClass &oled) : OLEDFrame(oled) {}

void OverlayFrame::draw() {
  SSD1306_DrawLine(0, 12, SSD1306_WIDTH - 1, 12, SSD1306_COLOR_WHITE);
  char t[8];
  char m[8];
  snprintf(t, 8, "U:%ds", (int)millis() / 1000);
  snprintf(m, 8, "M:%-6d", (int)esp_get_free_heap_size() / 1000);
  SSD1306_GotoXY(10, 0);
  SSD1306_Puts(t, &Font_7x10, SSD1306_COLOR_WHITE);
  SSD1306_GotoXY(SSD1306_WIDTH - 8 * 7, 0);
  SSD1306_Puts(m, &Font_7x10, SSD1306_COLOR_WHITE);
  /*

    display->setFont(ArialMT_Plain_10);
    display->setTextAlignment(TEXT_ALIGN_RIGHT);
    display->drawString(126, 0, "H:" + String(esp_get_free_heap_size()));
    display->setTextAlignment(TEXT_ALIGN_LEFT);
    display->drawString(20, 0,
                        "U:" + String((int)(esp_timer_get_time() / 1000000)));
    display->drawString(1, 0, (blip++ % 2 == 0) ? "X" : " ");
    display->drawLine(0, 11, 128, 11);
   */
}
