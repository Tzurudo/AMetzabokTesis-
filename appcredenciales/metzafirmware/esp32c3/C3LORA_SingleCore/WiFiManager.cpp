#include "WiFiManager.h"
#include <WiFi.h>
#include <ESPmDNS.h>

void setupWiFiAP() {
  WiFi.mode(WIFI_AP);
  WiFi.softAPConfig(IPAddress(192, 168, 4, 1), IPAddress(192, 168, 4, 1), IPAddress(255, 255, 255, 0));
  WiFi.softAP(AP_SSID, AP_PASS);
  Serial.print("✅ AP Iniciado: ");
  Serial.println(AP_SSID);
  Serial.print("   IP: ");
  Serial.println(WiFi.softAPIP());
}

void setupmDNS() {
  if (MDNS.begin("metzabok")) {
    Serial.println("✅ mDNS: http://metzabok.local");
  }
}
