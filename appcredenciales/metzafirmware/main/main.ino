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

/* =========================================================
   CONFIGURACIÓN GENERAL
   ========================================================= */
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 32
#define OLED_RESET   -1
#define BTN_BT        4

const int RELAY_PINS[] = {32, 33, 25, 26};
const char* BT_NAME = "Metzabook_ESP32";
const int MAX_SCHEDS = 5;

/* =========================================================
   OBJETOS
   ========================================================= */
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);
RTC_DS3231 rtc;
BluetoothSerial SerialBT;
Preferences preferences;
WiFiClientSecure clientSecure;
UniversalTelegramBot *bot = nullptr;

/* =========================================================
   VARIABLES GLOBALES
   ========================================================= */
String wifiSSID = "";
String wifiPass = "";
String tgToken  = "";
String tgChatId = "";

bool relayStatus[4]  = {false};
bool autoMode        = false;
bool telegramOK      = false;
bool btActive        = true;

struct Schedule {
  uint8_t onHour, onMinute, offHour, offMinute, daysMask;
  bool enabled;
};
Schedule channelSchedules[4][MAX_SCHEDS];
bool lastSchedState[4] = {false};

unsigned long lastDisplayUpdate = 0;
unsigned long lastTelegramCheck = 0;
unsigned long lastScheduleCheck = 0;

/* =========================================================
   CERTIFICADO ROOT (Let's Encrypt ISRG X1)
   ========================================================= */
const char* ROOT_CA = \
"-----BEGIN CERTIFICATE-----\n"
"MIIFazCCA1OgAwIBAgIRAIIQz7DSQONZRGPgu2OCiwAwDQYJKoZIhvcNAQELBQAw\n"
"TzELMAkGA1UEBhMCVVMxKTAnBgNVBAoTIEludGVybmV0IFNlY3VyaXR5IFJlc2Vh\n"
"cmNoIEdyb3VwMRUwEwYDVQQDEwxJU1JHIFJvb3QgWDEwHhcNMTUwNjA0MTEwNDM4\n"
"WhcNMzUwNjA0MTEwNDM4WjBPMQswCQYDVQQGEwJVUzEpMCcGA1UEChMgSW50ZXJu\n"
"ZXQgU2VjdXJpdHkgUmVzZWFyY2ggR3JvdXAxFTATBgNVBAMTDElTUkcgUm9vdCBY\n"
"MTAeFw0xNTA2MDQxMTA0MzhaFw0zNTA2MDQxMTA0MzhaME8xCzAJBgNVBAYTAlVT\n"
"MSkwJwYDVQQKEyBJbnRlcm5ldCBTZWN1cml0eSBSZXNlYXJjaCBHcm91cDEVMBMG\n"
"A1UEAxMMSVNSRyBSb290IFgxMIICIjANBgkqhkiG9w0BAQEFAAOCAg8AMIICCgKC\n"
"AgEAiNZcPcT1S1F/8nIuPkBUhZzF3hO4f6ZvKEIDfGwLvW6N9jHGxBf4FyZOPQ1O\n"
"AgMBAAGjQjBAMA4GA1UdDwEB/wQEAwIBBjAPBgNVHRMBAf8EBTADAQH/MB0GA1Ud\n"
"DgQWBBTfQfCFFzJbV0JT4w9nQyCJ4r3j2DANBgkqhkiG9w0BAQsFAAOCAgEAeTVl\n"
"-----END CERTIFICATE-----\n";

/* =========================================================
   PERSISTENCIA
   ========================================================= */
void saveMode() {
  preferences.begin("cfg", false);
  preferences.putBool("auto", autoMode);
  preferences.end();
}

void saveSchedules() {
  preferences.begin("scheds", false);
  preferences.putBytes("data", channelSchedules, sizeof(channelSchedules));
  preferences.end();
}

