#include <WiFi.h>
#include <HTTPClient.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

#include "driver/i2s.h"
#include <vector>

// =====================================================
// 1. WIFI AND BACKEND CONFIGURATION
// =====================================================

const char* WIFI_SSID = "DBANK_WIFI";
const char* WIFI_PASSWORD = "D@@nn33ll1234";

// Replace with the computer's LAN IP address
const char* BACKEND_URL = "http://192.168.8.139:8000/api/voice";

// =====================================================
// 2. OLED CONFIGURATION
// =====================================================

#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define OLED_RESET -1
#define OLED_ADDRESS 0x3C

Adafruit_SSD1306 display(
  SCREEN_WIDTH,
  SCREEN_HEIGHT,
  &Wire,
  OLED_RESET
);

// =====================================================
// 3. GPIO PIN CONFIGURATION
// =====================================================

// OLED
#define OLED_SDA 21
#define OLED_SCL 22

// INMP441 microphone
#define MIC_SCK 26
#define MIC_WS  25
#define MIC_SD  34

// MAX98357A amplifier
#define SPK_BCLK 27
#define SPK_LRC  14
#define SPK_DIN  13

// MAX98357A shutdown / enable
#define AMP_SD_PIN 32

// LED
#define LED_PIN 4

// =====================================================
// 4. I2S CONFIGURATION
// =====================================================

#define MIC_I2S_PORT I2S_NUM_0
#define SPK_I2S_PORT I2S_NUM_1

#define MIC_SAMPLE_RATE 16000
#define SPK_SAMPLE_RATE 24000

#define RECORD_SECONDS 3

// =====================================================
// 5. AUDIO CONFIGURATION
// =====================================================

#define MIC_CHUNK_SAMPLES 256
#define SPK_CHUNK_SAMPLES 256

// =====================================================
// 6. GLOBAL VARIABLES
// =====================================================

bool isRecording = false;

std::vector<int16_t> recordedAudio;

// HTTP response header
const char* responseHeaders[] = {
  "X-Reply-Text"
};

// =====================================================
// 7. OLED DISPLAY FUNCTIONS
// =====================================================

void showMessage(const String& line1,
                 const String& line2 = "") {

  display.clearDisplay();

  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);

  display.setCursor(0, 10);
  display.println(line1);

  display.setCursor(0, 30);
  display.println(line2);

  display.display();
}

// =====================================================
// 8. WIFI CONNECTION
// =====================================================

void connectWiFi() {

  showMessage("Connecting to WiFi...");

  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

  Serial.print("Connecting to WiFi");

  unsigned long startTime = millis();

  while (WiFi.status() != WL_CONNECTED) {

    delay(500);
    Serial.print(".");

    if (millis() - startTime > 30000) {
      Serial.println("\nWiFi connection timeout");
      showMessage("WiFi failed", "Check credentials");
      return;
    }
  }

  Serial.println();
  Serial.println("WiFi connected!");

  Serial.print("ESP32 IP address: ");
  Serial.println(WiFi.localIP());

  showMessage("WiFi connected", WiFi.localIP().toString());

  delay(1500);
}

// =====================================================
// 9. MICROPHONE INITIALIZATION
// =====================================================

bool initializeMicrophone() {

  Serial.println("Initializing INMP441 microphone...");

  i2s_config_t micConfig = {};

  micConfig.mode =
    (i2s_mode_t)(I2S_MODE_MASTER | I2S_MODE_RX);

  micConfig.sample_rate = MIC_SAMPLE_RATE;

  micConfig.bits_per_sample = I2S_BITS_PER_SAMPLE_32BIT;

  micConfig.channel_format = I2S_CHANNEL_FMT_ONLY_LEFT;

  micConfig.communication_format = I2S_COMM_FORMAT_I2S;

  micConfig.intr_alloc_flags = ESP_INTR_FLAG_LEVEL1;

  micConfig.dma_buf_count = 8;
  micConfig.dma_buf_len = 256;

  micConfig.use_apll = false;

  micConfig.tx_desc_auto_clear = false;

  micConfig.fixed_mclk = 0;

  esp_err_t result = i2s_driver_install(
    MIC_I2S_PORT,
    &micConfig,
    0,
    NULL
  );

  if (result != ESP_OK) {
    Serial.printf("Microphone I2S install failed: %d\n", result);
    return false;
  }

  i2s_pin_config_t micPins = {};

  micPins.bck_io_num = MIC_SCK;
  micPins.ws_io_num = MIC_WS;

  micPins.data_out_num = I2S_PIN_NO_CHANGE;
  micPins.data_in_num = MIC_SD;

  // IMPORTANT: Do not allow MCLK to use GPIO0.
  micPins.mck_io_num = I2S_PIN_NO_CHANGE;

  result = i2s_set_pin(MIC_I2S_PORT, &micPins);

  if (result != ESP_OK) {
    Serial.printf("Microphone pin configuration failed: %d\n", result);
    return false;
  }

  i2s_zero_dma_buffer(MIC_I2S_PORT);

  Serial.println("Microphone initialized.");

  return true;
}

