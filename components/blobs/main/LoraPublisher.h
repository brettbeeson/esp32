/*
   LoraPublisher.h

    Created on: 8 Jan 2019
        Author: bbeeson
*/

#ifndef LoraPublisher_H_
#define LoraPublisher_H_

#include "Blob.h"
#include "Publisher.h"

void LoraPublisherTask(void*);

class LoraPublisher : public Publisher {
  public:
    LoraPublisher(Blob *blob);
    ~LoraPublisher();
    bool publishReading(Reading *r);
       

    void begin();
  private:

};

#endif
