#pragma once
#include <Arduino.h>
#include <stdexcept>


class CapButton {
  public:
    CapButton(gpio_num_t pin, int threshold = 40, bool interrupts = false);
    virtual ~CapButton() {};
    virtual void touchCallBack() { }  // called repeatedly whenever pin is being touched. 
    void begin();     
    static void printHooks();
    static bool pollingEnabled;
    
  protected:
    virtual void keypressEvent() = 0;
    virtual void keyupEvent() = 0;
    gpio_num_t _pin;
    
  private:
    
    int _nthInstance=0;
    int _threshold=0;
    bool _lastState=false;
    bool _currentState=false;
    unsigned long _lastDebounceTime=0;  // the last time the output pin was toggled

    void pollTask();

    static const int MAX_INSTANCES = 5;
    static CapButton* instances[MAX_INSTANCES];
    static int nInstance;
    
    static void PollAllTask(void *);
    static int pollPeriodMs;

    static unsigned long debounceDelay;    // the debounce time; increase if the output flickers
    const int priority = 1;
};

class ButtonPrinter : public CapButton {
  public:
    ButtonPrinter(gpio_num_t pin, int threshold = 40, bool interrupts = false);
    void keypressEvent();
    void keyupEvent() ;

};
