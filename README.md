# Varuna — Complete Implementation Roadmap

---
// Import the functions you need from the SDKs you need
import { initializeApp } from "firebase/app";
// TODO: Add SDKs for Firebase products that you want to use
// https://firebase.google.com/docs/web/setup#available-libraries

// Your web app's Firebase configuration
const firebaseConfig = {
  apiKey: "AIzaSyAAvL-yCNXxDglQYc5aIMVhUZCJ7XXIOVo",
  authDomain: "varuna-git-1e145.firebaseapp.com",
  databaseURL: "https://varuna-git-1e145-default-rtdb.asia-southeast1.firebasedatabase.app",
  projectId: "varuna-git-1e145",
  storageBucket: "varuna-git-1e145.firebasestorage.app",
  messagingSenderId: "296662102641",
  appId: "1:296662102641:web:8c41cda4b23a9a9bc321d0"
};

// Initialize Firebase
const app = initializeApp(firebaseConfig);
---
## How This is Organized

```
PHASE 0: Workspace & Tools
PHASE 1: Individual Sensor Drivers (one at a time, bench)
PHASE 2: Core Measurement Engine (tilt → water height)
PHASE 3: Flood Detection State Machine
PHASE 4: Communication (SIM800L + GPRS)
PHASE 5: Server (receive data, dispatch alerts)
PHASE 6: Power Management & Adaptive Sampling
PHASE 7: Persistence (EEPROM + SPIFFS)
PHASE 8: Failure Detection & Diagnostics
PHASE 9: Console Commands (serial interface)
PHASE 10: Web Console (browser dashboard)
PHASE 11: C3 Companion (CSV feed + OTA)
PHASE 12: Integration Testing (all systems together)
PHASE 13: Enclosure & Waterproofing
PHASE 14: Field Calibration & Deployment

Each phase has:
  • WHAT you're building
  • WHAT you need before starting
  • EXACT steps
  • HOW to test it
  • DONE WHEN criteria
```

---

## PHASE 0 — Workspace, Tools, and Parts

### Step 0.1: Get All Hardware

```
MICROCONTROLLERS:
  □ ESP32-S3 dev board (with USB-C, UART pins exposed)
  □ Seeed XIAO ESP32-C3

SENSORS:
  □ MPU6050 breakout board (GY-521)
  □ BMP280 breakout board
  □ DS1307 RTC module (with CR2032 battery)
  □ GPS module (NEO-6M or similar, UART output, 9600 baud)

COMMUNICATION:
  □ SIM800L module (with external antenna)
  □ 2× SIM cards with data plans (one for S3, one for C3)
  □ SIM800L buck converter (4.0V, SIM800L is picky about voltage)

POWER:
  □ 8× 18650 Li-Ion cells (2600mAh each)
  □ 18650 battery holder (parallel configuration)
  □ Voltage divider resistors: 33kΩ + 100kΩ
  □ 100nF ceramic capacitor
  □ TP4056 charging module (or similar)

LEDS & INTERFACE:
  □ 5× red LEDs + resistors (obstruction lights)
  □ 2× LEDs (status + algorithm indicator)
  □ 1× momentary push button (algorithm toggle)
  □ 10kΩ pull-up resistor for button

WIRING:
  □ Breadboard(s) — at least 2 full-size
  □ Jumper wires (M-M, M-F, F-F)
  □ Logic level shifters (if needed for 3.3V/5V)

TOOLS:
  □ Multimeter
  □ USB cables (for ESP32-S3 and XIAO C3)
  □ Soldering iron + solder (for final assembly)
```

### Step 0.2: Set Up Development Environment

```
□ Install Arduino IDE 2.x (or PlatformIO — your preference)
□ Install ESP32 board package:
    Arduino → Boards Manager → search "esp32" → install by Espressif
□ Select board: "ESP32S3 Dev Module"
□ Install NO external libraries (everything is written from scratch)
    The firmware uses:
      - Wire.h (built-in, for I2C)
      - HardwareSerial.h (built-in, for UART)
      - EEPROM.h (built-in)
      - SPIFFS.h (built-in)
      - esp_task_wdt.h (built-in, watchdog)
      - esp_sleep.h (built-in, light sleep)
    All sensor drivers are custom (no Adafruit, no SparkFun libs)

□ Verify: compile an empty sketch for ESP32-S3, upload, see serial output
□ Set serial monitor to 115200 baud
```

### Step 0.3: Create Project Structure

```
Create a single .ino file to start. You'll keep everything in
one file (as the documentation describes — no separate .h/.cpp).

varuna_flood_monitor/
  └── varuna_flood_monitor.ino

Start with:

  #include <Wire.h>
  #include <HardwareSerial.h>
  #include <EEPROM.h>
  #include <SPIFFS.h>
  #include <esp_task_wdt.h>
  #include <esp_sleep.h>

  // ─── PIN DEFINITIONS ───
  #define SDA_0  8
  #define SCL_0  9
  #define SDA_1  4
  #define SCL_1  5
  #define GPS_RX 6
  #define GPS_TX 7
  #define SIM_RX 15
  #define SIM_TX 16
  #define SIM_RST 17
  #define BATTERY_PIN 2
  #define STATUS_LED 3
  #define ALGO_BUTTON 12
  #define ALGO_LED 13
  #define C3_FEED_PIN 14

  // ─── CONSTANTS ───
  #define ALPHA 0.98
  #define WDT_TIMEOUT_SEC 120

  void setup() {
    Serial.begin(115200);
    Serial.println("VARUNA BOOT");
  }

  void loop() {
    delay(1000);
  }

□ Upload this. See "VARUNA BOOT" in serial monitor.
□ DONE WHEN: Blank project compiles and runs on ESP32-S3.
```

---

## PHASE 1 — Individual Sensor Drivers

**Rule: Get each sensor working alone before combining anything.**

---

### Step 1.1: I2C Bus Setup

```
WHAT: Initialize two independent I2C buses.

ADD TO setup():

  TwoWire I2C_0 = TwoWire(0);
  TwoWire I2C_1 = TwoWire(1);

  void setup() {
    Serial.begin(115200);
    
    I2C_0.begin(SDA_0, SCL_0, 100000);  // Bus 0: MPU6050
    I2C_1.begin(SDA_1, SCL_1, 100000);  // Bus 1: RTC + BMP280
    
    Serial.println("I2C buses initialized");
  }

WIRE:
  MPU6050: SDA → GPIO 8, SCL → GPIO 9, VCC → 3.3V, GND → GND
  BMP280:  SDA → GPIO 4, SCL → GPIO 5, VCC → 3.3V, GND → GND
  DS1307:  SDA → GPIO 4, SCL → GPIO 5, VCC → 5V*, GND → GND
  (* DS1307 needs 5V. If your board is 3.3V only,
     use level shifters or switch to DS3231 which runs at 3.3V)

TEST:
  Write an I2C scanner for each bus:
  
  void scanBus(TwoWire &bus, const char* name) {
    Serial.printf("Scanning %s...\n", name);
    for (uint8_t addr = 1; addr < 127; addr++) {
      bus.beginTransmission(addr);
      if (bus.endTransmission() == 0) {
        Serial.printf("  Found device at 0x%02X\n", addr);
      }
    }
  }

  Call scanBus(I2C_0, "Bus 0") and scanBus(I2C_1, "Bus 1")

EXPECTED:
  Bus 0: Found device at 0x68 (MPU6050)
  Bus 1: Found device at 0x68 (DS1307) and 0x76 (BMP280)

□ DONE WHEN: All three devices detected on correct buses.
```

---

### Step 1.2: MPU6050 Driver

```
WHAT: Read raw accelerometer and gyroscope values.

WRITE THESE FUNCTIONS:

  1. mpuWriteReg(reg, value)
     → I2C_0 write single byte to MPU6050 register
  
  2. mpuReadReg(reg) → uint8_t
     → I2C_0 read single byte from MPU6050 register
  
  3. initMPU6050() → bool
     → Wake up: write 0x00 to register 0x6B
     → Set sample rate divider: reg 0x19 = 0x07
     → Set DLPF: reg 0x1A = 0x03
     → Set gyro range ±250°/s: reg 0x1B = 0x00
     → Set accel range ±2g: reg 0x1C = 0x00
     → Read WHO_AM_I (reg 0x75) → should be 0x68 or 0x72
     → Return true if WHO_AM_I valid
  
  4. readMPU6050()
     → Burst read 14 bytes starting from register 0x3B
     → Parse into: ax, ay, az (accel), temp, gx, gy, gz (gyro)
     → Convert accel: value / 16384.0 (for ±2g range) → in g
     → Convert gyro: value / 131.0 (for ±250°/s range) → in °/s
     → Print all values to serial

WIRE:
  Already done in Step 1.1.

TEST:
  Call readMPU6050() every 100ms in loop()
  Print: ax, ay, az (in g), gx, gy, gz (in °/s)

  With sensor flat on table:
    ax ≈ 0, ay ≈ 0, az ≈ 1.0 (gravity)
    gx ≈ 0, gy ≈ 0, gz ≈ 0

  Tilt sensor 45°:
    ax or ay should show ~0.7g
    az should show ~0.7g

  Rotate sensor:
    gx, gy, or gz should show rotation rate

□ DONE WHEN: Raw accel and gyro values make physical sense.
  Gravity reads ~1g on Z when flat.
  Tilting changes accel components correctly.
  Rotating shows gyro response.
```

---

### Step 1.3: MPU6050 Calibration

```
WHAT: Measure and store gyroscope offsets and accelerometer reference.

WRITE THESE FUNCTIONS:

  float gyroOffsetX, gyroOffsetY, gyroOffsetZ;
  float refAccX, refAccY, refAccZ;
  float refTiltX, refTiltY;

  5. calibrateGyro()
     → Keep sensor STILL
     → Read gyro 1000 times, 2ms apart
     → Average each axis → gyroOffset{X,Y,Z}
     → These are the "zero" values when not rotating

  6. calibrateAccel()
     → Keep sensor STILL and LEVEL
     → Read accel 500 times, 3ms apart
     → For each reading: compute totalG = sqrt(ax² + ay² + az²)
     → Only keep readings where 0.9 < totalG < 1.1
     → Average valid readings → refAcc{X,Y,Z}
     → Compute reference tilt:
       refTiltX = atan2(refAccY, sqrt(refAccX² + refAccZ²)) × 180/PI
       refTiltY = atan2(-refAccX, sqrt(refAccY² + refAccZ²)) × 180/PI

TEST:
  Run calibration in setup()
  Print offsets:
    Gyro offsets should be small (< 5 °/s typically)
    Accel reference should be near (0, 0, 1.0)
    Reference tilts should be near 0° if sensor is level

□ DONE WHEN: Calibration runs, offsets are small and consistent
  across multiple runs.
```

---

### Step 1.4: Complementary Filter

```
WHAT: Fuse accel + gyro into stable tilt angle.

ADD THESE VARIABLES:
  float filtTiltX = 0, filtTiltY = 0;
  unsigned long prevTime = 0;

WRITE THIS LOGIC (in loop):

  7. Every loop iteration:
     → Read MPU6050
     → Calculate dt = (millis() - prevTime) / 1000.0
     → Clamp: if dt > 2.0, dt = 0.01
     → prevTime = millis()
     
     → Accel angle:
       accelTiltX = atan2(ay, sqrt(ax*ax + az*az)) * 180.0/PI
       accelTiltY = atan2(-ax, sqrt(ay*ay + az*az)) * 180.0/PI
     
     → Gyro rate (with offset correction):
       gyroRateX = (raw_gx / 131.0) - gyroOffsetX
       gyroRateY = (raw_gy / 131.0) - gyroOffsetY
     
     → Complementary filter:
       filtTiltX = 0.98 * (filtTiltX + gyroRateX * dt) + 0.02 * accelTiltX
       filtTiltY = 0.98 * (filtTiltY + gyroRateY * dt) + 0.02 * accelTiltY
     
     → Subtract reference:
       correctedTiltX = filtTiltX - refTiltX
       correctedTiltY = filtTiltY - refTiltY
     
     → Combined angle from vertical:
       theta = sqrt(correctedTiltX * correctedTiltX + 
                    correctedTiltY * correctedTiltY)

TEST:
  Print theta every 100ms
  Sensor flat → theta ≈ 0°
  Tilt 30° → theta ≈ 30°
  Tilt 45° → theta ≈ 45°
  Tilt 90° → theta ≈ 90°
  
  Shake sensor → theta should stay stable (not jump wildly)
  Hold at angle for 60 seconds → should not drift

□ DONE WHEN: theta matches physical angle within ±2°.
  Stable under vibration. No significant drift over 1 minute.
```

---

### Step 1.5: BMP280 Driver

```
WHAT: Read pressure and temperature.

WRITE THESE FUNCTIONS:

  8. bmpReadReg(reg) → uint8_t
     → I2C_1 read from BMP280

  9. bmpWriteReg(reg, value)
     → I2C_1 write to BMP280

  10. initBMP280() → bool
      → Read chip ID from reg 0xD0
      → Accept: 0x58, 0x56, 0x57, or 0x60
      → Read calibration data:
         - T1 (unsigned 16-bit) from reg 0x88-0x89
         - T2 (signed 16-bit) from reg 0x8A-0x8B
         - T3 (signed 16-bit) from reg 0x8C-0x8D
         - P1 through P9 from reg 0x8E-0x9F
      → Configure: reg 0xF4 = 0x57, reg 0xF5 = 0x10
      → Return true

  11. bmpReadData(float *temperature, float *pressure)
      → Read 6 bytes: reg 0xF7-0xFC
      → Raw pressure = (byte0 << 12) | (byte1 << 4) | (byte2 >> 4)
      → Raw temperature = (byte3 << 12) | (byte4 << 4) | (byte5 >> 4)
      → Apply Bosch compensation formulas:
         Temperature compensation → bmpTFine variable
         Pressure compensation using bmpTFine
      → Output: temperature in °C, pressure in hPa

  NOTE: The compensation formulas are in the BMP280 datasheet.
  Use the integer arithmetic versions (not floating point).
  They look complex but are just a sequence of multiply/add/shift.

TEST:
  Read every 1 second, print pressure and temperature
  Expected at sea level: ~1013 hPa, room temperature
  Expected at altitude: lower (drops ~1 hPa per 8.5m elevation)
  
  Blow warm air on sensor → temperature rises
  Seal sensor in container and squeeze → pressure rises

□ DONE WHEN: Pressure reads within ±5 hPa of a known reference.
  Temperature reads room temperature correctly.
  Values are stable (not jumping).
```

---

### Step 1.6: DS1307 RTC Driver

```
WHAT: Read and write date/time that persists across power cycles.

WRITE THESE FUNCTIONS:

  12. bcdToDec(uint8_t val) → uint8_t
      → ((val >> 4) * 10) + (val & 0x0F)

  13. decToBcd(uint8_t val) → uint8_t
      → ((val / 10) << 4) | (val % 10)

  14. initRTC() → bool
      → I2C_1 read 7 bytes from address 0x68, register 0x00
      → Check clock-halt bit (byte 0, bit 7) → clear if set
      → Convert 12hr to 24hr format if bit 6 of byte 2 is set
      → Disable square wave: write 0x00 to register 0x07
      → Return true

  15. readRTC(year, month, day, hour, minute, second)
      → Read 7 bytes from register 0x00
      → Decode BCD: seconds, minutes, hours, day-of-week,
        day, month, year
      → Year = 2000 + decoded_year

  16. writeRTC(year, month, day, hour, minute, second)
      → Encode to BCD
      → Write 7 bytes to register 0x00
      → Ensure clock-halt bit is clear

  17. dateToUnix(year, month, day, hour, minute, second) → uint32_t
      → Count days from 1970-01-01
      → Add leap year corrections
      → Multiply by 86400, add hours*3600 + minutes*60 + seconds

WIRE:
  Already on I2C Bus 1 from Step 1.1.
  Make sure CR2032 battery is installed in RTC module.

TEST:
  In setup(): write a known time (e.g., 2025-01-15 10:30:00)
  In loop(): read and print every second
  → Time should increment by 1 second each time
  
  Power off the ESP32 for 30 seconds
  Power on → read time → should show ~30 seconds later
  (This proves the battery backup is working)

□ DONE WHEN: RTC keeps time, survives power cycle,
  reads/writes correctly in 24-hour format.
```

---

### Step 1.7: GPS NMEA Parser

```
WHAT: Parse GPS position, time, and quality from NMEA sentences.

WIRE:
  GPS TX → GPIO 6 (ESP32 RX)
  GPS RX → GPIO 7 (ESP32 TX)  (may not be needed)
  GPS VCC → 3.3V (or 5V depending on module)
  GPS GND → GND

SETUP:
  HardwareSerial GPS_Serial(1);  // UART1
  GPS_Serial.begin(9600, SERIAL_8N1, GPS_RX, GPS_TX);

WRITE THESE FUNCTIONS:

  18. processGPS()
      → While GPS_Serial.available():
        → Read byte, accumulate into nmeaBuffer[120]
        → On newline:
          → validateNMEAChecksum(buffer) → bool
          → If valid: parseNMEA(buffer)

  19. validateNMEAChecksum(buffer) → bool
      → Find '$' and '*'
      → XOR all bytes between them
      → Compare to hex value after '*'

  20. parseNMEA(buffer)
      → Split by commas
      → If starts with $GPGGA or $GNGGA:
        → Extract: time, lat, lon, fix quality,
          satellites, HDOP, altitude
      → If starts with $GPRMC or $GNRMC:
        → Extract: time, lat, lon, speed, date

  21. nmeaToDecimal(raw, direction) → float
      → degrees = int(raw / 100)
      → minutes = raw - degrees * 100
      → decimal = degrees + minutes / 60.0
      → If direction == 'S' or 'W': negate

VARIABLES:
  float gpsLat, gpsLon, gpsAlt, gpsSpeed, gpsHdop;
  int gpsSatellites;
  bool gpsFixValid;
  int gpsHour, gpsMin, gpsSec;
  int gpsDay, gpsMonth, gpsYear;

TEST:
  Take GPS module near a window (needs sky view)
  Print raw NMEA sentences first → verify they arrive
  Then enable parsing → print decoded values
  
  Time to first fix: 30 seconds to 10 minutes (cold start)
  After fix: lat/lon should match your actual location
  Check on Google Maps

□ DONE WHEN: GPS returns valid position that matches your
  actual location. Satellite count > 4. Fix valid = true.
```

---

### Step 1.8: SIM800L Basic Communication

```
WHAT: Send AT commands and get responses from SIM800L.

WIRE:
  SIM800L TX → GPIO 15 (ESP32 RX)
  SIM800L RX → GPIO 16 (ESP32 TX)
  SIM800L RST → GPIO 17
  SIM800L VCC → 4.0V (NOT 3.3V! Use buck converter or LDO)
  SIM800L GND → GND
  External antenna connected

  ⚠️ CRITICAL: SIM800L needs 4.0V ± 0.4V and up to 2A peak
     during transmission. A weak power supply causes random
     resets. Use a dedicated buck converter or large capacitor.

SETUP:
  HardwareSerial SIM_Serial(2);  // UART2
  SIM_Serial.begin(9600, SERIAL_8N1, SIM_RX, SIM_TX);

WRITE THESE FUNCTIONS:

  22. send_at_command(command, expected, timeout_ms) → result
      → Send command + "\r\n" to SIM_Serial
      → Accumulate response bytes
      → Check for expected substring or "ERROR"
      → Return AT_SUCCESS, AT_TIMEOUT, or AT_ERROR

  23. initSIM800L() → bool
      → Hardware reset: GPIO 17 LOW 200ms, HIGH, wait 2s
      → send_at_command("AT", "OK", 2000) — up to 5 retries
      → send_at_command("ATE0", "OK", 1000)
      → send_at_command("AT+CMGF=1", "OK", 1000)
      → send_at_command("AT+CPIN?", "READY", 5000)
      → send_at_command("AT+CREG?", "0,1", 5000) — or "0,5" for roaming
      → send_at_command("AT+CSQ", "OK", 2000) — read signal
      → Return true if all pass

TEST:
  Insert SIM card (data plan active)
  Run initSIM800L() in setup()
  Print each AT response
  
  Expected:
    AT → OK
    AT+CPIN? → +CPIN: READY
    AT+CREG? → +CREG: 0,1 (registered, home network)
    AT+CSQ → +CSQ: 15,0 (signal strength, 10-31 is usable)

□ DONE WHEN: SIM800L responds to AT commands.
  SIM card detected. Network registered. Signal > 10.
```

---

### Step 1.9: SIM800L GPRS HTTP POST

```
WHAT: Send a JSON payload to a server via GPRS.

WRITE THESE FUNCTIONS:

  24. gprsInit() → bool
      → AT+SAPBR=3,1,"Contype","GPRS"
      → AT+SAPBR=3,1,"APN","<your_apn>"  (e.g., "airtelgprs.com")
      → AT+SAPBR=1,1  (open bearer)
      → AT+SAPBR=2,1  (check IP assigned)
      → Return true if IP received

  25. gprsPostData(payload, length) → bool
      → AT+HTTPINIT
      → AT+HTTPPARA="CID",1
      → AT+HTTPPARA="URL","<server_url>"
      → AT+HTTPPARA="CONTENT","application/json"
      → AT+HTTPDATA=<length>,10000
      → Wait for "DOWNLOAD"
      → Send payload bytes
      → AT+HTTPACTION=1
      → Wait for +HTTPACTION:1,<status>,<length>
      → Check status == 200
      → AT+HTTPTERM
      → Return true if 200

FOR TESTING, you need something to receive the POST.
Easiest option: use a free service like webhook.site:
  1. Go to webhook.site
  2. Copy your unique URL
  3. Set that as your server URL
  4. Send POST → see it appear on the webpage

TEST PAYLOAD:
  char testPayload[] = "{\"d\":\"TEST\",\"h\":100}";
  gprsPostData(testPayload, strlen(testPayload));

□ DONE WHEN: JSON payload appears on webhook.site.
  HTTP response code 200 received by SIM800L.
```

---

### Step 1.10: Battery ADC Reading

```
WHAT: Read battery voltage through voltage divider.

WIRE:
  Battery+ ─── 33kΩ ──┬── GPIO 2 (ADC)
                       │
                     100kΩ
                       │
                      GND
  
  Add 100nF ceramic cap between GPIO 2 and GND.

WRITE:

  26. readBatteryVoltage() → float (millivolts)
      → analogRead(BATTERY_PIN)
      → Convert 12-bit ADC (0-4095) to voltage:
        adcVoltage = reading * 3300.0 / 4095.0  (mV at ADC pin)
        batteryVoltage = adcVoltage * 133.0 / 100.0  (undo divider)
      → Return batteryVoltage in mV

  27. batteryPercentage(voltage_mV) → float
      → Piecewise Li-Ion curve:
        ≥ 4100mV → 90 + (v-4100)*10/100
        ≥ 3850mV → 70 + (v-3850)*20/250
        ≥ 3700mV → 40 + (v-3700)*30/150
        ≥ 3500mV → 20 + (v-3500)*20/200
        ≥ 3300mV → 5 + (v-3300)*15/200
        ≥ 3000mV → 0 + (v-3000)*5/300
        < 3000mV → 0
      → Clamp 0-100

SETUP:
  analogReadResolution(12);

TEST:
  Connect a known voltage source (e.g., bench supply)
  Set to 4.2V → should read ~100%
  Set to 3.7V → should read ~40-50%
  Set to 3.3V → should read ~5%
  
  OR: connect your 18650 battery and check reading
  against multimeter measurement.

□ DONE WHEN: Voltage reading matches multimeter within ±50mV.
  Percentage follows piecewise curve.
```

---

**PHASE 1 CHECKPOINT:**
```
At this point you have 10 independent, tested modules:
  ✓ I2C bus scan
  ✓ MPU6050 raw read
  ✓ MPU6050 calibration
  ✓ Complementary filter → theta
  ✓ BMP280 pressure/temp
  ✓ DS1307 RTC read/write
  ✓ GPS NMEA parser
  ✓ SIM800L AT commands
  ✓ SIM800L GPRS POST
  ✓ Battery ADC

None of them depend on each other yet.
Each one works in isolation.
NOW you start combining.
```

---

## PHASE 2 — Core Measurement Engine

### Step 2.1: Water Height from Tilt (MODE 1)

```
WHAT: Compute H = L × cos(θ)

ADD:
  float olpLength = 200.0;  // cm — your tether length
  float waterHeight = 0.0;

IN LOOP:
  waterHeight = olpLength * cos(theta * PI / 180.0);

TEST:
  Hold sensor flat (θ ≈ 0°) → H ≈ L (200cm)
  Tilt 30° → H ≈ 173cm (200 × cos30°)
  Tilt 45° → H ≈ 141cm
  Tilt 60° → H ≈ 100cm
  Tilt 90° → H ≈ 0cm

  Use a protractor or phone inclinometer to verify angles.

□ DONE WHEN: H = L×cos(θ) matches expected values at
  several known angles within ±5cm.
```

---

### Step 2.2: Lateral Acceleration (Tether Detection)

```
WHAT: Detect whether tether is slack or taut.

ADD:
  float lateralAccel = 0.0;

IN LOOP (after reading MPU6050):
  lateralAccel = sqrt(ax*ax + ay*ay);  // in g units
  // Convert to m/s²: lateralAccel_ms2 = lateralAccel * 9.81;

LOGIC:
  bool tetherTaut = (lateralAccel * 9.81 > 0.15) && (theta > 3.0);

TEST:
  Sensor sitting on table (no tension) → lateralAccel ≈ 0 → SLACK
  Hold sensor tilted with a string pulling on it → lateralAccel > 0 → TAUT
  
  This is hard to test on bench. For now, just verify the
  numbers change when you tilt/pull the sensor.

□ DONE WHEN: lateralAccel is near zero when flat,
  increases when tilted under tension.
```

---

### Step 2.3: Mode Detection Engine

```
WHAT: Classify operating mode 0-3.

ADD:
  int currentMode = 0;  // 0=SLACK, 1=TAUT, 2=FLOOD, 3=SUBMERGED
  float pressureDeviation = 0.0;

WRITE:
  28. detectMode()
      → Read current pressure
      → pressureDeviation = currentPressure - baselinePressure
      
      → IF pressureDeviation > 5.0 (hPa):  // 5 hPa ≈ 5cm depth
           currentMode = 3;  // SUBMERGED
           depth = pressureDeviation / 0.0981;  // cm
           waterHeight = olpLength + depth;
      
      → ELSE IF lateralAccel*9.81 > 0.15 AND theta > 3.0:
           IF theta < 10.0 AND waterHeight > 0.95 * olpLength:
             currentMode = 2;  // FLOOD
             waterHeight = olpLength;
           ELSE:
             currentMode = 1;  // TAUT
             waterHeight = olpLength * cos(theta * PI / 180.0);
      
      → ELSE:
           currentMode = 0;  // SLACK
           waterHeight = 0;  // below threshold, safe

TEST:
  Sensor flat, no tension → MODE 0
  Sensor tilted, simulated tension → MODE 1, check H value
  Sensor nearly vertical (θ < 10°) → MODE 2 (need H > 0.95L)
  Wrap sensor in plastic bag, dip in water → MODE 3
  (pressure increases → submersion detected)
  
  NOTE: MODE 3 test needs BMP280 sealed. If you can't submerge
  it, simulate by blowing air at the sensor through a tube
  (slight pressure increase).

□ DONE WHEN: All 4 modes can be triggered and correctly
  identified. Water height computed appropriately for each.
```

---

### Step 2.4: Pressure Baseline Tracking

```
WHAT: Maintain rolling atmospheric pressure baseline.

ADD:
  #define BASELINE_SIZE 48
  float baselineBuffer[BASELINE_SIZE];
  int baselineIndex = 0;
  int baselineCount = 0;
  float baselinePressure = 0;
  unsigned long lastBaselineUpdate = 0;
  #define BASELINE_INTERVAL 1800000  // 30 minutes

WRITE:
  29. updateBaseline(pressure)
      → Only call when currentMode != 3 (not submerged)
      → If millis() - lastBaselineUpdate < BASELINE_INTERVAL: return
      → baselineBuffer[baselineIndex] = pressure
      → baselineIndex = (baselineIndex + 1) % BASELINE_SIZE
      → if baselineCount < BASELINE_SIZE: baselineCount++
      → Recompute average of all entries → baselinePressure
      → lastBaselineUpdate = millis()

  Initialize: take 10 rapid readings in setup(), average as
  initial baselinePressure.

TEST:
  Print baselinePressure every minute
  Should be stable (±0.5 hPa over an hour indoors)
  If you have a weather front passing, it may drift 1-3 hPa

□ DONE WHEN: Baseline tracks atmospheric pressure.
  Not updated during simulated submersion.
```

---

**PHASE 2 CHECKPOINT:**
```
You now have:
  ✓ Water height from tilt angle
  ✓ Tether slack/taut detection
  ✓ 4-mode detection engine
  ✓ Pressure baseline tracking
  ✓ Depth measurement for submerged mode

The CORE PHYSICS of the system works.
You can pick up the sensor, tilt it, and see a water height.
```

---

## PHASE 3 — Flood Detection State Machine

### Step 3.1: Zone Classification

```
WHAT: Classify water height into 4 zones.

ADD:
  float alertLevelCm = 120.0;
  float warningLevelCm = 180.0;
  float dangerLevelCm = 250.0;

  int currentZone = 0;  // 0=NORMAL, 1=ALERT, 2=WARNING, 3=DANGER

WRITE:
  30. classifyZone(heightCm) → int
      → if height >= dangerLevelCm: return 3 (DANGER)
      → if height >= warningLevelCm: return 2 (WARNING)
      → if height >= alertLevelCm: return 1 (ALERT)
      → return 0 (NORMAL)

TEST:
  classifyZone(50)  → 0 (NORMAL)
  classifyZone(130) → 1 (ALERT)
  classifyZone(200) → 2 (WARNING)
  classifyZone(260) → 3 (DANGER)

□ DONE WHEN: All four zones return correctly.
```

---

### Step 3.2: Rate of Change

```
WHAT: Calculate how fast water is rising or falling.

ADD:
  float previousHeight = -1;
  unsigned long previousHeightTime = 0;
  float ratePer15Min = 0;

WRITE:
  31. calculateRateOfChange(currentHeight, currentTime)
      → If previousHeight < 0: initialize, return
      → elapsed = (currentTime - previousHeightTime) / 1000.0 (seconds)
      → If elapsed < 60: return (too soon, noisy)
      → change = currentHeight - previousHeight
      → ratePer15Min = change * (900.0 / elapsed)  (normalize to 15 min)
      → Clamp: if |ratePer15Min| > 200: ratePer15Min = 0 (glitch)
      → Update previousHeight, previousHeightTime

  32. classifyRate(ratePer15Min) → int
      → if rate < 0: return 0 (FALLING → treated as SLOW)
      → if rate < 2.0: return 0 (SLOW)
      → if rate < 5.0: return 1 (MODERATE)
      → return 2 (FAST)

TEST:
  Simulate by manually setting waterHeight to increasing values:
    100, 102, 104, 106 over 4 minutes
    → rate should show ~2-3 cm/15min → MODERATE

□ DONE WHEN: Rate calculation produces sensible values.
  Rate categories match thresholds.
```

---

### Step 3.3: Sustained Rise Detection

```
WHAT: Detect if water has been consistently rising.

ADD:
  #define SUSTAINED_BUF_SIZE 4
  float sustainedBuffer[SUSTAINED_BUF_SIZE];
  uint32_t sustainedTimeBuffer[SUSTAINED_BUF_SIZE];
  int sustainedBufIndex = 0;
  int sustainedBufCount = 0;
  bool sustainedRise = false;

WRITE:
  33. updateSustainedBuffer(height, timestamp)
      → Store in circular buffer
      → If count < 4: sustainedRise = false; return
      → Extract ordered sequence (oldest → newest)
      → netRising = (newest > oldest + 0.5)
      → Count rising pairs (each > previous + 0.5)
      → sustainedRise = netRising AND (riseCount >= 2)

TEST:
  Feed: [100, 102, 104, 106] → sustained = TRUE
  Feed: [100, 102, 99, 101] → sustained = FALSE
  Feed: [100, 100, 100, 100] → sustained = FALSE

□ DONE WHEN: Sustained detection matches the examples
  in the documentation.
```

---

### Step 3.4: Decision Matrix

