# ESP32-A  --  IR command receiver + device drivers (MicroPython)  [debug/logging build]
#
# Needs Peter Hinch's micropython_ir library: copy the ir_rx/ folder
# (__init__.py and nec.py) onto the board.
#
# IR frame (Extended NEC):  addr16 = (device << 8) | action,  cmd8 = value
#
# LED (GPIO 2):  short blip every 2 s = alive, waiting
#                solid ~200 ms        = an IR frame was received
#                (on boot: 3 blinks)
#
# Testing A on its own (no ESP32-B needed):
#   1. Set SELFTEST_ON_BOOT = True to exercise every device at start-up, or
#   2. Press Ctrl-C in the REPL, then call:
#        selftest()                  run through all devices
#        inject(0x02, 0, 90)         pretend a frame arrived: servo to 90 deg
#        inject(DEV_TRAFFIC, 0, 3)   green light
#        status()                    counters and last frame
#      then call main_loop() to go back to listening.
#   3. Point any ordinary NEC TV remote at the receiver: frames are decoded and
#      logged as "no handler", which proves the receiver wiring works.

import sys
import time
from machine import Pin, PWM
from ir_rx.nec import NEC_16

# ---------------- debug settings ----------------
DEBUG = True                  # False silences all log lines
SELFTEST_ON_BOOT = False      # True = run selftest() before listening
STATS_EVERY_MS = 10000        # periodic status line (0 = off)

# ---------------- pins ----------------
IR_PIN = 27                      # IR receiver signal
LED_PIN = 2                      # on-board LED
TRAFFIC_PINS = (25, 26, 33)      # red, yellow, green
SERVO_PIN = 13
BUZZER_PIN = 12                  # strapping pin: move it if boot problems
# (button 18, DHT11 4, TEMT6000 34, I2C 21/22 stay as before)

# ---------------- protocol IDs (match B's sketch) ----------------
DEV_TRAFFIC = 0x01
DEV_SERVO = 0x02
DEV_BUZZER = 0x03
DEV_SYSTEM = 0x05
ACT_SET = 0x00

DEVICE_NAMES = {
    DEV_TRAFFIC: "TRAFFIC",
    DEV_SERVO: "SERVO",
    DEV_BUZZER: "BUZZER",
    DEV_SYSTEM: "SYSTEM",
}

IR_ERRORS = {
    -1: "REPEAT",
    -2: "BADSTART",
    -3: "BADBLOCK",
    -4: "BADREP",
    -5: "OVERRUN",
    -6: "BADDATA",
    -7: "BADADDR",
}


# ---------------- logging ----------------
def log(tag, msg):
    if DEBUG:
        print("[%8d] %-7s %s" % (time.ticks_ms(), tag, msg))


# ---------------- devices ----------------
led = Pin(LED_PIN, Pin.OUT, value=0)
red = Pin(TRAFFIC_PINS[0], Pin.OUT, value=0)
yellow = Pin(TRAFFIC_PINS[1], Pin.OUT, value=0)
green = Pin(TRAFFIC_PINS[2], Pin.OUT, value=0)
buzzer = Pin(BUZZER_PIN, Pin.OUT, value=0)   # active buzzer
servo = PWM(Pin(SERVO_PIN), freq=50)

stats = {"rx": 0, "ok": 0, "unknown": 0, "errors": 0, "repeats": 0, "exceptions": 0}
last_frame = None            # (ticks, addr, value)
last_ir_error = []           # error codes captured in the IR callback
led_until = 0


