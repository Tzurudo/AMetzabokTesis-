#include "BluetoothSerial.h"
#include <WiFi.h>
#include <WebServer.h>
#include <WebSocketsServer.h> // 🔌 Necesaria para comunicación WiFi con la App
#include <ESPmDNS.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <RTClib.h>
#include <Preferences.h>

/* =========================================================
   CONFIGURACIÓN GENERAL
========================================================= */

// OLED
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 32
#define OLED_RESET -1

// Pines
#define BTN_BT 4   // 🔘 Botón reinicio Bluetooth

// Relays
const int RELAY_PINS[] = {32, 33, 25, 26};

// WiFi timeout
const unsigned long WIFI_TIMEOUT = 20000;

// Bluetooth name
const char* BT_NAME = "Metzabook_ESP32";

/* =========================================================
   OBJETOS
========================================================= */

Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);
RTC_DS3231 rtc;
Preferences preferences;
BluetoothSerial SerialBT;
WebServer server(80);
WebSocketsServer webSocket = WebSocketsServer(81); // 🔌 WebSocket en puerto 81

/* =========================================================
   VARIABLES
========================================================= */

const int MAX_SCHEDS = 5;

struct Schedule {
  uint8_t onHour, onMinute;
  uint8_t offHour, offMinute;
  uint8_t daysMask;
  bool enabled;
};

Schedule channelSchedules[4][MAX_SCHEDS];

bool relayStatus[4] = {false};
bool lastSchedState[4] = {false};

bool autoMode = false;
bool bluetoothConnected = false;

// WiFi FSM
enum WiFiStatus { WIFI_DISCONNECTED, WIFI_CONNECTING, WIFI_CONNECTED };
WiFiStatus wifiState = WIFI_DISCONNECTED;
unsigned long wifiStartTime = 0;

// Botón
bool lastBtnState = HIGH;

/* =========================================================
   PREFERENCES
========================================================= */

void saveMode() {
  preferences.begin("modes", false);
  preferences.putBool("auto", autoMode);
  preferences.end();
}

void loadMode() {
  preferences.begin("modes", true);
  autoMode = preferences.getBool("auto", false);
  preferences.end();
}

void saveWifiCredentials(String ssid, String pass) {
  preferences.begin("wifi", false);
  preferences.putString("ssid", ssid);
  preferences.putString("pass", pass);
  preferences.end();
}

void loadAndConnectWiFi() {
  preferences.begin("wifi", true);
  String ssid = preferences.getString("ssid", "");
  String pass = preferences.getString("pass", "");
  preferences.end();

  if (ssid != "") {
    WiFi.begin(ssid.c_str(), pass.c_str());
    wifiState = WIFI_CONNECTING;
    wifiStartTime = millis();
    Serial.println("Intentando conectar a WiFi: " + ssid);
  }
}

/* =========================================================
   COMUNICACIÓN CENTRALIZADA
========================================================= */

void sendStatusToAllClients(String message) {
  // Enviar a Bluetooth
  if (SerialBT.hasClient()) {
    SerialBT.println(message);
  }
  
  // Enviar a todos los clientes WebSocket
  webSocket.broadcastTXT(message);
  
  Serial.println("TX: " + message);
}

void sendFullStatus() {
  for (int i = 0; i < 4; i++) {
    sendStatusToAllClients("CH" + String(i + 1) + "=" + (relayStatus[i] ? "ON" : "OFF"));
  }
  sendStatusToAllClients(autoMode ? "MODE:GLOBAL:AUTO" : "MODE:GLOBAL:MAN");
}

/* =========================================================
   PROCESAMIENTO DE COMANDOS
========================================================= */

