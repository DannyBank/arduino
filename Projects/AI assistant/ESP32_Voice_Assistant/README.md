
# ESP32 Gemini Voice Assistant

A cloud-connected voice assistant built around an **ESP32-WROOM-32**. The ESP32 captures audio from an INMP441 I2S microphone, sends it over Wi-Fi to a Python/FastAPI backend, and receives spoken responses generated using Google's Gemini API. A MAX98357A I2S amplifier drives the speaker, while a 128×64 OLED displays device status.

The current baseline uses **push-to-talk**. Local wake-word detection is a planned enhancement.

---

## Contents

1. Project overview
2. How it works
3. Hardware requirements
4. Wiring and pin assignments
5. Power and electrical safety
6. Software architecture
7. Suggested project structure
8. Backend installation
9. Gemini configuration
10. ESP32 firmware setup
11. Running the complete system
12. API reference
13. Audio format reference
14. Troubleshooting
15. Security and privacy
16. Project status and roadmap

---

## 1. Project overview

### Objective

Build a physical voice assistant that can:

- Capture spoken commands using an I2S microphone.
- Send audio to a cloud AI service for understanding.
- Generate a conversational response.
- Convert the response into speech.
- Play the speech through a speaker.
- Display basic status and response information on an OLED.

### Main technologies

| Area | Technology |
|---|---|
| Microcontroller | ESP32-WROOM-32 |
| Firmware framework | Arduino framework |
| Microphone | INMP441 I2S |
| Audio amplifier | MAX98357A I2S |
| Display | 128×64 I2C OLED |
| Backend | Python and FastAPI |
| AI | Google Gemini API |
| Backend server | Uvicorn |
| Device communication | Wi-Fi and HTTP |
| Embedded audio | ESP32 I2S peripherals |

---

## 2. How it works

The project has three main components.

### ESP32 device

The ESP32:

1. Connects to Wi-Fi.
2. Captures audio from the INMP441 microphone.
3. Records a fixed-duration audio sample when push-to-talk is triggered.
4. Sends the recording to the backend.
5. Receives generated speech audio.
6. Plays the audio through the MAX98357A amplifier and speaker.
7. Uses the OLED and LED to indicate device state.

### Python backend

The backend:

1. Receives raw PCM audio from the ESP32.
2. Wraps the PCM data in a WAV container.
3. Sends the audio to Gemini for speech understanding and response generation.
4. Sends the generated text to Gemini text-to-speech.
5. Returns raw PCM speech audio and the response text to the ESP32.

### Gemini API

Gemini performs the cloud AI tasks:

- Understands the user's spoken request.
- Generates a text response.
- Generates speech audio from that response.

### End-to-end flow

```text
User speaks
    |
    v
INMP441 microphone
    |
    v
ESP32-WROOM-32
    |
    | Records 16 kHz mono PCM
    |
    | HTTP POST over Wi-Fi
    v
Python FastAPI backend
    |
    v
PCM-to-WAV conversion
    |
    v
Gemini audio understanding
    |
    v
AI text response
    |
    v
Gemini text-to-speech
    |
    v
24 kHz mono audio
    |
    | HTTP response
    v
ESP32 I2S audio output
    |
    v
MAX98357A amplifier
    |
    v
Speaker
```

---

## 3. Hardware requirements

### Main components

| Component | Purpose |
|---|---|
| ESP32-WROOM-32 development board | Main controller and Wi-Fi |
| INMP441 I2S microphone | Captures voice |
| MAX98357A I2S amplifier | Drives the speaker using digital I2S audio |
| Compatible 4 Ω or 8 Ω speaker | Plays assistant responses |
| 128×64 I2C OLED | Displays status and text |
| LED | Status indicator |
| 220–330 Ω resistor | Limits LED current |
| Breadboard and jumper wires | Prototyping |
| Regulated power supply | Powers the ESP32 and amplifier |
| 3.7 V, 3000 mAh Li-ion battery | Optional portable power source |