def servo_angle(deg):
    clamped = max(0, min(180, deg))
    if clamped != deg:
        log("WARN", "servo angle %d clamped to %d" % (deg, clamped))
    us = 500 + (clamped * 2000) // 180           # 0.5 ms .. 2.5 ms
    servo.duty_u16(us * 65535 // 20000)
    log("SERVO", "angle=%d pulse=%d us" % (clamped, us))


def traffic_set(state):
    red.value(state == 1)
    yellow.value(state == 2)
    green.value(state == 3)


def traffic(value):
    names = {0: "OFF", 1: "RED", 2: "YELLOW", 3: "GREEN", 4: "CYCLE"}
    log("TRAFFIC", names.get(value, "unknown state %d" % value))
    if value == 4:                           # cycle once
        for s in (1, 2, 3):
            traffic_set(s)
            time.sleep_ms(600)
        traffic_set(0)
    else:
        traffic_set(value)


def beep(value):                             # value x 10 ms
    ms = max(1, value) * 10
    log("BUZZER", "beep %d ms" % ms)
    buzzer.value(1)
    time.sleep_ms(ms)
    buzzer.value(0)


# ---------------- macros (write whatever you like) ----------------
def macro_all_off():
    log("MACRO", "all off")
    traffic_set(0)
    buzzer.value(0)
    servo_angle(90)


def macro_demo():
    log("MACRO", "demo")
    for s in (1, 2, 3):
        traffic_set(s)
        beep(5)
        time.sleep_ms(400)
    traffic_set(0)
    for a in (0, 90, 180, 90):
        servo_angle(a)
        time.sleep_ms(500)


MACROS = {
    0: macro_all_off,
    1: macro_demo,
}


def run_macro(value):
    fn = MACROS.get(value)
    if fn:
        fn()
    else:
        log("WARN", "unknown macro %d" % value)


# ---------------- command table ----------------
COMMANDS = {
    (DEV_TRAFFIC, ACT_SET): traffic,
    (DEV_SERVO, ACT_SET): servo_angle,
    (DEV_BUZZER, ACT_SET): beep,
    (DEV_SYSTEM, ACT_SET): run_macro,
}


# ---------------- command handling ----------------
def handle(addr, value, src="IR"):
    global last_frame
    device, action = addr >> 8, addr & 0xFF
    last_frame = (time.ticks_ms(), addr, value)
    log(src, "frame addr=0x%04X cmd=0x%02X -> %s(%d) action=%d value=%d" % (
        addr, value, DEVICE_NAMES.get(device, "UNKNOWN"), device, action, value))

    fn = COMMANDS.get((device, action))
    if fn is None:
        stats["unknown"] += 1
        log("WARN", "no handler for device=%d action=%d" % (device, action))
        return
    t0 = time.ticks_ms()
    try:
        fn(value)
        stats["ok"] += 1
        log("OK", "done in %d ms" % time.ticks_diff(time.ticks_ms(), t0))
    except Exception as e:
        stats["exceptions"] += 1
        log("ERROR", "handler crashed: %r" % e)
        sys.print_exception(e)


def inject(device, action, value):
    """Simulate a received frame (for testing without ESP32-B)."""
    handle((device << 8) | action, value, src="SIM")


# ---------------- IR receive ----------------
queue = []                                   # filled by callback, drained by main loop


def ir_callback(data, addr, ctrl):
    # keep this short: just record, the main loop does the work and logging
    stats["rx"] += 1
    if data < 0:                             # NEC repeat frame
        stats["repeats"] += 1
        return
    if len(queue) < 16:
        queue.append((addr, data))
    else:
        stats["errors"] += 1


def ir_error(code):
    stats["errors"] += 1
    if len(last_ir_error) < 8:
        last_ir_error.append(code)


ir = NEC_16(Pin(IR_PIN, Pin.IN), ir_callback)
ir.error_function(ir_error)


# ---------------- status / selftest ----------------
def status():
    log("STATUS", "stats=%s queue=%d" % (stats, len(queue)))
    if last_frame:
        t, a, v = last_frame
        log("STATUS", "last frame %d ms ago: addr=0x%04X val=%d" % (
            time.ticks_diff(time.ticks_ms(), t), a, v))
    else:
        log("STATUS", "no frame received yet")


def selftest():
    log("TEST", "=== selftest start ===")
    log("TEST", "traffic light")
    for v in (1, 2, 3, 0):
        inject(DEV_TRAFFIC, ACT_SET, v)
        time.sleep_ms(400)
    log("TEST", "servo")
    for a in (0, 90, 180, 90):
        inject(DEV_SERVO, ACT_SET, a)
        time.sleep_ms(600)
    log("TEST", "buzzer")
    inject(DEV_BUZZER, ACT_SET, 10)
    log("TEST", "macro 0 (all off)")
    inject(DEV_SYSTEM, ACT_SET, 0)
    log("TEST", "=== selftest done ===")


# ---------------- LED ----------------
def led_update(now):
    if time.ticks_diff(led_until, now) > 0:
        led.value(1)
    elif (now % 2000) < 50:
        led.value(1)
    else:
        led.value(0)


# ---------------- main loop ----------------
def main_loop():
    global led_until
    next_stats = time.ticks_add(time.ticks_ms(), STATS_EVERY_MS)
    log("READY", "listening on GPIO %d (Ctrl-C for REPL, then selftest()/inject()/status())" % IR_PIN)
    while True:
        now = time.ticks_ms()

        while last_ir_error:
            code = last_ir_error.pop(0)
            log("IRERR", "%d %s" % (code, IR_ERRORS.get(code, "?")))

        if queue:
            addr, value = queue.pop(0)
            led_until = time.ticks_add(now, 200)
            handle(addr, value)

        if STATS_EVERY_MS and time.ticks_diff(now, next_stats) >= 0:
            next_stats = time.ticks_add(now, STATS_EVERY_MS)
            status()

        led_update(now)
        time.sleep_ms(5)


# ---------------- boot ----------------
for _ in range(3):                           # boot blink
    led.value(1)
    time.sleep_ms(100)
    led.value(0)
    time.sleep_ms(100)

servo_angle(90)
log("BOOT", "ESP32-A started. IR pin=%d LED pin=%d" % (IR_PIN, LED_PIN))
log("BOOT", "pins: traffic=%s servo=%d buzzer=%d" % (TRAFFIC_PINS, SERVO_PIN, BUZZER_PIN))

if SELFTEST_ON_BOOT:
    selftest()

main_loop()