void processCommand(String cmd) {
  cmd.trim();
  if (cmd.length() == 0) return;
  
  Serial.println("RX: " + cmd);

  // Comandos de Relés (ON1, OFF1, etc.)
  for (int i = 0; i < 4; i++) {
    if (cmd == "ON" + String(i + 1)) {
      if (autoMode) {
        sendStatusToAllClients("ERR: MODO AUTO ACTIVO");
        return;
      }
      digitalWrite(RELAY_PINS[i], HIGH);
      relayStatus[i] = true;
      sendStatusToAllClients("CH" + String(i + 1) + "=ON");
      return;
    }
    if (cmd == "OFF" + String(i + 1)) {
      if (autoMode) {
        sendStatusToAllClients("ERR: MODO AUTO ACTIVO");
        return;
      }
      digitalWrite(RELAY_PINS[i], LOW);
      relayStatus[i] = false;
      sendStatusToAllClients("CH" + String(i + 1) + "=OFF");
      return;
    }
  }

  // Comandos Globales
  if (cmd == "GLOBAL_AUTO") {
    autoMode = true;
    saveMode();
    sendStatusToAllClients("MODE:GLOBAL:AUTO");
    return;
  }
  if (cmd == "GLOBAL_MANUAL") {
    autoMode = false;
    saveMode();
    sendStatusToAllClients("MODE:GLOBAL:MAN");
    return;
  }

  if (cmd == "ALLOFF") {
    for (int i = 0; i < 4; i++) {
      digitalWrite(RELAY_PINS[i], LOW);
      relayStatus[i] = false;
    }
    sendFullStatus();
    return;
  }

  // Sincronización
  if (cmd == "STATUS") {
    sendFullStatus();
    return;
  }

  // Sincronización de Hora (SETTIME:h:m:s:d:mo:y)
  if (cmd.startsWith("SETTIME:")) {
    // Formato: SETTIME:HH:MM:SS:DD:MM:YYYY
    int parts[6];
    int currentPos = 8; // Después de "SETTIME:"
    for (int i = 0; i < 6; i++) {
      int nextColon = cmd.indexOf(':', currentPos);
      if (nextColon == -1 && i < 5) return; // Error de formato
      if (nextColon == -1) {
        parts[i] = cmd.substring(currentPos).toInt();
      } else {
        parts[i] = cmd.substring(currentPos, nextColon).toInt();
        currentPos = nextColon + 1;
      }
    }
    rtc.adjust(DateTime(parts[5], parts[4], parts[3], parts[0], parts[1], parts[2]));
    sendStatusToAllClients("TIME SET OK");
    return;
  }

  // Configuración WiFi (SETWIFI:ssid:pass)
  if (cmd.startsWith("SETWIFI:")) {
    int firstColon = cmd.indexOf(':');
    int secondColon = cmd.indexOf(':', firstColon + 1);
    if (secondColon != -1) {
      String ssid = cmd.substring(firstColon + 1, secondColon);
      String pass = cmd.substring(secondColon + 1);
      saveWifiCredentials(ssid, pass);
      sendStatusToAllClients("WIFI SET OK. REBOOTING...");
      delay(1000);
      ESP.restart();
    }
    return;
  }
}

/* =========================================================
   WEBSOCKET EVENT HANDLER
========================================================= */

void webSocketEvent(uint8_t num, WStype_t type, uint8_t * payload, size_t length) {
  switch (type) {
    case WStype_DISCONNECTED:
      Serial.printf("[%u] Disconnected!\n", num);
      break;
    case WStype_CONNECTED:
      {
        IPAddress ip = webSocket.remoteIP(num);
        Serial.printf("[%u] Connected from %d.%d.%d.%d\n", num, ip[0], ip[1], ip[2], ip[3]);
        // Al conectar, enviar estado actual
        sendFullStatus();
      }
      break;
    case WStype_TEXT:
      processCommand(String((char*)payload));
      break;
  }
}

/* =========================================================
   BLUETOOTH
========================================================= */

void startBluetooth() {
  SerialBT.begin(BT_NAME);
  Serial.println("Bluetooth iniciado");
}

void restartBluetooth() {
  Serial.println("Reiniciando Bluetooth...");
  SerialBT.end();
  delay(300);
  SerialBT.begin(BT_NAME);
  Serial.println("Bluetooth reiniciado");
}

/* =========================================================
   DISPLAY
========================================================= */