```
WHAT: Look up response level from zone × rate × sustained.

ADD:
  #define RESP_NORMAL 0
  #define RESP_WATCH 1
  #define RESP_WARNING 2
  #define RESP_FLOOD 3
  #define RESP_CRITICAL 4

  int currentResponseLevel = RESP_NORMAL;

WRITE:
  34. lookupDecisionMatrix(zone, rateCategory, sustained) → int
      
      static const int matrix[4][3][2] = {
        // [zone][rate][sustained: 0=no, 1=yes]
        // Zone NORMAL:
        {{RESP_NORMAL,RESP_NORMAL}, {RESP_NORMAL,RESP_WATCH}, {RESP_NORMAL,RESP_WATCH}},
        // Zone ALERT:
        {{RESP_WATCH,RESP_WATCH}, {RESP_WATCH,RESP_WARNING}, {RESP_WATCH,RESP_WARNING}},
        // Zone WARNING:
        {{RESP_WARNING,RESP_FLOOD}, {RESP_WARNING,RESP_FLOOD}, {RESP_FLOOD,RESP_CRITICAL}},
        // Zone DANGER:
        {{RESP_FLOOD,RESP_CRITICAL}, {RESP_CRITICAL,RESP_CRITICAL}, {RESP_CRITICAL,RESP_CRITICAL}}
      };
      
      return matrix[zone][rateCategory][sustained ? 1 : 0];

TEST:
  lookupDecisionMatrix(0, 0, false) → NORMAL
  lookupDecisionMatrix(1, 1, true) → WARNING
  lookupDecisionMatrix(3, 2, true) → CRITICAL
  Test every cell against the table in Section 18.

□ DONE WHEN: All 24 matrix cells return correct values.
```

---

### Step 3.5: Step-Down (De-escalation) Logic

```
WHAT: Prevent false all-clears with hysteresis.

ADD:
  int stepDownConsecutive = 0;
  unsigned long stateEntryTime = 0;
  
  #define STEPDOWN_READINGS_REQUIRED 4
  #define STEPDOWN_NORMAL_READINGS 8
  #define MIN_TIME_CRITICAL 900    // seconds
  #define MIN_TIME_FLOOD 1800
  #define MIN_TIME_WARNING 1800
  #define MIN_TIME_WATCH 900

WRITE:
  35. evaluateStepDown(currentLevel, matrixLevel, waterHeight,
                       rate, sustained, now) → int (new level)

      → If matrixLevel > currentLevel:
          ESCALATE immediately, reset stepDownConsecutive = 0,
          stateEntryTime = now
          return matrixLevel

      → If matrixLevel == currentLevel:
          HOLD, reset stepDownConsecutive = 0
          return currentLevel

      → If matrixLevel < currentLevel:
          Check step-down conditions for current level:
          
          CRITICAL→down: water < dangerLevelCm AND rate ≤ SLOW
          FLOOD→down: water < warningLevelCm
          WARNING→down: water < alertLevelCm
          WATCH→down: water < alertLevelCm AND rate ≤ SLOW AND !sustained
          
          If condition MET:
            stepDownConsecutive++
            required = (currentLevel == RESP_WATCH) ? 8 : 4
            minTime = getMinTimeForLevel(currentLevel)
            timeAtLevel = now - stateEntryTime
            
            If stepDownConsecutive >= required AND timeAtLevel >= minTime:
              DROP ONE LEVEL (not to matrixLevel!)
              stepDownConsecutive = 0
              stateEntryTime = now
              return currentLevel - 1
            Else:
              return currentLevel (hold, accumulating)
          
          If condition NOT MET:
            stepDownConsecutive = 0
            return currentLevel

TEST:
  Simulate escalation: set currentLevel = NORMAL,
  call with matrixLevel = FLOOD → should jump to FLOOD instantly
  
  Simulate de-escalation: set currentLevel = FLOOD,
  call with matrixLevel = NORMAL multiple times:
    Call 1: stepDown = 1, return FLOOD (hold)
    Call 2: stepDown = 2, return FLOOD (hold)
    Call 3: stepDown = 3, return FLOOD (hold)
    Call 4: stepDown = 4, IF time gate met → return WARNING (drop 1)
    NOT NORMAL — drops to WARNING, then must accumulate again

□ DONE WHEN: Escalation is instant. De-escalation takes 4 readings
  minimum and drops one level at a time. Time gates work.
```

---

### Step 3.6: Full Flood Evaluation Function

```
WHAT: Tie everything together into evaluateFloodStatus().

WRITE:
  36. evaluateFloodStatus(waterHeight, currentTime)
  
      Pseudocode:
      
      readingsSinceBoot++
      
      // COLD START LOGIC (first reading)
      if readingsSinceBoot == 1:
        zone = classifyZone(waterHeight)
        if zone == DANGER: currentResponseLevel = RESP_CRITICAL
        elif zone == WARNING: currentResponseLevel = RESP_WARNING
        else: currentResponseLevel = RESP_NORMAL
        stateEntryTime = currentTime
        return
      
      // Normal evaluation
      zone = classifyZone(waterHeight)
      calculateRateOfChange(waterHeight, currentTime)
      rateCategory = classifyRate(ratePer15Min)
      updateSustainedBuffer(waterHeight, currentTime)
      
      // Overrides
      effectiveSustained = sustainedRise
      if readingsSinceBoot <= 4 AND zone >= WARNING:
        effectiveSustained = true
      if ratePer15Min > 30 AND zone >= ALERT:
        effectiveSustained = true
      if ratePer15Min > 50:
        effectiveSustained = true
      
      // Matrix lookup
      matrixLevel = lookupDecisionMatrix(zone, rateCategory,
                                          effectiveSustained)
      
      // Step-down evaluation
      newLevel = evaluateStepDown(currentResponseLevel, matrixLevel,
                                   waterHeight, ratePer15Min,
                                   effectiveSustained, currentTime)
      
      // Apply
      if newLevel != currentResponseLevel:
        previousResponseLevel = currentResponseLevel
        currentResponseLevel = newLevel
        // Level change → will be included in next HTTP POST
        // Server detects transition and handles notification
        forceSaveEeprom()

TEST:
  Create a test sequence that simulates a flood:
  
  Time 0:   height = 50   → NORMAL
  Time 5m:  height = 80   → NORMAL (below alert)
  Time 10m: height = 130  → WATCH (in alert zone)
  Time 15m: height = 185  → WARNING (in warning zone)
  Time 20m: height = 200  → WARNING (still warning zone)
  Time 25m: height = 220  → FLOOD (approaching danger)
  Time 30m: height = 260  → CRITICAL (in danger zone + rising)
  
  Then simulate receding:
  Time 35m: height = 250  → CRITICAL (still in danger)
  Time 40m: height = 230  → CRITICAL (step-down counting)
  Time 45m: height = 210  → CRITICAL (step-down counting)
  Time 50m: height = 190  → CRITICAL (step-down counting)
  Time 55m: height = 170  → FLOOD (dropped one level after 4 readings)
  ...continue until NORMAL

  Print response level at each step.
  Verify it matches your expectations from the matrix + step-down rules.

□ DONE WHEN: Full evaluation chain works end to end.
  Escalation is fast, de-escalation is slow and gradual.
  Cold start logic works (boot into flood → immediate response).
```

---

**PHASE 3 CHECKPOINT:**
```
You now have the complete flood intelligence:
  ✓ Zone classification
  ✓ Rate of change calculation
  ✓ Sustained rise detection
  ✓ Decision matrix lookup
  ✓ Step-down hysteresis
  ✓ Full evaluation function with overrides and cold start

The state machine can be tested on the bench by feeding
simulated water heights. No real water needed.
```

---

## PHASE 4 — Communication Integration

### Step 4.1: JSON Payload Construction

```
WHAT: Build the server-bound JSON from sensor data.

WRITE:
  37. buildPayload(buffer, maxLen) → int (bytes written)
      → Construct JSON like:
        {"d":"VARUNA_TEST",
         "lat":12.97,"lon":77.59,
         "health":85,"mode":1,
         "originDist":0.0,"draftRate":0.0,
         "r":[{"t":1706000000,"h":150.3,"r":2.5,
               "z":1,"l":1,"b":85,"s":0}]}
      → Use snprintf or manual string building
      → Do NOT use ArduinoJson library — build manually
        to avoid memory overhead

  38. floatToStr(value, decimalPlaces, buffer)
      → Custom float-to-string (see Section 37 of docs)
      → Handle NaN, Inf, negative numbers
      → No sprintf %f (pulls in large lib on ESP32)

TEST:
  Build payload with current sensor values
  Print to serial
  Paste into a JSON validator (jsonlint.com)
  → Must be valid JSON, no trailing commas, proper quoting

□ DONE WHEN: Valid JSON generated from live sensor data.
```

---

### Step 4.2: Transmit Buffer

```
WHAT: Buffer readings between transmit intervals.

ADD:
  #define TX_BUFFER_SIZE 30
  struct TxReading {
    uint32_t timestamp;
    float waterHeight;
    float rateOfChange;
    int zone;
    int responseLevel;
    float batteryPercent;
    bool sustained;
  };
  TxReading txBuffer[TX_BUFFER_SIZE];
  int txBufferCount = 0;

WRITE:
  39. addToTransmitBuffer(reading)
      → If buffer full: shift oldest out (or overwrite oldest)
      → Add new reading at txBufferCount
      → txBufferCount++

  40. transmitBufferedData()
      → Build JSON with all buffered readings in r[] array
      → Call gprsPostData()
      → On success: txBufferCount = 0
      → On failure: keep buffer, increment fail counter
      → On 3 failures: archive to SPIFFS

TEST:
  Add 5 readings manually
  Transmit → should POST JSON with 5 entries in r[]
  Verify on webhook.site

□ DONE WHEN: Multiple readings buffered and transmitted
  in a single POST. Buffer clears on success.
```

---

### Step 4.3: GPS → RTC Sync

```
WHAT: Sync accurate GPS time to the RTC.

WRITE:
  41. syncRTCfromGPS()
      → Check conditions: GPS fix valid, year ≥ 2024,
        HDOP ≤ 5.0, satellites ≥ 4, not synced in last 24h
      → Write GPS date/time to DS1307 via writeRTC()
      → Read back and validate
      → Set flag: gpsTimeSynced = true

  42. getBestTimestamp() → uint32_t
      → If RTC time valid: return currentUnixTime
      → Else: return millis()/1000

IN LOOP: Check sync conditions periodically.

TEST:
  Let GPS get a fix
  Print time from GPS and from RTC
  After sync: they should match within 1 second

□ DONE WHEN: RTC shows correct time after GPS sync.
  Time persists across reboot (RTC battery backup).
```

---

### Step 4.4: SMS Diagnostic Channel

```
WHAT: Receive and respond to diagnostic SMS from field engineers.

WRITE:
  43. checkSimURC()
      → Monitor SIM_Serial for unsolicited result codes
      → Detect +CMT (incoming SMS)
      → Parse sender number and message body
      → If sender is in authorized list:
        processSMSCommand(sender, command)

  44. processSMSCommand(sender, command)
      → Match command string (PING, STATUS, BATT, etc.)
      → Generate response string
      → Queue SMS reply (max 5 in queue)
  
  45. sendSMSReply(phone, message)
      → AT+CMGS="<phone>"
      → Wait for ">"
      → Send message text
      → Send Ctrl+Z (0x1A)
      → Wait for "+CMGS" response

  46. contactExists(phone, list) → bool
      → Compare last 10 digits of phone

FOR NOW: hardcode 1-2 authorized numbers for testing.

TEST:
  Send SMS "PING" from your phone to the SIM card number
  Device should reply: "PONG <uptime>"
  
  Send "BATT" → should reply with battery voltage
  Send "STATUS" → should reply with system summary

□ DONE WHEN: SMS commands work for at least PING and BATT.
  Unauthorized numbers are ignored.
```

---

## PHASE 5 — Server Side

### Step 5.1: Set Up Server

```
WHAT: Create a server that receives HTTP POST from the device.

OPTIONS (pick one):
  A. Node.js + Express (simplest)
  B. Python + Flask
  C. Firebase Cloud Functions
  D. Any language you know — it's just an HTTP endpoint

EXAMPLE WITH NODE.JS:

  mkdir varuna-server && cd varuna-server
  npm init -y
  npm install express body-parser

  // server.js
  const express = require('express');
  const app = express();
  app.use(express.json());

  let latestData = {};

  app.post('/api/data', (req, res) => {
    console.log('Received:', JSON.stringify(req.body, null, 2));
    latestData = req.body;
    
    // Check for level transitions
    const readings = req.body.r || [];
    readings.forEach(r => {
      if (r.l >= 2) {
        console.log('⚠️ ELEVATED LEVEL:', r.l, 'at height', r.h);
        // TODO: dispatch notifications
      }
    });
    
    res.status(200).json({ ok: true });
  });

  app.get('/api/latest', (req, res) => {
    res.json(latestData);
  });

  app.listen(3000, () => console.log('Server on port 3000'));

DEPLOY:
  For testing: use ngrok to expose localhost
    npx ngrok http 3000
    → Get a public URL like https://abc123.ngrok.io
  
  Set this as your device's server URL:
    SERVER:https://abc123.ngrok.io/api/data

  For production: deploy to a VPS (DigitalOcean, AWS, etc.)

TEST:
  Power up device → let it transmit
  See data arrive in server console
  Access /api/latest in browser → see latest reading

□ DONE WHEN: Device data arrives at your server.
  You can see readings in the console and via API.
```

---

### Step 5.2: Database Storage

```
WHAT: Store readings persistently.

ADD TO SERVER:
  Use SQLite (simplest) or PostgreSQL/MongoDB

  CREATE TABLE readings (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    station_id TEXT,
    timestamp INTEGER,
    water_height REAL,
    rate REAL,
    zone INTEGER,
    response_level INTEGER,
    battery REAL,
    sustained BOOLEAN,
    health INTEGER,
    mode INTEGER,
    lat REAL,
    lon REAL,
    received_at DATETIME DEFAULT CURRENT_TIMESTAMP
  );

  On each POST: insert all readings from r[] array

TEST:
  Let device transmit for 1 hour
  Query database → verify readings are stored
  SELECT * FROM readings WHERE station_id = 'VARUNA_TEST' 
  ORDER BY timestamp DESC LIMIT 10;

□ DONE WHEN: Historical readings are queryable.
```

---

### Step 5.3: Level Transition Detection

```
WHAT: Server detects when response level changes and triggers alerts.

ADD TO SERVER:

  // Track previous level per station
  let stationLevels = {};  // { "VARUNA_TEST": 0 }

  function processReadings(stationId, readings) {
    const prevLevel = stationLevels[stationId] || 0;
    
    readings.forEach(r => {
      if (r.l > prevLevel) {
        // ESCALATION
        console.log(`🚨 ESCALATION: ${stationId} ${prevLevel} → ${r.l}`);
        dispatchAlerts(stationId, r);
      } else if (r.l < prevLevel) {
        // DE-ESCALATION
        console.log(`✅ DE-ESCALATION: ${stationId} ${prevLevel} → ${r.l}`);
        sendDeEscalation(stationId, r);
      }
      stationLevels[stationId] = r.l;
    });
  }

TEST:
  Simulate escalation by tilting sensor past thresholds
  Server should log "ESCALATION" messages
  Let water recede → server logs "DE-ESCALATION"

□ DONE WHEN: Server correctly detects every level transition.
```

---

### Step 5.4: SMS Notification Dispatch

```
WHAT: Server sends SMS to officials when level changes.

USE AN SMS API:
  Twilio (international, reliable, costs money)
  Fast2SMS (India-specific, cheaper)
  MSG91 (India-specific)
  Textlocal (India-specific)

EXAMPLE WITH TWILIO:

  npm install twilio

  const twilio = require('twilio');
  const client = twilio(ACCOUNT_SID, AUTH_TOKEN);

  function sendAlertSMS(phone, message) {
    client.messages.create({
      body: message,
      from: '+1XXXXXXXXXX',
      to: phone
    }).then(msg => console.log('SMS sent:', msg.sid));
  }

  function dispatchAlerts(stationId, reading) {
    const contacts = getContactsForLevel(stationId, reading.l);
    const message = formatAlertMessage(stationId, reading);
    contacts.forEach(c => sendAlertSMS(c.phone, message));
  }

CONTACTS DATABASE:
  CREATE TABLE contacts (
    id INTEGER PRIMARY KEY,
    station_id TEXT,
    name TEXT,
    phone TEXT,
    trigger_level INTEGER,
    active BOOLEAN DEFAULT TRUE
  );

  INSERT INTO contacts VALUES
    (1, 'VARUNA_TEST', 'Test Engineer', '+91XXXXXXXXXX', 1, TRUE);

TEST:
  Set your own phone number as a contact
  Trigger a level change on the device
  → Your phone should receive the alert SMS

□ DONE WHEN: You receive an SMS alert on your phone when
  the device escalates past a threshold.
```

---

### Step 5.5: Server Dashboard (Basic)

```
WHAT: Web page showing current station status.

SIMPLEST APPROACH: Serve a static HTML page from your Express server.

  app.use(express.static('public'));

  // public/index.html
  // Fetch /api/latest every 10 seconds
  // Display: water height, zone, level, battery, mode
  // Use colored backgrounds for zones (green/yellow/orange/red)

THIS IS THE MINIMUM VIABLE DASHBOARD.
You can make it fancy later with charts and maps.

□ DONE WHEN: You can open a webpage and see live station data.
```

---

**PHASE 5 CHECKPOINT:**
```
End-to-end data flow works:
  Device reads sensors
    → Evaluates flood state
    → POSTs JSON to server
    → Server stores readings
    → Server detects level changes
    → Server sends SMS to contacts
    → Dashboard shows live data

This is the MINIMUM VIABLE SYSTEM.
Everything after this is robustness and polish.
```

---

## PHASE 6 — Power Management & Adaptive Sampling

### Step 6.1: Adaptive Intervals

```
WHAT: Change sample/transmit rates based on flood level + battery.

WRITE:
  47. updateAdaptiveIntervals()
      → Look up intervals from the table in Section 6:
        
        Based on currentResponseLevel + batteryPercent:
        
        if level == CRITICAL && battery >= 20:
          sampleInterval = 120000    // 2 min
          transmitInterval = 120000  // 2 min
        elif level == NORMAL && battery >= 20 && rate is slow:
          sampleInterval = 1800000   // 30 min
          transmitInterval = 3600000 // 60 min
        // ... etc for all combinations
      
      → Call flushSustainedBufferIfIntervalChanged() if interval
        changed by 4× or more

TEST:
  Print intervals to serial
  Manually set response level → verify intervals change
  Manually set battery percentage low → verify conservation mode

□ DONE WHEN: Intervals match the table in Section 6 for all
  response level × battery combinations.
```

---

### Step 6.2: Light Sleep

```
WHAT: Sleep between samples to save power.

WRITE:
  48. canLightSleep() → bool
      → Algorithm enabled
      → sampleInterval >= 300000 (5 minutes)
      → currentResponseLevel != RESP_CRITICAL
      → No active SMS exchange
      → No pending diagnostic

  49. enterLightSleep(durationMs)
      → Guard: if < 10000: return
      → Clamp: if > (WDT_TIMEOUT-10)*1000: clamp
      → Turn off LEDs
      → Reset parser indices
      → Serial.flush()
      → Remove from watchdog
      → esp_sleep_enable_timer_wakeup(duration * 1000)
      → esp_light_sleep_start()
      → Post-wake: re-add watchdog, reset timers, flush GPS buffer

IN LOOP:
  If algorithm enabled and time to next event > 30 seconds:
    enterLightSleep(timeToNext - 5000)  // 5 second safety margin

TEST:
  Set sampleInterval to 5 minutes
  Observe: device sleeps between samples
  Measure current draw with multimeter:
    Active: ~80-150 mA (with SIM800L)
    Sleep: ~10-20 mA (peripherals still powered)

□ DONE WHEN: Device sleeps and wakes correctly.
  No missed samples. Watchdog doesn't trigger during sleep.
```

---

### Step 6.3: Low Battery Shutdown

```
WHAT: Protect batteries from deep discharge.

ADD:
  int lowVoltageCount = 0;

IN LOOP:
  if batteryVoltage < 3000:  // mV
    lowVoltageCount++
    if lowVoltageCount >= 3:  // 3 consecutive readings
      forceSaveEeprom()
      // Turn off everything
      // Enter deep sleep (no timer = sleep forever)
      esp_deep_sleep_start()
  else:
    lowVoltageCount = 0

TEST:
  Use a bench supply, lower voltage below 3.0V
  Device should save state and go to deep sleep
  (Current drops to < 1 mA)

□ DONE WHEN: Device shuts down safely at low voltage.
  State is preserved in EEPROM.
```

---

## PHASE 7 — Persistence

### Step 7.1: EEPROM State Save/Restore

```
WHAT: Survive reboots without losing flood state.

WRITE:
  50. eepromWriteFloat(addr, value)
  51. eepromReadFloat(addr) → float
  52. eepromWriteUint32(addr, value)
  53. eepromReadUint32(addr) → uint32_t

  54. saveStateToEeprom()
      → Write magic byte 0xA5 at address 0
      → Write response level at address 1
      → Write sustained buffer (4 floats at address 2-17)
      → Write timestamp at address 18
      → Write thresholds, OLP length, peak data
      → Follow the layout in Section 10

  55. restoreStateFromEeprom() → bool
      → Read magic byte — if not 0xA5: return false
      → Read saved timestamp
      → If (now - savedTime) > 1800 seconds: return false (too old)
      → Restore all values
      → Validate: level 0-4, heights positive, etc.
      → Return true

  56. markEepromDirty()
      → Set flag eepromDirty = true

  57. saveEepromIfNeeded()
      → If eepromDirty AND (millis() - lastSave > 1800000):
        → saveStateToEeprom()
        → eepromDirty = false
        → lastSave = millis()

  58. forceSaveEeprom()
      → saveStateToEeprom() immediately
      → Reset dirty flag and timer

IN SETUP:
  EEPROM.begin(512);
  if (restoreStateFromEeprom()):
    Serial.println("WARM BOOT — state restored")
  else:
    Serial.println("COLD BOOT — defaults loaded")

TEST:
  1. Let system run and reach WARNING level
  2. Reboot (REBOOT command or power cycle)
  3. After reboot: system should resume at WARNING level
     (not start from NORMAL)

□ DONE WHEN: State persists across reboots.
  Old state (> 30 min) is rejected.
  Validation catches corrupt data.
```

---

### Step 7.2: SPIFFS Data Archival

```
WHAT: Archive readings to flash filesystem.

WRITE:
  59. initSPIFFS()
      → SPIFFS.begin(true)  // format on first use

  60. archiveReading(timestamp, height, rate, zone, response, battery)
      → Filename: /log_YYYYMMDD.csv
      → Open file in append mode
      → Write: timestamp,height,rate,zone,response,battery\n
      → Close file

  61. checkSPIFFSSpace()
      → If SPIFFS.totalBytes() - SPIFFS.usedBytes() < 1024:
        → Delete oldest file
        → Log deletion

TEST:
  Write 100 readings
  Open serial → DUMP command → see readings come back
  Reboot → readings still there (SPIFFS survives reboot)
  Fill storage → verify oldest file is deleted

□ DONE WHEN: Readings are archived. Space management works.
  Data survives reboot.
```

---

## PHASE 8 — Failure Detection

### Step 8.1: Sensor Health Checks

```
WHAT: Detect sensor failures automatically.

WRITE:
  62. checkSensorHealth()   (call every 60 seconds)
      → Probe MPU6050 (I2C Bus 0, address 0x68)
        → 5 consecutive failures → mpuHealthy = false
      → Probe BMP280 (I2C Bus 1, address 0x76)
        → Failure → bmpAvailable = false
      → If was offline, re-probe → recovery

  63. i2cBusRecovery(sdaPin, sclPin)
      → Set SDA as INPUT_PULLUP
      → Clock 9 SCL pulses
      → Generate STOP condition
      → Re-init I2C bus

  64. checkAccelVariance()   (every 50 readings)
      → If variance < 0.0001 → MPU6050_FROZEN

  65. checkGravityMagnitude()  (every reading)
      → g = sqrt(ax² + ay² + az²)
      → If |g - 9.81| > 1.0 for 20 consecutive → ACCEL_DRIFT

TEST:
  Disconnect MPU6050 wire → within 60 seconds:
    → Serial: "MPU6050 OFFLINE"
  Reconnect → within 60 seconds:
    → Serial: "MPU6050 RECOVERED"

□ DONE WHEN: Sensor disconnection is detected and reported.
  Recovery works when sensor is reconnected.
```

---

### Step 8.2: Watchdog Timer

```
WHAT: Recover from MCU crashes.

IN SETUP:
  esp_task_wdt_init(180, true);  // 180s for init
  esp_task_wdt_add(NULL);

AFTER SETUP:
  esp_task_wdt_init(WDT_TIMEOUT_SEC, true);  // 120s operational

IN LOOP:
  esp_task_wdt_reset();  // Every iteration

IN LONG OPERATIONS (AT command waits):
  esp_task_wdt_reset();  // Inside wait loops

TEST:
  Add a while(1){} in loop temporarily
  → After 120 seconds: ESP32 resets automatically
  → On reboot: esp_reset_reason() shows watchdog reset

□ DONE WHEN: Watchdog triggers on intentional hang.
  Normal operation doesn't trigger it.
```

---

### Step 8.3: Geofence & Tether Monitoring

```
WHAT: Detect if buoy has drifted from anchor.

WRITE:
  66. checkGeofence()
      → distance = distanceFromOrigin(gpsLat, gpsLon,
                                       originLat, originLon)
      → If distance > geofenceRadiusM:
        geofenceBreachCount++
        if count >= 3: FLAG TETHER_DETACHMENT

  67. distanceFromOrigin(lat1, lon1, lat2, lon2) → float meters
      → dLat = (lat1 - lat2) * 111320.0
      → dLon = (lon1 - lon2) * 111320.0 * cos(lat2 * PI / 180.0)
      → return sqrt(dLat*dLat + dLon*dLon)

TEST:
  Set origin to your current GPS position
  Set geofence to 10 meters
  Walk 15 meters away with the device
  → Should detect breach after 3 GPS fixes

□ DONE WHEN: Geofence breach detected when moving beyond radius.
```

---

### Step 8.4: Health Score

```
WHAT: Composite 0-100 health score.

WRITE:
  68. computeHealthScore() → int
      → MPU status:     20 if healthy, 10 if recovering, 0 if offline
      → BMP status:     15 if healthy, 0 if offline
      → Battery:        15 if >70%, 10 if 20-70%, 5 if 5-20%, 0 if <5%
      → GPS:            10 if fix+HDOP<3, 5 if fix, 0 if no fix
      → SIM:            10 if registered+strong, 5 if weak, 0 if none
      → Tether:         15 if intact, 10 if unchecked, 0 if breach
      → Buoy integrity: 10 if no leak, 5 if slow draft, 0 if leak
      → Sensor agreement: 5 if cross-checks pass, 0 if disagree
      → Sum all → clamp 0-100

TEST:
  Normal operation → score 80-100
  Disconnect MPU → score drops by ~20
  Low battery → score drops by ~10

□ DONE WHEN: Health score reflects actual system condition.
```

---

## PHASE 9 — Console Commands

### Step 9.1: Command Parser

```
WHAT: Parse serial commands and route to handlers.

WRITE:
  69. processCommand(String cmd)
      → cmd.trim()
      → cmd.toUpperCase() for matching
      
      → if cmd == "PING": handlePing()
      → if cmd == "FLOODSTATUS": handleFloodStatus()
      → if cmd == "SENSORTEST": handleSensorTest()
      → if cmd == "GPSFIX": handleGPSFix()
      → if cmd == "BATT": handleBatt()
      → if cmd == "SIMSTATUS": handleSimStatus()
      → if cmd == "SIMSIGNAL": handleSimSignal()
      → if cmd == "NETTEST": handleNetTest()
      → if cmd == "GETTHRESH": handleGetThresh()
      → if cmd == "GETCONFIG": handleGetConfig()
      → if cmd == "DUMP": handleDump()
      → if cmd == "SIMREINIT": handleSimReinit()
      → if cmd == "SIMRESET": handleSimReset()
      → if cmd == "REBOOT": handleReboot()
      → if cmd == "SAVEEEPROM": handleSaveEeprom()
      → if cmd.startsWith("SETALERT:"): handleSetAlert(value)
      → if cmd.startsWith("SETWARN:"): handleSetWarn(value)
      → if cmd.startsWith("SETDANGER:"): handleSetDanger(value)
      → if cmd.startsWith("APN:"): handleSetAPN(value)
      → if cmd.startsWith("SERVER:"): handleSetServer(value)
      → // ... obstruction light commands
      → else: Serial.println("UNKNOWN_COMMAND")

IN LOOP:
  if (Serial.available()) {
    String cmd = Serial.readStringUntil('\n');
    processCommand(cmd);
  }

IMPLEMENT IN ORDER:
  First: PING, BATT, SENSORTEST (quick wins, immediately useful)
  Then: FLOODSTATUS, GETCONFIG, GETTHRESH
  Then: SIMSTATUS, NETTEST
  Then: maintenance commands (SIMREINIT, SIMRESET, REBOOT)
  Then: config commands (SETALERT, etc.)
  Last: DUMP, obstruction light commands

□ DONE WHEN: Each command produces the correct output.
  Test every command at least once.
```

---

### Step 9.2: CSV Output

```
WHAT: Output 39-field CSV at 1Hz on serial.

WRITE:
  70. outputCSV()
      → Build comma-separated string of all 39 fields
        (per Section 32 of documentation)
      → Serial.println(csvLine)
      → Also: c3FeedByte() each character on GPIO 14

THIS RUNS EVERY 1 SECOND regardless of algorithm state.

TEST:
  Open serial monitor at 115200 baud
  See CSV lines flowing at 1Hz
  Count fields — should be exactly 39

□ DONE WHEN: 39-field CSV outputs every second.
  Fields match Section 32 specification.
```

---

## PHASE 10 — Web Console

### Step 10.1: Web Serial Connection

```
WHAT: Browser-based terminal that connects to the device via USB.

CREATE: public/console.html (served by your Node.js server,
        or a standalone file opened in Chrome)

TECHNOLOGY: Web Serial API (Chrome only — that's fine)

WRITE JAVASCRIPT:

  let port, reader, writer;
  
  async function connect() {
    port = await navigator.serial.requestPort();
    await port.open({ baudRate: 115200 });
    
    reader = port.readable.getReader();
    writer = port.writable.getWriter();
    
    // Read loop
    while (true) {
      const { value, done } = await reader.read();
      if (done) break;
      const text = new TextDecoder().decode(value);
      appendToTerminal(text);
      parseCSVLine(text);  // Extract and display data
    }
  }
  
  async function sendCommand(cmd) {
    const encoded = new TextEncoder().encode(cmd + '\n');
    await writer.write(encoded);
  }

HTML STRUCTURE:
  - Connect/Disconnect buttons
  - Terminal output area (scrolling, monospaced font)
  - Command input field
  - Quick-access buttons for common commands:
    [PING] [BATT] [FLOODSTATUS] [SENSORTEST]
  - Live data display panel (parsed from CSV):
    Water Level: ___ cm
    Mode: ___
    Zone: ___
    Response: ___
    Battery: ___%

TEST:
  Open in Chrome
  Click Connect → select ESP32 serial port
  CSV data should flow into terminal
  Type PING → see PONG response
  Click BATT button → see battery status

□ DONE WHEN: Can connect, see live data, send commands,
  and see responses in the browser.
```

---

### Step 10.2: Local Commands

```
WHAT: Browser-only commands that never reach the device.

IMPLEMENT IN JAVASCRIPT:

  function processLocalCommand(cmd) {
    switch(cmd.toLowerCase()) {
      case 'help':     showHelp(); return true;
      case 'clear':    clearTerminal(); return true;
      case 'history':  showHistory(); return true;
      case 'connect':  connect(); return true;
      case 'disconnect': disconnect(); return true;
      case 'status':   showConnectionStatus(); return true;
      case 'about':    showAbout(); return true;
      default:
        if (cmd.startsWith('filter ')) {
          setFilter(cmd.split(' ')[1]); return true;
        }
        if (cmd === 'upload') {
          openFilePicker(); return true;
        }
        return false;  // Not a local command → send to device
    }
  }

  // In command handler:
  function handleCommand(cmd) {
    if (processLocalCommand(cmd)) return;
    sendCommand(cmd);  // Forward to device
  }

□ DONE WHEN: help, clear, history, connect, disconnect,
  status, filter, about all work without touching the device.
```

---

## PHASE 11 — XIAO C3 Companion

### Step 11.1: C3 CSV Receiver

```
WHAT: C3 receives CSV from S3 on GPIO and forwards to Firebase.

SEPARATE FIRMWARE — new Arduino project for XIAO C3.

WIRE:
  S3 GPIO 14 (TX) → C3 RX pin (e.g., GPIO 20)
  S3 GND → C3 GND

C3 FIRMWARE:
  - Serial input at 9600 baud on receiving pin
  - Parse incoming CSV lines
  - Extract key fields (height, mode, zone, level, battery)
  - Forward to Firebase RTDB via C3's own SIM800L

C3 has its own SIM800L wired to its own UART pins.

START SIMPLE:
  1. Just receive and print CSV lines (echo test)
  2. Then parse fields
  3. Then add SIM800L + GPRS
  4. Then send to Firebase

TEST:
  Upload C3 firmware
  Connect S3 GPIO 14 → C3 RX
  C3 serial monitor should show CSV lines from S3

□ DONE WHEN: C3 receives and parses CSV from S3.
```

---

### Step 11.2: C3 Firebase Integration

