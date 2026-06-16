#include <NimBLEDevice.h>

#define LED_PIN 2 // Built-in blue LED (Simulates the ignition relay)

#define SERVICE_UUID        "12345678-1234-1234-1234-123456789abc"
#define CHARACTERISTIC_UUID "87654321-4321-4321-4321-cba987654321"

// --- STATE VARIABLES ---
bool deviceConnected = false;
bool isUnlocked = false;
unsigned long unlockTime = 0;
const unsigned long AUTO_LOCK_DELAY = 30000; // 30 seconds in milliseconds

NimBLECharacteristic *pCharacteristic;

// --- CONNECTION MONITORING ---
class ServerCallbacks: public NimBLEServerCallbacks {
    void onConnect(NimBLEServer* pServer) {
      deviceConnected = true;
      Serial.println("\n[SYSTEM] Phone Connected!");
    }
    
    void onDisconnect(NimBLEServer* pServer) {
      deviceConnected = false;
      Serial.println("\n[SYSTEM] Phone Disconnected or Out of Range!");
      
      // Failsafe: Lock the bike immediately if connection drops
      if (isUnlocked) {
        isUnlocked = false;
        digitalWrite(LED_PIN, LOW);
        Serial.println("[SECURE] Auto-locked due to disconnect.");
      }
      
      NimBLEDevice::startAdvertising();
      Serial.println("[SYSTEM] Advertising restarted...");
    }
};

// --- COMMAND PROCESSING ---
class MyCallbacks: public NimBLECharacteristicCallbacks {
    void onWrite(NimBLECharacteristic *pChar) {
      String rxValue = pChar->getValue().c_str();

      // CRITICAL FIX: Clean the incoming text from MIT App Inventor
      rxValue.trim(); 
      
      // Print exactly what we received inside brackets to expose hidden spaces
      Serial.print("\n[BLUETOOTH] Received exactly: [");
      Serial.print(rxValue);
      Serial.println("]");

      // Use indexOf() to find the word, ignoring App Inventor's list brackets
      if (rxValue.indexOf("UNLOCK") != -1) {
        isUnlocked = true;
        unlockTime = millis(); // Start 30s timer
        digitalWrite(LED_PIN, HIGH); 
        Serial.println("[COMMAND] SUCCESS: Ignition UNLOCKED");
      } 
      else if (rxValue.indexOf("LOCK") != -1) {
        isUnlocked = false;
        digitalWrite(LED_PIN, LOW);  
        Serial.println("[COMMAND] SECURE: Ignition LOCKED manually");
      }
      else if (rxValue.indexOf("PING") != -1) {
        Serial.println("[COMMAND] PING received. Replying ONLINE.");
        pChar->setValue("ONLINE"); 
      }
    }
};

void setup() {
  Serial.begin(115200); 
  
  // Wait 2 seconds for the Serial Monitor to catch up
  delay(2000); 
  
  pinMode(LED_PIN, OUTPUT);
  digitalWrite(LED_PIN, LOW); 

  Serial.println("\nBooting Advanced FZ System...");
  
  // Initialize the NimBLE Server
  NimBLEDevice::init("FZ_Smart_Ignition");

  // --- DIAGNOSTIC: Verify the MAC Address ---
  Serial.print("[SYSTEM] My current MAC Address is: ");
  Serial.println(NimBLEDevice::getAddress().toString().c_str());
  // ------------------------------------------

  NimBLEServer *pServer = NimBLEDevice::createServer();
  
  pServer->setCallbacks(new ServerCallbacks());
  
  NimBLEService *pService = pServer->createService(SERVICE_UUID);
  
  pCharacteristic = pService->createCharacteristic(
                               CHARACTERISTIC_UUID,
                               NIMBLE_PROPERTY::WRITE | 
                               NIMBLE_PROPERTY::READ | 
                               NIMBLE_PROPERTY::NOTIFY
                             );

  pCharacteristic->setCallbacks(new MyCallbacks());
  pService->start();
  
  NimBLEAdvertising *pAdvertising = NimBLEDevice::getAdvertising();
  pAdvertising->addServiceUUID(SERVICE_UUID);
  pAdvertising->start();
  
  Serial.println("[SYSTEM] Ready. Waiting for app connection...");
}

void loop() {
  // --- AUTO-LOCK TIMER ---
  if (isUnlocked && deviceConnected) {
    if (millis() - unlockTime >= AUTO_LOCK_DELAY) {
      isUnlocked = false;
      digitalWrite(LED_PIN, LOW);
      Serial.println("\n[SECURE] 30-Second Timeout reached. Auto-Locked.");
    }
  }
  
  delay(10); // Keep loop stable
}