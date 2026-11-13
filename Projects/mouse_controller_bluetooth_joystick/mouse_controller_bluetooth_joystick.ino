// -----------------------------------------------------------------------------
// ESP32 Bluetooth Mouse Controller - Tuned (axis-configurable, smooth response)
// -----------------------------------------------------------------------------
#include <BleMouse.h>

BleMouse bleMouse(std::string("ESP32_Joystick_Mouse"), std::string("IoTDevLab"), 100);

// ---------------- Pins ----------------
const int JOYSTICK_X_PIN  = 34;
const int JOYSTICK_Y_PIN  = 35;
const int JOYSTICK_SW_PIN = 32;

// ---------------- ORIENTATION (set these after running DEBUG_AXES) ----------
// Convention after these flags: pushing RIGHT -> cursor right, DOWN -> cursor down.
const bool SWAP_AXES = false;  // true if pushing left/right moves the cursor up/down
const bool INVERT_X  = false;  // true if left/right are mirrored
const bool INVERT_Y  = false;  // true if up/down are mirrored

// Set true to print raw readings and detected direction (cursor is NOT moved).
const bool DEBUG_AXES = false;

// ---------------- Feel ----------------
const float DEADZONE   = 0.12f;  // fraction of full travel ignored around center
const float MAX_SPEED  = 10.0f;  // pixels per tick at full deflection
const float CURVE_MIX  = 0.7f;   // 0 = linear, 1 = fully quadratic (finer control near center)
const int   LOOP_MS    = 10;

int centerX = 2048;
int centerY = 2048;
float accX = 0, accY = 0;        // sub-pixel accumulators

// Normalize a raw ADC reading to -1.0 .. +1.0 using the calibrated center.
// Each side is scaled by its own available travel, so asymmetric sticks still reach full speed.
float normalize(int raw, int center) {
  float delta = raw - center;
  float range = (delta >= 0) ? (4095 - center) : center;
  if (range < 1) range = 1;
  float n = delta / range;
  if (n > 1.0f) n = 1.0f;
  if (n < -1.0f) n = -1.0f;
  return n;
}

// Deadzone + response curve -> speed in pixels/tick (signed).
float shapeAxis(float n) {
  float mag = fabsf(n);
  if (mag < DEADZONE) return 0.0f;
  mag = (mag - DEADZONE) / (1.0f - DEADZONE);              // restart from 0 at deadzone edge
  float curved = (1.0f - CURVE_MIX) * mag + CURVE_MIX * mag * mag;
  float speed = curved * MAX_SPEED;
  return (n < 0) ? -speed : speed;
}

// Turn a float speed into whole pixels, carrying the remainder to the next tick.
int takeWhole(float &acc, float speed) {
  if (speed == 0.0f) { acc = 0; return 0; }
  acc += speed;
  int whole = (int)acc;
  acc -= whole;
  return whole;
}

void setup() {
  Serial.begin(115200);
  pinMode(JOYSTICK_SW_PIN, INPUT_PULLUP);

  analogReadResolution(12);
  analogSetPinAttenuation(JOYSTICK_X_PIN, ADC_11db);
  analogSetPinAttenuation(JOYSTICK_Y_PIN, ADC_11db);

  Serial.println("Starting BLE Mouse service...");
  bleMouse.begin();

  Serial.println("Calibrating... do not touch the stick!");
  long sumX = 0, sumY = 0;
  for (int i = 0; i < 100; i++) {
    sumX += analogRead(JOYSTICK_X_PIN);
    sumY += analogRead(JOYSTICK_Y_PIN);
    delay(10);
  }
  centerX = sumX / 100;
  centerY = sumY / 100;
  Serial.printf("Center -> X: %d | Y: %d\n", centerX, centerY);
  if (DEBUG_AXES) Serial.println("DEBUG_AXES on: push the stick and read which pin/direction fires.");
}

void loop() {
  int rawX = analogRead(JOYSTICK_X_PIN);
  int rawY = analogRead(JOYSTICK_Y_PIN);

  float nX = normalize(rawX, centerX);
  float nY = normalize(rawY, centerY);

  // ---- Diagnostic: tells you exactly how your module is oriented ----
  if (DEBUG_AXES) {
    static unsigned long lastPrint = 0;
    if (millis() - lastPrint > 150) {
      lastPrint = millis();
      if (fabsf(nX) > 0.4f || fabsf(nY) > 0.4f) {
        Serial.printf("rawX=%4d (%+.2f) rawY=%4d (%+.2f)  ->  ", rawX, nX, rawY, nY);
        if (fabsf(nX) >= fabsf(nY))
          Serial.println(nX > 0 ? "X pin went HIGH" : "X pin went LOW");
        else
          Serial.println(nY > 0 ? "Y pin went HIGH" : "Y pin went LOW");
      }
    }
    delay(LOOP_MS);
    return;
  }

  if (!bleMouse.isConnected()) {
    accX = accY = 0;
    delay(500);
    return;
  }

  // Apply orientation so +X = right, +Y = down on screen
  if (SWAP_AXES) { float t = nX; nX = nY; nY = t; }
  if (INVERT_X) nX = -nX;
  if (INVERT_Y) nY = -nY;

  int moveX = takeWhole(accX, shapeAxis(nX));
  int moveY = takeWhole(accY, shapeAxis(nY));

  if (moveX != 0 || moveY != 0) {
    bleMouse.move(moveX, moveY);
  }

  // Joystick button = left click
  bool pressed = (digitalRead(JOYSTICK_SW_PIN) == LOW);
  if (pressed && !bleMouse.isPressed(MOUSE_LEFT))  bleMouse.press(MOUSE_LEFT);
  if (!pressed && bleMouse.isPressed(MOUSE_LEFT))  bleMouse.release(MOUSE_LEFT);

  delay(LOOP_MS);
}
