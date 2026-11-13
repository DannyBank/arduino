// -----------------------------------------------------------------------------
// IoTConnect Device Application
//
// Project   : MainController
// Generated : Automatically by IoTConnect
// Platform  : Arduino
// Copyright : 2026 IoTDevLab
// License   : MIT (SPDX-License-Identifier: MIT)
//
// Required drivers:
//   - HCSR04, Servo Motor
// -----------------------------------------------------------------------------
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <PubSubClient.h>
#include <ArduinoJson.h>
#include <ESP32Servo.h>

// Servo Object
Servo servo1;

// USER CONFIGURATION: update Wi-Fi credentials and hardware settings.
constexpr const char* HCSR04_1_POWERPIN = "3V3";
constexpr const char* HCSR04_1_GROUNDPIN = "GND";
constexpr int HCSR04_1_TRIGGERPIN = 13;
constexpr int HCSR04_1_ECHOPIN = 12;

constexpr const char* SERVO_MOTOR_1_POWERPIN = "3V3";
constexpr const char* SERVO_MOTOR_1_GROUNDPIN = "GND";
constexpr int SERVO_MOTOR_1_CONTROLPIN = 14;

const char* WIFI_SSID = "DBANK_WIFI";
const char* WIFI_PASSWORD = "D@@nn33ll1234";
const char* MQTT_HOST = "mqtt.iotworkshop.africa";
const int MQTT_PORT = 8883;
const char* MQTT_USER = "device_dev_jbbgU4tTLrgACkOk";
const char* MQTT_PASSWORD = "JUHsbQjuyDAA1Y6bO-ipxaVcOF63RGuD0zeqLpyTzhk";
const char* TELEMETRY_TOPIC = "dev_jbbgU4tTLrgACkOk/data";
const char* COMMAND_TOPIC = "dev_jbbgU4tTLrgACkOk/cmd";
const char* ROOT_CA = R"EOF(
-----BEGIN CERTIFICATE-----
MIIFTzCCAzegAwIBAgIUW2cDxTCTlyzl3FEHsxzcFnVVcZgwDQYJKoZIhvcNAQEL
BQAwNzEgMB4GA1UEAwwXSW9UQ29ubmVjdCBNUVRUIFJvb3QgQ0ExEzARBgNVBAoM
CklvVENvbm5lY3QwHhcNMjYwNzIxMjEwODE2WhcNMzYwNzE4MjEwODE2WjA3MSAw
HgYDVQQDDBdJb1RDb25uZWN0IE1RVFQgUm9vdCBDQTETMBEGA1UECgwKSW9UQ29u
bmVjdDCCAiIwDQYJKoZIhvcNAQEBBQADggIPADCCAgoCggIBAMIUJrFi4bpa314w
Zey63SzzAy4PHu+T2nVaXbuqavrfQtdlFfM6iAII6ISNuT9WPvXGPRNAfeJ0JzR/
NVKhNVWexhyYu9EMgE/+fTvsoalhAs2QMMr9no1RmxmrW04SyOLdWdtjhqh7xpcZ
PWFD+CrfgP0mGKMOsmcdUmAW+a7KG2bO0fylRPKcBvyo/nX6jDQt2mvK0omv8thQ
wdbaM/Sf4QHPTA7ZM2Vd8AIz75jlVfsQIKYNZt7xyGOL9igOtl16PBw16i1B9Vik
94eFJ1SGQLhDfynQWXOh0tcSClwIjbUjFRfxjFHt8fi6rPrdiwRecLX//GPDXDQ8
+UMXM2SpMLx0mLJGToJ5CaxjXF2OmD9UQ4oTe7wGshmxO3aj2fixRkMqNNS5Hj3l
bzbusZl1dMUzaZxXhVU2q2LIdemO/YVu45P8TMKbiOTdRvH/YwtGMuYIWdjg3vUL
H24ig0e7Lb1Ab/+CvPBilR1Qp63++BFLOqvHCJALgLfT7hsMXEABQ8uWJQ0gPayQ
HpkZQOcVEPd/ViVBuazCvh7w9/op7FBllhnaHZeL7b8INMAHH0wmgWYeZrD55H+R
qSmVFYlQrRp2rTEuVeph+WYBEUGlKdGjLXQQpuP7jUVHy1lF+eiLLeUFmCLzU2s0
NGdYSb+t/q4FFX4Tx6veb/OXIDDnAgMBAAGjUzBRMB0GA1UdDgQWBBR+bjbz+Tjr
LaLF035EoKnLJ/aWBTAfBgNVHSMEGDAWgBR+bjbz+TjrLaLF035EoKnLJ/aWBTAP
BgNVHRMBAf8EBTADAQH/MA0GCSqGSIb3DQEBCwUAA4ICAQDAmXCehI5LQaOFYOKi
Gne1Yc2VvLIW8vkYntMwcMVOaRtPSNE+tGjsqC/DOPmdY0kylFLRkjg7th3IqN9y
40gj19efA8Cxd0lZO7VFjwpvxYkW6NYWHbd2Y7dgy/D0hAOjLvLUmY7PF3/97P1K
/H3i1rW9gEVxO+xD0zyBe1Gn/P+X6CXEaDVR+obUk2pzB+iHTGC6XxTB5aLjeUbh
E+QXfhGjN63ccx7MPbh9W6NMANlbMpHMzWtT4LSnm3UBb2qgM8nJstwzYRPmZKwJ
l0BxsZFKkFrEld6S7LhU7Y6qQVmNO1wcS41Eldbf8YmOD9X9yC/aVYeF9xUe7vUI
zUCK3fmvWLUIX26LolMflYZfOVZUcI8MmqtoxsjS97i0aAWFz9LvNncA48pt1h46
hQslPTqEurXRkArGO7XL7bvWC3jPOrPGUwIg36JV/E59jDYO5zJngx5pwS20c5pG
vCbZfKR3qoX/Uc+haxav5BWxtqHrO25CNU5CQmFKBioS5A2YZaAYOS5FR5yLSxwB
5qkfHkiODM5uHncrCDyYFWBl4xxfxYzMxmMrXD92J+8W0EnQIMLpjjnXUPG/n44N
TeIJV1n3w3mhwpx4c7fZKCI+KwcJu48ezSonnMgPvwZP7Q/wqd3wCJdeIEtJwlX/
lm9gtTo4bhRLYjk4meM9lHjxvO==
-----END CERTIFICATE-----
)EOF";

