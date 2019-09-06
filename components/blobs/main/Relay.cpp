#include "Relay.h"
#include "Blob.h"
#include "OLEDUI.h"
#include "Sensor.h"

Relay ::Relay(Blob* blob,Sensor *inputSensor, int outputRelayPin, float setpoint,
              float deadband)
    : Controller(blob), _currentState(_off), _deadband(deadband), _lastChangeMs(0),
      _relayPin(outputRelayPin), _sensor(inputSensor), _setpoint(setpoint) {
  
}

void Relay ::begin() {
  pinMode(_relayPin, OUTPUT);
  digitalWrite(_relayPin, _currentState);
}

void Relay ::end() { _sensor = NULL; }

String Relay::toString() {
  String r;
  r += _currentState ? "ON" : "OFF";
  r += ", ";
  r += String(_setpoint, 1);
  r += "+-";
  r += String(_deadband / 2, 1);
  r += "C";
  return r;
}
