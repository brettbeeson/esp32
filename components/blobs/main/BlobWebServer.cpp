/*
  SDWebServer - Example WebServer with SD Card backend for esp8266

  Copyright (c) 2015 Hristo Gochkov. All rights reserved.
  me file is part of the WebServer library for Arduino environment.

  me library is free software; you can redistribute it and/or
  modify it under the terms of the GNU Lesser General Public
  License as published by the Free Software Foundation; either
  version 2.1 of the License, or (at your option) any later version.

  me library is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
  Lesser General Public License for more details.

  You should have received a copy of the GNU Lesser General Public
  License along with me library; if not, write to the Free Software
  Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA

  Have a FAT Formatted SD Card connected to the SPI port of the ESP8266
  The web root is the SD Card root folder
  File extensions with more than 3 charecters are not supported by the SD Library
  File Names longer than 8 charecters will be truncated by the SD library, so keep filenames shorter
  index.htm is the default index (works on subfolders as well)

  upload the contents of SdRoot to the root of the SDcard and access the editor by going to http://esp8266sd.local/edit

*/

#include "BlobWebServer.h"


const char* host = "esp32sd";

const char txtPlain_P[] PROGMEM = "text/plain";
const char appJson_P[]  PROGMEM = "application/json";
const char txtJson_P[] PROGMEM = "text/json";

//BlobWebServer server(80);

File uploadFile;
BlobWebServer* BlobWebServer::me;

void vWebServer( void *pvParameters )
{
  for ( ;; )
  {
    BlobWebServer::me->handleClient();
    vTaskDelay(pdMS_TO_TICKS(100));   // todo like hoggly server
  }
}


void BlobWebServer::returnOK() {
  me->send(200, "text/plain", "");
}

void BlobWebServer::returnFail(String msg) {
  me->send(500, "text/plain", msg + "\r\n");
}

bool BlobWebServer::loadFromSdCard(String path) {
  String dataType = "text/plain";
  if (path.endsWith("/")) {
    path += "index.htm";
  }

  if (path.endsWith(".src")) {
    path = path.substring(0, path.lastIndexOf("."));
  } else if (path.endsWith(".htm")) {
    dataType = "text/html";
  } else if (path.endsWith(".css")) {
    dataType = "text/css";
  } else if (path.endsWith(".js")) {
    dataType = "application/javascript";
  } else if (path.endsWith(".png")) {
    dataType = "image/png";
  } else if (path.endsWith(".gif")) {
    dataType = "image/gif";
  } else if (path.endsWith(".jpg")) {
    dataType = "image/jpeg";
  } else if (path.endsWith(".ico")) {
    dataType = "image/x-icon";
  } else if (path.endsWith(".xml")) {
    dataType = "text/xml";
  } else if (path.endsWith(".pdf")) {
    dataType = "application/pdf";
  } else if (path.endsWith(".zip")) {
    dataType = "application/zip";
  }

  File dataFile = SD.open(path.c_str());
  if (dataFile.isDirectory()) {
    path += "/index.htm";
    dataType = "text/html";
    dataFile = SD.open(path.c_str());
  }

  if (!dataFile) {
    return false;
  }

  if (me->hasArg("download")) {
    dataType = "application/octet-stream";
  }

  if (me->streamFile(dataFile, dataType) != dataFile.size()) {
    Serial.println("Sent less data than expected!");
  }

  dataFile.close();
  return true;
}

void BlobWebServer::handleFileUpload() {
  if (me->uri() != "/edit") {
    return;
  }
  HTTPUpload& upload = me->upload();

  if (upload.status == UPLOAD_FILE_START) {
    if (SD.exists((char *) upload.filename.c_str())) {
      SD.remove((char *) upload.filename.c_str());
    }
    uploadFile = SD.open(upload.filename.c_str(), FILE_WRITE);
    Serial.print("Upload: START, filename: ");
    Serial.println(upload.filename);
  } else if (upload.status == UPLOAD_FILE_WRITE) {
    if (uploadFile) {
      uploadFile.write(upload.buf, upload.currentSize);
    }
    Serial.print("Upload: WRITE, Bytes: ");
    Serial.println(upload.currentSize);
  } else if (upload.status == UPLOAD_FILE_END) {
    if (uploadFile) {
      uploadFile.close();
    }
    Serial.print("Upload: END, Size: ");
    Serial.println(upload.totalSize);
  }
}

