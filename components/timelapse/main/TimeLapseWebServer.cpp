#include "TimeLapseWebServer.h"
#include "FSEditor.h"
#include "SD_MMC.h"
#include "TimeLapseCamera.h"
#include "BBEsp32Lib.h"
#include <WiFiClient.h>

TimeLapseWebServer::TimeLapseWebServer(int port, TimeLapseCamera& tlc):
  AsyncWebServer(port),
  _tlc(tlc)

{
  this->addHandler(new FSEditor(SD_MMC));  // At /edit

  this->on("/log", HTTP_GET, [&](AsyncWebServerRequest * request) {
    int lines = 100;
    if (request->hasParam("lines")) {
      lines = request->getParam("lines")->value().toInt();
    }
    
    if (!this->sendLogFile(request, lines)) {
          request->send(404);
    }
  });

  this->serveStatic("/", tlc._filesys, "/").setDefaultFile("index.htm");

  this->onNotFound([](AsyncWebServerRequest * request) {
    Serial.printf("Sent 404\n");
    request->send(404);
  });
}

void TimeLapseWebServer::begin() {
  DefaultHeaders::Instance().addHeader("Access-Control-Allow-Origin", "*");
  AsyncWebServer::begin();
}

// Not working
bool TimeLapseWebServer::sendLogFile(AsyncWebServerRequest * request,int32_t relPos) {
  File dataFile = _tlc._filesys.open(_tlc.DefaultLogFile);
  if (!dataFile) return false;
  
  int32_t absPos = relPos;
  if (relPos < 0) absPos = dataFile.size() + relPos;
  dataFile.seek(absPos);
  while (dataFile.available()) {
    if (dataFile.read() == '\n') break;
  }
  absPos = dataFile.position();
  request->send(dataFile,"text/plain",dataFile.size() - absPos);
  return true;
}