```
WHAT: C3 sends parsed data to Firebase Realtime Database.

FIREBASE SETUP:
  1. Create Firebase project at console.firebase.google.com
  2. Create Realtime Database
  3. Set rules to allow authenticated writes
  4. Get database URL

C3 FIRMWARE:
  Use SIM800L GPRS to PATCH Firebase RTDB:
  
  AT+HTTPPARA="URL","https://your-project.firebaseio.com/
    varuna/STATION_ID/latest.json?auth=YOUR_SECRET"
  
  Payload: {"h":150.3,"m":1,"z":1,"l":1,"b":85,"t":1706000000}
  Method: PATCH (updates specific fields without overwriting)

TEST:
  Open Firebase console → Realtime Database
  See data updating in real-time as C3 sends

□ DONE WHEN: Firebase shows live sensor data from C3.
```

---

### Step 11.3: C3 OTA Agent

```
WHAT: C3 can reprogram the S3 over-the-air.

THIS IS THE MOST COMPLEX PART. Implement LAST.

STEPS:
  1. C3 monitors Firebase ota/command node
  2. On CHECK_READY: C3 checks conditions, reports readiness
  3. On BEGIN_DOWNLOAD: C3 downloads firmware chunks
  4. After checksum verified: C3 enters S3 into bootloader:
     - Pull S3 GPIO 0 LOW
     - Toggle S3 EN (reset)
     - S3 enters ROM bootloader
  5. C3 sends firmware via UART (ESP bootloader protocol)
  6. C3 resets S3 (toggle EN again)
  7. C3 monitors CSV feed for boot confirmation

THE ESP32 BOOTLOADER PROTOCOL:
  SLIP framing, specific command packets for:
  - SYNC
  - Read register
  - Write register (flash)
  - Flash begin
  - Flash data
  - Flash end
  
  Reference: esptool.py source code (Python)
  You'll need to reimplement the essential parts in C++
  
  This is significant work. Consider:
  - Option A: Implement a simplified version based on esptool protocol
  - Option B: Use the ESP32 OTA library directly on the S3 side
    (but this requires the S3 to download its own firmware)
  - Option C: Skip OTA for initial deployment — use physical USB

FOR NOW:
  Wire GPIO 0 and EN connections
  Test that you CAN put S3 into bootloader mode from C3
  Flash with a test binary
  The full 3-gate process can be refined later

□ DONE WHEN: C3 can put S3 into bootloader mode and flash
  a test firmware. CSV feed resumes after flash.
```

---

## PHASE 12 — Integration Testing

### Step 12.1: Bench Integration Test

```
WHAT: All systems running together on the bench.

SETUP:
  All sensors connected
  Both SIM cards active
  Server running
  Web console open
  C3 connected

RUN THROUGH THESE SCENARIOS:

  1. NORMAL OPERATION
     □ CSV output flowing at 1Hz
     □ C3 receiving and forwarding to Firebase
     □ GPRS POST succeeding to server
     □ Server receiving data
     □ Dashboard showing live data
     □ Health score > 80
     □ All console commands responding

  2. SIMULATED FLOOD (tilt sensor through thresholds)
     □ Tilt past alert → zone changes to ALERT
     □ Continue tilting → WARNING → DANGER
     □ Response level escalates (WATCH → WARNING → FLOOD → CRITICAL)
     □ Server detects transitions
     □ Server sends SMS alerts (to your test phone)
     □ Adaptive intervals shorten
     □ Status LED blinks faster

  3. SIMULATED RECESSION (return sensor to flat)
     □ Step-down kicks in (4 consecutive readings)
     □ Response level drops one at a time
     □ Server sends de-escalation messages
     □ Eventually: ALL CLEAR
     □ Intervals return to normal

  4. SENSOR FAILURE
     □ Disconnect MPU6050 → health score drops
     □ System reports sensor offline
     □ BMP280 still works for mode 3
     □ Reconnect → recovery detected

  5. COMMUNICATION FAILURE
     □ Remove SIM card antenna → signal lost
     □ GPRS fails → SPIFFS archival starts
     □ Readings buffered locally
     □ Reconnect antenna → GPRS recovers → buffered data sent

  6. POWER CYCLE
     □ Let system reach WARNING level
     □ Power off for 10 seconds
     □ Power on → warm boot restores WARNING level
     □ No false NORMAL → server doesn't get false all-clear

  7. SMS DIAGNOSTIC
     □ Send PING via SMS from your phone → get PONG reply
     □ Send BATT → get battery status
     □ Send FLOODSTATUS → get flood state

□ DONE WHEN: All 7 scenarios pass.
  This is your pre-deployment validation.
```

---

### Step 12.2: Duration Test

```
WHAT: Run continuously for 48+ hours.

LEAVE RUNNING:
  All systems active
  Server collecting data
  Monitor for:
    □ Memory leaks (free heap should stay stable)
    □ Watchdog resets (should be zero)
    □ GPRS connection drops and recoveries
    □ GPS time sync (once per 24 hours)
    □ EEPROM save cycles
    □ Battery drain rate

CHECK AFTER 48 HOURS:
  □ Free heap within 10% of startup value
  □ Zero watchdog resets
  □ All GPRS uploads successful (or recovered)
  □ GPS sync occurred at least once
  □ EEPROM saves throttled correctly (not every loop)
  □ Battery percentage trend matches expected drain

□ DONE WHEN: 48 hours stable operation with no crashes,
  no memory leaks, no data loss.
```

---

## PHASE 13 — Enclosure & Waterproofing

### Step 13.1: Mechanical Design

```
WHAT: Sealed, buoyant capsule for river deployment.

REQUIREMENTS:
  □ Waterproof (IP67 minimum, IP68 preferred)
  □ Buoyant (floats with all electronics + batteries inside)
  □ Bottom-heavy (self-righting — batteries at bottom)
  □ Tether attachment point at very bottom
  □ GPS antenna must have sky view (top of capsule)
  □ GSM antenna must be above waterline (or waterproof external)
  □ LED obstruction lights visible from all sides
  □ USB access port (sealed, for maintenance)
  □ Pressure port for BMP280 (sealed but pressure-conducting)

OPTIONS:
  A. PVC pipe with end caps (cheapest, easiest)
     - 4" or 6" PVC pipe
     - Glued end caps with O-ring seals
     - Epoxy cable glands for antenna/tether
  
  B. Pelican-style case (more professional)
     - Pre-made waterproof cases
     - More expensive but proven sealing
  
  C. Custom 3D-printed with silicone gaskets
     - Most design freedom
     - Requires careful sealing design

FOR FIRST PROTOTYPE: Use PVC pipe.
  4" PVC pipe × 12" long
  Bottom cap: permanent (glued)
  Top cap: removable with O-ring for maintenance
  Cable glands: waterproof PG7 or PG9

LAYOUT INSIDE:
  TOP:    GPS antenna, GSM antenna (cable through sealed gland)
  MIDDLE: ESP32-S3, XIAO C3, SIM800L, sensors
  BOTTOM: 8× 18650 batteries (ballast + power)
  
  Tether attaches to bottom cap through sealed eye-bolt
```

---

### Step 13.2: BMP280 Pressure Port

```
WHAT: Allow BMP280 to sense external pressure while keeping
water out of the electronics compartment.

OPTIONS:
  A. Silicone membrane (water blocks, pressure passes)
  B. Sealed air tube running from sensor to above waterline
  C. Waterproof pressure sensor housing (most reliable)

SIMPLEST: Option A
  Cover BMP280 port hole with medical-grade silicone membrane
  Thin enough to transmit pressure
  Thick enough to block water

FOR PROTOTYPE: Mount BMP280 near the top of the capsule.
When submerged, it will still sense pressure through the
capsule walls (PVC transmits pressure somewhat).
Not perfectly accurate but functional for prototype.
```

---

### Step 13.3: Tether System

```
WHAT: Fixed-length cable connecting buoy to anchor.

MATERIALS:
  □ Braided stainless steel cable (2-3mm diameter)
  □ Crimped end fittings (swage sleeves)
  □ Bottom: heavy weight anchor (concrete block, steel plate,
    or earth anchor — depends on riverbed type)
  □ Top: sealed eye-bolt through bottom of PVC capsule

MEASURE PRECISELY:
  L = desired flood threshold height in centimeters
  Measure under ~5N tension (slight pull, not stretched)
  Record to nearest centimeter

STORE IN FIRMWARE:
  Use handheld debugger command: OLP:<length_in_cm>
  e.g., OLP:200 for 2-meter tether
```

---

## PHASE 14 — Field Calibration & Deployment

### Step 14.1: Pre-Deployment Checks

```
□ All bench integration tests passed (Phase 12)
□ 48-hour duration test passed
□ Enclosure sealed and buoyancy tested (float in bathtub/pool)
□ Tether measured and recorded
□ Batteries fully charged
□ Both SIM cards active with data plans
□ Server running and accessible
□ Contacts configured in server database
□ Firmware version recorded
```

---

### Step 14.2: Field Deployment Procedure

```
AT THE SITE:

  1. INSTALL ANCHOR
     □ Select location: stable riverbed, accessible
     □ Install anchor (concrete/steel weight or driven stake)
     □ Ensure anchor cannot move in high current
     □ Record GPS coordinates of anchor point

  2. POWER ON DEVICE
     □ Connect batteries
     □ Wait for boot (8-15 seconds)
     □ Verify via handheld debugger:
       - LED solid ON
       - Serial output flowing
       - GPS acquiring fix

  3. CALIBRATE (via handheld debugger)
     □ Place buoy in water, floating freely
     □ Issue CAL command
     □ Wait for calibration (LED blinks during process)
     □ Verify: theta ≈ 0° when floating freely

  4. SET PARAMETERS (via handheld debugger)
     □ OLP:<tether_length_cm>
     □ SETORIGIN (capture GPS position)
     □ Verify: GETCONFIG shows correct values
     □ Set thresholds if different from defaults:
       SETALERT:<cm>  SETWARN:<cm>  SETDANGER:<cm>

  5. ATTACH TETHER
     □ Connect tether to buoy bottom
     □ Connect tether to anchor
     □ Deploy buoy into water

  6. ENABLE ALGORITHM
     □ Press algorithm toggle button (GPIO 12)
     □ Algorithm LED (GPIO 13) turns OFF = algorithm ENABLED
     □ Verify: FLOODSTATUS shows adaptive intervals active

  7. VERIFY READINGS
     □ Read water level manually (staff gauge or tape measure)
     □ Compare to system reading (FLOODSTATUS command)
     □ Error < 5cm: PASS ✅
     □ Error 5-15cm: adjust, recalibrate
     □ Error > 15cm: investigate

  8. VERIFY COMMUNICATION
     □ NETTEST → HTTP POST succeeds to server
     □ Check server dashboard → data appearing
     □ SIMSTATUS → registered, signal adequate
     □ Send test SMS from your phone → device replies

  9. VERIFY LIGHTS
     □ OBLIGHTON → LEDs flashing
     □ Correct pattern (IALA standard)
     □ Visible from water

  10. FINAL CHECKS
      □ BATT → battery level > 95%
      □ GPSFIX → fix valid, satellites > 4
      □ Health score > 85
      □ Close enclosure, seal

  11. DOCUMENT
      □ Record: date, time, GPS, water level, firmware version
      □ Photograph: buoy in water, staff gauge, anchor location
      □ Record: all calibration values, offsets, thresholds

  12. MONITOR REMOTELY
      □ Watch server dashboard for 1 hour after leaving
      □ Confirm regular data uploads arriving
      □ Confirm GPRS heartbeat is stable
```

# VARUNA FLOOD MONITORING SYSTEM — COMPLETE TECHNICAL DOCUMENTATION

---

## Part 1: System Architecture, Sensors, and Core Algorithms

---

## 1. System Architecture Overview

This is a **multi-sensor, multi-communication flood monitoring station** built on an **ESP32-S3**, deployed as a sealed buoy capsule tethered to a fixed riverbed anchor. It measures water level through **tether-angle geometry** (pendulum principle) with pressure-based depth measurement for submerged conditions, and transmits data to a cloud server via GPRS for server-side alert dispatch.

> **Design rationale:** Having the device manage its own alert dispatch created a hard dependency between the hardware in the field and the contact list it was trying to reach. Updating who gets notified required either physical access to the device or sending configuration commands via SMS — both impractical in a real deployment with dozens of stations across a river basin. Moving this responsibility to the server means the contact list, escalation rules, notification channels, and message formatting can all be changed through a web interface at any time, independently of the hardware. A Tahsildar gets transferred — update one field in the database. No firmware flash, no SMS command, no field visit.

```
┌─────────────────────────────────────────────────────────┐
│                    ESP32-S3 (Main MCU)                   │
│              Mounted inside sealed buoy capsule          │
│                                                         │
│  I2C Bus 0 (GPIO 8/9)    I2C Bus 1 (GPIO 4/5)         │
│  ├── MPU6050 (tilt)       ├── DS1307 (RTC)             │
│  │   (tether angle θ)     └── BMP280 (pressure/depth)  │
│  │                                                      │
│  UART1 (GPIO 6/7)  ──── GPS Module (9600 baud)        │
│  UART2 (GPIO 15/16) ──── SIM800L GSM (9600 baud)      │
│                                                         │
│  ┌─── XIAO C3 Interface ──────────────────────────┐    │
│  │  GPIO 14 (SW-UART TX) ──→ C3 RX: CSV feed      │    │
│  │  GPIO 43 (UART TX)    ──→ C3 RX: OTA data line │    │
│  │  GPIO 44 (UART RX)    ←── C3 TX: OTA responses │    │
│  │  GPIO 0  (BOOT/IO0)   ←── C3: bootloader ctrl  │    │
│  │  EN pin  (RESET)       ←── C3: reset control    │    │
│  └─────────────────────────────────────────────────┘    │
│                                                         │
│  GPIO 2 ──────── Battery ADC (voltage divider)          │
│  GPIO 12 ─────── Algorithm Toggle Button                │
│  GPIO 13 ─────── Algorithm Status LED                   │
│  GPIO 3 ──────── Status LED                             │
│  GPIO 17 ─────── SIM800L Reset                          │
│  5× Red LEDs ─── Marine Obstruction Lights              │
└─────────────────────────────────────────────────────────┘

Physical deployment:
  ┌──────────────────┐
  │   BUOY CAPSULE   │  ← Sealed, waterproof, bottom-heavy
  │   (floats or     │     Contains all electronics
  │    submerges)     │     Tether attaches at bottom
  └────────┬─────────┘
           │
     TETHER (length L = OLP length)
     Braided steel cable, precisely measured
           │
  ═════════●══════════════  RIVERBED
        ANCHOR
     (fixed, weighted)
```

---

## 2. The Core Measurement Principle — Buoy-Tether Pendulum (OLP)




# Water Level Detection Mechanism — Complete Explanation

---

## THE PHYSICAL SETUP

```
SIDE VIEW OF DEPLOYED SYSTEM:

Water Surface ~~~~~~~~~~●~~~~~~~~~~~~~
                       /|
                      / |
                     /  |
                L   /   |  H (what we want to measure)
  (fixed wire)     /    |
                  / θ   |
                 /      |
                /       |
Riverbed ─────●─────────+──────────────
            ANCHOR    (projection)
```

Three physical components:

```
1. ANCHOR
   • Fixed point bolted/weighted to the riverbed
   • Position never changes
   • This is the origin of our measurement

2. TETHER (OLP — Output Linkage Pendulum)
   • Fixed length wire connecting anchor to buoy
   • Length L is PRECISELY KNOWN (measured at installation)
   • L is chosen to equal the FLOOD THRESHOLD water level
   • Made of stiff braided cable or wire (minimal stretch)
   • Does NOT change length over time

3. BUOY (Capsule)
   • Floats at the water surface (density < water)
   • Contains MPU6050 (gyro + accelerometer)
   • Contains pressure sensor
   • Contains microcontroller + radio
   • Bottom-heavy design (ballast low, electronics low)
   • Tether attaches at the VERY BOTTOM of the capsule
```

---

## THE FUNDAMENTAL GEOMETRY

The buoy MUST float at the water surface. The tether CONSTRAINS the buoy to move along a circular arc of radius L centered at the anchor.

These two facts create a **right triangle**:

```
                    ● Buoy (at water surface)
                   /|
                  / |
                 /  |
            L   /   |  H = water level above riverbed
  (hypotenuse)/    |  (vertical leg)
              / θ   |
             /      |
            /       |
  Anchor  ●─────────+
              d
        (horizontal leg)
```

Three sides:
```
HYPOTENUSE:   L = tether length (KNOWN CONSTANT)
VERTICAL LEG: H = water level height (WHAT WE WANT)
HORIZONTAL:   d = horizontal displacement (don't care)
```

One angle:
```
θ = angle between tether and vertical
    (MEASURED by MPU6050)
```

**The equation:**

```
┌───────────────────────────────────┐
│                                   │
│        H = L × cos(θ)            │
│                                   │
│   L is known.                     │
│   θ is measured.                  │
│   Therefore H is determined.      │
│                                   │
└───────────────────────────────────┘
```

This is it. This is the entire core measurement.

---

## WHY THIS WORKS REGARDLESS OF CURRENT

This is the most important property. Watch what happens at **constant water level** with **different current speeds**:

```
NO CURRENT:              MODERATE CURRENT:         STRONG CURRENT:

      ● buoy                  ● buoy                      ● buoy
      |                      /                            /
      | θ=0°                / θ=30°                      / θ=60°
      |                    /                             /
  L   |   H=L            / L    H                      / L       H
      |                  /                              /
      |                 /                              /
      |                /                              /
──────●────       ────●──────d──             ────────●──────d────────

H = L×cos(0°)    H = L×cos(30°)             H = L×cos(60°)
H = L             H = 0.866L                 H = 0.5L

WAIT — these give DIFFERENT H values!
But the water level is THE SAME in all three cases!
```

**No.** Read again. If the water level is the same in all three cases, then H is the same, and therefore θ is the same. The current pushes the buoy **horizontally**, but:

```
The buoy ALWAYS floats at the surface → its height IS the water level
The buoy is constrained to the arc → H² + d² = L²
Therefore → cos(θ) = H/L REGARDLESS of d

Different current just changes d (horizontal position)
Same H → same θ → same measurement
The three diagrams above are WRONG — they show different H values
which would mean different water levels, not different currents
```

Let me show it correctly:

```
SAME water level H, different currents:

Calm:           Moderate:           Fast:
● (at surface)  ● (at surface)      ● (at surface)
|               /                    /
| θ₁           / θ₁                / θ₁
|              /                   /   ← ALL SAME ANGLE
|             /                   /       because ALL at same height H
●            ●                   ●

cos(θ₁) = H/L in ALL cases ✓
Current only changes WHERE on the surface the buoy sits
NOT how high the surface is
```

**The only thing that changes H is the actual water level rising or falling.**

---

## WHAT THE MPU6050 ACTUALLY MEASURES

The MPU6050 sits inside the buoy. It has two sensor types:

### Accelerometer (Gives Tilt from Gravity)

```
BUOY TILTED AT ANGLE θ:

    Gravity direction: straight down (always)
    Buoy's local Z-axis: along the capsule's long axis
    
    In buoy's reference frame:
    
        az = g × cos(θ)    ← component along capsule axis
        ax = g × sin(θ)    ← component perpendicular to axis
        
    Therefore:
        θ = atan2(ax, az)
```

**Problem:** On a buoy in a river, the accelerometer also picks up wave bobbing, vibration, turbulence. These corrupt the reading.

### Gyroscope (Gives Angular Velocity)

```
    ω = dθ/dt    (how fast the buoy is rotating)
    
    Integrate to get angle:
        θ(t) = θ₀ + ∫ω dt
        
    Problem: integration drift (20-80°/hour)
    After 1 hour the accumulated error could be 80°
```

### Sensor Fusion (Combining Both)

Neither sensor alone works. Together they do:

```
COMPLEMENTARY FILTER:

    θ_fused = 0.97 × (θ_prev + ω × dt) + 0.03 × atan2(ax, az)
              ─────────────────────────   ─────────────────────
              Gyro: accurate short-term   Accel: accurate long-term
              (no vibration noise)         (no drift)
              (but drifts over time)       (but noisy in waves)

    Result: ±0.5-2° accuracy
    This is what we use for θ in H = L × cos(θ)
```

Or better: use the MPU6050's **built-in DMP** which does this fusion in hardware and outputs clean angle values directly.

---

## THE FOUR OPERATING MODES

Water level can be in four regimes relative to tether length L. Each requires different detection logic:

### MODE 0: SLACK TETHER (H significantly below L)

```
Water Surface ~~~●~~~
                 |
                 | buoy floats freely
                 |
                 ⌢  tether hangs loose
                 |    (distance to anchor < L)
                 |
Riverbed ────────●──── Anchor
```

**What happens physically:**
- Water level H is much less than tether length L
- The buoy floats directly above (or near) the anchor
- The tether is NOT pulled taut — it droops with slack
- The buoy is free to orient however buoyancy dictates
- Buoyancy makes it perfectly upright (θ_buoy ≈ 0°)

**What sensors read:**
```
Accelerometer:  ax ≈ 0, ay ≈ 0, az ≈ g
                (pure gravity, no lateral component)
                lateral_accel = sqrt(ax² + ay²) ≈ 0

Gyroscope:      ω ≈ 0 (buoy sitting still)

Pressure:       P ≈ P_atmospheric
                (buoy at surface, sensor near waterline)
```

**How we detect this mode:**
```
IF lateral_accel < 0.15 m/s²
AND tilt < 3°
AND pressure ≈ P_atmospheric
THEN → MODE 0: SLACK
```

**What we report:**
```
"Water level is BELOW flood threshold"
"System is in SAFE condition"
H < L but exact value unknown
Alert level: GREEN (normal)
```

**Why we DON'T need exact H here:**
Because L was chosen as the flood threshold. If H < L, the situation is safe. We don't need to know if H = 0.3L or H = 0.6L — both are safe. The system's PURPOSE is flood warning, not precise continuous measurement of low water levels.

---

### MODE 1: TAUT TETHER (H approaching L)

```
Water Surface ~~~~~~●~~~~~~~~~
                   /|
                  / |
                 /  |
            L   / θ | H (approaching L)
               /    |
              / θ   |
             /      |
Riverbed ───●───────+────
          Anchor
```

**What happens physically:**
- Water has risen enough (or current is strong enough) to pull tether taut
- Buoy is constrained to the circular arc
- Tether is straight and under tension
- The right triangle geometry applies perfectly
- θ is well-defined and measurable

**Transition from MODE 0 to MODE 1:**
As water rises, at some point the buoy's distance from anchor equals L. The tether goes taut. This can also happen when current pushes the buoy far enough horizontally that the tether stretches to full length.

**What sensors read:**
```
Accelerometer:  ax ≠ 0 (lateral component from tether tension)
                lateral_accel = sqrt(ax² + ay²) > 0.15 m/s²
                
                The tether pulls on the bottom of the buoy.
                This pull has a horizontal component that shows
                up as persistent lateral acceleration.

Gyroscope:      θ is measurable and changing with water level
                ω shows slow changes as water rises/falls

Pressure:       P ≈ P_atmospheric (still floating at surface)
```

**How we detect this mode:**
```
IF lateral_accel > 0.15 m/s²
AND tilt > 3°
AND pressure ≈ P_atmospheric
THEN → MODE 1: TAUT
```

**What we compute:**
```
θ = fused angle from DMP or complementary filter
H = L × cos(θ)

Example with L = 2.0m:
  θ = 60° → H = 2.0 × cos(60°) = 1.00m
  θ = 45° → H = 2.0 × cos(45°) = 1.41m
  θ = 30° → H = 2.0 × cos(30°) = 1.73m
  θ = 15° → H = 2.0 × cos(15°) = 1.93m
  θ = 5°  → H = 2.0 × cos(5°)  = 1.99m
  θ = 0°  → H = 2.0 × cos(0°)  = 2.00m = L (FLOOD!)
```

**What we report:**
```
"Water level: H = [value] meters"
"Water level is at [H/L × 100]% of flood threshold"
Alert level: YELLOW (elevated) when H/L > 0.7
```

---

### MODE 2: FLOOD REACHED (H ≈ L)

```
Water Surface ~~~~~~●~~~~~~~~~~~~~
                    |
                    | θ ≈ 0° (nearly vertical)
                    |
                L   |  H ≈ L
                    |
                    |
                    |
Riverbed ───────────●────
                  Anchor
```

**What happens physically:**
- Water level has reached (or nearly reached) the tether length
- Tether is nearly vertical
- Buoy is almost directly above anchor
- θ approaches 0°
- cos(θ) approaches 1.0
- H approaches L

**What sensors read:**
```
Accelerometer:  Tilt is very small (θ → 0°)
                lateral_accel is decreasing
                Still detectable as taut (tension is vertical)

Gyroscope:      θ ≈ 0-5° and may be slowly changing

Pressure:       P ≈ P_atmospheric (still at surface)
                OR beginning to show slight submersion
```

**How we detect this mode:**
```
IF tether is TAUT (from MODE 1 detection)
AND θ < 10° (nearly vertical)
AND H/L > 0.95 (computed water level within 5% of L)
THEN → MODE 2: FLOOD
```

**Sensitivity concern at this mode:**

```
dH/dθ = -L × sin(θ)

At θ = 2°:  dH/dθ = -L × sin(2°) = -0.035L per degree
            1° error → only 3.5cm error (for L=1m)
            
At θ = 0°:  dH/dθ = 0
            cos(θ) is FLAT at the top
            Cannot distinguish θ=0° from θ=1° easily
```

At this point the water level reading becomes imprecise because cosine is flat near 0°. But it does not matter — we KNOW the water is at flood level. The pressure sensor takes over for any further rise.

**What we report:**
```
"⚠️ FLOOD LEVEL REACHED"
"Water level: H ≈ L = [value] meters"
Alert level: RED (flood)
```

---

### MODE 3: SUBMERGED (H > L)

```
Water Surface ~~~~~~~~~~~~~~~~~~~~~~~~~~~~
                    ↑ still rising
               ─────┼──── depth below surface
                    |
                    ● Buoy (PULLED UNDERWATER)
                    |
                    | tether (fully vertical, taut)
                L   |
                    |
                    |
Riverbed ───────────●────
                  Anchor
```

**What happens physically:**
- Water has risen ABOVE the tether length
- The buoy CANNOT reach the surface — tether holds it down
- The buoy is completely submerged
- Depth below surface = H - L
- Water pressure increases with depth

**What sensors read:**
```
Accelerometer:  θ ≈ 0° (tether vertical)
                May show turbulence from being underwater in current

Gyroscope:      θ ≈ 0°, little change

Pressure:       P = P_atmospheric + ρ × g × (H - L)
                ─────────────────────────────────────
                THIS IS THE KEY MEASUREMENT NOW
                
                P_gauge = P_measured - P_atmospheric
                depth = P_gauge / (ρ × g)
                H = L + depth
```

**How we detect this mode:**
```
IF pressure > P_atmospheric + SUBMERSION_THRESHOLD
   (threshold ≈ 500 Pa, equivalent to ~5cm of water)
THEN → MODE 3: SUBMERGED
```

**What we compute:**
```
depth_below_surface = (P_measured - P_atm) / (ρ_water × g)

H_total = L + depth_below_surface

Example with L = 2.0m:
  P_gauge = 980 Pa  → depth = 0.10m → H = 2.10m
  P_gauge = 4900 Pa → depth = 0.50m → H = 2.50m
  P_gauge = 9800 Pa → depth = 1.00m → H = 3.00m
```

**What we report:**
```
"🚨 CRITICAL: WATER ABOVE FLOOD LEVEL"
"Water level: H = [L + depth] meters"
"Buoy submerged [depth] meters below surface"
Alert level: BLACK (critical emergency)
```

---

## COMPLETE MODE TRANSITION DIAGRAM

```
                    WATER LEVEL RISING →
                    
  0m          H₁           H₂            L          H_max
  ├───────────┼────────────┼─────────────┼───────────┤
  
  ◄── MODE 0 ─►◄──── MODE 1 ────────────►◄─ MODE 2 ─►◄ MODE 3 ►
     SLACK          TAUT                    FLOOD      SUBMERGED
     GREEN          YELLOW                  RED        BLACK
     
     H unknown      H = L×cos(θ)           H ≈ L      H = L + ΔP/ρg
     H < L          H measured precisely    Transition Pressure-based
     SAFE           MONITORING              ALERT!     EMERGENCY!
     
  Sensor:         Sensor:                 Sensor:    Sensor:
  lateral_a≈0     lateral_a>0.15          θ→0°       P >> P_atm
  P ≈ P_atm       θ gives H              P rising   depth = ΔP/ρg


  TRANSITION TRIGGERS:
  
  MODE 0 → 1:  lateral_accel crosses above 0.15 m/s²
               AND tilt exceeds 3°
               (tether just went taut)
               
  MODE 1 → 2:  θ drops below 10°
               AND computed H > 0.95 × L
               (water approaching flood level)
               
  MODE 2 → 3:  pressure exceeds P_atm + 500 Pa
               (buoy going underwater)
               
  MODE 3 → 2:  pressure drops below P_atm + 500 Pa
               (water receding, buoy surfacing)
               
  MODE 2 → 1:  θ rises above 10°
               (water dropping from flood level)
               
  MODE 1 → 0:  lateral_accel drops below 0.10 m/s²
               (tether going slack — note hysteresis:
                0.15 to trigger, 0.10 to release)
```

---

## ACCURACY AT EACH MODE

```
MODE    METHOD              ACCURACY        NOTES
────    ──────              ────────        ─────
  0     None needed         N/A             H < L = safe. Done.
  
  1     H = L×cos(θ)        ±2-8cm          Depends on θ range:
        θ from MPU6050                      θ=60°: ±2cm per degree
        fused angle                         θ=30°: ±5cm per degree
                                            θ=10°: ±7cm per degree
                                            
  2     H ≈ L               ±5-10cm         cos(θ) insensitive near θ=0
        Transition zone                     Pressure begins to help
        
  3     H = L + ΔP/(ρg)     ±1-2cm          Pressure sensors are very
        Pressure-based                      accurate for depth measurement
```

---

## NUMERICAL WALKTHROUGH

Real example with **L = 2.0 meters** (flood threshold at 2m above riverbed):

```
SCENARIO 1: Normal day, water at 0.8m
━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
  H = 0.8m, L = 2.0m
  Distance from anchor to buoy = 0.8m < L = 2.0m
  Tether: SLACK
  
  Sensors: lateral_accel ≈ 0, pressure ≈ atmospheric
  Mode: 0 (GREEN)
  Report: "Water level below flood threshold. Safe."


SCENARIO 2: Heavy rain, water rising to 1.4m
━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
  H = 1.4m, L = 2.0m
  θ = arccos(1.4/2.0) = arccos(0.7) = 45.6°
  Tether: TAUT (current or geometry pulls it tight)
  
  Sensors: lateral_accel = 0.4 m/s², tilt = 45.6°
  Mode: 1 (YELLOW)
  Computed: H = 2.0 × cos(45.6°) = 1.40m
  Report: "Water level: 1.40m (70% of flood threshold)"


SCENARIO 3: Storm, water at 1.9m
━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
  H = 1.9m, L = 2.0m
  θ = arccos(1.9/2.0) = arccos(0.95) = 18.2°
  Tether: TAUT, nearly vertical
  
  Sensors: tilt = 18.2°, pressure ≈ atmospheric
  Mode: 1 approaching MODE 2 (YELLOW → RED)
  Computed: H = 2.0 × cos(18.2°) = 1.90m
  Report: "⚠️ Water level: 1.90m (95% of flood threshold)"


SCENARIO 4: Flood, water at exactly 2.0m
━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
  H = 2.0m = L
  θ = arccos(2.0/2.0) = arccos(1.0) = 0°
  Tether: TAUT, perfectly vertical
  
  Sensors: tilt ≈ 0°, pressure beginning to show submersion
  Mode: 2 (RED)
  Computed: H = 2.0 × cos(0°) = 2.0m
  Report: "🚨 FLOOD! Water at flood threshold: 2.0m"


SCENARIO 5: Severe flood, water at 2.5m
━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
  H = 2.5m > L = 2.0m
  Buoy submerged 0.5m below surface
  θ ≈ 0° (tether vertical, pulled down)
  
  Sensors: pressure = P_atm + 1000×9.81×0.5 = P_atm + 4905 Pa
  Mode: 3 (BLACK)
  Computed: depth = 4905 / (1000 × 9.81) = 0.50m
            H = 2.0 + 0.5 = 2.50m
  Report: "🚨🚨 CRITICAL: Water 0.5m ABOVE flood level! H = 2.5m"
```

---

## THE COMPLETE DECISION FLOWCHART

