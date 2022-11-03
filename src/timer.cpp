#include "timer.h"
#include "bsp.h"

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

		NRF_TIMER3->MODE = TIMER_MODE_MODE_Timer;
		NRF_TIMER3->BITMODE = TIMER_BITMODE_BITMODE_32Bit;
		NRF_TIMER3->PRESCALER = TIMER_PRESCALER;

		NRF_TIMER3->INTENCLR = TIMER_INTENCLR_COMPARE0_Msk
			| TIMER_INTENCLR_COMPARE1_Msk
			| TIMER_INTENCLR_COMPARE2_Msk
			| TIMER_INTENCLR_COMPARE3_Msk
			| TIMER_INTENCLR_COMPARE4_Msk
			| TIMER_INTENCLR_COMPARE5_Msk;

	NVIC_SetPriority(TIMER3_IRQn, 1);
	NVIC_SetPriority(TIMER4_IRQn, 1);

	NRF_TIMER3->TASKS_CLEAR = 1;
	NRF_TIMER4->TASKS_CLEAR = 1;

	//NRF_TIMER3->CC[0] = 1000000;
	NRF_TIMER3->INTENSET = (1 << 16);

	//NRF_TIMER3->TASKS_START = 1;
	NRF_TIMER4->TASKS_START = 1;

  for (int i=0;i<NUMBER_OF_TIMERS;i++) {
    this->timers[i] = new Timer(i);
  }

	this->hardwareTimer = new HardwareControlledTimer();
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

HardwareControlledTimer* TimerModule::getHardwareControlledTimer() {
	if (this->hardwareTimer->isUsed()) {
		return NULL;
	}
	else {
		this->hardwareTimer->setUsed(true);
		return this->hardwareTimer;
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
HardwareControlledTimer::HardwareControlledTimer() {
	this->callback = NULL;
  this->controller = NULL;
  this->duration = 1000000;
  this->used = false;
}

void HardwareControlledTimer::enable(volatile void* event) {
		NVIC_DisableIRQ(TIMER3_IRQn);
		NRF_TIMER3->CC[0] = this->duration;
		NRF_TIMER3->INTENSET = (1 << 16);

	  NRF_PPI->CH[0].EEP = (uint32_t)event;
	  NRF_PPI->CH[0].TEP = (uint32_t)&(NRF_TIMER3->TASKS_START);

		NRF_PPI->CH[1].EEP = (uint32_t)event;
		NRF_PPI->CH[1].TEP = (uint32_t)&(NRF_TIMER3->TASKS_CLEAR);

	  NRF_PPI->CHEN = 3;
		NVIC_ClearPendingIRQ(TIMER3_IRQn);
		NVIC_EnableIRQ(TIMER3_IRQn);
}

void HardwareControlledTimer::disable() {
	NRF_TIMER3->TASKS_STOP = 1;
	NRF_TIMER3->TASKS_CLEAR = 1;

	NRF_PPI->CH[0].EEP = 0;
	NRF_PPI->CH[0].TEP = 0;

	NRF_PPI->CH[1].EEP = 0;
	NRF_PPI->CH[1].TEP = 0;

	NRF_PPI->CHEN = 0;
}

void HardwareControlledTimer::setCallback(ControllerCallback callback, Controller* controller) {
  this->callback = callback;
  this->controller = controller;
}

bool HardwareControlledTimer::isUsed() {
	return this->used;
}

ControllerCallback HardwareControlledTimer::getCallback() {
	return this->callback;
}
Controller* HardwareControlledTimer::getController() {
	return this->controller;
}

int HardwareControlledTimer::getDuration() {
	return this->duration;
}
void HardwareControlledTimer::setDuration(int duration) {
	this->duration = duration;
}

void HardwareControlledTimer::setUsed(bool used) {
	this->used = used;
}

void HardwareControlledTimer::release() {
	// disable PPI
	this->disable();
	this->used = false;
}

extern "C" void TIMER3_IRQHandler(void) {
	if (NRF_TIMER3->EVENTS_COMPARE[0]) {
		NRF_TIMER3->EVENTS_COMPARE[0] = 0UL;
		HardwareControlledTimer* timer = TimerModule::instance->hardwareTimer;
		(timer->getController() ->* timer->getCallback())();
		NRF_TIMER3->CC[0] = 0;
	}
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