### Recommended additional components

- 100 µF electrolytic capacitor for amplifier supply decoupling.
- 100 nF ceramic capacitor for high-frequency decoupling.
- 10 kΩ pull-down resistor for the MAX98357A SD/SD_MODE pin.
- Suitable battery protection and charging circuitry.
- Regulated converters appropriate for the ESP32 and amplifier.

Check the exact specifications and pin labels of your breakout boards. Different breakout boards may have different power requirements.

---

## 4. Wiring and pin assignments

The following is the project's current reference pin configuration.

### OLED display

| OLED pin | ESP32 pin |
|---|---|
| VCC | 3.3 V (if supported by the display board) |
| GND | GND |
| SDA | GPIO 21 |
| SCL | GPIO 22 |

### INMP441 microphone

| INMP441 pin | ESP32 pin |
|---|---|
| VDD | 3.3 V |
| GND | GND |
| SCK / BCLK | GPIO 26 |
| WS / LRCL | GPIO 25 |
| SD | GPIO 34 |
| L/R | GND |

The L/R pin selects the microphone's left/right channel. This wiring assumes the firmware reads the left channel.

### MAX98357A amplifier

| MAX98357A pin | ESP32 / supply |
|---|---|
| VIN | Regulated supply appropriate for the breakout (typically 5 V) |
| GND | Common GND |
| BCLK | GPIO 27 |
| LRC / LRCLK | GPIO 14 |
| DIN | GPIO 13 |
| SD / SD_MODE | GPIO 32 |

Connect a 10 kΩ pull-down resistor between SD/SD_MODE and GND so the amplifier remains disabled while the ESP32 boots. Confirm the shutdown logic against your specific amplifier board.

### Speaker

Connect the speaker between:

- `SPK+`
- `SPK−`

**Do not connect either speaker output to GND.** The MAX98357A uses a bridge-tied output.

### LED

| LED connection | ESP32 |
|---|---|
| GPIO side, through 220–330 Ω resistor | GPIO 4 |
| Other LED leg | GND |

### I2S peripheral assignment

The classic ESP32 has two I2S peripherals, which this project assigns as follows:

| Peripheral | Direction | Device |
|---|---|---|
| I2S0 | RX | INMP441 microphone |
| I2S1 | TX | MAX98357A amplifier |

The MAX98357A does not need MCLK. In both legacy I2S pin configurations, use:

```cpp
.mck_io_num = I2S_PIN_NO_CHANGE
```

This prevents an unintended MCLK GPIO assignment.

---

## 5. Power and electrical safety

Power design is important, especially when using a Li-ion battery and speaker amplifier.

- A single-cell Li-ion battery is nominally 3.7 V but can reach approximately **4.2 V when fully charged**.
- Do not connect a Li-ion cell directly to the ESP32 3.3 V pin.
- A TP4056 is a charger; it is not, by itself, a regulated 3.3 V supply or a complete power-path solution.
- Use appropriate battery protection and regulated converters.
- Check that the supply can handle current peaks from Wi-Fi transmission and speaker playback.
- Connect ESP32, microphone, and amplifier grounds together.
- Keep microphone wiring away from speaker and amplifier power wiring where possible.
- Place the 100 µF and 100 nF capacitors close to the amplifier's supply pins. Observe electrolytic capacitor polarity.
- Disconnect power before changing wiring.

If the amplifier buzzes while SD/SD_MODE is held LOW and its I2S inputs are disconnected, investigate the supply, grounding, pin identification, wiring, and module condition before changing firmware.

---

## 6. Software architecture

### ESP32 firmware

The firmware uses the Arduino framework and typically includes:

- `WiFi.h` for wireless networking.
- `HTTPClient.h` for backend communication.
- `driver/i2s.h` for microphone capture and speaker playback.
- `Adafruit_GFX` and `Adafruit_SSD1306` for the OLED.
- GPIO functions for the LED and amplifier enable/shutdown control.

