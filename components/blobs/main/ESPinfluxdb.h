/* Influxdb library

   MIT license
   Written by HW Wong
   Adapted by BBeeson
   - need at add batching
   - whitespace tags/fields/etc breaks
   - convert to ESP-IDF usage
*/

#ifndef INFLUXDB_H
#define INFLUXDB_H
#include "Arduino.h"

#if defined(ESP8266)
#include <ESP8266HTTPClient.h>
#elif defined(ESP32)
#include <HTTPClient.h>
#endif

#include <time.h>
#include <vector>
using namespace std;


enum DB_RESPONSE {DB_SUCCESS, DB_ERROR, DB_CONNECT_FAILED};

// Url encode function
String URLEncode(String msg);

class dbMeasurement
{
  public:
    dbMeasurement(String m);

    String measurement;


    void addField(String key, float value);
    void addTag(String key, String value);
    void setTimeStamp(time_t tsUnixEpochSeconds);
    void empty();
    String postString();

  private:
    String _data;
    String _tag;
    String _timestamp;
};

class Influxdb
{
  public:
    Influxdb(const char* host, uint16_t port);

    DB_RESPONSE opendb(String db);
    DB_RESPONSE opendb(String db, String user, String password);
    DB_RESPONSE query(String sql);
    //uint8_t createDatabase(char *dbname);
    DB_RESPONSE response();
    String _precision = "ns"; //  [ns*,u,ms,s,m,h]
    void addMeasurement(dbMeasurement data);
    DB_RESPONSE write(); // all measurements will be rewritten and cleared
    
    
  private:
    DB_RESPONSE write(dbMeasurement data);
    DB_RESPONSE write(String data);
    vector<dbMeasurement> measurements;
    String _port;
    String _host;
    String _db;


    DB_RESPONSE _response = DB_ERROR;

};




#endif
