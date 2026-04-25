#include "BluetoothSerial.h"
#include <WiFi.h>
#include <WebServer.h>
#include <ESPmDNS.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <RTClib.h>
#include <Preferences.h>

/* ─── Pines / constantes ─── */
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 32
#define OLED_RESET   -1
#define BTN_D4        4

const int RELAY_PINS[]  = {32, 33, 25, 26};
const char* BT_NAME     = "Metzabook_ESP32";
const int MAX_SCHEDS    = 5;

// Tiempo sin conexión BT antes de pasar a WiFi
const unsigned long BT_WAIT_MS = 10000UL; 

/* ─── Modos ─── */
enum OpMode { MODE_BT, MODE_WIFI };
OpMode currentMode = MODE_BT;

/* ─── Objetos ─── */
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);
RTC_DS3231 rtc;
BluetoothSerial SerialBT;
Preferences prefs;
WebServer server(80);

/* ─── Variables ─── */
String wifiSSID, wifiPass;
bool relayStatus[4] = {false};
bool autoMode       = false;

struct Schedule {
  uint8_t onHour, onMinute, offHour, offMinute, daysMask;
  bool enabled;
};
Schedule scheds[4][MAX_SCHEDS];
bool lastSchedState[4] = {false};

// Timers
unsigned long lastDisplay   = 0;
unsigned long lastSchedChk  = 0;
unsigned long btNoClientAt  = 0;   
bool btClientEver           = false; 

// Botón triple-press
int  btnCount       = 0;
bool btnLast        = HIGH;
unsigned long btnT  = 0;
const unsigned long TRIPLE_WIN = 1500;

/* =========================================================
   PERSISTENCIA
   ========================================================= */
void loadAll() {
  prefs.begin("wifi", true);
  wifiSSID = prefs.getString("ssid", "");
  wifiPass = prefs.getString("pass", "");
  prefs.end();

  prefs.begin("cfg", true);
  autoMode = prefs.getBool("auto", false);
  prefs.end();

  prefs.begin("scheds", true);
  if (prefs.getBytesLength("data") == sizeof(scheds)) {
    prefs.getBytes("data", scheds, sizeof(scheds));
  } else {
    for (int i = 0; i < 4; i++)
      for (int j = 0; j < MAX_SCHEDS; j++)
        scheds[i][j].enabled = false;
  }
  prefs.end();
}

void saveMode() {
  prefs.begin("cfg", false);
  prefs.putBool("auto", autoMode);
  prefs.end();
}

void saveScheds() {
  prefs.begin("scheds", false);
  prefs.putBytes("data", scheds, sizeof(scheds));
  prefs.end();
}

/* =========================================================
   DISPLAY  (128x32)
   ========================================================= */
void updateDisplay() {
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);

  DateTime now = rtc.now();

  // Renglón 1: Modo y tiempo
  display.setCursor(0, 0);
  display.print("Mod:");
  display.print(autoMode ? "Auto" : "Man ");
  display.print(" Time:");
  char timeBuf[6];
  sprintf(timeBuf, "%02d:%02d", now.hour(), now.minute());
  display.print(timeBuf);

  // Renglón 2: Estados de outputs (focos)
  display.setCursor(0, 10);
  display.print("0ut: ");
  for (int i = 0; i < 4; i++) {
    display.print(relayStatus[i] ? "1" : "0");
    if (i < 3) display.print(" ");
  }

  // Renglón 3: Estados de WF y BT
  display.setCursor(0, 20);
  display.print("WF: ");
  display.print(WiFi.status() == WL_CONNECTED ? "ON" : "OFF");
  display.print(" BT: ");
  display.print(currentMode == MODE_BT && SerialBT.connected() ? "ON" : "OFF");

  // Renglón 4: Reservado para futuras expansiones
  display.setCursor(0, 30);
  // Dejar vacío por ahora

  display.display();
}

/* =========================================================
   WEB SERVER
   ========================================================= */
