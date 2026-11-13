#include <WiFi.h>
#include <ArduinoOTA.h>
#include "secrets.h"

#define LED_BUILTIN 2

void setup() {
  pinMode(LED_BUILTIN, OUTPUT);
  
  // Begin Wi-Fi connection using secrets
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  
  int attempts = 0;
  // Try connecting for 20 seconds (40 iterations * 500ms)
  while (WiFi.status() != WL_CONNECTED && attempts < 40) {
    digitalWrite(LED_BUILTIN, HIGH);
    delay(250);
    digitalWrite(LED_BUILTIN, LOW);
    delay(250);
    attempts++;
  }

  // Check if connection succeeded
  if (WiFi.status() == WL_CONNECTED) {
    digitalWrite(LED_BUILTIN, HIGH); // Solid ON = Connected successfully
  } else {
    // Fast blink loop = Connection failed / Timed out
    while (true) {
      digitalWrite(LED_BUILTIN, HIGH);
      delay(100);
      digitalWrite(LED_BUILTIN, LOW);
      delay(100);
    }
  }

  // Configure OTA parameters
  ArduinoOTA.setHostname("esp32-device");
  ArduinoOTA.setPassword(OTA_PASSWORD);

  ArduinoOTA.begin();
}

void loop() {
  // Must be called continuously to listen for wireless updates
  ArduinoOTA.handle();
}