// =====================================================
// 10. SPEAKER INITIALIZATION
// =====================================================

bool initializeSpeaker() {

  Serial.println("Initializing MAX98357A speaker...");

  // Keep amplifier disabled during initialization.
  digitalWrite(AMP_SD_PIN, LOW);

  i2s_config_t speakerConfig = {};

  speakerConfig.mode =
    (i2s_mode_t)(I2S_MODE_MASTER | I2S_MODE_TX);

  speakerConfig.sample_rate = SPK_SAMPLE_RATE;

  speakerConfig.bits_per_sample = I2S_BITS_PER_SAMPLE_16BIT;

  speakerConfig.channel_format = I2S_CHANNEL_FMT_RIGHT_LEFT;

  speakerConfig.communication_format = I2S_COMM_FORMAT_I2S;

  speakerConfig.intr_alloc_flags = ESP_INTR_FLAG_LEVEL1;

  speakerConfig.dma_buf_count = 8;
  speakerConfig.dma_buf_len = 256;

  speakerConfig.use_apll = false;

  speakerConfig.tx_desc_auto_clear = true;

  speakerConfig.fixed_mclk = 0;

  esp_err_t result = i2s_driver_install(
    SPK_I2S_PORT,
    &speakerConfig,
    0,
    NULL
  );

  if (result != ESP_OK) {
    Serial.printf("Speaker I2S install failed: %d\n", result);
    return false;
  }

  i2s_pin_config_t speakerPins = {};

  speakerPins.bck_io_num = SPK_BCLK;
  speakerPins.ws_io_num = SPK_LRC;

  speakerPins.data_out_num = SPK_DIN;
  speakerPins.data_in_num = I2S_PIN_NO_CHANGE;

  // IMPORTANT: Disable unused MCLK.
  speakerPins.mck_io_num = I2S_PIN_NO_CHANGE;

  result = i2s_set_pin(SPK_I2S_PORT, &speakerPins);

  if (result != ESP_OK) {
    Serial.printf("Speaker pin configuration failed: %d\n", result);
    return false;
  }

  i2s_zero_dma_buffer(SPK_I2S_PORT);

  // Keep amplifier OFF until actual audio playback.
  digitalWrite(AMP_SD_PIN, LOW);

  Serial.println("Speaker initialized.");

  return true;
}

// =====================================================
// 11. RECORD AUDIO FROM INMP441
// =====================================================

bool recordAudio() {

  Serial.println("Recording started...");

  showMessage("Listening...", "Speak now");

  digitalWrite(LED_PIN, HIGH);

  recordedAudio.clear();

  const size_t totalSamples =
    MIC_SAMPLE_RATE * RECORD_SECONDS;

  recordedAudio.reserve(totalSamples);

  int32_t rawSamples[MIC_CHUNK_SAMPLES];

  size_t capturedSamples = 0;

  while (capturedSamples < totalSamples) {

    size_t bytesRead = 0;

    esp_err_t result = i2s_read(
      MIC_I2S_PORT,
      rawSamples,
      sizeof(rawSamples),
      &bytesRead,
      portMAX_DELAY
    );

    if (result != ESP_OK) {
      Serial.println("Microphone read error.");
      digitalWrite(LED_PIN, LOW);
      return false;
    }

    size_t samplesRead = bytesRead / sizeof(int32_t);

    for (size_t i = 0; i < samplesRead; i++) {

      // INMP441 provides 24-bit audio in a 32-bit slot.
      // Keep the upper 16 bits for signed PCM audio.
      int16_t sample = (int16_t)(rawSamples[i] >> 16);

      recordedAudio.push_back(sample);

      capturedSamples++;

      if (capturedSamples >= totalSamples) {
        break;
      }
    }
  }

  digitalWrite(LED_PIN, LOW);

  Serial.printf(
    "Recording complete. Samples: %u\n",
    (unsigned int)recordedAudio.size()
  );

  return true;
}

// =====================================================
// 12. URL DECODE RESPONSE TEXT
// =====================================================

