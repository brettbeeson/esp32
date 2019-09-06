#pragma once

#include "Controller.h"
#include "Sensor.h"
#include <Arduino.h>

/*
 * setpoint and deadband:
 *
 *        setpoint
 *            V
 * 23         24          25
 * |<---- deadband ------->|
 *
 * Cases:
 * Is OFF: If >25, turn ON
 * Is ON : If <23, turn OFF
 * Changes only allowed once per minute.
 *
 * _on and _off are required as some relays as *active* when output is *low*
 */

class Relay : public Controller {
public:
  Relay(Blob* blob,Sensor *inputSensor, int outputRelayPin, float setpoint,
        float deadband);
  void begin();
  void end();
  virtual void adjust() = 0;
  virtual String toString();
  int _minChangeMs = 10 * 1000;
  int _on = LOW;   // this value of digital outputs turns on *device* ...
  int _off = HIGH; // ... and this value of digital output turns it off.
  bool on() { return _currentState==_on; }

protected:
  int _currentState = _off;
  float _deadband = 0;
  long _lastChangeMs = 0;
  int _relayPin = -1;
  Sensor *_sensor = NULL;
  float _setpoint = 0;
  
};
