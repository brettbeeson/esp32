#pragma once

#include "Blob.h"
#include "Publisher.h"
#include "ESPinfluxdb.h"

class InfluxPublisher : public Publisher {
  
  public:

    InfluxPublisher(Blob *blob,String _influxServer,int _port, String _db, String _measurement,String _user,String _password);   
    ~InfluxPublisher();
    void begin();
    bool publishReading(Reading *r);
    // Override to empty the queue of Readings and publish *in a batch*
    int publishReadings();
    
  private:
        
    String db;
    Influxdb influxdb;
    String measurement;
    String password;
    String user;
    
    
};
