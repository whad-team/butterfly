#include "timer.h"

TimerModule *TimerModule::instance = NULL;

TimerModule::TimerModule() {
	instance = this;

	// Enable High frequency clock if it is disabled
	if (NRF_CLOCK->EVENTS_HFCLKSTARTED == 0UL) {
		NRF_CLOCK->TASKS_HFCLKSTART = 1UL;
		while (NRF_CLOCK->EVENTS_HFCLKSTARTED == 0UL);
	}
	NRF_TIMER4->MODE = TIMER_MODE_MODE_Timer;
	NRF_TIMER4->BITMODE = TIMER_BITMODE_BITMODE_32Bit;
	NRF_TIMER4->PRESCALER = TIMER_PRESCALER;

	NRF_TIMER4->INTENCLR = TIMER_INTENCLR_COMPARE0_Msk
		| TIMER_INTENCLR_COMPARE1_Msk
		| TIMER_INTENCLR_COMPARE2_Msk
		| TIMER_INTENCLR_COMPARE3_Msk
		| TIMER_INTENCLR_COMPARE4_Msk
		| TIMER_INTENCLR_COMPARE5_Msk;

	NVIC_SetPriority(TIMER4_IRQn, 1);

	NRF_TIMER4->TASKS_CLEAR = 1;
	NRF_TIMER4->TASKS_START = 1;

  for (int i=0;i<NUMBER_OF_TIMERS;i++) {
    this->timers[i] = new Timer(i);
  }
}

uint32_t TimerModule::getTimestamp() {
	NRF_TIMER4->TASKS_CAPTURE[5] = 1UL;
	uint32_t ticks = NRF_TIMER4->CC[5];
	return ticks;
}


Timer* TimerModule::getTimer() {
  for (int i=0;i<NUMBER_OF_TIMERS;i++) {
    if (!this->timers[i]->isUsed()) {
      // We found a free timer
      this->timers[i]->setUsed(true);
      return this->timers[i];
    }
  }
  return NULL;
}

void TimerModule::releaseTimer(Timer* timer) {
    for (int i=0;i<NUMBER_OF_TIMERS;i++) {
      if (timer == this->timers[i]) {
        if (this->timers[i]->isStarted()) {
          this->timers[i]->stop();
        }
        this->timers[i]->setUsed(false);
        break;
      }
    }
}

Timer::Timer(int id) {
  this->id = id;
  this->mode = SINGLE_SHOT;
  this->started = false;
  this->callback = NULL;
  this->controller = NULL;
  this->duration = 1000000;
  this->used = false;
}

void Timer::release() {
  TimerModule::instance->releaseTimer(this);
}
bool Timer::isStarted() {
  return this->started;
}

Controller* Timer::getController() {
  return this->controller;
}

void Timer::update(int duration) {
  this->update(duration, TimerModule::instance->getTimestamp());
}

void Timer::update(int duration, int timestamp) {
  this->duration = duration;
  if (this->isStarted()) {
    	//NVIC_DisableIRQ(TIMER4_IRQn);
      NRF_TIMER4->CC[this->id] = timestamp + duration;
			NRF_TIMER4->INTENSET |= 1 << (16+this->id);
    	//NVIC_ClearPendingIRQ(TIMER4_IRQn);
    	//NVIC_EnableIRQ(TIMER4_IRQn);
  }
}

int Timer::getLastTimestamp() {
	return this->lastTimestamp;
}

void Timer::setLastTimestamp(int lastTimestamp) {
	this->lastTimestamp = lastTimestamp;
}

void Timer::setMode(TimerMode mode) {
  this->mode = mode;
}

void Timer::setCallback(ControllerCallback callback, Controller* controller) {
  this->callback = callback;
  this->controller = controller;
}

void Timer::start() {
    this->started = true;
    NVIC_DisableIRQ(TIMER4_IRQn);
    NRF_TIMER4->CC[this->id] = TimerModule::instance->getTimestamp() + this->duration;
    NRF_TIMER4->INTENSET |= 1 << (16+this->id);
    NVIC_ClearPendingIRQ(TIMER4_IRQn);
    NVIC_EnableIRQ(TIMER4_IRQn);
}

void Timer::stop() {
    this->started = false;
    //NVIC_DisableIRQ(TIMER4_IRQn);
    NRF_TIMER4->CC[this->id] = 0;
    NRF_TIMER4->INTENCLR |= 1 << (16+this->id);
    //NVIC_ClearPendingIRQ(TIMER4_IRQn);
    //NVIC_EnableIRQ(TIMER4_IRQn);
}

bool Timer::isUsed() {
  return this->used;
}


void Timer::setUsed(bool used) {
  this->used = used;
}

ControllerCallback Timer::getCallback() {
  return this->callback;
}

TimerMode Timer::getMode() {
  return this->mode;
}

int Timer::getDuration() {
  return this->duration;
}

extern "C" void TIMER4_IRQHandler(void) {

  for (int i=0;i<NUMBER_OF_TIMERS;i++) {
	 if (NRF_TIMER4->EVENTS_COMPARE[i]) {
		 NRF_TIMER4->EVENTS_COMPARE[i] = 0UL;
		 uint32_t now = TimerModule::instance->getTimestamp();
      if (TimerModule::instance->timers[i]->isStarted()) {
        Timer* timer = TimerModule::instance->timers[i];
				timer->setLastTimestamp(now);
  		  bool repeat = (timer->getController() ->* timer->getCallback())();
        if (!repeat || timer->getMode() == SINGLE_SHOT) {
          NRF_TIMER4->CC[i] = 0;
          //NRF_TIMER4->INTENCLR |= 1 << (16+i);
        }
        else {
    			NRF_TIMER4->CC[i] = now + timer->getDuration() - 11;
    			//NRF_TIMER4->INTENSET |= 1 << (16+i);
        }
      }
   }
  }
}
