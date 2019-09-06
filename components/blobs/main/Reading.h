#pragma once

#include <time.h>
#include <Arduino.h>

#define READING_STR 64

struct sample_t {
  bool set;
  time_t t;
  float v;
} ;
struct measurement_t {
  char id[READING_STR];
  char location[READING_STR];
  char metric[READING_STR];
  char units[READING_STR];
  sample_t *samples;
};

/*
   todo:
*/
  class Reading {
  public:

    Reading(const Reading &r);
    Reading();
    Reading(const measurement_t *m, const sample_t s);
    virtual ~Reading();
    // Set timestamp to now
    void setValue(float v);
    // Specify a timestamp instead of now
    void setValue(float v,time_t t);
    float getValue();

    static const int elements  = 10;

    String metric;          // 2
    String id;              // 3  ED^34324d
    String units;           // 4   C
    String location;        // 5  /home/DSTempSensorsss/
    time_t timestamp=0;     

    String tostr();
    String toJSON();
    uint8_t* toJSONBytes(int *len);
    String mqttTopic();
    String mqttValue();
    //String valueStr(int decimals = 1);
    String valueStr(int decimals = 1, float minimum = NAN, float maximum = NAN);
    String shortStr();

    static Reading* fromJSON(String json);
    static Reading* fromJSON(const char* json);
    static Reading* fromJSONBytes(uint8_t* json, int len);

    void clear();

  private:
    float value=NAN;            // 1

};
