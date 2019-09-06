#pragma once

#include <freertos/FreeRTOS.h>
#include <AsyncTCP.h>
#include <ESPAsyncWebServer.h>
#include <FS.h>

class TimeLapseCamera;

class TimeLapseWebServer : public AsyncWebServer {

  public:
    TimeLapseWebServer(int port,TimeLapseCamera& tlc);
    TimeLapseWebServer(int port);
    void begin(); // override : todo "does NOT override!"
    bool sendLogFile(AsyncWebServerRequest * request,int32_t relPos);
  private:

    TimeLapseCamera& _tlc;
};
