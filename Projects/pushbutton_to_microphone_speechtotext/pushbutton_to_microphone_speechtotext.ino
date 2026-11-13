#include <WiFi.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <driver/i2s.h>

// --- Wi-Fi & API Credentials ---
const char* ssid = "DBANK_WIFI";
const char* password = "D@@nn33ll1234";
const char* witAiToken = "PYDF7EOWM4XCR6DNHQTIKOMM3RNGU477";

// --- Pin Definitions ---
#define BUTTON_PIN 14
#define I2S_PORT I2S_NUM_0
#define I2S_SCK  33
#define I2S_WS   25
#define I2S_SD   32

// Audio recording buffer settings (Record ~3 seconds of audio at 16kHz)
#define SAMPLE_RATE 16000
#define RECORD_SECONDS 3
#define BUFFER_SIZE (SAMPLE_RATE * 2 * RECORD_SECONDS) // 16-bit = 2 bytes per sample

int16_t* audioBuffer = NULL;

void setup() {
  Serial.begin(115200);
  delay(1000);

  // Configure Button
  pinMode(BUTTON_PIN, INPUT_PULLUP);

  // Allocate memory for audio buffer in Heap
  audioBuffer = (int16_t*)malloc(BUFFER_SIZE);
  if (audioBuffer == NULL) {
    Serial.println("Failed to allocate audio buffer memory!");
  }

  // Connect to Wi-Fi
  WiFi.begin(ssid, password);
  Serial.print("Connecting to Wi-Fi");
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\nWi-Fi Connected!");

  // Configure I2S Microphone
  const i2s_pin_config_t pin_config = {
    .bck_io_num = I2S_SCK,
    .ws_io_num = I2S_WS,
    .data_out_num = I2S_PIN_NO_CHANGE,
    .data_in_num = I2S_SD
  };

  const i2s_config_t i2s_config = {
    .mode = (i2s_mode_t)(I2S_MODE_MASTER | I2S_MODE_RX),
    .sample_rate = SAMPLE_RATE,
    .bits_per_sample = I2S_BITS_PER_SAMPLE_32BIT, // Mic outputs 32-bit container
    .channel_format = I2S_CHANNEL_FMT_ONLY_LEFT,
    .communication_format = I2S_COMM_FORMAT_I2S,
    .intr_alloc_flags = ESP_INTR_FLAG_LEVEL1,
    .dma_buf_count = 8,
    .dma_buf_len = 512,
    .use_apll = false
  };

  i2s_driver_install(I2S_PORT, &i2s_config, 0, NULL);
  i2s_set_pin(I2S_PORT, &pin_config);

  Serial.println("Ready! Press and hold the button, then speak into the microphone.");
}

void loop() {
  // Check if button is pressed (LOW because of INPUT_PULLUP)
  if (digitalRead(BUTTON_PIN) == LOW) {
    Serial.println("\n[Recording... Speak now!]");

    size_t bytesRead = 0;
    int totalBytesRecorded = 0;

    // Flush old I2S data
    i2s_zero_dma_buffer(I2S_PORT);

    // Record audio until buffer is full or button is released
    unsigned long startTime = millis();
    while (totalBytesRecorded < BUFFER_SIZE) {
      int32_t tempSample = 0;
      size_t chunkRead = 0;
      
      i2s_read(I2S_PORT, &tempSample, sizeof(tempSample), &chunkRead, portMAX_DELAY);
      
      if (chunkRead > 0) {
        // Downsample 32-bit I2S frame down to 16-bit PCM for the API
        audioBuffer[totalBytesRecorded / 2] = (int16_t)(tempSample >> 16);
        totalBytesRecorded += 2;
      }

      // Stop early if user releases button
      if (digitalRead(BUTTON_PIN) == HIGH && (millis() - startTime > 1000)) {
        break; 
      }
    }

    Serial.println("[Recording Finished. Transcribing...] ");
    sendAudioToCloud(audioBuffer, totalBytesRecorded);
    
    // Debounce delay after release
    delay(500);
  }
}

void sendAudioToCloud(int16_t* buffer, int length) {
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("Error: Lost Wi-Fi connection");
    return;
  }

  HTTPClient http;
  
  // Wit.ai Speech API Endpoint
  http.begin("https://api.wit.ai/speech?v=20230215");
  
  // Set headers (Wit.ai accepts raw 16kHz 16-bit signed PCM audio streams)
  http.addHeader("Authorization", String("Bearer ") + witAiToken);
  http.addHeader("Content-Type", "audio/raw; encoding=signed-integer; bits=16; rate=16000; endian=little");

  // Send POST request with raw audio buffer payload
  int httpResponseCode = http.POST((uint8_t*)buffer, length);

  if (httpResponseCode > 0) {
    String response = http.getString();
    Serial.print("API Response: ");
    Serial.println(response);

    // Parse JSON response to extract the transcribed text string
    String finalTranscript = extractFinalText(response);
    
    if (finalTranscript.length() > 0) {
      Serial.print(">> Final Transcribed Text: ");
      Serial.println(finalTranscript);
    } else {
      Serial.println(">> No speech recognized.");
    }
  } else {
    Serial.print("Error on HTTP request: ");
    Serial.println(httpResponseCode);
  }

  http.end();
}

String extractFinalText(String response) {
  String finalTranscript = "";
  int startIndex = 0;

  // Loop through the response string line-by-line (NDJSON format)
  while (startIndex < response.length()) {
    int endIndex = response.indexOf('\n', startIndex);
    if (endIndex == -1) {
      endIndex = response.length(); // Handle the last line if no trailing newline exists
    }

    String line = response.substring(startIndex, endIndex);
    line.trim();

    if (line.length() > 0) {
      JsonDocument doc;
      DeserializationError error = deserializeJson(doc, line);
      
      if (!error) {
        bool isFinal = doc["is_final"] | false;
        const char* text = doc["text"];

        // Update the transcript text as it builds up from partial to final
        if (text != nullptr) {
          finalTranscript = String(text);
        }
        
        // Stop once we hit the official final result line
        if (isFinal) {
          break;
        }
      }
    }
    startIndex = endIndex + 1;
  }

  return finalTranscript;
}
