/*
 * ESP32-B  --  4x4 keypad IR remote (transmitter)   [debug/logging build]
 *
 * Libraries (Library Manager): "IRremote" (v4.x), "Keypad" (Mark Stanley)
 *
 * Wiring
 *   IR LED / transmitter signal -> GPIO 4  (use a transistor driver for range)
 *   Keypad rows  R1..R4 -> GPIO 32, 33, 25, 26
 *   Keypad cols  C1..C4 -> GPIO 16, 17, 18, 19
 *   Status LED (on-board)  -> GPIO 2
 *
 * Entering a command on the keypad:
 *   [device key] [number 0-255] [#]      e.g.  B 9 0 #  -> servo to 90 deg
 *   [*] clears whatever you have typed
 *     A = Traffic light (0 off, 1 red, 2 yellow, 3 green, 4 cycle)
 *     B = Servo (angle 0-180)
 *     C = Buzzer (beep length = number x 10 ms)
 *     D = System / macro (0 = everything off)
 *
 * IR frame (Extended NEC):  address16 = (device << 8) | action,  command8 = value
 *
 * LED (GPIO 2):  short blip every 2 s = alive and idle
 *                solid ~150 ms        = an IR frame was just sent
 *                3 fast blinks        = rejected input (bad value, etc.)
 *
 * Serial Monitor (115200 baud, line ending "Newline") test commands:
 *   help               list commands
 *   pins               print pin map
 *   status             print counters and settings
 *   debug 0|1|2        log level (0 quiet, 1 events, 2 verbose)
 *   keytest on|off     on = keys are only logged, nothing is sent (checks wiring)
 *   send <A-D> <0-255> send one frame without the keypad, e.g.  send B 90
 *   demo on|off        send a test sequence every 2 s (test A without typing)
 */

#include <Arduino.h>
#include <IRremote.hpp>
#include <Keypad.h>

#define IR_TX_PIN 4
#define LED_PIN   2

// ----- shared protocol IDs (keep identical in A's main.py) -----
#define DEV_TRAFFIC 0x01
#define DEV_SERVO   0x02
#define DEV_BUZZER  0x03
#define DEV_SYSTEM  0x05
#define ACT_SET     0x00

// ----- logging -----
int debugLevel = 2;   // 0 quiet, 1 events, 2 verbose
#define LOG(fmt, ...)  do { Serial.printf("[%8lu] " fmt "\n", millis(), ##__VA_ARGS__); } while (0)
#define LOG1(fmt, ...) do { if (debugLevel >= 1) LOG(fmt, ##__VA_ARGS__); } while (0)
#define LOG2(fmt, ...) do { if (debugLevel >= 2) LOG(fmt, ##__VA_ARGS__); } while (0)

// ----- keypad -----
const byte ROWS = 4, COLS = 4;
char keys[ROWS][COLS] = {
  {'1', '2', '3', 'A'},
  {'4', '5', '6', 'B'},
  {'7', '8', '9', 'C'},
  {'*', '0', '#', 'D'}
};
byte rowPins[ROWS] = {13, 12, 14, 27};
byte colPins[COLS] = {26, 25, 33, 32};
Keypad keypad = Keypad(makeKeymap(keys), rowPins, colPins, ROWS, COLS);

// ----- state -----
int  currentDevice = -1;   // -1 = waiting for a device key
char numBuf[4] = "";       // up to 3 digits
byte numLen = 0;

bool keyTest = false;
bool demoOn = false;
unsigned long lastDemo = 0;
int demoStep = 0;

unsigned long sentCount = 0, keyCount = 0, rejectCount = 0;
unsigned long ledUntil = 0;

String serialLine = "";

// demo sequence: {device, value}
const int DEMO_LEN = 10;
const int demoSeq[DEMO_LEN][2] = {
  {DEV_TRAFFIC, 1}, {DEV_TRAFFIC, 2}, {DEV_TRAFFIC, 3}, {DEV_TRAFFIC, 0},
  {DEV_SERVO, 0},   {DEV_SERVO, 90},  {DEV_SERVO, 180}, {DEV_SERVO, 90},
  {DEV_BUZZER, 10}, {DEV_SYSTEM, 0}
};

// ----- helpers -----
int deviceFor(char k) {
  switch (k) {
    case 'A': return DEV_TRAFFIC;
    case 'B': return DEV_SERVO;
    case 'C': return DEV_BUZZER;
    case 'D': return DEV_SYSTEM;
  }
  return -1;
}

const char* deviceName(int d) {
  switch (d) {
    case DEV_TRAFFIC: return "TRAFFIC";
    case DEV_SERVO:   return "SERVO";
    case DEV_BUZZER:  return "BUZZER";
    case DEV_SYSTEM:  return "SYSTEM";
  }
  return "?";
}

void clearEntry() {
  currentDevice = -1;
  numBuf[0] = 0;
  numLen = 0;
}

void flashLed(unsigned long ms) {
  ledUntil = millis() + ms;
}

void errorBlink() {            // 3 fast blinks (blocking, short)
  for (int i = 0; i < 3; i++) {
    digitalWrite(LED_PIN, HIGH); delay(80);
    digitalWrite(LED_PIN, LOW);  delay(80);
  }
}

void ledUpdate() {
  unsigned long now = millis();
  bool on = (long)(ledUntil - now) > 0;     // sent-frame flash
  if (!on && (now % 2000) < 50) on = true;  // heartbeat blip
  digitalWrite(LED_PIN, on ? HIGH : LOW);
}

void printPins() {
  LOG("PINS  IR TX=%d  LED=%d", IR_TX_PIN, LED_PIN);
  LOG("PINS  rows: %d %d %d %d", rowPins[0], rowPins[1], rowPins[2], rowPins[3]);
  LOG("PINS  cols: %d %d %d %d", colPins[0], colPins[1], colPins[2], colPins[3]);
}

