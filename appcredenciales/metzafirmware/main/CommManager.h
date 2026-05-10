#ifndef COMM_MANAGER_H
#define COMM_MANAGER_H

#include "Globals.h"

void handleWiFi();
void setupWebServer();
void processCommand(String cmd);
void setupLoRa();
void handleLoRa();

#endif
