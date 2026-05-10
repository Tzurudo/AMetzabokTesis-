#include "Globals.h"

// Definición de Objetos Globales
Preferences preferences;
BluetoothSerial SerialBT;
WebServer server(80);
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);
RTC_DS3231 rtc;

// Pines de Relays
const int RELAY_PINS[] = {32, 33, 25, 26};

// Variables Globales
Schedule channelSchedules[4][MAX_SCHEDS];
bool relayStatus[4]     = {false, false, false, false};
bool lastSchedState[4]  = {false, false, false, false};
bool autoMode           = false;
bool bluetoothEnabled   = false;          // Arranca en WiFi + LoRa
String relayNames[4]    = {"Relay 1", "Relay 2", "Relay 3", "Relay 4"};
unsigned long lastBTActivity = 0;

// Estados WiFi
WiFiStatus currentWiFiStatus = WIFI_DISCONNECTED;
unsigned long wifiConnectStart = 0;
