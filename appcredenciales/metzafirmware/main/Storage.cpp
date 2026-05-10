#include "Storage.h"

void saveSchedules() {
  preferences.begin("scheds", false);
  preferences.putBytes("data", (uint8_t*)channelSchedules, sizeof(channelSchedules));
  preferences.end();
  Serial.println("Schedules guardados en NVS");
}

void loadSchedules() {
  preferences.begin("scheds", true);
  size_t read = preferences.getBytes("data", (uint8_t*)channelSchedules, sizeof(channelSchedules));
  preferences.end();

  if (read != sizeof(channelSchedules)) {
    Serial.println("Inicializando schedules por defecto...");
    for (int i = 0; i < 4; i++) {
      for (int j = 0; j < MAX_SCHEDS; j++) {
        channelSchedules[i][j] = {0, 0, 0, 0, 0, false};
      }
    }
    saveSchedules();
  } else {
    Serial.println("Schedules cargados de NVS");
  }
}

void saveMode() {
  preferences.begin("modes", false);
  preferences.putBool("globalAuto", autoMode);
  preferences.end();
}

void loadMode() {
  preferences.begin("modes", true);
  autoMode = preferences.getBool("globalAuto", false);
  preferences.end();
  Serial.printf("Global Mode: %s\n", autoMode ? "AUTO" : "MANUAL");
}

void saveRelayNames() {
  preferences.begin("rnames", false);
  for(int i=0; i<4; i++) {
    char key[10];
    snprintf(key, sizeof(key), "n%d", i);
    preferences.putString(key, relayNames[i]);
  }
  preferences.end();
}

void loadRelayNames() {
  preferences.begin("rnames", true);
  for(int i=0; i<4; i++) {
    char key[10];
    snprintf(key, sizeof(key), "n%d", i);
    relayNames[i] = preferences.getString(key, "Relay " + String(i+1));
  }
  preferences.end();
}
