#include "InputHandler.h"

static unsigned long lastBtnPress = 0;
static int clickCount = 0;
static bool lastBtnState = HIGH;

void setupButton() {
  pinMode(BUTTON_PIN, INPUT_PULLUP);
}

void checkButton() {
  bool currentState = digitalRead(BUTTON_PIN);
  unsigned long now = millis();

  // Detección de flanco de bajada (presionado)
  if (currentState == LOW && lastBtnState == HIGH) {
    if (now - lastBtnPress > 50) { // Debounce
      clickCount++;
      lastBtnPress = now;
      Serial.printf("Click %d\n", clickCount);
    }
  }
  
  lastBtnState = currentState;

  // Si pasa más de 1 segundo sin clicks, evaluamos
  if (clickCount > 0 && (now - lastBtnPress > 1000)) {
    if (clickCount == 3) {
      bluetoothEnabled = !bluetoothEnabled;
      if (bluetoothEnabled) {
        WiFi.disconnect(true); // Apagar WiFi para modo BT único
        SerialBT.begin("Metzabook_ESP32");
        lastBTActivity = now; 
        Serial.println("MODO: Bluetooth Único");
      } else {
        SerialBT.end();
        Serial.println("MODO: WiFi + LoRa");
      }
    }
    clickCount = 0; // Reiniciar contador
  }

  // Lógica de retorno automático a WiFi/LoRa (10 seg de inactividad o desconexión)
  if (bluetoothEnabled) {
    bool hasClient = SerialBT.hasClient();
    if (!hasClient && (now - lastBTActivity > 10000)) {
      bluetoothEnabled = false;
      SerialBT.end();
      Serial.println("Timeout BT: Regresando a WiFi + LoRa");
    }
    if (hasClient) lastBTActivity = now; // Mantener vivo mientras haya cliente
  }
}
