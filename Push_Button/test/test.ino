// Define the button pin
const int BUTTON_PIN = 14; 

// Variables to handle button state and debouncing
int lastButtonState = HIGH;      // The previous reading from the input pin
int buttonState;                 // The current stable reading from the input pin
unsigned long lastDebounceTime = 0;  // The last time the output pin was toggled
unsigned long debounceDelay = 50;    // The debounce time in milliseconds; increase if flickering persists

void setup() {
  Serial.begin(115200);
  delay(500);
  Serial.println("Push Button Test Initialized...");

  // Configure the button pin as an Input with an internal pull-up resistor
  // Unpressed = HIGH (3.3V)
  // Pressed = LOW (GND)
  pinMode(BUTTON_PIN, INPUT_PULLUP);
}

void loop() {
  // Read the current state of the button
  int reading = digitalRead(BUTTON_PIN);

  // If the switch changed, due to noise or pressing, reset the debounce timer
  if (reading != lastButtonState) {
    lastDebounceTime = millis();
  }

  // Once the delay has passed, whatever the reading is now is stable
  if ((millis() - lastDebounceTime) > debounceDelay) {
    
    // If the button state has actually changed from our last stable state:
    if (reading != buttonState) {
      buttonState = reading;

      // Since we use INPUT_PULLUP, LOW means the button is pressed down
      if (buttonState == LOW) {
        Serial.println("Button Pressed!");
      } else {
        Serial.println("Button Released!");
      }
    }
  }

  // Save the reading for the next loop iteration
  lastButtonState = reading;
}