void printStatus() {
  LOG("STATUS uptime=%lus debug=%d keytest=%s demo=%s",
      millis() / 1000, debugLevel, keyTest ? "ON" : "off", demoOn ? "ON" : "off");
  LOG("STATUS keys=%lu sent=%lu rejected=%lu  entry: device=%s digits='%s'",
      keyCount, sentCount, rejectCount,
      currentDevice > 0 ? deviceName(currentDevice) : "none", numBuf);
}

void printHelp() {
  Serial.println(F("Commands: help | pins | status | debug 0|1|2 | keytest on|off | send <A-D> <0-255> | demo on|off"));
}

void sendCommand(int device, int action, int value) {
  uint16_t address = ((uint16_t)device << 8) | (uint8_t)action;
  unsigned long t0 = micros();
  IrSender.sendNEC(address, (uint8_t)value, 0);
  unsigned long took = micros() - t0;
  sentCount++;
  LOG1("TX #%lu  %s action=%d value=%d  addr=0x%04X cmd=0x%02X  (%lu us)",
       sentCount, deviceName(device), action, value, address, value, took);
  flashLed(150);
}

// ----- serial test console -----
void handleSerialLine(String line) {
  line.trim();
  if (line.length() == 0) return;
  String lower = line;
  lower.toLowerCase();

  if (lower == "help") { printHelp(); return; }
  if (lower == "pins") { printPins(); return; }
  if (lower == "status") { printStatus(); return; }

  if (lower.startsWith("debug ")) {
    int lvl = lower.substring(6).toInt();
    debugLevel = constrain(lvl, 0, 2);
    LOG("debug level = %d", debugLevel);
    return;
  }
  if (lower.startsWith("keytest ")) {
    keyTest = lower.endsWith("on");
    clearEntry();
    LOG("keytest %s (keys are %s)", keyTest ? "ON" : "off",
        keyTest ? "only logged, nothing sent" : "processed normally");
    return;
  }
  if (lower.startsWith("demo ")) {
    demoOn = lower.endsWith("on");
    demoStep = 0;
    lastDemo = 0;
    LOG("demo %s", demoOn ? "ON (frame every 2 s)" : "off");
    return;
  }
  if (lower.startsWith("send ")) {
    char d = 0; int v = -1;
    if (sscanf(line.c_str(), "%*s %c %d", &d, &v) == 2) {
      int dev = deviceFor(toupper(d));
      if (dev > 0 && v >= 0 && v <= 255) {
        sendCommand(dev, ACT_SET, v);
        return;
      }
    }
    rejectCount++;
    LOG("WARN usage: send <A-D> <0-255>   e.g. send B 90");
    return;
  }
  rejectCount++;
  LOG("WARN unknown command '%s' (type help)", line.c_str());
}

void pollSerial() {
  while (Serial.available()) {
    char c = Serial.read();
    if (c == '\n' || c == '\r') {
      handleSerialLine(serialLine);
      serialLine = "";
    } else if (serialLine.length() < 60) {
      serialLine += c;
    }
  }
}

// ----- keypad logic -----
void handleKey(char k) {
  keyCount++;
  LOG2("KEY '%c' (#%lu)", k, keyCount);

  if (keyTest) {                        // wiring test: log only
    LOG("KEYTEST pressed '%c'", k);
    return;
  }

  if (k == '*') {                       // cancel
    clearEntry();
    LOG1("entry cleared");
    return;
  }

  int dev = deviceFor(k);
  if (dev > 0) {                        // device key starts a new entry
    clearEntry();
    currentDevice = dev;
    LOG1("device = %s, type a number then #", deviceName(dev));
    return;
  }

  if (k >= '0' && k <= '9') {           // digit
    if (currentDevice < 0) {
      rejectCount++;
      LOG("WARN press A/B/C/D first");
      errorBlink();
      return;
    }
    if (numLen < 3) {
      numBuf[numLen++] = k;
      numBuf[numLen] = 0;
      LOG1("value so far: %s", numBuf);
    } else {
      LOG("WARN max 3 digits");
    }
    return;
  }

  if (k == '#') {                       // send
    if (currentDevice < 0 || numLen == 0) {
      rejectCount++;
      LOG("WARN nothing to send (need device + number)");
      errorBlink();
      clearEntry();
      return;
    }
    int value = atoi(numBuf);
    if (value > 255) {
      rejectCount++;
      LOG("WARN value %d out of range 0-255, not sent", value);
      errorBlink();
    } else {
      sendCommand(currentDevice, ACT_SET, value);
    }
    clearEntry();
  }
}

void setup() {
  Serial.begin(115200);
  delay(300);
  pinMode(LED_PIN, OUTPUT);
  digitalWrite(LED_PIN, LOW);
  IrSender.begin(IR_TX_PIN);

  Serial.println();
  Serial.println(F("=== ESP32-B IR remote (debug build) ==="));
  printPins();
  printHelp();
  LOG("READY. Press A/B/C/D, a number, then #");

  // boot LED test: 2 blinks
  for (int i = 0; i < 2; i++) {
    digitalWrite(LED_PIN, HIGH); delay(120);
    digitalWrite(LED_PIN, LOW);  delay(120);
  }
}

void loop() {
  pollSerial();

  char k = keypad.getKey();
  if (k) handleKey(k);

  if (demoOn && millis() - lastDemo >= 2000) {
    lastDemo = millis();
    LOG1("DEMO step %d/%d", demoStep + 1, DEMO_LEN);
    sendCommand(demoSeq[demoStep][0], ACT_SET, demoSeq[demoStep][1]);
    demoStep = (demoStep + 1) % DEMO_LEN;
  }

  ledUpdate();
}
