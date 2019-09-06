#pragma once
#include <Arduino.h>
#include <freertos/FreeRTOS.h>
#include <rom/rtc.h>
#include <vector>
#include <stdexcept>

using namespace std;

class Controller;
class Publisher;
class Sensor;
class Reader;
class Reading;
class BlobWebServer;

class Blob {

public:
  class queue_full : public runtime_error {

    public:
      queue_full(const char* msg) : runtime_error(msg){}

      
    const char *what() const throw() {
      return "Queue is full.";
    }
  };

  enum Event {
    PublishEvent,
    UnknownEvent
  };

  Blob();
  virtual ~Blob();
  virtual void begin(int useServicesMask);
  virtual void begin();
  virtual void end();
  virtual void refresh();
  // Return Readings published
  virtual int publish();
  // false if queue if full, true if read ok
  void readSensors(int timeoutMs=1000);
  void readSensorsTaskify(int priority = 1, int periodMs = 10000);
  void readReaders();
  void adjustControllers();
  void addReading(Reading *r, int iReadingsQueue = 0);
  String toString();
  bool configFromJSON(Stream &s);

  void testBasicFunctions();
  void raiseEvent(Event e){};
  void printReadingsQueue(int i = 0);
  void printReadings();
  QueueHandle_t readingsQueue(int i);
  UBaseType_t readingsInQueue(int i = 0);

  String location = "default";
  String id;

  void add(Publisher *p);
  void add(Reader *r);
  void add(Controller *c);
  void add(Sensor *s);

  void remove(Publisher *p);
  void remove(Reader *r);
  void remove(Controller *c);
  void remove(Sensor *s);

  static const unsigned int USE_I2C = 1 << 0;
  static const unsigned int USE_SD = 1 << 1;

  static const unsigned int USE_LORA = 1 << 3;
  static const unsigned int USE_HTTPD = 1 << 4;

  friend void ReadSensorsTask(void *);

protected:
  Publisher *publisher = NULL;
  Reader *_reader = NULL;
  vector<Sensor *> sensors;
  vector<Controller *> controllers;

  static String getMacStr();

  void setupVoltageMeasurement();

private:
  int _readSensorsTaskPeriod = 10000;
  int useServicesMask;

  bool testSDCard();
  bool testLora();
  static SemaphoreHandle_t i2cSemaphore;

  QueueHandle_t _readingsQueue0;
  QueueHandle_t _readingsQueue1;

  void setupI2CBus();
  void setupSDCard();
  void setupLora();
  void setupWebServer();
};
