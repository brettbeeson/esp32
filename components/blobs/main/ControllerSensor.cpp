
#include "ControllerSensor.h"
#include "Blob.h"

ControllerSensor::ControllerSensor(Blob* blob,Controller &c,String name) :
  Sensor (blob,name), _controller(c) {
}


ControllerSensor::~ControllerSensor() {
  Sensor::end();
}


void ControllerSensor::begin() {
  Sensor::begin(1);
  assert(_blob);
  readings[0]->metric = String("state:") + _id;
  readings[0]->units = "X";
  readings[0]->id = _id;
 
}

void ControllerSensor::read() {
  readings[0]->setValue(_controller.state()); 
}


