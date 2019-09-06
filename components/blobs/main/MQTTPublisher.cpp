#include "MQTTPublisher.h"
//#include "OLEDUI.h"
#include "Reading.h"
#include <Arduino.h>

MQTTPublisher::MQTTPublisher(Blob* blob, WiFiClient wifiClient, String mqttServer, int mqttPort)
  : Publisher(blob),
    //_mqttClient(wifiClient),
    _mqttServer(mqttServer),
    _mqttPort(mqttPort)
{
  _mqttClient.setServer(_mqttServer.c_str(), _mqttPort);
}

bool MQTTPublisher::publishReading(Reading *r) {
  
  reconnect();
  return  (_mqttClient.publish(r->mqttTopic().c_str(), r->mqttValue().c_str()));
    }

/*
   Todo fail after 3 tries?
*/
void MQTTPublisher::reconnect() {
  // Loop until we're reconnected
  while (!_mqttClient.connected()) {
    Serial.print("Attempting MQTT connection to " );
    Serial.print(_mqttServer); Serial.print(":"); Serial.print(_mqttPort); Serial.print("...");
    // Create a random client ID
    // \todo Name the blob
    String clientId = "ESP32Client-";
    clientId += String(random(0xffff), HEX);
    // Attempt to connect
    if (_mqttClient.connect(clientId.c_str())) {
      Serial.println("connected");
      // Once connected, publish an announcement...
      //client.publish("outTopic", "hello world");
      // ... and resubscribe
      //client.subscribe("#");
    } else {
      Serial.print("failed, rc=");
      Serial.print(_mqttClient.state());
      Serial.println(" try again in 5 seconds");
      // Wait before retrying
      delay(5000);
    }
  }
}

MQTTPublisher::~MQTTPublisher() {
  Publisher::end();
}