void loadAll() {
  preferences.begin("wifi", true);
  wifiSSID = preferences.getString("ssid", "");
  wifiPass = preferences.getString("pass", "");
  preferences.end();

  preferences.begin("tg", true);
  tgToken  = preferences.getString("token", "");
  tgChatId = preferences.getString("chat",  "");
  preferences.end();

  preferences.begin("cfg", true);
  autoMode = preferences.getBool("auto", false);
  preferences.end();

  preferences.begin("scheds", true);
  if (preferences.getBytesLength("data") == sizeof(channelSchedules)) {
    preferences.getBytes("data", channelSchedules, sizeof(channelSchedules));
  } else {
    for (int i = 0; i < 4; i++)
      for (int j = 0; j < MAX_SCHEDS; j++)
        channelSchedules[i][j].enabled = false;
  }
  preferences.end();
}

/* =========================================================
   WIFI
   ========================================================= */
void connectWiFi() {
  if (wifiSSID == "") return;
  WiFi.mode(WIFI_STA);
  WiFi.begin(wifiSSID.c_str(), wifiPass.c_str());
  Serial.print("Conectando WiFi");
  unsigned long t = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - t < 15000) {
    delay(300); Serial.print(".");
  }
  Serial.println(WiFi.status() == WL_CONNECTED ? "\nWiFi OK: " + WiFi.localIP().toString() : "\nWiFi FAIL");
}

/* =========================================================
   TELEGRAM
   ========================================================= */
void setupTelegram() {
  if (tgToken == "") return;
  clientSecure.setCACert(ROOT_CA);
  if (bot != nullptr) delete bot;
  bot = new UniversalTelegramBot(tgToken, clientSecure);
  telegramOK = true;
  Serial.println("Telegram listo");
}

void tgSend(String msg) {
  if (telegramOK && bot && WiFi.status() == WL_CONNECTED && tgChatId != "")
    bot->sendMessage(tgChatId, msg, "");
}

void checkTelegram() {
  if (!telegramOK || !bot || WiFi.status() != WL_CONNECTED) return;
  if (millis() - lastTelegramCheck < 3000) return;
  lastTelegramCheck = millis();

  int n = bot->getUpdates(bot->last_message_received + 1);
  for (int i = 0; i < n; i++) {
    String text = bot->messages[i].text;
    String chat = bot->messages[i].chat_id;
    text.toLowerCase();

    if (chat != tgChatId) { bot->sendMessage(chat, "No autorizado", ""); continue; }

    // Canales ON/OFF
    bool processed = false;
    for (int ch = 0; ch < 4; ch++) {
      if (text == "on" + String(ch + 1)) {
        if (!autoMode) { digitalWrite(RELAY_PINS[ch], HIGH); relayStatus[ch] = true; bot->sendMessage(chat, "✅ CH" + String(ch + 1) + " ON", ""); }
        else bot->sendMessage(chat, "❌ Modo AUTO activo", "");
        processed = true; break;
      }
      if (text == "off" + String(ch + 1)) {
        if (!autoMode) { digitalWrite(RELAY_PINS[ch], LOW); relayStatus[ch] = false; bot->sendMessage(chat, "✅ CH" + String(ch + 1) + " OFF", ""); }
        else bot->sendMessage(chat, "❌ Modo AUTO activo", "");
        processed = true; break;
      }
    }

    if (!processed) {
      if (text == "auto")   { autoMode = true;  saveMode(); bot->sendMessage(chat, "🔄 AUTO", ""); }
      else if (text == "manual") { autoMode = false; saveMode(); bot->sendMessage(chat, "🔄 MANUAL", ""); }
      else if (text == "alloff") {
        for (int j = 0; j < 4; j++) { digitalWrite(RELAY_PINS[j], LOW); relayStatus[j] = false; }
        bot->sendMessage(chat, "🔴 Todos OFF", "");
      }
      else if (text == "status") {
        String s = "📊 Metzabook\n";
        s += autoMode ? "⚙️ AUTO\n" : "⚙️ MANUAL\n";
        for (int j = 0; j < 4; j++)
          s += "CH" + String(j + 1) + ": " + (relayStatus[j] ? "ON" : "OFF") + "\n";
        s += "W: " + String(WiFi.status() == WL_CONNECTED ? "ON" : "NO");
        bot->sendMessage(chat, s, "");
      }
    }
  }
}

