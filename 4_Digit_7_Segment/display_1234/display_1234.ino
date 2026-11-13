
/*
 * ESP32 + 4-digit, 7-segment display
 * Display type: Common Anode
 *
 * Segment resistors: 220-330 ohms, one per segment.
 * Digit pins should use suitable transistor drivers.
 */

// Segment pins: a, b, c, d, e, f, g, dp
const int segmentPins[] = {
  13, // A - Display pin 11
  26, // B - Display pin 7
  22, // C - Display pin 4
  19, // D - Display pin 2
  18, // E - Display pin 1
  27, // F - Display pin 10
  23, // G - Display pin 5
  21  // DP - Display pin 3
};

// Digit select pins: D1, D2, D3, D4
const int digitPins[] = {
  14, // D1 - Display pin 12
  33, // D2 - Display pin 9
  32, // D3 - Display pin 8
  25  // D4 - Display pin 6
};

/*
 * Bit order: dp, g, f, e, d, c, b, a
 * Common-anode: LOW = ON, HIGH = OFF
 */
const byte digitPatterns[10] = {
  0b11000000, // 0
  0b11111001, // 1
  0b10100100, // 2
  0b10110000, // 3
  0b10011001, // 4
  0b10010010, // 5
  0b10000010, // 6
  0b11111000, // 7
  0b10000000, // 8
  0b10010000  // 9
};

// Change these if your digit-driver circuit
// requires inverted control logic.
const int DIGIT_ON  = HIGH;
const int DIGIT_OFF = LOW;

void setup() {
  for (int i = 0; i < 8; i++) {
    pinMode(segmentPins[i], OUTPUT);
    digitalWrite(segmentPins[i], HIGH);
  }

  for (int i = 0; i < 4; i++) {
    pinMode(digitPins[i], OUTPUT);
    digitalWrite(digitPins[i], DIGIT_OFF);
  }
}

void loop() {
  displayNumber(1234);
}

void displayNumber(int value) {
  value = constrain(value, 0, 9999);

  int digits[4] = {
    (value / 1000) % 10,
    (value / 100) % 10,
    (value / 10) % 10,
    value % 10
  };

  // Keep refreshing the display for persistence of vision.
  for (int i = 0; i < 4; i++) {
    allDigitsOff();
    setSegments(digits[i]);
    digitalWrite(digitPins[i], DIGIT_ON);
    delay(5);
  }
}

void setSegments(int n) {
  byte pattern = digitPatterns[n];

  for (int i = 0; i < 8; i++) {
    bool bitVal = (pattern >> i) & 0x01;
    digitalWrite(segmentPins[i], bitVal ? HIGH : LOW);
  }
}

void allDigitsOff() {
  for (int i = 0; i < 4; i++) {
    digitalWrite(digitPins[i], DIGIT_OFF);
  }
}