void updateDisplay() {
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);

  DateTime now = rtc.now();

  display.setCursor(0, 0);
  display.printf("%02d:%02d:%02d", now.hour(), now.minute(), now.second());

  display.setCursor(0, 12);
  display.print("ST: ");
  for (int i = 0; i < 4; i++) {
    display.print(relayStatus[i] ? "I " : "O ");
  }

  display.setCursor(0, 24);
  display.print("WiFi:");
  display.print(wifiState == WIFI_CONNECTED ? "ON" :
                wifiState == WIFI_CONNECTING ? "..." : "OFF");

  display.setCursor(70, 24);
  display.print(autoMode ? "AUTO" : "MAN");

  display.display();
}

/* =========================================================
   WIFI
========================================================= */

void handleWiFi() {
  if (wifiState == WIFI_CONNECTING) {
    if (WiFi.status() == WL_CONNECTED) {
      wifiState = WIFI_CONNECTED;
      Serial.println("WiFi conectado. IP: " + WiFi.localIP().toString());

      MDNS.begin("metzabok");
      server.begin();
      webSocket.begin();
      webSocket.onEvent(webSocketEvent);

    } else if (millis() - wifiStartTime > WIFI_TIMEOUT) {
      wifiState = WIFI_DISCONNECTED;
      WiFi.disconnect();
      Serial.println("WiFi timeout");
    }
  }
}

/* =========================================================
   BOTÓN (REINICIO BLUETOOTH)
========================================================= */

void handleButton() {
  bool currentState = digitalRead(BTN_BT);
  if (currentState == LOW && lastBtnState == HIGH) {
    restartBluetooth();
    delay(250); // anti-rebote
  }
  lastBtnState = currentState;
}

/* =========================================================
   SCHEDULE
========================================================= */

void checkSchedules() {
  if (!autoMode) return;

  DateTime now = rtc.now();
  int currentMinutes = now.hour() * 60 + now.minute();
  int currentDay = now.dayOfTheWeek();

  for (int i = 0; i < 4; i++) {
    bool shouldBeOn = false;
    for (int j = 0; j < MAX_SCHEDS; j++) {
      if (!channelSchedules[i][j].enabled) continue;
      if (channelSchedules[i][j].daysMask & (1 << currentDay)) {
        int onM = channelSchedules[i][j].onHour * 60 + channelSchedules[i][j].onMinute;
        int offM = channelSchedules[i][j].offHour * 60 + channelSchedules[i][j].offMinute;
        if ((onM < offM && currentMinutes >= onM && currentMinutes < offM) ||
            (onM > offM && (currentMinutes >= onM || currentMinutes < offM))) {
          shouldBeOn = true;
        }
      }
    }

    if (shouldBeOn != lastSchedState[i]) {
      relayStatus[i] = shouldBeOn;
      digitalWrite(RELAY_PINS[i], shouldBeOn);
      lastSchedState[i] = shouldBeOn;
      // ✅ Notificar cambio a la App
      sendStatusToAllClients("CH" + String(i + 1) + "=" + (shouldBeOn ? "ON" : "OFF"));
    }
  }
}

/* =========================================================
   SETUP
========================================================= */

void setup() {
  Serial.begin(115200);
  Wire.begin(21, 22);

  // OLED
  if(!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) {
    Serial.println("SSD1306 allocation failed");
  }

  // RTC
  if (!rtc.begin()) {
    Serial.println("Couldn't find RTC");
  }

  // Relays
  for (int i = 0; i < 4; i++) {
    pinMode(RELAY_PINS[i], OUTPUT);
    digitalWrite(RELAY_PINS[i], LOW);
  }

  // Botón
  pinMode(BTN_BT, INPUT_PULLUP);

  // Cargar datos
  loadMode();
  loadAndConnectWiFi();

  // Bluetooth
  startBluetooth();
}

/* =========================================================
   LOOP
========================================================= */

void loop() {
  handleButton();
  handleWiFi();

  if (wifiState == WIFI_CONNECTED) {
    server.handleClient();
    webSocket.loop();
  }

  // Leer comandos de Bluetooth
  if (SerialBT.available()) {
    String btCmd = SerialBT.readStringUntil('\n');
    processCommand(btCmd);
  }

  checkSchedules();
  updateDisplay();

  delay(50); // Reducido delay para mejor respuesta
}