/* =========================================================
   DISPLAY OLED 128x32
   Layout (text size 1 = 6x8px, ~21 chars/línea):

   Línea 0 (y=0):  [AUTO|MAN]  [RUN|CFG]   HH:MM:SS
   Línea 1 (y=11): 1:[ON|OF] 2:[ON|OF] 3:[ON|OF] 4:[ON|OF]
   Línea 2 (y=23): W:[ON|NO] BT:[ON|NO] TG:[OK|--]
   ========================================================= */
void updateDisplay() {
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);

  DateTime now = rtc.now();
  bool wifiOn  = (WiFi.status() == WL_CONNECTED);
  bool needCfg = (wifiSSID == "" || tgToken == "");

  /* ── Línea 0: Modo | Estado | Hora ── */
  display.setCursor(0, 0);
  display.print(autoMode ? "AUTO" : "MAN ");

  display.setCursor(30, 0);
  display.print(needCfg ? "CFG" : "RUN");

  display.setCursor(66, 0);
  display.printf("%02d:%02d:%02d", now.hour(), now.minute(), now.second());

  /* ── Línea 1: Estado de los 4 canales ── */
  // Cada canal: "1:ON " (5 chars) o "1:OF " (5 chars) → 20 chars total
  display.setCursor(0, 11);
  for (int i = 0; i < 4; i++) {
    display.print(String(i + 1));
    display.print(":");
    display.print(relayStatus[i] ? "ON" : "OF");
    if (i < 3) display.print(" ");
  }

  /* ── Línea 2: Conexiones ── */
  // "W:ON BT:ON TG:OK" → 17 chars
  display.setCursor(0, 23);
  display.print("W:");
  display.print(wifiOn ? "ON" : "NO");

  display.setCursor(36, 23);
  display.print("BT:");
  display.print(btActive ? "ON" : "NO");

  display.setCursor(78, 23);
  display.print("TG:");
  display.print(telegramOK ? "OK" : "--");

  display.display();
}

/* =========================================================
   BLUETOOTH — Comandos
   ========================================================= */
void sendStatus() {
  for (int i = 0; i < 4; i++)
    SerialBT.println("CH" + String(i + 1) + "=" + (relayStatus[i] ? "ON" : "OFF"));
  SerialBT.println("MODE:GLOBAL:" + String(autoMode ? "AUTO" : "MANUAL"));
  SerialBT.println("WIFI:" + String(WiFi.status() == WL_CONNECTED ? "ON" : "OFF"));
  SerialBT.println("TG:" + String(telegramOK ? "OK" : "NO"));
  SerialBT.println("BT:ON");
}

