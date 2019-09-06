#include "LoraReader.h"
#include "Reading.h"
#include "Blob.h"
#include <Arduino.h>
#include <stdexcept>
#include "esp_log.h"

static const char *TAG = "LoraReader";
// I think this is okay here - allows task to wait on queue forever
#define INCLUDE_vTaskSuspend                    1

#define MAX_PKT_LENGTH 511

LoraReader* LoraReaderSingleton = NULL;

/*
   This is an ISR, so do minimal work: just flag it

*/
void LoraReaderPacketsAvailableISR(int packetSize) {
  LoraReaderSingleton->receivePackets(packetSize);
}

/*
   Stub to run non-returning parsePackets()
*/
void LoraReaderParserTask(void *_lr) {
  LoraReader *lr = (LoraReader *) _lr;
  lr->parsePackets(); // will block on queue and never return
  assert(0);
}

/**
   LoraReader
*/
LoraReader::LoraReader(Blob* blob, int packetsQueueLength)
  : Reader(blob),
    _packetsQueue(NULL)    {
  // brutal singleton pattern
  assert(!LoraReaderSingleton);
  LoraReaderSingleton = this;
  
  _packetsQueue = xQueueCreate(packetsQueueLength, sizeof(String *));
}

LoraReader::~LoraReader() {
  Reader::end();
  vTaskDelete(_parsePacketsTask);
  _parsePacketsTask=NULL;
}


// todo check priorities
void LoraReader::taskify(int parsePeriodMs /* ignored*/, int priority)  {
  LoRa.onReceive(LoraReaderPacketsAvailableISR);
  // Blocked on packets-to-parse queue. Can be lower priority?
  ESP_LOGI(TAG,"NOT PINNING: LoraReaderParserTask");
  xTaskCreate(LoraReaderParserTask, "LoraReaderParserTask", 10000 /*stack*/, this, 1 , &_parsePacketsTask );
  LoRa.receive();
}

void LoraReader::begin() {
  // Check Lora is available?
}

int LoraReader::read() {
  throw std::runtime_error("Use taskify() instead of LoraReader::read()");
  return 0;
}

/**
   Get bytes from Lora and make strings to parse later
*/
void LoraReader::receivePackets(int packetSize) {
  char packet[MAX_PKT_LENGTH + 1];
  float rssi;
  String *p = NULL;
  BaseType_t xStatus;

  if (packetSize > MAX_PKT_LENGTH) {
    ESP_LOGW(TAG,"Ignoring packet with size=%d max=%d", packetSize, MAX_PKT_LENGTH);
    return;
  }
  // Bad for memory use with a string, apparently "Why Arduino string is evil"
  memset(packet, 0, MAX_PKT_LENGTH + 1); // pad with nulls. allow extra one for String() constructor
  for (int i = 0; i < packetSize; i++) {
    packet[i] = char(LoRa.read());    // convert bytes to ascii chars
  }
  //ESP_LOGV(TAG,"Packet:%s",packet);
  // todo: make a char * instead of String
  p = new String(packet);
  rssi = LoRa.packetRssi();
  xStatus = xQueueSendToBack(_packetsQueue, &p, 0);   // dequeuer will delete
  if (xStatus != pdPASS) {
    ESP_LOGE(TAG,"Could not send LoRa packet to queue. QueueFull=%d", xStatus == errQUEUE_FULL);
    //OLEDUI.message("LoRa reading lost");
    if (p) delete p; p = 0;
  }

}
/**
   Read this->packetsQueue --> parse --> add to this->readings queue
   Never returns - block on while/portMAX_DELAY
   todo: change to char
*/
void  LoraReader::parsePackets() {
  BaseType_t xReadingQueueStatus;
  Reading* r;
  String *s; // packet. need to delete after dequeue
  while ( xQueueReceive(this->_packetsQueue, &s, portMAX_DELAY) == pdPASS) {
        assert(0);
    // todo: use TimeKeeper

//    TimeTask.syncToNTP();
    r = Reading::fromJSON(*s);
    if (!r) {
      //OLEDUI.message("Invalid Lora reading");
      ESP_LOGE(TAG,"Couldn't parse a Reading from Lora: %s", s->c_str());
    } else {
      // If no timestamp, apply this. It will be later than real reading, but the best we can do.
      if (r->timestamp == 0) {
        assert(0);
        //r->timestamp = TimeTask.getTimeRounded(TimestampRoundingS);
      }
      //OLEDUI.message("LoRa:" + r->metric.substring(0, 5) + "=" + r->getValue());
      // Pointer to reading is copied. User will delete the object.
      xReadingQueueStatus = xQueueSendToBack(_readingsQueue, &r, pdMS_TO_TICKS(1000) /* timeout */);
      if (xReadingQueueStatus != pdPASS) {
        ESP_LOGE(TAG,"Could not send reading from packet to LoRa queue. QueueFull=%d", xReadingQueueStatus == errQUEUE_FULL);
        //OLEDUI.message("LoRa reading lost");
        delete(r); r = NULL;
      }
    }
    if (s) delete s; s = NULL;
  }
}
