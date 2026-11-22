# XiaoZhi AI, ESP32 Voice Assistant, and Edge AI Notes

> Consolidated project notes from the available XiaoZhi/ESP32 discussion
> through 16 November 2026.
>
> **Scope note:** This README consolidates the conversation history
> available in this chat, including a summary of the earlier
> troubleshooting discussion and the subsequent messages. It is not a
> verbatim export of every earlier chat message; some earlier turns were
> available only as summarized context.

## Table of contents

1.  [Project overview](#project-overview)
2.  [Hardware and board context](#hardware-and-board-context)
3.  [Earlier setup and troubleshooting
    history](#earlier-setup-and-troubleshooting-history)
4.  [OLED display wiring](#oled-display-wiring)
5.  [Audio hardware wiring](#audio-hardware-wiring)
6.  [Getting XiaoZhi to speak
    English](#getting-xiaozhi-to-speak-english)
7.  [How XiaoZhi works](#how-xiaozhi-works)
8.  [XiaoZhi versus a custom OpenAI
    integration](#xiaozhi-versus-a-custom-openai-integration)
9.  [Quantization and Edge AI](#quantization-and-edge-ai)
10. [Quantizing a model with ESP-PPQ](#quantizing-a-model-with-esp-ppq)
11. [Recommended Edge AI project
    path](#recommended-edge-ai-project-path)
12. [Useful links](#useful-links)
13. [Outstanding checks and next
    steps](#outstanding-checks-and-next-steps)

------------------------------------------------------------------------

## Project overview

The project is to get a XiaoZhi AI voice assistant working on a
**classic ESP32 development board without PSRAM**, then understand how
its cloud voice-assistant workflow compares with a custom AI application
that calls an API such as OpenAI.

A second goal is to learn how to train and quantize smaller
machine-learning models so they can run locally on an ESP32.

The key architectural distinction is:

-   **XiaoZhi:** the ESP32 is primarily an audio/device client. It
    captures and plays audio and communicates with a server that
    performs the main speech and language processing.
-   **TinyML / Edge AI:** a small trained model can run directly on the
    ESP32 for tasks such as sensor classification, anomaly detection, or
    simple keyword recognition.
-   **Hybrid AI:** the ESP32 performs local sensing and time-critical
    actions, while a cloud service handles more demanding conversation
    and reasoning.

## Hardware and board context

### Known hardware

  Component             Status / model
  --------------------- ----------------------------------------------
  Main board            Classic ESP32, no PSRAM
  Wi-Fi                 Connected successfully
  OLED                  Now displaying after wiring changes
  Microphone            INMP441 I2S MEMS microphone
  Amplifier             MAX98357A I2S amplifier
  Push-to-talk button   Not connected at the time of troubleshooting
  Speaker               Intended to connect to MAX98357A output

The exact ESP32 development-board variant and exact firmware filename
were not established in the available discussion. Those details matter
when selecting or rebuilding firmware.

### Capability note

A classic ESP32 without PSRAM can run small embedded machine-learning
workloads, but it is not suitable for running a ChatGPT-like large
language model locally. XiaoZhi can still provide conversational AI
because the demanding speech and LLM processing is performed remotely.

------------------------------------------------------------------------

## Earlier setup and troubleshooting history

The earlier troubleshooting discussion established the following:

1.  XiaoZhi firmware was flashed successfully.
2.  The device connected to Wi-Fi.
3.  Initially, no microphone or speaker was connected.
4.  The OLED did not initially show anything.
5.  The user noted that GPIO15 was not exposed or available on their
    board.
6.  The OLED was subsequently connected using GPIO21 and GPIO22 and
    began displaying.
7.  The INMP441 microphone and MAX98357A amplifier were identified as
    the audio components.
8.  The push-to-talk button was not connected.
9.  The reported symptom during one troubleshooting stage was that
    speaking caused no visible change on the screen.
10. The user later reported that everything seemed to be working and
    asked how to make XiaoZhi speak English.

### Important firmware/pin warning

The standard classic ESP32 XiaoZhi breadboard configuration referenced
in the troubleshooting discussion used:

-   OLED SDA: GPIO4
-   OLED SCL: GPIO15

The user's board did not expose GPIO15, so GPIO21 and GPIO22 were
considered as alternative I2C pins.

**Changing the OLED wires alone is not always sufficient.** The
firmware's OLED/I2C configuration must use the same pins as the physical
wiring. A firmware build configured for GPIO4/GPIO15 may not work with
an OLED physically connected to GPIO21/GPIO22 unless the firmware is
configured or rebuilt accordingly.

The display driver and resolution must also match the OLED module.
Common possibilities include:

-   SSD1306, 128x64
-   SSD1306, 128x32
-   SH1106, 128x64

Do not assume the display controller from its physical appearance alone.
Check the module documentation or markings where possible.

------------------------------------------------------------------------

## OLED display wiring

For the user's chosen GPIO21/GPIO22 arrangement:

  OLED module pin   ESP32 connection
  ----------------- ------------------
  VCC               3V3
  GND               GND
  SDA               GPIO21
  SCL               GPIO22

GPIO21 and GPIO22 are commonly used as I2C SDA and SCL pins on classic
ESP32 development boards.

### OLED checks

If the display stops working:

1.  Check that VCC is connected to 3V3 and GND to GND.
2.  Check that SDA and SCL have not been swapped.
3.  Confirm the firmware is configured for SDA GPIO21 and SCL GPIO22.
4.  Confirm the OLED controller and resolution match the firmware build.
5.  Check the serial log for I2C or display-initialisation errors.

An I2C scanner can help identify the OLED's I2C address, commonly 0x3C
or 0x3D. However, uploading a scanner sketch replaces the XiaoZhi
firmware temporarily, so the XiaoZhi firmware must be flashed again
afterward.

------------------------------------------------------------------------

## Audio hardware wiring

The following pin assignments were discussed for the classic ESP32
XiaoZhi budget configuration. Confirm that they match the exact firmware
build before powering the circuit.

### INMP441 microphone

  INMP441 pin   ESP32 connection
  ------------- ------------------
  VDD           3V3
  GND           GND
  L/R           GND
  WS            GPIO25
  SCK           GPIO26
  SD            GPIO32

The INMP441 is an I2S digital microphone.

### MAX98357A amplifier

  MAX98357A pin   ESP32 connection
  --------------- ---------------------------------
  VIN             3V3, if supported by the module
  GND             GND
  DIN             GPIO33
  BCLK            GPIO14
  LRC             GPIO27

Connect the passive speaker to the amplifier's **SPK+** and **SPK-**
terminals.

**Do not connect SPK- to ESP32 GND.** The speaker outputs are
bridge-tied amplifier outputs; neither speaker terminal should be
treated as ground.

Check the MAX98357A breakout's input-voltage specifications and the
speaker's impedance/power rating before use.

### Push-to-talk

The standard configuration discussed used GPIO5 for an external
push-to-talk control. GPIO0 is also commonly the ESP32 BOOT button, but
whether it can be used for XiaoZhi interaction depends on the firmware's
configured button mapping.

At the point when the user reported the microphone and amplifier models,
no push-to-talk button was connected. The expected interaction mode and
button mapping should be checked against the exact firmware version. Do
not assume wake-word detection is enabled on every build.

### Suggested audio troubleshooting order

1.  Confirm common ground between ESP32, microphone and amplifier.
2.  Confirm the INMP441 wiring, especially WS, SCK and SD.
3.  Confirm the microphone pin configuration in the firmware.
4.  Add/configure the push-to-talk button if required by the build.
5.  Test whether the OLED changes to a listening state while the button
    is held.
6.  Only after microphone capture works, test the MAX98357A and speaker.
7.  If the screen never changes state, inspect the serial logs and
    confirm the device is connected to the expected XiaoZhi server.

------------------------------------------------------------------------

## Getting XiaoZhi to speak English

Two different language settings should be distinguished:

1.  **Interface language:** the language used for menus and display
    text.
2.  **Conversation language:** the language the AI understands and uses
    when responding.

Changing the display language alone does not necessarily change the AI's
spoken language.

The suggested configuration workflow was:

1.  Open the XiaoZhi website: <https://xiaozhi.me/>.

2.  Sign in.

3.  Open the console.

4.  Find the registered device under Devices.

5.  Open the device's configuration or role configuration.

6.  Look for language, model, voice, or role settings. Exact labels can
    vary with the service version.

7.  Select English where a conversation-language option is available.

8.  Select an English-capable voice.

9.  Optionally set the role/personality instruction to:

    > You are a helpful English-speaking AI assistant. Always understand
    > and respond in clear, natural English. Speak English even when the
    > user speaks another language. Keep your answers friendly, simple
    > and conversational.

10. Save the settings.

11. Restart the ESP32 using RESET/EN and test a new conversation.

The conversation did not establish the exact names of the settings in
the user's console, so the labels above should be treated as guidance
rather than guaranteed menu names.

------------------------------------------------------------------------

## How XiaoZhi works

XiaoZhi is an embedded voice-assistant framework connected to a server.
The ESP32 is not running the main conversational LLM.

A typical voice interaction follows this path:

1.  The user speaks.
2.  The INMP441 microphone captures audio.
3.  ESP32 firmware receives and processes the audio.
4.  The device sends audio data to the XiaoZhi service over Wi-Fi using
    the communication mechanisms supported by that firmware/server
    version.
5.  A server-side ASR (automatic speech recognition) service converts
    speech into text.
6.  The recognised text is sent to a large language model (LLM).
7.  The LLM generates a response.
8.  A TTS (text-to-speech) service converts the response into audio.
9.  The server sends the audio back to the ESP32.
10. The MAX98357A amplifier drives the speaker so the user can hear the
    response.
11. The OLED can display status, recognised text, or response
    information, depending on the firmware.

### XiaoZhi workflow diagram

``` text
USER SPEAKS
    |
    v
INMP441 MICROPHONE
    |
    v
ESP32 FIRMWARE
(audio capture, processing, network communication)
    |
    v
WI-FI / XIAOZHI SERVER
    |
    +--> ASR: speech to text
    |
    +--> LLM: understand and generate answer
    |
    +--> TTS: answer text to speech
    |
    v
AUDIO RESPONSE SENT TO ESP32
    |
    v
MAX98357A AMPLIFIER
    |
    v
SPEAKER
```

The ESP32 handles the device-side work, while the server handles the
computationally expensive language and speech tasks.

------------------------------------------------------------------------

## XiaoZhi versus a custom OpenAI integration

A custom application can use an ASP.NET Core or Python backend and call
an AI endpoint such as OpenAI.

### Approach A: XiaoZhi

-   Existing firmware handles much of the device audio and communication
    work.
-   The server-side service coordinates speech recognition, LLM
    processing and speech synthesis.
-   The user configures available models, voices and roles through the
    XiaoZhi ecosystem.
-   Hardware interaction can be integrated through supported device
    capabilities and MCP-style tools, depending on the firmware and
    server setup.
-   It is a convenient route to an existing voice-assistant ecosystem.

### Approach B: custom OpenAI integration

A custom system gives the developer more control, but more components
must be implemented.

``` text
ESP32 + MICROPHONE
        |
        v
YOUR ASP.NET CORE BACKEND
        |
        +--> Authentication and device management
        +--> Audio upload/streaming
        +--> Conversation history
        +--> Speech recognition service
        +--> OpenAI LLM API
        +--> Text-to-speech service
        +--> Hardware command validation
        |
        v
ESP32 + AMPLIFIER + SPEAKER
```

Potential responsibilities include:

-   Audio capture and encoding on the ESP32.
-   Secure transport to the backend.
-   Device authentication.
-   Speech-to-text selection.
-   LLM requests and conversation history.
-   Text-to-speech generation.
-   Audio streaming back to the device.
-   Tool/function execution and hardware control.
-   Logging, rate limiting and usage monitoring.

### Comparison

  -----------------------------------------------------------------------
  Feature                 XiaoZhi                 Custom OpenAI
                                                  integration
  ----------------------- ----------------------- -----------------------
  Audio capture           Existing firmware       Developer
                          support                 implements/integrates
                                                  it

  Speech recognition      Configured service      Developer selects
                                                  service

  LLM                     Configured              Developer selects
                          provider/model          model/API

  Voice generation        Configured service      Developer selects
                                                  service

  Hardware integration    Supported device        Custom commands and
                          features/MCP            tools

  Backend                 Existing ecosystem      Own backend

  Customisation           Depends on framework    High

  Security responsibility Shared with service     Primarily developer's
                          configuration           responsibility
  -----------------------------------------------------------------------

### Recommended custom architecture

For a developer comfortable with ASP.NET Core, a useful architecture is:

-   ESP32: microphone, speaker, sensors, relays and local safety logic.
-   ASP.NET Core AI Gateway: device authentication, audio routing,
    conversation state, tool execution and logging.
-   Speech recognition service: audio-to-text.
-   LLM provider: reasoning and response generation.
-   TTS provider: text-to-audio.

**Security:** Do not put an OpenAI API key in ESP32 firmware. Keep
provider credentials on the backend. Authenticate each device and
validate every command before it can operate a relay, lock, motor or
other physical device.

A provider's realtime voice API may reduce the need to orchestrate
separate ASR and TTS steps. The ESP32 may still need a gateway or
protocol adapter to handle authentication and device communication
safely.

------------------------------------------------------------------------

## Quantization and Edge AI

### What is quantization?

Quantization reduces the precision used to represent a model's numerical
weights and sometimes its activations.

A common starting point is converting FP32 (32-bit floating point)
values to INT8 (8-bit integer) values.

For a model with 10 million weights, approximate raw weight storage is:

  Numeric format     Bits per weight   Approximate weight storage
  ---------------- ----------------- ----------------------------
  FP32                            32                        40 MB
  FP16                            16                        20 MB
  INT8                             8                        10 MB
  INT4                             4                         5 MB

These are illustrative raw-weight calculations. Actual model files also
contain metadata, scales, biases and other structures. Not every runtime
or ESP32 target supports every format.

### Benefits

Quantization can reduce:

-   Model storage requirements.
-   Memory needed for model weights.
-   Memory bandwidth.
-   Some inference computation costs.

### Important limitation

A smaller model file does not automatically mean the model can run on an
ESP32.

Inference also requires memory for:

-   Activations and intermediate tensors.
-   Input and output buffers.
-   Runtime data structures.
-   Stack and application memory.
-   Other firmware features running at the same time.

A model may fit in flash but fail during inference because its working
RAM requirements are too high.

### What can run on the classic ESP32 without PSRAM?

  -----------------------------------------------------------------------
  Model/task category                 Practical assessment
  ----------------------------------- -----------------------------------
  Small sensor classification         Practical

  Simple anomaly detection            Practical

  Small gesture-recognition model     Depends on model and input

  Small keyword spotting              Constrained but possible with
                                      suitable models

  Image classification                Very constrained

  Small language model                Generally not practical

  ChatGPT-like LLM                    Not practical
  -----------------------------------------------------------------------

The classic ESP32 is suitable for small, carefully designed TinyML
workloads, not for a general-purpose conversational LLM.

### Espressif tools

-   ESP-DL documentation:
    <https://docs.espressif.com/projects/esp-dl/en/latest/>
-   ESP-PPQ repository: <https://github.com/espressif/esp-ppq>
-   ESP-DL quantization tutorial:
    <https://docs.espressif.com/projects/esp-dl/en/latest/tutorials/how_to_quantize_model.html>

ESP-DL is Espressif's deep-learning inference framework. ESP-PPQ is the
quantization tool used in the discussed deployment workflow.

------------------------------------------------------------------------

## Quantizing a model with ESP-PPQ

The suggested workflow is:

``` text
COLLECT / PREPARE DATA
        |
        v
TRAIN MODEL (e.g. PyTorch)
        |
        v
EXPORT MODEL TO ONNX
        |
        v
PREPARE REPRESENTATIVE CALIBRATION DATA
        |
        v
QUANTIZE WITH ESP-PPQ
        |
        v
EXPORT ESP-DL MODEL (.espdl)
        |
        v
TEST ACCURACY AND MEMORY REQUIREMENTS
        |
        v
DEPLOY TO ESP-IDF / ESP32
```

### Step 1: Install Python tools

Example CPU installation commands:

``` bash
pip install torch torchvision torchaudio --index-url https://download.pytorch.org/whl/cpu

pip install esp-ppq
```

Check the current ESP-PPQ installation instructions before using these
commands, since dependencies and supported versions can change.

### Step 2: Train a small model

A useful first model is a sensor classifier, such as a vibration
classifier with categories:

-   Normal vibration.
-   Excessive vibration.
-   Possible mechanical fault.

The training data should represent the conditions the device will
encounter in real use.

### Step 3: Export a PyTorch model to ONNX

Simplified example:

``` python
import torch

model.eval()

dummy_input = torch.randn(1, 3)

torch.onnx.export(
    model,
    dummy_input,
    "sensor_model.onnx",
    input_names=["input"],
    output_names=["output"],
    opset_version=13
)
```

This example assumes a model with an input shape of `[1, 3]`. Real input
dimensions must match the trained model. The installed ONNX exporter and
target toolchain must support the selected opset and operators.

### Step 4: Prepare calibration data

Post-training quantization generally needs representative calibration
examples. These should resemble actual device inputs, including the
normal range and important edge cases.

The calibration data loader is model-specific; it cannot be replaced
with an arbitrary placeholder.

### Step 5: Quantize with ESP-PPQ

The following is a simplified illustrative example. It assumes that
`calibration_loader` has already been created correctly for the ONNX
model.

``` python
from esp_ppq.api import espdl_quantize_onnx

quantized = espdl_quantize_onnx(
    onnx_import_file="sensor_model.onnx",
    espdl_export_file="sensor_model.espdl",
    calib_dataloader=calibration_loader,
    calib_steps=32,
    input_shape=[1, 3],
    target="c",
    num_of_bits=8,
    device="cpu"
)
```

For the classic ESP32, the earlier discussion identified `target="c"` as
the target setting to use in the ESP-PPQ workflow. Confirm the exact
arguments against the installed ESP-PPQ release and its current
documentation.

A W8A8-style configuration is a sensible starting point for many small
models:

-   W8: 8-bit weights.
-   A8: 8-bit activations.

Do not assume every operator or model architecture is supported by the
selected target.

### Step 6: Inspect, validate and deploy

The output `.espdl` model is intended for ESP-DL deployment. Inspect the
generated model information and diagnostics using the tools provided by
the installed version.

Before deploying:

1.  Compare the quantized model's accuracy with the original FP32 model.
2.  Test representative and edge-case inputs.
3.  Check model flash size.
4.  Check peak inference RAM usage.
5.  Confirm operator support.
6.  Test inference on the actual ESP32, not only on the computer.
7.  Test stability while the rest of the firmware is running.

### ESP-DL versus TensorFlow Lite Micro

These are different deployment ecosystems. An ESP-DL `.espdl` model is
not the same as a TensorFlow Lite Micro `.tflite` model. Choose the
runtime first, then prepare and export the model for that runtime.

------------------------------------------------------------------------

## Recommended Edge AI project path

Three levels were discussed.

### Level 1: TinyML on the existing ESP32

Good starting projects:

-   Sensor classification.
-   Vibration anomaly detection.
-   Electrical consumption anomaly detection.
-   Environmental condition classification.
-   Simple keyword recognition.

These can run locally and do not need an internet connection once
deployed.

### Level 2: Hybrid AI

-   ESP32 reads sensors and makes time-critical local decisions.
-   XiaoZhi or a custom backend handles conversational AI.
-   The AI interprets user requests and sends structured commands.
-   ESP32 validates commands and operates hardware.

This architecture is useful for smart door locks, gas monitoring, and
home automation.

For safety-related projects, local protective logic must not depend on
cloud availability or an LLM response. For example, a gas shutoff or
door-lock safety mechanism should enforce its own sensor thresholds,
interlocks, and fail-safe behaviour locally.

### Level 3: Upgrade for more demanding Edge AI

For more demanding neural networks, vision and audio processing,
consider a more capable board such as an ESP32-S3 with PSRAM or an
ESP32-P4-based system. Select the board based on the model's measured
memory and compute requirements.

------------------------------------------------------------------------

## Useful links

  ---------------------------------------------------------------------------------------------------------------------------------
  Resource                            Link
  ----------------------------------- ---------------------------------------------------------------------------------------------
  XiaoZhi website                     <https://xiaozhi.me/>

  ESP-DL documentation                <https://docs.espressif.com/projects/esp-dl/en/latest/>

  ESP-DL quantization tutorial        <https://docs.espressif.com/projects/esp-dl/en/latest/tutorials/how_to_quantize_model.html>

  ESP-PPQ GitHub                      <https://github.com/espressif/esp-ppq>

  OpenAI API documentation            <https://platform.openai.com/docs/>
  ---------------------------------------------------------------------------------------------------------------------------------

------------------------------------------------------------------------

## Outstanding checks and next steps

The following details should be confirmed before making further firmware
changes:

-   [ ] Exact ESP32 board variant (for example, 30-pin or 38-pin
    development board).
-   [ ] Exact XiaoZhi firmware filename and version.
-   [ ] Whether the firmware's OLED SDA/SCL pins are configured as
    GPIO21/GPIO22.
-   [ ] OLED controller and resolution (SSD1306 or SH1106; 128x32 or
    128x64).
-   [ ] Whether the INMP441 pin configuration matches the firmware.
-   [ ] Whether the MAX98357A pin configuration matches the firmware.
-   [ ] Which button or interaction mode the current firmware expects.
-   [ ] Whether the device's English voice and conversation settings
    have been saved and tested.
-   [ ] For TinyML, choose a first project and collect representative
    data before training.

### Suggested first TinyML project

**Vibration anomaly detection** is a good first end-to-end experiment
because it teaches the complete workflow:

1.  Collect sensor data.
2.  Label examples.
3.  Train a small model in Python.
4.  Evaluate the original model.
5.  Export to ONNX.
6.  Quantize with ESP-PPQ.
7.  Export an ESP-DL model.
8.  Measure memory and inference performance.
9.  Deploy to the ESP32.
10. Test real-time inference on the hardware.

This provides a practical foundation for more advanced embedded AI
projects.
