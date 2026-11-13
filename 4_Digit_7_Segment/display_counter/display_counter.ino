#include <IRremote.hpp>

// Define the GPIO pin connected to the VS1838B OUT pin
const int IR_RECEIVE_PIN = 4;

void setup() {
  // Start Serial Monitor at 115200 baud
  Serial.begin(115200);
  delay(1000);

  // Initialize the IR receiver
  IrReceiver.begin(IR_RECEIVE_PIN, ENABLE_LED_FEEDBACK);
  
  Serial.print(F("IR Receiver ready on pin "));
  Serial.println(IR_RECEIVE_PIN);
}

void loop() {
  // Check if an IR signal has been received and decoded
  if (IrReceiver.decode()) {
    
    // Print a summary of the received signal to the Serial Monitor
    Serial.println("----------------------------------------");
    IrReceiver.printIRResultShort(&Serial);
    
    // Print the specific protocol and hex value
    Serial.print(F("Protocol: "));
    Serial.println(getProtocolString(IrReceiver.decodedIRData.protocol));
    
    Serial.print(F("Hex Code: 0x"));
    Serial.println(IrReceiver.decodedIRData.command, HEX);
    
    Serial.print(F("Raw Data: 0x"));
    Serial.println(IrReceiver.decodedIRData.decodedRawData, HEX);
    
    // Enable receiving of the next value
    IrReceiver.resume();
  }
}