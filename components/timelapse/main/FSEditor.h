#pragma once

#include <ESPAsyncWebServer.h>

/*
   Need to add folders (SPIFFS, from which this is taken, has none)
*/
class FSEditor: public AsyncWebHandler {
  private:
  
    bool _authenticated = false;
    String _folder = "/";
    fs::FS _fs;
    String _password;
    uint32_t _startTime = 0;
    String _username;

  public:
#ifdef ESP32
    FSEditor(const fs::FS& fs, const String& username = String(), const String& password = String());
#else
    FSEditor(const String& username = String(), const String& password = String(), const fs::FS& fs = SPIFFS);
#endif
    virtual bool canHandle(AsyncWebServerRequest *request) override final;
    virtual void handleRequest(AsyncWebServerRequest *request) override final;
    virtual void handleUpload(AsyncWebServerRequest *request, const String& filename, size_t index, uint8_t *data, size_t len, bool final) override final;
    virtual bool isRequestHandlerTrivial() override final {
      return false;
    }
};
