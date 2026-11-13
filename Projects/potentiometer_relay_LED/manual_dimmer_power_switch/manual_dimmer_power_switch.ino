const int potPin = 34;      // ESP32 Analog input pin (GPIO 34)
const int relayPin = 23;    // Digital pin connected to Relay IN

// Array of 6 LED pins
const int ledPins[6] = {2, 4, 18, 19, 21, 22}; 

// PWM properties for ESP32 Core v3.x
const int freq = 5000;      // 5 kHz PWM frequency
const int resolution = 8;   // 8-bit resolution (0 - 255 brightness)

void setup() {
  Serial.begin(115200);
  pinMode(relayPin, OUTPUT);
  digitalWrite(relayPin, LOW);

  // Attach and configure each LED pin for PWM using the v3.x syntax
  for (int i = 0; i < 6; i++) {
    ledcAttach(ledPins[i], freq, resolution); 
    ledcWrite(ledPins[i], 0); // Start with LEDs off
  }
}

void loop() {
  int potValue = analogRead(potPin); // Reads 0 to 4095 on ESP32
  Serial.print("Potentiometer Value: ");
  Serial.println(potValue);

  // 1. Master Power Switch (Relay) Action
  // Turns relay ON when past the halfway mark (2048)
  if (potValue > 2048) {
    digitalWrite(relayPin, HIGH); 
  } else {
    digitalWrite(relayPin, LOW);  
  }

  // 2. Manual Dimmer Action
  // Map the 0-4095 potentiometer range to 0-255 PWM brightness
  int brightness = map(potValue, 0, 4095, 0, 255);

  // Apply the brightness level directly to each LED pin
  for (int i = 0; i < 6; i++) {
    ledcWrite(ledPins[i], brightness);
  }

  delay(20); // Smooth update delay
}