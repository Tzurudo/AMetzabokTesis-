#include "BluetoothSerial.h"
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <RTClib.h>
#include <Preferences.h>
#include <UniversalTelegramBot.h>
#include <ArduinoJson.h>
#include <time.h>
#include <sys/time.h>

/* =========================================================
   CONFIGURACIÓN GENERAL
   ========================================================= */
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 32
#define OLED_RESET -1
#define BTN_BT 4
const int RELAY_PINS[] = {32, 33, 25, 26};
const unsigned long WIFI_TIMEOUT = 20000;
const char* BT_NAME = "Metzabook_ESP32";

/* =========================================================
   OBJETOS Y VARIABLES GLOBALES
   ========================================================= */
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);
RTC_DS3231 rtc;
Preferences preferences;
BluetoothSerial SerialBT;

// Telegram
WiFiClientSecure clientSecure;
UniversalTelegramBot *bot = nullptr;
String telegramToken = "";
String telegramChatId = "";
String wifiSSID = "";
String wifiPassword = "";

// Estado
bool relayStatus[4] = {false};
bool autoMode = false;
bool btActive = true;
bool telegramConfigured = false;
enum WiFiStatus { WIFI_DISCONNECTED, WIFI_CONNECTING, WIFI_CONNECTED };
WiFiStatus wifiState = WIFI_DISCONNECTED;

// Horarios
const int MAX_SCHEDS = 5;
struct Schedule {
  uint8_t onHour, onMinute, offHour, offMinute, daysMask;
  bool enabled;
};
Schedule channelSchedules[4][MAX_SCHEDS];
bool lastSchedState[4] = {false};

// Control de Botón (Triple Clic)
int buttonPressCount = 0;
unsigned long lastButtonPressTime = 0;
const unsigned long MULTI_CLICK_TIME = 500; // Ventana para clics rápidos
bool lastBtnState = HIGH;

/* =========================================================
   CERTIFICADO TELEGRAM (ISRG Root X1)
   ========================================================= */
const char* METZA_CERT_ROOT =  "-----BEGIN CERTIFICATE-----\n"
  "MIIFazCCA1OgAwIBAgIRAIIQz7DSQONZRGPgu2OCiwAwDQYJKoZIhvcNAQELBQAw\n"
  "TzELMAkGA1UEBhMCVVMxKTAnBgNVBAoTIEludGVybmV0IFNlY3VyaXR5IFJlc2Vh\n"
  "cmNoIEdyb3VwMRUwEwYDVQQDEwxJU1JHIFJvb3QgWDEwHhcNMTUwNjA0MTEwNDM4\n"
  "WhcNMzUwNjA0MTEwNDM4WjBPMQswCQYDVQQGEwJVUzEpMCcGA1UEChMgSW50ZXJu\n"
  "ZXQgU2VjdXJpdHkgUmVzZWFyY2ggGvUZ2j/1IHFHPYHtB2DY8PK+rL5xI/9WcVvM\n"
  "80mX7L3trSXIdQdLb0XNqn+KbbGtKNYHr3kyWqP+1UziA9V9aQ8Z9azH84j0PhYI\n"
  "XRiLbjjJ+T0R0tQyYhMqbL2kE9vHZ80yP+UyHB+WrX8gDBlc2/4W4B4B0Hq0P94lx\n"
  "U7KUVeV2OjiPzAgMBAAGjQjBAMA4GA1UdDwEB/wQEAwIBBjAPBgNVHRMBAf8EBTAD\n"
  "AQH/MB0GA1UdDgQWBBTfQfCFFzJbV0JT4w9nQyCJ4r3j2DANBgkqhkiG9w0BAQsF\n"
  "AAOCAgEAeTVlBqW6sqB6DjhggQv5DZ2d2/bnLElT6/JqT7eoLGTYqCkqbNpmYgRA\n"
  "0kmZB4qXH8KEFPqGpLX1J7vC+VB24yywW4DM4YcdY5F+8FpcI9pLI/bH8+8oXvQl\n"
  "kr4CO/Q4Y4UmuBqgZk3ZPHFftkLpNMhlOG/H0G2pXVSLJqKGGgUotO3kFrHSl9Kj\n"
  "9uG1YYAF9oA/9xA/s0wM6CqxXW1lZcxYbWjrqmOYD5kCWdcTgUBUgbp1ndgw9zlY\n"
  "DGeg67CXcKzD0Dt0ZOGM6Z3hK+YrsI8N6PZ+Jzjkt4PpNrFrErT9HYm/pnmim4ej\n"
  "QZAdh32XyPcUxqA8dCG5yf4/nTj4cQ/8GndqizRchCLf1R88qDYk/UiHY5BxwAqc\n"
  "EoJfBptq5A6NWUxVIBpYwEfgHHEh5Vr6aunD5iA0kCVzFzS+K/Wvx0rWwzU5W02C\n"
  "7OPiqstKsmF/1JKSyp05V9Z9E4Jq8YJhQqT1rB64SYeTnb2T91jLtM3Y2/GOiXHx\n"
  "+jC00QZNKQqWWr4bKtR5VOTkOUqR6hbyWvZ94ELIqM6UJvQ9lA5uO+09X6vEbPKk\n"
  "YZUc0ciT56wtlG4s0VQOq55CHqxgEGQyxHzt+gUyHnyO0bVhH6gjwX7IP2F2qoUe\n"
  "IfFMR9q+u4onFxJphMftl4RpkP/TFmgUCp5M9RrV9D0PnqQb9Aq+AWb1oY3vxr7r\n"
  "X2ZtZ8B4lwfkEZEpxhH8YOVjfn65MpCnoEX8bMiWvfuzXY8yWEM+XMChSp9ne8lX\n"
  "4d8pEi9tCnhTYwBFe5NHcrPF1x5Q1Uc=\n"
  "-----END CERTIFICATE-----\n";

