#pragma once 

#include <freertos/FreeRTOS.h>
#include <Arduino.h>

class Blob;

void ReaderTask(void*);

class Reader {
  
  public:
    Reader(Blob* blob);
    virtual ~Reader() {};
    virtual void  end();
    virtual void taskify(int readFrequencyMs, int priority);
    // Return -1 on error, 0 on none-to-send, >0 for successful sends
    virtual int read() = 0;
    
    friend class BlobBridge;
    friend class Blob;
    friend void ReaderTask(void *_p);
    
  protected:
    virtual void begin() = 0;
    QueueHandle_t _readingsQueue = NULL;
    int _readPeriodMs;
    Blob* _blob;
    TaskHandle_t _taskHandle=0;
    String _id;
    
  private:

};