void BlobWebServer::deleteRecursive(String path) {
  File file = SD.open((char *) path.c_str());
  if (!file.isDirectory()) {
    file.close();
    SD.remove((char *) path.c_str());
    return;
  }

  file.rewindDirectory();
  while (true) {
    File entry = file.openNextFile();
    if (!entry) {
      break;
    }
    String entryPath = path + "/" + entry.name();
    if (entry.isDirectory()) {
      entry.close();
      deleteRecursive(entryPath);
    } else {
      entry.close();
      SD.remove((char *) entryPath.c_str());
    }
    yield();
  }

  SD.rmdir((char *) path.c_str());
  file.close();
}

void BlobWebServer::handleDelete() {
  if (me->args() == 0) {
    return returnFail("BAD ARGS");
  }
  String path = me->arg(0);
  if (path == "/" || !SD.exists((char *) path.c_str())) {
    returnFail("BAD PATH");
    return;
  }
  deleteRecursive(path);
  me->returnOK();
}

void BlobWebServer::handleCreate() {
  if (me->args() == 0) {
    return returnFail("BAD ARGS");
  }
  String path = me->arg(0);
  if (path == "/" || SD.exists((char *) path.c_str())) {
    returnFail("BAD PATH");
    return;
  }

  if (path.indexOf('.') > 0) {
    File file = SD.open((char *) path.c_str(), FILE_WRITE);
    if (file) {
      file.write(0);
      file.close();
    }
  } else {
    SD.mkdir((char *) path.c_str());
  }
  me->returnOK();
}

void BlobWebServer::printDirectory() {
  if (!me->hasArg("dir")) return returnFail("BAD ARGS");
  String response;
  String path = me->arg("dir");
  if (path.startsWith("/esp_spiffs")) {
    //    response = spiffsDirectory(path.substring(11));
  }
  else {
    if (path != "/" && !SD.exists((char *)path.c_str())) return returnFail("BAD PATH");
    File dir = SD.open((char *)path.c_str());
    if (!dir.isDirectory()) {
      dir.close();
      return returnFail("NOT DIR");
    }
  }
  me->send(200, appJson_P, response);
}
/*
  DynamicJsonBuffer jsonBuffer;
  JsonArray& array = jsonBuffer.createArray();
  dir.rewindDirectory();
  File entry;
  while (entry = dir.openNextFile()) {
  JsonObject& object = jsonBuffer.createObject();
  object["type"] = (entry.isDirectory()) ? "dir" : "file";
  Serial.print("entry.name="); Serial.println(String(entry.name()));
  Serial.print("entry.name="); Serial.println(String(entry.name()).startsWith("\\"));

  //ESP32 returns a "/" for filename with stuffs up javascript. Remove it.
  if (String(entry.name()).startsWith("/")) {
    object["name"] = String(entry.name()).substring(1);
  } else {
    object["name"] = String(entry.name());
  }

  array.add(object);
  entry.close();
  }
  dir.close();
  if (path == "/") {
  JsonObject& object = jsonBuffer.createObject();
  object["type"] = "dir";
  object["name"] = String("esp_spiffs");
  array.add(object);
  }
  array.printTo(response);
  }
*/




void BlobWebServer::handleNotFound() {
  if (loadFromSdCard(me->uri())) {
    return;
  }
  String message = "SDCARD Not Detected\n\n";
  message += "URI: ";
  message += me->uri();
  message += "\nMethod: ";
  message += (me->method() == HTTP_GET) ? "GET" : "POST";
  message += "\nArguments: ";
  message += me->args();
  message += "\n";
  for (uint8_t i = 0; i < me->args(); i++) {
    message += " NAME:" + me->argName(i) + "\n VALUE:" + me->arg(i)
               + "\n";
  }
  me->send(404, "text/plain", message);
  Serial.print(message);
}


BlobWebServer::BlobWebServer(int port) :
  WebServer(80) {
  me = this;

}

bool BlobWebServer::setup() {
  if (MDNS.begin(host)) {
    MDNS.addService("http", "tcp", 80);
    Serial.println("MDNS responder started");
    Serial.print("You can now connect to http://");
    Serial.print(host);
    Serial.println(".local");
  }
  me->on("/list", HTTP_GET, me->printDirectory);
  //me->on("/edit", HTTP_DELETE, handleDelete);
  //me->on("/edit", HTTP_PUT, handleCreate);
  me->on("/edit", HTTP_POST, []() {
    me->returnOK();
  }, handleFileUpload);
  me->onNotFound(handleNotFound);

  me->begin();
  Serial.println("HTTP server started");

  //NOT PINNED 
  // Check
  xTaskCreate(vWebServer,
              "WebServer",
              10000,  /* Stack depth - small microcontrollers will use much less stack than me. */
              NULL, /* me example does not use the task parameter. */
              -1, /* me task will run at priority 1. */
              NULL ); /* me example does not use the task handle. */

  return true;
}