/* =========================================================
   PROTOTIPOS
   ========================================================= */
void setupTelegram();
void sendTelegramMessage(String msg);
void processBluetoothCommand(String cmd);
void loadAndConnectWiFi();
void stopWiFi();
void startBluetooth();
void stopBluetooth();

/* =========================================================
   FUNCIONES DE PERSISTENCIA
   ========================================================= */
void saveMode() {
  preferences.begin("modes", false);
  preferences.putBool("auto", autoMode);
  preferences.end();
}

void loadCredentials() {
  preferences.begin("wifi", true);
  wifiSSID = preferences.getString("ssid", "");
  wifiPassword = preferences.getString("pass", "");
  preferences.end();
  preferences.begin("telegram", true);
  telegramToken = preferences.getString("token", "");
  telegramChatId = preferences.getString("chatid", "");
  preferences.end();
  preferences.begin("modes", true);
  autoMode = preferences.getBool("auto", false);
  preferences.end();
}

/* =========================================================
   CORE 0: TAREA DE TELEGRAM
   ========================================================= */
void telegramTask(void * pvParameters) {
  unsigned long lastCheck = 0;
  
  for(;;) {
    if (telegramConfigured && WiFi.status() == WL_CONNECTED && bot != nullptr) {
      if (millis() - lastCheck > 3000) { // Revisar cada 3s
        int numNewMessages = bot->getUpdates(bot->last_message_received + 1);
        while (numNewMessages) {
          for (int i = 0; i < numNewMessages; i++) {
            String text = bot->messages[i].text;
            String chat_id = bot->messages[i].chat_id;
            text.toLowerCase();
            
            if (chat_id != telegramChatId) {
              bot->sendMessage(chat_id, "No autorizado", "");
              continue;
            }
            
            // Control de relés por Telegram
            for (int ch = 0; ch < 4; ch++) {
              if (text == "on" + String(ch+1)) {
                if (!autoMode) {
                  digitalWrite(RELAY_PINS[ch], HIGH);
                  relayStatus[ch] = true;
                  bot->sendMessage(chat_id, "Canal " + String(ch+1) + " ON", "");
                } else {
                  bot->sendMessage(chat_id, "Error: Modo AUTO activo", "");
                }
              }
              if (text == "off" + String(ch+1)) {
                if (!autoMode) {
                  digitalWrite(RELAY_PINS[ch], LOW);
                  relayStatus[ch] = false;
                  bot->sendMessage(chat_id, "Canal " + String(ch+1) + " OFF", "");
                } else {
                  bot->sendMessage(chat_id, "Error: Modo AUTO activo", "");
                }
              }
            }
            
            if (text == "auto") { autoMode = true; saveMode(); bot->sendMessage(chat_id, "Modo: AUTO", ""); }
            if (text == "manual") { autoMode = false; saveMode(); bot->sendMessage(chat_id, "Modo: MANUAL", ""); }
            if (text == "status") {
              String s = "Estado:\n";
              for(int i=0; i<4; i++) s += "CH" + String(i+1) + (relayStatus[i]?" ON":" OFF") + "\n";
              s += "Modo: " + String(autoMode?"AUTO":"MANUAL");
              bot->sendMessage(chat_id, s, "");
            }
          }
          numNewMessages = bot->getUpdates(bot->last_message_received + 1);
        }
        lastCheck = millis();
      }
    }
    vTaskDelay(100 / portTICK_PERIOD_MS); // Pequeño respiro
  }
}

