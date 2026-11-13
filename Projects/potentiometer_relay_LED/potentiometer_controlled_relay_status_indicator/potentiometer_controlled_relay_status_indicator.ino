const int potPin = 34;      // ESP32 Analog input pin (GPIO 34)
const int relayPin = 23;    // Digital pin connected to Relay IN

// Array of 6 LED pins (2 Red, 2 Yellow, 2 Blue)
const int ledPins[6] = {2, 4, 18, 19, 21, 22}; 

void setup() {
  Serial.begin(115200);     // Initialize serial communication
  
  pinMode(relayPin, OUTPUT);
  digitalWrite(relayPin, LOW);

  // Set all LED pins as outputs and ensure they start OFF
  for (int i = 0; i < 6; i++) {
    pinMode(ledPins[i], OUTPUT);
    digitalWrite(ledPins[i], LOW);
  }
}

void loop() {
  int potValue = analogRead(potPin); // Reads 0 to 4095 on ESP32
  Serial.print("Potentiometer Value: ");
  Serial.println(potValue);

  // 1. Control the Relay: Turns ON past the halfway mark (2048)
  if (potValue > 2048) {
    digitalWrite(relayPin, HIGH); 
  } else {
    digitalWrite(relayPin, LOW);  
  }

  // 2. Control the 6 LEDs like a level bar graph
  // Maps the 0-4095 range cleanly to 0 through 6 lit LEDs
  int ledsToLight = map(potValue, 0, 4095, 0, 6);

  for (int i = 0; i < 6; i++) {
    if (i < ledsToLight) {
      digitalWrite(ledPins[i], HIGH); // Turn LED on
    } else {
      digitalWrite(ledPins[i], LOW);  // Turn LED off
    }
  }
  
  delay(50); // Small delay for smooth response
}