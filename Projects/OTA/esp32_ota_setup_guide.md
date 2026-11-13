# ESP32 Wireless & MQTT OTA Reference Guide

This reference guide covers the complete end-to-end workflow for setting up, securing, and remotely updating ESP32 firmware—both locally via ArduinoOTA and remotely via MQTT from an Android tablet running Termux.

---

## Table of Contents
1. [Project Structure & Secrets Management](#1-project-structure--secrets-management)
2. [Base Firmware Code (Wi-Fi, OTA & OLED)](#2-base-firmware-code-wi-fi-ota--oled)
3. [Initial USB Flash Procedure](#3-initial-usb-flash-procedure)
4. [Visual LED Status Reference ("Blind" Monitoring)](#4-visual-led-status-reference-blind-monitoring)
5. [Subsequent Local OTA Updates](#5-subsequent-local-ota-updates)
6. [Remote Updates via MQTT (Termux / Tablet)](#6-remote-updates-via-mqtt-termux--tablet)

---

## 1. Project Structure & Secrets Management

Never hardcode your credentials directly into your main sketch. Create a separate header file in your project directory.

### `secrets.h`
```cpp
#ifndef SECRETS_H
#define SECRETS_H

#define WIFI_SSID "Your_2.4GHz_SSID"
#define WIFI_PASSWORD "Your_WiFi_Password"
#define OTA_PASSWORD "SecureAdminPassword123"

#endif
```
*Note: Add `secrets.h` to your `.gitignore` file if using version control.*

---

## 2. Base Firmware Code (Wi-Fi, OTA & OLED)

Below is the complete, modularized template combining Wi-Fi connection logic, OTA handlers, blind LED feedback, and an SSD1306 128x64 I2C OLED display initialization.

```cpp
#include <WiFi.h>
#include <ArduinoOTA.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include "secrets.h"

// OLED display settings
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define OLED_RESET    -1
#define SCREEN_ADDRESS 0x3C

Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

void setupWiFi() {
  pinMode(LED_BUILTIN, OUTPUT);
  
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  
  int attempts = 0;
  while (WiFi.status() != WL_CONNECTED && attempts < 40) {
    digitalWrite(LED_BUILTIN, HIGH);
    delay(250);
    digitalWrite(LED_BUILTIN, LOW);
    delay(250);
    attempts++;
  }

  if (WiFi.status() == WL_CONNECTED) {
    digitalWrite(LED_BUILTIN, HIGH); // Solid ON = Connected successfully
  } else {
    while (true) {
      digitalWrite(LED_BUILTIN, HIGH);
      delay(100);
      digitalWrite(LED_BUILTIN, LOW);
      delay(100);
    }
  }
}

void setup() {
  // 1. Initialize Wi-Fi and blind monitor status
  setupWiFi();

  // 2. Initialize OTA parameters
  ArduinoOTA.setHostname("esp32-device");
  ArduinoOTA.setPassword(OTA_PASSWORD);
  ArduinoOTA.begin();

  // 3. Initialize the OLED display
  if(!display.begin(SSD1306_SWITCHCAPVCC, SCREEN_ADDRESS)) {
    while(1);
  }

  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(0, 0);
  display.println("ESP32 OTA Ready!");
  display.setCursor(0, 16);
  display.print("IP: ");
  display.println(WiFi.localIP());
  display.display();
}

void loop() {
  // CRITICAL: Keep this running continuously to listen for incoming wireless updates
  ArduinoOTA.handle();

  // Add your custom project loop logic here
}
```

---

## 3. Initial USB Flash Procedure

1. **Connect:** Plug your ESP32 board into your computer using a USB data cable.
2. **Select Port:** In your IDE, navigate to **Tools > Port** and select the correct COM port.
3. **Upload:** Compile and flash the initial firmware via USB.
4. **Verify:** Check that the build completes successfully and the device boots.

---

## 4. Visual LED Status Reference ("Blind" Monitoring)

If your board is deployed without a physical USB serial monitor attached:

* **Solid ON:** Wi-Fi connection successful; device is fully connected and ready.
* **Fast Blinking:** Connection failed or timed out. (Verify that your router is broadcasting a 2.4 GHz band, as the ESP32 does not support 5 GHz).

---

## 5. Subsequent Local OTA Updates

Once the initial USB flash is complete, you can push future updates entirely over your local Wi-Fi network:

1. Ensure your computer is connected to the same local Wi-Fi network as the ESP32.
2. In the Arduino IDE under **Tools > Port**, select `esp32-device at <IP_Address>` (network port).
3. Click **Upload**.
4. Enter your `OTA_PASSWORD` when prompted.

---

## 6. Remote Updates via MQTT (Termux / Tablet)

To push binary updates remotely from an Android tablet running Termux via an MQTT trigger:

### A. ESP32 MQTT Listener Snippet
Integrate this update handler alongside your Wi-Fi and MQTT client loop:
```cpp
#include <Update.h>
#include <HTTPClient.h>
#include <PubSubClient.h>

void performOTA(const char* firmwareUrl) {
  HTTPClient http;
  http.begin(firmwareUrl);
  int httpCode = http.GET();

  if (httpCode == HTTP_CODE_OK) {
    int contentLength = http.getSize();
    if (Update.begin(contentLength)) {
      WiFiClient* clientStream = http.getStreamPtr();
      size_t written = Update.writeStream(*clientStream);
      if (written == contentLength && Update.end() && Update.isFinished()) {
        ESP.restart(); // Reboot into updated firmware
      }
    }
  }
  http.end();
}
```

### B. Termux Setup & Compilation Pipeline
1. Install tools in Termux:
   ```bash
   pkg update && pkg upgrade
   pkg install python python-pip mosquitto-clients git
   curl -fsSL https://raw.githubusercontent.com/arduino/arduino-cli/master/install.sh | sh
   ```
2. Compile your sketch into a `.bin` file locally on your tablet:
   ```bash
   arduino-cli compile --fqbn esp32:esp32:esp32 /path/to/your/sketch
   ```
3. Start a local HTTP server in the directory containing your compiled binary:
   ```bash
   python3 -m http.server 8080
   ```
4. Publish the update trigger message via MQTT to your broker:
   ```bash
   mosquitto_pub -h <MQTT_BROKER_IP> -t "esp32/ota/update" -m "http://<TABLET_IP>:8080/sketch.ino.esp32.bin"
   ```