/*
  ESP32 "B"  -  Dashboard + IR remote controller   (Arduino)
  128x64 SSD1306 OLED (I2C) + IR receiver  ->  controls ESP32 "A" over ESP-NOW

  Libraries (Library Manager):
    - Adafruit GFX Library
    - Adafruit SSD1306
    - IRremote  (Arduino-IRremote, v4.x)
  Board package: esp32 by Espressif (2.x or 3.x both work)

  Wiring:  OLED SDA -> GPIO21, SCL -> GPIO22, VCC 3V3, GND
           IR receiver OUT -> GPIO4

  Remote: pre-mapped for the common 21-key "car MP3" NEC remote.
  If your remote differs, open Serial Monitor (115200), press each key,
  and copy the printed cmd values into mapKey() below.

    2 / VOL+ = Up      8 / VOL- = Down     4 / PREV = Left    6 / NEXT = Right
    5 / PLAY = OK      CH- = Back          CH = Home
    100+ = silence buzzer                  200+ = ring on/off
*/
#include <Arduino.h>
#include <Wire.h>
#include <WiFi.h>
#include <esp_now.h>
#include <esp_wifi.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <IRremote.hpp>

// ------------------------------ CONFIG -------------------------------
const int IR_RECEIVE_PIN = 4;
#define SCREEN_W        128
#define SCREEN_H        64
#define OLED_ADDR       0x3C
#define ESPNOW_CHANNEL  1          // must match ESPNOW_CHANNEL in A's main.py
#define LINK_TIMEOUT_MS 3000
#define IDLE_HOME_MS    45000

Adafruit_SSD1306 display(SCREEN_W, SCREEN_H, &Wire, -1);
const uint8_t BCAST[6] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};

// ------------------------------ TYPES --------------------------------
enum Screen { SC_HOME, SC_MENU, SC_RING, SC_SERVO, SC_TRAFFIC, SC_BUZZER, SC_CLIMATE, SC_LINK };
enum Key { K_NONE, K_UP, K_DOWN, K_LEFT, K_RIGHT, K_OK, K_BACK, K_HOME, K_BUZZSTOP, K_RINGTOG };

struct Telemetry {
  float dhtT = NAN, dhtH = NAN, ahtT = NAN, ahtH = NAN, bmpT = NAN, bmpP = NAN;
  int light = 0, btn = 0, ringMode = 0, ringHue = 0, ringBri = 0;
  int servoAngle = 90, servoSweep = 0, trafficMode = 0, trafficMask = 0, buzz = 0;
  uint32_t uptime = 0;
} tel;

const char* RING_MODES[]    = {"Off", "Solid", "Rainbow", "Chase", "Breathe", "Twinkle"};
const char* TRAFFIC_MODES[] = {"Off", "Auto", "Red", "Yellow", "Green", "Blink"};
const char* BUZZ_ITEMS[]    = {"Beep", "Melody", "Alarm (loop)", "Stop"};
struct MenuItem { const char* name; Screen target; };
const MenuItem MENU[] = {
  {"Ring Light", SC_RING}, {"Servo Arm", SC_SERVO}, {"Traffic Light", SC_TRAFFIC},
  {"Buzzer", SC_BUZZER},   {"Climate", SC_CLIMATE}, {"Link Info", SC_LINK}
};
const int MENU_COUNT = sizeof(MENU) / sizeof(MENU[0]);

// ------------------------------ STATE --------------------------------
Screen screen = SC_HOME;
int menuSel = 0, row = 0;
bool useF = false;
uint32_t lastInput = 0, lastLocal = 0, lastDraw = 0, lastRx = 0, lastPing = 0, flashUntil = 0;
char flashMsg[24] = "";
uint32_t rxCount = 0, txCount = 0;
int rxRssi = 0;
int lastBtn = -1;
bool linkOnline = false;

// UI "shadow" of A's state, updated instantly on key press so the remote feels snappy
int uiRingMode = 0, uiHue = 0, uiBri = 0, uiAngle = 90, uiSweep = 0, uiTraffic = 0;