```
        ┌──────────────────────────┐
        │  READ ALL SENSORS        │
        │  • ax, ay, az (accel)    │
        │  • gx, gy, gz (gyro)    │
        │  • P (pressure)         │
        │  • Compute θ (fusion)   │
        └────────────┬─────────────┘
                     │
                     ▼
        ┌──────────────────────────┐
        │  P > P_atm + 500 Pa ?   │
        └─────┬──────────┬─────────┘
              │YES       │NO
              ▼          ▼
     ┌────────────┐   ┌──────────────────────┐
     │  MODE 3    │   │                      │
     │  SUBMERGED │   │  lateral_accel       │
     │            │   │  > 0.15 m/s²         │
     │  depth =   │   │  AND tilt > 3° ?     │
     │  ΔP/(ρg)   │   └───┬──────────┬───────┘
     │            │       │YES       │NO
     │  H = L +   │       ▼          ▼
     │  depth     │  ┌─────────┐  ┌─────────┐
     │            │  │ TETHER  │  │ TETHER  │
     │  🚨 BLACK  │  │ IS TAUT │  │ IS SLACK│
     └────────────┘  └────┬────┘  └────┬────┘
                          │            │
                          ▼            ▼
                   ┌────────────┐  ┌────────────┐
                   │  θ < 10°   │  │  MODE 0    │
                   │  AND       │  │  SLACK     │
                   │  H > 0.95L │  │            │
                   │  ?         │  │  H < L     │
                   └──┬────┬───┘  │  (safe)    │
                      │YES │NO    │            │
                      ▼    ▼      │  🟢 GREEN  │
               ┌────────┐┌─────┐ └────────────┘
               │ MODE 2 ││MODE │
               │ FLOOD  ││ 1   │
               │        ││TAUT │
               │ H ≈ L  ││     │
               │        ││H =  │
               │🔴 RED  ││L×   │
               │        ││cosθ │
               └────────┘│     │
                         │🟡   │
                         │YELLW│
                         └─────┘
```

---

## WHAT MAKES THIS SYSTEM CLEVER

```
PROPERTY                              WHY IT MATTERS
━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━

1. Current-independent measurement     θ gives H regardless of
                                       how fast the river flows

2. Slack = Safe                        Don't need to solve the
                                       hardest measurement case
                                       because it is the safe case

3. Two measurement principles          Trig (H<L) and pressure (H>L)
   cover the full range                seamlessly hand off to each other

4. Natural flood threshold             L = flood level by design
                                       θ → 0 IS the flood alarm
                                       No calibration table needed

5. Self-validating                     Pressure confirms trig reading
                                       near the transition (H ≈ L)
                                       If they disagree → sensor fault

6. Graceful degradation                If MPU6050 fails → pressure
                                       still detects flood (MODE 3)
                                       If pressure fails → trig still
                                       works for H < L (MODE 1)

7. Single known constant               Only need to know L precisely
                                       No flow calibration needed
                                       for water level measurement
```

## 3. Sensor Fusion & Complementary Filter

```
                    MPU6050
              (inside floating buoy)
                   ╱       ╲
        Accelerometer    Gyroscope
        (gravity + tether (angular velocity
         tension vector)   of buoy rotation)
                   ╲       ╱
              Complementary Filter
              α = 0.98 (gyro-heavy)
                      │
              Filtered Tilt θ (X, Y)
                      │
              Combined angle from vertical
                      │
              H = L × cos(θ)
```

**Why 98% gyro weight on a buoy:**
```
Problem: Buoy is on water. Waves, current turbulence, and
         impacts create high-frequency accelerations that
         corrupt the accelerometer reading.

Gyroscope: Immune to linear acceleration. Reports only
           rotational velocity. Excellent for short-term
           angle tracking through wave noise.

Accelerometer: Reports true gravity direction over time
               (drift-free), but contaminated by every
               wave bump, current eddy, and collision.

Solution: Trust gyro 98% for smooth short-term tracking.
          Use accel 2% to correct long-term gyro drift.
          
Result: ±0.5-2° accuracy even in choppy water.
```

The complementary filter formula:
```
filtTiltX = α × (filtTiltX + gyroRate × dt) + (1-α) × accelAngle

Where:
  α = 0.98 (ALPHA constant)
  dt = time since last reading (seconds)
  gyroRate = (gyroX - gyroOffsetX) / 131.0 (degrees/second)
  accelAngle = atan2(ay, sqrt(ax² + az²)) × 180/π (degrees)
```

**Additional sensor: Lateral acceleration for tether detection**
```
When tether is SLACK: buoy floats freely, upright
  → lateral_accel = sqrt(ax² + ay²) ≈ 0

When tether is TAUT: cable pulls on buoy bottom
  → lateral_accel > 0.15 m/s² (persistent pull)
  → This distinguishes MODE 0 from MODE 1
```

**Calibration** (`recalibrate()`):
- Gyroscope offset measured at startup (average of 1000 samples while stationary)
- Accelerometer reference gravity direction recorded (500 samples, filtered for |g| ∈ [0.9, 1.1])
- Reference tilt angles computed:
  - `refTiltX = atan2(refAccY, sqrt(refAccX² + refAccZ²)) × 180/π`
  - `refTiltY = atan2(-refAccX, sqrt(refAccY² + refAccZ²)) × 180/π`
- These define the "zero angle" reference
- Should be performed with buoy floating freely (MODE 0) for best results
- Corrected tilt = filtered tilt - reference tilt
- Combined angle: `θ = sqrt(correctedTiltX² + correctedTiltY²)`

---

## 4. Flood Detection State Machine (System 1)

This is a **two-dimensional classification** followed by a **response matrix**:

### Dimension 1: Height Zones
```
Water Height (cm)
    │
    │  DANGER zone        ──── dangerLevelCm (250 cm default)
    │  ████████████████
    │  WARNING zone       ──── warningLevelCm (180 cm)
    │  ▓▓▓▓▓▓▓▓▓▓▓▓▓▓
    │  ALERT zone         ──── alertLevelCm (120 cm)
    │  ░░░░░░░░░░░░░░░
    │  NORMAL zone
    └──────────────────────── Time
```

### Dimension 2: Rate of Change Categories
```
Rate (cm/15min)    Category
< 0                RATE_FALLING
0 – 2.0            RATE_SLOW
2.0 – 5.0          RATE_MODERATE
> 5.0              RATE_FAST
> 30.0             RATE_EXTREME (special override)
> 50.0             RATE_CATASTROPHIC (special override)
```

### Response Matrix (Zone × Rate × Sustained → Response Level)

```
                    NOT Sustained                    Sustained Rise
                    ─────────────                    ──────────────
              SLOW      MODERATE    FAST       SLOW      MODERATE    FAST
            ┌──────────────────────────────┬──────────────────────────────┐
   NORMAL   │ NORMAL    NORMAL    NORMAL   │ NORMAL     WATCH     WATCH  │
            ├──────────────────────────────┼──────────────────────────────┤
   ALERT    │ WATCH      WATCH    WATCH    │ WATCH     WARNING   WARNING │
            ├──────────────────────────────┼──────────────────────────────┤
   WARNING  │ WARNING   WARNING    FLOOD   │ FLOOD      FLOOD   CRITICAL│
            ├──────────────────────────────┼──────────────────────────────┤
   DANGER   │ FLOOD    CRITICAL  CRITICAL  │ CRITICAL  CRITICAL CRITICAL│
            └──────────────────────────────┴──────────────────────────────┘
```

### State Transition Rules — Anti-Oscillation Logic

The system uses **hysteresis and time gates** to prevent false alerts:

```
                    ┌───────────┐
         ┌────────►│  CRITICAL  │◄── Instant on DANGER+FAST
         │         └──────┬────┘    or sustained extreme rate
         │                │
         │    Requires 4 consecutive    Min hold: 900s
         │    lower readings + time     
         │                │
         │         ┌──────▼────┐
         │    ┌───►│   FLOOD   │◄── DANGER zone or WARNING+FAST
         │    │    └──────┬────┘    Min hold: 1800s
         │    │           │
         │    │    4 consecutive lower readings
         │    │           │
         │    │    ┌──────▼────┐
         ├────┤───►│  WARNING  │◄── WARNING zone or ALERT+FAST
         │    │    └──────┬────┘    Min hold: 1800s
         │    │           │
         │    │    4 consecutive lower readings
         │    │           │
         │    │    ┌──────▼────┐
         │    └───►│   WATCH   │◄── ALERT zone or moderate rate
         │         └──────┬────┘    Min hold: 900s
         │                │
         │    8 consecutive normal readings
         │                │
         │         ┌──────▼────┐
         └─────────│  NORMAL   │    Default state
                   └───────────┘
```

**Step-down logic:**
```
STEPDOWN_READINGS_REQUIRED = 4   // Must see 4 consecutive lower-zone readings
STEPDOWN_NORMAL_READINGS   = 8   // 8 readings needed to return to NORMAL

Plus minimum time at each level:
MIN_TIME_CRITICAL  = 900   // 15 minutes minimum at CRITICAL
MIN_TIME_FLOOD     = 1800  // 30 minutes minimum at FLOOD
MIN_TIME_WARNING   = 1800  // 30 minutes
MIN_TIME_WATCH     = 900   // 15 minutes
```

Escalation is instant; de-escalation is gradual.

**AFTER any level transition:**
- New level stored for inclusion in next HTTP POST payload
- EEPROM state saved (`forceSaveEeprom`)
- Serial status printed
- The device does NOT dispatch any SMS or voice alert
- The server detects the level change from the next payload and handles all notification routing

**What the server sees at each level:**
```
WATCH:    Included in next HTTP POST → server notifies ops team
WARNING:  Included in next HTTP POST → server notifies local officials
FLOOD:    Included in next HTTP POST → server notifies district authorities
CRITICAL: Included in next HTTP POST → server notifies all + SEOC + IVR
```

### Sustained Rise Detection

A circular buffer of 4 readings tracks whether water is **consistently rising**:

```
float sustainedBuffer[4];        // Last 4 water height readings
uint32_t sustainedTimeBuffer[4]; // Timestamps

Algorithm:
  1. Add new reading to circular buffer
  2. If buffer not full (< 4 readings) → sustained = false
  3. Extract ordered sequence from circular buffer
  4. Check net rising: ordered[3] > ordered[0] + 0.5cm
  5. Count rising pairs:
     (h₁ > h₀ + 0.5) → +1
     (h₂ > h₁ + 0.5) → +1
     (h₃ > h₂ + 0.5) → +1
  6. sustainedRise = netRising AND (riseCount ≥ 2)
```

```
Example — Sustained Rise DETECTED:
  [100.0, 101.5, 103.0, 105.2]
  Net: 105.2 > 100.0 + 0.5 ✓
  Pairs rising: 3 of 3 ≥ 2 ✓
  → sustainedRise = TRUE

Example — NOT Sustained (oscillating):
  [100.0, 102.0, 99.5, 101.0]
  Net: 101.0 > 100.0 + 0.5 ✓
  Pairs rising: only 1 (102→99.5 drops, 99.5→101.0 rises)
  → riseCount = 1 < 2
  → sustainedRise = FALSE
```

**Buffer flush on interval change:** When sampling interval changes dramatically (e.g., from 30min to 2min during escalation), old readings at the long interval are invalidated — the buffer is filled with the last reading to avoid comparing readings taken 30 minutes apart with ones taken 2 minutes apart.

---

## 5. Communication Architecture — Device Reports, Server Alerts

### The Division of Responsibility

```
┌──────────────────────────────────────────────────────────────────┐
│                                                                  │
│  DEVICE (ESP32-S3 in buoy)          SERVER (cloud)              │
│  ─────────────────────────          ───────────────              │
│                                                                  │
│  • Reads sensors                    • Receives HTTP POST        │
│  • Detects operating mode           • Stores reading history    │
│  • Computes water height            • Compares response level   │
│  • Evaluates flood state              to previous known level   │
│  • Determines response level        • On level CHANGE:         │
│  • Sends ONE HTTP POST per            dispatches notifications  │
│    transmit interval                  via SMS, WhatsApp, IVR,   │
│  • Done.                              email                     │
│                                     • Manages all contact lists │
│  The device does NOT:               • Handles cooldowns         │
│  • Store contact numbers            • Sends de-escalation msgs  │
│  • Send alert SMS                   • Sends all-clear msgs     │
│  • Make voice calls                 • Provides web dashboard    │
│  • Wait for acknowledgments         • Logs all events          │
│  • Manage escalation chains                                     │
│                                                                  │
└──────────────────────────────────────────────────────────────────┘
```

### What the Device Sends

Every transmit interval, the SIM800L makes one HTTP POST to the server. The payload contains everything the server needs:

```json
{
  "d": "VARUNA_GDV_KG01",
  "lat": 16.5100,
  "lon": 81.8200,
  "health": 85,
  "mode": 1,
  "originDist": 1.3,
  "draftRate": 0.02,
  "r": [
    {
      "t": 1742556720,
      "h": 275.3,
      "r": 12.45,
      "z": 3,
      "l": 4,
      "b": 78,
      "s": 1
    }
  ]
}
```

```
Payload fields:
  d          = Station identifier (unique per deployment)
  lat, lon   = GPS coordinates
  health     = Composite health score (0-100)
  mode       = Current operating mode (0-3)
  originDist = Distance from anchor origin (meters)
  draftRate  = Draft pressure change rate (hPa/hr)
  r[]        = Readings array (buffered since last transmit)
    t = Unix timestamp
    h = Water height (cm)
    r = Rate of change (cm/15min)
    z = Zone classification (0=NORMAL, 1=ALERT, 2=WARNING, 3=DANGER)
    l = Response level (0=NORMAL, 1=WATCH, 2=WARNING, 3=FLOOD, 4=CRITICAL)
    b = Battery percent
    s = Sustained rise flag (0/1)
```

The device's role ends the moment the HTTP POST receives a 2xx response.

### What the Server Does on Receiving a POST

```
INCOMING PAYLOAD
      │
      ▼
┌──────────────────────────────────────────────┐
│  1. Parse and validate JSON                   │
│  2. Store readings in time-series database    │
│  3. Extract latest response level (l)        │
│  4. Compare to previousStoredLevel            │
│     for this station                          │
└──────────────────┬───────────────────────────┘
                   │
          ┌────────┼─────────┐
          │        │         │
     l > prev  l == prev  l < prev
     ESCALATE  NO CHANGE  DE-ESCALATE
          │        │         │
          ▼        ▼         ▼
   Send alerts  Check if   Send de-esc
   to contacts  reminder   messages
   at new level is due     to prev contacts
                   │         │
                   ▼         │
              Sustained?     ▼
              Send reminder  l == NORMAL?
              at cooldown    Send ALL CLEAR
              interval       Reset event
```

### Server-Side Notification Dispatch

**Level transitions trigger SMS to specific Indian government contacts:**

```
TRANSITION                CONTACTS NOTIFIED              CHANNEL
──────────────────────    ──────────────────────────     ────────
NORMAL → WATCH            Internal ops team only         Dashboard
                          (no government officials)      

WATCH → WARNING           Sarpanch / Ward officer        SMS
                          Tahsildar circle office         SMS

WARNING → FLOOD           District Control Room           SMS + IVR
                          District Collector duty #       SMS + IVR
                          Local police station            SMS
                          duty officer

FLOOD → CRITICAL          All FLOOD contacts              SMS (no cooldown)
                          State Emergency Operations      SMS + IVR
                          Centre (SEOC)

Any level → lower level   Everyone notified at the       SMS
                          higher level gets
                          de-escalation message

Any level → NORMAL        Everyone contacted during      SMS
                          the entire event gets
                          ALL CLEAR message
```

### Server-Side Cooldown & Reminder Logic

```
The cooldown exists to handle SUSTAINED levels, not to
throttle the device. The device only sends one POST per
interval anyway.

SUSTAINED LEVEL REMINDERS:
  WATCH:     No reminder (ops team sees dashboard)
  WARNING:   Reminder every 60 minutes
  FLOOD:     Reminder every 30 minutes
  CRITICAL:  Reminder every 15 minutes

Reminders are worded DIFFERENTLY from escalation alerts:

  Escalation: "VARUNA FLOOD ALERT | Station: Godavari-KG-01 |
               Level: 275cm (DANGER) | Rising at 12cm/15min |
               GPS: 16.5N 81.8E | 14:32 IST 21-Mar-2026"

  Reminder:   "VARUNA STATUS UPDATE | Station: Godavari-KG-01 |
               Level: 280cm (DANGER) | FLOOD ALERT ONGOING |
               Rising at 8cm/15min | Duration: 45min |
               GPS: 16.5N 81.8E | 15:17 IST 21-Mar-2026"

Recipients can immediately tell whether the situation
WORSENED (escalation) or is PERSISTING (reminder).
```

### SMS Message Format

```
Designed for Indian government officials receiving from unknown numbers.
Structured, credible, no abbreviations:

ESCALATION ALERT:
┌─────────────────────────────────────────────────────────────┐
│ VARUNA FLOOD ALERT | Station: Godavari-KG-01 |             │
│ Level: 275cm (DANGER) | Rising at 12cm/15min |             │
│ GPS: 16.5N 81.8E | 14:32 IST 21-Mar-2026                  │
└─────────────────────────────────────────────────────────────┘

STATUS REMINDER:
┌─────────────────────────────────────────────────────────────┐
│ VARUNA STATUS UPDATE | Station: Godavari-KG-01 |           │
│ Level: 280cm (DANGER) | FLOOD ALERT ONGOING |              │
│ Rising at 8cm/15min | Duration: 45min |                    │
│ GPS: 16.5N 81.8E | 15:17 IST 21-Mar-2026                  │
└─────────────────────────────────────────────────────────────┘

DE-ESCALATION:
┌─────────────────────────────────────────────────────────────┐
│ VARUNA LEVEL CHANGE | Station: Godavari-KG-01 |            │
│ Level DROPPED: FLOOD → WARNING |                            │
│ Current: 210cm | Falling at 3cm/15min |                    │
│ Monitoring continues | 15:45 IST 21-Mar-2026               │
└─────────────────────────────────────────────────────────────┘

ALL CLEAR:
┌─────────────────────────────────────────────────────────────┐
│ VARUNA ALL CLEAR | Station: Godavari-KG-01 |               │
│ Water level returned to NORMAL |                            │
│ Current: 95cm | Peak during event: 285cm |                 │
│ Event duration: 3hr 20min |                                │
│ Normal monitoring resumed | 17:52 IST 21-Mar-2026          │
└─────────────────────────────────────────────────────────────┘
```

### Contact Management

```
All contacts stored in server database.
Managed through web interface.
No SMS commands, no device-side storage.

Server database schema (simplified):

  contacts:
    id            INT
    station_id    VARCHAR
    name          VARCHAR     (e.g., "Tahsildar, Kakinada Circle")
    phone         VARCHAR     (e.g., "+919876543210")
    trigger_level INT         (minimum level to notify)
    channels      SET         (SMS, WhatsApp, IVR, Email)
    active        BOOLEAN
    notes         TEXT

  notifications_log:
    id            INT
    station_id    VARCHAR
    contact_id    INT
    level         INT
    message       TEXT
    channel       VARCHAR
    sent_at       TIMESTAMP
    delivered     BOOLEAN
    
If a Tahsildar is transferred and replaced:
  → Update one row in contacts table
  → New person starts receiving from next event
  → No firmware change, no field visit, no SMS command
```

### GPRS as Primary, SMS as Diagnostic-Only

```
┌─────────────────────────────────────────────────────────────┐
│  COMMUNICATION CHANNELS                                      │
│                                                              │
│  PRIMARY: GPRS HTTP POST                                     │
│  ├── Every transmit interval                                 │
│  ├── Carries full sensor payload                             │
│  ├── Server confirms with HTTP 2xx                           │
│  ├── Retry on failure, archive to SPIFFS if persistent       │
│  └── If 3 consecutive failures → flag GPRS_OFFLINE           │
│                                                              │
│  FALLBACK: Serial dump + SPIFFS archive (if GPRS offline)   │
│  ├── Buffered data written to serial output                  │
│  ├── Archived to SPIFFS for later retrieval                  │
│  └── Data not lost, just delayed                             │
│                                                              │
│  DIAGNOSTIC: Inbound SMS (from authorized field engineers)   │
│  ├── STATUS → device replies with current state              │
│  ├── Maintenance commands as documented                      │
│  └── DIAG_* → diagnostic suite commands                      │
│                                                              │
│  NOT USED FOR:                                               │
│  ✗ Sending alert SMS to authorities                          │
│  ✗ Storing contact numbers                                   │
│  ✗ Making voice calls                                        │
│  ✗ Waiting for acknowledgments                               │
│  ✗ Managing escalation chains                                │
└─────────────────────────────────────────────────────────────┘
```

---



## 6. Adaptive Sampling System (System 2)

The system **dynamically adjusts** how often it reads sensors and transmits data based on flood response level and battery condition:

```
                        Battery OK        Battery LOW       Battery CRITICAL
                        (≥20%)            (10-20%)          (<10%)
                    ┌─────────────────┬─────────────────┬─────────────────┐
CRITICAL            │ S:2m    T:2m    │ S:5m    T:5m    │ S:5m    T:5m    │
                    │ GPRS:ON         │ GPRS:ON         │ GPRS:CONSERVE   │
                    ├─────────────────┼─────────────────┼─────────────────┤
FLOOD               │ S:5m    T:5m    │ S:5m    T:5m    │ S:5m    T:5m    │
                    │ GPRS:ON         │ GPRS:ON         │ GPRS:CONSERVE   │
                    ├─────────────────┼─────────────────┼─────────────────┤
WARNING             │ S:5m    T:5m    │ S:5m    T:5m    │ S:5m    T:5m    │
                    │ GPRS:ON         │ GPRS:ON         │ GPRS:CONSERVE   │
                    ├─────────────────┼─────────────────┼─────────────────┤
WATCH               │ S:10m   T:15m   │ S:15m   T:30m   │ S:120m  T:240m  │
                    │ GPRS:ON         │ GPRS:ON         │ GPRS:CONSERVE   │
                    ├─────────────────┼─────────────────┼─────────────────┤
NORMAL (mod/fast)   │ S:15m   T:30m   │ S:60m   T:120m  │ S:120m  T:240m  │
                    │ GPRS:ON         │ GPRS:ON         │ GPRS:CONSERVE   │
                    ├─────────────────┼─────────────────┼─────────────────┤
NORMAL (slow/fall)  │ S:30m   T:60m   │ S:60m   T:120m  │ S:120m  T:240m  │
                    │ GPRS:ON         │ GPRS:ON         │ GPRS:CONSERVE   │
                    └─────────────────┴─────────────────┴─────────────────┘

S = Sample interval    T = Transmit interval
GPRS:ON = full JSON payload each transmit
GPRS:CONSERVE = reduced payload (latest reading only, no array)
                to minimize radio-on time

Note: At CRITICAL battery + CRITICAL/FLOOD response, GPRS stays
active because the server MUST receive data to dispatch alerts.
The device does not send alert SMS even when GPRS is offline.
If GPRS fails at critical battery, data is archived to SPIFFS
and the server detects the station as silent.
```

### Battery Management

```
Battery: 8× 18650 Li-Ion 2600mAh in PARALLEL
  Voltage = single cell voltage (3.0V–4.2V)
  Capacity = 8 × 2600mAh = 20,800mAh (20.8Ah)

  Full charge:  4.20V
  Nominal:      3.70V
  Low:          3.30V
  Empty/cutoff: 3.00V (protect cells from damage)

Read via voltage divider:
  R1 = 33kΩ (top), R2 = 100kΩ (bottom)
  Divider ratio = (R1+R2)/R2 = 133/100 = 1.33
  At 4.2V full:  ADC sees 3.158V (safe, under 3.3V)
  At 3.0V empty: ADC sees 2.256V
  100nF ceramic cap across R2 for noise reduction

Thresholds:
  BATTERY_LOW_THRESH     = 20%  → extend intervals, disable GPRS non-critical
  BATTERY_CRITICAL_THRESH = 10% → maximum conservation mode
```

**Piecewise Li-Ion discharge curve mapping:**
```
Li-ion discharge is NOT linear. Using piecewise approximation:

  4.20V–4.10V  →  100%–90%   (steep initial drop)
  4.10V–3.85V  →   90%–70%   (gradual)
  3.85V–3.70V  →   70%–40%   (flat plateau region)
  3.70V–3.50V  →   40%–20%   (starting to drop)
  3.50V–3.30V  →   20%–5%    (dropping faster)
  3.30V–3.00V  →    5%–0%    (steep final drop)

Why not linear? At 3.3V, linear mapping says 25% remaining
when actually only 5% remains. This would prevent the
BATTERY_LOW_THRESH (20%) from triggering power-saving
intervals in time.

Voltage
4.2V ┤■■
     │  ■■■
4.0V ┤     ■■■■
     │          ■■■■■■■■■■■■■■■   ← Flat plateau
3.7V ┤                         ■■■
     │                            ■■■
3.5V ┤                               ■■
     │                                 ■■
3.3V ┤                                   ■■
     │                                     ■■■
3.0V ┤                                        ■
     └──────────────────────────────────────────
     0%    20%    40%    60%    80%    100%
```

**Low-voltage cutoff protection:**
```
3 consecutive readings below 3.00V required to trigger shutdown.
Prevents transient voltage dips from causing false shutdown.

Shutdown sequence:
  1. Force save all state to EEPROM
  2. Turn off obstruction lights
  3. Turn off status LED
  4. Enter deep sleep (no wake timer = sleep until reset/power cycle)
  Purpose: protect 18650 cells from damage below 2.8V
```

### Transmit Buffer

```
TX_BUFFER_SIZE = 30 readings buffered before transmit
Each entry: {waterHeight, rateOfChange, timestamp}

When transmit interval fires:
  → GPRS POST full JSON payload to server
  → Server handles all alerting from payload
  → On POST failure: archive to SPIFFS
  → On 3× consecutive failure: flag GPRS_OFFLINE, keep archiving

NOTE: While GPRS is offline, the server receives NO data and
therefore sends NO alerts. The server detects this as a
"silent station" and can flag it on the dashboard.

The device does NOT fall back to sending alert SMS.
It continues monitoring and archiving locally.
```

---

## 7. SIM800L Communication — HTTP POST & Diagnostic Channel

### Hardware

```
SIM800L on hardware UART2 (GPIO 15/16, 9600 baud)
RST pin on GPIO 17 for hardware reset
SIM card: any data-enabled Indian SIM (Airtel, Jio, BSNL, etc.)
```

### Initialization Sequence

```
1. Hardware reset (pull RST LOW 200ms, then HIGH)
2. Wait 2 seconds for boot
3. Send "AT" up to 5 times (2s timeout each)
4. ATE0 (disable echo)
5. AT+CMGF=1 (SMS text mode — for diagnostic channel only)
6. AT+CPIN? (check SIM card present)
7. AT+CREG? (check network registration, 5 quick attempts)
8. AT+CSQ (read signal strength)
9. AT+CSCS="GSM" (character set)
10. AT+CNMI=2,2,0,0,0 (direct SMS delivery — diagnostic inbound)
11. AT+CMGDA="DEL ALL" (clear stored messages)

NOTE: No contact lists loaded, no tiers configured.
GPRS bearer initialized at first transmit interval.
```

### AT Command Engine

```
ATResult send_at_command(command, expected_response, timeout_ms)
  → Returns: AT_SUCCESS, AT_TIMEOUT, or AT_ERROR
  → Accumulates response bytes into simResponseBuffer[256]
  → Checks for expected substring OR "ERROR"
  → Feeds watchdog during wait
```

### Primary Function: GPRS HTTP POST

```
Every transmit interval:

  1. Check GPRS bearer connected (AT+SAPBR)
  2. If not connected → gprsInit() (set APN, open bearer)
  3. AT+HTTPINIT
  4. Set URL (AT+HTTPPARA="URL",server_endpoint)
  5. Set content type (application/json)
  6. AT+HTTPDATA=<length>,10000 → wait for "DOWNLOAD"
  7. Send JSON payload bytes
  8. AT+HTTPACTION=1 (POST)
  9. Wait for +HTTPACTION response
  10. Check HTTP status code (200-299 = success)
  11. AT+HTTPTERM

  On success: clear transmit buffer, reset fail counter
  On failure: increment gprsConsecutiveFails
    → After 3 consecutive fails: archive to SPIFFS
    → Continue buffering data for next attempt
    → Flag GPRS_OFFLINE in health score
```

### Secondary Function: Diagnostic SMS Channel

```
INBOUND SMS PROCESSING:
  SIM800L delivers SMS directly to UART as:
    +CMT: "<sender>","","<timestamp>"
    <message body>

  State machine:
    SMS_RX_IDLE → receive +CMT header → SMS_RX_HEADER_RECEIVED
    SMS_RX_HEADER_RECEIVED → receive body → process command → IDLE

  Authorization: field engineer phone numbers stored on device
  (small list, 5 max — these are NOT alert contacts,
   these are people who physically maintain the device)

OUTBOUND SMS (diagnostic replies only):
  When a field engineer sends a diagnostic command via SMS,
  the device replies with results.
  This is the ONLY outbound SMS the device ever sends.
  
  SMS queue: reduced to 5 slots (diagnostic replies only)
  No alert SMS, no contact tiers, no voice calls.
```

### Periodic Maintenance

```
Signal strength check:   every 60 seconds (AT+CSQ)
Registration check:      every 30 seconds (AT+CREG?)
Network recovery:        automatic re-registration if lost
```

---

## 8. DS1307 RTC Integration

```
Initialization:
  1. Check I2C presence on Bus 1 (address 0x68)
  2. Convert 12-hour format to 24-hour if needed
  3. Clear clock-halt bit (bit 7 of register 0x00)
  4. Disable square-wave output (register 0x07 = 0x00)

Time synchronization priority:
  1. GPS time (highest accuracy) → synced to RTC once per 24 hours
  2. RTC time (persists across power cycles via battery backup)
  3. millis()/1000 (fallback, no absolute time)

Sync conditions for GPS→RTC:
  - GPS fix valid
  - GPS time valid (year ≥ 2024)
  - HDOP ≤ 5.0
  - Satellites ≥ 4
  - Not already synced in last 24 hours
```

**RTC register reading:**
```
Read 7 bytes starting from register 0x00:
  Byte 0: Seconds (BCD, mask 0x7F — bit 7 is clock halt)
  Byte 1: Minutes (BCD, mask 0x7F)
  Byte 2: Hours (BCD, mask 0x3F — 24-hour mode)
  Byte 3: Day of week (BCD, mask 0x07)
  Byte 4: Day (BCD, mask 0x3F)
  Byte 5: Month (BCD, mask 0x1F)
  Byte 6: Year (BCD, offset from 2000)
```

**Unix time conversion** (`dateToUnix`): Custom algorithm avoiding `time.h` — appropriate for embedded use. Uses the formula based on counting days with leap year correction, then adding hours/minutes/seconds.

**Time validity check:** Year must be ≥ 2024, month 1-12, day 1-31.

**Best timestamp function** (`getBestTimestamp()`):
```
IF rtcTimeValid AND currentUnixTime > 0:
  return currentUnixTime        (real calendar time)
ELSE:
  return millis() / 1000        (uptime in seconds, no date)
```

---

## 9. GPS NMEA Parsing

```
Sentences parsed:
  $GPGGA / $GNGGA → Position, altitude, fix quality, satellites, HDOP, time
  $GPRMC / $GNRMC → Position, speed, date, time

Processing flow:
  GPS_Serial.available() → accumulate into nmeaBuffer[120]
      │
      ▼
  On newline → validate checksum (XOR between $ and *)
      │
      ▼
  Strip checksum → split by commas → parse fields
      │
      ▼
  Update: gpsLat, gpsLon, gpsAlt, gpsSpeed, gpsHdop,
          gpsSatellites, gpsFixValid, gpsHour/Min/Sec,
          gpsGGADay/Month/Year
```

**Checksum validation:**
```
XOR all bytes between '$' and '*'
Compare computed checksum to stated hex value after '*'
Only process sentences that pass validation
```

**NMEA coordinate conversion:**
```
nmeaToDecimal(coord, direction):
  Raw NMEA: DDMM.MMMM (degrees + minutes)
  degrees = integer(raw / 100)
  minutes = raw - (degrees × 100)
  decimal = degrees + minutes / 60.0
  If direction is 'S' or 'W': decimal = -decimal
```

**GPS→RTC sync** (`syncRTCfromGPS()`):
```
Conditions: GPS fix valid, time valid, year ≥ 2024,
            HDOP ≤ 5.0, satellites ≥ 4,
            not synced in last 24 hours (86400000 ms)

Action: Write GPS date/time to DS1307 via I2C
        Immediately read back and validate
        Set gpsTimeSynced = true
        Reset 24-hour sync timer
```

---

## 10. EEPROM State Persistence

### State Save Layout

```
Addr  Size  Content
────  ────  ───────────────────────────
0     1     Magic byte (0xA5)
1     1     Current response level
2-17  16    Sustained buffer (4 × float)
18    4     Save timestamp (uint32)
22    4     Alert level threshold
26    4     Warning level threshold
30    4     Danger level threshold
34    4     OLP length (tether length L)
38    4     Peak water height
42    4     Peak time
46    4     Previous water height
50    4     State entry time

54-69 16    Authorized field engineer numbers
             (5 × phone number, for diagnostic SMS)
70    1     Authorized count
71    1     Authorized magic (0xC3)

80    4     Origin latitude (float)
84    4     Origin longitude (float)
88    4     Geofence radius (float, meters)
92    1     Origin magic (0xGF)

290   4     Obstruction light ON time
294   4     Obstruction light OFF time
298   1     Obstruction light magic (0xBE)
300   16    Obstruction preset name
```

### Warm Boot Recovery

