#include "LoraPublisher.h"
#include "Reading.h"
#include "Blob.h"
#include "LoRa.h"
#include "OLEDUI.h"
#include <Arduino.h>

#undef LOG_LOCAL_LEVEL
#define LOG_LOCAL_LEVEL ESP_LOG_VERBOSE
#include "esp_log.h"
static const char *TAG = "LoraPublisher";

LoraPublisher::LoraPublisher(Blob *blob) : Publisher(blob) {}

LoraPublisher::~LoraPublisher() { Publisher::end(); }
void LoraPublisher::begin() {
  // check lora is running
}

bool LoraPublisher::publishReading(Reading *r) {

  OLEDUI.disableButtons(); // Lora stuffs the capacitance touch

  assert(LoRa.beginPacket() == 1);
  LoRa.print(r->toJSON());
  LoRa.endPacket(false); // blocks until sen

  ESP_LOGV(TAG,"LoRa: %s",r->toJSON().c_str());
  OLEDUI.enableButtons();
  return true;
}
