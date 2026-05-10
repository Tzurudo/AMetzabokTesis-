#ifndef LORA_MANAGER_H
#define LORA_MANAGER_H

#include "Globals.h"

void setupLoRa();
bool sendLoRaToSlave(const char* message);
void receiveLoRa();

#endif