```
On boot:
  1. Read magic byte — if 0xA5, state was saved
  2. Check age: if (now - savedTime) > 1800 seconds → too old, discard
  3. Restore response level, sustained buffer, thresholds, peak data
  4. Resume flood monitoring from last known state
  
Purpose: Power glitches during floods don't reset alert state.
         Device resumes monitoring at correct response level.
         Server handles notification continuity independently.
```

**Validation on restore:**
```
Response level: must be 0-4 (RESP_NORMAL to RESP_CRITICAL)
Sustained buffer: each value must not be NaN, Inf, negative, or > 10000
Thresholds: must be ascending (alert < warning < danger) and positive
OLP length: must be positive and < 10000
Peak height: must be positive and < 10000
Previous height: must not be NaN or Inf
```

### Write Throttling

```
EEPROM_SAVE_INTERVAL = 1,800,000 ms (30 minutes)

markEepromDirty() → sets flag
saveEepromIfNeeded() → checks flag + interval → writes if due
forceSaveEeprom() → immediate write (on critical state change)

Purpose: Protect flash from wear (ESP32 EEPROM is flash-backed,
         limited to ~100K write cycles)
```

### EEPROM Helper Functions

```
eepromWriteFloat(addr, value)  — writes 4 bytes
eepromReadFloat(addr)          — reads 4 bytes
eepromWriteUint32(addr, value) — writes 4 bytes
eepromReadUint32(addr)         — reads 4 bytes
```

---

## 11. Marine Obstruction Light System

```
Standards implemented:
  - IALA Recommendation O-139 (international maritime)
  - USCG 33 CFR Part 66 (US private aids to navigation)

Hardware: 5 red LEDs, all flash simultaneously
          Currently all mapped to GPIO 2 (development placeholder)

10 presets available:
  IALA_FL4S    500ms ON / 3500ms OFF  (default, 12.5% duty)
  IALA_FL2S    300ms ON / 2200ms OFF
  IALA_FL5S    500ms ON / 4500ms OFF
  IALA_QFL     250ms ON / 750ms OFF   (60 flashes/min)
  IALA_VQF     167ms ON / 333ms OFF   (120 flashes/min)
  IALA_LFL10   2000ms ON / 8000ms OFF
  IALA_ISO4    2000ms ON / 2000ms OFF
  IALA_ISO2    1000ms ON / 1000ms OFF
  USCG_STD     1000ms ON / 4000ms OFF
  USCG_QFL     600ms ON / 600ms OFF

Custom timing allowed:
  ON range:  50–5000 ms
  OFF range: 100–30000 ms

Timing persisted to EEPROM (survives reboot).
Preset name stored alongside timing values.
```

**Flash update logic** (`obLightUpdate(now)`):
```
If disabled: ensure all LEDs off, return
If LEDs on and elapsed ≥ onTime: turn off, reset timer
If LEDs off and elapsed ≥ offTime: turn on, reset timer
```

---

## 12. Dual Operating Modes (Algorithm Toggle)

```
┌──────────────────────────┬──────────────────────────────┐
│  ALGORITHM DISABLED      │  ALGORITHM ENABLED           │
│  (Continuous Mode)       │  (Adaptive Mode)             │
├──────────────────────────┼──────────────────────────────┤
│  LED ON (GPIO 13)        │  LED OFF                     │
│  Sensors read every loop │  Adaptive intervals          │
│  No flood evaluation     │  Full flood state machine    │
│  No alerts dispatched    │  HTTP POST carries levels    │
│  No light-sleep          │  Light-sleep when idle       │
│  CSV output at 1 Hz      │  Output gated by interval    │
│  Default on boot         │  Toggle via button           │
│  For bench testing       │  For deployment              │
└──────────────────────────┴──────────────────────────────┘

Toggle: Physical button (GPIO 12) on the device,
        or handheld debugger command during bench testing.

Not available from the remote console. Algorithm state
is set before deployment and must not be changed on a
live monitoring device without physical presence.
```

**Debounce:** 250ms minimum between presses.

**On enable:** Reset sample/transmit timers, update adaptive intervals based on current state.

**On disable:** Sensors read continuously, data logging continues, obstruction lights continue, GPS/RTC/SIM housekeeping continues, but no flood evaluation, no adaptive intervals, no light-sleep.

---

## 13. XIAO C3 — OTA Programming Agent & Firebase Data Feed

The Seeed XIAO ESP32-C3 is a companion microcontroller permanently mounted alongside the ESP32-S3 inside the sealed buoy capsule. It has two responsibilities:

1. **CSV Data Feed** — Receives live sensor data from the S3 via software UART and forwards it to Firebase via its own SIM800L GPRS connection
2. **OTA Firmware Delivery** — Receives firmware binaries from the server, programs them into the S3 using the ESP32 ROM bootloader protocol, and confirms successful boot

### 13.1 Physical Connections

```
┌──────────────────────┐          ┌──────────────────────┐
│     ESP32-S3         │          │    XIAO ESP32-C3     │
│     (Main MCU)       │          │    (OTA Agent)       │
│                      │          │                      │
│  GPIO 14 (SW-TX) ────┼──9600──→┼── RX (GPIO X)       │
│  CSV feed, 1Hz       │  baud   │  Parsed → Firebase   │
│                      │          │                      │
│  GPIO 43 (UART TX) ──┼─115200─→┼── RX (GPIO 20)      │
│  OTA data / normal   │  baud   │  Bootloader protocol │
│                      │          │  or normal UART      │
│  GPIO 44 (UART RX) ──┼─115200─←┼── TX (GPIO 21)      │
│  OTA responses       │  baud   │  Bootloader responses│
│                      │          │                      │
│  GPIO 0 (BOOT) ──────┼────←────┼── GPIO 2 (output)   │
│  LOW = bootloader    │          │  C3 controls boot    │
│  HIGH = normal boot  │          │  mode of S3          │
│                      │          │                      │
│  EN (RESET) ─────────┼────←────┼── GPIO 3 (output)   │
│  LOW pulse = reset   │          │  C3 controls reset   │
│                      │          │  of S3               │
│                      │          │                      │
│  GND ────────────────┼─────────┼── GND               │
│                      │          │                      │
│                      │          │  Own SIM800L ────→   │
│                      │          │  GPRS to Firebase    │
│                      │          │  (independent of S3) │
└──────────────────────┘          └──────────────────────┘

Wire count between boards: 5 signal + 1 ground = 6 wires total

GPIO 14: Software UART TX at 9600 baud (S3 → C3, CSV only)
GPIO 43/44: Hardware UART at 115200 baud (bidirectional, OTA)
GPIO 0: Bootstrap control (C3 → S3, normally floating/HIGH)
EN: Reset control (C3 → S3, normally floating/HIGH)
```

### 13.2 CSV Data Feed (Continuous)

```
Every 1 second, the S3 transmits a 39-field CSV line on GPIO 14
at 9600 baud using software UART bit-banging.

The C3 receives this, parses the fields, and forwards selected
values to Firebase Realtime Database via its own SIM800L GPRS
connection.

The CSV feed runs continuously regardless of OTA state.
During OTA, the feed continues on GPIO 14 while the OTA
protocol uses the separate GPIO 43/44 channel.

The two channels are electrically and logically independent.
A failed OTA cannot disrupt the CSV feed.
```

### 13.3 OTA Firmware Update — Complete Process

```
┌─────────────────────────────────────────────────────────────────┐
│                    OTA UPDATE SEQUENCE                           │
│                                                                 │
│  PHASE 1: Server Side (Dashboard)                              │
│  ────────────────────────────────                              │
│  Engineer uploads .bin file through web dashboard.              │
│  Server computes CRC32/MD5 of the binary.                      │
│  Server writes to Firebase:                                     │
│    varuna/<device_id>/ota/command: "CHECK_READY"               │
│  Dashboard shows: "Waiting for device readiness..."            │
│                                                                 │
│  PHASE 2: C3 Readiness Acknowledgment (Gate 1)                │
│  ──────────────────────────────────────────────                │
│  C3 watches Firebase ota/command node continuously.            │
│  On seeing CHECK_READY, C3 checks:                             │
│    • Is S3 CSV feed active? (confirms S3 is running)          │
│    • Is current zone < WARNING? (safe to interrupt)           │
│    • Is C3 SIM800L connected and GPRS active?                 │
│    • Is C3 SPIFFS/LittleFS free space sufficient?             │
│                                                                 │
│  C3 writes to Firebase:                                         │
│    ota/status: "READY"                                         │
│    OR                                                           │
│    ota/status: "BUSY:FLOOD_EVENT_ACTIVE"                       │
│    OR                                                           │
│    ota/status: "BUSY:INSUFFICIENT_STORAGE"                     │
│    OR                                                           │
│    ota/status: "BUSY:GPRS_OFFLINE"                             │
│                                                                 │
│  Dashboard shows readiness result to engineer.                  │
│  Engineer must click CONFIRM to proceed.                        │
│  If BUSY, engineer can retry later.                             │
│                                                                 │
│  ── GATE 1 PASSED ──                                           │
│                                                                 │
│  PHASE 3: Firmware Download to C3                              │
│  ────────────────────────────────                              │
│  Server breaks .bin file into 4KB chunks.                       │
│  Stores chunks in Firebase Storage (or RTDB base64 segments). │
│  Signals C3: ota/command: "BEGIN_DOWNLOAD"                     │
│  Includes: total_chunks, total_bytes, checksum                 │
│                                                                 │
│  C3 downloads one chunk at a time over GPRS:                   │
│    Read chunk N from Firebase                                   │
│    Write to local filesystem (SPIFFS/LittleFS)                 │
│    Acknowledge: ota/progress: "CHUNK_N_OK"                     │
│    Server sends next chunk only after ACK received             │
│                                                                 │
│  Speed: ~7-10 KB/s on GPRS                                     │
│  Time for 1MB binary: ~100-150 seconds download                │
│  Time for 500KB binary: ~50-75 seconds download                │
│                                                                 │
│  If a chunk fails (timeout, corruption):                       │
│    C3 reports: ota/progress: "CHUNK_N_FAIL"                    │
│    Server retransmits that chunk                                │
│    Up to 3 retries per chunk before aborting                   │
│                                                                 │
│  After all chunks received:                                     │
│    C3 reassembles complete binary on filesystem                │
│    Computes CRC32/MD5 of received file                         │
│    Compares to server-provided checksum                        │
│                                                                 │
│  ── GATE 2: CHECKSUM VERIFICATION ──                           │
│                                                                 │
│  Match: ota/status: "FIRMWARE_VERIFIED"                        │
│  Mismatch: ota/status: "CHECKSUM_FAIL"                         │
│    → Server can retransmit corrupted chunks                    │
│    → Or abort and retry full download                          │
│                                                                 │
│  ── GATE 2 PASSED ──                                           │
│                                                                 │
│  PHASE 4: Programming the ESP32-S3                             │
│  ────────────────────────────────────                          │
│                                                                 │
│  C3 reports: ota/status: "FLASHING_START"                      │
│                                                                 │
│  Step 1: Enter bootloader mode                                  │
│    C3 pulls GPIO 0 of S3 LOW (via C3 GPIO 2)                  │
│    C3 pulls EN of S3 LOW for 200ms (via C3 GPIO 3)            │
│    C3 releases EN HIGH                                          │
│    S3 reboots into ROM serial bootloader                       │
│    (GPIO 0 held LOW during reset = download mode)              │
│    C3 releases GPIO 0 HIGH                                     │
│                                                                 │
│  Step 2: Flash firmware via UART                                │
│    C3 communicates with S3 ROM bootloader on GPIO 43/44        │
│    at 115200 baud using ESP serial protocol (SLIP framing)    │
│    C3 sends firmware in 4KB blocks                             │
│    Each block: slip-encoded frame + CRC                        │
│    S3 bootloader writes each block to flash                    │
│    S3 bootloader acknowledges each block                       │
│    C3 reports progress: ota/progress: "FLASH_BLOCK_N/TOTAL"   │
│                                                                 │
│  Step 3: Finish and reset                                       │
│    C3 sends flash-finish command to bootloader                 │
│    C3 releases GPIO 0 (ensure HIGH for normal boot)           │
│    C3 toggles EN LOW then HIGH (clean reset)                   │
│    S3 reboots into new firmware                                │
│                                                                 │
│  Flash time for 1MB binary at 115200 baud: ~90 seconds        │
│  Flash time for 500KB binary: ~45 seconds                      │
│                                                                 │
│  PHASE 5: Boot Confirmation (Gate 3)                           │
│  ───────────────────────────────────                           │
│                                                                 │
│  After S3 reset, C3 monitors GPIO 14 CSV feed:                 │
│    Expecting CSV output to resume within 15-20 seconds         │
│    (S3 setup() runs calibration, sensor init, etc.)            │
│                                                                 │
│  C3 waits for 3-5 consecutive valid CSV lines:                 │
│    Valid = correct number of fields, parseable numbers,        │
│    reasonable values (theta < 90, pressure 300-1200, etc.)    │
│                                                                 │
│  SUCCESS (CSV resumes with valid data):                         │
│    ota/status: "OTA_COMPLETE"                                  │
│    ota/result: {                                                │
│      "success": true,                                          │
│      "flash_time_sec": 92,                                     │
│      "boot_time_sec": 18,                                      │
│      "csv_validated": true,                                    │
│      "timestamp": 1742556720                                   │
│    }                                                            │
│    Dashboard shows: "✅ Firmware updated successfully"          │
│                                                                 │
│  FAILURE (CSV not detected within 60 seconds):                  │
│    ota/status: "OTA_FAILED:NO_BOOT"                            │
│    Dashboard shows: "❌ Device not responding after flash.     │
│                      Field visit required."                     │
│                                                                 │
│  PARTIAL (CSV resumes but values look wrong):                   │
│    ota/status: "OTA_WARNING:ABNORMAL_OUTPUT"                   │
│    Dashboard shows: "⚠️ Device booted but output anomalous.   │
│                      Investigate."                              │
│                                                                 │
│  ── GATE 3 PASSED (or failed with clear reporting) ──          │
│                                                                 │
└─────────────────────────────────────────────────────────────────┘
```

### 13.4 The Three Acknowledgment Gates

```
GATE 1: READINESS CHECK
  Purpose:  Prevent OTA during active flood events or when
            C3 is not in a state to receive firmware
  Who asks: Server (via Firebase)
  Who answers: C3 (checks its own state + S3 CSV zone field)
  Pass condition: C3 ready, S3 running, zone < WARNING
  Fail action: Dashboard shows "BUSY" with reason, retry later
  
GATE 2: CHECKSUM VERIFICATION
  Purpose:  Catch any corruption during GSM download before
            the binary touches the S3's flash memory
  Who checks: C3 (computes hash of downloaded file)
  Pass condition: Computed hash matches server-provided hash
  Fail action: Server retransmits corrupted chunks or full file
               S3 is never touched — still running old firmware

GATE 3: BOOT CONFIRMATION
  Purpose:  Confirm the new firmware is actually running and
            producing valid sensor data
  Who checks: C3 (monitors CSV feed on GPIO 14)
  Pass condition: 3-5 valid CSV lines within 60 seconds of reset
  Fail action: Dashboard shows failure with specific reason
               If S3 stuck in bootloader: field visit required
               If S3 boots but outputs garbage: may need reflash
```

### 13.5 Safety Properties

```
1. ROM BOOTLOADER IS PERMANENT
   The ESP32-S3's ROM bootloader is in mask ROM — it cannot
   be erased, overwritten, or corrupted by any firmware.
   As long as GPIO 0 can be pulled LOW and EN toggled,
   the S3 can ALWAYS be put into download mode.
   A bad firmware flash does not brick the device.

2. OLD FIRMWARE RUNS UNTIL GATE 2 PASSES
   The S3 continues running its current (working) firmware
   throughout Phases 1-3. It is only rebooted into bootloader
   mode in Phase 4, after the binary has been fully downloaded
   AND checksum-verified on the C3's filesystem.

3. CSV FEED IS INDEPENDENT OF OTA CHANNEL
   GPIO 14 (CSV) and GPIO 43/44 (OTA) are separate pins.
   The CSV feed continues during download phases.
   During Phase 4 (actual flashing), CSV stops — expected.

4. FLOOD EVENT PROTECTION
   Gate 1 checks the current zone from the CSV feed.
   If zone ≥ WARNING, OTA is refused. Flood monitoring
   takes absolute priority over firmware updates.

5. THREE-GATE CONFIRMATION
   The dashboard shows clear pass/fail at each gate.
   No ambiguity about whether the update succeeded.

WHAT CANNOT BE RECOVERED REMOTELY:
  If flash write fails mid-way (power loss, C3 crash),
  S3 is stuck with partially-written firmware. C3 can
  detect this (CSV never resumes) and CAN retry: re-enter
  bootloader mode and reflash. If retry also fails
  (hardware fault), field visit required.
```

### 13.6 OTA Timeline

```
Typical OTA update for a 700KB firmware binary:

  Phase 1: Server prep                    ~5 seconds
  Phase 2: Readiness check (GSM round-trip) ~10-15 seconds
  Phase 3: Download 700KB at ~8 KB/s      ~90 seconds
           + chunk acknowledgments         ~20 seconds
  Phase 4: Flash at 115200 baud           ~60 seconds
  Phase 5: Boot + CSV validation          ~20 seconds
  ────────────────────────────────────────────────────
  TOTAL:                                  ~3.5 minutes

  During this time:
    Minutes 0-2.5: S3 running normally, CSV feeding Firebase
    Minutes 2.5-3.5: S3 in bootloader, CSV paused (~60 seconds)
    Minute 3.5+: S3 running new firmware, CSV resumes

  Maximum downtime of flood monitoring: ~60-90 seconds
```

### 13.7 C3 Firmware Responsibility Summary

```
┌─────────────────────────────────────────────────────────────────┐
│  XIAO C3 FIRMWARE RESPONSIBILITIES                              │
│                                                                 │
│  CONTINUOUS (always running):                                   │
│  ├── Receive CSV on GPIO 14 (9600 baud, 1Hz)                  │
│  ├── Parse CSV fields                                           │
│  ├── Forward to Firebase RTDB via own SIM800L GPRS             │
│  ├── Monitor Firebase ota/command node for OTA triggers        │
│  └── Keep GPIO 0 and EN floating (normal S3 boot)             │
│                                                                 │
│  ON OTA TRIGGER (server-initiated):                            │
│  ├── Gate 1: Check readiness, report to Firebase               │
│  ├── Download firmware chunks to local filesystem              │
│  ├── Gate 2: Verify checksum                                    │
│  ├── Enter S3 bootloader (GPIO 0 LOW + EN toggle)             │
│  ├── Flash firmware via UART (GPIO 43/44, 115200 baud)        │
│  ├── Reset S3 (EN toggle)                                       │
│  ├── Gate 3: Monitor CSV feed for boot confirmation            │
│  └── Report result to Firebase                                  │
│                                                                 │
│  THE C3 DOES NOT:                                               │
│  ├── Process or evaluate flood data (that's the S3's job)     │
│  ├── Send alerts to anyone (that's the server's job)          │
│  ├── Store historical data (that's SPIFFS on the S3)          │
│  ├── Accept commands from the browser console (that's USB)    │
│  └── Display debug information (debug display is removed)      │
│                                                                 │
│  THE C3'S SIM800L IS INDEPENDENT OF THE S3'S SIM800L:         │
│  ├── Separate SIM card, separate GPRS connection               │
│  ├── S3's SIM800L: sends sensor data to server via HTTP POST  │
│  ├── C3's SIM800L: sends CSV feed to Firebase + handles OTA   │
│  └── If one SIM fails, the other continues independently       │
└─────────────────────────────────────────────────────────────────┘
```

---

## 14. Software UART Implementation

Two UART channels connect to the XIAO C3:

```
CSV Display Feed (GPIO 14):
  - 9600 baud (104 μs/bit) — software bit-bang, very reliable
  - TX-only (S3 → C3)
  - Interrupts disabled during each byte TX
  - Standard: Start bit (LOW) → 8 data bits LSB-first → Stop bit (HIGH)
  - Continuous 1Hz output regardless of OTA state
  - Carries 39-field CSV line (same as USB serial output)

OTA / Programming UART (GPIO 43/44):
  - 115200 baud, hardware UART (not bit-banged)
  - Bidirectional: TX (GPIO 43) and RX (GPIO 44)
  - Used by C3 to program S3 via ROM bootloader protocol
  - During normal operation: idle (available for future use)
  - During OTA: ESP serial protocol (SLIP framing)
```

**Software UART TX implementation** (`c3FeedByte()`):
```
At 9600 baud, each bit = 104 microseconds

Disable interrupts
  Send start bit (LOW) → wait 104μs
  Send 8 data bits LSB-first → wait 104μs each
  Send stop bit (HIGH) → wait 104μs
Enable interrupts

Total per byte: ~1040μs (~1ms)
39-field CSV line (~384 chars): ~400ms per line at 9600 baud
```

---

## 15. BMP280 Pressure — Dual Role: Atmospheric Baseline & Depth Measurement

The BMP280 serves two critical functions in this system:

**Role 1: Atmospheric baseline tracking (all modes)**
```
Baseline: Rolling average of 48 readings (spaced 30 min apart)
         = 24-hour atmospheric pressure baseline
         Updated ONLY when buoy is at the surface (MODE 0, 1, or 2)
         
Purpose: Distinguish weather pressure changes from submersion
```

**Role 2: Depth measurement (MODE 3 — submerged)**
```
When water level exceeds tether length L, the buoy is pulled
underwater. The tether holds it at depth (H - L) below surface.

Physics: P_water = P_atmospheric + ρ × g × depth
         
         depth = (P_measured - P_baseline) / (ρ_water × g)
         depth = ΔP / (1000 × 9.81)  [meters]
         depth = ΔP / 0.0981         [cm, with ΔP in hPa]

         H_total = L + depth

Accuracy: ±1-2cm (pressure sensors are excellent for depth)
          Much better than tilt measurement at this range
```

**Mode transition detection via pressure:**
```
Submersion states:
  ΔP < 5 hPa   → State 0: Surface (MODE 0, 1, or 2)
  ΔP > 5 hPa   → State 1: Possible submersion
  ΔP > 8 hPa   → State 2: Confirmed submersion (MODE 3)
  ΔP > 15 hPa  → State 3: Deep submersion (MODE 3, severe)

  5 hPa ≈ 5.1 cm depth
  8 hPa ≈ 8.2 cm depth
  15 hPa ≈ 15.3 cm depth
```

**Cross-validation between measurement methods:**
```
Near the MODE 2 → MODE 3 transition:
  Tilt says:     H = L × cos(2°) = 0.9994L ≈ L
  Pressure says: H = L + 5cm
  If they DISAGREE → sensor fault detected
  If they AGREE → high confidence in reading
```

**BMP280 initialization:**
```
1. Check I2C presence on Bus 1 (address 0x76)
2. Read chip ID (0x58, 0x56, 0x57, or 0x60 accepted)
3. Read calibration coefficients (T1-T3, P1-P9) from registers
4. Configure: oversampling x4 temp, x16 pressure, normal mode
   (register 0xF4 = 0x57, register 0xF5 = 0x10)
5. Take 10 readings → establish initial pressure baseline
```

**Baseline maintenance rule:**
```
Update baseline ONLY when submersionState == 0
(buoy confirmed at surface)

This prevents the baseline from drifting upward during
submersion, which would cause under-reporting of depth.

BASELINE_SIZE = 48 samples
BASELINE_INTERVAL = 1,800,000 ms (30 minutes between updates)
```

**BMP280 raw data compensation:**
Uses the Bosch BMP280 compensation formulas with integer arithmetic for temperature (via `bmpTFine`) and 64-bit integer arithmetic for pressure, as specified in the datasheet. Output in hPa (hectopascals).

---

## 16. Sensor Health & Graceful Degradation

The system has **two measurement channels** that cover different regimes:

```
Primary (MODE 0-2): MPU6050 (tilt angle → H = L×cos(θ))
Primary (MODE 3):   BMP280 (pressure → H = L + depth)

These are NOT backup for each other — they cover DIFFERENT modes.
Both are always active when available.
```

**MPU6050 failure:**
```
IF 5 consecutive I2C read failures:
  mpuHealthy = false
  activeSensor = SENSOR_NONE for tilt measurement
  
  System can still detect:
    MODE 0: Via BMP280 (pressure ≈ atmospheric = safe)
    MODE 3: Via BMP280 (pressure > atmospheric = submerged)
    
  System CANNOT detect:
    MODE 1: No tilt → no H = L×cos(θ) → no precise water level
    MODE 2: No tilt → cannot confirm H ≈ L exactly
    
  Fallback behavior:
    Report "TILT SENSOR OFFLINE"
    If pressure shows submersion → report MODE 3 directly
    If pressure shows atmospheric → report "SAFE (sensor degraded)"
    Cannot report intermediate water levels
```

**BMP280 failure:**
```
IF I2C read fails:
  bmpAvailable = false
  
  System can still detect:
    MODE 0: Via MPU6050 (lateral accel ≈ 0 = slack)
    MODE 1: Via MPU6050 (H = L×cos(θ) precisely)
    MODE 2: Via MPU6050 (θ → 0° = flood level)
    
  System CANNOT detect:
    MODE 3: No pressure → cannot measure depth below surface
    
  Fallback behavior:
    Report "PRESSURE SENSOR OFFLINE"
    If θ ≈ 0° → report MODE 2 (flood reached)
    Cannot quantify how far ABOVE flood level water has risen
    Alert dispatched at MODE 2 regardless
```

**Both sensors failed:**
```
activeSensor = SENSOR_NONE
Report "CRITICAL: ALL SENSORS OFFLINE"
Response level set to CRITICAL in next HTTP POST
Continue attempting I2C recovery every 60 seconds
```

**Health check every 60 seconds** (`checkSensorHealth()`):
```
1. I2C Bus 0: Probe MPU6050 address (0x68)
   └── Fail? → Clock 9 SCL pulses (bus recovery), retry
       └── Still fail? → increment mpuConsecutiveFailures
           └── ≥ 5? → mpuHealthy = false

2. I2C Bus 1: Probe BMP280 address (0x76)
   └── Fail? → bmpAvailable = false

3. If MPU6050 was offline, re-probe
   └── Recovered? → Re-initialize (reg 0x6B = 0x00),
       recalibrate, switch back to SENSOR_MPU6050

4. GPS fix tracking:
   └── No fix for > 1 hour? → WARNING:GPS_NO_FIX_1HR
```

**I2C bus recovery** (`i2cBusRecovery()`):
```
1. Set SDA as INPUT_PULLUP
2. Set SCL as OUTPUT
3. Clock 9 pulses on SCL (release stuck slave)
4. Generate STOP condition (SDA LOW → HIGH while SCL HIGH)
5. Re-initialize I2C bus at 100kHz
```



## 17. Failure Detection & Diagnostics System

The system implements three categories of failure detection, each with autonomous on-device detection and server-coordinated diagnostics.

### Architecture Overview

```
┌─────────────────────────────────────────────────────────────────┐
│                FAILURE DETECTION ARCHITECTURE                    │
│                                                                 │
│  ALWAYS ACTIVE (on-device, every loop):                        │
│  ├── A1: MPU6050 latch-up / frozen output detection            │
│  ├── A2: MPU6050 drift / gravity magnitude check               │
│  ├── A3: BMP280 range check / staleness / cross-validation     │
│  ├── B1: Battery voltage monitoring & staged shutdown          │
│  ├── B2: Watchdog timer (MCU crash recovery)                   │
│  ├── B3: SIM800L communication health                          │
│  ├── C1-PASSIVE: GPS origin drift detection                    │
│  └── C2-PASSIVE: Draft pressure trend (leak detection)         │
│                                                                 │
│  SERVER-TRIGGERED (activate via GSM command):                  │
│  ├── C1-FULL: Tether break diagnostic suite                    │
│  │   ├── Oscillation period analysis                           │
│  │   ├── Lateral acceleration sustained-slack check            │
│  │   └── GPS displacement + heading analysis                   │
│  ├── C3: Anchor shift detection (L_eff comparison)             │
│  └── FULL_DIAG: Run all diagnostics, report to server          │
│                                                                 │
│  CONTINUOUS SELF-CALIBRATION:                                  │
│  ├── Atmospheric pressure baseline (while at surface)          │
│  ├── Gravity reference correction (in SLACK mode)              │
│  └── Draft baseline tracking (biofouling/leak detection)       │
└─────────────────────────────────────────────────────────────────┘
```

---

### 17.1 Origin Coordinate System — GPS Geofence

At deployment, the system stores the GPS position as the **anchor origin**. Every GPS fix is compared to this origin. If the buoy drifts beyond a configurable radius (default = OLP length + margin), the system flags a potential detachment.

```
DEPLOYMENT:
  Store: originLat, originLon (from first valid GPS fix after CAL)
  Store: geofenceRadiusM = olpLength / 100.0 (convert cm to meters)
         + GEOFENCE_MARGIN_M (default 5.0 meters for GPS error)

                    Geofence boundary
                   ╱                  ╲
                 ╱     Safe zone        ╲
               ╱    (buoy can reach      ╲
              │      anywhere on arc)      │
              │                            │
              │        ● Origin            │
              │     (anchor GPS)           │
               ╲                          ╱
                 ╲                      ╱
                   ╲__________________╱
                   
                   radius = L + margin

EVERY GPS FIX:
  distance = distanceFromOrigin(currentLat, currentLon,
                                originLat, originLon)
  
  IF distance > geofenceRadiusM:
    geofenceBreachCount++
    
    IF geofenceBreachCount >= 3 (consecutive):
      → FLAG: TETHER_DETACHMENT_SUSPECTED
      → Include in next HTTP POST to server
      → Server dispatches alert to all tiers:
        "VARUNA ALERT: Buoy may be detached.
         Distance from anchor: [X]m
         Position: [lat],[lon]
         Heading: [bearing]° from origin"
      → Increase GPS fix rate to continuous
      → Begin logging position trail
      → Set flag: tetherBreachDetected = true
      → Server can issue DIAG_TETHER command for full analysis
          
  ELSE:
    geofenceBreachCount = 0
```

**Distance calculation (flat-earth approximation for short distances):**
```
For distances < 10km, simplified formula is sufficient:

  dLat = (currentLat - originLat) × 111320.0   (meters)
  dLon = (currentLon - originLon) × 111320.0 × cos(originLat × π/180)
  distance = sqrt(dLat² + dLon²)               (meters)

Accuracy: ±0.1% at equator, ±0.5% at 60° latitude
More than sufficient for 1-5 meter geofence detection
```

**Origin coordinate persistence:**
```
EEPROM storage:
  EEPROM_ORIGIN_LAT_ADDR    (4 bytes, float)
  EEPROM_ORIGIN_LON_ADDR    (4 bytes, float)
  EEPROM_ORIGIN_MAGIC_ADDR  (1 byte, 0xGF)
  EEPROM_GEOFENCE_RADIUS    (4 bytes, float, meters)

Set at deployment using handheld debugger:
  SETORIGIN              (capture current GPS as origin)
  SETORIGIN:<lat>,<lon>  (manual coordinates)
  SETFENCE:<meters>      (set geofence radius)

Queryable via console diagnostic:
  GETCONFIG reports origin and fence values
  GPSFIX reports distance from origin
```

---

### 17.2 Category A — Sensor Failures

#### A1: MPU6050 Latch-Up (Frozen Output)

```
WHAT HAPPENS:
  Electrical noise or moisture causes I2C bus lock.
  MPU6050 returns identical values every read, or 0xFF on all bytes.

DETECTION (runs every loop iteration):

  Maintain variance buffer of last 50 accelerometer readings:
  
  accelVarianceBuffer[50]   ← rolling window of |a| magnitudes
  
  Every 50 readings:
    variance = computeVariance(accelVarianceBuffer, 50)
    
    A floating buoy in water ALWAYS has micro-motion:
    thermal drift, micro-currents, wind ripple.
    Variance should NEVER be exactly zero.
    
    IF variance < 0.0001 m/s²:
      mpuFrozenCount++
      
      IF mpuFrozenCount >= 3 (consecutive variance windows):
        → FLAG: MPU6050_FROZEN
        → Attempt recovery:
           1. I2C bus recovery (9 SCL clocks)
           2. MPU6050 power cycle (reg 0x6B = 0x80, then 0x00)
           3. Re-read WHO_AM_I register
           4. If WHO_AM_I ≠ 0x68 → I2C_BUS_FAULT
           5. If WHO_AM_I == 0x68 but still frozen → MPU6050_HARDWARE_FAULT
        → Report to serial: "FAULT:MPU6050_FROZEN:RECOVERY_[OK/FAIL]"
        → If recovery fails → mpuHealthy = false
        
    ELSE:
      mpuFrozenCount = 0

  ALSO — I2C register validation (every 60 seconds):
    whoami = mpuReadReg(0x75)
    IF whoami ≠ 0x68 AND whoami ≠ 0x72:
      → I2C_BUS_CORRUPTION
      → Full bus recovery sequence
```

#### A2: MPU6050 Drift Beyond Correction