void processCommand(String cmd) {
  cmd.trim();
  cmd.toUpperCase();
  Serial.println("BT< " + cmd);

  /* STATUS */
  if (cmd == "STATUS") { sendStatus(); return; }

  /* MODO */
  if (cmd == "GLOBAL_AUTO")   { autoMode = true;  saveMode(); SerialBT.println("MODE:GLOBAL:AUTO");   tgSend("📱 AUTO por BT");   return; }
  if (cmd == "GLOBAL_MANUAL") { autoMode = false; saveMode(); SerialBT.println("MODE:GLOBAL:MANUAL"); tgSend("📱 MANUAL por BT"); return; }

  /* ALLOFF */
  if (cmd == "ALLOFF") {
    for (int i = 0; i < 4; i++) { digitalWrite(RELAY_PINS[i], LOW); relayStatus[i] = false; SerialBT.println("CH" + String(i + 1) + "=OFF"); }
    tgSend("🔴 EMERGENCIA: Todos OFF"); return;
  }

  /* SETTIME:HH:MM:SS:DD:MM:YYYY */
  if (cmd.startsWith("SETTIME:")) {
    int p[6]; int idx = 0; int start = 8;
    for (int i = 0; i < 6; i++) {
      int end = cmd.indexOf(':', start);
      if (end == -1) end = cmd.length();
      p[idx++] = cmd.substring(start, end).toInt();
      start = end + 1;
    }
    if (idx == 6) {
      rtc.adjust(DateTime(p[5], p[4], p[3], p[0], p[1], p[2]));
      SerialBT.println("TIME_SYNC_OK");
    }
    return;
  }

  /* GETSCHEDS */
  if (cmd == "GETSCHEDS") {
    for (int ch = 0; ch < 4; ch++)
      for (int s = 0; s < MAX_SCHEDS; s++)
        if (channelSchedules[ch][s].enabled) {
          SerialBT.printf("LSCHED:%d:%d:%d:%d:%d:%d:%d\n", ch + 1, s,
            channelSchedules[ch][s].daysMask,
            channelSchedules[ch][s].onHour,  channelSchedules[ch][s].onMinute,
            channelSchedules[ch][s].offHour, channelSchedules[ch][s].offMinute);
          delay(10);
        }
    SerialBT.println("SYNC_DONE");
    return;
  }

  /* SETSCHED:CH:IDX:MASK:ON_H:ON_M:OFF_H:OFF_M */
  if (cmd.startsWith("SETSCHED:")) {
    int p[7]; int idx = 0; int start = 9;
    for (int i = 0; i < 7; i++) {
      int end = cmd.indexOf(':', start);
      if (end == -1) end = cmd.length();
      p[idx++] = cmd.substring(start, end).toInt();
      start = end + 1;
    }
    if (idx == 7) {
      int ch = p[0] - 1, s = p[1];
      if (ch >= 0 && ch < 4 && s >= 0 && s < MAX_SCHEDS) {
        channelSchedules[ch][s] = { (uint8_t)p[3], (uint8_t)p[4], (uint8_t)p[5], (uint8_t)p[6], (uint8_t)p[2], true };
        saveSchedules();
        SerialBT.println("SCHED_SAVED:" + String(ch + 1) + ":" + String(s));
      }
    }
    return;
  }

  /* DIS_SCHED:CH:IDX */
  if (cmd.startsWith("DIS_SCHED:")) {
    int col = cmd.indexOf(':', 10);
    if (col > 0) {
      int ch = cmd.substring(10, col).toInt() - 1;
      int s  = cmd.substring(col + 1).toInt();
      if (ch >= 0 && ch < 4 && s >= 0 && s < MAX_SCHEDS) {
        channelSchedules[ch][s].enabled = false;
        saveSchedules();
        SerialBT.println("SCHED_DISABLED:" + String(ch + 1) + ":" + String(s));
      }
    }
    return;
  }

  /* CLEAR_SCHEDS:CH */
  if (cmd.startsWith("CLEAR_SCHEDS:")) {
    int ch = cmd.substring(13).toInt() - 1;
    if (ch >= 0 && ch < 4) {
      for (int s = 0; s < MAX_SCHEDS; s++) channelSchedules[ch][s].enabled = false;
      saveSchedules();
      SerialBT.println("SCHEDS_CLEARED:" + String(ch + 1));
    }
    return;
  }

  /* SET_WIFI:SSID:PASS */
  if (cmd.startsWith("SET_WIFI:")) {
    String rest = cmd.substring(9);
    int sep = rest.indexOf(':');
    if (sep > 0) {
      wifiSSID = rest.substring(0, sep);
      wifiPass = rest.substring(sep + 1);
      preferences.begin("wifi", false);
      preferences.putString("ssid", wifiSSID);
      preferences.putString("pass", wifiPass);
      preferences.end();
      SerialBT.println("WiFi guardado: " + wifiSSID);
      delay(400);
      SerialBT.println("Conectando a WiFi...");
      connectWiFi();
      if (!telegramOK) setupTelegram();
    }
    return;
  }

  /* SET_TELEGRAM:TOKEN:CHATID */
  if (cmd.startsWith("SET_TELEGRAM:")) {
    String rest = cmd.substring(13);
    int sep = rest.indexOf(':');
    if (sep > 0) {
      tgToken  = rest.substring(0, sep);
      tgChatId = rest.substring(sep + 1);
      preferences.begin("tg", false);
      preferences.putString("token", tgToken);
      preferences.putString("chat",  tgChatId);
      preferences.end();
      SerialBT.println("Telegram guardado");
      setupTelegram();
    }
    return;
  }

  /* CONNECT_WIFI */
  if (cmd == "CONNECT_WIFI") {
    if (wifiSSID != "") { SerialBT.println("Conectando a WiFi..."); connectWiFi(); if (!telegramOK) setupTelegram(); }
    else SerialBT.println("ERROR: WiFi no configurado");
    return;
  }

  /* ON1-4 / OFF1-4 */
  for (int i = 0; i < 4; i++) {
    if (cmd == "ON" + String(i + 1)) {
      if (!autoMode) { digitalWrite(RELAY_PINS[i], HIGH); relayStatus[i] = true; SerialBT.println("CH" + String(i + 1) + "=ON"); tgSend("📱 CH" + String(i + 1) + " ON"); }
      else SerialBT.println("ERROR: Modo AUTO activo");
      return;
    }
    if (cmd == "OFF" + String(i + 1)) {
      if (!autoMode) { digitalWrite(RELAY_PINS[i], LOW); relayStatus[i] = false; SerialBT.println("CH" + String(i + 1) + "=OFF"); tgSend("📱 CH" + String(i + 1) + " OFF"); }
      else SerialBT.println("ERROR: Modo AUTO activo");
      return;
    }
  }

  SerialBT.println("COMANDO DESCONOCIDO: " + cmd);
}

