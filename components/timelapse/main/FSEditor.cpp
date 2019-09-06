#include "FSEditor.h"
#include "BBEsp32Lib.h"
#include <SD_MMC.h>
#include <ArduinoJson.h>
#include <FS.h>

#define MAXLENGTH_FILEPATH 128

using namespace bbesp32lib;

const char *excludeListFileFS = "/.exclude.files";

typedef struct ExcludeListS {
  char *item;
  ExcludeListS *next;
} ExcludeList;

static ExcludeList *excludes = NULL;

static bool matchWild(const char *pattern, const char *testee) {
  const char *nxPat = NULL, *nxTst = NULL;

  while (*testee) {
    if (( *pattern == '?' ) || (*pattern == *testee)) {
      pattern++; testee++;
      continue;
    }
    if (*pattern == '*') {
      nxPat = pattern++; nxTst = testee;
      continue;
    }
    if (nxPat) {
      pattern = nxPat + 1; testee = ++nxTst;
      continue;
    }
    return false;
  }
  while (*pattern == '*') {
    pattern++;
  }
  return (*pattern == 0);
}

static bool addExclude(const char *item) {
  size_t len = strlen(item);
  if (!len) {
    return false;
  }
  ExcludeList *e = (ExcludeList *)malloc(sizeof(ExcludeList));
  if (!e) {
    return false;
  }
  e->item = (char *)malloc(len + 1);
  if (!e->item) {
    free(e);
    return false;
  }
  memcpy(e->item, item, len + 1);
  e->next = excludes;
  excludes = e;
  return true;
}

static void loadExcludeList(fs::FS &_fs, const char *filename) {
  static char linebuf[MAXLENGTH_FILEPATH];
  fs::File excludeFile = _fs.open(filename, "r");
  if (!excludeFile) {
    //addExclude("/*.js.gz");
    return;
  }
#ifdef ESP32
  if (excludeFile.isDirectory()) {
    excludeFile.close();
    return;
  }
#endif
  if (excludeFile.size() > 0) {
    uint8_t idx;
    bool isOverflowed = false;
    while (excludeFile.available()) {
      linebuf[0] = '\0';
      idx = 0;
      int lastChar;
      do {
        lastChar = excludeFile.read();
        if (lastChar != '\r') {
          linebuf[idx++] = (char) lastChar;
        }
      } while ((lastChar >= 0) && (lastChar != '\n') && (idx < MAXLENGTH_FILEPATH));

      if (isOverflowed) {
        isOverflowed = (lastChar != '\n');
        continue;
      }
      isOverflowed = (idx >= MAXLENGTH_FILEPATH);
      linebuf[idx - 1] = '\0';
      if (!addExclude(linebuf)) {
        excludeFile.close();
        return;
      }
    }
  }
  excludeFile.close();
}

/*
static bool isExcluded(fs::FS &_fs, const char *filename) {
  if (excludes == NULL) {
    loadExcludeList(_fs, excludeListFileFS);
  }
  ExcludeList *e = excludes;
  while (e) {
    if (matchWild(e->item, filename)) {
      return true;
    }
    e = e->next;
  }
  return false;
}
*/
// WEB HANDLER IMPLEMENTATION

#ifdef ESP32
FSEditor::FSEditor(const fs::FS& fs, const String& username, const String& password)
#else
FSEditor::FSEditor(const String& username, const String& password, const fs::FS& fs)
#endif
  : _fs(fs),
   _password(password),
  _username(username)
{}

bool FSEditor::canHandle(AsyncWebServerRequest *request) {
  if (request->url().equalsIgnoreCase("/edit")) {
    if (request->method() == HTTP_GET) {
      if (request->hasParam("list"))
        return true;
      if (request->hasParam("edit")) {
        request->_tempFile = _fs.open(request->arg("edit"), "r");
        if (!request->_tempFile) {
          return false;
        }
#ifdef ESP32
        if (request->_tempFile.isDirectory()) {
          request->_tempFile.close();
          return false;
        }
#endif
      }
      if (request->hasParam("download")) {
        request->_tempFile = _fs.open(request->arg("download"), "r");
        if (!request->_tempFile) {
          return false;
        }
#ifdef ESP32
        if (request->_tempFile.isDirectory()) {
          request->_tempFile.close();
          return false;
        }
#endif
      }
      request->addInterestingHeader("If-Modified-Since");
      return true;
    }
    else if (request->method() == HTTP_POST)
      return true;
    else if (request->method() == HTTP_DELETE)
      return true;
    else if (request->method() == HTTP_PUT)
      return true;

  }
  return false;
}