String urlDecode(const String& input) {

  String output = "";

  for (size_t i = 0; i < input.length(); i++) {

    char c = input[i];

    if (c == '+') {
      output += ' ';
    }
    else if (c == '%' && i + 2 < input.length()) {

      char hex[3] = {
        input[i + 1],
        input[i + 2],
        '\0'
      };

      output += (char)strtol(hex, NULL, 16);

      i += 2;
    }
    else {
      output += c;
    }
  }

  return output;
}

// =====================================================
// 13. PLAY AUDIO THROUGH MAX98357A
// =====================================================

void playAudio(const uint8_t* data, size_t length) {

  if (data == nullptr || length < 2) {
    Serial.println("No audio data received.");
    return;
  }

  // Ensure the amplifier starts with a clean buffer.
  digitalWrite(AMP_SD_PIN, LOW);

  i2s_zero_dma_buffer(SPK_I2S_PORT);

  // Enable MAX98357A only when audio is ready.
  digitalWrite(AMP_SD_PIN, HIGH);

  delay(20);

  showMessage("AI speaking...");

  digitalWrite(LED_PIN, HIGH);

  const int16_t* monoAudio =
    reinterpret_cast<const int16_t*>(data);

  size_t sampleCount = length / sizeof(int16_t);

  int16_t stereoAudio[SPK_CHUNK_SAMPLES * 2];

  size_t offset = 0;

  while (offset < sampleCount) {

    size_t count = min(
      (size_t)SPK_CHUNK_SAMPLES,
      sampleCount - offset
    );

    // Duplicate mono audio into left and right channels.
    for (size_t i = 0; i < count; i++) {

      int16_t sample = monoAudio[offset + i];

      stereoAudio[i * 2] = sample;
      stereoAudio[i * 2 + 1] = sample;
    }

    size_t bytesWritten = 0;

    esp_err_t result = i2s_write(
      SPK_I2S_PORT,
      stereoAudio,
      count * 2 * sizeof(int16_t),
      &bytesWritten,
      portMAX_DELAY
    );

    if (result != ESP_OK || bytesWritten == 0) {

      Serial.println("Speaker playback error.");

      break;
    }

    offset += count;
  }

  // Allow the final samples to finish playing.
  delay(50);

  digitalWrite(LED_PIN, LOW);

  // Shut down amplifier when playback finishes.
  digitalWrite(AMP_SD_PIN, LOW);

  i2s_zero_dma_buffer(SPK_I2S_PORT);

  Serial.println("Audio playback finished.");

  showMessage("Ready", "Press R to speak");
}

// =====================================================
// 14. SEND AUDIO TO CLOUD BACKEND
// =====================================================

void sendAudioToBackend() {

  if (WiFi.status() != WL_CONNECTED) {

    Serial.println("WiFi is disconnected.");

    showMessage("WiFi disconnected");

    return;
  }

  if (recordedAudio.empty()) {

    Serial.println("No recorded audio.");

    return;
  }

  Serial.println("Sending audio to backend...");

  showMessage("Processing...", "Contacting AI");

  HTTPClient http;

  http.setTimeout(180000); // 3 minutes

  // Collect the response text header.
  http.collectHeaders(responseHeaders, 1);

  if (!http.begin(BACKEND_URL)) {

    Serial.println("Could not initialize HTTP connection.");

    showMessage("Backend error");

    return;
  }

  http.addHeader("Content-Type", "application/octet-stream");

  const uint8_t* audioBytes =
    reinterpret_cast<const uint8_t*>(recordedAudio.data());

  size_t audioLength = recordedAudio.size() * sizeof(int16_t);

  Serial.println("Uploading audio to Gemini backend...");
  Serial.printf("Audio size: %u bytes\n", (unsigned int)audioLength);

  unsigned long requestStart = millis();

  int responseCode = http.POST((uint8_t*)audioBytes, audioLength);

  Serial.printf("HTTP request duration: %lu ms\n", millis() - requestStart);

  Serial.printf("Backend HTTP status: %d\n", responseCode);
  Serial.println(http.errorToString(responseCode));

  Serial.printf("Backend HTTP status: %d\n", responseCode);

  if (responseCode != HTTP_CODE_OK) {

    Serial.println("Backend request failed.");

    if (responseCode > 0) {
      Serial.println(http.getString());
    }

    http.end();

    showMessage("AI request failed", "Check backend");

    return;
  }

  // Retrieve the assistant's text response.
  String replyText = http.header("X-Reply-Text");

  if (replyText.length() > 0) {

    replyText = urlDecode(replyText);

    Serial.println("AI response:");
    Serial.println(replyText);

    showMessage("AI response:", replyText.substring(0, 20));
  }

  // Retrieve binary TTS audio.
  int contentLength = http.getSize();

  WiFiClient* stream = http.getStreamPtr();

  std::vector<uint8_t> responseAudio;

  if (contentLength > 0) {
    responseAudio.reserve((size_t)contentLength);
  }

  uint8_t buffer[1024];

  unsigned long lastDataTime = millis();

  while (http.connected() || stream->available()) {

    size_t availableBytes = stream->available();

    if (availableBytes > 0) {

      size_t bytesToRead = min(
        availableBytes,
        sizeof(buffer)
      );

      int bytesRead = stream->readBytes(
        buffer,
        bytesToRead
      );

      if (bytesRead > 0) {

        responseAudio.insert(
          responseAudio.end(),
          buffer,
          buffer + bytesRead
        );

        lastDataTime = millis();
      }
    }
    else {

      if (contentLength > 0 &&
          responseAudio.size() >= (size_t)contentLength) {
        break;
      }

      if (millis() - lastDataTime > 10000) {
        Serial.println("Timed out receiving audio.");
        break;
      }

      delay(5);
    }
  }

  http.end();

  Serial.printf(
    "Received audio bytes: %u\n",
    (unsigned int)responseAudio.size()
  );

  if (responseAudio.size() < 2) {

    Serial.println("No usable TTS audio received.");

    showMessage("No audio received");

    return;
  }

  playAudio(
    responseAudio.data(),
    responseAudio.size()
  );
}

