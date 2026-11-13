#include <WiFi.h>
#include <WebServer.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include "DHT.h"

// --- Wi-Fi Credentials ---
const char* ssid = "DBANK_WIFI";
const char* password = "D@@nn33ll1234";

// --- Sensor Configuration ---
#define DHTPIN 4
#define DHTTYPE DHT11
DHT dht(DHTPIN, DHTTYPE);

// --- OLED Configuration ---
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define OLED_RESET    -1
#define SCREEN_ADDRESS 0x3C
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

// Create web server on port 80
WebServer server(80);

// Global variables
float currentHumidity = 0.0;
float currentTempC = 0.0;
float currentTempF = 0.0;
String customMessage = "Hello from Browser!";
unsigned long lastSensorUpdate = 0;
const long sensorInterval = 2000;

void handleRoot() {
  String html = "<!DOCTYPE html><html><head>";
  html += "<title>ESP32 OLED Text Controller</title>";
  html += "<style>body{font-family:Arial,sans-serif; text-align:center; margin-top:40px; background:#f4f4f9;}";
  html += ".card{background:white; padding:20px; border-radius:10px; display:inline-block; box-shadow:0 4px 8px rgba(0,0,0,0.1); width:320px;}";
  html += "input[type=text]{width:85%; padding:8px; margin:10px 0; border:1px solid #ccc; border-radius:4px;}";
  html += "input[type=submit]{background:#4CAF50; color:white; border:none; padding:10px 15px; border-radius:4px; cursor:pointer;}";
  html += "input[type=submit]:hover{background:#45a049;}</style></head><body>";
  html += "<div class='card'>";
  html += "<h2>ESP32 OLED Controller</h2>";
  
  if (!isnan(currentTempC)) {
    html += "<p><strong>Temp:</strong> " + String(currentTempC, 1) + " &deg;C | <strong>Hum:</strong> " + String(currentHumidity, 1) + "%</p>";
  }
  
  html += "<form action='/set' method='GET'>";
  html += "<input type='text' name='msg' placeholder='Type message for OLED...' maxlength='30' value='" + customMessage + "'>";
  html += "<br><input type='submit' value='Send to OLED'>";
  html += "</form>";
  html += "</div></body></html>";

  server.send(200, "text/html", html);
}

void handleSet() {
  if (server.hasArg("msg")) {
    customMessage = server.arg("msg");
  }
  // Redirect back to the main page
  server.sendHeader("Location", "/");
  server.send(303);
}

void updateSensorAndDisplay() {
  unsigned long currentMillis = millis();
  if (currentMillis - lastSensorUpdate >= sensorInterval) {
    lastSensorUpdate = currentMillis;

    currentHumidity = dht.readHumidity();
    currentTempC = dht.readTemperature();
    currentTempF = dht.readTemperature(true);

    // Update OLED Screen
    display.clearDisplay();
    display.setTextSize(1);
    display.setTextColor(SSD1306_WHITE);

    // Header Line
    display.setCursor(0, 0);
    display.println(F("ESP32 Web Monitor"));
    display.drawLine(0, 10, 128, 10, SSD1306_WHITE);

    // Sensor Readings Line
    display.setCursor(0, 14);
    if (!isnan(currentTempC)) {
      display.print(F("T: ")); display.print(currentTempC, 1); display.print((char)247); display.print(F("C "));
      display.print(F("H: ")); display.print(currentHumidity, 0); display.println(F("%"));
    } else {
      display.println(F("Sensor Error!"));
    }

    display.drawLine(0, 26, 128, 26, SSD1306_WHITE);

    // Custom Web Message Section
    display.setCursor(0, 32);
    display.println(F("Web Message:"));
    display.setCursor(0, 44);
    display.setTextSize(1);
    display.println(customMessage);

    display.display();
  }
}

void setup() {
  Serial.begin(115200);
  delay(1000);

  dht.begin();

  if(!display.begin(SSD1306_SWITCHCAPVCC, SCREEN_ADDRESS)) {
    Serial.println(F("SSD1306 allocation failed. Check wiring!"));
    for(;;);
  }
  
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(0, 20);
  display.println(F("Connecting WiFi..."));
  display.display();

  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  
  Serial.println("\nWiFi connected.");
  Serial.print("IP Address: http://");
  Serial.println(WiFi.localIP());

  // Define web server routes
  server.on("/", handleRoot);
  server.on("/set", handleSet);
  server.begin();
  Serial.println("HTTP server started");
}

void loop() {
  server.handleClient();
  updateSensorAndDisplay();
}