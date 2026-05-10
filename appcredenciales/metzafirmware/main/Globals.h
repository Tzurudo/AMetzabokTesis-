#ifndef GLOBALS_H
#define GLOBALS_H

#include <Arduino.h>
#include "BluetoothSerial.h"
#include <WiFi.h>
#include <WebServer.h>
#include <Adafruit_SSD1306.h>
#include <RTClib.h>
#include <Preferences.h>

// Configuración OLED
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 32
#define OLED_RESET -1

// Botón
#define BUTTON_PIN 4

// LoRa Pins (ESP32 Slave)
#define LORA_SCK  18
#define LORA_MISO 19
#define LORA_MOSI 23
#define LORA_NSS  5
#define LORA_RST  14
#define LORA_DIO0 2

// Configuración de Persistencia
extern Preferences preferences;

// Objetos Globales
extern BluetoothSerial SerialBT;
extern WebServer server;
extern Adafruit_SSD1306 display;
extern RTC_DS3231 rtc;

// Pines de Relays
extern const int RELAY_PINS[];

// Estructuras
const int MAX_SCHEDS = 5;
struct Schedule {
  uint8_t onHour;
  uint8_t onMinute;
  uint8_t offHour;
  uint8_t offMinute;
  uint8_t daysMask; // Bits: 0=Dom, 1=Lun, 2=Mar, 3=Mie, 4=Jue, 5=Vie, 6=Sab
  bool enabled;
};

// Variables Globales
extern Schedule channelSchedules[4][MAX_SCHEDS];
extern bool relayStatus[4];
extern bool lastSchedState[4];
extern bool autoMode;
extern bool bluetoothEnabled;
extern String relayNames[4];
extern unsigned long lastBTActivity;

// Estados WiFi
enum WiFiStatus { WIFI_DISCONNECTED, WIFI_CONNECTING, WIFI_CONNECTED };
extern WiFiStatus currentWiFiStatus;
extern unsigned long wifiConnectStart;
const unsigned long WIFI_TIMEOUT = 20000;

#endif
