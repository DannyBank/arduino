const int ledPins[] = {2, 4, 18, 19, 21, 22}; // Define your GPIO pins
const int numLeds = 6;

void setup() {
  for (int i = 0; i < numLeds; i++) {
    pinMode(ledPins[i], OUTPUT);
  }
}

void loop() {
  // Cycle through the LEDs one by one
  for (int i = 0; i < numLeds; i++) {
    digitalWrite(ledPins[i], HIGH);
    delay(300);
    digitalWrite(ledPins[i], LOW);
  }
}