The current firmware uses the **legacy ESP32 I2S driver API**. Arduino-ESP32 core **2.0.17** is the baseline version for this implementation. Arduino-ESP32 3.x uses newer APIs and may require firmware migration.

### Backend

The Python backend uses:

- FastAPI for HTTP endpoints.
- Uvicorn to run the API server.
- `google-genai` for Gemini API access.
- `python-dotenv` for environment variables.
- Python's built-in `wave` module to create WAV input from PCM.

### Primary and fallback model behavior

The backend is designed to:

1. Try the configured primary Gemini chat model.
2. Retry transient HTTP errors using the SDK retry policy.
3. If the primary model still fails with HTTP 503, try the configured fallback chat model.
4. Generate speech using the separately configured TTS model.

Chat and TTS are separate API operations. A successful chat response does not guarantee TTS will succeed.

Model identifiers, API features, quotas, and pricing can change. Confirm that the model IDs in your environment are currently available to your Google AI Studio project.

---

## 7. Suggested project structure

A suggested repository layout is:

```text
esp32-gemini-voice-assistant/
|
|-- firmware/
|   `-- assistant/
|       `-- assistant.ino
|
|-- backend/
|   |-- main.py
|   |-- requirements.txt
|   |-- .env
|   `-- .env.example
|
|-- .gitignore
`-- README.md
```

The exact folder names can differ. Ensure the commands you run are executed from the correct directory.

### Suggested `.gitignore`

```gitignore
# Python
__pycache__/
*.py[cod]
.venv/
venv/
.env

# Build output
build/
*.o
*.a

# Editors and operating systems
.vscode/
.idea/
.DS_Store
Thumbs.db
```

Do not commit real API keys or Wi-Fi passwords.

---

## 8. Backend installation

### Prerequisites

- Python 3.10 or later recommended.
- Internet access from the computer running the backend.
- A Gemini API key.
- ESP32 and backend computer on the same reachable network, unless the backend is hosted elsewhere.

### Create a virtual environment

Windows PowerShell:

```powershell
cd backend
py -m venv .venv
.venv\Scripts\Activate.ps1
```

Windows Command Prompt:

```cmd
cd backend
py -m venv .venv
.venv\Scripts\activate.bat
```

Linux or macOS:

```bash
cd backend
python3 -m venv .venv
source .venv/bin/activate
```

### Install dependencies

Create `requirements.txt`:

```text
fastapi
uvicorn[standard]
google-genai
python-dotenv
```

Install:

```bash
python -m pip install --upgrade pip
pip install -r requirements.txt
```

### Create the environment file

Create `backend/.env`:

```dotenv
GEMINI_API_KEY=YOUR_GEMINI_API_KEY

GEMINI_CHAT_MODEL=YOUR_AVAILABLE_PRIMARY_CHAT_MODEL
GEMINI_FALLBACK_MODEL=YOUR_AVAILABLE_FALLBACK_CHAT_MODEL

GEMINI_TTS_MODEL=YOUR_AVAILABLE_TTS_MODEL
GEMINI_VOICE=Kore
```

Replace the placeholders with model IDs available to your API project. If you already have working model IDs in your local configuration, retain those values.

