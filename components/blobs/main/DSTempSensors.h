#pragma once

#include "Blob.h"
#include "Sensor.h"
#include <DallasTemperature.h>
#include <OneWire.h>      // https://github.com/stickbreaker/OneWire.git which enters critical section before read
#include <vector>

using namespace std;

//
// calibration table - put in SPIFFS
// use an ice bath
//
// typedef uint8_t DeviceAddress[8];
typedef struct CaliT {
  DeviceAddress addr;
  float offset;
} CaliT;

const CaliT CalibrationTable[] = {
    {.addr = {0x28, 0x9C, 0x35, 0x79, 0x97, 0x11, 0x03, 0x27}, .offset = 0.8},
    {.addr = {0x28, 0x12, 0x8E, 0x79, 0x97, 0x11, 0x03, 0xE1}, .offset = 0.9},
    {.addr = {0x28, 0x0A, 0x57, 0x79, 0x97, 0x11, 0x03, 0xB6}, .offset = 0.9},
    {.addr = {0x28, 0x86, 0xC1, 0x79, 0x97, 0x04, 0x03, 0x19}, .offset = 0.7},
    {.addr = {0x28, 0x01, 0x8E, 0x79, 0x97, 0x11, 0x03, 0xE3}, .offset = 1.1},
    {.addr = {0x28, 0x85, 0x42, 0x79, 0x97, 0x04, 0x03, 0xC5}, .offset = 1.2},
    {.addr = {0x28, 0x53, 0x5B, 0x79, 0x97, 0x04, 0x03, 0x3C}, .offset = 1.3},
    {.addr = {0x28, 0xDD, 0xE8, 0x79, 0x97, 0x04, 0x03, 0xC4}, .offset = 0.4}

};

class DSTempSensor {

public:
  DSTempSensor(DallasTemperature *sensorBus, DeviceAddress addr);
  DSTempSensor() { assert(0); };
  String toString();
  float getTempC();
  const char *getAddrCStr();
  float _calibration = 0;
  bool _calibrated = false;
  

private:
  const int AddrLength = 8;
  DeviceAddress
      _addr; // = {0, 0, 0, 0, 0, 0, 0, 0}; // call sensor by addr - faster
  char _addrCStr[17]; // AddrLen * 2 + 1
  const char *getAddrCStr(DeviceAddress addr);
  DallasTemperature *_sensorBus = NULL;
};

class DSTempSensors : public Sensor {

public:
  DSTempSensors(Blob *blob, int pin);
  ~DSTempSensors();
  void begin();
  // void begin(OneWire *existingBus);
  void read();
  String toString();
  static String addrToString(const DeviceAddress a);
protected:
  

private:
  OneWire *_oneWire; // Setup a oneWire instance to communicate with any OneWire
                     // devices (not just Maxim/Dallas temperature ICs)
  DallasTemperature *_sensorBus; // instance of OneWire bus
  vector<DSTempSensor *>
      _sensors; // store array of addresses, as callilng by address is faster
  int _pin;     // data pin with 4.7k resistor to 3.3V \todo Make configurable
};
