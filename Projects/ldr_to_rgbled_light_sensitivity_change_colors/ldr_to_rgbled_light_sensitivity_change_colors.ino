// Pin definitions for the single RGB LED
const int LED_R = 25;
const int LED_G = 26;
const int LED_B = 27;

// LDR Sensor Pin
const int LDR_PIN = 34;

// PWM Configuration for ESP32
const int PWM_FREQ = 5000;    // 5 kHz PWM frequency
const int PWM_RES = 8;      // 8-bit resolution (0 - 255)

// Variables for smoothing LDR readings
float smoothedLdr = 0.0;
unsigned long lastPrint = 0;

void setup() {
  Serial.begin(115200);
  delay(500);
  Serial.println("Starting LDR-Controlled RGB LED...");

  // Configure PWM pins using modern ESP32 Core API
  ledcAttach(LED_R, PWM_FREQ, PWM_RES);
  ledcAttach(LED_G, PWM_FREQ, PWM_RES);
  ledcAttach(LED_B, PWM_FREQ, PWM_RES);

  // Configure LDR input pin
  pinMode(LDR_PIN, INPUT);
}

void loop() {
  // 1. Read raw LDR value (0 to 4095 on ESP32 12-bit ADC)
  int rawLdr = analogRead(LDR_PIN);

  // 2. Apply a Low-Pass Filter (Smoothing) to prevent jitter/flickering
  if (smoothedLdr == 0.0) {
    smoothedLdr = rawLdr; // Initialize on first run
  } else {
    smoothedLdr = (0.15 * rawLdr) + (0.85 * smoothedLdr);
  }

  // 3. Map the smoothed LDR value directly to the color spectrum step (0 to 765)
  // Adjust input bounds (e.g., 100 to 3500) if your specific LDR setup doesn't reach the full 0-4095 range
  int colorStep = map(constrain((int)smoothedLdr, 100, 3500), 100, 3500, 0, 765);

  // 4. Generate RGB values based on the LDR-driven color step
  int r, g, b;
  getColorFromStep(colorStep, r, g, b);

  // 5. Apply PWM values to the LED pins
  // (Note: If you are using a Common Anode LED, change values to: 255 - r, etc.)
  ledcWrite(LED_R, r);
  ledcWrite(LED_G, g);
  ledcWrite(LED_B, b);

  // Print diagnostics to Serial Monitor every 200ms
  if (millis() - lastPrint > 200) {
    Serial.print("Raw LDR: ");
    Serial.print(rawLdr);
    Serial.print(" | Filtered: ");
    Serial.print((int)smoothedLdr);
    Serial.print(" | Color Step: ");
    Serial.println(colorStep);
    lastPrint = millis();
  }

  delay(20); // Small loop delay for stability
}

// Helper function to create a smooth cross-fade across the RGB spectrum
void getColorFromStep(int step, int &r, int &g, int &b) {
  if (step < 255) {
    r = 255 - step;
    g = step;
    b = 0;
  } else if (step < 510) {
    step -= 255;
    r = 0;
    g = 255 - step;
    b = step;
  } else {
    step -= 510;
    r = step;
    g = 0;
    b = 255 - step;
  }
}