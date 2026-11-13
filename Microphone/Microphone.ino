#include <driver/i2s.h>

// I2S Peripheral configuration
#define I2S_PORT I2S_NUM_0

// Define ESP32 GPIO pins connected to the microphone
#define I2S_SCK  33  // Serial Clock (BCLK)
#define I2S_WS   25  // Word Select (LRC)
#define I2S_SD   32  // Serial Data (DOUT)

void setup() {
  Serial.begin(115200);
  delay(1000);
  Serial.println("Initializing I2S Microphone...");

  // Configure I2S pin mapping
  const i2s_pin_config_t pin_config = {
    .bck_io_num = I2S_SCK,
    .ws_io_num = I2S_WS,
    .data_out_num = I2S_PIN_NO_CHANGE, // Not used for a microphone (input only)
    .data_in_num = I2S_SD
  };

  // Configure I2S operational modes
  const i2s_config_t i2s_config = {
    .mode = (i2s_mode_t)(I2S_MODE_MASTER | I2S_MODE_RX), // Master Receiver mode
    .sample_rate = 16000,                               // 16 kHz sampling rate
    .bits_per_sample = I2S_BITS_PER_SAMPLE_32BIT,       // 32-bit data format (standard for these mics)
    .channel_format = I2S_CHANNEL_FMT_ONLY_LEFT,        // Single channel mode
    .communication_format = I2S_COMM_FORMAT_I2S,        // Standard I2S format
    .intr_alloc_flags = ESP_INTR_FLAG_LEVEL1,           // Interrupt level
    .dma_buf_count = 4,
    .dma_buf_len = 512,
    .use_apll = false
  };

  // Install and start the I2S driver
  i2s_driver_install(I2S_PORT, &i2s_config, 0, NULL);
  i2s_set_pin(I2S_PORT, &pin_config);
  
  Serial.println("Microphone ready! Open Serial Plotter to see audio waveform.");
}

void loop() {
  int32_t sample = 0;
  size_t bytesRead = 0;

  // Read raw audio data from the I2S buffer
  i2s_read(I2S_PORT, &sample, sizeof(sample), &bytesRead, portMAX_DELAY);

  if (bytesRead >  0) {
    // Shift the 32-bit sample down to viewable range for the Serial Plotter
    int scaledSample = sample >> 14; 

    // Print to Serial Monitor / Plotter
    Serial.println(scaledSample);
  }
}