void setupWebServer() {
  server.on("/status", []() {
    String json = "{\"mode\":\"" + String(autoMode ? "auto" : "manual") + "\", \"relays\":[";
    for(int i=0; i<4; i++) {
      json += String(relayStatus[i] ? "true" : "false");
      if(i < 3) json += ",";
    }
    json += "]}";
    server.send(200, "application/json", json);
  });

  for (int i=0; i<4; i++) {
    String pathOn = "/foco" + String(i+1) + "/on";
    String pathOff = "/foco" + String(i+1) + "/off";
    server.on(pathOn.c_str(), [i]() { 
      if (!autoMode) {
        digitalWrite(RELAY_PINS[i], HIGH); 
        relayStatus[i] = true;
        server.send(200, "text/plain", "CH" + String(i+1) + "=ON"); 
      } else server.send(403, "text/plain", "AUTO MODE ACTIVE");
    });
    server.on(pathOff.c_str(), [i]() { 
      if (!autoMode) {
        digitalWrite(RELAY_PINS[i], LOW); 
        relayStatus[i] = false;
        server.send(200, "text/plain", "CH" + String(i+1) + "=OFF"); 
      } else server.send(403, "text/plain", "AUTO MODE ACTIVE");
    });
  }

  server.on("/alloff", []() {
    for (int i = 0; i < 4; i++) {
      digitalWrite(RELAY_PINS[i], LOW);
      relayStatus[i] = false;
    }
    server.send(200, "text/plain", "ALL OFF");
  });

  server.on("/mode/auto", []() { 
    autoMode = true; 
    saveMode(); 
    // Apagar todos los focos al cambiar a automático
    for (int i = 0; i < 4; i++) { 
      digitalWrite(RELAY_PINS[i], LOW); 
      relayStatus[i] = false; 
    }
    server.send(200, "text/plain", "AUTO"); 
  });
  server.on("/mode/manual", []() { autoMode = false; saveMode(); server.send(200, "text/plain", "MANUAL"); });

  server.begin();
  Serial.println("Web server started");
}

/* =========================================================
   WIFI
   ========================================================= */
void connectWiFi() {
  if (wifiSSID == "") return;
  
  WiFi.mode(WIFI_STA);
  WiFi.begin(wifiSSID.c_str(), wifiPass.c_str());
  Serial.print("Connecting WiFi: " + wifiSSID);
  
  unsigned long t = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - t < 15000) {
    delay(500); 
    Serial.print(".");
  }
  
  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("\nWiFi OK IP: " + WiFi.localIP().toString());
    if (MDNS.begin("metzabok")) {
      Serial.println("MDNS responder started: metzabok.local");
    }
    setupWebServer();
  } else {
    Serial.println("\nWiFi FAIL");
  }
}

/* =========================================================
   MODOS
   ========================================================= */
void activateWifiMode() {
  Serial.println("\n>> Switching to WiFi Mode");
  SerialBT.end(); 
  currentMode = MODE_WIFI;
  connectWiFi();
  updateDisplay();
}

void activateBTMode() {
  Serial.println("\n>> Switching to BT Mode");
  WiFi.disconnect(true); 
  WiFi.mode(WIFI_OFF);
  
  currentMode = MODE_BT;
  SerialBT.begin(BT_NAME);
  SerialBT.setTimeout(50);
  
  btNoClientAt  = millis(); 
  btClientEver  = false;
  
  updateDisplay();
}

/* =========================================================
   BOTÓN triple-press D4
   ========================================================= */
void handleButton() {
  bool st = digitalRead(BTN_D4);
  if (st == LOW && btnLast == HIGH) {
    unsigned long now = millis();
    if (now - btnT > TRIPLE_WIN) btnCount = 0;
    btnCount++;
    btnT = now;
    if (btnCount >= 3) {
      btnCount = 0;
      currentMode == MODE_BT ? activateWifiMode() : activateBTMode();
    }
  }
  btnLast = st;
}

/* =========================================================
   DETECCIÓN: 10s sin cliente BT → WiFi
   ========================================================= */
void checkBTTimeout() {
  if (currentMode != MODE_BT) return;
  bool connected = SerialBT.connected();

  if (connected) {
    btClientEver = true;
    btNoClientAt = millis(); 
  } else {
    if (millis() - btNoClientAt >= BT_WAIT_MS) {
      Serial.println("Timeout: 10s without BT -> Auto WiFi");
      activateWifiMode();
    }
  }
}

/* =========================================================
   BLUETOOTH — comandos
   ========================================================= */
void sendStatus() {
  for (int i = 0; i < 4; i++)
    SerialBT.println("CH"+String(i+1)+"="+(relayStatus[i]?"ON":"OFF"));
  SerialBT.println("MODE:GLOBAL:"+(String)(autoMode?"AUTO":"MANUAL"));
  SerialBT.println("WIFI:"+(String)(WiFi.status()==WL_CONNECTED?"ON":"OFF"));
  SerialBT.println("BT:ON");
}

