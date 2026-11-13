# =====================================================================
#  ESP32 "A"  -  Smart Node  (MicroPython)
#  Ring light, button, SG90, traffic light, DHT11, AHT20+BMP280,
#  TEMT6000, buzzer  ->  controlled over ESP-NOW from ESP32 "B"
#
#  Save as main.py on the board.  Needs MicroPython >= 1.21 (espnow).
#  Protocol (plain text, broadcast, no pairing needed):
#    A -> B : DBK,S,<dhtT>,<dhtH>,<ahtT>,<ahtH>,<bmpT>,<bmpP>,<light%>,
#             <btnCount>,<ringMode>,<ringHue>,<ringBri>,<servoAngle>,
#             <servoSweep>,<trafficMode>,<trafficMask>,<buzzActive>,<uptime>
#    B -> A : DBK,C,<cmd>,<value>
#             cmd: rm ringMode | rh hue | rb brightness | sa servoAngle |
#                  ss sweep | tm trafficMode | bz buzzer(0 stop,1 beep,
#                  2 melody,3 alarm) | ping
# =====================================================================
import time, struct, gc, math, random
import network, neopixel, dht, espnow
from machine import Pin, PWM, ADC, I2C

# ------------------------------ PINS ---------------------------------
LED_RING_PIN    = 14
SERVO_PIN       = 13
TRAFFIC_RED_PIN = 25
TRAFFIC_YELLOW_PIN = 26
TRAFFIC_GREEN_PIN  = 33
BUTTON_PIN      = 18
DHT11_PIN       = 4
BUZZER_PIN      = 12
TEMT6000_PIN    = 34
I2C_SDA_PIN     = 21
I2C_SCL_PIN     = 22

# --------------------------- SETTINGS --------------------------------
RING_LEDS        = 8      # <-- set to your ring size (12 / 16 / 24 ...)
BUTTON_ACTIVE_LOW = True   # button wired to GND (internal pull-up used)
BUZZER_ACTIVE    = False   # True = active buzzer (on/off only), False = passive (tones)
SERVO_MIN_US     = 500
SERVO_MAX_US     = 2400
ESPNOW_CHANNEL   = 1       # must match ESPNOW_CHANNEL in the B sketch
TELEMETRY_MS     = 500

BCAST = b'\xff\xff\xff\xff\xff\xff'
tick = time.ticks_ms
diff = time.ticks_diff


def hsv(h, s=1.0, v=1.0):
    """h: 0..255 -> (r,g,b) 0..255"""
    h = (h & 255) / 255 * 6
    i = int(h)
    f = h - i
    p = v * (1 - s)
    q = v * (1 - s * f)
    t = v * (1 - s * (1 - f))
    r, g, b = ((v, t, p), (q, v, p), (p, v, t), (p, q, v), (t, p, v), (v, p, q))[i % 6]
    return int(r * 255), int(g * 255), int(b * 255)


