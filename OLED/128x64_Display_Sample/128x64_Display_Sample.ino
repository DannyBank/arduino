# -----------------------------------------------------------------------------
# IoTConnect Device Application
#
# Project   : WeatherFromWokwi
# Generated : Automatically by IoTConnect
# Platform  : MicroPython
# Copyright : 2026 IoTDevLab
# License   : MIT (SPDX-License-Identifier: MIT)
#
# This file was generated automatically.
# Changes made directly to this file may be overwritten when regenerated.
#
# Required drivers:
#   - DHTx, Servo motor
#
# Device configuration:
#   Sensors   : DHT22
#   Actuators : Servo motor
#   Network   : Configured
# -----------------------------------------------------------------------------
# MQTT protocol: 3.1.1 compatibility (umqtt.simple does not implement MQTT 5).
# Install any hardware driver modules named below before uploading this file.
# DHT22 (DHTx): {"wires":{},"position":{"x":128,"y":80},"viewport":{"x":-132,"y":39.91666793823236,"zoom":1},"interface":"GPIO","wireModes":{"powerPin":"auto","groundPin":"auto"},"wireColors":{},"portAnchors":{"powerPin":"right:0","groundPin":"right:1"},"terminalSide":"RIGHT","terminalSideManual":false,"powerPin":"3V3","groundPin":"GND","dataPin":"15"}
# Servo motor (Servo motor): {"wires":{},"viewport":{"x":128.6092740016186,"y":139.0548675401353,"zoom":0.4515},"frequency":"50","interface":"PWM","wireModes":{"powerPin":"auto","groundPin":"auto"},"portAnchors":{"powerPin":"right:0","groundPin":"right:1"},"powerPin":"3V3","groundPin":"GND","controlPin":"13"}
import json, time, network, ssl
from umqtt.simple import MQTTClient
from machine import Pin, PWM, I2C
import ssd1306
import dht

# USER CONFIGURATION: set these values before uploading to the board.
DHTX_1_POWERPIN = "3V3"
DHTX_1_GROUNDPIN = "GND"
DHTX_1_DATAPIN = 15
SERVO_MOTOR_1_POWERPIN = "3V3"
SERVO_MOTOR_1_GROUNDPIN = "GND"
SERVO_MOTOR_1_CONTROLPIN = 13
WIFI_SSID = "Wokwi-GUEST"
WIFI_PASSWORD = ""
MQTT_HOST = "mqtt.iotworkshop.africa"
MQTT_PORT = 8883
MQTT_USER = "device_dev_2dz39mFPg9tUeU76".encode()
MQTT_PASSWORD = "nlAI16okIt1gHCwlyfGaClcGBe0gSe3dJQIRpYuiYFw".encode()
MQTT_CA_FILE = "mqtt-ca.crt"
TELEMETRY_TOPIC = "dev_2dz39mFPg9tUeU76/data".encode()
COMMAND_TOPIC = "dev_2dz39mFPg9tUeU76/cmd".encode()
RESULT_TOPIC = "dev_2dz39mFPg9tUeU76/result".encode()

# HARDWARE SETUP
# I2C setup for OLED Display (SCL=Pin 22, SDA=Pin 21)
i2c = I2C(0, scl=Pin(22), sda=Pin(21))
oled = ssd1306.SSD1306_I2C(128, 64, i2c)

# DHT22 Sensor setup on GPIO 15
dht_sensor = dht.DHT22(Pin(DHTX_1_DATAPIN))

# Servo Motor setup
ACTUATOR_STATE = {}
servo_1 = PWM(Pin(SERVO_MOTOR_1_CONTROLPIN), freq=50)
servo_1.duty_u16(4915)  # Initial position (90 degrees approx)
ACTUATOR_STATE["servo_motor_1"] = {"angle": 90}

# Cached sensor values for OLED display
latest_temp = 0.0
latest_hum = 0.0

def read_components():
    global latest_temp, latest_hum
    try:
        dht_sensor.measure()
        latest_temp = dht_sensor.temperature()
        latest_hum = dht_sensor.humidity()
    except Exception as error:
        print("[SENSOR] Failed to read DHT22 sensor:", error)
        
    return {
        "dhtx_1": {
            "humidity": latest_hum,
            "temperature": latest_temp
        },
        "servo_motor_1": dict(ACTUATOR_STATE["servo_motor_1"])
    }

def apply_command(command):
    component = command.get("component")
    action = command.get("action")
    value = (command.get("parameters") or {}).get("value")
    if component == "servo_motor_1":
        if action != "angle": raise ValueError("Unsupported servo action")
        angle = max(0, min(180, float(value)))
        servo_1.duty_u16(int(1638 + (angle / 180) * 6554))
        ACTUATOR_STATE[component]["angle"] = angle
    else:
        raise ValueError("Unknown actuator component")
    return {"components": ACTUATOR_STATE}

def on_command(topic, message):
    print("[MQTT] Received on", topic, ":", message)
    command = json.loads(message)
    try:
        state = apply_command(command)
        result = {"commandId": command.get("commandId"), "status": "SUCCEEDED", "state": state}
        print("[DEVICE] Applied", command.get("component"), command.get("action"), (command.get("parameters") or {}).get("value"))
    except Exception as error:
        result = {"commandId": command.get("commandId"), "status": "FAILED", "error": str(error)}
        print("[DEVICE] Command failed:", error)
    client.publish(RESULT_TOPIC, json.dumps(result))
    print("[MQTT] Result published:", result)

wlan = network.WLAN(network.STA_IF)
wlan.active(True)
print("[WIFI] Connecting to", WIFI_SSID)
wlan.connect(WIFI_SSID, WIFI_PASSWORD)
while not wlan.isconnected():
    time.sleep_ms(250)
print("[WIFI] Connected:", wlan.ifconfig())
client = MQTTClient(
    client_id="esp32_dht_client",
    server=MQTT_HOST,
    port=MQTT_PORT,
    user=MQTT_USER,
    password=MQTT_PASSWORD,
    ssl=True, 
    ssl_params={
        "server_hostname": MQTT_HOST,
        "cert_reqs": ssl.CERT_NONE
    } # Set to False if your local broker doesn't use certificates
)
client.set_callback(on_command)
print("[MQTT] Connecting to {}:{}".format(MQTT_HOST, MQTT_PORT))
client.connect()
print("[MQTT] Connected")
client.subscribe(COMMAND_TOPIC)
print("[MQTT] Subscribed:", COMMAND_TOPIC)
last_telemetry = time.ticks_ms() - 10000
while True:
    client.check_msg()
    now = time.ticks_ms()
    if time.ticks_diff(now, last_telemetry) >= 10000:
        telemetry = {"components": read_components()}
        client.publish(TELEMETRY_TOPIC, json.dumps(telemetry))
        print("[MQTT] Telemetry published:", telemetry)
        last_telemetry = now
    time.sleep_ms(20)
