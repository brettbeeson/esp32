#include "HeatingRelay.h"
#include "OLEDUI.h"
#include "esp_log.h"
#undef LOG_LOCAL_LEVEL
#define LOG_LOCAL_LEVEL ESP_LOG_VERBOSE
static const char *TAG = "HeatingRelay";

HeatingRelay ::HeatingRelay(Blob* blob,Sensor *inputSensor, int outputRelayPin,
                            float setpoint, float deadband)
    : Relay(blob,inputSensor, outputRelayPin, setpoint, deadband) {}

void HeatingRelay ::adjust() {
  try {
    Reading *r = _sensor->getReadingPtr(0);
    if (!r || std::isnan(r->getValue())) {
      ESP_LOGW(TAG, "No temp reading available for HeatingRelay");
      return;
    }
    if (_lastChangeMs == 0) {
      ESP_LOGI(TAG, "Initially (pin %d): %s ", _relayPin,
               _currentState == _on ? "ON" : "OFF");
      OLEDUI.message("HeatingRelay: OFF");
      _lastChangeMs = millis();
    }
    if (millis() > _lastChangeMs + _minChangeMs) {
      if (_currentState == _on) {
        if (r->getValue() > _setpoint + _deadband / 2) {
          _currentState = _off;
          _lastChangeMs = millis();
          digitalWrite(_relayPin, _currentState);
          ESP_LOGI(TAG, "Set relay (pin %d) to: %s as temp: %s", _relayPin,
                   _currentState == _on ? "ON" : "OFF", r->valueStr().c_str());

          OLEDUI.message("HeatingRelay: OFF");
        }
      } else if (_currentState == _off) {
        if (r->getValue() < _setpoint - _deadband / 2.0f) {
          _currentState = _on;
          _lastChangeMs = millis();
          digitalWrite(_relayPin, _on);
          ESP_LOGI(TAG, "Set relay (pin %d) to: %s as temp: %s", _relayPin,
                   _currentState == _on ? "ON" : "OFF", r->valueStr().c_str());

          OLEDUI.message("HeatingRelay: ON");
        }
      }
    }
  } catch (std::exception &e) {
    ESP_LOGW(TAG, "Exception: %s", e.what());
  }
}
String HeatingRelay::toString() {
  String r;
  r += "H: ";
  r += _currentState ? "ON" : "OFF";
  r += ", ";
  r += String(_setpoint, 1);
  r += "+-";
  r += String(_deadband / 2, 1);
  r += "C";
  return r;
}
float HeatingRelay::state() { return (_currentState == _on ? 1 : 0); }