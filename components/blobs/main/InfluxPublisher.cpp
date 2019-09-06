#include "Blob.h"
//#include "OLEDUI.h"
#include "BBEsp32Lib.h"
#include "InfluxPublisher.h"
#include "Reading.h"
#include "esp_log.h"
#include <Arduino.h>
#include <WiFi.h>

#undef LOG_LOCAL_LEVEL
#define LOG_LOCAL_LEVEL ESP_LOG_VERBOSE
#include "esp_log.h"
static const char *TAG ="InfluxPublisher";

InfluxPublisher::InfluxPublisher(Blob *blob, String _server, int _port,
                                 String _db, String _measurement, String _user,
                                 String _password)
    : Publisher(blob), db(_db), influxdb(_server.c_str(), _port),
      measurement(_measurement), password(_password), user(_user)

{
  ESP_LOGV(TAG, "Influx::Influx:: %s,%d,%s,%s,%s", _server.c_str(), _port,
           db.c_str(), user.c_str(), password.c_str());
  influxdb._precision = "s";
}

void InfluxPublisher::begin() {
  // if (WiFi.status() != WL_CONNECTED) throw
  // std::runtime_error("InfluxPublisher::begin(): no Wifi");

  influxdb.opendb(db, user, password); // just stores data, no connection test.
  // test and throw if required
}

/*
  Override to empty the queue of Readings and publish *in a batch*
  Block until items available in the queue, then get them all, publish and return
*/
int InfluxPublisher::publishReadings() {
  BaseType_t xStatus;
  Reading *r = NULL;
  int retval;
  int nPublished = 0;
  
/*
  if (block) {
    // When the queue is non-zero, start collecting the batch
    xStatus = xQueuePeek(_readingsQueue, &r, portMAX_DELAY);
    // when blocking, expect to be here only when an item is available
    assert(xStatus == pdPASS);
  }
  */

  // Collect all readings currently available
  xStatus = pdPASS;
  while (xStatus == pdPASS) {
    // block a short period to collect batch
    xStatus = xQueueReceive(_readingsQueue, &r, pdMS_TO_TICKS(100));
    if (xStatus == pdPASS) {
      assert(r->id != "");
      assert(r->location != "");
      assert(r->metric != "");
      assert(r->units != "");
      
      ESP_LOGV(TAG,"Influxing: %s", r->tostr().c_str());
      dbMeasurement row(this->measurement);
      row.addTag("location", r->location);
      row.addTag("id", r->id);
      String f = r->metric + r->units;
      assert(!std::isnan(r->getValue()));
      row.addField(f, r->getValue());
      if (r->timestamp) {
        row.setTimeStamp(r->timestamp); // seconds
      }
      influxdb.addMeasurement(row);     // copys
      nPublished++;
      if (r)
        delete r;
      r = NULL;
    }
  }
  // Publish all available readings
  retval = influxdb.write();
  if (retval != DB_SUCCESS) {
    // readings are lost!
    String m = String("Influx failed:") + String(retval);
    throw std::runtime_error(m.c_str());
  } else {
    bbesp32lib::Blink.flash();
    ESP_LOGI(TAG, "Influxed as batch: %d", nPublished);
  }
  
  return nPublished;
}

bool InfluxPublisher::publishReading(Reading *r) { assert(0); }

InfluxPublisher::~InfluxPublisher() { Publisher::end(); }