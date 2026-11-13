#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <IRremote.hpp>
#include <WiFi.h>
#include <Keypad.h>

// OLED Display Settings
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define OLED_RESET    -1
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

// IR Receiver Pin
const int IR_RECEIVE_PIN = 4;

// TODO: Replace these placeholder Hex codes with your remote's actual button codes
#define IR_BUTTON_UP   0x18 
#define IR_BUTTON_DOWN 0x52 
#define IR_BUTTON_OK   0x1C 

// 4x4 Membrane Keypad Setup (Mapped to pins 1-8 of the ribbon cable)
const byte ROWS = 4; 
const byte COLS = 4; 
char keys[ROWS][COLS] = {
  {'1','2','3','A'},
  {'4','5','6','B'},
  {'7','8','9','C'},
  {'*','0','#','D'}
};

// Row pins (Pins 1, 2, 3, 4 of the membrane cable connector)
byte rowPins[ROWS] = {13, 12, 14, 27}; 
// Column pins (Pins 5, 6, 7, 8 of the membrane cable connector)
byte colPins[COLS] = {26, 25, 33, 32}; 

Keypad keypad = Keypad(makeKeymap(keys), rowPins, colPins, ROWS, COLS);

// Credential Lookup Structure
struct WifiCredential {
  String lookupCode;
  String username;
  String password;
};

// Credential Lookup Array
WifiCredential credentialTable[] = {
  { "783898", "DBANK_WIFI", "D@@nn33ll1234" },
  { "242783", "DbankWiFi2", "D@@nn33ll123" }
};
const int credentialCount = sizeof(credentialTable) / sizeof(credentialTable[0]);

// Navigation & State variables
int networkCount = 0;
int selectedIndex = 0;
int topVisibleIndex = 0;
const int maxVisibleLines = 4;
bool enteringPassword = false;
String inputBuffer = "";
String targetSSID = "";

void setup() {
  Serial.begin(115200);
  delay(1000);

  // Initialize OLED
  if(!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) {
    Serial.println(F("SSD1306 allocation failed"));
    for(;;);
  }

  // Initialize IR Receiver
  IrReceiver.begin(IR_RECEIVE_PIN, ENABLE_LED_FEEDBACK);

  // Show Loading Screen
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(0, 0);
  display.println(F("Scanning WiFi..."));
  display.display();

  // Scan for Wi-Fi networks
  WiFi.mode(WIFI_STA);
  WiFi.disconnect();
  networkCount = WiFi.scanNetworks();

  delay(500);
  drawMenu();
}

void loop() {
  // 1. Handle IR Remote Navigation
  if (IrReceiver.decode()) {
    unsigned long keyCommand = IrReceiver.decodedIRData.command;

    if (!enteringPassword && networkCount > 0) {
      if (keyCommand == IR_BUTTON_UP) {
        selectedIndex--;
        if (selectedIndex < 0) selectedIndex = networkCount - 1;
        updateScrollWindow();
        drawMenu();
      } 
      else if (keyCommand == IR_BUTTON_DOWN) {
        selectedIndex++;
        if (selectedIndex >= networkCount) selectedIndex = 0;
        updateScrollWindow();
        drawMenu();
      } 
      else if (keyCommand == IR_BUTTON_OK) {
        targetSSID = WiFi.SSID(selectedIndex);
        enteringPassword = true;
        inputBuffer = "";
        drawPasswordPrompt();
      }
    }
    IrReceiver.resume();
  }

  // 2. Handle Membrane Keypad Input for Passcode Lookup
  if (enteringPassword) {
    char key = keypad.getKey();
    if (key) {
      if (key == '#') {
        processCredentials(inputBuffer);
      } 
      else if (key == '*') {
        enteringPassword = false;
        drawMenu();
      } 
      else {
        if (inputBuffer.length() < 10) {
          inputBuffer += key;
          drawPasswordPrompt();
        }
      }
    }
  }
}

void updateScrollWindow() {
  if (selectedIndex < topVisibleIndex) {
    topVisibleIndex = selectedIndex;
  } else if (selectedIndex >= topVisibleIndex + maxVisibleLines) {
    topVisibleIndex = selectedIndex - maxVisibleLines + 1;
  }
}

void drawMenu() {
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);

  display.setCursor(0, 0);
  display.print(F("WiFi Networks ("));
  display.print(networkCount);
  display.print(F("):"));

  if (networkCount == 0) {
    display.setCursor(0, 20);
    display.print(F("No networks found!"));
    display.display();
    return;
  }

  int yPos = 12;
  for (int i = topVisibleIndex; i < min(topVisibleIndex + maxVisibleLines, networkCount); i++) {
    yPos += 12;
    display.setCursor(0, yPos);

    if (i == selectedIndex) {
      display.print(F(">"));
    } else {
      display.print(F(" "));
    }

    display.print(i + 1);
    display.print(F(". "));

    String ssid = WiFi.SSID(i);
    if (ssid.length() > 14) {
      ssid = ssid.substring(0, 14) + "..";
    }
    display.print(ssid);
  }
  display.display();
}

void drawPasswordPrompt() {
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);

  display.setCursor(0, 0);
  display.print(F("Enter Code for:"));
  display.setCursor(0, 12);
  display.print(targetSSID.substring(0, 16));

  display.setCursor(0, 30);
  display.print(F("Code: "));
  display.print(inputBuffer);

  display.setCursor(0, 52);
  display.print(F("#=OK  *=Cancel"));
  display.display();
}

void processCredentials(String code) {
  String matchedUser = "";
  String matchedPass = "";
  bool found = false;

  for (int i = 0; i < credentialCount; i++) {
    if (credentialTable[i].lookupCode == code) {
      matchedUser = credentialTable[i].username;
      matchedPass = credentialTable[i].password;
      found = true;
      break;
    }
  }

  display.clearDisplay();
  display.setCursor(0, 10);

  if (!found) {
    display.print(F("Invalid Code!"));
    display.display();
    delay(2000);
    inputBuffer = "";
    drawPasswordPrompt();
    return;
  }

  display.print(F("Connecting..."));
  display.setCursor(0, 25);
  display.print(targetSSID);
  display.display();

  WiFi.begin(targetSSID.c_str(), matchedPass.c_str());

  int attempts = 0;
  while (WiFi.status() != WL_CONNECTED && attempts < 25) {
    delay(500);
    attempts++;
  }

  display.clearDisplay();
  display.setCursor(0, 20);
  if (WiFi.status() == WL_CONNECTED) {
    display.print(F("Connected Success!"));
    Serial.println("\nConnected successfully using credentials!");
  } else {
    display.print(F("Connection Failed!"));
    Serial.println("\nFailed to connect with provided credentials.");
  }
  display.display();
  delay(3000);
  
  enteringPassword = false;
  drawMenu();
}