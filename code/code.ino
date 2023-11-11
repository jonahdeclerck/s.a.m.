#include "secrets.h"

#include <Wire.h>
#include <WiFi.h>
#include <Adafruit_PN532.h>
#include <HTTPClient.h>
#include <CuteBuzzerSounds.h>

// PN532 setup
#define SDA_PIN 21
#define SCL_PIN 22
Adafruit_PN532 nfc(SDA_PIN, SCL_PIN);


// WiFi credentials

const char* ssid = WIFI_SSID;
const char* password = WIFI_PASSWORD;
const char* server = SERVER_URL;

// Buzzer on pin 13
const int buzzerPin = 13;

void setup(void) {
  Serial.begin(115200);

  // Start NFC reader
  nfc.begin();
  uint32_t versiondata = nfc.getFirmwareVersion();
  if (!versiondata) {
    Serial.print("Didn't find PN53x board");
    while (1);
  }
  nfc.SAMConfig();
  Serial.println("Waiting for an NFC card ...");

  // Connect to WiFi
  WiFi.begin(ssid, password);

  while (WiFi.status() != WL_CONNECTED) {
    delay(1000);
    Serial.print(".");
  }
  Serial.println("\nWiFi connected!");

  // Initialize the buzzer pin as output
  cute.init(buzzerPin);
  cute.play(S_BUTTON_PUSHED);
}

void loop(void) {
  uint8_t success;
  uint8_t uid[] = { 0, 0, 0, 0, 0, 0, 0 };
  uint8_t uidLength;

  success = nfc.readPassiveTargetID(PN532_MIFARE_ISO14443A, uid, &uidLength);

  if (success) {
    cute.play(S_CONNECTION);  
    // Reverse the UID bytes
    for (uint8_t i = 0; i < uidLength / 2; i++) {
      uint8_t temp = uid[i];
      uid[i] = uid[uidLength - 1 - i];
      uid[uidLength - 1 - i] = temp;
    }

    // Convert reversed UID to decimal
    uint64_t uid_decimal = 0;
    for (uint8_t i = 0; i < uidLength; i++) {
      uid_decimal <<= 8;  // Shift left by one byte
      uid_decimal |= uid[i];  // Add the current byte
    }

    // Send UID to the server
    if (WiFi.status() == WL_CONNECTED) {
      HTTPClient http;
      http.begin(server);

      http.addHeader("Content-Type", "application/json");
      String message = "{\"card_id\":\"" + String((unsigned long long)uid_decimal) + "\" , \"scanner_id\":" + 1 + "}";
      int httpCode = http.POST(message);
      http.end();
    }
  } else {
    cute.play(S_DISCONNECTION);
  }
}