void FSEditor::handleRequest(AsyncWebServerRequest *request) {
  if (_username.length() && _password.length() && !request->authenticate(_username.c_str(), _password.c_str()))
    return request->requestAuthentication();

  if (request->method() == HTTP_GET) {
    if (request->hasParam("list")) {
      String path = request->getParam("list")->value();

      if (!_fs.exists((char *)path.c_str())) {
        Serial.println("BAD PATH");
        return ;
      }
      File dir = _fs.open((char *)path.c_str());
      if (!dir.isDirectory()) {
        dir.close();
        Serial.println("BAD PATH");
        return ;
      }
      // Need to stream!
      AsyncResponseStream *response = request->beginResponseStream("application/json");
      DynamicJsonDocument doc(JSON_OBJECT_SIZE(2) + JSON_OBJECT_SIZE(256) * 2); //max length 256
      JsonObject obj = doc.to<JsonObject>();
      char jsonStr[512 + 128];
      dir.rewindDirectory();
      File entry;
      bool first = true;
      response->print("[");
      while (entry = dir.openNextFile()) {
        //Serial.print(entry.name()); Serial.println("<-- entry");
        obj["type"] = (entry.isDirectory()) ? "dir" : "file";
        // return just the filename (no slashes, no folders)
        // edit.htm wants this format
        obj["name"] = slash("", getFilename(entry.name()), "");

        serializeJson(obj, jsonStr);
        if (!first) response->print(",");
        first = false;
        response->print(jsonStr);

        entry.close();
      }
      response->print("]");

      dir.close();
      /*
        if(path == "/"){
        object = array.createNestedObject();
        object["type"] = "dir";
        object["name"] = String("/");
        array.add(object);
        }
      */
      //String response;
      //serializeJson(jsonBuffer,response);
      //array.printTo(response);
      //Serial.println(response);
      request->send(response);
    } // end list

    else if (request->hasParam("edit") || request->hasParam("download")) {
      request->send(request->_tempFile, request->_tempFile.name(), String(), request->hasParam("download"));
    }
    else {
      const char * buildTime = __DATE__ " " __TIME__ " GMT";
      if (request->header("If-Modified-Since").equals(buildTime)) {
        request->send(304);
      } else {
        //request->redirect("/edit.htm");
        // use the SD card's file
        request->send(_fs, "/edit.htm", "text/html");

        /*

          AsyncWebServerResponse *response = request->beginResponse_P(200, "text/html", edit_htm_gz, edit_htm_gz_len);
          response->addHeader("Content-Encoding", "gzip");
          response->addHeader("Last-Modified", buildTime);
          request->send(response);
        */
      }
    }
  } else if (request->method() == HTTP_DELETE) {
    if (request->hasParam("path", true)) {
      _fs.remove(request->getParam("path", true)->value());
      request->send(200, "", "DELETE: " + request->getParam("path", true)->value());
    } else
      request->send(404);
  } else if (request->method() == HTTP_POST) {
    if (request->hasParam("data", true, true) && _fs.exists(request->getParam("data", true, true)->value()))
      request->send(200, "", "UPLOADED: " + request->getParam("data", true, true)->value());
    else
      request->send(500);
  } else if (request->method() == HTTP_PUT) {

    if (request->hasParam("path", true)) {
      String filename = request->getParam("path", true)->value();
      // Strip "." from start, which breaks
      if (filename.startsWith(".")) {
        filename = filename.substring(1, filename.length());
      }


      if (filename.endsWith("/")) {
        filename = slash("/", filename, ""); // ensure it's like: /this/format/bro
        // dir
        if (_fs.exists(filename)) {
          request->send(200);
        } else {
          if (_fs.mkdir(filename)) {
            request->send(200, "", "MKDIR: " + filename);
          } else {
            Serial.println( "Couldn't mkdir: " + filename);
            request->send(500, "Couldn't mkdir: " + filename);
          }
        }
      } else {
        // file
        filename = slash("/", filename, ""); // ensure it's like: /this/format/bro
        //Serial.printf("HTTP_PUT: filename:%s",filename.c_str());
        if (_fs.exists(filename)) {
          request->send(200);
        } else {
          fs::File f = _fs.open(filename, "w");
          if (f) {
            f.write((uint8_t)0x00);
            f.close();
            request->send(200, "", "CREATE: " + filename);
          } else {
            request->send(500);
          }
        }
      }
    } else
      request->send(400);
  }
}

void FSEditor::handleUpload(AsyncWebServerRequest *request, const String& filename, size_t index, uint8_t *data, size_t len, bool final) {
  if (!index) {
    if (!_username.length() || request->authenticate(_username.c_str(), _password.c_str())) {
      _authenticated = true;
      request->_tempFile = _fs.open(filename, "w");
      _startTime = millis();
    }
  }
  if (_authenticated && request->_tempFile) {
    if (len) {
      request->_tempFile.write(data, len);
    }
    if (final) {
      request->_tempFile.close();
    }
  }
}
