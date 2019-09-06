#include "LoraReaderPoll.h"
#include "BBEsp32Lib.h"
#include "OLEDUI.h"
#include "Reading.h"
#include "Blob.h"
#include <Arduino.h>

#undef LOG_LOCAL_LEVEL
#define LOG_LOCAL_LEVEL ESP_LOG_VERBOSE
#include "esp_log.h"
static const char *TAG = "LoraReader";

// I think this is okay here - allows task to wait on queue forever
// better to place in specific rtos header
#define INCLUDE_vTaskSuspend                    1

#define MAX_PKT_LENGTH 511

#include "LoRa.h"
// from lora.cpp
#define REG_DIO_MAPPING_1 0x40
#define REG_FIFO_ADDR_PTR        0x0d
#define REG_FIFO_TX_BASE_ADDR    0x0e
#define REG_FIFO_RX_BASE_ADDR    0x0f
#define REG_FIFO_RX_CURRENT_ADDR 0x10
#define REG_IRQ_FLAGS            0x12
#define REG_RX_NB_BYTES 0x13
// IRQ masks

#define IRQ_TX_DONE_MASK           0x08
#define IRQ_PAYLOAD_CRC_ERROR_MASK 0x20
#define IRQ_RX_DONE_MASK 0x40
#define REG_PAYLOAD_LENGTH 0x22
LoraReaderPoll *LoraReaderPollSingleton = NULL;

void LoraReaderPacketAvailableISR() {
  static const int flag = 1; // bit weird. just need a flag here maybe use packetsize?
  BaseType_t xHigherPriorityTaskWoken = pdFALSE;

  if (xQueueOverwriteFromISR(LoraReaderPollSingleton->_packetsAvailable, &flag, &xHigherPriorityTaskWoken) == pdFALSE) {
    assert(0); // cannot print in an ISR! maybe log an error?
  }
  // See FreeRTOS example
  if ( xHigherPriorityTaskWoken == pdTRUE )                portYIELD_FROM_ISR();
}

/*
   Stub to run non-returning parsePackets()
*/
void LoraReaderPollParserTask(void *_lr) {
  LoraReaderPoll *lr = (LoraReaderPoll *) _lr;
  lr->parsePackets(); // will block on queue and never return
  assert(0);
}


void LoraReaderPollReaderTask(void *_l) {

  LoraReaderPoll* l = (LoraReaderPoll *) _l;
  int flag = 0;

  while ( xQueueReceive(l->_packetsAvailable, &flag, portMAX_DELAY) == pdPASS) {
    ESP_LOGV(TAG,"UnBlocked");
    int irqFlags = LoRa.readRegister(REG_IRQ_FLAGS);

    // clear IRQ's
    LoRa.writeRegister(REG_IRQ_FLAGS, irqFlags);

    if ((irqFlags & IRQ_PAYLOAD_CRC_ERROR_MASK) == 0) {
      // received a packet
      LoRa._packetIndex = 0;

      // read packet length
      int packetLength = LoRa._implicitHeaderMode ? LoRa.readRegister(REG_PAYLOAD_LENGTH) : LoRa.readRegister(REG_RX_NB_BYTES);
      ESP_LOGV(TAG,"packetLength: %d", packetLength);
      // set FIFO address to current RX address
      LoRa.writeRegister(REG_FIFO_ADDR_PTR, LoRa.readRegister(REG_FIFO_RX_CURRENT_ADDR));
      l->readPacket(packetLength);
      LoRa.writeRegister(REG_FIFO_ADDR_PTR, 0);
    }  else {
      ESP_LOGV(TAG,"CRC error");
    }
  }
}

/**
   LoraReaderPoll
*/

LoraReaderPoll::LoraReaderPoll(Blob* blob, int packetsQueueLength)
  : Reader(blob),
    _packetsQueue(NULL)    {
  assert(LoraReaderPollSingleton == NULL);
  LoraReaderPollSingleton = this;
  _packetsQueue = xQueueCreate(packetsQueueLength, sizeof(String *));
  _packetsAvailable = xQueueCreate(1, sizeof(int) /* packet length */);
  assert(_packetsAvailable);
}

