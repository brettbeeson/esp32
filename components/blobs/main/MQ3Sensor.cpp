#include "MQ3Sensor.h"
#include "Blob.h"


MQ3Sensor ::MQ3Sensor (Blob* blob, int pin)
  : Sensor(blob), _pin(pin) {
}

void MQ3Sensor ::begin() {
  Sensor::begin(1);
  readings[0]->metric = "alcohol";
  readings[0]->units = "percent";  // percent of maximum
  readings[0]->id = _blob->id + "-MQ3Alcohol";
  readings[0]->location = "air";
  pinMode(_pin, INPUT);
}

MQ3Sensor ::~MQ3Sensor () {
 Sensor::end();
}


void MQ3Sensor ::read() {
  readings[0]->setValue ( float(analogRead(_pin)) / 4096.0);
  //ESP_LOGV(TAG,"MQ3Sensor rawRead:%d", analogRead(_pin) );
}
