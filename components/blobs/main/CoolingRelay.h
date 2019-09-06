#pragma once

#include "Relay.h"

class CoolingRelay : public Relay {
public:
  CoolingRelay(Blob* blob,Sensor *inputSensor, int outputRelayPin, float setpoint,
               float deadband);
  void adjust(); // change relay state if required
  String toString();
  float state();
};