```
WHAT HAPPENS:
  Temperature change shifts accelerometer offset.
  Gyro bias drifts faster than complementary filter corrects.

DETECTION:

  Gravity magnitude check (every reading):
    g_measured = sqrt(ax² + ay² + az²)
    
    IF |g_measured - 9.81| > 1.0 m/s²:
      gravityErrorCount++
      
      IF gravityErrorCount >= 20 (consecutive):
        → FLAG: ACCEL_CALIBRATION_DRIFT
        → Check context: is buoy under extreme acceleration?
           (wave impact, collision — transient, should clear)
        → If sustained > 5 seconds:
           → ACCEL_HARDWARE_FAULT or ORIENTATION_LOST
           
    ELSE:
      gravityErrorCount = 0

  SLACK mode auto-correction (continuous self-calibration):
    IF currentMode == MODE_0_SLACK:
      Buoy is floating upright, no tether tension.
      Accelerometer SHOULD read: ax ≈ 0, ay ≈ 0, az ≈ 9.81
      
      IF |az - 9.81| > 0.5 OR |ax| > 0.5 OR |ay| > 0.5:
        → FLAG: ACCEL_OFFSET_DRIFT
        → Auto-correct offsets:
           accelOffsetX = -ax_measured
           accelOffsetY = -ay_measured
           accelOffsetZ = 9.81 - az_measured
        → Apply to all future readings
        → Log: "AUTOCAL:ACCEL_OFFSET_CORRECTED"
        → This is SELF-CALIBRATION, not a fault
        
      This runs only in MODE 0 because that is the only mode
      where the buoy's orientation is known a priori (upright).
```

#### A3: BMP280 Pressure Sensor Failure

```
WHAT HAPPENS:
  Water intrusion destroys sensor element.
  Sensor reads 0, maximum value, or constant stuck value.

DETECTION (runs every BMP280 read, every 5 seconds):

  Range check:
    IF currentPressure < 300 hPa OR currentPressure > 1200 hPa:
      → Atmospheric pressure on Earth: 870-1084 hPa
      → Even at 10m depth: P < 2000 hPa
      → FAULT: PRESSURE_OUT_OF_RANGE
      → bmpAvailable = false

  Staleness check:
    Maintain pressureVarianceBuffer[20]
    
    IF pressureVariance < 0.01 hPa over 20 readings:
      → Even in calm conditions, pressure fluctuates
         from micro-waves, wind, thermal changes
      → FAULT: PRESSURE_SENSOR_FROZEN
      → Attempt: re-initialize BMP280
      → If still frozen: bmpAvailable = false

  Cross-validation with tilt (MODE 1, tether TAUT):
    IF currentMode == MODE_1_TAUT AND theta > 20°:
      Buoy is significantly tilted → definitely at surface.
      Pressure SHOULD be near atmospheric baseline.
      
      IF currentPressure > baselinePressure + 50 hPa:
        → Pressure says deeply submerged
        → Tilt says clearly at surface
        → FAULT: PRESSURE_TILT_DISAGREEMENT
        → Trust tilt (MPU6050), flag pressure as suspect
        → Report: "FAULT:PRESSURE_CROSS_CHECK_FAIL"
        
    IF currentMode == MODE_3_SUBMERGED AND theta > 15°:
      → Pressure says submerged, tilt says angled (surface)
      → Contradiction → one sensor is wrong
      → Apply confidence weighting:
         If pressure changed gradually → trust pressure
         If pressure jumped suddenly → suspect pressure fault
```

---

### 17.3 Category B — System Failures

#### B1: Battery Death (Staged Shutdown)

```
Handled by readBatteryLevel() with piecewise Li-ion curve.
(See Section 6 for voltage-to-percent mapping)

STAGED RESPONSE:

  VOLTAGE     PERCENT    STATE         ACTION
  ─────────   ───────    ─────         ──────────────────────────
  > 3.85V     > 70%      GOOD          Normal operation
  
  3.50-3.85V  20-70%     OK            Normal, report in telemetry
  
  3.30-3.50V  5-20%      LOW           • Extend sample intervals
                                       • Reduce GPRS to conserve mode
                                       • Reduce obstruction light
                                         duty cycle
                                       • Report LOW in HTTP POST
                                       
  3.00-3.30V  0-5%       CRITICAL      • Emergency mode:
                                         measure + transmit 1×/hour
                                       • Disable obstruction lights
                                       • Force EEPROM save
                                       • Report CRITICAL in HTTP POST
                                       
  < 3.00V     0%         CUTOFF        • Force save all state
                                       • Deep sleep (no wake timer)
                                       • Protects cells from damage
                                       
  3 consecutive readings below cutoff required to prevent
  transient voltage dips from triggering false shutdown.
```

#### B2: MCU Crash / Lockup

```
HANDLED BY: ESP32 hardware watchdog timer

  Setup: WDT_TIMEOUT_SEC = 120 seconds
  Initially configured at 180 seconds during setup() (extended
  for sensor initialization and SIM800L boot), then reconfigured
  to 120 seconds for operational mode.
  
  Every loop iteration: esp_task_wdt_reset()
  During long operations (AT commands, SMS send): reset inside loops
  
  IF loop() hangs for > 120 seconds:
    → Hardware watchdog triggers panic
    → ESP32 performs hard reset
    → System reboots
    → setup() runs again
    → EEPROM warm boot restores state (if < 30 min old)
    → System resumes monitoring
  
  AFTER REBOOT — detect reason:
    esp_reset_reason_t reason = esp_reset_reason()
    
    IF reason == ESP_RST_TASK_WDT OR reason == ESP_RST_WDT:
      watchdogResetCount++  (stored in RTC memory, survives reset)
      
      Report: "STATUS:WATCHDOG_RECOVERY:COUNT=[N]"
      
      IF watchdogResetCount > 10 (in 24 hours):
        → FAULT: FREQUENT_CRASHES
        → Report in HTTP POST health score
        → Server flags station for service
        → Possible causes: memory leak, I2C bus permanent
           fault, SIM800L hanging system
```

#### B3: SIM800L Communication Failure

```
  AT command timeout      → Retry up to 5 times
  No network registration → Periodic re-check every 30 seconds
  GPRS upload failure     → 3 fails → flag GPRS_OFFLINE
  Signal lost             → Periodic re-registration check
  Module unresponsive     → Hardware reset (RST pin LOW 200ms)
  
  SERVER-SIDE MONITORING:
    Server expects GPRS heartbeat at transmitInterval.
    IF no data received for 3× transmitInterval:
      → Server flags buoy as SILENT on dashboard
      → Server can attempt SMS diagnostic command to buoy
      → If buoy responds → GPRS path broken, SMS path works
      → If no response → buoy may be offline or out of range
```

---

### 17.4 Category C — Physical Failures

#### C1: Tether Break Detection

**Two-tier detection: passive (always on) + active (server-triggered)**

```
TIER 1 — PASSIVE (always active, no server command needed):

  GPS GEOFENCE CHECK (every valid GPS fix):
  
    distance = distanceFromOrigin(currentLat, currentLon,
                                  originLat, originLon)
    
    IF distance > geofenceRadiusM:
      geofenceBreachConsecutive++
      
      IF geofenceBreachConsecutive >= 3:
        → ALERT: TETHER_DETACHMENT_SUSPECTED
        → Flag in next HTTP POST to server
        → Server dispatches:
           "VARUNA CRITICAL: Buoy detached!
            Dist from anchor: [X]m
            Position: [lat],[lon]
            Moving: [bearing]° at [speed]km/h"
        → Begin continuous GPS logging (position trail)
        → Set flag: tetherBreachDetected = true
        → Server can issue DIAG_TETHER for full analysis
          
    ELSE:
      geofenceBreachConsecutive = 0


TIER 2 — ACTIVE DIAGNOSTICS (triggered by server command):

  Server sends SMS: "DIAG_TETHER" to buoy
  
  Buoy executes full tether diagnostic suite:

  ┌─────────────────────────────────────────────────────┐
  │  TETHER BREAK DIAGNOSTIC SUITE                      │
  │                                                     │
  │  TEST 1: Sustained Slack Check                      │
  │  ──────────────────────────                         │
  │  Monitor lateral_accel for 5 minutes:               │
  │                                                     │
  │  IF lateral_accel < 0.10 m/s² sustained > 5 min    │
  │  AND currentMode == MODE_0_SLACK:                   │
  │    → Buoy has NO tether tension for extended period │
  │    → In flowing water, this should not happen       │
  │    → RESULT: TETHER_TENSION_ABSENT                  │
  │                                                     │
  │  TEST 2: Oscillation Period Analysis                │
  │  ──────────────────────────────────                 │
  │  Attempt to measure pendulum oscillation:           │
  │  Sample tilt angle at 10 Hz for 60 seconds.         │
  │  Compute FFT or zero-crossing period.               │
  │                                                     │
  │  IF no periodic oscillation detected:               │
  │    oscillationPeriod = UNDEFINED (∞)                │
  │    → No pendulum restoring force                    │
  │    → RESULT: NO_OSCILLATION_DETECTED                │
  │                                                     │
  │  IF oscillation detected:                           │
  │    T = measured period                              │
  │    L_eff = g × (T/2π)²                             │
  │    Compare to L_KNOWN (olpLength)                   │
  │    IF |L_eff - L_KNOWN| < 15%:                     │
  │      → RESULT: TETHER_INTACT (oscillation matches) │
  │    ELSE:                                            │
  │      → RESULT: TETHER_LENGTH_ANOMALY               │
  │                                                     │
  │  TEST 3: GPS Displacement Analysis                  │
  │  ────────────────────────────────                   │
  │  Record GPS position every 10 seconds for 5 min.   │
  │  Compute: total displacement, heading, speed.       │
  │                                                     │
  │  IF displacement > geofenceRadiusM:                 │
  │    → Buoy has moved beyond tether reach             │
  │    → RESULT: BUOY_DISPLACED_BEYOND_TETHER           │
  │                                                     │
  │  IF speed > 0.5 m/s sustained AND no oscillation:  │
  │    → Buoy moving freely with current               │
  │    → RESULT: BUOY_DRIFTING_FREE                     │
  │                                                     │
  │  COMPOSITE VERDICT:                                 │
  │  ──────────────────                                 │
  │  IF (TENSION_ABSENT OR NO_OSCILLATION)             │
  │  AND (DISPLACED OR DRIFTING):                       │
  │    → VERDICT: TETHER_BROKEN (high confidence)      │
  │                                                     │
  │  IF TENSION_ABSENT but NOT DISPLACED:               │
  │    → VERDICT: TETHER_POSSIBLY_BROKEN                │
  │    → (could be very calm water with no current)     │
  │                                                     │
  │  IF oscillation matches AND within geofence:        │
  │    → VERDICT: TETHER_INTACT                         │
  │                                                     │
  │  Report full results via SMS reply + GPRS upload    │
  └─────────────────────────────────────────────────────┘
```

**SMS diagnostic reply format:**
```
"VARUNA TETHER DIAG:
 Tension: [PRESENT/ABSENT]
 Oscillation: [T=Xs/NONE]
 L_eff: [X]cm vs L=[Y]cm
 GPS dist: [X]m from origin
 Speed: [X]m/s
 Verdict: [INTACT/BROKEN/UNKNOWN]"
```

---

#### C2: Buoy Leak / Water Ingress Detection

**Always active — passive monitoring**

```
DRAFT PRESSURE TREND MONITORING:

  Track the pressure reading when buoy is at surface
  (MODE 0 or MODE 1) as "draft pressure":
  
  draftPressure = currentPressure - baselinePressure
  
  This represents how deep the buoy sits in the water.
  As water enters the capsule, mass increases, buoy sits lower,
  draft pressure increases.

  Maintain 24-hour draft history:
    draftHistory[24]   ← one reading per hour (hourly average)
    draftHistoryIdx    ← circular index
    draftHistoryCount  ← entries filled
  
  Every hour (when buoy is at surface):
    Store current average draft pressure in draftHistory[]
    
    IF draftHistoryCount >= 24:
      draftRate = (draftCurrent - draft24hAgo) / 24.0
      (hPa per hour)
      
      Biofouling:  draftRate ≈ 0.001-0.01 hPa/hr (weeks to show)
      Slow leak:   draftRate ≈ 0.05-0.5 hPa/hr (days to show)
      Fast leak:   draftRate > 0.5 hPa/hr (hours to concern)

      IF draftRate > 0.5 hPa/hr:
        → ALERT: POSSIBLE_LEAK
        → Flag in HTTP POST health score
        → Server dispatches: "VARUNA: Draft increasing [X] hPa/hr.
                Possible water ingress. Inspect buoy."
                
      IF draftRate > 2.0 hPa/hr:
        → ALERT: ACTIVE_LEAK
        → Flag in HTTP POST as critical health issue
        → Server dispatches: "VARUNA CRITICAL: Rapid draft increase.
                Active water ingress suspected.
                Retrieve buoy immediately."

  CROSS-CHECK — Sinking Detection:
    IF pressure increasing (buoy going deeper)
    AND currentMode == MODE_1_TAUT (tether taut)
    AND theta NOT approaching 0° (water not actually rising):
      → Pressure up + tilt unchanged = buoy sinking, not flood
      → ALERT: BUOY_SINKING
      → Server dispatches: "VARUNA CRITICAL: Buoy sinking without
              water rise. Water ingress confirmed. Retrieve immediately."

  OPTIONAL HUMIDITY SENSOR (DHT11 inside capsule):
    IF available:
      Read every 5 minutes
      IF humidity > 80%:
        → ALERT: MOISTURE_DETECTED
      IF humidity > 95%:
        → ALERT: MOISTURE_CRITICAL
        → Possible condensation on electronics
```

---

#### C3: Anchor Shift Detection

**Server-triggered diagnostic**

```
Server sends SMS: "DIAG_ANCHOR" to buoy

Buoy executes anchor position analysis:

  ┌─────────────────────────────────────────────────────┐
  │  ANCHOR SHIFT DIAGNOSTIC                            │
  │                                                     │
  │  TEST 1: Effective Tether Length (L_eff)            │
  │  ─────────────────────────────────────              │
  │  Measure oscillation period T over 60 seconds.      │
  │  L_eff = g × (T / 2π)²                             │
  │                                                     │
  │  Compare to L_KNOWN:                                │
  │    IF L_eff < L_KNOWN - 10%:                       │
  │      → Tether effectively shortened                 │
  │      → Possible: sediment burial of anchor/base     │
  │      → RESULT: TETHER_SHORTENED                     │
  │                                                     │
  │    IF L_eff > L_KNOWN + 10%:                       │
  │      → Tether effectively lengthened                │
  │      → Possible: anchor dragged, tether stretched   │
  │      → RESULT: TETHER_LENGTHENED                    │
  │                                                     │
  │    IF |L_eff - L_KNOWN| < 10%:                     │
  │      → RESULT: TETHER_LENGTH_NORMAL                 │
  │                                                     │
  │  TEST 2: Equilibrium Angle Shift                    │
  │  ─────────────────────────────                      │
  │  In TAUT mode, record average θ over 5 minutes.    │
  │  Compare to historical average at similar water      │
  │  levels (from archived data).                        │
  │                                                     │
  │  IF θ_current differs from θ_historical by > 10°   │
  │  at similar water level:                             │
  │    → Anchor position has changed                    │
  │    → RESULT: EQUILIBRIUM_SHIFTED                    │
  │                                                     │
  │  TEST 3: GPS Centroid Shift                         │
  │  ──────────────────────                             │
  │  Record GPS positions over 10 minutes.              │
  │  Compute centroid (average position).               │
  │  Compare to historical centroid.                     │
  │                                                     │
  │  IF centroid shifted > 2m from deployment centroid: │
  │    → Anchor has moved laterally                     │
  │    → RESULT: ANCHOR_DISPLACED                       │
  │                                                     │
  │  COMPOSITE REPORT:                                  │
  │    L_eff vs L_KNOWN, equilibrium angle,             │
  │    GPS centroid shift, recommended action            │
  └─────────────────────────────────────────────────────┘

SMS diagnostic reply:
"VARUNA ANCHOR DIAG:
 L_eff: [X]cm (known: [Y]cm, diff: [Z]%)
 Eq.angle: [X]° (historical: [Y]°)
 Centroid shift: [X]m
 Verdict: [NORMAL/SEDIMENT/DRAGGED]"
```

---

### 17.5 Server-Triggered Diagnostic Commands

These commands are sent from the server (or an authorized field engineer phone) via SMS. The buoy executes the diagnostic suite and replies with results.

```
DIAGNOSTIC COMMANDS (via SMS from authorized number):

  DIAG_TETHER        Full tether integrity check
                     (5 minutes of data collection)
                     → Reply: tension, oscillation, GPS, verdict

  DIAG_ANCHOR        Anchor position analysis
                     (10 minutes of data collection)
                     → Reply: L_eff, angle shift, centroid, verdict

  DIAG_LEAK          Force immediate leak/draft analysis
                     (uses existing 24h draft history)
                     → Reply: draft rate, humidity, verdict

  DIAG_SENSORS       Full sensor health report
                     → Reply: MPU6050 status, BMP280 status,
                       variance readings, gravity magnitude,
                       cross-validation results

  DIAG_ALL           Run ALL diagnostics sequentially
                     (≈20 minutes total)
                     → Reply: comprehensive health report
                     → Also uploaded via GPRS as JSON

  DIAG_ABORT         Cancel running diagnostic
                     (if taking too long or conditions changed)

TIMING:
  DIAG_TETHER:  ~5 minutes  (oscillation + GPS sampling)
  DIAG_ANCHOR:  ~10 minutes (longer GPS averaging)
  DIAG_LEAK:    ~10 seconds (reads existing history)
  DIAG_SENSORS: ~30 seconds (variance computation)
  DIAG_ALL:     ~20 minutes (sequential, all above)

DURING DIAGNOSTICS:
  Normal flood monitoring CONTINUES
  Diagnostics run in parallel using existing sensor data
  Flood alerts are NOT suppressed during diagnostics
  Watchdog is fed during long diagnostic operations
```

---

### 17.6 Continuous Self-Calibration

These calibrations run automatically without server commands or manual intervention.

```
┌─────────────────────────────────────────────────────────────────┐
│  AUTO-CALIBRATION 1: Atmospheric Pressure Baseline              │
│  ──────────────────────────────────────────────                 │
│  WHEN: Buoy confirmed at surface (MODE 0, 1, or 2)            │
│  HOW:  Rolling 48-sample average, updated every 30 min         │
│  WHY:  Weather fronts shift P by ±2-4 hPa over hours          │
│        Must track this to detect submersion accurately          │
│  STATUS: ✅ Implemented (baselinePressure)                      │
│                                                                 │
│  AUTO-CALIBRATION 2: Gravity Reference Vector                  │
│  ──────────────────────────────────────────────                 │
│  WHEN: MODE 0 (SLACK — buoy floating upright freely)           │
│  HOW:  Accelerometer should read (0, 0, 9.81)                 │
│        Any deviation = accumulated offset drift                 │
│        Correct: offset = expected - measured                    │
│  WHY:  Temperature changes shift MEMS accel offset             │
│        by 0.5-2 mg/°C (0.005-0.02 m/s²)                       │
│  INTERVAL: Every time MODE 0 persists > 60 seconds             │
│                                                                 │
│  AUTO-CALIBRATION 3: Tether Length Verification                │
│  ──────────────────────────────────────────────                 │
│  WHEN: MODE 1 (TAUT — buoy oscillating on tether)             │
│  HOW:  Measure pendulum oscillation period T                   │
│        L_eff = g × (T/2π)²                                    │
│        Compare to olpLength                                    │
│  WHY:  Detects sediment burial, tether stretch,                │
│        anchor shift without manual measurement                  │
│  INTERVAL: Every 10 minutes while in MODE 1                    │
│  ALERT: If |L_eff - olpLength| > 15% for > 1 hour            │
│                                                                 │
│  AUTO-CALIBRATION 4: Draft Baseline                            │
│  ──────────────────────────────────────────────                 │
│  WHEN: Buoy at surface (MODE 0 or 1)                          │
│  HOW:  Track pressure offset from atmospheric                   │
│        (represents how deep buoy sits = draft)                 │
│        Slow upward drift = biofouling or water ingress         │
│  INTERVAL: Hourly average stored for 24 hours                  │
│  ALERT: Rate > 0.5 hPa/hr → possible leak                     │
│                                                                 │
│  CANNOT AUTO-CALIBRATE (requires manual):                      │
│  ──────────────────────────────────────────                     │
│  ❌ Absolute water level (need staff gauge)                     │
│  ❌ Absolute current speed (need flow meter)                    │
│  ❌ Physical tether length (need tape measure)                  │
│  ❌ Origin GPS coordinates (set at deployment)                  │
│  These require manual verification quarterly.                   │
└─────────────────────────────────────────────────────────────────┘
```

---

### 17.7 Health Score — Composite System Health Metric

All failure detections feed into a single health score reported in telemetry:

```
HEALTH SCORE (0-100):

  Component              Weight    Score
  ─────────────────────  ──────    ─────
  MPU6050 status         20 pts    20 = healthy, 10 = recovering,
                                   0 = offline
  BMP280 status          15 pts    15 = healthy, 0 = offline
  Battery level          15 pts    15 = >70%, 10 = 20-70%,
                                   5 = 5-20%, 0 = <5%
  GPS fix quality        10 pts    10 = fix+HDOP<3, 5 = fix+HDOP>3,
                                   0 = no fix
  SIM/communication      10 pts    10 = registered+RSSI>10,
                                   5 = registered+weak,
                                   0 = unregistered
  Tether integrity       15 pts    15 = confirmed intact,
                                   10 = not recently checked,
                                   0 = breach detected
  Buoy integrity         10 pts    10 = no leak indicators,
                                   5 = slow draft increase,
                                   0 = active leak suspected
  Sensor agreement       5 pts     5 = cross-checks pass,
                                   0 = disagreements detected

  Total: sum of components

  REPORTING:
    Score 80-100: GREEN  — "System healthy"
    Score 60-79:  YELLOW — "Degraded, monitoring continues"
    Score 40-59:  ORANGE — "Significant issues, service recommended"
    Score 20-39:  RED    — "Critical degradation, service required"
    Score 0-19:   BLACK  — "System failure, data unreliable"

  Health score included in:
    • Every GPRS data upload (JSON field "health")
    • Every serial CSV output (field 39)
    • SMS STATUS reply
    • DIAG_ALL report
```

---

### 17.8 Field Calibration Procedure

#### When to Calibrate

```
EVENT                     CALIBRATION NEEDED
━━━━━━━━━━━━━━━━━━━━      ━━━━━━━━━━━━━━━━━━━━━━━━━━━━
Initial deployment        FULL calibration (all parameters)
After maintenance         Verification calibration
After flood event         Verification (anchor may have moved)
Quarterly check           Spot verification
After firmware update     Sensor recalibration
After battery replacement IMU recalibration (CoM may shift)
Seasonal (temp change)    Pressure baseline update
```

#### Full Calibration Protocol (Initial Deployment)

```
STEP 1: DRY CALIBRATION (Before deployment, in lab/workshop)
━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━

  1a. IMU CALIBRATION (via handheld debugger)
      • Place buoy on flat, level surface
      • Power on, wait 30 seconds for stabilization
      • Issue CAL command from handheld debugger
      • Gyro: 1000 samples averaged for offset
      • Accel: 500 samples averaged for reference gravity
      • Store offsets

  1b. PRESSURE SENSOR CALIBRATION
      • Record pressure reading in open air
      • Compare to known local atmospheric pressure
        (from weather service or calibrated barometer)
      • Store offset in EEPROM if needed

  1c. PHYSICAL MEASUREMENTS
      • Measure and record:
          L (tether length) with tape measure ± 1 cm
          Capsule dimensions ± 1 mm
          Total buoy mass with batteries ± 1 g
      • Set OLP length via handheld debugger: OLP:<cm>

  1d. BUOYANCY VERIFICATION
      • Place buoy in tank of water
      • Verify it floats (if it sinks, add flotation)
      • Measure actual draft (waterline position) with ruler

  1e. TETHER LENGTH VERIFICATION
      • Stretch tether to full length under ~5N tension
      • Measure with tape measure
      • Record L_verified ± 0.5 cm


STEP 2: WET CALIBRATION (At deployment site)
━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━

  2a. REFERENCE WATER LEVEL
      • Install temporary staff gauge near deployment point
      • Read water level: H_reference
      • OR: measure from fixed point down to water surface

  2b. DEPLOY AND VERIFY
      • Anchor the buoy
      • Wait 5 minutes for stabilization
      • System reports: H_system = L × cos(θ)
      • Compare: H_error = H_system - H_reference
      • |H_error| < 5 cm → PASS ✅
      • |H_error| 5-15 cm → ADJUST calibration offset
      • |H_error| > 15 cm → INVESTIGATE

  2c. ORIGIN COORDINATES
      • Issue SETORIGIN command from handheld debugger
      • Captures current GPS as anchor origin
      • Sets geofence radius = L + 5m margin

  2d. OSCILLATION BASELINE
      • Gently push buoy sideways and release
      • System records oscillation period
      • L_eff computed and compared to L_KNOWN
      • |L_eff - L_KNOWN| < 15% → PASS ✅

  2e. P_atm BASELINE
      • While buoy floating at surface:
      • Record P_atm_tracked value
      • Compare to weather service P_atm

  2f. DOCUMENT DEPLOYMENT
      Record and store (in EEPROM + written log):
      • Date, time, GPS coordinates
      • L_KNOWN, H_reference at deployment
      • All calibration offsets
      • Battery voltage at deployment
      • Firmware version
      • Anchor type and installation method
      • River name and station identifier
```

#### Verification Calibration (Quarterly)

```
TIME: ~30 minutes on site

1. READ STAFF GAUGE (or measure water level manually)
   H_manual = ___ m

2. READ SYSTEM OUTPUT (via diagnostic command or console)
   H_system = ___ m

3. COMPARE
   Error = H_system - H_manual
   |Error| < 5 cm   → PASS ✅  System is accurate
   |Error| 5-15 cm  → WARNING ⚠️  Recalibrate offset
   |Error| > 15 cm  → FAIL ❌  Investigate

4. CHECK TETHER
   • Visual inspection: tether intact, no debris
   • Tug test: firm connection at buoy and anchor
   • Verify tether length hasn't changed if possible

5. CHECK BUOY
   • Visual: biofouling level? Damage? Waterline position?
   • Compare waterline to deployment photo
   • Wipe excessive growth if present

6. CHECK SYSTEM HEALTH
   • Issue BATT command: battery voltage?
   • Issue FLOODSTATUS: health score?
   • Check GPRS connectivity: NETTEST
   • Review any alerts in server log

7. RECORD
   • Log all readings and observations
   • Photo of buoy and staff gauge
   • Update calibration offset if needed
```

---



# Part 2: Firmware Execution, Algorithms, Commands, and Data Formats

---

## 18. The Complete Decision Matrix — Core Intelligence

The `lookupDecisionMatrix()` function is the brain of the flood detection. Here is the actual 3D matrix as coded:

```
                    NOT Sustained                    Sustained Rise
                    ─────────────                    ──────────────
              SLOW      MODERATE    FAST       SLOW      MODERATE    FAST
            ┌──────────────────────────────┬──────────────────────────────┐
   NORMAL   │ NORMAL    NORMAL    NORMAL   │ NORMAL     WATCH     WATCH  │
            ├──────────────────────────────┼──────────────────────────────┤
   ALERT    │ WATCH      WATCH    WATCH    │ WATCH     WARNING   WARNING │
            ├──────────────────────────────┼──────────────────────────────┤
   WARNING  │ WARNING   WARNING    FLOOD   │ FLOOD      FLOOD   CRITICAL│
            ├──────────────────────────────┼──────────────────────────────┤
   DANGER   │ FLOOD    CRITICAL  CRITICAL  │ CRITICAL  CRITICAL CRITICAL│
            └──────────────────────────────┴──────────────────────────────┘
              Zone ↑     Rate →              Zone ↑     Rate →

Implementation:
static const int matrix[4][3][2] = {
    // [zone][rate_index][sustained?]
    // Zone NORMAL:
    {{NORMAL,NORMAL}, {NORMAL,WATCH},  {NORMAL,WATCH}},
    // Zone ALERT:
    {{WATCH, WATCH},  {WATCH, WARNING},{WATCH, WARNING}},
    // Zone WARNING:
    {{WARNING,FLOOD}, {WARNING,FLOOD}, {FLOOD, CRITICAL}},
    // Zone DANGER:
    {{FLOOD,CRITICAL},{CRITICAL,CRITICAL},{CRITICAL,CRITICAL}}
};
```

**Critical design insight:** RATE_FALLING is mapped to RATE_SLOW — the system treats falling water the same as slowly rising water for the matrix lookup, but uses the falling flag separately in step-down logic.

---

## 19. The Step-Down (De-escalation) Algorithm

This is the **most sophisticated part** of the flood logic, preventing false all-clears:

```
evaluateStepDown(currentLevel, matrixLevel, waterHeight, rate, sustained, time)
        │
        ├── matrixLevel > currentLevel? → ESCALATE IMMEDIATELY
        │                                  (reset stepDownConsecutive)
        │
        ├── matrixLevel == currentLevel? → HOLD (reset counter)
        │
        └── matrixLevel < currentLevel? → CHECK STEP-DOWN CONDITIONS
                │
                ├── CRITICAL→down: water < dangerLevel AND rate FALLING/SLOW
                ├── FLOOD→down:    water < warningLevel
                ├── WARNING→down:  water < alertLevel
                ├── WATCH→down:    water < alertLevel AND rate FALLING/SLOW
                │                  AND NOT sustained
                │
                └── Condition met?
                    ├── YES: increment stepDownConsecutive
                    │        ├── count ≥ 4 (or 8 for WATCH→NORMAL)?
                    │        │   AND time gate satisfied?
                    │        │   ├── YES: DROP ONE LEVEL (not to matrix level!)
                    │        │   └── NO:  HOLD current level
                    │        │
                    │        └── (accumulate evidence)
                    │
                    └── NO:  RESET counter, HOLD current level
```

**Key safety features:**
1. **One level at a time** — Can't jump from CRITICAL to NORMAL; must pass through FLOOD → WARNING → WATCH → NORMAL
2. **Consecutive readings** — 4 readings must ALL show lower conditions (not just one good reading)
3. **8 readings for WATCH→NORMAL** — Extra caution before declaring all-clear
4. **Time gates** — Minimum 15–30 minutes at each level before stepping down
5. **Asymmetric** — Escalation is instant; de-escalation is gradual

**After any level transition:**
- Level change recorded in state
- Included in next HTTP POST payload
- Server detects transition and handles notification
- EEPROM state saved via `forceSaveEeprom()`

---

## 20. Rate of Change Calculation

```
calculateRateOfChange(currentHeight, currentTime)

Guard conditions:
  ├── No previous reading? → Initialize, rate = 0
  ├── elapsed < 60 seconds? → Skip (too noisy for rate)
  └── Proceed:

  change = currentHeight - previousHeight  (cm)
  rateOfChange = change / elapsed           (cm/sec)
  ratePer15Min = change × (900 / elapsed)   (cm/15min, normalized)
  
  Clamp: |ratePer15Min| ≤ 200 cm/15min (reject sensor glitches)
  
  Update previous values for next calculation
```

**Why normalize to 15 minutes?** Flood forecasting standards typically report rise rates in cm/hour or cm/15min. The 15-minute window matches common hydrological practice and the system's own sustained-rise buffer window.

**Minimum elapsed time of 60 seconds** ensures the rate calculation is not dominated by sensor noise on closely-spaced readings.

---

## 21. Sustained Rise Detection — Detailed Logic

```
updateSustainedBuffer(waterHeight, timestamp)

Circular buffer: [h₀, h₁, h₂, h₃] ordered chronologically

Step 1: Add new reading to buffer
Step 2: If buffer not full (< 4 readings) → sustained = false

Step 3: Extract ordered sequence from circular buffer
Step 4: Check net rising: ordered[3] > ordered[0] + 0.5cm
Step 5: Count rising pairs:
        (h₁ > h₀ + 0.5) → +1
        (h₂ > h₁ + 0.5) → +1
        (h₃ > h₂ + 0.5) → +1

Step 6: sustainedRise = netRising AND (riseCount ≥ 2)
```

**Buffer flush on interval change:**

When sampling interval changes dramatically (e.g., from 30min to 2min during escalation), old readings at the long interval are invalidated. The function `flushSustainedBufferIfIntervalChanged()` detects when the old interval was ≥ 4× the new interval. In that case, the buffer is filled with the last reading to avoid comparing readings taken at vastly different time scales.

---

## 22. Override Mechanisms

The `evaluateFloodStatus()` function has several **override bypasses** that force `effectiveSustained = true`:

```
Override 1: Cold Start
  IF readingsSinceBoot ≤ 4 AND zone ≥ WARNING
  → Force sustained = true
  → Reason: Just booted into a flood; don't wait 4 readings

Override 2: Extreme Rate
  IF ratePer15Min > 30 cm/15min AND zone ≥ ALERT
  → Force sustained = true
  → Reason: Flash flood; 30cm/15min is ~2cm/minute

Override 3: Catastrophic Rate
  IF ratePer15Min > 50 cm/15min (any zone)
  → Force sustained = true
  → Reason: Dam break / tsunami; override everything
```

