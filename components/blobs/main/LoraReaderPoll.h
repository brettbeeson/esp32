#pragma once

#include <freertos/FreeRTOS.h>
#include <Arduino.h>
#include "LoRa.h"     // use local copy
#include "Reader.h"

void LoraReaderPollOnReceive(int packetSize, void* LoraReaderPollPtr);

class Blob;

class LoraReaderPoll : public Reader {

  public:
    LoraReaderPoll(Blob* blob, int packetsQueueLength = 10);
    ~LoraReaderPoll();
    int read();
    void begin();
    void taskify(int , int );
    const int TimestampRoundingS = 10; // untimestamped Readings will be stamp with now(), rounded to this value
    friend void LoraReaderPollParserTask(void *);
    friend void LoraReaderPollReaderTask(void *);
    friend void LoraReaderPacketAvailableISR();
    void readPacket(int );
  private:
    //void receivePackets(int packetSize);
    void parsePackets();
    QueueHandle_t _packetsQueue;
    //volatile int _packetSize;
    volatile QueueHandle_t _packetsAvailable;
    const int _dio0 = 26; // todo: get from lora or config
};
