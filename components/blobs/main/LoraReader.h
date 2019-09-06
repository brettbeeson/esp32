#pragma once

#include <freertos/FreeRTOS.h>
#include <Arduino.h>
#include "LoRa.h"     // use local copy
#include "Reader.h"

void LoraReaderOnReceive(int packetSize, void* LoraReaderPtr);

class Blob;

class LoraReader : public Reader {

  public:
    LoraReader(Blob* blob, int packetsQueueLength = 10);
    ~LoraReader();
    int read();
    void begin();
    void taskify(int , int );
    const int TimestampRoundingS = 10; // untimestamped Readings will be stamp with now(), rounded to this value
    friend void LoraReaderParserTask(void *_lr);
    friend void LoraReaderPacketsAvailableISR(int packetSize);
  
  private:
    void receivePackets(int packetSize);
    void parsePackets();
    QueueHandle_t _packetsQueue;
    //volatile int _packetSize;
    //TaskHandle_t _receivePacketsTaskHandle;
    TaskHandle_t _parsePacketsTask = NULL;
};
