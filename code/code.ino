#include <Wire.h>
#include <Adafruit_PN532.h>
#include <WiFi.h>
#include <WiFiManager.h>
#include <Preferences.h>
#include <HTTPClient.h>

// PN532 setup
#define SDA_PIN 21 
#define SCL_PIN 22 
Adafruit_PN532 nfc(SDA_PIN, SCL_PIN);

// WiFiManager setup
WiFiManager wifiManager;
Preferences preferences;
char scannerId[40];
WiFiManagerParameter custom_scan_id("scan", "Scanner ID", scannerId, 40);

// Server URL
const char *server = "https://sam.requestcatcher.com/";

// Buzzer on pin 13
const int buzzerPin = 13;

// Buzzer beeping
void beep(int amount)
{
  for (int i = 0; i < amount; i++)
  {
    digitalWrite(buzzerPin, HIGH);
    delay(50);
    digitalWrite(buzzerPin, LOW);
    delay(50);
  }
}

void errorBeep()
{
  digitalWrite(buzzerPin, HIGH);
  delay(500);
  digitalWrite(buzzerPin, LOW);
}

// Save config from WiFiManager GUI
void saveConfigCallback()
{
  Serial.println("Should save config");
  String newValue = custom_scan_id.getValue();
  newValue.toCharArray(scannerId, sizeof(scannerId));

  preferences.begin("settings", false);
  preferences.putString("scannerId", scannerId);
  preferences.end();
}

void setup(void)
{
  Serial.begin(115200);

  // Setup WiFi AP
  String mac = String(WiFi.macAddress());
  String SSID = "S.A.M_" + mac;
  delay(10);
  wifiManager.addParameter(&custom_scan_id);
  wifiManager.setSaveConfigCallback(saveConfigCallback);
  wifiManager.autoConnect(SSID.c_str());

  // Get & set Scanner ID from GUI
  preferences.begin("settings", true);
  String storedScannerId = preferences.getString("scannerId", "");
  storedScannerId.toCharArray(scannerId, sizeof(scannerId));
  preferences.end();

  // Print WiFi & Scanner ID config
  Serial.println("");
  Serial.println("WiFi connected");
  Serial.println("IP address:");
  Serial.println(WiFi.localIP());
  Serial.print("Scanner ID: ");
  Serial.println(scannerId);

  // Start NFC reader
  nfc.begin();
  uint32_t versiondata = nfc.getFirmwareVersion();
  if (!versiondata)
  {
    Serial.print("Didn't find PN53x board");
    while (1)
      ;
  }
  nfc.SAMConfig();
  Serial.println("Waiting for an NFC card ...");

  // Initialize the buzzer pin as output
  pinMode(buzzerPin, OUTPUT);

  beep(1);
}

void loop(void)
{
  uint8_t success;
  uint8_t uid[] = {0, 0, 0, 0, 0, 0, 0};
  uint8_t uidLength;

  success = nfc.readPassiveTargetID(PN532_MIFARE_ISO14443A, uid, &uidLength);

  if (success)
  {
    beep(1);

    // Reverse the UID bytes
    for (uint8_t i = 0; i < uidLength / 2; i++)
    {
      uint8_t temp = uid[i];
      uid[i] = uid[uidLength - 1 - i];
      uid[uidLength - 1 - i] = temp;
    }

    // Convert reversed UID to decimal
    uint64_t uid_decimal = 0;
    for (uint8_t i = 0; i < uidLength; i++)
    {
      uid_decimal <<= 8;     // Shift left by one byte
      uid_decimal |= uid[i]; // Add the current byte
    }

    Serial.print("Reversed UID in Decimal: ");
    Serial.println((unsigned long long)uid_decimal);

    // Send UID to the server
    if (WiFi.status() == WL_CONNECTED)
    {
      HTTPClient http;
      http.begin(server);
      http.addHeader("Content-Type", "application/json");
      http.addHeader("scanner-id", scannerId);
      http.addHeader("card-id", String((unsigned long long)uid_decimal));

      int httpCode = http.POST("");

      Serial.println(httpCode);

      http.end();

      if (httpCode == 200)
      {
        beep(2);
      }

      else if (httpCode == 404)
      {
        errorBeep();
      }

      Serial.println(httpCode);
    }

    // Wait a bit before scanning again
    delay(1000);
  }
  else
  {
    errorBeep();
  }
}
