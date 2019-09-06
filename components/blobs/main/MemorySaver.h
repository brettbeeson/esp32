#pragma once

#include "Reading.h"
#include "Publisher.h"


#define N_MEASUREMENTS CONFIG_MEMORYSAVER_MEASUREMENTS
#define N_SAMPLES CONFIG_MEMORYSAVER_SAMPLES

namespace rtcmem {
  RTC_DATA_ATTR extern measurement_t measurements[N_MEASUREMENTS];
}

/**
 * Takes reading and published (writes) to RTC memory
 * Use MemoryReader to retrieve after a nice deepsleep
 */
class MemorySaver: public Publisher {

  public:

    MemorySaver(Blob *blob);
    ~MemorySaver();
    void begin();
    bool publishReading(Reading *r);
    static void print();
    static void init();
    static int nMeasurements();
    static int nTotalSamples();

  private:

    measurement_t* findMeasurement(String id);
    measurement_t* addMeasurement(String id);
    
};
