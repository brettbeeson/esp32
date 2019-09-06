#pragma once

#include <WiFi.h>
#include <WiFiClient.h>
#include <WebServer.h>
#include <ESPmDNS.h>
#include <SPI.h>
#include <SD.h>
#include <ArduinoJson.h>

class BlobWebServer : public WebServer {
  public:
    BlobWebServer (int port);
    static BlobWebServer* me;
    bool setup();
    static void printDirectory();
    static void handleNotFound();
    static void returnFail(String msg);
    static bool loadFromSdCard(String path);
    static void handleFileUpload();
    static void deleteRecursive(String path);
    static void handleDelete();
    static void handleCreate();
    void returnOK();
};
