#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

// Define OLED display dimensions
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64

// Declaration for an SSD1306 display connected to I2C (SDA, SCL pins)
#define OLED_RESET -1 // Reset pin # (or -1 if sharing Arduino reset pin)
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

// Define GPIO pins for HC-SR04
const int trigPin = 5;
const int echoPin = 18;

// Speed of sound constant for cm/us
#define SOUND_SPEED 0.0343

long duration;
float distanceCm;

void setup() {
  // Initialize Serial Monitor for debugging (optional)
  Serial.begin(115200);

  // Initialize the pins
  pinMode(trigPin, OUTPUT);
  pinMode(echoPin, INPUT);

  // Initialize the OLED display with I2C address 0x3C (common for 128x64)
  if(!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) {
    Serial.println(F("SSD1306 allocation failed"));
    for(;;); // Don't proceed, loop forever
  }

  // Clear display buffer and show startup splash screen
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(15, 25);
  display.print(F("HC-SR04 Ready!"));
  display.display();
  delay(2000);
}

void loop() {
  // Clear the trigger pin
  digitalWrite(trigPin, LOW);
  delayMicroseconds(2);
  
  // Set the trigger pin HIGH for 10 microseconds
  digitalWrite(trigPin, HIGH);
  delayMicroseconds(10);
  digitalWrite(trigPin, LOW);
  
  // Read the echo pin travel time
  duration = pulseIn(echoPin, HIGH);
  
  // Calculate distance in centimeters
  distanceCm = duration * SOUND_SPEED / 2;

  // Clear the display buffer for new data
  display.clearDisplay();

  // Draw a border box around the screen
  display.drawRect(0, 0, SCREEN_WIDTH, SCREEN_HEIGHT, SSD1306_WHITE);

  // Print static title label
  display.setTextSize(1);
  display.setCursor(18, 10);
  display.print(F("OBJECT DISTANCE"));

  // Print the measured distance value
  display.setTextSize(2);
  display.setCursor(20, 32);
  
  if (duration == 0 || distanceCm > 400) {
    display.print(F("Out of Range"));
  } else {
    display.print(distanceCm, 1); // Print with 1 decimal place
    display.setTextSize(1);
    display.print(F(" cm"));
  }

  // Push the buffer contents to the physical screen
  display.display();

  // Wait a short moment before taking the next measurement
  delay(400);
}