/* =========================================================
   CORE 1: LÓGICA PRINCIPAL (Bluetooth, UI, Botón)
   ========================================================= */
void setupTelegram() {
  clientSecure.setCACert(METZA_CERT_ROOT);
  if (bot != nullptr) delete bot;
  bot = new UniversalTelegramBot(telegramToken, clientSecure);
  telegramConfigured = true;
  
  // Sincronizar Hora
  if (rtc.begin()) {
    DateTime now = rtc.now();
    struct tm tm;
    tm.tm_year = now.year() - 1900; tm.tm_mon = now.month() - 1; tm.tm_mday = now.day();
    tm.tm_hour = now.hour(); tm.tm_min = now.minute(); tm.tm_sec = now.second();
    time_t t = mktime(&tm);
    struct timeval tv = { .tv_sec = t, .tv_usec = 0 };
    settimeofday(&tv, NULL);
  }
}

void handleButton() {
  bool currentState = digitalRead(BTN_BT);
  unsigned long now = millis();
  
  if (currentState == LOW && lastBtnState == HIGH) {
    if (now - lastButtonPressTime > MULTI_CLICK_TIME) {
      buttonPressCount = 1;
    } else {
      buttonPressCount++;
    }
    lastButtonPressTime = now;
    
    if (buttonPressCount == 3) {
      buttonPressCount = 0;
      if (btActive) {
        stopBluetooth();
        Serial.println("Bluetooth desactivado por usuario");
      } else {
        startBluetooth();
        Serial.println("Bluetooth activado por usuario");
      }
    }
  }
  lastBtnState = currentState;
}

void processBluetoothCommand(String cmd) {
  cmd.trim();
  if (cmd == "STATUS") {
    for(int i=0; i<4; i++) SerialBT.println("CH" + String(i+1) + "=" + String(relayStatus[i]?"ON":"OFF"));
    SerialBT.println(autoMode ? "MODE:GLOBAL:AUTO" : "MODE:GLOBAL:MANUAL");
    SerialBT.println("WIFI:" + String(WiFi.status()==WL_CONNECTED?"ON":"OFF"));
    SerialBT.println("TG:" + String(telegramConfigured?"OK":"NO"));
    return;
  }
  if (cmd == "GLOBAL_AUTO") { autoMode = true; saveMode(); SerialBT.println("MODE:GLOBAL:AUTO"); return; }
  if (cmd == "GLOBAL_MANUAL") { autoMode = false; saveMode(); SerialBT.println("MODE:GLOBAL:MANUAL"); return; }
  
  // Control de canales
  for (int i=0; i<4; i++) {
    if (cmd == "ON"+String(i+1)) { if(!autoMode){digitalWrite(RELAY_PINS[i],HIGH); relayStatus[i]=true; SerialBT.println("CH"+String(i+1)+"=ON");} return; }
    if (cmd == "OFF"+String(i+1)) { if(!autoMode){digitalWrite(RELAY_PINS[i],LOW); relayStatus[i]=false; SerialBT.println("CH"+String(i+1)+"=OFF");} return; }
  }
}