LoraReaderPoll::~LoraReaderPoll() {
  assert(0);
  // not implemented
}
// todo check priorities
void LoraReaderPoll::taskify(int readPeriodMs, int priority)  {

  // register a ISR
  ESP_LOGV(TAG,"LoraReaderPoll::taskify");
  pinMode(_dio0, INPUT);
  LoRa.writeRegister(REG_DIO_MAPPING_1, 0x00);
  LoRa._onReceive = NULL;
  attachInterrupt(digitalPinToInterrupt(_dio0), LoraReaderPacketAvailableISR, RISING);

  // Blocked on packets-to-parse queue. Can be lower priority?
  xTaskCreate(LoraReaderPollParserTask, "LoraReaderPollParserTask", 10000 /*stack*/, this, 1 , NULL );
  // Block on semaphore from ISR
  xTaskCreate(LoraReaderPollReaderTask, "LoraReaderPollReaderTask", 10000 /*stack*/, this, 2 , NULL );

  LoRa.receive();
}


void LoraReaderPoll::begin() {
  // Check Lora is available?
}


int LoraReaderPoll::read() {
  assert(0); // use taskify
  return 0;
}
/**
   Get bytes from Lora and make strings to parse later
*/
void LoraReaderPoll::readPacket(int packetSize) {

  char packet[MAX_PKT_LENGTH + 1];
  
  String *p = NULL;
  BaseType_t xStatus;


  // packetSize = LoRa.parsePacket();

  if (packetSize == 0 || packetSize > MAX_PKT_LENGTH) {
    printf("Ignoring packet with size=%d (max=%d)", packetSize, MAX_PKT_LENGTH);
    return;
  }
  // Bad for memory use with a string, apparently "Why Arduino string is evil"
  memset(packet, 0, MAX_PKT_LENGTH + 1); // pad with nulls. allow extra one for String() constructor
  for (int i = 0; i < packetSize; i++) {
    packet[i] = char(LoRa.read());    // convert bytes to ascii chars
  }
  ESP_LOGV(TAG,"Packet:%s", packet);
  // todo: make a char * instead of String
  // todo: statically allocate queue memory with an segment of MAX_PKT_LENGTH x queuelength
  p = new String(packet);
  assert(p);
  //int rssi = LoRa.packetRssi();
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
void  LoraReaderPoll::parsePackets() {
  BaseType_t xReadingQueueStatus;
  Reading* r;
  String *s; // packet. need to delete after dequeue
  while ( xQueueReceive(this->_packetsQueue, &s, portMAX_DELAY) == pdPASS) {
    r = Reading::fromJSON(*s);
    if (!r) {
      OLEDUI.message("Invalid Lora reading");
      ESP_LOGE(TAG,"Couldn't parse a Reading from Lora: %s", s->c_str());
    } else {
      // If no timestamp, apply this. It will be later than real reading, but the best we can do.
      if (r->timestamp == 0) {
        time_t now = time(NULL);
        assert(bbesp32lib::timeIsValid());
        r->timestamp =  bbesp32lib::mround_time_t(now,10);
      }
      //OLEDUI.message("LoRa:" + r->metric.substring(0, 5) + "=" + r->getValue());
      // Pointer to reading is copied. User will delete the object.
      xReadingQueueStatus = xQueueSendToBack(_readingsQueue, &r, pdMS_TO_TICKS(1000) /* timeout */);
      if (xReadingQueueStatus != pdPASS) {
        ESP_LOGE(TAG,"Could not send reading from packet to LoRa queue. QueueFull=%d", xReadingQueueStatus == errQUEUE_FULL);
        OLEDUI.message("LoRa reading lost");
        delete(r); r = NULL;
      }
    }
    if (s) delete s; s = NULL;
  }
}
