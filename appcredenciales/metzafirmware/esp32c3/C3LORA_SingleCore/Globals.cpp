#include "Globals.h"

const char* AP_SSID = "Metzabook-Gateway";
const char* AP_PASS = "12345678";

WebServer server(80);
String lastLoRaResponse = "";
unsigned long lastResponseTime = 0;

// Nombres por defecto de los relays
String relayNames[NUM_RELAYS] = {
  "Salida 1",
  "Salida 2",
  "Salida 3",
  "Salida 4"
};

void initEEPROM() {
  EEPROM.begin(EEPROM_SIZE);
  loadRelayNames();
}

void loadRelayNames() {
  for (int i = 0; i < NUM_RELAYS; i++) {
    String name = "";
    int addr = RELAY_NAMES_ADDR + (i * RELAY_NAME_LEN);
    
    for (int j = 0; j < RELAY_NAME_LEN; j++) {
      char c = EEPROM.read(addr + j);
      if (c == 0) break;
      name += c;
    }
    
    // Si está vacío, usa el nombre por defecto
    if (name.length() > 0) {
      relayNames[i] = name;
    }
  }
  Serial.println("[EEPROM] Nombres cargados desde EEPROM");
}

void saveRelayNames() {
  for (int i = 0; i < NUM_RELAYS; i++) {
    int addr = RELAY_NAMES_ADDR + (i * RELAY_NAME_LEN);
    
    // Limpia la sección de memoria
    for (int j = 0; j < RELAY_NAME_LEN; j++) {
      EEPROM.write(addr + j, 0);
    }
    
    // Guarda el nuevo nombre
    for (int j = 0; j < relayNames[i].length() && j < RELAY_NAME_LEN - 1; j++) {
      EEPROM.write(addr + j, relayNames[i][j]);
    }
    EEPROM.write(addr + RELAY_NAME_LEN - 1, 0); // Null terminator
  }
  
  EEPROM.commit();
  Serial.println("[EEPROM] Nombres guardados en EEPROM");
}

String getRelayName(int index) {
  if (index >= 0 && index < NUM_RELAYS) {
    return relayNames[index];
  }
  return "Salida " + String(index + 1);
}

void setRelayName(int index, String name) {
  if (index >= 0 && index < NUM_RELAYS) {
    if (name.length() > 0) {
      relayNames[index] = name;
    } else {
      relayNames[index] = "Salida " + String(index + 1);
    }
  }
}