WiFiClientSecure networkClient;
PubSubClient mqtt(networkClient);

void publishTelemetry() {
  // 1. Send trigger pulse to HC-SR04
  digitalWrite(HCSR04_1_TRIGGERPIN, LOW);
  delayMicroseconds(2);
  digitalWrite(HCSR04_1_TRIGGERPIN, HIGH);
  delayMicroseconds(10);
  digitalWrite(HCSR04_1_TRIGGERPIN, LOW);

  // 2. Measure echo pulse duration (30ms timeout)
  long duration = pulseIn(HCSR04_1_ECHOPIN, HIGH, 30000);

  // 3. Calculate distance in cm
  float distance = 0.0;
  if (duration > 0) {
    distance = (duration * 0.0343) / 2.0;
  }

  // 4. Automatic Servo Control based on Distance (0cm to 20cm)
  if (distance > 0.0 && distance <= 20.0) {
    // Map distance (20 down to 0) to servo angle (0 up to 180 degrees)
    // As distance gets closer to 0cm, angle increases towards 180°.
    int calculatedAngle = map((int)distance, 20, 0, 0, 180);
    calculatedAngle = constrain(calculatedAngle, 0, 180);
    
    servo1.write(calculatedAngle);
    
    Serial.print(F("Proximity trigger! Distance: "));
    Serial.print(distance);
    Serial.print(F(" cm -> Servo Angle: "));
    Serial.print(calculatedAngle);
    Serial.println(F("°"));
  } else if (distance > 20.0) {
    // Return servo to default position when object is far away
    servo1.write(90);
  }

  // 5. Build telemetry JSON payload
  JsonDocument document;
  document["components"]["hcsr04_1"]["distance"] = distance;
  document["components"]["servo_motor_1"]["angle"] = servo1.read();

  char payload[256];
  serializeJson(document, payload);

  // 6. Publish to MQTT topic
  mqtt.publish(TELEMETRY_TOPIC, payload);

  // 7. Debugging output to Serial Monitor
  Serial.print(F("Published telemetry - Distance: "));
  Serial.print(distance);
  Serial.println(F(" cm"));
}

void onCommand(char* topic, byte* payload, unsigned int length) {
  // Copy payload into a null-terminated string buffer
  String message = "";
  for (unsigned int i = 0; i < length; i++) {
    message += (char)payload[i];
  }

  // Debug incoming command
  Serial.print(F("Command received on topic: "));
  Serial.println(topic);
  Serial.print(F("Payload: "));
  Serial.println(message);

  JsonDocument doc;
  DeserializationError error = deserializeJson(doc, message);
  if (error) {
    Serial.print(F("deserializeJson() failed: "));
    Serial.println(error.f_str());
    return;
  }

  // Check if the command targets the servo motor
  const char* component = doc["component"];
  if (component != nullptr && String(component) == "servo_motor_1") {
    // Extract the angle from parameters -> value
    if (doc["parameters"]["value"].is<int>()) {
      int targetAngle = doc["parameters"]["value"];
      targetAngle = constrain(targetAngle, 0, 180); // Safety bounds for SG90
      servo1.write(targetAngle);

      Serial.print(F("Servo motor moved to: "));
      Serial.print(targetAngle);
      Serial.println(F(" degrees"));
    }
  }
}

void connectMqtt() {
  while (!mqtt.connected()) {
    Serial.print(F("Attempting MQTT connection..."));
    if (mqtt.connect("dev_jbbgU4tTLrgACkOk", MQTT_USER, MQTT_PASSWORD)) {
      Serial.println(F("connected"));
      mqtt.subscribe(COMMAND_TOPIC);
      Serial.print(F("Subscribed to: "));
      Serial.println(COMMAND_TOPIC);
    } else {
      Serial.print(F("failed, rc="));
      Serial.print(mqtt.state());
      Serial.println(F(" try again in 5 seconds"));
      delay(5000);
    }
  }
}

void setup() {
  Serial.begin(115200);
  delay(1000);

  // Configure HC-SR04 sensor pins
  pinMode(HCSR04_1_TRIGGERPIN, OUTPUT);
  pinMode(HCSR04_1_ECHOPIN, INPUT);

  // Initialize Servo motor pin
  servo1.attach(SERVO_MOTOR_1_CONTROLPIN);
  servo1.write(90); // Default home position (90 degrees)

  // Connect to Wi-Fi
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  Serial.print(F("Connecting to WiFi"));
  while (WiFi.status() != WL_CONNECTED) {
    delay(250);
    Serial.print(F("."));
  }
  Serial.println(F("\nWiFi connected."));

  // Configure Secure Client & MQTT Server
  networkClient.setCACert(ROOT_CA);
  mqtt.setServer(MQTT_HOST, MQTT_PORT); 
  mqtt.setCallback(onCommand); 
  connectMqtt();
}

void loop() {
  if (!mqtt.connected()) {
    connectMqtt();
  }
  mqtt.loop();

  static unsigned long last = 0;
  if (millis() - last >= 1000) {
    last = millis();
    publishTelemetry();
  }
}