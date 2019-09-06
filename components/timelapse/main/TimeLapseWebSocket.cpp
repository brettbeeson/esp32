#include "TimeLapseWebSocket.h"
#include "TimeLapseCamera.h"
#include "BBEsp32Lib.h"
#include <ArduinoJson.h>

#include <stdio.h>

using namespace std;
using namespace bbesp32lib;

TimeLapseWebSocket::TimeLapseWebSocket(const String& url, TimeLapseCamera& tlc, int port):
  AsyncWebSocket(url),
  _tlc(tlc)
{
  this->onEvent(TimeLapseWebSocket::onWebSocketEvent);
}

void merge(JsonObject& dest, const JsonObject& src) {
  for (JsonPair kvp : src) {
    dest[kvp.key()] = kvp.value();
  }
}


void TimeLapseWebSocket::onWebSocketEvent(AsyncWebSocket * serverBase, AsyncWebSocketClient * client, AwsEventType type, void * arg, uint8_t *data, size_t len) {

  TimeLapseWebSocket* server = (TimeLapseWebSocket*) serverBase;

  if (type == WS_EVT_CONNECT) {
	  //printf("ws[%s][%ul] connect.\n", server->url(), client->id());
    client->text("{\"event\":\"message\",\"data\":{\"text\":\"Connected to Espy!\"}}");
    server->_tlc._wiFiUsers++;  // tell others not to disconnect wifi
  } else if (type == WS_EVT_DISCONNECT) {
	  //std::printf("ws[%s][%ul] disconnect\n", server->url(), client->id());
    server->_tlc._wiFiUsers--;
  } else if (type == WS_EVT_ERROR) {
	  //std::printf("ws[%s][%ul] error(%u): %s\n", server->url(), client->id(), *((uint16_t*)arg), (char*)data);
  } else if (type == WS_EVT_DATA) {
    AwsFrameInfo * info = (AwsFrameInfo*)arg;
    String msg = "";
    if (info->final && info->index == 0 && info->len == len) {
      //the whole message is in a single frame and we got all of it's data
      //printf("ws[%s][%u] %s-message[%llu]: ", server->url(), client->id(), (info->opcode == WS_TEXT) ? "text" : "binary", info->len);

      if (info->opcode == WS_TEXT) {
        for (size_t i = 0; i < info->len; i++) {
          msg += (char) data[i];
        }
      } else {
        char buff[3];
        for (size_t i = 0; i < info->len; i++) {
          sprintf(buff, "%02x ", (uint8_t) data[i]);
          msg += buff ;
        }
      }
      //.printf("%s\n", msg.c_str());
      server->handleWebSocketMessage(client, msg);
    } else {
      //message is comprised of multiple frames or the frame is split into multiple packets
      if (info->index == 0) {
        if (info->num == 0) {
          //          .printf("ws[%s][%u] %s-message start\n", server->url(), client->id(), (info->message_opcode == WS_TEXT) ? "text" : "binary");
        }
        //      .printf("ws[%s][%u] frame[%u] start[%llu]\n", server->url(), client->id(), info->num, info->len);
      }
      //  .printf("ws[%s][%u] frame[%u] %s[%llu - %llu]: ", server->url(), client->id(), info->num, (info->message_opcode == WS_TEXT) ? "text" : "binary", info->index, info->index + len);

      if (info->opcode == WS_TEXT) {
        for (size_t i = 0; i < info->len; i++) {
          msg += (char) data[i];
        }
      } else {
        char buff[3];
        for (size_t i = 0; i < info->len; i++) {
          sprintf(buff, "%02x ", (uint8_t) data[i]);
          msg += buff ;
        }
      }
      //.printf("%s\n", msg.c_str());

      if ((info->index + len) == info->len) {
        //.printf("ws[%s][%u] frame[%u] end[%llu]\n", server->url(), client->id(), info->num, info->len);
        if (info->final) {
          //.printf("ws[%s][%u] %s-message end\n", server->url(), client->id(), (info->message_opcode == WS_TEXT) ? "text" : "binary");
          server->handleWebSocketMessage(client, msg);
        }
      }
    }
  }
}