Create an API key through [Google AI Studio](https://aistudio.google.com/api-keys).

### Start the backend

From the `backend` directory:

```bash
uvicorn main:app --host 0.0.0.0 --port 8000 --reload
```

If your `main.py` includes a direct-run block, you can alternatively use:

```bash
python main.py
```

Check these URLs on the backend computer:

- `http://127.0.0.1:8000/` — service information.
- `http://127.0.0.1:8000/health` — health check.
- `http://127.0.0.1:8000/docs` — interactive API documentation.

---

## 9. Gemini configuration

### API key

The Gemini API key must remain on the Python backend. The ESP32 should communicate only with your backend and must never contain the Gemini API key.

### Retry behavior

The Google Gen AI SDK retry configuration can retry transient errors such as:

| HTTP status | General meaning |
|---|---|
| 408 | Request timeout |
| 429 | Rate limit or quota pressure |
| 500 | Internal server error |
| 502 | Bad gateway |
| 503 | Service unavailable |
| 504 | Gateway timeout |

Retries do not guarantee success and can increase response time. They do not bypass account quotas, billing requirements, or model access restrictions.

A **503** usually indicates temporary service unavailability or overload. A **429** usually indicates rate-limit or quota pressure. Investigate them separately.

### TTS configuration

The TTS model and voice are configured separately from the chat model. Ensure the selected TTS model supports audio output and that the configured voice is supported by that model.

---

## 10. ESP32 firmware setup

### Arduino IDE

1. Install Arduino IDE.
2. Install Espressif ESP32 board support.
3. Select a board compatible with ESP32-WROOM-32, commonly **ESP32 Dev Module**.
4. Select the correct serial port.
5. Install the display libraries used by the sketch.

Typical external libraries:

- Adafruit GFX Library.
- Adafruit SSD1306.

Wi-Fi, HTTPClient, and I2S headers are provided by the ESP32 Arduino core.

### Core version

The firmware described here uses:

```cpp
#include <driver/i2s.h>
```

and the legacy I2S API. Use Arduino-ESP32 **2.0.17** as the known baseline unless you have migrated the firmware to the newer I2S API.

### Configure Wi-Fi and backend URL

Set the Wi-Fi credentials and server URL in the firmware:

```cpp
const char* WIFI_SSID = "YOUR_WIFI_NAME";
const char* WIFI_PASSWORD = "YOUR_WIFI_PASSWORD";

const char* SERVER_URL =
    "http://YOUR_COMPUTER_LAN_IP:8000/api/voice";
```

Find the computer's LAN IP address on Windows:

```cmd
ipconfig
```

Use the IPv4 address of the active network adapter, for example `192.168.8.100`.

**Do not use `localhost` or `127.0.0.1` in the ESP32 firmware.** Those addresses refer to the device making the request, not your computer.

Ensure the computer firewall allows inbound TCP connections on port 8000 from your local network.

### HTTP timeout

The backend may need time to process audio, retry a model request, and generate speech. The firmware may need a longer timeout, for example:

```cpp
http.setTimeout(180000);
```

This is a diagnostic starting point, not a substitute for fixing a stalled backend or repeated API failure.

### Upload the firmware

1. Connect the ESP32 to your computer by USB.
2. Select the correct board and serial port.
3. Enter Wi-Fi credentials and backend URL.
4. Compile and upload the sketch.
5. Open Serial Monitor using the baud rate configured in the firmware.
6. Wait for the Wi-Fi connection.
7. Use the firmware's push-to-talk command. In the current version, `r` starts a recording and sends it to the backend; `h` displays help.

---

## 11. Running the complete system

Follow this order for an end-to-end test:

1. Connect the microphone, OLED, amplifier, and speaker.
2. Start the FastAPI backend on your computer.
3. Confirm `http://127.0.0.1:8000/health` responds successfully.
4. Confirm the ESP32 firmware uses your computer's LAN IP.
5. Power on or reset the ESP32.
6. Wait for the ESP32 to connect to Wi-Fi.
7. Open Serial Monitor.
8. Send `r` to record and submit a voice sample.
9. Watch the FastAPI terminal for audio receipt, Gemini processing, and TTS output.
10. Listen for the response through the speaker.

The first request can take longer than later requests because of network conditions, model availability, and service startup behavior.

---

## 12. API reference

### `GET /`

Returns basic service information and configured model identifiers.

### `GET /health`

Returns a basic backend health response. This checks that the FastAPI process is responding; it does not prove Gemini is reachable.

### `POST /api/voice`

Receives raw PCM audio in the HTTP request body.

**Input assumptions**

- 16,000 samples per second.
- Mono channel.
- 16-bit signed PCM.
- Raw PCM bytes, without a WAV header.

**Successful response**

- HTTP status: `200`.
- Body: raw 16-bit mono PCM speech audio.
- `X-Reply-Text`: URL-encoded text response.
- `X-Audio-Sample-Rate`: output sample rate.
- `X-Audio-Channels`: output channel count.
- `X-Audio-Bits`: output sample depth.

**Error response**

The backend returns a JSON error response with an HTTP status such as `400`, `500`, or `503`. The ESP32 firmware should check the HTTP status before treating the response body as audio.

| Method | Endpoint | Purpose |
|---|---|---|
| GET | `/` | Service information |
| GET | `/health` | Basic health check |
| GET | `/docs` | Interactive API documentation |
| POST | `/api/voice` | Process voice request |

---

## 13. Audio format reference

| Stage | Audio format |
|---|---|
| Microphone capture | 16 kHz, mono, 16-bit PCM |
| ESP32 upload | Raw PCM bytes |
| Backend input to Gemini | WAV container wrapping 16 kHz mono PCM |
| TTS target output | 24 kHz, mono, 16-bit PCM |
| Backend response | Raw PCM bytes |
| Speaker playback | I2S through MAX98357A |

### Expected recording size

For a 3-second recording at 16 kHz, mono, 16-bit:

```text
16,000 samples/second
× 3 seconds
× 1 channel
× 2 bytes/sample
= 96,000 bytes
```

The backend should therefore receive approximately:

```text
Received 96000 bytes of audio from ESP32.
```

This confirms the expected payload size, but does not prove the microphone captured intelligible audio.

**Important:** The backend and firmware must agree on the audio encoding. If the TTS service returns a WAV file but the ESP32 expects raw PCM, the WAV header must be removed before playback. If the TTS sample rate or channel count differs from the firmware's I2S configuration, convert the audio or update the playback configuration.

---

## 14. Troubleshooting

### ESP32 reports HTTP status `-11`

This project has encountered `-11` as an ESP32 HTTPClient read-timeout error.

Check:

1. Is FastAPI still running?
2. Does the backend terminal show that the request arrived?
3. Is Gemini taking a long time or retrying?
4. Is the ESP32 timeout long enough?
5. Is the Wi-Fi connection stable?

Try a longer timeout for diagnosis:

```cpp
http.setTimeout(180000);
```

### Gemini returns HTTP 503

The model or service may be temporarily unavailable or overloaded.

- Allow the SDK retry policy to run.
- Confirm the fallback model is available to your project.
- Check Google's service status and model availability.
- Retry later if the service remains overloaded.

A 503 is not the same as a 429 quota error.

### Gemini returns HTTP 429

The request may have exceeded a rate limit or quota, or the project may require a different billing or usage configuration.

Check the API project's quota, billing status, and model-specific limits. Automatic retries cannot solve a hard quota limit.

### Backend receives audio but Gemini does not understand it

Check:

- INMP441 VDD is connected to 3.3 V.
- INMP441 L/R is connected to GND for the selected channel.
- SCK and WS match the firmware pin configuration.
- The firmware reads the correct I2S channel and sample format.
- The microphone is close enough to the speaker/user and away from excessive noise.
- The captured audio is not silent, clipped, or dominated by electrical noise.

### MAX98357A produces buzzing or static

Check:

1. Correct VIN and GND pins.
2. Stable amplifier supply with adequate current capability.
3. Common ground between ESP32 and amplifier.
4. Speaker connected only between SPK+ and SPK−.
5. Correct BCLK, LRC, and DIN wiring.
6. MCLK disabled using `I2S_PIN_NO_CHANGE`.
7. SD/SD_MODE pull-down and firmware control.
8. Local supply decoupling capacitors.

If the amplifier buzzes while disabled with its I2S inputs disconnected, investigate the supply, wiring, pin identification, and module condition before changing audio code.

### Chat works but TTS fails

Chat and TTS are separate API calls. Check:

- The configured TTS model is available to your project.
- The model supports audio output.
- The selected voice is supported.
- The backend extracts the audio bytes correctly.
- The returned encoding matches the ESP32 playback configuration.

### ESP32 cannot reach FastAPI

- Confirm both devices are on a network that allows them to communicate.
- Use the computer's LAN IPv4 address in the firmware.
- Confirm Uvicorn is bound to `0.0.0.0`, not only `127.0.0.1`.
- Allow inbound TCP port 8000 through the computer firewall.
- Test `http://COMPUTER_LAN_IP:8000/health` from another device on the same network.

---

## 15. Security and privacy

- Keep the Gemini API key on the backend only.
- Do not commit `.env` to source control.
- Do not expose the development FastAPI server directly to the public internet without authentication, TLS, rate limiting, and appropriate request controls.
- Use HTTPS when deploying beyond a trusted local network.
- Add request-size limits and per-device authentication before supporting multiple devices.
- Voice recordings are sent to the configured Gemini service for processing. Review Google's applicable API data-use and retention terms before using this project with sensitive conversations.
- Avoid logging raw audio, API keys, or sensitive speech content.
- Rotate any API key that is accidentally exposed.

---

## 16. Project status and roadmap

### Current baseline

- [x] ESP32 Wi-Fi connectivity.
- [x] INMP441 microphone capture.
- [x] Fixed-duration push-to-talk recording.
- [x] OLED status display.
- [x] FastAPI backend.
- [x] Gemini chat integration.
- [x] Gemini TTS integration in backend.
- [x] ESP32 speaker playback.
- [x] Primary-model retry configuration.
- [x] Fallback chat model configuration for primary-model 503 errors.

The exact status of an end-to-end feature should be confirmed with a successful hardware test using the current API model IDs and account.

### Phase 1 — Reliable push-to-talk

- [ ] Complete repeatable end-to-end tests.
- [ ] Improve backend and firmware error reporting.
- [ ] Add audio capture quality checks.
- [ ] Improve recovery after Wi-Fi or API failure.

### Phase 2 — Hands-free operation

- [ ] Select or train a small wake-word model.
- [ ] Integrate local wake-word inference using TensorFlow Lite Micro or another suitable embedded inference framework.
- [ ] Add a microphone ring buffer.
- [ ] Coordinate microphone reads so the wake-word detector and recorder do not compete for the same I2S input.
- [ ] Add voice activity detection and end-of-speech detection.
- [ ] Implement listening, processing, speaking, and idle states.

### Phase 3 — Better user experience

- [ ] Display recognized speech and assistant responses on the OLED.
- [ ] Add volume controls.
- [ ] Add a mute control.
- [ ] Add configurable recording duration.
- [ ] Add cancellation and timeout behavior.
- [ ] Add Wi-Fi configuration without recompiling firmware.

### Phase 4 — More capable assistant

- [ ] Add short conversation history.
- [ ] Add configurable assistant instructions and personality.
- [ ] Add optional alternative cloud AI providers.
- [ ] Add local command handling for simple actions.
- [ ] Explore offline support for selected commands.

### Wake-word implementation note

TensorFlow Lite Micro performs inference; it does not train the wake-word model on the ESP32. Train or obtain a suitable model separately (for example, using Edge Impulse), then deploy the exported model to the ESP32. Design the microphone data path so a single controlled audio capture task can feed both wake-word detection and subsequent recording.

---

## Contributing

When modifying the project:

1. Keep ESP32 and backend audio formats synchronized.
2. Keep API keys and Wi-Fi passwords out of source control.
3. Test the backend independently before debugging the ESP32.
4. Test microphone capture and speaker playback independently before testing the complete voice loop.
5. Record the ESP32 board package version, Python version, dependency versions, and Gemini model IDs used for a known-working build.
6. Update this README when pin assignments, API formats, or setup steps change.

## License

Choose and add a license before distributing or publishing this project. Ensure the licenses of libraries, model assets, and other dependencies are compatible with the license you select.
