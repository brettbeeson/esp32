#pragma once

#include <freertos/FreeRTOS.h>
#include <freertos/event_groups.h>
#include <Arduino.h>
#include "Blob.h"

class Blob;

void PublisherTask(void*);

class Publisher {

  public:
    Publisher(Blob* blob);
    virtual ~Publisher();
    void end();
    virtual void begin() {};
    // Publish all readings in the queue. Don't block. Publish individually (override for batches)
    virtual int publishReadings();
    // Publish a single reading
    virtual bool publishReading(Reading *r) = 0;
    void taskify(int priority = 1);
    void waitUntilPublished();

    Blob* _blob = NULL;
    int _publishPeriodMs = 1000;
    QueueHandle_t _readingsQueue = NULL;

    eTaskState taskState();

    EventGroupHandle_t _state;
    
    static const int STATE_IDLE = BIT0;

    friend class Blob;

  protected:

  private:
    TaskHandle_t _publishTask=0;
    String _id;
    




};