These overrides push the matrix lookup into the "sustained" column, which always returns equal or higher response levels. The elevated response level is transmitted to the server in the next HTTP POST, where it triggers the appropriate notification chain.

---

## 23. Cold Start Logic

When the system boots (or warm-boots into a flood):

```
FIRST READING (readingsSinceBoot == 1):

  IF zone == DANGER → Set CRITICAL immediately
  IF zone == WARNING → Set WARNING immediately
  ELSE → Stay NORMAL

  IF level ≥ WARNING → Response level set immediately
                        Next HTTP POST carries elevated level
                        Server detects cold-start escalation
                        and dispatches appropriate alerts
  
  Map response level to floodAlertLevel for LED:
    NORMAL/WATCH → 0
    WARNING      → 1
    FLOOD        → 2
    CRITICAL     → 3
  
  RETURN (skip normal evaluation for first reading)
```

This handles the scenario where the system reboots during an active flood — it doesn't wait for sustained rise detection; it responds immediately based on absolute water level.

---

## 24. Complete `setup()` Initialization Sequence

```
┌─ BOOT ────────────────────────────────────────────────────────────┐
│                                                                    │
│  1. Serial.begin(115200)                                          │
│  2. Configure LED (GPIO 3)                                        │
│  3. Watchdog: 180s timeout (extended for init)                    │
│                                                                    │
│  4. I2C Bus 0 (GPIO 8/9, 100kHz)                                 │
│  5. I2C Bus 1 (GPIO 4/5, 100kHz)                                 │
│  6. GPS UART1 (9600 baud, GPIO 6/7)                              │
│  7. SIM UART2 (9600 baud, GPIO 15/16)                            │
│                                                                    │
│  8. Algorithm toggle init (GPIO 12 button, GPIO 13 LED)           │
│     → Default: DISABLED (continuous data mode for bench testing)  │
│                                                                    │
│  9. C3 OTA interface (GPIO 0 = BOOT, EN = RESET)                 │
│     GPIO 0 and EN are controlled by the XIAO C3.                 │
│     During setup(), these pins are in normal state                │
│     (S3 booted normally, not in bootloader mode).                │
│     No initialization needed on the S3 side — the C3             │
│     controls both pins as outputs.                                │
│     GPIO 43/44: UART to C3 — idle during normal operation.       │
│     GPIO 14: CSV feed to C3 — begins output after setup().       │
│                                                                    │
│ 10. C3 display feed init (GPIO 14 SW-TX at 9600 baud)            │
│ 11. Obstruction light init (5 LEDs, restore EEPROM preset)       │
│                                                                    │
│ 12. EEPROM.begin(512)                                             │
│ 13. SPIFFS init (for data archival)                               │
│                                                                    │
│ 14. DS1307 RTC init on I2C Bus 1                                 │
│     ├── Convert 12hr→24hr if needed                               │
│     ├── Clear clock-halt bit                                      │
│     ├── Read time, validate (year ≥ 2024)                         │
│     └── Set bootUnixTime, sessionStartUnix                        │
│                                                                    │
│ 15. EEPROM warm boot restore                                     │
│     ├── Check magic byte (0xA5)                                   │
│     ├── Check age (< 30 minutes old?)                             │
│     ├── Restore: response level, sustained buffer, thresholds,    │
│     │   OLP length, peak height, previous height, state entry     │
│     └── OR cold boot: initialize all to defaults                  │
│                                                                    │
│ 16. Restore authorized field engineer numbers from EEPROM         │
│                                                                    │
│ 17. MPU6050 init                                                  │
│     ├── Wake (reg 0x6B = 0x00)                                   │
│     ├── Sample rate divider = 7 (1kHz/8 = 125Hz)                 │
│     ├── DLPF = 3 (44Hz bandwidth)                                │
│     ├── Gyro range = ±250°/s                                     │
│     ├── Accel range = ±2g                                         │
│     └── WHO_AM_I check (0x68 or 0x72)                            │
│                                                                    │
│ 18. Sensor source determination                                   │
│     ├── MPU healthy → Tilt measurement available (MODE 0-2)      │
│     ├── BMP available → Pressure measurement available (MODE 3)  │
│     ├── Both available → Full 4-mode operation                    │
│     ├── MPU only → Modes 0-2 only, no depth if submerged         │
│     ├── BMP only → Can detect safe/submerged, no intermediate    │
│     └── Neither → SENSOR_NONE, flag critical in health score     │
│                                                                    │
│ 19. BMP280 init on I2C Bus 1                                     │
│     ├── Check chip ID (0x58/0x56/0x57/0x60)                      │
│     ├── Read calibration coefficients (T1-T3, P1-P9)             │
│     ├── Configure: oversampling x4 temp, x16 press, normal mode  │
│     └── Take 10 readings → establish pressure baseline            │
│                                                                    │
│ 20. SIM800L init (GPRS data + diagnostic SMS)                    │
│     ├── Hardware reset                                            │
│     ├── AT commands (echo off, SMS text mode for diagnostics)     │
│     ├── SIM card check                                            │
│     ├── Quick network registration (5 attempts)                   │
│     ├── Signal strength                                           │
│     ├── Configure SMS direct delivery (diagnostic inbound)        │
│     └── NOTE: No contact lists loaded, no tiers configured        │
│           GPRS bearer initialized at first transmit interval      │
│                                                                    │
│ 21. MPU6050 CALIBRATION (if healthy)                              │
│     ├── 3-second LED blink (6 × 500ms) — visual warning          │
│     ├── Gyro calibration: 1000 samples @ 2ms = 2 seconds         │
│     │   → Average = gyroOffset{X,Y,Z}                            │
│     ├── Accel calibration: 500 samples @ 3ms = 1.5 seconds       │
│     │   → Filter: only accept |totalG| ∈ [0.9, 1.1]             │
│     │   → Average = refAcc{X,Y,Z}                                │
│     ├── Compute reference tilt angles:                            │
│     │   refTiltX = atan2(refAccY, √(refAccX² + refAccZ²))       │
│     │   refTiltY = atan2(-refAccX, √(refAccY² + refAccZ²))      │
│     └── Initialize complementary filter state                     │
│                                                                    │
│ 22. Initialize all timing variables to millis()                   │
│ 23. Set ADC to 12-bit resolution                                  │
│ 24. Initial battery reading                                       │
│ 25. Calculate initial adaptive intervals                          │
│ 26. Initialize stateEntryTime                                     │
│                                                                    │
│ 27. Reconfigure watchdog: 180s → 120s (operational timeout)       │
│ 28. LED ON (solid = system ready)                                 │
│ 29. Print all configuration to serial                             │
│ 30. Send initial data burst notification                          │
│                                                                    │
│  NOTE: processCommand() handles 33 commands in 5 categories:     │
│    Local (browser-only, never reach firmware)                     │
│    Diagnostics (read-only), Maintenance (state-changing),         │
│    Configuration (param-changing), Obstruction Lights             │
│    Installation params (OLP, CAL, ORIGIN, ALGO) are handled      │
│    by the handheld debugger firmware, not this command set        │
│                                                                    │
│  Total setup time: ~8-15 seconds (dominated by SIM800L init)     │
└───────────────────────────────────────────────────────────────────┘
```

---

## 25. Complete `loop()` Execution Flow

```
┌─ LOOP START ──────────────────────────────────────────────────────┐
│                                                                    │
│  ┌─ ALWAYS (every iteration) ─────────────────────────────────┐   │
│  │  1. Watchdog reset                                          │   │
│  │  2. Check serial input → processCommand()                  │   │
│  │     (33 commands: diagnostics, maintenance, config, lights) │   │
│  │     Local commands (help, clear, etc.) never reach device   │   │
│  │     PING handler — immediate PONG response                  │   │
│  │     SENSORTEST — single-shot sensor read on demand          │   │
│  │     NETTEST — test HTTP POST on demand                      │   │
│  │     REBOOT — confirmation flow then esp_restart()           │   │
│  │  3. Check SIM800L URC → checkSimURC() → diagnostic SMS     │   │
│  │  4. Process diagnostic SMS reply queue (if any pending)     │   │
│  │  5. Read GPS (non-blocking NMEA accumulation)              │   │
│  │  6. Calculate dt (complementary filter timestep)            │   │
│  │     └── Clamp: if dt > 2.0s → use 0.01s (sleep recovery)  │   │
│  │  7. Check algorithm toggle button (GPIO 12)                │   │
│  │  8. Update obstruction lights (flash timing)               │   │
│  └─────────────────────────────────────────────────────────────┘   │
│                                                                    │
│  ┌─ PERIODIC HOUSEKEEPING ─────────────────────────────────────┐   │
│  │  • RTC read              (every 1 second)                   │   │
│  │  • GPS→RTC sync          (once per 24 hours if conditions)  │   │
│  │  • SIM signal check      (every 60 seconds)                 │   │
│  │  • SIM registration check(every 30 seconds)                 │   │
│  │  • Battery ADC read      (every 30 seconds)                 │   │
│  │  • Sensor health check   (every 60 seconds)                 │   │
│  │  • GPS geofence check    (every valid GPS fix)              │   │
│  │  • Draft pressure logging (every 1 hour, surface only)      │   │
│  │  • Accel variance check  (every 50 readings)                │   │
│  │  • Gravity magnitude check (every reading)                  │   │
│  │  • Pressure range/staleness (every BMP280 read)             │   │
│  │  • Health score update   (every 60 seconds)                 │   │
│  └─────────────────────────────────────────────────────────────┘   │
│                                                                    │
│  ┌─ CONTINUOUS SENSOR READ ────────────────────────────────────┐   │
│  │  IF mpuHealthy:                                              │   │
│  │    • Read 14 bytes from MPU6050 (accel XYZ + temp + gyro XYZ)│  │
│  │    • Convert to g and °/s                                    │   │
│  │    • Complementary filter:                                   │   │
│  │      filtTiltX = 0.98×(filtTiltX + gx×dt) + 0.02×accTiltX  │   │
│  │    • Subtract reference tilt → corrected angles             │   │
│  │    • θ = √(correctedX² + correctedY²) [angle from vertical]│   │
│  │    • Compute lateral_accel = √(ax² + ay²)                   │   │
│  │                                                              │   │
│  │  MODE DETECTION (every loop):                                │   │
│  │    • Check pressure: ΔP > 500 Pa? → MODE 3                 │   │
│  │    • Check tension: lateral_accel > 0.15? → TAUT            │   │
│  │    • Check angle: θ < 10° AND H > 0.95L? → MODE 2          │   │
│  │    • Otherwise: MODE 1 (TAUT) or MODE 0 (SLACK)            │   │
│  │                                                              │   │
│  │  WATER HEIGHT CALCULATION (by mode):                         │   │
│  │    • MODE 0: H = 0 (below threshold, safe)                 │   │
│  │    • MODE 1: H = L × cos(θ)                                │   │
│  │    • MODE 2: H ≈ L                                          │   │
│  │    • MODE 3: H = L + ΔP/(ρ×g)                              │   │
│  └──────────────────────────────────────────────────────────────┘   │
│                                                                    │
│  ┌─ BMP280 PRESSURE (every 5 seconds) ─────────────────────────┐   │
│  │  • Read temperature + pressure                              │   │
│  │  • Evaluate submersion (0=normal, 1-3=submerged)           │   │
│  │  • Update pressure baseline (every 30 min if not submerged)│   │
│  └─────────────────────────────────────────────────────────────┘   │
│                                                                    │
│  ┌─ ALGORITHM ENABLED PATH ────────────────────────────────────┐   │
│  │                                                             │   │
│  │  IF (now - lastFloodSample ≥ sampleInterval):              │   │
│  │    ├── Fresh sensor reading (per current mode)             │   │
│  │    │   MODE 0: waterHeight = 0                              │   │
│  │    │   MODE 1: fresh MPU read → H = L×cos(θ)              │   │
│  │    │   MODE 2: H = L (flood threshold)                     │   │
│  │    │   MODE 3: H = L + depth from BMP280                   │   │
│  │    ├── calculateRateOfChange()                              │   │
│  │    ├── evaluateFloodStatus()  ← STATE MACHINE              │   │
│  │    │   ├── Classify zone                                    │   │
│  │    │   ├── Classify rate                                    │   │
│  │    │   ├── Update sustained buffer                          │   │
│  │    │   ├── Apply overrides (cold start, extreme rate)      │   │
│  │    │   ├── Matrix lookup                                    │   │
│  │    │   ├── Step-down evaluation                             │   │
│  │    │   ├── Level change recorded in state                   │   │
│  │    │   ├── Included in next HTTP POST payload               │   │
│  │    │   └── Server detects transition and handles notification│  │
│  │    ├── updateAdaptiveIntervals()                            │   │
│  │    ├── addToTransmitBuffer()                                │   │
│  │    ├── logReading()                                         │   │
│  │    ├── EEPROM save (if dirty + interval elapsed)           │   │
│  │    └── Print FLOOD: status line                             │   │
│  │                                                             │   │
│  │  IF (now - lastTransmit ≥ transmitInterval):               │   │
│  │    └── transmitBufferedData()                               │   │
│  │        ├── GPRS POST to server                              │   │
│  │        ├── Server handles all alerting from payload         │   │
│  │        ├── On POST failure: archive to SPIFFS               │   │
│  │        └── On 3× failure: flag GPRS_OFFLINE, keep archiving│   │
│  │                                                             │   │
│  └─────────────────────────────────────────────────────────────┘   │
│                                                                    │
│  ┌─ ALGORITHM DISABLED PATH ───────────────────────────────────┐   │
│  │  • Water height computed per current mode (continuous)      │   │
│  │  • Log reading (continuous, for data acquisition)          │   │
│  │  • No flood evaluation                                      │   │
│  │  • No adaptive intervals                                    │   │
│  │  • No transmit to server                                    │   │
│  └─────────────────────────────────────────────────────────────┘   │
│                                                                    │
│  ┌─ LED STATUS INDICATION ─────────────────────────────────────┐   │
│  │  CRITICAL: 5 Hz flash  (100ms on / 100ms off)              │   │
│  │  FLOOD:    2 Hz flash  (250ms on / 250ms off)              │   │
│  │  WARNING:  1 Hz flash  (500ms on / 500ms off)              │   │
│  │  WATCH:    0.33 Hz     (1000ms on / 2000ms off)            │   │
│  │  NORMAL:   Solid ON                                         │   │
│  └─────────────────────────────────────────────────────────────┘   │
│                                                                    │
│  ┌─ SERIAL CSV OUTPUT (every 1 second) ────────────────────────┐   │
│  │  39 fields comma-separated                                  │   │
│  │  ALSO: Duplicate to XIAO C3 via software UART (GPIO 14)    │   │
│  │        C3 parses CSV and forwards to Firebase (data feed)   │   │
│  │        C3 also uses CSV to validate boot after OTA update   │   │
│  └─────────────────────────────────────────────────────────────┘   │
│                                                                    │
│  ┌─ SLEEP / DELAY ─────────────────────────────────────────────┐   │
│  │  Algorithm ENABLED:                                         │   │
│  │    Calculate time to next event (sample or transmit)        │   │
│  │    IF canLightSleep() AND timeToNext > 30 seconds:          │   │
│  │      → enterLightSleep(timeToNext - 5s safety margin)      │   │
│  │    ELSE:                                                    │   │
│  │      → delay(50ms)                                          │   │
│  │                                                             │   │
│  │  Algorithm DISABLED:                                        │   │
│  │    → delay(50ms) — continuous ~20Hz loop rate              │   │
│  └─────────────────────────────────────────────────────────────┘   │
│                                                                    │
└─ LOOP END ────────────────────────────────────────────────────────┘
```

---

## 26. Light Sleep Implementation

```
enterLightSleep(sleepDurationMs)

Guard: sleepDurationMs < 10 seconds → skip (not worth it)
Clamp: sleepDurationMs ≤ (WDT_TIMEOUT - 10) seconds → prevent WDT reset

Pre-sleep:
  1. Turn off obstruction LEDs
  2. Reset NMEA parser index (prevent corrupt partial sentence)
  3. Reset SIM URC parser index
  4. Flush serial output
  5. Remove task from watchdog (WDT can't fire during sleep)

Sleep:
  esp_sleep_enable_timer_wakeup(duration_microseconds)
  esp_light_sleep_start()    ← CPU halts, peripherals stay powered

Post-wake:
  1. Re-add task to watchdog
  2. Reset watchdog timer
  3. Reset prevTime to millis() (fix dt calculation)
  4. Flush GPS serial buffer (accumulated garbage during sleep)
  5. Reset parser indices

canLightSleep() conditions:
  ├── Algorithm must be enabled
  ├── Sample interval ≥ 5 minutes
  ├── NOT in CRITICAL response
  ├── No active diagnostic SMS exchange in progress
  └── Diagnostic reply queue empty

Note: Light sleep does NOT affect the C3's ability to monitor
the CSV feed or initiate OTA. The C3 operates independently.
When the S3 wakes from sleep, the CSV feed resumes and the
C3 continues normally.
```

**Why not deep sleep?** Light sleep preserves RAM (all variables), UART state, and I2C bus configuration. Deep sleep would require full reboot. The tradeoff is light sleep saves less power but wakes instantly.

---

## 27. GPRS Data Upload — JSON Format

```json
{
  "d": "VARUNA_GDV_KG01",
  "lat": 16.5100,
  "lon": 81.8200,
  "health": 85,
  "mode": 1,
  "originDist": 1.3,
  "draftRate": 0.02,
  "r": [
    {
      "t": 1742556720,
      "h": 275.3,
      "r": 12.45,
      "z": 3,
      "l": 4,
      "b": 78,
      "s": 1
    }
  ]
}
```

```
Fields:
  d          = Station/device identifier
  lat, lon   = GPS position
  health     = Composite health score (0-100)
  mode       = Current operating mode (0-3)
  originDist = Distance from anchor origin (meters)
  draftRate  = Draft pressure change rate (hPa/hr)
  r[]        = Readings array (up to 30 buffered)
    t = Unix timestamp
    h = Water height (cm)
    r = Rate of change (cm/15min)
    z = Zone (0-3)
    l = Response level (0-4)
    b = Battery percent
    s = Sustained rise flag (0/1)
```

**Transmit flow:**
```
GPRS POST to server
    │
    ├── Success → clear buffer, reset fail counter
    │             Server now has latest readings
    │             Server handles all alerting
    │
    └── Fail → increment gprsConsecutiveFails
              │
              ├── < 3 fails → retry next transmit interval
              │               archive to SPIFFS as safety copy
              │
              └── ≥ 3 fails → GPRS_OFFLINE flagged
                              Archive all data to SPIFFS
                              Dump to serial for local capture
                              Continue sampling and buffering
                              Attempt GPRS reconnect periodically
```

---

## 28. SPIFFS Data Archival

```
File structure: /log_YYYYMMDD.csv
Each line: timestamp,height,rate,zone,response,battery

Storage management:
  IF freeBytes < 1024:
    → Delete oldest file (first file in directory listing)
    → Log deletion event

This creates a rolling log that survives reboots and network outages.
Data is archived to SPIFFS whenever:
  - GPRS upload fails
  - A flood sample is taken (always, as backup)
  - transmitBufferedData() can't reach the server
```

---

## 29. Console & Diagnostic Command Reference

> **Command set philosophy:** Every command in this system has a single clear answer to: *"When would someone actually type this on a deployed device from a remote console?"*
>
> Physical installation parameters (OLP length, origin coordinates, sensor calibration) are set once at deployment using the handheld debugger and never change remotely. Algorithm toggling and debug mode are bench testing operations. Contact management is on the server. Clock sync is automatic from GPS. Anything that cannot be meaningfully acted on from a remote browser console has been removed.
>
> What remains is a clean separation:
> - **Local commands** run in the browser and never touch the device
> - **Diagnostics** are read-only — nothing changes on the device
> - **Remote maintenance** commands change device state and exist because diagnostics identified a specific problem
> - **Live configuration** covers the small number of parameters that legitimately change post-deployment
> - **Obstruction lights** are grouped separately because they are a self-contained subsystem
>
> Total: 33 commands. No ambiguity about when or why each one is used.

---

### 29.1 🖥️ Local Commands

These run entirely in the browser. They are never sent to the device over the serial link.

```
┌───────────────────┬──────────────────────────────────────────────────┐
│ Command           │ Description                                      │
├───────────────────┼──────────────────────────────────────────────────┤
│ help              │ Show all available commands                       │
│ clear             │ Clear the terminal output                        │
│ history           │ Show previously entered commands                  │
│ connect           │ Connect to device via Web Serial                 │
│ disconnect        │ Disconnect Web Serial                            │
│ status            │ Show Web Serial and Firebase connection state    │
│ filter <type>     │ Filter log output:                               │
│                   │   all / status / error / warning / flood         │
│ upload            │ Open firmware file picker for OTA update         │
│ about             │ Show firmware version and hardware summary       │
└───────────────────┴──────────────────────────────────────────────────┘

These commands are processed by the browser-side JavaScript.
The device firmware does not implement or recognise them.
```

---

### 29.2 🔍 Diagnostics

Read-only commands sent to the device. Nothing is changed on the device. Use these first whenever investigating a problem.

```
┌──────────────────┬───────────────────────────────────────────────────┐
│ Command          │ Description                                       │
├──────────────────┼───────────────────────────────────────────────────┤
│ PING             │ Confirm the device is alive and responding.       │
│                  │ Device replies: PONG:<uptime_seconds>             │
│                  │                                                   │
│ FLOODSTATUS      │ Full flood state report:                          │
│                  │   Water level, zone, response level, rate of      │
│                  │   change, sustained rise flag, peak height,       │
│                  │   step-down counter, algorithm state,             │
│                  │   sample/transmit intervals, battery,             │
│                  │   readings since boot, operating mode (0-3).     │
│                  │                                                   │
│ SENSORTEST       │ Fire one reading from each sensor and return     │
│                  │ raw values:                                       │
│                  │   MPU6050: ax, ay, az, gx, gy, gz, WHO_AM_I     │
│                  │   BMP280: pressure (hPa), temperature (°C)       │
│                  │   Computed: theta, lateral_accel, g_magnitude    │
│                  │   Health: variance, frozen count, gravity error  │
│                  │ Use to verify sensor health remotely without     │
│                  │ retrieving the device.                            │
│                  │                                                   │
│ GPSFIX           │ Current GPS state:                                │
│                  │   Latitude, longitude, altitude                   │
│                  │   Fix valid (yes/no), satellite count, HDOP      │
│                  │   Time from GPS, last sync to RTC                │
│                  │   Distance from origin (if origin set)           │
│                  │                                                   │
│ BATT             │ Battery status:                                   │
│                  │   Voltage (mV), percentage, state                │
│                  │   (GOOD / LOW / CRITICAL / CUTOFF)               │
│                  │                                                   │
│ SIMSTATUS        │ Full SIM800L modem report:                        │
│                  │   Module available, SIM card ready,              │
│                  │   Network registered (home/roaming/none),        │
│                  │   Signal RSSI, GPRS connected,                   │
│                  │   Last POST success/fail, consecutive failures   │
│                  │                                                   │
│ SIMSIGNAL        │ Signal strength only — faster than SIMSTATUS.    │
│                  │   Returns: SIGNAL:<rssi>                         │
│                  │                                                   │
│ NETTEST          │ Fire a test HTTP POST to the configured server.  │
│                  │ Reports:                                          │
│                  │   Success/failure, HTTP response code,           │
│                  │   Round-trip time in milliseconds,               │
│                  │   Server URL used.                               │
│                  │ Confirms data actually reaches the server        │
│                  │ end-to-end, not just that the modem is           │
│                  │ registered on the network.                        │
│                  │                                                   │
│ GETTHRESH        │ Return current flood thresholds:                  │
│                  │   Alert: <cm>, Warning: <cm>, Danger: <cm>      │
│                  │                                                   │
│ GETCONFIG        │ Return all current configuration values:          │
│                  │   APN, server URL, OLP length, geofence radius, │
│                  │   sample interval, transmit interval,            │
│                  │   origin coordinates, algorithm state,           │
│                  │   obstruction light preset                       │
│                  │                                                   │
│ DUMP             │ Print the full on-device data log.               │
│                  │ Historical readings stored in RAM ring buffer    │
│                  │ (up to 120 entries).                              │
│                  │ Format: LOG:unix,datetime,theta,height,          │
│                  │         pressure,lat,lon                          │
│                  │ Also reports peak height and minimum height      │
│                  │ for the current session.                          │
└──────────────────┴───────────────────────────────────────────────────┘

PRINCIPLE: Every diagnostic command is safe to run at any time,
in any state, without side effects. Running SENSORTEST during a
flood does not interrupt monitoring. Running NETTEST does not
interfere with the regular transmit cycle.
```

---

### 29.3 🔧 Remote Maintenance

Use these when diagnostics have identified a specific problem. These commands change device state.

```
┌──────────────────┬───────────────────────────────────────────────────┐
│ Command          │ Description                                       │
├──────────────────┼───────────────────────────────────────────────────┤
│ SIMREINIT        │ Full SIM800L software re-initialisation.          │
│                  │ Use when SIMSTATUS shows the modem is present    │
│                  │ but not registered on the network.                │
│                  │ Sequence: echo off → SMS mode → SIM check →     │
│                  │ network registration → signal strength →         │
│                  │ configure SMS delivery.                           │
│                  │                                                   │
│ SIMRESET         │ Hardware reset of the SIM800L module.             │
│                  │ Pulls RST pin LOW for 200ms, waits for reboot.  │
│                  │ Use when SIMREINIT fails or the modem is         │
│                  │ completely unresponsive (no AT response).         │
│                  │                                                   │
│ REBOOT           │ Clean software reboot of the ESP32-S3.           │
│                  │ Saves current state to EEPROM before restarting. │
│                  │ On reboot, warm boot recovery restores flood     │
│                  │ state if saved state is < 30 minutes old.        │
│                  │ Requires confirmation: device sends              │
│                  │   CONFIRM_REBOOT? — console must reply CONFIRM   │
│                  │   within 10 seconds or reboot is cancelled.      │
│                  │                                                   │
│ SAVEEEPROM       │ Force an immediate EEPROM save.                   │
│                  │ Use after any configuration change to confirm    │
│                  │ it is persisted before the next automatic save   │
│                  │ interval (30 minutes).                            │
│                  │ Returns: EEPROM_SAVED:<timestamp>                │
└──────────────────┴───────────────────────────────────────────────────┘

PRINCIPLE: Each of these is an intervention. You should always
run the corresponding diagnostic command first (e.g., SIMSTATUS
before SIMREINIT, FLOODSTATUS before anything affecting flood
state). The console UI should suggest this workflow.
```

---

### 29.4 ⚙️ Live Configuration

These are the only parameters that legitimately change after a device is deployed. Everything else is set at installation using the handheld debugger.

```
┌──────────────────────┬───────────────────────────────────────────────┐
│ Command              │ Description                                   │
├──────────────────────┼───────────────────────────────────────────────┤
│ SETALERT:<cm>        │ Set the Alert threshold in centimetres.       │
│                      │ Must be > 0 and < current Warning level.     │
│                      │ Triggers EEPROM save.                         │
│                      │ Returns: ALERT_LEVEL_SET=<cm>                │
│                      │                                               │
│ SETWARN:<cm>         │ Set the Warning threshold in centimetres.     │
│                      │ Must be > current Alert and < current Danger.│
│                      │ Triggers EEPROM save.                         │
│                      │ Returns: WARNING_LEVEL_SET=<cm>              │
│                      │                                               │
│ SETDANGER:<cm>       │ Set the Danger threshold in centimetres.      │
│                      │ Must be > current Warning level.              │
│                      │ Triggers EEPROM save.                         │
│                      │ Returns: DANGER_LEVEL_SET=<cm>               │
│                      │                                               │
│ APN:<apn>            │ Set the GPRS APN string.                      │
│                      │ Use when changing SIM operator in the field.  │
│                      │ Takes effect at next GPRS connection attempt.│
│                      │ Returns: APN_SET=<apn>                       │
│                      │                                               │
│ SERVER:<url>         │ Set the server URL the device posts data to.  │
│                      │ Full URL including protocol and path.         │
│                      │ Takes effect at next transmit interval.       │
│                      │ Returns: SERVER_SET=<url>                    │
└──────────────────────┴───────────────────────────────────────────────┘

WHY ONLY THESE FIVE:

  Thresholds change because rivers behave differently season
  to season. The monsoon Alert level for a Godavari station may
  be 120cm but the winter Alert level may be 80cm. An engineer
  sitting at the dashboard adjusts these without a field visit.

  APN changes because SIM cards get swapped. If you switch from
  Airtel to BSNL on a remote station, you need to update the APN
  without opening the capsule.

  Server URL changes during development-to-production migration,
  or if the server endpoint moves.

  Everything else — OLP length, origin coordinates, sensor
  calibration, algorithm enable, height inversion — is a physical
  installation parameter set once by the handheld debugger at
  deployment time. Changing these remotely on a live device without
  physical verification is worse than not changing them at all.
```

---

### 29.5 💡 Obstruction Light Commands

```
┌─────────────────────────────┬────────────────────────────────────────┐
│ Command                     │ Description                            │
├─────────────────────────────┼────────────────────────────────────────┤
│ OBLIGHTON                   │ Enable obstruction lights.             │
│                             │ Flashing resumes at current timing.   │
│                             │                                        │
│ OBLIGHTOFF                  │ Disable obstruction lights.            │
│                             │ All LEDs turned off immediately.      │
│                             │                                        │
│ GETOBLIGHT                  │ Query current light state:             │
│                             │   ON/OFF, on_ms, off_ms, period,     │
│                             │   duty cycle %, current preset name   │
│                             │                                        │
│ GETOBPRESETS                │ List all available flash presets:      │
│                             │   Name, on_ms, off_ms, period,        │
│                             │   duty cycle, description             │
│                             │   (10 presets: IALA + USCG standards) │
│                             │                                        │
│ SETOBLIGHT:<on_ms>,<off_ms> │ Set custom flash timing.              │
│                             │   on_ms: 50–5000 ms                   │
│                             │   off_ms: 100–30000 ms                │
│                             │ Saves to EEPROM. Preset set to CUSTOM.│
│                             │                                        │
│ SETOBPRESET:<name>          │ Apply a named standard preset.        │
│                             │ See GETOBPRESETS for available names. │
│                             │ Saves to EEPROM.                       │
│                             │ Examples:                              │
│                             │   SETOBPRESET:IALA_FL4S               │
│                             │   SETOBPRESET:USCG_STD                │
└─────────────────────────────┴────────────────────────────────────────┘

WHY THESE ARE REMOTELY CONFIGURABLE:

  Obstruction light timing may need to change based on
  maritime authority directives, seasonal navigation patterns,
  or coordination with other aids to navigation in the waterway.
  The timing has no interaction with the flood monitoring logic —
  it is a self-contained subsystem that can be adjusted safely
  at any time without side effects on sensor readings or
  flood state evaluation.
```

---

### 29.6 Diagnostic SMS Channel (Field Engineers)

When a field engineer is on-site with no laptop or internet, they can interact with the device via SMS from their phone. This is a subset of the diagnostic commands above.

```
Authorized field engineer numbers (up to 5):
  Stored on device in EEPROM
  Set at deployment using the handheld debugger
  NOT the same as alert contacts (those are on the server)

Inbound SMS commands (from authorized numbers only):

  PING               → Reply: "PONG <uptime>"
  STATUS             → Reply: full status summary via SMS
  FLOODSTATUS        → Reply: flood state summary via SMS
  BATT               → Reply: battery voltage and percentage
  GPSFIX             → Reply: GPS coordinates and fix status
  SIMSTATUS          → Reply: modem and network state
  GETTHRESH          → Reply: current threshold values
  SENSORTEST         → Reply: raw sensor readings

  SIMREINIT          → Re-initialise SIM800L, reply with result
  SIMRESET           → Hardware reset SIM800L, reply with result

  SETALERT:<cm>      → Set threshold, reply with confirmation
  SETWARN:<cm>       → Set threshold, reply with confirmation
  SETDANGER:<cm>     → Set threshold, reply with confirmation

  OBLIGHTON          → Enable, reply with confirmation
  OBLIGHTOFF         → Disable, reply with confirmation

  DIAG_TETHER        → Run tether diagnostic, reply with results
  DIAG_ANCHOR        → Run anchor diagnostic, reply with results
  DIAG_LEAK          → Run leak analysis, reply with results
  DIAG_SENSORS       → Run sensor health check, reply with results
  DIAG_ALL           → Run all diagnostics, reply with results
  DIAG_ABORT         → Cancel running diagnostic

REMOVED FROM SMS:
  ✗ CAL               (requires physical presence with debugger)
  ✗ OLP:<cm>          (installation parameter, debugger only)
  ✗ ALGOON/ALGOOFF    (bench testing, debugger only)
  ✗ RESETFLOOD        (too dangerous remotely)
  ✗ REBOOT            (confirmation flow not possible over SMS)
  ✗ APN / SERVER      (infrastructure changes need console context)
  ✗ SETOBLIGHT/PRESET (low priority for SMS channel)

AUTHORIZATION:
  Phone number matching uses last-10-digit comparison
  to handle international format variations:
    +919876543210 matches 09876543210 matches 9876543210
```

