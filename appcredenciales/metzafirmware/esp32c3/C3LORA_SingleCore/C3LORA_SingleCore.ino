#include "Globals.h"
#include "WiFiManager.h"
#include "LoRaManager.h"
#include "WebInterface.h"
#include <BLEDevice.h>
#include <BLEServer.h>
#include <BLEUtils.h>
#include <BLE2902.h>

// BLE
BLECharacteristic *pCharacteristic = nullptr;
bool deviceConnected = false;

class MyServerCallbacks : public BLEServerCallbacks {
  void onConnect(BLEServer* pServer) { deviceConnected = true; }
  void onDisconnect(BLEServer* pServer) { 
    deviceConnected = false; 
    pServer->getAdvertising()->start();
  }
};

class MyCharCallbacks : public BLECharacteristicCallbacks {
  void onWrite(BLECharacteristic* pChar) {
    String value = pChar->getValue();
    if (value.length() > 0) {
      Serial.print("BLE RX: "); Serial.println(value);
      sendLoRaToSlave(value.c_str());
    }
  }
};

void setupBLE() {
  BLEDevice::init("Metzabook-Gateway");
  BLEServer *pServer = BLEDevice::createServer();
  pServer->setCallbacks(new MyServerCallbacks());
  BLEService *pService = pServer->createService("4fafc201-1fb5-459e-8fcc-c5c9c331914b");
  pCharacteristic = pService->createCharacteristic(
    "beb5483e-36e1-4688-b7f5-ea07361b26a8",
    BLECharacteristic::PROPERTY_READ | BLECharacteristic::PROPERTY_WRITE | BLECharacteristic::PROPERTY_NOTIFY
  );
  pCharacteristic->setCallbacks(new MyCharCallbacks());
  pCharacteristic->addDescriptor(new BLE2902());
  pService->start();
  pServer->getAdvertising()->start();
  Serial.println("✅ BLE Listo");
}

void setup() {
  Serial.begin(115200);
  delay(1000);
  
  pinMode(LED_PIN, OUTPUT);

  setupLoRa();
  setupWiFiAP();
  setupmDNS();
  initEEPROM();
  setupWebServer();
  setupBLE();
  
  Serial.println("\n🚀 Gateway Metzabook Iniciado");
}

void loop() {
  server.handleClient();
  receiveLoRa();
  
  // Heartbeat LED
  digitalWrite(LED_PIN, (millis() % 1000 < 100));

  // Si hay respuesta LoRa y BLE conectado, notificar a la app
  if (deviceConnected && lastLoRaResponse.length() > 0) {
    pCharacteristic->setValue(lastLoRaResponse.c_str());
    pCharacteristic->notify();
    lastLoRaResponse = ""; // Limpiar tras notificar
  }
}