void processCommand(String cmd) {
  cmd.trim();
  String original = cmd;
  int sep = cmd.indexOf(':');
  String keyword = (sep >= 0) ? cmd.substring(0, sep) : cmd;
  keyword.toUpperCase();
  String cmdUpper = (sep >= 0) ? keyword + original.substring(sep) : keyword;
  cmd = cmdUpper;

  if (cmd == "STATUS") { sendStatus(); return; }
  if (cmd == "GLOBAL_AUTO")   { 
    autoMode = true; 
    saveMode(); 
    // Apagar todos los focos al cambiar a automático para evitar interferencias
    for (int i = 0; i < 4; i++) { 
      digitalWrite(RELAY_PINS[i], LOW); 
      relayStatus[i] = false; 
      SerialBT.println("CH" + String(i + 1) + "=OFF");
    }
    SerialBT.println("MODE:GLOBAL:AUTO");   
    return; 
  }
  if (cmd == "GLOBAL_MANUAL") { autoMode=false; saveMode(); SerialBT.println("MODE:GLOBAL:MANUAL"); return; }

  if (cmd == "ALLOFF") {
    for (int i=0;i<4;i++) { digitalWrite(RELAY_PINS[i],LOW); relayStatus[i]=false; SerialBT.println("CH"+String(i+1)+"=OFF"); }
    return;
  }

  if (cmd.startsWith("SETTIME:")) {
    int p[6]={}, idx=0, s=8;
    for (int i=0;i<6;i++) { int e=cmd.indexOf(':',s); if(e<0)e=cmd.length(); p[idx++]=cmd.substring(s,e).toInt(); s=e+1; }
    if (idx==6) { rtc.adjust(DateTime(p[5],p[4],p[3],p[0],p[1],p[2])); SerialBT.println("TIME_SYNC_OK"); }
    return;
  }

  if (cmd == "GETSCHEDS") {
    for (int ch=0;ch<4;ch++) {
      for (int s=0;s<MAX_SCHEDS;s++) {
        if (scheds[ch][s].enabled) {
          SerialBT.print("LSCHED:");
          SerialBT.print(ch+1);
          SerialBT.print(":");
          SerialBT.print(s);
          SerialBT.print(":");
          SerialBT.print(scheds[ch][s].daysMask);
          SerialBT.print(":");
          SerialBT.print(scheds[ch][s].onHour);
          SerialBT.print(":");
          SerialBT.print(scheds[ch][s].onMinute);
          SerialBT.print(":");
          SerialBT.print(scheds[ch][s].offHour);
          SerialBT.print(":");
          SerialBT.println(scheds[ch][s].offMinute);
          delay(15);
        }
      }
    }
    SerialBT.println("SYNC_DONE"); 
    return;
  }

  if (cmd.startsWith("SETSCHED:")) {
    int p[7]={}, idx=0, s=9;
    for (int i=0;i<7;i++) { int e=cmd.indexOf(':',s); if(e<0)e=cmd.length(); p[idx++]=cmd.substring(s,e).toInt(); s=e+1; }
    if (idx==7) {
      int ch=p[0]-1, si=p[1];
      if (ch>=0&&ch<4&&si>=0&&si<MAX_SCHEDS) {
        scheds[ch][si]={(uint8_t)p[3],(uint8_t)p[4],(uint8_t)p[5],(uint8_t)p[6],(uint8_t)p[2],true};
        saveScheds(); 
        SerialBT.println("SCHED_SAVED:"+String(ch+1)+":"+String(si));
      }
    }
    return;
  }

  if (cmd.startsWith("DIS_SCHED:")) {
    int c=cmd.indexOf(':',10); 
    if(c>0){
      int ch=cmd.substring(10,c).toInt()-1;
      int si=cmd.substring(c+1).toInt();
      if(ch>=0&&ch<4&&si>=0&&si<MAX_SCHEDS){
        scheds[ch][si].enabled=false;
        saveScheds();
        SerialBT.println("SCHED_DISABLED:"+String(ch+1)+":"+String(si));
      }
    }
    return;
  }

  if (cmd.startsWith("SET_WIFI:")) {
    String r=cmd.substring(9); 
    int sep=r.indexOf(':');
    if(sep>0){
      wifiSSID=r.substring(0,sep);
      wifiPass=r.substring(sep+1);
      prefs.begin("wifi",false);
      prefs.putString("ssid",wifiSSID);
      prefs.putString("pass",wifiPass);
      prefs.end();
      SerialBT.println("WiFi saved: "+wifiSSID);
    }
    return;
  }

  for (int i=0;i<4;i++) {
    if (cmd=="ON"+String(i+1))  { 
      if(!autoMode){
        digitalWrite(RELAY_PINS[i],HIGH);
        relayStatus[i]=true; 
        SerialBT.println("CH"+String(i+1)+"=ON");
      } else SerialBT.println("ERROR:AUTO"); 
      return; 
    }
    if (cmd=="OFF"+String(i+1)) { 
      if(!autoMode){
        digitalWrite(RELAY_PINS[i],LOW); 
        relayStatus[i]=false;
        SerialBT.println("CH"+String(i+1)+"=OFF");
      } else SerialBT.println("ERROR:AUTO"); 
      return; 
    }
  }

  SerialBT.println("UNKNOWN:" + cmd);
}