---

### 29.7 Commands Removed and Why

For documentation completeness, the following commands from previous firmware versions have been removed:

```
┌────────────────────┬─────────────────────────────────────────────────┐
│ Removed Command    │ Reason                                          │
├────────────────────┼─────────────────────────────────────────────────┤
│ ADDTIER1/2/3       │ Contact management moved to server database.   │
│ CLEARTIERS         │ No contact lists on device. Updating who gets  │
│ SAVECONTACTS       │ notified now requires zero hardware changes.   │
│                    │                                                 │
│ ADDAUTH            │ Authorized field engineer numbers are set at   │
│                    │ deployment with the handheld debugger. Adding  │
│                    │ them remotely defeats the security model.      │
│                    │                                                 │
│ SMS:<phone>:<msg>  │ Device does not send messages to anyone. All   │
│                    │ alerting is server-side.                        │
│                    │                                                 │
│ GPRSINIT           │ GPRS bearer is managed internally by the       │
│                    │ transmit cycle. Manual init interferes and     │
│                    │ serves no purpose post-deployment.             │
│                    │                                                 │
│ RESETFLOOD         │ Too dangerous for remote console. Resetting    │
│                    │ flood state during an active event suppresses  │
│                    │ all alerts. If needed, belongs behind admin    │
│                    │ confirmation on the server dashboard.          │
│                    │                                                 │
│ ALGOON / ALGOOFF   │ Algorithm toggling is a bench testing op.      │
│ ALGOSTATUS         │ Handled by handheld debugger pre-deployment.   │
│                    │ No valid reason to toggle flood detection on   │
│                    │ a live deployed device from a remote console.  │
│                    │                                                 │
│ INVERTHEIGHT       │ One-time physical installation decision set    │
│                    │ at deployment. Not a runtime parameter.        │
│                    │                                                 │
│ SERIALON/OFF       │ The CSV serial output is what the console      │
│                    │ reads. Disabling it from the console defeats   │
│                    │ the purpose of the console.                    │
│                    │                                                 │
│ SETTHRESH:a,w,d    │ Replaced by individual SETALERT, SETWARN,     │
│                    │ SETDANGER. One malformed combined command      │
│                    │ cannot corrupt all three thresholds at once.   │
│                    │                                                 │
│ TIME:<unix>        │ RTC syncs from GPS automatically. If GPS has  │
│ SETDATE:...        │ a fix, time is correct. If not, the right     │
│                    │ response is to investigate GPS, not patch the  │
│                    │ clock remotely with potentially wrong data.    │
│                    │                                                 │
│ CAL                │ Recalibration requires buoy to be stationary   │
│                    │ and level. Triggering it blindly from a remote │
│                    │ console on a buoy in a river is meaningless.   │
│                    │ Done at deployment with handheld debugger.     │
│                    │                                                 │
│ OLP:<cm>           │ Tether length is a physical constant measured  │
│ MOUNT:<cm>         │ at installation. Changing it remotely without  │
│                    │ physical verification introduces silent error  │
│                    │ into every water level reading.                │
│                    │                                                 │
│ DEBUGON / OFF      │ The XIAO C3 is no longer a debug display.     │
│ DEBUGSTATUS        │ It is an OTA programming agent that does not   │
│                    │ accept or display debug packets. The C3's     │
│                    │ UART connection to the S3 (GPIO 43/44) is     │
│                    │ reserved for the OTA bootloader protocol.     │
│                    │ Debug output has no destination.               │
│                    │                                                 │
│ SETORIGIN          │ Origin GPS coordinates are captured at         │
│ SETFENCE           │ deployment. Changing the geofence remotely     │
│ GETORIGIN          │ could mask a tether break. Diagnostic only:   │
│                    │ GPSFIX reports distance from origin.           │
│                    │ GETCONFIG reports origin and fence values.     │
└────────────────────┴─────────────────────────────────────────────────┘
```

---

### 29.8 Command Count Summary

```
Category               Count    Processed By
─────────────────────  ─────    ────────────
Local (browser-only)     9      Browser JavaScript
Diagnostics             11      Device firmware (read-only)
Remote Maintenance       4      Device firmware (state-changing)
Live Configuration       5      Device firmware (config-changing)
Obstruction Lights       6      Device firmware (light subsystem)
                       ────
Total                   35*

*Note: 33 unique actions. GETOBLIGHT and GETOBPRESETS appear in
the obstruction light group but are functionally diagnostic
(read-only). Counted once each.

SMS Channel: 21 commands (subset of above, for field engineers)
```

### 29.9 New Command Implementation Notes

```
PING HANDLING:
  processCommand("PING")
    → Serial.println("PONG:" + String(millis()/1000))
    → Zero overhead, immediate response
    → Used by console to detect if device is connected and responsive

REBOOT HANDLING:
  processCommand("REBOOT")
    → Serial.println("CONFIRM_REBOOT?")
    → Set rebootPending = true, rebootConfirmTimeout = millis() + 10000
    
  Next serial input within 10 seconds:
    → If "CONFIRM":
        forceSaveEeprom()
        Serial.println("REBOOTING...")
        Serial.flush()
        delay(100)
        esp_restart()
    → If anything else or timeout:
        rebootPending = false
        Serial.println("REBOOT_CANCELLED")

NETTEST HANDLING:
  processCommand("NETTEST")
    → startTime = millis()
    → Compose minimal test payload: {"d":"<id>","test":true}
    → result = gprsPostData(testPayload, len)
    → elapsed = millis() - startTime
    → Serial.print("NETTEST:")
    → Serial.print(result ? "OK" : "FAIL")
    → Serial.print(",HTTP=<code>,RTT=<elapsed>ms,URL=")
    → Serial.println(gprsServer)

SENSORTEST HANDLING:
  processCommand("SENSORTEST")
    → readMPU()  (one fresh reading)
    → bmpReadData(&temp, &press)
    → Report all raw values, computed angles, health indicators
    → Does NOT affect complementary filter state
    → Does NOT count as a flood sample reading
```

---

## 30. Complete Real-Time Data Flow

```
                    ┌──────────────┐
                    │  PHYSICAL    │
                    │  WORLD       │
                    │  (River)     │
                    └──────┬───────┘
                           │
              ┌────────────┼────────────┐
              ▼                         ▼
        ┌──────────┐              ┌──────────┐
        │ MPU6050  │              │ BMP280   │
        │ Accel +  │              │ Pressure │
        │ Gyro     │              │ + Temp   │
        │ (I2C)    │              │ (I2C)    │
        └────┬─────┘              └────┬─────┘
             │                         │
        Complementary              Calibration
        Filter (α=0.98)           Compensation
             │                         │
        Reference                  Baseline
        Subtraction                Subtraction
             │                         │
    ┌────────┼────────┐                │
    │        │        │                │
    ▼        ▼        ▼                ▼
┌───────┐┌───────┐┌────────┐    ┌──────────┐
│Tilt θ ││Lateral││Filtered│    │ΔP from   │
│(angle ││Accel  ││Angle   │    │atmospheric│
│from   ││(tens- ││Quality │    │baseline   │
│vert.) ││ion)   ││        │    │           │
└───┬───┘└───┬───┘└────────┘    └─────┬────┘
    │        │                        │
    └────┬───┘                        │
         │                            │
         ▼                            ▼
┌─────────────────────────────────────────────┐
│           MODE DETECTION ENGINE             │
│                                             │
│  ΔP > 500 Pa?                               │
│  ├── YES → MODE 3: SUBMERGED               │
│  │         H = L + ΔP/(ρg)                 │
│  │                                          │
│  └── NO → lateral_accel > 0.15 AND θ > 3°? │
│           ├── NO  → MODE 0: SLACK (safe)    │
│           │         H < L, unknown exact     │
│           │                                  │
│           └── YES → θ < 10° AND H > 0.95L? │
│                     ├── YES → MODE 2: FLOOD │
│                     │         H ≈ L          │
│                     └── NO  → MODE 1: TAUT  │
│                               H = L×cos(θ)  │
└───────────────────┬─────────────────────────┘
                    │
                    ▼
            ┌───────────────┐
            │ Water Height  │
            │ H (cm)        │
            │ + Mode (0-3)  │
            └───────┬───────┘
                    │
          ┌─────────┼──────────┐
          │         │          │
          ▼         │          ▼
   ┌────────────┐   │   ┌──────────────┐
   │ FAILURE    │   │   │ SELF-        │
   │ DETECTION  │   │   │ CALIBRATION  │
   │            │   │   │              │
   │ A1: Frozen │   │   │ Gravity ref  │
   │ A2: Drift  │   │   │ P baseline   │
   │ A3: P fail │   │   │ L_eff verify │
   │ C1: GPS    │   │   │ Draft track  │
   │ C2: Leak   │   │   │              │
   └──────┬─────┘   │   └──────┬───────┘
          │         │          │
          ▼         │     Applied to
   ┌────────────┐   │     current readings
   │Health Score│   │
   │(0-100)     │   │
   └──────┬─────┘   │
          │         │
          └────┬────┘
               │
       ┌───────┼────────────┐
       │       │            │
       ▼       ▼            ▼
  ┌────────┐ ┌─────────┐ ┌──────────┐
  │Rate of │ │Sustained│ │  Zone    │
  │Change  │ │Rise Det │ │Classify  │
  │(cm/15m)│ │(4-buf)  │ │(4 zones) │
  └───┬────┘ └────┬────┘ └────┬─────┘
      │           │            │
      ▼           ▼            ▼
  ┌──────────────────────────────────┐
  │     DECISION MATRIX              │
  │     matrix[zone][rate][sustained]│
  │     → Proposed Response Level    │
  └───────────────┬──────────────────┘
                  │
           ┌──────▼──────┐
           │ Step-Down   │
           │ Evaluator   │
           │ (hysteresis)│
           └──────┬──────┘
                  │
           ┌──────▼──────┐
           │  New Level  │
           └──────┬──────┘
                  │
      ┌───────────┼───────────┐──────────┐
      ▼           ▼           ▼          ▼
  ┌────────┐ ┌────────┐ ┌────────┐ ┌────────┐
  │Level   │ │Level   │ │Level   │ │ Hold   │
  │UP      │ │DOWN    │ │→NORMAL │ │(no chg)│
  │(escal.)│ │(de-esc)│ │(clear) │ │        │
  └───┬────┘ └───┬────┘ └───┬────┘ └───┬────┘
      │          │          │          │
      └──────────┴──────────┴──────────┘
                       │
              Record new level in state
              Include in next HTTP POST
                       │
                       ▼
          ┌──────────────────────────┐
          │   TRANSMIT BUFFER        │
          │   (30 readings buffered) │
          └────────────┬─────────────┘
                       │
          At transmit interval:
                       │
                       ▼
          ┌──────────────────────────┐
          │   SIM800L GPRS           │
          │   HTTP POST to server    │
          └────────────┬─────────────┘
                       │
              ┌────────┼────────┐
              ▼        ▼        ▼
        ┌────────┐ ┌──────┐ ┌────────┐
        │ Server │ │SPIFFS│ │ Serial │
        │ (HTTP) │ │backup│ │ dump   │
        │        │ │      │ │(GPRS   │
        │handles │ │local │ │offline)│
        │ALL     │ │store │ │        │
        │alerts  │ │      │ │        │
        └────────┘ └──────┘ └────────┘

  ┌─ PARALLEL: C3 DATA FEED ──────────────────────────┐
  │  GPIO 14 (SW-UART, 9600 baud, every 1 second)     │
  │          │                                          │
  │          ▼                                          │
  │  ┌──────────────────┐                               │
  │  │   XIAO C3        │                               │
  │  │   ├── Parse CSV   │                               │
  │  │   ├── Forward to  │                               │
  │  │   │   Firebase    │                               │
  │  │   │   (own GPRS)  │                               │
  │  │   └── OTA agent   │                               │
  │  │       (idle until  │                               │
  │  │        server      │                               │
  │  │        triggers)   │                               │
  │  └──────────────────┘                               │
  └─────────────────────────────────────────────────────┘

  ┌─ COMMAND INPUT ──────────────────────────────────────┐
  │  Browser Console (Web Serial)                         │
  │  ├── Diagnostics: PING, FLOODSTATUS, SENSORTEST,    │
  │  │    GPSFIX, BATT, SIMSTATUS, SIMSIGNAL, NETTEST,  │
  │  │    GETTHRESH, GETCONFIG, DUMP                      │
  │  ├── Maintenance: SIMREINIT, SIMRESET, REBOOT,      │
  │  │    SAVEEEPROM                                      │
  │  ├── Config: SETALERT, SETWARN, SETDANGER, APN,     │
  │  │    SERVER                                          │
  │  └── Lights: OBLIGHTON/OFF, GET/SET timing           │
  │                                                       │
  │  SMS (field engineer diagnostic channel):             │
  │  └── Subset of above (21 commands)                    │
  └───────────────────────────────────────────────────────┘
```

---

## 31. Timing Budget Analysis

```
In CONTINUOUS mode (algorithm disabled), one loop iteration:

  Watchdog reset:              ~0 ms
  Serial check:                ~0-1 ms
  SIM URC check:               ~0-1 ms
  Diagnostic SMS check:        ~0 ms (rare, only field engineer replies)
  GPS read (NMEA accumulate):  ~0-2 ms
  Algorithm button check:      ~0 ms
  Obstruction light update:    ~0 ms
  RTC read (every 1s):         ~1 ms
  MPU6050 read:                ~1 ms
  Complementary filter:        ~0 ms (arithmetic)
  Mode detection:              ~0 ms (comparisons)
  BMP280 read (every 5s):      ~2 ms
  Serial CSV output (every 1s):~3-5 ms
  C3 feed SW-UART (384 chars): ~40 ms at 9600 baud
  delay(50):                   50 ms
  ─────────────────────────────────
  Total: ~55-100 ms per iteration → ~10-18 Hz effective rate


In ADAPTIVE mode (algorithm enabled, NORMAL, battery OK):

  Sample interval: 30 minutes
  Between samples: light sleep or 50ms loop iterations
  During sleep: ~0 mA CPU draw (peripherals still powered)
  
  Wake → read sensors → evaluate → possibly transmit → sleep
  Active time per sample: ~200-500 ms
  
  Duty cycle: 500ms / 1,800,000ms = 0.028%
```

---

## 32. Serial CSV Output Format — 39 Fields

```
Field  Content                    Example
─────  ────────────────────────   ─────────
 1     theta (degrees)            12.45
 2     waterHeight (cm)           45.23
 3     correctedTiltX (deg)       10.32
 4     correctedTiltY (deg)       6.89
 5     olpLength (cm)             100.00
 6     horizontalDist (cm)        21.56
 7     currentPressure (hPa)      1013.25
 8     currentTemperature (°C)    28.50
 9     baselinePressure (hPa)     1013.00
10     pressureDeviation (hPa)    0.25
11     submersionState (0-3)      0
12     estimatedDepth (cm)        0.00
13     bmpAvailable (0/1)         1
14     unixTime                   1706000000
15     dateTimeString             2024-01-23 10:30:00
16     rtcValid (0/1)             1
17     ratePer15Min (cm/15m)      1.234
18     floodAlertLevel (0-3)      0
19     sessionDuration (sec)      3600
20     peakHeight (cm)            48.50
21     minHeight (cm)             12.30
22     latitude                   12.971600
23     longitude                  77.594600
24     altitude (m)               920.5
25     gpsSatellites              8
26     gpsFixValid (0/1)          1
27     simSignalRSSI              18
28     simRegistered (0/1)        1
29     simAvailable (0/1)         1
30     currentZone (0-3)          0
31     currentResponseLevel (0-4) 0
32     sustainedRise (0/1)        0
33     batteryPercent             85.5
34     sampleInterval (sec)       1800
35     transmitInterval (sec)     3600
36     obLightEnabled (0/1)       1
37     algorithmEnabled (0/1)     0
38     currentMode (0-3)          1
39     healthScore (0-100)        85
```

This CSV is output at 1Hz on USB serial AND duplicated to the XIAO C3 via GPIO 14 software UART. The C3 parses selected fields and forwards them to Firebase. The C3 also uses this CSV to validate boot after an OTA firmware update (Gate 3).

---

## 33. Data Logging — RAM Ring Buffer

```
LOG_SIZE = 120 entries (circular buffer)

Each entry:
  logHeight[i]    — water height (cm)
  logTheta[i]     — tilt angle (degrees)
  logPressure[i]  — atmospheric/water pressure (hPa)
  logTime[i]      — Unix timestamp

LOG_INTERVAL = 10,000 ms (10 seconds) between entries

Peak and minimum tracking:
  peakHeight — highest water level recorded this session
  peakTime   — timestamp of peak
  minHeight  — lowest water level recorded this session
  minTime    — timestamp of minimum

DUMP command outputs the complete log in CSV format:
  LOG:START
  LOG:UNIX,DATE_TIME,THETA,HEIGHT,PRESSURE,LAT,LON
  LOG:<unix>,<datetime>,<theta>,<height>,<pressure>,<lat>,<lon>
  ...
  LOG:PEAK_HEIGHT=<value>,PEAK_TIME=<unix>
  LOG:MIN_HEIGHT=<value>,MIN_TIME=<unix>
  LOG:TOTAL_READINGS=<count>
  LOG:END
```

---

## 34. Inter-System Communication Summary

```
┌────────────────────────────────────────────────────────────────┐
│                    ESP32-S3 MAIN CONTROLLER                     │
│                                                                │
│  USB Serial (115200) ←──→ Browser Console (Web Serial API)    │
│    ├── 39-field CSV at 1Hz (continuous telemetry)              │
│    ├── STATUS: messages (system events)                        │
│    ├── FLOOD: status lines (level transitions)                 │
│    ├── LOG: data dump (historical readings)                    │
│    ├── PONG: heartbeat response                                │
│    ├── TEST: sensor/network test results                       │
│    ├── CONFIG: configuration query responses                   │
│    └── Commands in (35 commands, 5 categories)                 │
│                                                                │
│  ┌─── XIAO C3 Interface ──────────────────────────────┐       │
│  │  GPIO 14 (SW-TX 9600) ──→ C3: CSV data feed        │       │
│  │    └── C3 parses → Firebase RTDB (own GPRS)        │       │
│  │  GPIO 43/44 (UART 115200) ←──→ C3: OTA channel    │       │
│  │    └── ROM bootloader protocol during firmware update│      │
│  │  GPIO 0 (BOOT) ←── C3: bootloader mode control    │       │
│  │  EN (RESET) ←── C3: hardware reset control         │       │
│  └─────────────────────────────────────────────────────┘       │
│                                                                │
│  HW-UART2 (GPIO 15/16, 9600) ←──→ SIM800L                    │
│    ├── GPRS HTTP POST (primary data channel to server)         │
│    ├── Inbound SMS (diagnostic commands from field engineers)  │
│    └── Outbound SMS (diagnostic replies only, no alerts)       │
│                                                                │
│  HW-UART1 (GPIO 6/7, 9600) ←── GPS Module                    │
│    └── NMEA sentences ($GPGGA, $GPRMC)                        │
│                                                                │
│  I2C Bus 0 (GPIO 8/9) ←──→ MPU6050                            │
│    └── 14-byte burst read (accel+gyro)                        │
│                                                                │
│  I2C Bus 1 (GPIO 4/5) ←──→ DS1307 RTC                        │
│                        ←──→ BMP280                             │
│                                                                │
│  GPIO 2 ←── Battery ADC (voltage divider)                     │
│  GPIO 3 ──→ Status LED                                        │
│  GPIO 12 ←── Algorithm toggle button                           │
│  GPIO 13 ──→ Algorithm status LED                              │
│  GPIO 17 ──→ SIM800L RST                                      │
│  GPIO 2×5 ──→ Obstruction LEDs                                │
│                                                                │
│  ──→ GPRS Server (JSON POST — server handles ALL alerting)    │
│  ──→ SPIFFS filesystem (CSV archival, backup when offline)    │
│  ──→ EEPROM (state persistence, settings)                     │
└────────────────────────────────────────────────────────────────┘
```

---

## 35. Critical Design Decisions & Tradeoffs

```
┌──────────────────────────────────┬──────────────────────────────────────────┬─────────────────────────────────────────────┐
│ Decision                         │ Rationale                                │ Risk                                        │
├──────────────────────────────────┼──────────────────────────────────────────┼─────────────────────────────────────────────┤
│ H = L×cos(θ) not sin(θ)         │ Buoy measures angle from vertical; cos   │ cos is flat near 0° — poor resolution near  │
│                                  │ gives vertical projection                │ flood level, but pressure takes over         │
│                                  │                                          │                                             │
│ Tether length L = flood          │ Natural alarm at θ→0°, no calibration    │ System cannot precisely measure H > L with   │
│ threshold                        │ table needed                             │ tilt alone                                   │
│                                  │                                          │                                             │
│ Slack tether = safe (no          │ Simplifies system; low water doesn't     │ Cannot report exact low water level for      │
│ measurement)                     │ need precision                           │ hydrological records                         │
│                                  │                                          │                                             │
│ BMP280 for MODE 3 depth          │ Pressure sensors excellent at depth      │ Requires accurate atmospheric baseline;      │
│                                  │ measurement (±1cm)                       │ weather fronts cause ±2 hPa shift            │
│                                  │                                          │                                             │
│ Lateral accel for tether         │ Distinguishes slack from taut without    │ Wave action can cause false lateral           │
│ detection                        │ additional hardware                      │ acceleration readings                        │
│                                  │                                          │                                             │
│ 0.98 gyro weight in              │ Filters vibration/wave noise from river  │ Slow response to sudden tilt changes         │
│ complementary filter             │ on floating buoy                         │                                             │
│                                  │                                          │                                             │
│ Default algorithm DISABLED       │ Processing IDE / console gets data       │ Could deploy accidentally without algorithm  │
│                                  │ immediately for development              │                                             │
│                                  │                                          │                                             │
│ EEPROM save every 30 min         │ Protects flash from wear (~100K cycles) │ Up to 30 min of state lost on crash          │
│                                  │                                          │                                             │
│ Step-down requires 4             │ Prevents oscillation at zone boundaries  │ Delays de-escalation by 4× sample interval  │
│ consecutive readings             │                                          │                                             │
│                                  │                                          │                                             │
│ Light sleep (not deep)           │ Preserves RAM and state, instant wake    │ Higher standby current than deep sleep       │
│                                  │                                          │                                             │
│ BMP280 on I2C Bus 1 (with RTC)  │ Saves GPIO pins                          │ Bus contention if both accessed rapidly      │
│                                  │                                          │                                             │
│ All alerting on server, none     │ Contact lists, escalation rules, and     │ If GPRS offline, server gets no data and     │
│ on device                        │ notification channels updated via web    │ no alerts fire. Mitigated by server-side     │
│                                  │ without touching hardware                │ "silent station" detection                   │
│                                  │                                          │                                             │
│ Device sends HTTP POST only      │ Single communication responsibility.     │ Dependency on cellular data coverage.        │
│                                  │ Device is a sensor, not a notification   │ Mitigated by SPIFFS archival                 │
│                                  │ system. Simplifies firmware.             │                                             │
│                                  │                                          │                                             │
│ SMS retained for diagnostic      │ Field engineers in remote locations may  │ Minimal risk — only 5 authorized numbers,   │
│ channel only                     │ not have laptop/internet                 │ only maintenance commands                    │
│                                  │                                          │                                             │
│ Server detects level             │ Device evaluates flood state (for        │ Slight delay: server learns of transition at │
│ transitions, not device          │ adaptive sampling). Decision of WHO to   │ next transmit interval. At CRITICAL, that's  │
│                                  │ notify is purely server-side             │ 2 minutes — acceptable                       │
│                                  │                                          │                                             │
│ Cooldowns and reminders on       │ Server can adjust intervals, suppress    │ Server must be reliable. If server down, no  │
│ server                           │ during maintenance, batch across stations│ alerts from any station                      │
│                                  │                                          │                                             │
│ GPS geofence always active,      │ Passive geofence is cheap (one distance │ GPS accuracy ±3-5m means margin must         │
│ full tether diag server-         │ calc per fix); full analysis takes 5 min │ account for error; could miss slow drift     │
│ triggered                        │ and burns battery                        │                                             │
│                                  │                                          │                                             │
│ Draft rate for leak vs           │ Biofouling is slow (weeks), leaks are    │ Seasonal temperature changes alter water     │
│ biofouling                       │ fast (hours) — rate distinguishes them   │ density, could cause false draft shift        │
│                                  │                                          │                                             │
│ Accel variance for frozen        │ Buoy on water always has micro-motion;   │ In very calm, sheltered water the threshold  │
│ sensor                           │ zero variance = impossible = fault       │ may need tuning                              │
│                                  │                                          │                                             │
│ Health score composite metric    │ Single number for server dashboards and  │ Weights are subjective; a critical component │
│                                  │ automated fleet management               │ at 0 could still give score of 65            │
│                                  │                                          │                                             │
│ Self-calibration only in MODE 0  │ Buoy orientation known only when         │ Long periods without MODE 0 (sustained high  │
│                                  │ floating freely upright                  │ water) prevent gravity recalibration          │
│                                  │                                          │                                             │
│ No ultrasonic backup             │ Ultrasonic unreliable in splash zone;    │ Single-principle measurement (tilt) for      │
│                                  │ buoy geometry prevents downward mounting │ MODE 1; BMP280 is only independent check     │
│                                  │                                          │                                             │
│ C3 as OTA agent, not debug      │ Debug display has zero value post-       │ OTA adds complexity. A C3 firmware bug could │
│ display                          │ deployment. OTA eliminates field visits  │ interfere if it accidentally toggles GPIO 0  │
│                                  │ for firmware updates                     │ or EN. Mitigated by default-HIGH pin state   │
│                                  │                                          │                                             │
│ Three-gate OTA acknowledgment    │ Each gate catches different failure      │ Process takes ~3.5 minutes. Cannot be        │
│                                  │ class: bad timing, data corruption,      │ accelerated due to GSM bandwidth limits      │
│                                  │ flash failures                           │                                             │
│                                  │                                          │                                             │
│ Flood event blocks OTA (Gate 1)  │ Firmware update reboots S3 causing      │ If firmware bug found DURING flood, can't    │
│                                  │ 60-90s monitoring downtime. During       │ patch until event subsides. Correct tradeoff: │
│                                  │ flood, unacceptable                      │ monitoring continuity > firmware freshness    │
│                                  │                                          │                                             │
│ CSV feed validates boot (Gate 3) │ CSV is S3's proof-of-life. Valid data    │ Firmware bug producing valid-looking CSV but │
│                                  │ = firmware working. No separate protocol │ broken flood logic passes Gate 3             │
│                                  │                                          │                                             │
│ GPIO 0 controlled by C3          │ GPIO 0 is ESP32-S3 bootstrap pin.       │ If C3 crashes with GPIO 0 LOW, S3 enters    │
│                                  │ Same mechanism as esptool.py             │ bootloader on next reset. Mitigated by C3    │
│                                  │                                          │ watchdog and default-HIGH pin state           │
│                                  │                                          │                                             │
│ 33 commands, strictly            │ Every command answers "when would someone│ Reduces flexibility for edge cases; engineer  │
│ categorized                      │ type this on a deployed device?" — if    │ who needs remote recalibration must retrieve │
│                                  │ "never", command removed from console    │ the device                                   │
│                                  │                                          │                                             │
│ No RESETFLOOD on console         │ Accidentally resetting during active     │ If state machine locks up, only remote fix   │
│                                  │ event silences all server-side alerts    │ is REBOOT (warm boot recovery)               │
│                                  │                                          │                                             │
│ No algorithm toggle on console   │ Disabling flood detection on deployed    │ Stuck algorithm cannot be restarted remotely │
│                                  │ device from browser is never correct     │ without REBOOT                               │
│                                  │                                          │                                             │
│ REBOOT requires confirmation     │ Prevents accidental reboot during        │ SMS channel cannot support confirmation, so  │
│                                  │ active flood                             │ REBOOT is console-only                       │
│                                  │                                          │                                             │
│ Separate SETALERT/SETWARN/       │ One malformed command cannot corrupt     │ Three commands instead of one; acceptable    │
│ SETDANGER                        │ all three values                         │ tradeoff for safety                          │
│                                  │                                          │                                             │
│ NETTEST as diagnostic            │ Confirms entire data path end-to-end:    │ Consumes one GPRS transaction; should not    │
│                                  │ device → GPRS → HTTP → server            │ be spammed                                  │
│                                  │                                          │                                             │
│ Installation params excluded     │ Changing tether length remotely without  │ Field engineer must physically visit;        │
│ from console                     │ measuring physical tether introduces     │ cannot be done in emergency                  │
│                                  │ silent systematic error                  │                                             │
└──────────────────────────────────┴──────────────────────────────────────────┴─────────────────────────────────────────────┘
```

---

## 36. System State Summary — What's Tracked

```
┌─ PERSISTENT (EEPROM, survives reboot) ────────────────────────┐
│  Response level, sustained buffer, thresholds, OLP length,    │
│  peak height/time, previous water height, state entry time,   │
│  authorized field engineer numbers (5 max),                   │
│  origin GPS coordinates (lat, lon), geofence radius,          │
│  obstruction light timing and preset name                     │
└───────────────────────────────────────────────────────────────┘

┌─ SEMI-PERSISTENT (SPIFFS, survives reboot) ───────────────────┐
│  Daily CSV archives: timestamp, height, rate, zone, response  │
│  Rolling log with automatic oldest-file deletion              │
└───────────────────────────────────────────────────────────────┘

┌─ VOLATILE (RAM, lost on reboot) ──────────────────────────────┐
│  Complementary filter state, gyro offsets, reference tilt,    │
│  rate of change, current pressure/baseline, GPS position,     │
│  SIM800L state, diagnostic SMS reply queue (5 slots),         │
│  step-down counter (for state machine hysteresis),            │
│  session duration, transmit buffer (30 entries),              │
│  sensor log (120 entries), battery percent,                   │
│  current operating mode (0-3), lateral acceleration,          │
│  tether tension state, geofence breach count,                 │
│  accel variance buffer, pressure variance buffer,             │
│  gravity error count, MPU frozen count,                       │
│  draft history (24hr), health score,                          │
│  active diagnostic state, diagnostic results,                 │
│  REBOOT confirmation pending flag and timeout,                │
│  NETTEST in-progress flag                                     │
└───────────────────────────────────────────────────────────────┘

┌─ RTC (Battery-backed, survives power loss) ───────────────────┐
│  Current date/time (synced from GPS every 24 hours)           │
└───────────────────────────────────────────────────────────────┘

┌─ XIAO C3 (independent state) ────────────────────────────────┐
│  OTA download progress (chunk index, total chunks)            │
│  Downloaded firmware file on C3 filesystem                    │
│  OTA phase (IDLE/DOWNLOADING/VERIFYING/FLASHING/CONFIRMING)  │
│  CSV validation buffer (last 5 lines for Gate 3)             │
│  The S3 has no knowledge of the OTA process — from the S3's  │
│  perspective, an OTA update looks like a power cycle.         │
└───────────────────────────────────────────────────────────────┘
```

---

## 37. Utility Functions

### Float to String (`floatToStr`)
```
Custom float-to-string conversion avoiding sprintf %f
(which pulls in large floating-point formatting library).

Handles: NaN, Inf, -Inf, overflow (>999999999)
Supports 0-4 decimal places
Uses integer arithmetic for fractional part
Recursive for negative values
```

### BCD Conversion
```
bcdToDec(val): ((val >> 4) * 10) + (val & 0x0F)
decToBcd(val): ((val / 10) << 4) | (val % 10)
Used for DS1307 RTC register encoding/decoding.
```

### Contact Duplicate Check (`contactExists`)
```
Compares last 10 digits of phone numbers to handle
international format variations:
  +919876543210 matches 09876543210 matches 9876543210
Used for authorized field engineer number validation.
```

### Unix Time ↔ Date Conversion
```
dateToUnix(): Custom algorithm counting days with leap year
              correction, then adding hours/minutes/seconds.
              Does not use time.h library.

unixToDate(): Reverse — extracts year, month, day, hour,
              minute, second, day-of-week from Unix timestamp.
              Iterates through years and months.
```

---

## End of Documentation

This document covers the complete VARUNA Flood Monitoring System:

- **Part 1** (Sections 1-17): System architecture, physical measurement principle (buoy-tether pendulum), sensor fusion, flood detection state machine, server-centric alert architecture, adaptive sampling, SIM800L communication, RTC/GPS integration, EEPROM persistence, obstruction lights, algorithm toggle, XIAO C3 OTA agent, BMP280 pressure/depth, sensor health, and the complete failure detection & diagnostics system with self-calibration and field calibration procedures.

- **Part 2** (Sections 18-37): Decision matrix implementation, step-down algorithm, rate of change calculation, sustained rise detection, override mechanisms, cold start logic, complete setup() and loop() execution flows, light sleep, GPRS data upload format, SPIFFS archival, the full 33-command console reference with rationale for every inclusion and exclusion, real-time data flow diagram, timing budget, 39-field CSV output format, data logging, inter-system communication summary, design decisions & tradeoffs table, system state summary, and utility functions.

---