// =====================================================
// 15. COMPLETE VOICE ASSISTANT WORKFLOW
// =====================================================

void startVoiceAssistant() {

  if (isRecording) {
    return;
  }

  if (WiFi.status() != WL_CONNECTED) {

    Serial.println("Connect to WiFi first.");

    showMessage("WiFi unavailable");

    return;
  }

  isRecording = true;

  bool success = recordAudio();

  if (success) {
    sendAudioToBackend();
  }

  recordedAudio.clear();

  isRecording = false;

  showMessage("Ready", "Press R to speak");
}

// =====================================================
// 16. SERIAL COMMAND HANDLER
// =====================================================

void handleSerialCommands() {

  if (!Serial.available()) {
    return;
  }

  char command = Serial.read();

  command = tolower(command);

  if (command == 'r') {

    Serial.println("Voice assistant activated.");

    startVoiceAssistant();
  }

  else if (command == 's') {

    Serial.println("Stop command received.");

    // The current recording implementation is fixed-duration.
    // This command does not interrupt an active recording.
    showMessage("Ready", "Press R to speak");
  }

  else if (command == 'h') {

    Serial.println();
    Serial.println("ESP32 Voice Assistant Commands");
    Serial.println("--------------------------------");
    Serial.println("R = Record and send voice command");
    Serial.println("S = Stop / return to ready display");
    Serial.println("H = Show help");
    Serial.println();
  }
}

// =====================================================
// 17. SETUP
// =====================================================

void setup() {

  Serial.begin(115200);

  delay(1000);

  Serial.println();
  Serial.println("================================");
  Serial.println("ESP32 CLOUD VOICE ASSISTANT");
  Serial.println("================================");

  // Configure LED.
  pinMode(LED_PIN, OUTPUT);
  digitalWrite(LED_PIN, LOW);

  // IMPORTANT:
  // Keep amplifier shut down from the very beginning.
  pinMode(AMP_SD_PIN, OUTPUT);
  digitalWrite(AMP_SD_PIN, LOW);

  // Initialize I2C OLED.
  Wire.begin(OLED_SDA, OLED_SCL);

  if (!display.begin(
        SSD1306_SWITCHCAPVCC,
        OLED_ADDRESS
      )) {

    Serial.println("OLED initialization failed.");
  }
  else {

    showMessage("Starting...", "ESP32 Voice AI");
  }

  // Connect to WiFi.
  connectWiFi();

  // Initialize microphone.
  if (!initializeMicrophone()) {

    Serial.println("Microphone initialization failed.");

    showMessage("Microphone error");

    while (true) {
      delay(1000);
    }
  }

  // Initialize speaker.
  if (!initializeSpeaker()) {

    Serial.println("Speaker initialization failed.");

    showMessage("Speaker error");

    while (true) {
      delay(1000);
    }
  }

  // Ensure amplifier remains disabled after initialization.
  digitalWrite(AMP_SD_PIN, LOW);

  Serial.println();
  Serial.println("Voice assistant is ready.");
  Serial.println("Press R to record.");
  Serial.println("Press H for help.");

  showMessage("Ready", "Press R to speak");
}

// =====================================================
// 18. MAIN LOOP
// =====================================================

void loop() {

  handleSerialCommands();

  delay(10);
}

