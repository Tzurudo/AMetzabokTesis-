#ifndef GLOBALS_H
#define GLOBALS_H

#include <Arduino.h>
#include <LoRa.h>
#include <WebServer.h>
#include <EEPROM.h>

// Pines LoRa (ESP32-C3)
#define PIN_NSS   7
#define PIN_RST   3
#define PIN_DIO0  1
#define PIN_SCK   6
#define PIN_MOSI  4
#define PIN_MISO  5
#define LED_PIN   8

// Direcciones LoRa
#define GATEWAY_ADDRESS  0x01
#define SLAVE_ADDRESS    0x02

// Config LoRa
#define LORA_FREQ_HZ       915E6
#define LORA_TX_POWER      20
#define LORA_SPREAD_FACTOR 12
#define LORA_BANDWIDTH     125E3
#define LORA_CODING_RATE   5

// Config WiFi AP
extern const char* AP_SSID;
extern const char* AP_PASS;

// EEPROM Config
#define EEPROM_SIZE 512
#define RELAY_NAMES_ADDR 0
#define RELAY_NAME_LEN 20
#define NUM_RELAYS 4

// Objetos Globales
extern WebServer server;
extern String lastLoRaResponse;
extern unsigned long lastResponseTime;
extern String relayNames[NUM_RELAYS];

// Funciones EEPROM
void initEEPROM();
void saveRelayNames();
void loadRelayNames();
String getRelayName(int index);
void setRelayName(int index, String name);

#endif
