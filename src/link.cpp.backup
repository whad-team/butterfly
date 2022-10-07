#include "link.h"


LinkModule::LinkModule() {
  this->controlPin = 0;
  this->mode = RESET_LINK;
}

void LinkModule::configureLink(LinkMode mode, uint8_t pin) {
  this->mode = mode;
  this->controlPin = pin;
  if (mode == RESET_LINK) {
    this->resetLink();
  }
  else if (mode == LINK_SLAVE) {
    //this->resetLink();
    this->setupSlave();
  }
  else if (mode == LINK_MASTER) {
    //this->resetLink();
    this->setupMaster();
  }
}


void LinkModule::resetLink() {
  NRF_P0->PIN_CNF[this->controlPin] = 0;
  NRF_GPIOTE->CONFIG[0] = 0;
  NRF_GPIOTE->CONFIG[1] = 0;

  NRF_GPIOTE->EVENTS_IN[0] = 0;
  NRF_GPIOTE->EVENTS_IN[1] = 0;

  NRF_PPI->CH[0].EEP = 0;
  NRF_PPI->CH[0].TEP = 0;

  NRF_PPI->CH[1].EEP = 0;
  NRF_PPI->CH[1].TEP = 0;
}

void LinkModule::setupSlave() {
  this->initInput(this->controlPin);
  this->initPPI(this->controlPin);
}

void LinkModule::setupMaster() {
  this->initOutput(this->controlPin);
}

void LinkModule::sendSignalToSlave(int signal) {
  if (signal == START_SLAVE_RADIO) {
	   NRF_P0->OUTCLR =   (1ul << this->controlPin);
  }
  else {
    	NRF_P0->OUTSET =   (1ul << this->controlPin);
  }
}
void LinkModule::initInput(int pin) {
  NRF_P0->PIN_CNF[pin] = (GPIO_PIN_CNF_SENSE_Disabled << GPIO_PIN_CNF_SENSE_Pos)
                                      | (GPIO_PIN_CNF_DRIVE_S0S1 << GPIO_PIN_CNF_DRIVE_Pos)
                                      | (GPIO_PIN_CNF_PULL_Disabled << GPIO_PIN_CNF_PULL_Pos)
                                      | (GPIO_PIN_CNF_INPUT_Connect << GPIO_PIN_CNF_INPUT_Pos)
                                      | (GPIO_PIN_CNF_DIR_Input << GPIO_PIN_CNF_DIR_Pos);
}


void LinkModule::initOutput(int pin) {
  NRF_P0->PIN_CNF[pin] = (GPIO_PIN_CNF_SENSE_Disabled << GPIO_PIN_CNF_SENSE_Pos)
                                      | (GPIO_PIN_CNF_DRIVE_S0S1 << GPIO_PIN_CNF_DRIVE_Pos)
                                      | (GPIO_PIN_CNF_PULL_Disabled << GPIO_PIN_CNF_PULL_Pos)
                                      | (GPIO_PIN_CNF_INPUT_Connect << GPIO_PIN_CNF_INPUT_Pos)
                                      | (GPIO_PIN_CNF_DIR_Output << GPIO_PIN_CNF_DIR_Pos);
  NRF_P0->OUT = 0;
}

void LinkModule::initPPI(int pin) {
  NRF_GPIOTE->CONFIG[0] =  (GPIOTE_CONFIG_POLARITY_HiToLo << GPIOTE_CONFIG_POLARITY_Pos)
                     | (pin << GPIOTE_CONFIG_PSEL_Pos)
                     | (GPIOTE_CONFIG_MODE_Event << GPIOTE_CONFIG_MODE_Pos);
  NRF_GPIOTE->CONFIG[1] =  (GPIOTE_CONFIG_POLARITY_LoToHi << GPIOTE_CONFIG_POLARITY_Pos)
                     | (pin << GPIOTE_CONFIG_PSEL_Pos)
                     | (GPIOTE_CONFIG_MODE_Event << GPIOTE_CONFIG_MODE_Pos);

  NRF_GPIOTE->EVENTS_IN[0] = 0;
  NRF_GPIOTE->EVENTS_IN[1] = 0;

  NRF_PPI->CH[0].EEP = (uint32_t)&(NRF_GPIOTE->EVENTS_IN[0]);
  NRF_PPI->CH[0].TEP = (uint32_t)&(NRF_RADIO->TASKS_START);


  NRF_PPI->CH[1].EEP = (uint32_t)&(NRF_GPIOTE->EVENTS_IN[1]);
  NRF_PPI->CH[1].TEP = (uint32_t)&(NRF_RADIO->TASKS_STOP);

  NRF_PPI->CHEN= 0x00000003;
}