void handleBT() {
  if (currentMode != MODE_BT || !SerialBT.available()) return;
  String cmd = SerialBT.readStringUntil('\n');
  processCommand(cmd);
}

/* =========================================================
   HORARIOS
   ========================================================= */
void checkSchedules() {
  if (!autoMode) return;
  DateTime now = rtc.now();
  int curMin = now.hour() * 60 + now.minute();
  int curDay = now.dayOfTheWeek(); 
  
  for (int ch=0; ch<4; ch++) {
    bool shouldBeOn = false;
    for (int s=0; s<MAX_SCHEDS; s++) {
      Schedule& sc = scheds[ch][s];
      if (!sc.enabled) continue;
      if (!(sc.daysMask & (1 << curDay))) continue;
      int onTime  = sc.onHour * 60 + sc.onMinute;
      int offTime = sc.offHour * 60 + sc.offMinute;
      if (onTime < offTime) {
        if (curMin >= onTime && curMin < offTime) { shouldBeOn = true; break; }
      } else if (onTime > offTime) {
        if (curMin >= onTime || curMin < offTime) { shouldBeOn = true; break; }
      }
    }
    
    if (shouldBeOn != lastSchedState[ch]) {
      digitalWrite(RELAY_PINS[ch], shouldBeOn ? HIGH : LOW); 
      relayStatus[ch] = shouldBeOn; 
      lastSchedState[ch] = shouldBeOn;
      if (currentMode == MODE_BT && SerialBT.connected()) {
        SerialBT.println("CH" + String(ch+1) + "=" + String(shouldBeOn ? "ON" : "OFF"));
      }
    }
  }
}

/* =========================================================
   SETUP & LOOP
   ========================================================= */
void setup() {
  Serial.begin(115200);
  Wire.begin(21, 22);

  if (!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) Serial.println("OLED FAIL");
  else {
    display.clearDisplay(); display.setTextSize(1); display.setTextColor(SSD1306_WHITE);
    display.setCursor(0,0);  display.println("Metzabook v2.0");
    display.setCursor(0,12); display.println("Modo: Bluetooth");
    display.display();
  }

  if (!rtc.begin()) Serial.println("RTC FAIL");

  for (int i=0;i<4;i++) { 
    pinMode(RELAY_PINS[i], OUTPUT); 
    digitalWrite(RELAY_PINS[i], LOW); 
  }
  pinMode(BTN_D4, INPUT_PULLUP);

  loadAll();

  WiFi.mode(WIFI_OFF);
  SerialBT.begin(BT_NAME);
  SerialBT.setTimeout(50);
  currentMode  = MODE_BT;
  btNoClientAt = millis();

  Serial.println("=== Metzabok Ready (mDNS: metzabok.local) ===");
}

void loop() {
  handleButton();

  if (currentMode == MODE_BT) {
    handleBT();
    checkBTTimeout();
  } else if (currentMode == MODE_WIFI) {
    server.handleClient();
  }

  if (millis() - lastSchedChk >= 30000) { 
    checkSchedules(); 
    lastSchedChk = millis(); 
  }

  if (millis() - lastDisplay >= 500) { 
    updateDisplay(); 
    lastDisplay = millis(); 
  }

  delay(10);
}