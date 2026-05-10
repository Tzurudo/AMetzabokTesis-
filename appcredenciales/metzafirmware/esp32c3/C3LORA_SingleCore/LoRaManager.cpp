#include "LoRaManager.h"
#include <SPI.h>

void setupLoRa() {
  SPI.begin(PIN_SCK, PIN_MISO, PIN_MOSI, PIN_NSS);
  LoRa.setPins(PIN_NSS, PIN_RST, PIN_DIO0);

  if (!LoRa.begin(LORA_FREQ_HZ)) {
    Serial.println("❌ LoRa Init Falló");
    while (true);
  }

  LoRa.setTxPower(LORA_TX_POWER);
  LoRa.setSpreadingFactor(LORA_SPREAD_FACTOR);
  LoRa.setCodingRate4(LORA_CODING_RATE);
  LoRa.setSignalBandwidth(LORA_BANDWIDTH);
  Serial.println("✅ LoRa OK");
}

bool sendLoRaToSlave(const char* message) {
  if (!message || strlen(message) == 0) return false;
  
  LoRa.beginPacket();
  LoRa.write(SLAVE_ADDRESS);
  LoRa.write(GATEWAY_ADDRESS);
  LoRa.write((uint8_t)strlen(message));
  LoRa.print(message);
  
  bool success = LoRa.endPacket();
  Serial.print("📡 LoRa TX -> 0x02: ");
  Serial.println(message);
  return success;
}

void receiveLoRa() {
  int packetSize = LoRa.parsePacket();
  if (packetSize < 3) return;
  
  uint8_t dest = LoRa.read();
  uint8_t src = LoRa.read();
  uint8_t len = LoRa.read();
  
  if (dest != GATEWAY_ADDRESS && dest != 0xFF) return;
  
  char buffer[129] = {0};
  for (uint8_t i = 0; i < len && i < 128; i++) {
    buffer[i] = (char)LoRa.read();
  }
  
  if (src == SLAVE_ADDRESS) {
    lastLoRaResponse = String(buffer);
    lastResponseTime = millis();
    Serial.print("📡 LoRa RX <- 0x02: ");
    Serial.println(buffer);
  }
}
