#pragma once
#include "Reading.h"
#include <time.h>

class Blob;

// void SensorReadTask(void*);



class Sensor {



public:
  Sensor(Blob *blob, String id = "");
  virtual ~Sensor();
  virtual void begin()=0;                            // subclass to implement and call begin(nreadings)
  virtual void begin(int _nReadings);                // starts hardware
  void taskify(int sampleFrequencyMs, int priority); // returns, spawns a task
  // Stops the task. MUST call this in derived class destructor or at less
  // before ~Sensor
  virtual void end();
  virtual void read() = 0; // ask hardware to run, result stored in lastReadings
  void timestamp(time_t t);
  const Reading getReading(int channel);
  Reading *getReadingPtr(int channel, bool throwIfNull = true);
  void clearReadings();
  // throws QueueFull
  void  copyReadingsToQueue(int timeoutMs=1000);

  friend void SensorReadTask(void *SensorPtr);
  friend class Blob;
    void setLocation(String location);
  void setID(String id);
  String toString(bool brief = false);
  Blob *_blob = NULL;
  
  int nReadings=0; // subclass to set  

protected:
  
  String _id;
  QueueHandle_t _readingsQueue = NULL;
  Reading **readings = NULL; // Array of pointers. Sensor can have multiple
                             // readings (e.g. RH, temp).
  int _taskPeriodMs = -1;
  TaskHandle_t _taskHandle = NULL;

private:
};
