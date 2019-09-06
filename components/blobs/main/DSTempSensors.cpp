#include "DSTempSensors.h"
#include "Blob.h"
#include <DallasTemperature.h>

#undef LOG_LOCAL_LEVEL
#define LOG_LOCAL_LEVEL ESP_LOG_VERBOSE
#include "esp_log.h"
static const char *TAG = "DSTempSensor";

#define MAX_POSSIBLE_SENSORS 10


String DSTempSensors::addrToString(const DeviceAddress a) {
  char s[17];
  for (int i = 0; i < 8; i++) {
    snprintf(&(s[i * 2]), 3 /* 2 \0 */, "%02X", a[i]);
  }
  return String(s);
}

/**
   DSTempSensor
*/
DSTempSensor ::DSTempSensor(DallasTemperature *sensorBus, DeviceAddress addr) {
  _sensorBus = sensorBus;

  // copy addr : it's an array
  memcpy(_addr, addr, AddrLength);
  // make _addrCStr
  for (int i = 0; i < AddrLength; i++) {
    snprintf(&(_addrCStr[i * 2]), 3 /* 2 \0 */, "%02X", _addr[i]);
  }

  bool match = true;
  int nCali = sizeof(CalibrationTable) / sizeof(CaliT);

  for (int i = 0; _calibrated == false && i < nCali; i++) {
    match = true;
    for (int j = 0; j < 8; j++) { // each byte
      match = match && (CalibrationTable[i].addr[j] == _addr[j]);
    }
    if (match) {
      _calibrated = true;
      _calibration = CalibrationTable[i].offset;
    }
  }
}

String DSTempSensor::toString() {
  char buf[256];
  snprintf(buf, 256, "addr:%s cali[%d]:%3f res:%d", _addrCStr, _calibrated, _calibration, _sensorBus->getResolution(_addr));
  return String(buf);
}

float DSTempSensor::getTempC() {
  return (_sensorBus->getTempC(_addr) + _calibration);
}

const char *DSTempSensor::getAddrCStr() { return _addrCStr; }

/**
   DSTempSensors
*/

DSTempSensors::DSTempSensors(Blob *blob, int pin) : Sensor(blob), _pin(pin) {}

DSTempSensors::~DSTempSensors() {

  Sensor::end();
  for (int i = 0; i < _sensors.size(); i++) {
    delete (_sensors[i]);
    _sensors[i] = NULL;
  }

  if (_oneWire) {
    delete (_oneWire);
    _oneWire = NULL;
  }
  if (_sensorBus) {
    delete (_sensorBus);
    _sensorBus = NULL;
  }
}

/*
 */
void DSTempSensors::begin() {

  /*
// Reliable - first run through all indexes, then get the address of each
  int n;
  for (n = 0; n <= MAX_POSSIBLE_SENSORS &&
              _sensorBus->getTempCByIndex(n) != DEVICE_DISCONNECTED_C;
       n++) {
  };
  */
  DeviceAddress addr;
  int n;

  _oneWire = new OneWire(_pin);
  _sensorBus = new DallasTemperature(_oneWire);
  _sensorBus->begin();
  _sensorBus->setResolution(10); // 9,10,11,12

  n = _sensorBus->getDS18Count();

  DSTempSensor *dts = NULL;
  for (int i = 0; i < n; i++) {
    if (_sensorBus->getAddress(addr, i)) {
      dts = new DSTempSensor(_sensorBus, addr);
      _sensors.push_back(dts);
    
      if (!_sensors.back()->_calibrated) {
        ESP_LOGW(TAG, "No calibration for dts:%s", _sensors.back()->getAddrCStr());
      }
    } else {
      ESP_LOGW(TAG, "Phantom address DS #%d %s", i, addrToString(addr).c_str());
    }
  }

  ESP_LOGI(TAG, "Found: %d sensors on pin: %d", (int)_sensors.size(), _pin);

  Sensor::begin(_sensors.size()); // call after nReadings is known

  for (int i = 0; i < _sensors.size(); i++) {
    readings[i]->metric = String("temperature");
    readings[i]->units = String("C");
    readings[i]->id = _blob->id + "-" + String(_sensors[i]->getAddrCStr());
    readings[i]->id = String(_sensors[i]->getAddrCStr());
  }
}

String DSTempSensors::toString() {
  String r;
  for (int i = 0; i < _sensors.size(); i++) {
    r += i>0 ? "," : "";
    r = r + _sensors[i]->toString();
  }
  return r;
}
/*
  \todo Consider ASYNC to save energy (see setWaitForConversion)
*/
void DSTempSensors::read() {

  double temp = 666;

  _sensorBus->requestTemperatures();
  for (int i = 0; i < _sensors.size(); i++) {
    temp = _sensors[i]->getTempC();
    if (temp == DEVICE_DISCONNECTED_C || temp < -100 || temp > 100) {
      ESP_LOGW(TAG, "Could not read temperature data ( %f) on unit #%d : %s", temp,
               i, _sensors[i]->getAddrCStr());
      readings[i]->setValue(NAN);
    } else {
      readings[i]->setValue(temp);
      ESP_LOGV(TAG, "Read temperature (%f) on unit #%d : %s", temp, i, _sensors[i]->getAddrCStr());
    }
  }
}
