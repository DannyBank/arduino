// Define GPIO pins on the ESP32
const int trigPin = 5;
const int echoPin = 18;

// Define sound speed in centimeters per microsecond
// Speed of sound is roughly 343 m/s = 0.0343 cm/us
// Sound travels out and back, so we divide by 2: 0.0343 / 2 = 0.01715
#define SOUND_SPEED 0.0343

long duration;
float distanceCm;

void setup() {
  // Start the Serial Monitor
  Serial.begin(115200);
  
  // Configure the pins
  pinMode(trigPin, OUTPUT);
  pinMode(echoPin, INPUT);
}

void loop() {
  // Clear the trigPin by setting it LOW
  digitalWrite(trigPin, LOW);
  delayMicroseconds(2);
  
  // Trigger the sensor by setting the trigPin HIGH for 10 microseconds
  digitalWrite(trigPin, HIGH);
  delayMicroseconds(10);
  digitalWrite(trigPin, LOW);
  
  // Read the echoPin, returns the sound wave travel time in microseconds
  duration = pulseIn(echoPin, HIGH);
  
  // Calculate the distance
  distanceCm = duration * SOUND_SPEED / 2;
  
  // Print the distance to the Serial Monitor
  if (duration == 0) {
    Serial.println("Out of range / No echo received");
  } else {
    Serial.print("Distance: ");
    Serial.print(distanceCm);
    Serial.println(" cm");
  }
  
  // Wait a short moment before the next reading
  delay(500);
}