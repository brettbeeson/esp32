#include "Arduino.h"

#include "ESPinfluxdb.h"
#include "esp_http_client.h"

#undef LOG_LOCAL_LEVEL
#define LOG_LOCAL_LEVEL ESP_LOG_VERBOSE
static const char *TAG = "ESPinfluxdb";
#include "esp_log.h"

Influxdb::Influxdb(const char *host, uint16_t port) {
  _port = String(port);
  _host = String(host);
}

DB_RESPONSE Influxdb::opendb(String db, String user, String password) {
  _db = db + "&u=" + user + "&p=" + password;
  return DB_SUCCESS;
}

DB_RESPONSE Influxdb::opendb(String db) {
  assert(0);
  HTTPClient http;
  http.begin("http://" + _host + ":" + _port +
             "/query?q=show%20databases"); // HTTP

  int httpCode = http.GET();

  if (httpCode == 200) {
    _response = DB_SUCCESS;
    String payload = http.getString();
    http.end();

    if (payload.indexOf("db") > 0) {
      _db = db;
      Serial.println(payload);
      return _response;
    }
  } else {
    Serial.println(HTTPClient::errorToString(httpCode));
  }
  _response = DB_ERROR;
  printf("Database open failed");
  return _response;
}

void Influxdb::addMeasurement(dbMeasurement data) {
  measurements.push_back(data);
}

DB_RESPONSE Influxdb::write() {
  String url;
  esp_err_t err;
  String data;
  int tries = 0;

  if (measurements.size() == 0) {
    return DB_SUCCESS; // of sorts
  }
  ESP_LOGV(TAG, "measurements: %d", (int)measurements.size());

  // Prepare URL
  url = "http://" + _host + ":" + _port + "/write?db=" + _db +
        "&precision=" + _precision;
  ESP_LOGV(TAG, "url: %s", url.c_str());
  esp_http_client_config_t config = {};
  config.url = url.c_str();

  // Prepare post string
  for (size_t i = 0; i < measurements.size(); i++) {
    if (i > 0)
      data += "\n";
    data += measurements[i].postString();
  }
  ESP_LOGV(TAG, "data: %s", data.c_str());

  // POST
  esp_http_client_handle_t client = esp_http_client_init(&config);
  ESP_ERROR_CHECK(esp_http_client_set_header(client, "Content-Type", "text/plain"));
  ESP_ERROR_CHECK(esp_http_client_set_method(client, HTTP_METHOD_POST));;
  ESP_ERROR_CHECK(esp_http_client_set_post_field(client,data.c_str(),data.length()));

  while ((err = esp_http_client_perform(client)) !=
         ESP_OK) { // while "connection refused", just keep trying a few times
    if (tries) {
      String m =
          String("Influxdb write failed ") + String(tries) + String(" times");
      Serial.println(m);
      ESP_ERROR_CHECK_WITHOUT_ABORT(err);
      delay(1000 * tries); // back off tex
    }
    tries++;
    if (tries > 3) {
      return DB_CONNECT_FAILED;
    }
  }
  int http_status_code = esp_http_client_get_status_code(client);
  if (http_status_code == 204) {
    _response = DB_SUCCESS;
  } else {
    ESP_LOGW(TAG, "Influx error on sending POST. httpResponseCode=%d",
             http_status_code);
    char buffer[256];
    int bytesRead = esp_http_client_read(client, buffer, 256);
    if (bytesRead < 1) {
      ESP_LOGV(TAG, "No Response: %d ", bytesRead);
    } else {
      ESP_LOGV(TAG, "Response: %s", buffer);
    }
    _response = DB_ERROR;
  }
  measurements.clear();
  esp_http_client_cleanup(client);
  return _response;
}
DB_RESPONSE Influxdb::write(dbMeasurement data) {
  return write(data.postString());
}

DB_RESPONSE Influxdb::write(String data) {
  assert(0);
  HTTPClient http;

  // DEBUG_PRINT("HTTP post begin...");

  http.begin("http://" + _host + ":" + _port + "/write?db=" + _db +
             "&precision=" + _precision); // HTTP
  http.addHeader("Content-Type", "text/plain");

  int httpResponseCode = http.POST(data);

  if (httpResponseCode == 204) {
    _response = DB_SUCCESS;
    String response = http.getString(); // Get the response to the request
  } else {
    Serial.printf("Influx error on sending POST. httpResponseCode=%d\n",
                  httpResponseCode);
    _response = DB_ERROR;
  }

  http.end();
  return _response;
}

DB_RESPONSE Influxdb::query(String sql) {
  assert(0);
  String url = "/query?";
  url += "pretty=true&";
  url += "db=" + _db;
  url += "&q=" + URLEncode(sql);

  HTTPClient http;

  http.begin("http://" + _host + ":" + _port + url); // HTTP

  // start connection and send HTTP header
  int httpCode = http.GET();

  // httpCode will be negative on error
  if (httpCode == 200) {
    // HTTP header has been send and Server response header has been handled
    _response = DB_SUCCESS;
    String reply = http.getString();
    Serial.println(reply);

  } else {
    _response = DB_ERROR;
    //    DEBUG_PRINT("[HTTP] GET... failed, error: " + httpCode);
  }

  http.end();
  return _response;
}

DB_RESPONSE Influxdb::response() { return _response; }

/* -----------------------------------------------*/
//        Field object
/* -----------------------------------------------*/
dbMeasurement::dbMeasurement(String m) { measurement = m; }

void dbMeasurement::empty() {
  _data = "";
  _tag = "";
  _timestamp = "";
}

void dbMeasurement::addTag(String key, String value) {
  _tag += "," + key + "=" + value;
}

void dbMeasurement::addField(String key, float value) {
  _data = (_data == "") ? (" ") : (_data += ",");
  _data += key + "=" + String(value);
}

// only second precisino allowed for
void dbMeasurement::setTimeStamp(time_t tsUnixEpochSeconds) {
  // \todo consider _precision!
  _timestamp = " " + String(tsUnixEpochSeconds);
}

String dbMeasurement::postString() {
  return measurement + _tag + _data + _timestamp;
}

// URL Encode with Arduino String object
String URLEncode(String msg) {
  const char *hex = "0123456789abcdef";
  String encodedMsg = "";

  uint16_t i;
  for (i = 0; i < msg.length(); i++) {
    if (('a' <= msg.charAt(i) && msg.charAt(i) <= 'z') ||
        ('A' <= msg.charAt(i) && msg.charAt(i) <= 'Z') ||
        ('0' <= msg.charAt(i) && msg.charAt(i) <= '9')) {
      encodedMsg += msg.charAt(i);
    } else {
      encodedMsg += '%';
      encodedMsg += hex[msg.charAt(i) >> 4];
      encodedMsg += hex[msg.charAt(i) & 15];
    }
  }
  return encodedMsg;
}
