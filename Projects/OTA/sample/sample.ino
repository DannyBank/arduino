#include <WiFi.h>
#include <ArduinoOTA.h>

const char* ssid = "DBANK_WIFI";
const char* password = "D@@nn33ll1234";

void setup() {
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
  }

  // Set optional hostname and authentication password
  ArduinoOTA.setHostname("esp32-main");
  ArduinoOTA.setPassword("D@@nn33ll1234");

  ArduinoOTA.begin();
}

void loop() {
  // MUST be called frequently to listen for incoming wireless updates
  ArduinoOTA.handle(); 
}