void processConfigCommand(String cmd) {
  if (cmd.startsWith("SET_WIFI:")) {
    String rest = cmd.substring(9); int sep = rest.indexOf(':');
    if (sep > 0) {
      wifiSSID = rest.substring(0, sep); wifiPassword = rest.substring(sep + 1);
      preferences.begin("wifi", false); preferences.putString("ssid", wifiSSID); preferences.putString("pass", wifiPassword); preferences.end();
      SerialBT.println("WiFi guardado: " + wifiSSID);
    }
  }
  if (cmd.startsWith("SET_TELEGRAM:")) {
    String rest = cmd.substring(13); int sep = rest.indexOf(':');
    if (sep > 0) {
      telegramToken = rest.substring(0, sep); telegramChatId = rest.substring(sep + 1);
      preferences.begin("telegram", false); preferences.putString("token", telegramToken); preferences.putString("chatid", telegramChatId); preferences.end();
      setupTelegram();
      SerialBT.println("Telegram guardado");
    }
  }
  if (cmd == "CONNECT_WIFI") loadAndConnectWiFi();
}

void loadAndConnectWiFi() {
  if (wifiSSID != "") {
    WiFi.begin(wifiSSID.c_str(), wifiPassword.c_str());
    wifiState = WIFI_CONNECTING;
  }
}

void stopWiFi() { WiFi.disconnect(true); WiFi.mode(WIFI_OFF); wifiState = WIFI_DISCONNECTED; }
void startBluetooth() { SerialBT.begin(BT_NAME); SerialBT.setTimeout(50); btActive = true; }
void stopBluetooth() { SerialBT.end(); btActive = false; }

void updateDisplay() {
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);
  DateTime now = rtc.now();
  display.setCursor(0,0); display.print(autoMode?"AUTO":"MAN");
  display.setCursor(70,0); display.printf("%02d:%02d:%02d", now.hour(), now.minute(), now.second());
  display.setCursor(0,12); display.print("ST: ");
  for(int i=0; i<4; i++) display.print(relayStatus[i]?"I ":"O ");
  display.setCursor(0,24); display.print("W:"); display.print(WiFi.status()==WL_CONNECTED?"ON":"OFF");
  display.setCursor(35,24); display.print("TG:"); display.print(telegramConfigured?"OK":"NO");
  display.setCursor(90,24); display.print("BT:"); display.print(btActive?"ON":"OFF");
  display.display();
}

void setup() {
  Serial.begin(115200);
  Wire.begin(21, 22);
  for (int i=0; i<4; i++) { pinMode(RELAY_PINS[i], OUTPUT); digitalWrite(RELAY_PINS[i], LOW); }
  pinMode(BTN_BT, INPUT_PULLUP);
  
  if(!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) Serial.println("OLED error");
  if(!rtc.begin()) Serial.println("RTC error");
  
  loadCredentials();
  if (wifiSSID != "") loadAndConnectWiFi(); // Conectar WiFi siempre al inicio
  if (telegramToken != "") setupTelegram();
  startBluetooth();

  // LANZAR TAREA EN CORE 0
  xTaskCreatePinnedToCore(telegramTask, "TelegramTask", 10000, NULL, 1, NULL, 0);
}

void loop() {
  handleButton();
  if (WiFi.status() == WL_CONNECTED && wifiState == WIFI_CONNECTING) wifiState = WIFI_CONNECTED;
  
  static unsigned long lastDisplay = 0;
  if (millis() - lastDisplay >= 500) { updateDisplay(); lastDisplay = millis(); }
  
  if (btActive && SerialBT.available()) {
    String btCmd = SerialBT.readStringUntil('\n');
    if (btCmd.startsWith("SET_") || btCmd.startsWith("CONNECT_")) processConfigCommand(btCmd);
    else processBluetoothCommand(btCmd);
  }
  
  // Aquí puedes añadir el checkSchedules() si quieres automatización
  vTaskDelay(10 / portTICK_PERIOD_MS);
}