void handleBT() {
  if (!SerialBT.available()) return;
  String cmd = SerialBT.readStringUntil('\n');
  processCommand(cmd);
}

/* =========================================================
   HORARIOS AUTOMÁTICOS
   ========================================================= */
void checkSchedules() {
  if (!autoMode) return;
  DateTime now = rtc.now();
  int curMin = now.hour() * 60 + now.minute();
  int curDay = now.dayOfTheWeek();

  for (int ch = 0; ch < 4; ch++) {
    bool shouldOn = false;
    for (int s = 0; s < MAX_SCHEDS; s++) {
      Schedule &sch = channelSchedules[ch][s];
      if (!sch.enabled) continue;
      if (!(sch.daysMask & (1 << curDay))) continue;
      int on  = sch.onHour  * 60 + sch.onMinute;
      int off = sch.offHour * 60 + sch.offMinute;
      if (on < off) { if (curMin >= on && curMin < off) { shouldOn = true; break; } }
      else           { if (curMin >= on || curMin < off) { shouldOn = true; break; } }
    }
    if (shouldOn != lastSchedState[ch]) {
      digitalWrite(RELAY_PINS[ch], shouldOn ? HIGH : LOW);
      relayStatus[ch] = shouldOn;
      lastSchedState[ch] = shouldOn;
      String msg = "⏰ CH" + String(ch + 1) + " " + (shouldOn ? "ON" : "OFF");
      Serial.println(msg);
      tgSend(msg);
    }
  }
}

/* =========================================================
   SETUP & LOOP
   ========================================================= */
void setup() {
  Serial.begin(115200);
  Serial.println("\n=== METZABOOK ESP32 ===");

  Wire.begin(21, 22);

  // OLED
  if (!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) {
    Serial.println("OLED FAIL");
  } else {
    display.clearDisplay();
    display.setTextSize(1);
    display.setTextColor(SSD1306_WHITE);
    display.setCursor(0, 0);
    display.println("Metzabook v2.0");
    display.setCursor(0, 12);
    display.println("Iniciando...");
    display.display();
  }

  // RTC
  if (!rtc.begin()) Serial.println("RTC FAIL");

  // Relés
  for (int i = 0; i < 4; i++) {
    pinMode(RELAY_PINS[i], OUTPUT);
    digitalWrite(RELAY_PINS[i], LOW);
    lastSchedState[i] = false;
  }

  // Botón BT
  pinMode(BTN_BT, INPUT_PULLUP);

  // Bluetooth
  SerialBT.begin(BT_NAME);
  SerialBT.setTimeout(50);
  Serial.println("BT: " + String(BT_NAME));

  // Cargar configuración y conectar
  loadAll();

  if (wifiSSID != "") connectWiFi();
  if (tgToken  != "") setupTelegram();
}

void loop() {
  handleBT();
  checkTelegram();

  // Display cada 500ms
  if (millis() - lastDisplayUpdate >= 500) {
    updateDisplay();
    lastDisplayUpdate = millis();
  }

  // Horarios cada 60s
  if (millis() - lastScheduleCheck >= 60000) {
    checkSchedules();
    lastScheduleCheck = millis();
  }

  delay(10);
}