/*
   Message from the client/browser, probably in event/data format
*/
void TimeLapseWebSocket::handleWebSocketMessage(AsyncWebSocketClient * client, String& msg) {
  String errorMessage;

  // Parse the message
  const size_t capacity = JSON_OBJECT_SIZE(2) + JSON_OBJECT_SIZE(10) + 500;
  DynamicJsonDocument msgDoc(capacity);
  DeserializationError msgError = deserializeJson(msgDoc, msg);
  if (msgError) {
	  //printf("deserializeJson() failed: %s", msgError.c_str());
    return;
  }

  // Distribute the message
  // Pretty hacky - could use a bind system / classes
  // but this is the only place it is required
  //
  const char* event = msgDoc["event"]; // "status"
  const JsonObject& msgData = msgDoc["data"];

  //printf("Event received: %s", event);

  if (strcmp(event, "status") == 0) {

    // Return the status. Ignore msgData - status is read-only
    // eg. {"event":"status","data":{"uploadMode":"noupload","sleepfrom":12,"sleepto":6,"photoIntervalS":11}}";
    String json;
    DynamicJsonDocument doc = _tlc.status();
    serializeJson(doc, json);
    client->text(json);

  } else if (strcmp(event, "message") == 0) {
    
    //printf("Message from client: %s", msgData["text"].as<char*>());
    
  } else if (strcmp(event, "reset") == 0) {
    
    //printf("Reseting now.");
    delay(1000);
    ESP.restart();
    
  } else if (strcmp(event, "log") == 0) {
    
    int lines;
    String m;
    lines = msgData["lines"];
    //std::printf("lines = %d",lines);
    if (lines == 0) lines = -100;
    if (lines > 1000) lines = 1000;
    if (lines < -1000) lines = -1000;
    if (lines < 0) {
      m = LogFile.tail(-lines);
    } else { // lines>0
      m = LogFile.head(lines);
    }
    DynamicJsonDocument doc(JSON_OBJECT_SIZE(4) + JSON_OBJECT_SIZE(30));
    doc["event"] = "log";
    const JsonObject& returnMsgData = doc.createNestedObject("data");
    returnMsgData["lines"] = lines;
    returnMsgData["log"] = m.c_str();
    String json;
    serializeJson(doc, json); // todo: stream it?
    client->text(json);
    
  } else if (strcmp(event, "settings") == 0) {
    
    String json;
    if (msgData.size() > 0) {
      _tlc.applyConfig(msgData); // write settings
      _tlc.saveConfigToFile(msgData);
      errorMessage = "Saved settings";
    }
    // read all, to report back as a copy
    // this may be different if settings were invalid
    DynamicJsonDocument data = _tlc.getConfig();
    if (msgData.size() > 0) {
      _tlc.saveConfigToFile(data); // write to config file if settings changed
    }
    // Make a message containing the config
    DynamicJsonDocument doc(JSON_OBJECT_SIZE(2) + JSON_OBJECT_SIZE(25));
    doc["event"] = "settings";
    doc["data"] = data.as<JsonObject>();
    serializeJson(doc, json);
    //.println("Sending:"); serializeJson(doc, stdout);
    client->text(json);
  } else {
    errorMessage = String("Unknown event of type:") + String(event);
  }

  // Send make an error message if necessary
  //debugV("Done message event. MessageBack: %s", errorMessage.c_str());
  if (errorMessage != "" ) {
    String json;
    const size_t capacity = JSON_OBJECT_SIZE(1) + JSON_OBJECT_SIZE(2);
    DynamicJsonDocument doc(capacity);
    doc["event"] = "message";
    const JsonObject& nestedObject = doc.createNestedObject("data");
    nestedObject["text"] = errorMessage.c_str();
    serializeJson(doc, json);
    //    .println(json);
    client->text(json);
  }
}
