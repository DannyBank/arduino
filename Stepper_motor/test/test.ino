#include <Stepper.h>

// Define the number of steps per revolution for the 28BYJ-48 motor (half-step mode)
const int stepsPerRevolution = 2048; 

// Initialize the Stepper library with safe ESP32 GPIO pins 
// Constructor order for 4-phase unipolar drivers: IN1, IN3, IN2, IN4
// Mapping: IN1 -> 14, IN2 -> 27, IN3 -> 26, IN4 -> 25
// Passing sequence to library: 14, 26, 27, 25
Stepper myStepper(stepsPerRevolution, 14, 26, 27, 25);

void setup() {
  Serial.begin(115200);
  
  // Set the speed to 10 rpm
  myStepper.setSpeed(10);
  
  Serial.println("Stepper motor test initialized with safe GPIOs.");
}

void loop() {
  Serial.println("Rotating clockwise...");
  myStepper.step(stepsPerRevolution); // Rotate 1 full revolution clockwise
  delay(1000);

  Serial.println("Rotating counter-clockwise...");
  myStepper.step(-stepsPerRevolution); // Rotate 1 full revolution counter-clockwise
  delay(1000);
}