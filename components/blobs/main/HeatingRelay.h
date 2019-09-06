#pragma once

#include "Relay.h"

class HeatingRelay : public Relay {
public:
  HeatingRelay(Blob* blob,Sensor *inputSensor, int outputRelayPin, float setpoint,
               float deadband);
  void adjust(); // change relay state if required
  float state();
String toString();
private:
};