// radio receive buffer (filled in callback, parsed in loop)
portMUX_TYPE mux = portMUX_INITIALIZER_UNLOCKED;
char rxBuf[256];
volatile bool rxFlag = false;
volatile int8_t rxRssiTmp = 0;

// =========================== ESP-NOW ================================
#if ESP_ARDUINO_VERSION >= ESP_ARDUINO_VERSION_VAL(3, 0, 0)
void onRecv(const esp_now_recv_info_t* info, const uint8_t* data, int len) {
  int8_t rssi = info->rx_ctrl ? info->rx_ctrl->rssi : 0;
#else
void onRecv(const uint8_t* mac, const uint8_t* data, int len) {
  int8_t rssi = 0;
#endif
  if (len <= 0 || len >= (int)sizeof(rxBuf)) return;
  portENTER_CRITICAL(&mux);
  memcpy(rxBuf, data, len);
  rxBuf[len] = 0;
  rxRssiTmp = rssi;
  rxFlag = true;
  portEXIT_CRITICAL(&mux);
}

void sendCmd(const char* cmd, int val) {
  char buf[40];
  int n = snprintf(buf, sizeof(buf), "DBK,C,%s,%d", cmd, val);
  if (esp_now_send(BCAST, (uint8_t*)buf, n) == ESP_OK) txCount++;
  lastLocal = millis();
}

void radioBegin() {
  WiFi.mode(WIFI_STA);
  WiFi.disconnect();
  esp_wifi_set_channel(ESPNOW_CHANNEL, WIFI_SECOND_CHAN_NONE);
  if (esp_now_init() != ESP_OK) {
    Serial.println("ESP-NOW init failed");
    return;
  }
  esp_now_register_recv_cb(onRecv);
  esp_now_peer_info_t peer = {};
  memcpy(peer.peer_addr, BCAST, 6);
  peer.channel = ESPNOW_CHANNEL;
  peer.encrypt = false;
  esp_now_add_peer(&peer);
}

float fval(const char* s) {
  float v = atof(s);
  return v < -90 ? NAN : v;
}

bool parseTelemetry(char* s) {
  char* tok[24];
  int n = 0;
  for (char* p = strtok(s, ","); p && n < 24; p = strtok(NULL, ",")) tok[n++] = p;
  if (n < 19 || strcmp(tok[0], "DBK") != 0 || strcmp(tok[1], "S") != 0) return false;
  char** d = tok + 2;
  tel.dhtT = fval(d[0]);  tel.dhtH = fval(d[1]);
  tel.ahtT = fval(d[2]);  tel.ahtH = fval(d[3]);
  tel.bmpT = fval(d[4]);  tel.bmpP = fval(d[5]);
  tel.light = atoi(d[6]);
  tel.btn = atoi(d[7]);
  tel.ringMode = atoi(d[8]);  tel.ringHue = atoi(d[9]);  tel.ringBri = atoi(d[10]);
  tel.servoAngle = atoi(d[11]);  tel.servoSweep = atoi(d[12]);
  tel.trafficMode = atoi(d[13]);  tel.trafficMask = atoi(d[14]);
  tel.buzz = atoi(d[15]);
  tel.uptime = strtoul(d[16], NULL, 10);
  return true;
}

void flash(const char* msg, uint32_t ms = 1800) {
  strncpy(flashMsg, msg, sizeof(flashMsg) - 1);
  flashMsg[sizeof(flashMsg) - 1] = 0;
  flashUntil = millis() + ms;
}

void pollRadio() {
  if (rxFlag) {
    char local[256];
    portENTER_CRITICAL(&mux);
    memcpy(local, rxBuf, sizeof(local));
    rxRssi = rxRssiTmp;
    rxFlag = false;
    portEXIT_CRITICAL(&mux);
    if (parseTelemetry(local)) {
      rxCount++;
      lastRx = millis();
      if (lastBtn >= 0 && tel.btn != lastBtn) flash("Button pressed!");
      lastBtn = tel.btn;
      if (millis() - lastLocal > 1500) {          // no recent local edit -> trust A's state
        uiRingMode = tel.ringMode;  uiHue = tel.ringHue;  uiBri = tel.ringBri;
        uiAngle = tel.servoAngle;   uiSweep = tel.servoSweep;  uiTraffic = tel.trafficMode;
      }
    }
  }
  linkOnline = rxCount > 0 && (millis() - lastRx) < LINK_TIMEOUT_MS;
  if (!linkOnline && millis() - lastPing > 3000) {   // ask A to talk to us
    lastPing = millis();
    sendCmd("ping", 0);
  }
}

// =========================== HELPERS ================================
float bestTemp() {
  if (!isnan(tel.ahtT)) return tel.ahtT;
  if (!isnan(tel.bmpT)) return tel.bmpT;
  return tel.dhtT;
}
float bestHum() { return !isnan(tel.ahtH) ? tel.ahtH : tel.dhtH; }
float toUnit(float c) { return useF ? c * 1.8f + 32.0f : c; }

void tempStr(char* out, size_t n, float c) {
  if (isnan(c)) snprintf(out, n, "--");
  else snprintf(out, n, "%.1f%c%c", toUnit(c), 247, useF ? 'F' : 'C');
}

void centerText(const char* s, int cx, int y, int size) {
  int16_t x1, y1; uint16_t w, h;
  display.setTextSize(size);
  display.getTextBounds(s, 0, 0, &x1, &y1, &w, &h);
  display.setCursor(cx - (int)w / 2, y);
  display.print(s);
}

// ============================= ICONS ================================
void iconTherm(int x, int y) {
  display.drawRoundRect(x + 2, y, 5, 12, 2, WHITE);
  display.fillRect(x + 4, y + 4, 1, 8, WHITE);
  display.fillCircle(x + 4, y + 13, 3, WHITE);
}
void iconDrop(int x, int y) {
  display.fillTriangle(x + 4, y, x, y + 8, x + 8, y + 8, WHITE);
  display.fillCircle(x + 4, y + 10, 4, WHITE);
}
void iconSun(int x, int y) {
  display.fillCircle(x + 4, y + 4, 2, WHITE);
  display.drawLine(x + 4, y - 1, x + 4, y - 3 + 1, WHITE);
  display.drawLine(x + 4, y + 9, x + 4, y + 7, WHITE);
  display.drawLine(x - 1, y + 4, x + 1, y + 4, WHITE);
  display.drawLine(x + 9, y + 4, x + 7, y + 4, WHITE);
  display.drawPixel(x + 1, y + 1, WHITE); display.drawPixel(x + 7, y + 1, WHITE);
  display.drawPixel(x + 1, y + 7, WHITE); display.drawPixel(x + 7, y + 7, WHITE);
}

// ============================= HEADER ===============================
void drawLinkBars() {
  int q = 0;
  if (linkOnline) q = (rxRssi == 0) ? 4 : (rxRssi > -60 ? 4 : rxRssi > -70 ? 3 : rxRssi > -80 ? 2 : 1);
  for (int i = 0; i < 4; i++) {
    int h = 3 + i * 2, x = 108 + i * 5, y = 11 - h;
    if (i < q) display.fillRect(x, y, 3, h, BLACK);
    else display.drawRect(x, y, 3, h, BLACK);
  }
}

void drawHeader(const char* title) {
  display.fillRect(0, 0, SCREEN_W, 14, WHITE);
  display.setTextSize(1);
  display.setTextColor(BLACK);
  display.setCursor(4, 3);
  display.print(title);
  drawLinkBars();
  display.setTextColor(WHITE);
}

void drawLabel(const char* s, int x, int y, int w, bool sel) {      // label chip, inverted when selected
  if (sel) {
    display.fillRoundRect(x, y - 2, w, 11, 2, WHITE);
    display.setTextColor(BLACK);
  } else {
    display.setTextColor(WHITE);
  }
  display.setTextSize(1);
  display.setCursor(x + 3, y);
  display.print(s);
  display.setTextColor(WHITE);
}

void drawBar(int x, int y, int w, int h, int pct) {
  display.drawRoundRect(x, y, w, h, 2, WHITE);
  int fw = (w - 4) * constrain(pct, 0, 100) / 100;
  if (fw > 0) display.fillRect(x + 2, y + 2, fw, h - 4, WHITE);
}

// ============================= SCREENS ==============================
void drawHome() {
  drawHeader("DBANK HUB");
  char b[16];
  float t = bestTemp(), h = bestHum();

  iconTherm(1, 19);
  display.setTextSize(2);
  display.setCursor(12, 20);
  if (isnan(t)) display.print("--.-");
  else { snprintf(b, sizeof(b), useF ? "%.0f" : "%.1f", toUnit(t)); display.print(b); }
  display.setTextSize(1);
  display.write(247);
  display.print(useF ? 'F' : 'C');

  iconDrop(77, 19);
  display.setTextSize(2);
  display.setCursor(88, 20);
  if (isnan(h)) display.print("--"); else display.print((int)(h + 0.5f));
  display.setTextSize(1);
  display.print('%');

  iconSun(2, 40);
  drawBar(14, 39, 52, 9, tel.light);
  display.setCursor(72, 40);
  if (isnan(tel.bmpP)) display.print("--- hPa"); else { snprintf(b, sizeof(b), "%.0f hPa", tel.bmpP); display.print(b); }

  display.drawFastHLine(0, 51, SCREEN_W, WHITE);
  display.setCursor(0, 55);
  char line[32];
  if (millis() < flashUntil) {
    display.print(flashMsg);
  } else if (!linkOnline) {
    if ((millis() / 500) & 1) display.print("NO LINK - waiting...");
  } else if ((millis() / 3000) & 1) {
    display.print("[OK] open menu");
  } else {
    snprintf(line, sizeof(line), "%s S%d %s", RING_MODES[constrain(tel.ringMode, 0, 5)], tel.servoAngle,
             TRAFFIC_MODES[constrain(tel.trafficMode, 0, 5)]);
    display.print(line);
  }
}

void drawMenu() {
  drawHeader("MENU");
  int top = constrain(menuSel - 1, 0, max(0, MENU_COUNT - 4));
  for (int i = 0; i < 4 && top + i < MENU_COUNT; i++) {
    int idx = top + i, y = 17 + i * 12;
    bool sel = (idx == menuSel);
    if (sel) display.fillRoundRect(2, y, 118, 11, 3, WHITE);
    display.setTextColor(sel ? BLACK : WHITE);
    display.setTextSize(1);
    display.setCursor(10, y + 2);
    display.print(MENU[idx].name);
    if (sel) { display.setCursor(108, y + 2); display.print('>'); }
  }
  display.setTextColor(WHITE);
  int trackH = 46, thumbH = max(8, trackH * 4 / MENU_COUNT);
  int thumbY = 17 + (trackH - thumbH) * menuSel / max(1, MENU_COUNT - 1);
  display.drawFastVLine(125, 17, trackH, WHITE);
  display.fillRect(124, thumbY, 3, thumbH, WHITE);
}

void drawRing() {
  drawHeader("RING LIGHT");
  // row 0: mode
  drawLabel("MODE", 0, 20, 40, row == 0);
  display.setCursor(48, 20); display.print('<');
  centerText(RING_MODES[constrain(uiRingMode, 0, 5)], 85, 20, 1);
  display.setCursor(122, 20); display.print('>');
  // row 1: hue
  drawLabel("COLOR", 0, 35, 40, row == 1);
  display.drawFastHLine(50, 41, 72, WHITE);
  for (int i = 0; i <= 12; i++) display.drawFastVLine(50 + i * 6, 38 + (i & 1) * 1, 7 - (i & 1) * 2, WHITE);
  int mx = 50 + uiHue * 72 / 255;
  display.fillTriangle(mx, 46, mx - 3, 51, mx + 3, 51, WHITE);
  // row 2: brightness
  drawLabel("LIGHT", 0, 55, 40, row == 2);
  drawBar(50, 53, 50, 10, uiBri);
  char b[8]; snprintf(b, sizeof(b), "%d%%", uiBri);
  display.setCursor(103, 55); display.print(b);
}

void drawServo() {
  drawHeader("SERVO ARM");
  drawLabel("ANGLE", 0, 20, 40, row == 0);
  display.setTextSize(2);
  char b[8]; snprintf(b, sizeof(b), "%3d", uiAngle);
  display.setCursor(2, 32); display.print(b);
  display.setTextSize(1); display.write(247);
  drawLabel("SWEEP", 0, 52, 40, row == 1);
  display.setCursor(46, 52); display.print(uiSweep ? "ON" : "OFF");
  // dial
  const int cx = 96, cy = 62, r = 30;
  for (int a = 0; a <= 180; a += 4) {
    float th = PI - a * PI / 180.0f;
    display.drawPixel(cx + (int)(r * cosf(th)), cy - (int)(r * sinf(th)), WHITE);
  }
  for (int a = 0; a <= 180; a += 45) {
    float th = PI - a * PI / 180.0f;
    display.drawLine(cx + (int)((r - 4) * cosf(th)), cy - (int)((r - 4) * sinf(th)),
                     cx + (int)(r * cosf(th)), cy - (int)(r * sinf(th)), WHITE);
  }
  float th = PI - tel.servoAngle * PI / 180.0f;     // real position reported by A
  if (millis() - lastLocal < 1500) th = PI - uiAngle * PI / 180.0f;
  display.drawLine(cx, cy, cx + (int)((r - 6) * cosf(th)), cy - (int)((r - 6) * sinf(th)), WHITE);
  display.fillCircle(cx, cy, 3, WHITE);
}

void drawTraffic() {
  drawHeader("TRAFFIC LIGHT");
  drawLabel("MODE", 0, 20, 40, true);
  centerText(TRAFFIC_MODES[constrain(uiTraffic, 0, 5)], 46, 34, 2);
  display.setTextSize(1);
  display.setCursor(2, 54); display.print("< > change  OK=Auto");
  // housing
  display.drawRoundRect(98, 16, 26, 47, 6, WHITE);
  const int ys[3] = {25, 39, 53};
  for (int i = 0; i < 3; i++) {
    bool lit = tel.trafficMask & (1 << i);
    if (lit) display.fillCircle(111, ys[i], 5, WHITE); else display.drawCircle(111, ys[i], 5, WHITE);
  }
}

void drawBuzzer() {
  drawHeader("BUZZER");
  for (int i = 0; i < 4; i++) {
    int y = 17 + i * 12;
    bool sel = (row == i);
    if (sel) display.fillRoundRect(2, y, 84, 11, 3, WHITE);
    display.setTextColor(sel ? BLACK : WHITE);
    display.setTextSize(1);
    display.setCursor(8, y + 2);
    display.print(BUZZ_ITEMS[i]);
  }
  display.setTextColor(WHITE);
  // speaker icon + animated waves
  display.fillRect(94, 34, 5, 8, WHITE);
  display.fillTriangle(99, 34, 99, 41, 106, 46, WHITE);
  display.fillTriangle(99, 34, 106, 29, 106, 46, WHITE);
  int waves = tel.buzz ? 1 + (millis() / 220) % 3 : 0;
  for (int w = 0; w < waves; w++) display.drawCircleHelper(106, 38, 5 + w * 4, 0x6, WHITE);
  if (tel.buzz) { display.setCursor(92, 54); display.print("PLAYING"); }
}

void drawClimate() {
  drawHeader("CLIMATE");
  char b[28], t[12];
  display.setTextSize(1);
  tempStr(t, sizeof(t), tel.dhtT);
  if (isnan(tel.dhtH)) snprintf(b, sizeof(b), "DHT11  %s", t); else snprintf(b, sizeof(b), "DHT11  %s  %.0f%%", t, tel.dhtH);
  display.setCursor(0, 18); display.print(b);
  tempStr(t, sizeof(t), tel.ahtT);
  if (isnan(tel.ahtH)) snprintf(b, sizeof(b), "AHT20  %s", t); else snprintf(b, sizeof(b), "AHT20  %s  %.0f%%", t, tel.ahtH);
  display.setCursor(0, 28); display.print(b);
  tempStr(t, sizeof(t), tel.bmpT);
  if (isnan(tel.bmpP)) snprintf(b, sizeof(b), "BMP280 %s", t); else snprintf(b, sizeof(b), "BMP280 %s %.0fhPa", t, tel.bmpP);
  display.setCursor(0, 38); display.print(b);
  iconSun(0, 51);
  drawBar(14, 50, 70, 9, tel.light);
  snprintf(b, sizeof(b), "%d%%", tel.light);
  display.setCursor(88, 52); display.print(b);
}

void drawLink() {
  drawHeader("LINK INFO");
  char b[30];
  display.setTextSize(1);
  snprintf(b, sizeof(b), "ESP-NOW  CH%d  %s", ESPNOW_CHANNEL, linkOnline ? "ONLINE" : "OFFLINE");
  display.setCursor(0, 18); display.print(b);
  if (rxCount) snprintf(b, sizeof(b), "Last pkt %.1fs  %ddBm", (millis() - lastRx) / 1000.0f, rxRssi);
  else snprintf(b, sizeof(b), "No packets yet");
  display.setCursor(0, 28); display.print(b);
  snprintf(b, sizeof(b), "RX %lu   TX %lu", (unsigned long)rxCount, (unsigned long)txCount);
  display.setCursor(0, 38); display.print(b);
  snprintf(b, sizeof(b), "A uptime %lus", (unsigned long)tel.uptime);
  display.setCursor(0, 48); display.print(b);
  String mac = WiFi.macAddress();
  display.setCursor(0, 57); display.print("B ");
  display.print(mac);
}

void draw() {
  display.clearDisplay();
  display.setTextColor(WHITE);
  switch (screen) {
    case SC_HOME:    drawHome();    break;
    case SC_MENU:    drawMenu();    break;
    case SC_RING:    drawRing();    break;
    case SC_SERVO:   drawServo();   break;
    case SC_TRAFFIC: drawTraffic(); break;
    case SC_BUZZER:  drawBuzzer();  break;
    case SC_CLIMATE: drawClimate(); break;
    case SC_LINK:    drawLink();    break;
  }
  display.display();
}

void splash() {
  for (int i = 0; i <= 100; i += 4) {
    display.clearDisplay();
    display.setTextColor(WHITE);
    centerText("DBANK", 64, 12, 3);
    centerText("SMART NODE HUB", 64, 40, 1);
    drawBar(24, 52, 80, 9, i);
    display.display();
    delay(20);
  }
}

// ============================ ACTIONS ===============================
void setRingMode(int m) { uiRingMode = (m + 6) % 6; sendCmd("rm", uiRingMode); }
void setHue(int h)      { uiHue = h & 255;          sendCmd("rh", uiHue); }
void setBri(int b)      { uiBri = constrain(b, 0, 100); sendCmd("rb", uiBri); }
void setAngle(int a)    { uiAngle = constrain(a, 0, 180); uiSweep = 0; sendCmd("sa", uiAngle); }
void setSweep(int s)    { uiSweep = s; sendCmd("ss", s); }
void setTraffic(int m)  { uiTraffic = (m + 6) % 6; sendCmd("tm", uiTraffic); }

void goScreen(Screen s) { screen = s; row = 0; }

void handleKey(Key k, bool rep) {
  lastInput = millis();
  int dir = (k == K_RIGHT) ? 1 : (k == K_LEFT) ? -1 : 0;
  int vert = (k == K_DOWN) ? 1 : (k == K_UP) ? -1 : 0;

  if (k == K_HOME)     { goScreen(SC_HOME); return; }
  if (k == K_BUZZSTOP) { sendCmd("bz", 0); flash("Buzzer stopped"); return; }
  if (k == K_RINGTOG)  { setRingMode(uiRingMode ? 0 : 2); return; }
  if (k == K_BACK) {
    if (screen == SC_MENU) goScreen(SC_HOME);
    else if (screen != SC_HOME) goScreen(SC_MENU);
    return;
  }

  switch (screen) {
    case SC_HOME:
      if (k == K_OK || k == K_RIGHT || k == K_DOWN) goScreen(SC_MENU);
      break;
    case SC_MENU:
      if (vert) menuSel = (menuSel + vert + MENU_COUNT) % MENU_COUNT;
      else if (k == K_OK || k == K_RIGHT) goScreen(MENU[menuSel].target);
      else if (k == K_LEFT) goScreen(SC_HOME);
      break;
    case SC_RING:
      if (vert) row = (row + vert + 3) % 3;
      else if (row == 0 && dir) setRingMode(uiRingMode + dir);
      else if (row == 1 && dir) setHue(uiHue + dir * (rep ? 10 : 5));
      else if (row == 2 && dir) setBri(uiBri + dir * (rep ? 10 : 5));
      else if (k == K_OK) setRingMode(uiRingMode ? 0 : 2);
      break;
    case SC_SERVO:
      if (vert) row = (row + vert + 2) % 2;
      else if (row == 0 && dir) setAngle(uiAngle + dir * (rep ? 10 : 5));
      else if (row == 0 && k == K_OK) setAngle(90);
      else if (row == 1 && (dir || k == K_OK)) setSweep(!uiSweep);
      break;
    case SC_TRAFFIC:
      if (dir) setTraffic(uiTraffic + dir);
      else if (vert) setTraffic(uiTraffic - vert);
      else if (k == K_OK) setTraffic(1);
      break;
    case SC_BUZZER:
      if (vert) row = (row + vert + 4) % 4;
      else if (k == K_OK || k == K_RIGHT) sendCmd("bz", row == 3 ? 0 : row + 1);
      break;
    case SC_CLIMATE:
      if (k == K_OK) useF = !useF;          // OK toggles C / F
      break;
    case SC_LINK:
      break;
  }
}

// ============================== IR ==================================
Key mapKey(uint16_t c) {
  switch (c) {
    case 0x18: case 0x15: return K_UP;        // 2, VOL+
    case 0x52: case 0x07: return K_DOWN;      // 8, VOL-
    case 0x08: case 0x44: return K_LEFT;      // 4, PREV
    case 0x5A: case 0x40: return K_RIGHT;     // 6, NEXT
    case 0x1C: case 0x43: return K_OK;        // 5, PLAY/PAUSE
    case 0x45: case 0x09: return K_BACK;      // CH-, EQ
    case 0x46: case 0x16: return K_HOME;      // CH, 0
    case 0x19:            return K_BUZZSTOP;  // 100+
    case 0x0D:            return K_RINGTOG;   // 200+
    default:              return K_NONE;
  }
}

void pollIR() {
  static uint32_t lastKeyMs = 0;
  if (!IrReceiver.decode()) return;
  if (IrReceiver.decodedIRData.protocol != UNKNOWN) {
    bool rep = IrReceiver.decodedIRData.flags & IRDATA_FLAGS_IS_REPEAT;
    uint16_t cmd = IrReceiver.decodedIRData.command;
    if (!rep) Serial.printf("IR proto=%d addr=0x%X cmd=0x%X\n",
                            (int)IrReceiver.decodedIRData.protocol,
                            (unsigned)IrReceiver.decodedIRData.address, (unsigned)cmd);
    Key k = mapKey(cmd);
    bool arrow = (k == K_UP || k == K_DOWN || k == K_LEFT || k == K_RIGHT);
    if (k != K_NONE && (!rep || arrow) && millis() - lastKeyMs > 90) {
      lastKeyMs = millis();
      handleKey(k, rep);
    }
  }
  IrReceiver.resume();
}

// ============================= SETUP/LOOP ===========================
void setup() {
  Serial.begin(115200);
  Wire.begin();                        // SDA 21, SCL 22
  Wire.setClock(400000);
  if (!display.begin(SSD1306_SWITCHCAPVCC, OLED_ADDR)) {
    Serial.println("SSD1306 not found (check wiring / OLED_ADDR)");
    for (;;) delay(1000);
  }
  display.setTextWrap(false);
  splash();
  IrReceiver.begin(IR_RECEIVE_PIN, DISABLE_LED_FEEDBACK);
  radioBegin();
  Serial.print("Node B MAC: ");
  Serial.println(WiFi.macAddress());
  sendCmd("ping", 0);
  lastInput = millis();
}

void loop() {
  pollIR();
  pollRadio();
  if (screen != SC_HOME && millis() - lastInput > IDLE_HOME_MS) goScreen(SC_HOME);
  if (millis() - lastDraw >= 70) {
    lastDraw = millis();
    draw();
  }
}
