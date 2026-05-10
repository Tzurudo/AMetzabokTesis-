#include "Globals.h"
#include "Storage.h"
#include "UI.h"
#include "ScheduleManager.h"
#include "CommManager.h"
#include "InputHandler.h"

void setup() {
  Serial.begin(115200);
  Wire.begin(21, 22);
  
  setupButton();

  if(!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) {
    Serial.println("OLED failed");
  }
  display.clearDisplay();
  display.println("Iniciando...");
  display.display();

  if (!rtc.begin()) {
    Serial.println("RTC failed");
  }

  loadSchedules();
  loadMode();
  loadRelayNames();
  
  for(int i=0; i<4; i++) {
    pinMode(RELAY_PINS[i], OUTPUT);
    digitalWrite(RELAY_PINS[i], LOW);
  }

  // Por defecto iniciamos WiFi y LoRa
  setupLoRa();
  Serial.println("Modo Inicial: WiFi + LoRa");
}

void loop() {
  static unsigned long lastUpdate = 0;
  unsigned long now = millis();
  
  checkButton();

  if (bluetoothEnabled) {
    if (SerialBT.available()) {
      String command = SerialBT.readStringUntil('\n');
      command.trim();
      if (command.length() > 0) processCommand(command);
    }
  } else {
    handleWiFi();
    handleLoRa();
    if (currentWiFiStatus == WIFI_CONNECTED) {
      server.handleClient();
    }
  }

  if (now - lastUpdate >= 1000) {
    checkSchedules();
    updateDisplay();
    lastUpdate = now;
  }
}