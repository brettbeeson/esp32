#pragma once 

#include <freertos/FreeRTOS.h>
#include <Arduino.h>

#include <WiFi.h>
#include <PubSubClient.h>
#include "Blob.h"
#include "Publisher.h"

class MQTTPublisher : public Publisher {
  public:
    MQTTPublisher(Blob* blob,WiFiClient wifiClient,String mqttServer, int mqttPort);
    ~MQTTPublisher();
    void begin();
    bool  publishReading(Reading *);
    
  private:
    PubSubClient _mqttClient;
    WiFiClient _wifiClient;
    void reconnect();
    String _mqttServer;
    int _mqttPort;
};
