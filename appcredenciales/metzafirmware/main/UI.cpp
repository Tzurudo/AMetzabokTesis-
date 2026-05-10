#include "UI.h"

void updateDisplay() {
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);
  
  DateTime now = rtc.now();
  
  display.setCursor(0,0);
  char buf[25];
  snprintf(buf, sizeof(buf), "%02d:%02d:%02d [%s]", now.hour(), now.minute(), now.second(), autoMode ? "AUTO" : "MAN");
  display.println(buf);

  display.setCursor(0, 12);
  display.print("ST: ");
  for(int i=0; i<4; i++) {
    display.print(relayStatus[i] ? "I " : "O ");
  }

  display.setCursor(0, 24);
  const char* stWifi = (currentWiFiStatus == WIFI_CONNECTED) ? "ON" : 
                       (currentWiFiStatus == WIFI_CONNECTING) ? "..." : "OFF";
  const char* stBT = bluetoothEnabled ? "ON" : "OFF";
  display.printf("WiFi:%s BT:%s", stWifi, stBT);
  
  display.display();
}
