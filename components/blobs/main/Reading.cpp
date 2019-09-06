#include "Reading.h"
#include "BBEsp32Lib.h"
#include <Arduino.h>
#include <ArduinoJson.h>

#undef LOG_LOCAL_LEVEL
#define LOG_LOCAL_LEVEL ESP_LOG_VERBOSE
static const char *TAG = "Reading";
#include "esp_log.h"

#define SECONDS_TO_2000 8760 * 3600 * 30

Reading::Reading(const Reading &r) {
  value = r.value;
  units = r.units;
  location = r.location;
  metric = r.metric;
  id = r.id;
  timestamp = r.timestamp;
}

Reading::Reading(const measurement_t *m, const sample_t s) {

  value = s.v;
  timestamp = s.t;
  units = m->units;
  location = m->location;
  metric = m->metric;
  id = m->id;
}

Reading::Reading() {}

float Reading::getValue() { return value; }

void Reading::setValue(float v) {

  ESP_LOGV(TAG, "setValue: timestamp = %s", bbesp32lib::timestamp().c_str());
  if (bbesp32lib::timeIsValid()) {
    time_t now;
    time(&now);
    timestamp = now;
  } else {
    ESP_LOGV(TAG, "setValue: no time set");
    timestamp = 0;
  }
  value = v;
}

void Reading::setValue(float v, time_t t) {

  timestamp = t;
  value = v;
}

/**
 * todo// breaks on silly values for value
  // consider a char* or clip values
 */
String Reading::tostr() {

  return String("Rdng::") + metric + "," + String(value, 2) + "," + id + "," +
         location + "," + units + "," + timestamp;
}

String Reading::toJSON() {
  String jsonStr;
  const size_t capacity = JSON_OBJECT_SIZE(Reading::elements) + 30;
  DynamicJsonDocument doc(capacity);

  doc["metric"] = metric;
  doc["value"] = value;
  doc["id"] = id;
  doc["location"] = location;
  doc["units"] = units;
  doc["timestamp"] = timestamp;
  serializeJson(doc, jsonStr);
  return jsonStr;
}

String Reading::shortStr() {
  // \todo Cache and make char[50] - called from OLED at 20Hz.
  String m;
  m = metric.substring(0, 5);
  m += "(";
  m += id.substring(0, 3);
  m += ")";
  m += "=";
  m += valueStr(1, -1000000, 1000000);
  return m;
}

String Reading::valueStr(int decimals, float minimum, float maximum) {
  float v = this->value;
  if (!isnan(minimum))
    v = max(minimum, v);
  if (!isnan(maximum))
    v = min(maximum, v);
  return String(v, decimals);
}

Reading *Reading ::fromJSON(String json) {

  Reading *r = NULL;
  const size_t capacity = JSON_OBJECT_SIZE(Reading::elements) + 100;
  DynamicJsonDocument jsonDoc(capacity); // Static segfaults.

  DeserializationError error = deserializeJson(jsonDoc, json);
  if (error) {
    ESP_LOGW(TAG, "Failed to parse (%s): %s", error.c_str(), json.c_str());
  } else {
    r = new Reading();
    r->value = jsonDoc["value"];
    r->id = jsonDoc["id"].as<String>(); // \todo Is this the best way, with
                                        // String? Or char* cpy?
    r->location = jsonDoc["location"].as<String>();
    r->metric = jsonDoc["metric"].as<String>();
    r->units = jsonDoc["units"].as<String>();
    r->timestamp = jsonDoc["timestamp"];
  }
  return r;
}

Reading *Reading ::fromJSONBytes(uint8_t* jsonBytes, int len) {
  assert(0); // check this function... looks dodgy
  char *s;
  String str;
  s = (char *)malloc(len); // sizeof(uint8_t) =assumed= 1
  memcpy((void *)s, (void *) &jsonBytes, len);
  str = s;
  free(s);
  s = 0;
  return fromJSON(String(s));
}

uint8_t *Reading::toJSONBytes(int *len) {
  String r;
  char *s;
  r = this->toJSON();

  s = (char *)malloc(r.length()); // sizeof(uint8_t) =assumed= 1
  *len = r.length();
  strncpy(s, r.c_str(), *len);

  return (uint8_t *)s;
}

Reading::~Reading() {
  ESP_LOGD(TAG, "Destructing");
}


String Reading::mqttTopic() { return location + "/" + metric; }
String Reading::mqttValue() { return String(value); }
void Reading::clear() {
  value = NAN;
  // label = "";
  // units = "";
  // location = "";
}