# ============================== RING ==================================
class Ring:
    MODES = 6   # 0 off,1 solid,2 rainbow,3 chase,4 breathe,5 twinkle

    def __init__(self):
        self.np = neopixel.NeoPixel(Pin(LED_RING_PIN), RING_LEDS)
        self.mode = 2
        self.prev_mode = 2
        self.hue = 100
        self.bri = 40          # 0..100
        self.phase = 0
        self.last = 0
        self.stars = [0.0] * RING_LEDS

    def set_mode(self, m):
        m %= self.MODES
        if m:
            self.prev_mode = m
        self.mode = m

    def toggle(self):
        self.set_mode(0 if self.mode else self.prev_mode)

    def _fill(self, c):
        for i in range(RING_LEDS):
            self.np[i] = c

    def update(self, now):
        if diff(now, self.last) < 33:
            return
        self.last = now
        self.phase += 1
        n = RING_LEDS
        k = self.bri / 100
        m = self.mode
        if m == 0:
            self._fill((0, 0, 0))
        elif m == 1:
            r, g, b = hsv(self.hue)
            self._fill((int(r * k), int(g * k), int(b * k)))
        elif m == 2:
            for i in range(n):
                r, g, b = hsv(self.phase * 3 + i * 256 // n)
                self.np[i] = (int(r * k), int(g * k), int(b * k))
        elif m == 3:
            head = (self.phase // 2) % n
            r, g, b = hsv(self.hue)
            for i in range(n):
                lvl = max(0.0, 1 - ((head - i) % n) / 6) * k
                self.np[i] = (int(r * lvl), int(g * lvl), int(b * lvl))
        elif m == 4:
            lvl = (math.sin(self.phase * 0.08) + 1) / 2 * k
            r, g, b = hsv(self.hue)
            self._fill((int(r * lvl), int(g * lvl), int(b * lvl)))
        elif m == 5:
            if random.getrandbits(8) < 40:
                self.stars[random.getrandbits(8) % n] = 1.0
            for i in range(n):
                self.stars[i] *= 0.88
                r, g, b = hsv(self.hue + i * 6)
                lvl = self.stars[i] * k
                self.np[i] = (int(r * lvl), int(g * lvl), int(b * lvl))
        self.np.write()


# ============================== SERVO =================================
class Servo:
    def __init__(self):
        self.pwm = PWM(Pin(SERVO_PIN), freq=50)
        self.angle = 90
        self.sweep = False
        self.dir = 1
        self.last = 0
        self.write(90)

    def write(self, a):
        a = max(0, min(180, int(a)))
        self.angle = a
        us = SERVO_MIN_US + (SERVO_MAX_US - SERVO_MIN_US) * a / 180
        self.pwm.duty_u16(int(us / 20000 * 65535))

    def update(self, now):
        if self.sweep and diff(now, self.last) >= 20:
            self.last = now
            a = self.angle + 2 * self.dir
            if a >= 180:
                a, self.dir = 180, -1
            elif a <= 0:
                a, self.dir = 0, 1
            self.write(a)


# ============================= TRAFFIC ================================
class Traffic:
    OFF, AUTO, RED, YELLOW, GREEN, BLINK = range(6)
    SEQ = ((4, 5000), (2, 1500), (1, 5000), (3, 1000))   # G, Y, R, R+Y

    def __init__(self):
        self.pins = (Pin(TRAFFIC_RED_PIN, Pin.OUT), Pin(TRAFFIC_YELLOW_PIN, Pin.OUT),
                     Pin(TRAFFIC_GREEN_PIN, Pin.OUT))
        self.mode = self.AUTO
        self.mask = 0
        self.idx = 0
        self.t0 = tick()
        self.blink_on = False
        self._apply(self.SEQ[0][0])

    def _apply(self, mask):
        self.mask = mask
        for i, p in enumerate(self.pins):
            p.value((mask >> i) & 1)

    def set_mode(self, m):
        self.mode = m % 6
        self.idx = 0
        self.t0 = tick()
        if self.mode == self.OFF:
            self._apply(0)
        elif self.mode == self.AUTO:
            self._apply(self.SEQ[0][0])
        elif self.mode == self.RED:
            self._apply(1)
        elif self.mode == self.YELLOW:
            self._apply(2)
        elif self.mode == self.GREEN:
            self._apply(4)

    def update(self, now):
        if self.mode == self.AUTO:
            if diff(now, self.t0) >= self.SEQ[self.idx][1]:
                self.idx = (self.idx + 1) % len(self.SEQ)
                self.t0 = now
                self._apply(self.SEQ[self.idx][0])
        elif self.mode == self.BLINK:
            if diff(now, self.t0) >= 500:
                self.t0 = now
                self.blink_on = not self.blink_on
                self._apply(2 if self.blink_on else 0)


# ============================== BUZZER ================================
NOTE = {"C5": 523, "D5": 587, "E5": 659, "G5": 784, "A5": 880, "C6": 1047}
PAT_BEEP   = ((2400, 120),)
PAT_CHIRP  = ((3000, 40),)
PAT_MELODY = ((523, 140), (659, 140), (784, 140), (1047, 260), (0, 60),
              (784, 140), (1047, 360))
PAT_ALARM  = ((2000, 220), (1200, 220))


class Buzzer:
    def __init__(self):
        if BUZZER_ACTIVE:
            self.pin = Pin(BUZZER_PIN, Pin.OUT, value=0)
            self.pwm = None
        else:
            self.pwm = PWM(Pin(BUZZER_PIN), freq=1000, duty_u16=0)
        self.seq = None
        self.idx = 0
        self.loop = False
        self.t_end = 0

    def _tone(self, f):
        if BUZZER_ACTIVE:
            self.pin.value(1 if f else 0)
        elif f:
            self.pwm.freq(f)
            self.pwm.duty_u16(24000)
        else:
            self.pwm.duty_u16(0)

    def play(self, seq, loop=False):
        self.seq, self.idx, self.loop = seq, 0, loop
        self._step(tick())

    def _step(self, now):
        f, d = self.seq[self.idx]
        self._tone(f)
        self.t_end = time.ticks_add(now, d)

    def stop(self):
        self.seq = None
        self._tone(0)

    @property
    def active(self):
        return 1 if self.seq else 0

    def update(self, now):
        if self.seq and diff(now, self.t_end) >= 0:
            self.idx += 1
            if self.idx >= len(self.seq):
                if self.loop:
                    self.idx = 0
                else:
                    self.stop()
                    return
            self._step(now)


# ============================== SENSORS ===============================
class AHT20:
    def __init__(self, i2c, addr=0x38):
        self.i2c, self.addr = i2c, addr
        time.sleep_ms(40)
        i2c.writeto(addr, b'\xBE\x08\x00')
        time.sleep_ms(20)

    def trigger(self):
        self.i2c.writeto(self.addr, b'\xAC\x33\x00')

    def fetch(self):
        d = self.i2c.readfrom(self.addr, 7)
        if d[0] & 0x80:
            return None
        h = ((d[1] << 12) | (d[2] << 4) | (d[3] >> 4)) / 1048576 * 100
        t = (((d[3] & 0x0F) << 16) | (d[4] << 8) | d[5]) / 1048576 * 200 - 50
        return t, h


class BMP280:
    def __init__(self, i2c, addr):
        self.i2c, self.addr = i2c, addr
        cal = i2c.readfrom_mem(addr, 0x88, 24)
        (self.T1, self.T2, self.T3, self.P1, self.P2, self.P3, self.P4,
         self.P5, self.P6, self.P7, self.P8, self.P9) = struct.unpack("<HhhHhhhhhhhh", cal)
        i2c.writeto_mem(addr, 0xF5, b'\x08')   # IIR filter x4
        i2c.writeto_mem(addr, 0xF4, b'\x57')   # T x2, P x16, normal mode

    def read(self):
        d = self.i2c.readfrom_mem(self.addr, 0xF7, 6)
        adc_p = (d[0] << 12) | (d[1] << 4) | (d[2] >> 4)
        adc_t = (d[3] << 12) | (d[4] << 4) | (d[5] >> 4)
        v1 = (adc_t / 16384.0 - self.T1 / 1024.0) * self.T2
        v2 = ((adc_t / 131072.0 - self.T1 / 8192.0) ** 2) * self.T3
        t_fine = v1 + v2
        temp = t_fine / 5120.0
        v1 = t_fine / 2.0 - 64000.0
        v2 = v1 * v1 * self.P6 / 32768.0
        v2 = v2 + v1 * self.P5 * 2.0
        v2 = v2 / 4.0 + self.P4 * 65536.0
        v1 = (self.P3 * v1 * v1 / 524288.0 + self.P2 * v1) / 524288.0
        v1 = (1.0 + v1 / 32768.0) * self.P1
        if v1 == 0:
            return temp, None
        p = 1048576.0 - adc_p
        p = (p - v2 / 4096.0) * 6250.0 / v1
        v1 = self.P9 * p * p / 2147483648.0
        v2 = p * self.P8 / 32768.0
        p = p + (v1 + v2 + self.P7) / 16.0
        return temp, p / 100.0


class Sensors:
    def __init__(self):
        self.dht = dht.DHT11(Pin(DHT11_PIN))
        self.adc = ADC(Pin(TEMT6000_PIN))
        self.adc.atten(ADC.ATTN_11DB)
        self.dhtT = self.dhtH = None
        self.ahtT = self.ahtH = None
        self.bmpT = self.bmpP = None
        self.light = 0.0
        self.aht = self.bmp = None
        self.i2c = I2C(0, sda=Pin(I2C_SDA_PIN), scl=Pin(I2C_SCL_PIN), freq=400000)
        devs = self.i2c.scan()
        print("I2C devices:", [hex(d) for d in devs])
        try:
            if 0x38 in devs:
                self.aht = AHT20(self.i2c)
        except OSError as e:
            print("AHT20 init failed", e)
        for a in (0x76, 0x77):
            if a in devs:
                try:
                    if self.i2c.readfrom_mem(a, 0xD0, 1)[0] in (0x58, 0x60):
                        self.bmp = BMP280(self.i2c, a)
                        break
                except OSError as e:
                    print("BMP280 init failed", e)
        self.t_dht = self.t_i2c = self.t_adc = tick()
        self.aht_wait = False

    def update(self, now):
        # light: smoothed, every 100 ms
        if diff(now, self.t_adc) >= 100:
            self.t_adc = now
            s = 0
            for _ in range(8):
                s += self.adc.read_u16()
            pct = s / 8 / 65535 * 100
            self.light = pct if self.light == 0 else self.light * 0.7 + pct * 0.3
        # DHT11: max once / 2.5 s
        if diff(now, self.t_dht) >= 2500:
            self.t_dht = now
            try:
                self.dht.measure()
                self.dhtT, self.dhtH = self.dht.temperature(), self.dht.humidity()
            except OSError:
                pass
        # AHT20 + BMP280: once / second (AHT conversion is split to stay non-blocking)
        if self.aht_wait:
            if diff(now, self.t_i2c) >= 90:
                self.aht_wait = False
                try:
                    r = self.aht.fetch()
                    if r:
                        self.ahtT, self.ahtH = r
                except OSError:
                    pass
        elif diff(now, self.t_i2c) >= 1000:
            self.t_i2c = now
            if self.aht:
                try:
                    self.aht.trigger()
                    self.aht_wait = True
                except OSError:
                    pass
            if self.bmp:
                try:
                    self.bmpT, self.bmpP = self.bmp.read()
                except OSError:
                    pass


# ============================== BUTTON ================================
class Button:
    def __init__(self):
        self.pin = Pin(BUTTON_PIN, Pin.IN, Pin.PULL_UP if BUTTON_ACTIVE_LOW else None)
        self.down = False
        self.t = 0
        self.count = 0

    def update(self, now):
        """returns True once per press"""
        pressed = (self.pin.value() == 0) if BUTTON_ACTIVE_LOW else (self.pin.value() == 1)
        if pressed and not self.down and diff(now, self.t) > 60:
            self.down = True
            self.t = now
            self.count += 1
            return True
        if not pressed and self.down and diff(now, self.t) > 60:
            self.down = False
            self.t = now
        return False


# ============================== RADIO =================================
sta = network.WLAN(network.STA_IF)
sta.active(True)
try:
    sta.disconnect()
except Exception:
    pass
try:
    sta.config(channel=ESPNOW_CHANNEL)
except Exception as ex:
    print("channel set skipped:", ex)
radio = espnow.ESPNow()
radio.active(True)
radio.add_peer(BCAST)

ring = Ring()
servo = Servo()
traffic = Traffic()
buzzer = Buzzer()
sensors = Sensors()
button = Button()
t_boot = tick()


def f1(v):
    return "-99" if v is None else "%.1f" % v


def send_state():
    msg = "DBK,S,%s,%s,%s,%s,%s,%s,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d" % (
        f1(sensors.dhtT), f1(sensors.dhtH), f1(sensors.ahtT), f1(sensors.ahtH),
        f1(sensors.bmpT), f1(sensors.bmpP),
        int(sensors.light), button.count,
        ring.mode, ring.hue, ring.bri,
        servo.angle, 1 if servo.sweep else 0,
        traffic.mode, traffic.mask, buzzer.active,
        diff(tick(), t_boot) // 1000)
    try:
        radio.send(BCAST, msg)
    except OSError as e:
        print("send failed", e)


def handle(raw):
    try:
        p = raw.decode().split(",")
        if len(p) < 4 or p[0] != "DBK" or p[1] != "C":
            return
        cmd, val = p[2], int(p[3])
    except (ValueError, UnicodeError):
        return
    if cmd == "rm":
        ring.set_mode(val)
    elif cmd == "rh":
        ring.hue = val & 255
    elif cmd == "rb":
        ring.bri = max(0, min(100, val))
    elif cmd == "sa":
        servo.sweep = False
        servo.write(val)
    elif cmd == "ss":
        servo.sweep = bool(val)
    elif cmd == "tm":
        traffic.set_mode(val)
    elif cmd == "bz":
        if val == 0:
            buzzer.stop()
        elif val == 1:
            buzzer.play(PAT_BEEP)
        elif val == 2:
            buzzer.play(PAT_MELODY)
        elif val == 3:
            buzzer.play(PAT_ALARM, loop=True)
    send_state()      # instant echo so B's screen reflects the real state


# ============================== MAIN ==================================
print("Node A ready. MAC:", ":".join("%02X" % b for b in sta.config("mac")))
buzzer.play(PAT_MELODY)
t_tx = tick()
t_gc = tick()

while True:
    now = tick()

    while radio.any():
        mac, raw = radio.irecv(0)
        if raw:
            handle(raw)

    if button.update(now):
        ring.toggle()
        buzzer.play(PAT_CHIRP)
        send_state()

    sensors.update(now)
    ring.update(now)
    servo.update(now)
    traffic.update(now)
    buzzer.update(now)

    if diff(now, t_tx) >= TELEMETRY_MS:
        t_tx = now
        send_state()

    if diff(now, t_gc) >= 5000:
        t_gc = now
        gc.collect()

    time.sleep_ms(5)