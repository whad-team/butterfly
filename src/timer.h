#ifndef TIMER_H
#define TIMER_H
#include "controller.h"

#define TIMER_PRESCALER 4
#define NUMBER_OF_TIMERS 5
typedef bool (Controller::*ControllerCallback) (void);

typedef enum TimerMode {
  SINGLE_SHOT = 1,
  REPEATED = 2
} TimerMode;

class Timer {
  private:
    int id;
    int duration;
    int lastTimestamp;
    TimerMode mode;
    Controller *controller;
    ControllerCallback callback;
    bool started;
    bool used;

  public:
    Timer(int id);

    int getLastTimestamp();
    void setLastTimestamp(int lastTimestamp);
    void setMode(TimerMode mode);
    void setCallback(ControllerCallback callback, Controller* controller);
    void update(int duration, int timestamp);
    void update(int duration);
    void start();
    void stop();
    bool isStarted();
    bool isUsed();
    ControllerCallback getCallback();
    Controller* getController();
    TimerMode getMode();
    int getDuration();
    void setUsed(bool used);
    void release();
};

class HardwareControlledTimer {
  private:
    int duration;
    Controller *controller;
    ControllerCallback callback;
    bool used;

  public:
    HardwareControlledTimer();

    void enable(volatile void* event);
    void disable();

    bool isUsed();
    ControllerCallback getCallback();
    Controller* getController();
    int getDuration();

    void setCallback(ControllerCallback callback, Controller* controller);
    void setDuration(int duration);

    void setUsed(bool used);
    void release();
};

class TimerModule {
	public:
    Timer* timers[5];
    HardwareControlledTimer* hardwareTimer;

		static TimerModule *instance;

    TimerModule();
    uint32_t getTimestamp();
    Timer* getTimer();
    HardwareControlledTimer* getHardwareControlledTimer();
    void releaseTimer(Timer* timer);
};
#endif
