#include "controller.h"
#include "core.h"


Controller::Controller(Radio *radio) {
	this->radio = radio;
}

void Controller::addPacket(Packet* packet) {
	Core::instance->pushMessageToQueue(Whad::buildMessageFromPacket(packet));
}

void Controller::sendDebug(const char* msg) {
	Core::instance->pushMessageToQueue(Whad::buildVerboseMessage(msg));
}
