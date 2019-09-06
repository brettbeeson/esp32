#pragma once

#include "Sensor.h"
#include <Arduino.h>

class Controller {

public:
  Controller(Blob* blob);
  virtual ~Controller();
  virtual void begin(){};
  virtual void end();
  virtual float state()=0;
  void taskify(int taskPeriodMs, int priority,
               int stackDepth = 10000); // returns, spawns a task
  
  virtual void adjust() = 0;
  bool showOnOLED = false;
  virtual String toString();
  friend void ControllerAdjustReadTask(void *controllerPtr);
  int _taskPeriodMs = 0;
friend class Blob;
private:
  TaskHandle_t _taskHandle = NULL;
  Blob* _blob=NULL;
  String _id;
};
