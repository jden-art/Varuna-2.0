# Varuna-2.0-NOT-GIVING-UP-NIGA


```
// Import the functions you need from the SDKs you need
import { initializeApp } from "firebase/app";
// TODO: Add SDKs for Firebase products that you want to use
// https://firebase.google.com/docs/web/setup#available-libraries

// Your web app's Firebase configuration
const firebaseConfig = {
  apiKey: "AIzaSyBzpgemPlO1eG5SRNuXaOVQDECMqY5bDts",
  authDomain: "varuna-flood.firebaseapp.com",
  databaseURL: "https://varuna-flood-default-rtdb.asia-southeast1.firebasedatabase.app",
  projectId: "varuna-flood",
  storageBucket: "varuna-flood.firebasestorage.app",
  messagingSenderId: "1050491658503",
  appId: "1:1050491658503:web:e706aa5ff1a0c3fc1de5a2"
};

// Initialize Firebase
const app = initializeApp(firebaseConfig);
```













# VARUNA Firmware — Complete Phase & Step Breakdown

---

## Development Environment Notes

| Target MCU | IDE | Framework |
|---|---|---|
| XIAO ESP32-C3 (Brain) | Arduino IDE | ESP32 Arduino Core (ESP-IDF based) |
| STM32F103C8T6 Blue Pill (Comms) | STM32CubeIDE | HAL / LL (bare-metal, no Arduino) |
| PC Visualization | Processing IDE | Processing 4.x |

---
```mermaid
flowchart TD
    P1["Phase 1 (Sensors)"] --> P2["Phase 2 (Calibration + NVS)"]
    P2 --> P3["Phase 3 (Flood Algorithm)"]
    P3 --> P4["Phase 4 (Underwater + Detach)"]
    P4 --> P5["Phase 5 (Adaptive Sampling)"]
    P5 --> P6["Phase 6 (Data Storage)"]
    
    P7["Phase 7 (STM32 Firmware)"] --> P8["Phase 8 (Integration)"]
    
    P6 --> P8
    P8 --> P9["Phase 9 (Complete System)"]
    P9 --> P10["Phase 10 (Visualization)"]

    style P8 fill:#e1f5fe,stroke:#01579b,stroke-width:2px
    style P7 fill:#fff3e0,stroke:#e65100,stroke-width:2px
```

# PHASE 1: FOUNDATION — Hardware Verification & Basic Sensor Reading

**Major milestone achieved at end of Phase 1:**
*XIAO ESP32-C3 wakes from deep sleep on a timer, powers on the sensor rail via MOSFET, reads all three I2C sensors (MPU6050 at 0x69, BMP180 at 0x77, DS3231 at 0x68), reads battery voltage via ADC, prints all readings to USB Serial debug, powers off the sensor rail, and goes back to deep sleep. This proves every sensor, every I2C address, every MOSFET switch, every pin assignment, and the deep sleep/wake cycle work on real hardware.*

---

### PHASE 1 — STEP 1: Project Structure, Pin Definitions, MOSFET Control & Deep Sleep Skeleton

```
📋 STEP OBJECTIVE:
Create the complete Arduino IDE project for XIAO ESP32-C3.
Define ALL pin assignments as named constants.
Implement MOSFET_1 (sensor rail) and MOSFET_2/3 (comms rail) GPIO control.
Implement deep sleep with timer wakeup only (ext0 added later).
Implement battery voltage ADC reading with the 1MΩ+1MΩ divider.
Implement voltage-to-percentage conversion using the 18650 discharge curve.
On each wake cycle: read battery, print to USB debug serial, go back to sleep.

🔬 OBSERVABLE OUTPUT:
USB Serial prints every 30 seconds (short sleep for testing):
    "VARUNA BOOT — Wake reason: TIMER"
    "Battery: 3.82V (57%)"
    "Sensor rail: OFF"
    "Comms rail: OFF"
    "Sleeping for 30 seconds..."
```

---

### PHASE 1 — STEP 2: I2C Bus Initialization & Device Scan (MPU6050, BMP180, DS3231)

```
📋 STEP OBJECTIVE:
Initialize I2C bus on D4 (GPIO6 SDA) and D5 (GPIO7 SCL).
Power on sensor rail (MOSFET_1 LOW).
Scan I2C bus and verify all three devices respond:
    - DS3231 at 0x68
    - MPU6050 at 0x69 (verify WHO_AM_I register = 0x68)
    - BMP180 at 0x77 (verify chip ID register = 0x55)
Report any missing sensors as faults.
Power off sensor rail after scan.
Continue deep sleep cycle from Step 1.

🔬 OBSERVABLE OUTPUT:
USB Serial prints on each wake:
    "Sensor rail ON"
    "I2C scan: 0x68 [DS3231] FOUND"
    "I2C scan: 0x69 [MPU6050] FOUND — WHO_AM_I=0x68 OK"
    "I2C scan: 0x77 [BMP180] FOUND — ChipID=0x55 OK"
    "All sensors operational"
    "Sensor rail OFF"
    "Battery: 3.82V (57%)"
    "Sleeping..."
If a sensor is disconnected, it prints MISSING and sets a fault flag.
```

---

### PHASE 1 — STEP 3: MPU6050 Raw Accelerometer Reading with Multi-Sample Averaging

```
📋 STEP OBJECTIVE:
Configure MPU6050 registers (PWR_MGMT_1, SMPLRT_DIV, CONFIG, GYRO_CFG, ACCEL_CFG).
Read 10 consecutive accelerometer samples at 20ms intervals.
Sort each axis array, discard 2 highest and 2 lowest, average remaining 6.
Print raw and averaged ax, ay, az values in g units.
Verify gravity vector: az ≈ +1.0g when sensor is flat, face up.

🔬 OBSERVABLE OUTPUT:
USB Serial prints:
    "MPU6050 configured: ±2g, ±250°/s, DLPF 44Hz"
    "10 samples captured in 200ms"
    "After outlier rejection (6 of 10):"
    "  ax = 0.012g   ay = -0.008g   az = 0.998g"
    "  Total G = 0.998 (expected ~1.00)"
Tilting the sensor shows ax, ay changing while az decreases.
```

---

### PHASE 1 — STEP 4: Tilt Angle Calculation (Tangential Mounting Formulas)

```
📋 STEP OBJECTIVE:
From averaged accelerometer data, calculate:
    accTiltX = atan2(ay, az) × 180/π
    accTiltY = atan2(ax, az) × 180/π
Print both tilt angles in degrees.
Calculate total theta = sqrt(tiltX² + tiltY²), clamp to [0, 90].
Calculate water height H = OLP × cos(θ) and drift D = OLP × sin(θ).
Use hardcoded OLP = 100.0 cm for now (NVS comes later).
Print H and D values.

🔬 OBSERVABLE OUTPUT:
USB Serial prints:
    "Tilt X: 0.71°   Tilt Y: -0.43°"
    "Theta (total tilt from vertical): 0.83°"
    "OLP = 100.00 cm"
    "Water Height H = 99.99 cm"
    "Horizontal Drift D = 1.45 cm"
Tilting the sensor 30° shows H ≈ 86.6 cm, D ≈ 50.0 cm.
```

---

### PHASE 1 — STEP 5: DS3231 RTC Timestamp Reading

```
📋 STEP OBJECTIVE:
Read DS3231 registers (seconds, minutes, hours, day, date, month, year).
Convert BCD to binary.
Convert to Unix epoch timestamp.
Print human-readable datetime and Unix timestamp.
Detect if RTC is unset (year < 2024) and flag it.

🔬 OBSERVABLE OUTPUT:
USB Serial prints:
    "DS3231 Time: 2025-01-15 14:32:07"
    "Unix Timestamp: 1736952727"
    "RTC Status: VALID"
OR if unset:
    "DS3231 Time: 2000-01-01 00:00:00"
    "RTC Status: UNSET — using boot count as relative time"
```

---

### PHASE 1 — STEP 6: BMP180 Pressure and Temperature Reading

```
📋 STEP OBJECTIVE:
Read BMP180 calibration coefficients from EEPROM registers (0xAA-0xBF).
Implement full BMP180 uncompensated temperature and pressure read sequence.
Apply BMP180 datasheet compensation algorithm (integer math).
Print pressure in hPa and temperature in °C.
Validate reading: 300 < pressure < 1200 hPa, else flag sensor failure.

🔬 OBSERVABLE OUTPUT:
USB Serial prints:
    "BMP180 Calibration loaded: AC1=8240 AC2=-1196 AC3=-14709 ..."
    "BMP180 Pressure: 1013.42 hPa"
    "BMP180 Temperature: 28.5 °C"
    "BMP180 Status: OK"
Breathing on the sensor or covering the port shows pressure increase.
```

---

### PHASE 1 — STEP 7: Complete Sensor Cycle Integration

```
📋 STEP OBJECTIVE:
Combine ALL sensor readings into a single wake cycle:
    1. Wake from deep sleep
    2. Read battery voltage + percentage
    3. Power on sensor rail
    4. Wait 50ms stabilization
    5. Verify all I2C devices (quick check, not full scan)
    6. Read MPU6050 (10 samples, outlier rejection, tilt calculation)
    7. Read DS3231 (timestamp)
    8. Read BMP180 (pressure, temperature)
    9. Power off sensor rail
    10. Print complete reading summary
    11. Deep sleep for sample_interval

This is the COMPLETE sensor reading cycle used by all future steps.

🔬 OBSERVABLE OUTPUT:
USB Serial prints a complete formatted reading block:
    "═══ VARUNA Reading ═══"
    "Time: 2025-01-15 14:32:07 (1736952727)"
    "Battery: 3.82V (57%)"
    "MPU6050: ax=0.012 ay=-0.008 az=0.998"
    "Tilt: X=0.71° Y=-0.43° θ=0.83°"
    "Water Level: H=99.99cm D=1.45cm (OLP=100.00cm)"
    "BMP180: 1013.42 hPa, 28.5°C"
    "Sensor Faults: NONE"
    "═══════════════════════"
    "Next wake in 30 seconds"
```

---

# PHASE 2: CALIBRATION & NVS PERSISTENCE

**Major milestone achieved at end of Phase 2:**
*System performs full MPU6050 calibration on first boot (gyro offsets + accelerometer zero reference), stores ALL calibration values and configuration in NVS, reloads them across reboots, and shows zero-corrected tilt angles. Subsequent boots skip calibration and load saved values. Recalibration can be triggered by a flag. All configuration keys (OLP length, thresholds, phone numbers, etc.) are initialized with defaults and persisted.*

---

### PHASE 2 — STEP 1: NVS Initialization & Default Configuration Storage

```
📋 STEP OBJECTIVE:
Initialize NVS (Preferences library on ESP32).
On first boot (boot_count key does not exist):
    Store ALL default configuration values:
        olp_length=100.0, alert_level=120.0, warning_level=180.0,
        danger_level=250.0, device_id="VARUNA-001",
        phone_1="", phone_2="", phone_3="",
        apn="internet", server_url="",
        ref_tilt_x=0.0, ref_tilt_y=0.0,
        gyro_offset_x=0.0, gyro_offset_y=0.0,
        last_tx_timestamp=0, write_pointer=0,
        detach_flag=0, detach_timestamp=0,
        current_mode=0, pressure_baseline=1013.25,
        boot_count=1
On subsequent boots: load all values from NVS, increment boot_count.
Print all loaded configuration values.

🔬 OBSERVABLE OUTPUT:
First boot:
    "NVS: First boot detected — writing defaults"
    "boot_count = 1"
    "olp_length = 100.00 cm"
    "alert_level = 120.00 cm"
    ... (all values printed)
Second boot:
    "NVS: Configuration loaded"
    "boot_count = 2"
    "olp_length = 100.00 cm"
    ... (same values, boot_count incremented)
```

---

### PHASE 2 — STEP 2: Full MPU6050 Calibration (Gyro Offsets + Accelerometer Zero Reference)

```
📋 STEP OBJECTIVE:
On first boot (ref_tilt_x and ref_tilt_y are both 0.0 in NVS):
    Perform FULL CALIBRATION:
    
    Step A — Gyroscope offset (1000 readings, 2ms apart):
        Average raw gyroX, gyroY → gyroOffsetX, gyroOffsetY
        Save to NVS
    
    Step B — Accelerometer zero reference (500 readings, 3ms apart):
        For each: validate total_g between 0.9 and 1.1
        Average valid readings → refAccX, refAccY, refAccZ
        If <100 valid: use single reading fallback
        Calculate refTiltX = atan2(refAccY, refAccZ) × 180/π
        Calculate refTiltY = atan2(refAccX, refAccZ) × 180/π
        Save refTiltX, refTiltY to NVS

On subsequent boots: load calibration from NVS, skip calibration.
Apply calibration to tilt readings: correctedTiltX = accTiltX - refTiltX

Add recalibration flag (calibration_pending in NVS).
If calibration_pending is set: run shortened calibration (300 samples each),
    update NVS, clear flag.

🔬 OBSERVABLE OUTPUT:
First boot (calibration):
    "CALIBRATION: Gyroscope offset (1000 samples)..."
    "  gyroOffsetX = 127.34, gyroOffsetY = -42.89"
    "CALIBRATION: Accelerometer reference (500 samples)..."
    "  Valid samples: 487 / 500"
    "  refAccX=0.011 refAccY=-0.007 refAccZ=0.999"
    "  refTiltX = -0.40°   refTiltY = 0.63°"
    "CALIBRATION COMPLETE — saved to NVS"

Second boot (loaded):
    "Calibration loaded from NVS"
    "  refTiltX = -0.40°   refTiltY = 0.63°"
    "Corrected Tilt: X=0.00° Y=0.00° θ=0.00°"
    "Water Height: H=100.00cm (calibrated zero)"
```

---

### PHASE 2 — STEP 3: Persistent Configuration with NVS-Backed Sensor Cycle

```
📋 STEP OBJECTIVE:
Integrate NVS into the complete sensor reading cycle:
    - OLP length from NVS (not hardcoded)
    - Calibration references from NVS
    - Boot count tracking
    - All calculations use NVS-stored parameters
    - Previous reading comparison (load previous water level from NVS
      for rate calculation in future phases)
    - Save current water level and timestamp to NVS after each reading
      (as "last_reading_level" and "last_reading_timestamp")

Verify that changing OLP length in NVS (via code modification for now)
changes the water height output proportionally.

🔬 OBSERVABLE OUTPUT:
    "═══ VARUNA Reading #5 ═══"
    "Config: OLP=100.00cm AL=120.00 WL=180.00 DL=250.00"
    "Calibration: refTiltX=-0.40° refTiltY=0.63°"
    "Raw Tilt: X=14.60° Y=10.20°"
    "Corrected Tilt: X=15.00° Y=9.57°"
    "Theta: 17.79°"
    "Water Height: H=95.23cm D=30.57cm"
    "Previous Level: 95.18cm (30 seconds ago)"
    "BMP180: 1013.42 hPa, 28.5°C"
    "Battery: 3.82V (57%)"
    "═══════════════════════════"
```

---

# PHASE 3: FLOOD DETECTION ALGORITHM

**Major milestone achieved at end of Phase 3:**
*System calculates rate of change (normalized to per-15-minute), maintains a 4-reading sustained-rise circular buffer, classifies zone from water level, applies the complete 24-combination decision matrix, determines response level (NORMAL/WATCH/WARNING/FLOOD_ALERT/CRITICAL), implements extreme rate override, implements step-down hysteresis, and prints the full decision output each cycle. The flood detection algorithm is fully operational.*

---

### PHASE 3 — STEP 1: Rate of Change Calculation (Normalized to per-15-minute)

```
📋 STEP OBJECTIVE:
Load previous reading (water level + timestamp) from NVS.
Calculate rate_of_change:
    elapsed_seconds = current_timestamp - previous_timestamp
    level_change = current_level - previous_level
    rate_per_15min = level_change × (900.0 / elapsed_seconds)
    
Classify rate:
    < 2.0 cm/15min → SLOW (0)
    2.0 to 4.99 → MODERATE (1)
    ≥ 5.0 → FAST (2)
    Negative → FALLING (separate flag)

Handle edge cases:
    - First reading (no previous data): rate = 0, category = SLOW
    - Invalid elapsed time (≤0 or >86400): rate = 0, category = SLOW

Save current level and timestamp to NVS for next cycle.
Print rate with category label.

🔬 OBSERVABLE OUTPUT:
    "Rate Calculation:"
    "  Previous: 95.18cm at 1736952697"
    "  Current:  95.23cm at 1736952727"
    "  Elapsed: 30s"
    "  Level Change: +0.05cm"
    "  Rate (normalized to /15min): +0.75 cm/15min"
    "  Category: SLOW"
```

---

### PHASE 3 — STEP 2: Zone Classification & Sustained Rise Buffer

```
📋 STEP OBJECTIVE:
Zone Classification using NVS-stored thresholds:
    waterHeight vs alert_level, warning_level, danger_level
    Assign zone: NORMAL(0), ALERT(1), WARNING(2), DANGER(3)

Sustained Rise circular buffer:
    Maintain float array[4] for last 4 water level readings
    Maintain uint8 buffer_index (0-3, wraps)
    On each reading: store current level at buffer_index, advance index
    Count rising pairs: for i=1..3, if reading[i] > reading[i-1] → rise_count++
    If rise_count >= 3: sustained = TRUE
    
    Handle circular indexing correctly (modulo 4).
    
    Cold start override:
        If boot_count <= 4 AND zone >= WARNING: force sustained = TRUE
    
    Save buffer to NVS (4 floats + index) for persistence across reboots.

Print zone, sustained flag, and buffer contents.

🔬 OBSERVABLE OUTPUT:
    "Zone Classification:"
    "  Water Height: 95.23cm"
    "  Thresholds: AL=120.00 WL=180.00 DL=250.00"
    "  Zone: NORMAL (0)"
    ""
    "Sustained Rise Buffer: [94.90, 95.05, 95.18, 95.23]"
    "  Rising pairs: 3/3 → SUSTAINED = YES"
    ""
    "Boot count: 5 (cold start override: NO)"
```

---

### PHASE 3 — STEP 3: Complete Decision Matrix (24 Combinations)

```
📋 STEP OBJECTIVE:
Implement the FULL 24-combination decision matrix as a function:
    Input: zone (0-3), rate_category (0-2), sustained (bool)
    Output: response_level (0-4: NORMAL, WATCH, WARNING, FLOOD_ALERT, CRITICAL)

Implement as explicit if-else chain or lookup table.
Every single combination must be present — no shortcut logic that might
miss a combination.

Implement extreme rate override:
    If rate_per_15min > 30.0 AND zone >= ALERT (1):
        response_level = CRITICAL (4) regardless of other factors

Print the full decision path showing which row of the matrix was matched.

🔬 OBSERVABLE OUTPUT:
    "Decision Matrix:"
    "  Zone: NORMAL (0)"
    "  Rate: SLOW (0)  — 0.75 cm/15min"
    "  Sustained: YES"
    "  Matrix Row: NORMAL + SLOW + YES → NORMAL"
    "  Extreme Rate Override: NO (rate 0.75 < 30.0)"
    "  RESPONSE LEVEL: NORMAL (0)"

When simulated with WARNING zone:
    "  Zone: WARNING (2)"
    "  Rate: FAST (2) — 6.30 cm/15min"
    "  Sustained: YES"
    "  Matrix Row: WARNING + FAST + YES → CRITICAL"
    "  RESPONSE LEVEL: CRITICAL (4)"
```

---

### PHASE 3 — STEP 4: Step-Down Hysteresis & ALL CLEAR Logic

```
📋 STEP OBJECTIVE:
Implement step-down counters (stored in NVS):
    consecutive_below_danger, consecutive_below_warning,
    consecutive_below_alert, consecutive_normal_stable

Update counters after each reading based on current zone and rate.
Apply step-down rules:
    CRITICAL → FLOOD_ALERT: 4 below danger + rate FALLING/SLOW
    FLOOD_ALERT → WARNING: 4 below warning
    WARNING → WATCH: 4 below alert
    WATCH → NORMAL: 8 in NORMAL zone + SLOW + not sustained

Track previous_response_level (in NVS) for transition detection.
Detect escalation transitions (new WARNING, new CRITICAL, etc.)
    → Set force_transmit flag.
Detect de-escalation from ≥WARNING to NORMAL:
    → Set all_clear flag (triggers ALL CLEAR SMS later).

Track peak_level and peak_timestamp (in NVS) for ALL CLEAR message.

Print step-down counter state and any transition events.

🔬 OBSERVABLE OUTPUT:
    "Step-Down Counters:"
    "  Below DANGER: 12 consecutive (need 4)"
    "  Below WARNING: 8 consecutive (need 4)"
    "  Below ALERT: 5 consecutive (need 4)"
    "  Normal+Stable: 3 consecutive (need 8)"
    ""
    "Response Level Transition: WATCH → NORMAL (after 8 stable)"
    "ALL CLEAR condition met!"
    "  Peak was: 185.30cm at 2025-01-15 14:32:07"
    "  Current: 95.23cm — NORMAL"
    "force_transmit = YES (all clear)"
```

---

# PHASE 4: UNDERWATER & DETACHMENT DETECTION

**Major milestone achieved at end of Phase 4:**
*System detects buoy submersion via BMP180 pressure deviation from rolling baseline, detects tether detachment via sustained low-theta with normal pressure, detects buoy inversion via negative accZ, and detects sudden disturbance via large theta jumps. All detection flags are stored in NVS and trigger force-transmit. The cross-validation logic (MPU6050 + BMP180) correctly distinguishes submersion from detachment.*

---

### PHASE 4 — STEP 1: BMP180 Pressure Baseline & Underwater Detection

```
📋 STEP OBJECTIVE:
Implement rolling 48-reading pressure baseline:
    float pressure_history[48] stored in NVS (or LittleFS as small file)
    uint8 pressure_history_index stored in NVS
    On each reading with valid pressure:
        If deviation < 5 hPa: update baseline
        Calculate baseline = average of non-zero entries in history array

Implement underwater detection:
    deviation = current_pressure - pressure_baseline
    < 5 hPa: NORMAL — update baseline
    5-8 hPa: LOG, no action
    8-15 hPa: SUSPECT — set flag, reduce sample interval next cycle
    > 15 hPa: CONFIRMED UNDERWATER — trigger alert, force transmit
    > 1100 hPa absolute: deep submersion alert
    < 300 or > 1200 hPa: SENSOR FAILURE — mark BMP unavailable

Print pressure analysis each cycle.

🔬 OBSERVABLE OUTPUT:
Normal:
    "BMP180 Pressure Analysis:"
    "  Current: 1013.42 hPa"
    "  Baseline: 1013.15 hPa (48 samples)"
    "  Deviation: +0.27 hPa → NORMAL"
    "  Underwater: NO"
    "  Baseline updated"

If pressure port is covered (simulating submersion):
    "  Current: 1032.80 hPa"
    "  Baseline: 1013.15 hPa"
    "  Deviation: +19.65 hPa → CONFIRMED UNDERWATER"
    "  Estimated depth: ~20.0cm"
    "  underwater_flag = TRUE"
    "  force_transmit = TRUE"
```

---

### PHASE 4 — STEP 2: Tether Detachment & Tamper Detection

```
📋 STEP OBJECTIVE:
Implement detachment detection using MPU6050 theta + BMP180 cross-validation:

    Maintain low_theta_count (RAM, reset on boot)
    Maintain last_normal_theta (RAM)
    
    Each cycle:
        If theta > 10°: last_normal_theta = theta, low_theta_count = 0
        If theta < 3° AND NOT underwater: low_theta_count++
        Else: low_theta_count = 0
        
        If low_theta_count >= 6 AND last_normal_theta > 10°:
            TETHER DETACHED CONFIRMED
            Set detach_flag + detach_timestamp in NVS
            Set force_transmit, gps_needed flags
    
    Buoy inversion: if az < -0.3g → tamper_flag, force_transmit, gps_needed
    
    Sudden disturbance: if abs(theta - previous_theta) > 15° → 
        monitor_flag, increase sampling for 30 minutes

Implement cross-validation table:
    theta<3° + pressure NORMAL → possible detachment (count)
    theta<3° + pressure ELEVATED → submersion (NOT detachment)
    az < -0.3 → inversion/tamper

Print detection status each cycle.

🔬 OBSERVABLE OUTPUT:
Normal:
    "Detachment Check:"
    "  theta=17.79° → Normal tilt (reset low count)"
    "  low_theta_count = 0"
    "  last_normal_theta = 17.79°"
    "  accZ = 0.95g → Upright"
    "  Detach: NO   Tamper: NO   Underwater: NO"

Detachment scenario (theta stays near 0° for 6+ readings):
    "  theta=1.20° → LOW THETA (count: 6)"
    "  pressure deviation: +0.3 hPa → Normal (NOT underwater)"
    "  last_normal_theta was 22.5°"
    "  *** TETHER DETACHMENT CONFIRMED ***"
    "  detach_flag=TRUE  gps_needed=TRUE  force_transmit=TRUE"
```

---

# PHASE 5: ADAPTIVE SAMPLING & POWER MANAGEMENT

**Major milestone achieved at end of Phase 5:**
*System dynamically adjusts sample_interval and transmit_interval based on the complete adaptive table (zone × rate × battery level × detachment state), implements all battery conservation modes (CONSERVE, HIBERNATE, EMERGENCY), correctly prioritizes flood detection over power saving, and determines whether each cycle requires transmission. Deep sleep interval changes are visible in the debug output.*

---

### PHASE 5 — STEP 1: Battery-Aware Adaptive Interval Selection

```
📋 STEP OBJECTIVE:
Implement the COMPLETE adaptive sampling table from the specification.
All rows must be present:
    - NORMAL + SLOW at various battery levels
    - NORMAL + MOD/FAST at various battery levels
    - ALERT at various battery levels
    - WARNING/FAST+SUSTAINED at various battery levels
    - DANGER at various battery levels
    - LOW_BATT + each zone
    - VERY_LOW_BATT + each zone
    - CRITICAL_BATT + each zone
    - DETACHMENT modes (first 24h vs after 24h)

Input: zone, rate_category, sustained, battery_percentage, detach_flag,
       detach_timestamp, current_timestamp
Output: sample_interval_min, transmit_interval_min, transmit_mode
        (GPRS, SMS_ONLY, LOCAL_ONLY)

Print selected intervals and the rule that matched.

🔬 OBSERVABLE OUTPUT:
    "Adaptive Sampling:"
    "  Zone: NORMAL  Rate: SLOW  Sustained: NO"
    "  Battery: 57% (> 50%)"
    "  Detached: NO"
    "  Rule: NORMAL+SLOW+BATT>50%"
    "  → sample_interval = 30 min"
    "  → transmit_interval = 480 min (8 hours)"
    "  → transmit_mode = GPRS"

Low battery scenario:
    "  Battery: 12% (< 15%)"
    "  Zone: NORMAL"
    "  Rule: VERY_LOW_BATT+NORMAL"
    "  → sample_interval = 120 min"
    "  → transmit_interval = 2880 min (48 hours)"
```

---

### PHASE 5 — STEP 2: Transmission Decision Logic & Force Transmit

```
📋 STEP OBJECTIVE:
Determine if current cycle requires transmission:
    Load last_tx_timestamp from NVS
    Calculate minutes_since_tx
    If minutes_since_tx >= transmit_interval: transmission_needed = TRUE
    
Force transmit overrides:
    - Response level escalated to >= WARNING (compare with previous)
    - Underwater flag newly set (compare with previous)
    - Detachment flag newly set
    - Tamper flag set
    - ALL CLEAR condition (descalation from >=WARNING to NORMAL)
    
Track previous states in NVS for comparison:
    previous_response_level, previous_underwater_flag

Print transmission decision with reasoning.
Do NOT actually transmit yet (comms rail code is Phase 6).
Instead, just print what WOULD happen.

🔬 OBSERVABLE OUTPUT:
Normal cycle (no transmit):
    "Transmission Decision:"
    "  Last TX: 120 minutes ago"
    "  Transmit interval: 480 minutes"
    "  Force overrides: NONE"
    "  Decision: NO TRANSMISSION (next in 360 minutes)"

Escalation cycle:
    "Transmission Decision:"
    "  Last TX: 5 minutes ago"
    "  Transmit interval: 15 minutes"
    "  Force override: ESCALATION (NORMAL → WARNING)"
    "  Decision: FORCE TRANSMIT NOW"
```

---

### PHASE 5 — STEP 3: Deep Sleep Interval Control & Complete Sensor Cycle

```
📋 STEP OBJECTIVE:
Apply adaptive sample_interval to actual deep sleep timer.
Convert sample_interval_min to microseconds for esp_sleep_enable_timer_wakeup().
Integrate ALL features from Phases 1-5 into the unified wake cycle:
    1. Wake, check wake reason
    2. Read battery
    3. Power on sensors, read all (MPU6050, BMP180, DS3231)
    4. Apply calibration, calculate water level
    5. Calculate rate, zone, sustained rise
    6. Apply decision matrix → response level
    7. Apply step-down hysteresis
    8. Check underwater, detachment, tamper
    9. Determine adaptive intervals
    10. Determine transmission need
    11. Save all state to NVS
    12. Power off sensors
    13. Deep sleep for sample_interval

Print complete status block with all values.
Verify that changing conditions (tilt, pressure) cause
intervals to change in real-time.

🔬 OBSERVABLE OUTPUT:
    "═══════════════════════════════════════"
    "  VARUNA Reading #12"
    "  Time: 2025-01-15 14:35:07"
    "  Battery: 3.82V (57%)"
    "─── Sensors ───"
    "  θ=17.79° → H=95.23cm D=30.57cm"
    "  BMP: 1013.42 hPa, 28.5°C"
    "─── Algorithm ───"
    "  Rate: +0.75 cm/15min (SLOW)"
    "  Zone: NORMAL | Sustained: YES"
    "  Response: NORMAL (0)"
    "  Step-down: N/A"
    "─── Detection ───"
    "  Underwater: NO | Detach: NO | Tamper: NO"
    "─── Intervals ───"
    "  Sample: 30 min | Transmit: 480 min | Mode: GPRS"
    "  Transmit this cycle: NO (next in 360 min)"
    "═══════════════════════════════════════"
    "Sleeping for 1800 seconds (30 min)..."
```

---

# PHASE 6: DATA STORAGE (LittleFS)

**Major milestone achieved at end of Phase 6:**
*Every sensor reading is stored as a 32-byte binary record in LittleFS. Records are written in a circular buffer pattern across the 2MB partition (65,536 records max). The write pointer persists across reboots via NVS. Records include all data fields (timestamp, water level, rate, zone, response, battery, tilt, pressure, temperature, flags, GPS). Untransmitted records can be scanned and serialized for transmission. Data survives reboots.*

---

### PHASE 6 — STEP 1: LittleFS Mount & 32-Byte Record Write

```
📋 STEP OBJECTIVE:
Initialize LittleFS on the 2MB partition.
If mount fails: format and remount.
Define the 32-byte record structure as a packed struct.
After each sensor reading cycle: pack all current values into a record.
Write record to file at position (write_pointer × 32).
Advance write_pointer, wrap at 65536.
Save write_pointer to NVS.
Print record hex dump for verification.

🔬 OBSERVABLE OUTPUT:
    "LittleFS: Mounted (2MB)"
    "Record #42 written at byte offset 1344:"
    "  HEX: 67 8E 1A 67 0B 25 00 4B 00 00 39 0E ..."
    "  Fields: ts=1736952727 level=95.23cm rate=0.75"
    "          zone=0 resp=0 batt=3.820V"
    "          tiltX=15.00° tiltY=9.57° θ=17.79°"
    "          press=1013.42 temp=28.5°C"
    "          flags=0b00000001 (sustained)"
    "  write_pointer: 43"
```

---

### PHASE 6 — STEP 2: Record Reading, Scanning & Transmission Queue

```
📋 STEP OBJECTIVE:
Implement read_record(index) to retrieve a 32-byte record by index.
Implement scan for untransmitted records:
    Scan backwards from write_pointer
    Find records where flags bit 7 (tx_success) == 0
    Collect up to 48 record indices (oldest first)
    
Implement mark_transmitted(index):
    Read record, set bit 7 of flags byte, write back

Implement record-to-CSV serialization:
    Format: "R:<ts>,<lvl>,<rate>,<zone>,<resp>,<batt>,<press>,<temp>,<flags>\n"
    This is the format that will be sent to STM32 in Phase 8.

Print pending record count and first/last pending timestamps.

🔬 OBSERVABLE OUTPUT:
    "Data Storage Status:"
    "  Total records: 42"
    "  Untransmitted: 42 (all since boot — no comms yet)"
    "  Oldest pending: 2025-01-15 14:00:07 (#0)"
    "  Newest pending: 2025-01-15 14:35:07 (#41)"
    "  Batch for next TX: 42 records"
    ""
    "Sample serialized record:"
    "  R:1736952727,95.23,0.75,0,0,3.820,1013.42,28.5,1"
```

---

# PHASE 7: STM32 BLUE PILL FIRMWARE (Communication Slave)

**Major milestone achieved at end of Phase 7:**
*STM32 Blue Pill firmware is complete. It boots, sends "READY" on UART1, receives data dump commands from XIAO, initializes SIM800L via AT commands, sends GPRS HTTP POST with JSON payload, sends SMS alerts, requests GPS fix from NEO-6M, checks for incoming SMS commands, relays commands to XIAO, signals XIAO via interrupt wire (PB0), and shuts down gracefully. All AT command sequences include timeout handling and error reporting. This is a fully functional communication slave.*

---

### PHASE 7 — STEP 1: STM32CubeIDE Project, UART Init & Handshake

```
📋 STEP OBJECTIVE:
Create STM32CubeIDE project for STM32F103C8T6 (Blue Pill).
Configure using HAL:
    - System clock: 72 MHz (HSE 8MHz + PLL)
    - UART1 (PA9/PA10): 115200 baud → XIAO communication
    - UART2 (PA2/PA3): 9600 baud → SIM800L
    - UART3 (PB11 RX only): 9600 baud → GPS
    - PB0: GPIO output (interrupt wire to XIAO)
    - PB1: GPIO output (SIM800L RST/power key)
    - PC13: GPIO output (onboard LED, active LOW)

Boot sequence:
    1. Init all peripherals
    2. LED ON
    3. Send "READY\n" on UART1
    4. Wait for "ACK\n" from XIAO (timeout 10 seconds)
    5. Enter command wait loop

Print all UART activity to UART1 for debugging during development.

🔬 OBSERVABLE OUTPUT (on XIAO USB serial, relayed from UART1):
    STM32 sends "READY\n"
    XIAO sees it, sends "ACK\n"
    STM32 LED blinks to confirm handshake complete
    STM32 waits for commands
```

---

### PHASE 7 — STEP 2: SIM800L Initialization & AT Command Engine

```
📋 STEP OBJECTIVE:
Implement AT command send/receive engine:
    send_at_command(command, expected_response, timeout_ms)
    Returns: SUCCESS, TIMEOUT, ERROR

Implement SIM800L power-on sequence:
    Toggle PB1 (power key) HIGH 1s then LOW
    Wait 3s for boot
    "AT" → OK (3 retries)
    "ATE0" → OK
    "AT+CMGF=1" → OK
    "AT+CPIN?" → "+CPIN: READY"
    
Implement network registration wait:
    Loop 15 times, 2s apart:
    "AT+CREG?" → look for "+CREG: 0,1" or "+CREG: 0,5"
    
Get signal strength:
    "AT+CSQ" → parse RSSI value

Report results to XIAO via UART1:
    "SIGNAL:<rssi>\n" on success
    "TX:FAIL:1\n" if SIM800L unresponsive
    "TX:FAIL:2\n" if no network registration

🔬 OBSERVABLE OUTPUT (on XIAO USB serial, received from STM32):
    "SIM800L init: AT → OK"
    "SIM800L echo off: OK"
    "SIM800L SMS mode: OK"
    "SIM card: READY"
    "Network registration: OK (attempt 3)"
    "Signal strength: RSSI=15"
    "SIGNAL:15"
```

---

### PHASE 7 — STEP 3: GPRS HTTP POST Transmission

```
📋 STEP OBJECTIVE:
Implement complete GPRS sequence:
    Receive "TX:DATA:<count>\n" from XIAO
    Receive N "R:<csv>\n" records
    Receive "TX:END\n"
    
    Build JSON payload in RAM buffer (~4KB max):
        Construct the exact JSON structure specified in Feature 10
        Include device metadata, battery, config, system info
        Include all reading records as JSON array
    
    Execute GPRS AT sequence:
        SAPBR configure + open
        HTTPINIT, HTTPPARA (URL, content type)
        HTTPDATA (length), send JSON bytes
        HTTPACTION=1, wait for response
        Parse HTTP status code
        HTTPTERM, SAPBR close
    
    Report result:
        "TX:OK:<http_status>\n" or "TX:FAIL:<code>\n"

🔬 OBSERVABLE OUTPUT (on XIAO USB serial):
    (XIAO sends data dump)
    STM32 responds:
    "GPRS: Bearer opened"
    "HTTP: POST 1247 bytes to <url>"
    "HTTP: Status 200, Response 42 bytes"
    "TX:OK:200"
    
    OR on failure:
    "HTTP: Status 500"
    "TX:FAIL:5"
```

---

### PHASE 7 — STEP 4: SMS Send & Receive with Command Relay

```
📋 STEP OBJECTIVE:
Implement SMS sending:
    Receive "SMS:ALERT:<phone>,<message>\n" from XIAO
    "AT+CMGF=1" → OK
    "AT+CMGS=\"<phone>\"" → > prompt
    Send message body
    Send 0x1A (Ctrl+Z)
    Wait for "+CMGS:" (30s timeout)
    Reply "SMS:OK\n" or "SMS:FAIL\n"

Implement SMS receiving (check after GPRS):
    "AT+CMGL=\"REC UNREAD\"" → parse all unread SMS
    For each SMS:
        Extract sender number and message body
        Check if sender matches authorized numbers
        (authorized numbers received from XIAO in data header)
        If authorized: relay command to XIAO "CMD:<body>\n"
        Delete SMS: "AT+CMGD=<index>"

🔬 OBSERVABLE OUTPUT:
SMS send:
    "SMS: Sending to +91XXXXXXXXXX"
    "SMS: +CMGS:42 — sent OK"
    "SMS:OK"

SMS receive:
    "SMS: 2 unread messages"
    "SMS[1]: From +91XXXXXXXXXX Body='STATUS'"
    "  → Authorized sender — relaying"
    "CMD:STATUS"
    "SMS[1]: Deleted"
    "SMS[2]: From +91YYYYYYYYYY Body='spam'"
    "  → Unauthorized — deleting"
    "SMS[2]: Deleted"
```

---

### PHASE 7 — STEP 5: GPS Fix Acquisition

```
📋 STEP OBJECTIVE:
Implement GPS fix acquisition:
    Receive "GPS:FIX\n" from XIAO
    Parse NMEA sentences from UART3 (GPS):
        Look for $GPRMC or $GNRMC (fix status + lat/lon)
        Look for $GPGGA or $GNGGA (fix quality + HDOP)
        Parse latitude/longitude from NMEA format to decimal degrees
    
    Timeout: 90 seconds
    
    Reply:
        "GPS:OK:<lat>,<lon>,<hdop>\n" on success
        "GPS:NOFIX\n" on timeout

Implement NMEA sentence parser:
    Validate checksum (XOR of all characters between $ and *)
    Extract fields by comma position
    Convert NMEA lat/lon (DDMM.MMMMM) to decimal degrees

🔬 OBSERVABLE OUTPUT:
    "GPS: Waiting for fix (90s timeout)..."
    "GPS: $GPRMC sentence received — Status=A (valid)"
    "GPS: Lat=28.61394 Lon=77.20902 HDOP=1.2"
    "GPS:OK:28.61394,77.20902,1.2"
    
    OR:
    "GPS: Timeout after 90 seconds"
    "GPS:NOFIX"
```

---

### PHASE 7 — STEP 6: Complete STM32 Command Loop & Shutdown

```
📋 STEP OBJECTIVE:
Integrate ALL STM32 handlers into the main command loop:
    Wait for commands on UART1
    Parse command type and dispatch to handler:
        "TX:DATA:..." → GPRS transmission handler
        "SMS:ALERT:..." → SMS send handler
        "GPS:FIX" → GPS fix handler
        "ACK" → Acknowledgment (no action)
        "CMD:ACK" → Command acknowledgment
        "SHUTDOWN" → Shutdown handler
    
    Shutdown handler:
        "AT+CPOWD=1" → SIM800L power down
        Send "SHUTDOWN:OK\n" to XIAO
        LED OFF
        Enter infinite WFI loop (wait for power cut)
    
    After completing TX/SMS/GPS:
        Pulse PB0 LOW for 100ms (interrupt XIAO to wake)
        Wait for XIAO response
        Wait for "SHUTDOWN\n"
        Execute shutdown

Error handling:
    If any AT command fails 3 times → report failure and continue
    If UART1 receives garbage → ignore, wait for valid command

🔬 OBSERVABLE OUTPUT:
Complete transmission cycle:
    STM32 receives data, transmits GPRS, checks SMS, reports results
    Pulses interrupt to XIAO
    Receives "SHUTDOWN"
    Sends "SHUTDOWN:OK"
    LED off, enters WFI
    Power cut by XIAO
```

---

# PHASE 8: XIAO ↔ STM32 INTEGRATION (Transmission Cycle)

**Major milestone achieved at end of Phase 8:**
*XIAO powers on the comms rail, performs the complete handshake with STM32, dumps pending data records via UART, sends SMS alert commands when warranted, requests GPS when needed, goes to deep sleep while STM32 handles SIM800L, wakes on STM32 interrupt, receives results, processes any incoming SMS commands, marks transmitted records as successful, shuts down STM32, powers off comms rail, and returns to normal deep sleep cycle. The full end-to-end data path works: Sensor → XIAO → STM32 → SIM800L → Server.*

---

### PHASE 8 — STEP 1: XIAO Comms Rail Power-On & STM32 Handshake

```
📋 STEP OBJECTIVE:
When transmission is needed (determined in Phase 5):
    Power on comms rail: D1 (GPIO3) → LOW
    Wait 200ms for STM32 + SIM800L power stabilization
    Listen for "READY\n" on UART1 (timeout 5 seconds)
    If timeout: retry power cycle (D1 HIGH 1s, D1 LOW, wait again)
    If still timeout: log comms failure, power off, sleep
    Send "ACK\n" on UART1
    Handshake complete

Configure UART1 on XIAO: TX=D8/GPIO8, RX=D9/GPIO9 at 115200 baud.
Implement line-by-line UART reading with timeout.

🔬 OBSERVABLE OUTPUT:
    "Comms rail: ON"
    "Waiting for STM32 READY..."
    "Received: READY"
    "Sent: ACK"
    "Handshake complete"
```

---

### PHASE 8 — STEP 2: Data Dump & SMS Alert Commands to STM32

```
📋 STEP OBJECTIVE:
After handshake:
    Scan LittleFS for untransmitted records (up to 48)
    Send "TX:DATA:<count>\n"
    For each record:
        Serialize to CSV format
        Send "R:<csv>\n"
    Send "TX:END\n"

If response_level >= WARNING (and this is a new escalation):
    Determine alert tier:
        WARNING → phone_1 only
        FLOOD_ALERT → phone_1 + phone_2
        CRITICAL → phone_1 + phone_2 + phone_3
    Format SMS message according to specified formats
    Send "SMS:ALERT:<phone>,<message>\n" for each phone

If underwater_flag newly set:
    Send underwater SMS

If all_clear_flag set:
    Send ALL CLEAR SMS to all phones that received original alert

If gps_needed:
    Send "GPS:FIX\n"

🔬 OBSERVABLE OUTPUT:
    "Data dump: 12 pending records"
    "Sent: TX:DATA:12"
    "Sent: R:1736952727,95.23,0.75,0,0,3.820,1013.42,28.5,1"
    ... (12 records)
    "Sent: TX:END"
    ""
    "Alert required: WARNING"
    "  Tier 1: +91XXXXXXXXXX"
    "Sent: SMS:ALERT:+91XXXXXXXXXX,[VARUNA-WARN] Site:VARUNA-001..."
    ""
    "GPS not needed"
```

---

### PHASE 8 — STEP 3: Deep Sleep During STM32 Transmission & Interrupt Wake

```
📋 STEP OBJECTIVE:
After sending all commands to STM32:
    XIAO goes to deep sleep to save power while STM32 handles SIM800L
    Configure wake sources:
        ext0 wakeup on GPIO5 (D3) falling edge — STM32 interrupt
        Timer wakeup 120 seconds — safety timeout
    
    Enter deep sleep
    
On wake from ext0 (STM32 PB0 pulse):
    Read all pending UART1 messages from STM32:
        "TX:OK:<id>\n" → mark records as transmitted
        "TX:FAIL:<code>\n" → keep records pending
        "SMS:OK\n" → log success
        "SMS:FAIL\n" → log failure
        "GPS:OK:<lat>,<lon>,<hdop>\n" → store in NVS + current record
        "GPS:NOFIX\n" → log
        "CMD:<command>\n" → process command
        "SIGNAL:<rssi>\n" → store signal strength
    
    Send "CMD:ACK\n" for any received commands
    Send "SHUTDOWN\n"
    Wait for "SHUTDOWN:OK\n"
    Power off comms rail: D1 → HIGH
    
On wake from timer (120s safety timeout):
    STM32 may have hung. Power off comms rail anyway.
    Mark transmission as failed.

🔬 OBSERVABLE OUTPUT:
    "Sleeping while STM32 transmits..."
    "...woke by STM32 interrupt (GPIO5)"
    "Received: TX:OK:200"
    "  → Marking 12 records as transmitted"
    "Received: SMS:OK"
    "Received: SIGNAL:15"
    "Received: CMD:STATUS"
    "  → Processing STATUS command"
    "Sent: CMD:ACK"
    "Sent: SHUTDOWN"
    "Received: SHUTDOWN:OK"
    "Comms rail: OFF"
    "Next sleep: 1800 seconds (30 min)"
```

---

### PHASE 8 — STEP 4: SMS Command Processing on XIAO

```
📋 STEP OBJECTIVE:
Process SMS commands relayed from STM32 as "CMD:<body>\n":

    "CMD:STATUS" → queue status reply SMS for next transmit cycle
    "CMD:SET THRESHOLD <a>,<w>,<d>" → parse, validate, save to NVS
    "CMD:SET OLP <value>" → parse, validate, save to NVS
    "CMD:CALIBRATE" → set calibration_pending flag in NVS
    "CMD:RESET" → queue reply SMS, then esp_restart()
    "CMD:MODE <name>" → change operating mode
    "CMD:LOGS <n>" → queue last n readings as reply SMS
    "CMD:GPS" → set gps_needed flag for next transmit
    "CMD:HELP" → queue help text as reply SMS

Commands that need reply SMS: set a reply_sms_pending flag with the
message content, to be sent during the NEXT transmission cycle.

🔬 OBSERVABLE OUTPUT:
    "Processing CMD: SET THRESHOLD 100,160,200"
    "  Parsed: alert=100.00 warning=160.00 danger=200.00"
    "  Validation: OK (0 < 100 < 160 < 200)"
    "  Saved to NVS"
    "  Reply queued: 'THRESHOLDS SET: A=100 W=160 D=200'"

    "Processing CMD: CALIBRATE"
    "  calibration_pending flag set in NVS"
    "  Reply queued: 'CALIBRATION SCHEDULED FOR NEXT CYCLE'"
```

---

# PHASE 9: COMPLETE SYSTEM INTEGRATION & BOOT SMS

**Major milestone achieved at end of Phase 9:**
*The entire system operates autonomously end-to-end. On boot: calibrates (if needed), reads sensors, runs flood algorithm, stores data, transmits boot SMS, then enters the infinite wake-read-decide-store-transmit-sleep cycle. All features from Phases 1-8 work together. SMS commands work bidirectionally. GPRS uploads work. Alerts escalate and de-escalate correctly. Battery management adapts intervals. The device can be deployed and operate for months unattended.*

---

### PHASE 9 — STEP 1: First-Boot Sequence with Boot SMS

```
📋 STEP OBJECTIVE:
Implement the COMPLETE first-boot sequence as specified in Section 6.1:
    1. Initialize all peripherals
    2. Check NVS for existing config (first boot vs restart)
    3. Read battery — emergency hibernate if < 3.4V
    4. Power on sensor rail, verify all sensors
    5. Configure MPU6050
    6. Full calibration (if first boot or calibration_pending)
    7. Initial BMP180 reading for pressure baseline
    8. Initialize LittleFS
    9. Initialize circular buffers
    10. Power off sensor rail
    11. Power on comms rail
    12. Send boot SMS: "[VARUNA-BOOT] Site:VARUNA-001 Online Batt:57% FW:1.0 Mode:NORMAL"
    13. Send initial data via GPRS
    14. Check for pending SMS commands
    15. Power off comms rail
    16. Set initial intervals
    17. Enter deep sleep

🔬 OBSERVABLE OUTPUT:
    "═══ VARUNA v1.0 — First Boot ═══"
    "NVS: First boot — writing defaults"
    "Battery: 3.82V (57%) — OK"
    "Sensor rail: ON"
    "I2C: DS3231(0x68) OK  MPU6050(0x69) OK  BMP180(0x77) OK"
    "Calibrating gyroscope (1000 samples)..."
    "Calibrating accelerometer (500 samples)..."
    "Calibration saved to NVS"
    "BMP180 baseline: 1013.25 hPa"
    "LittleFS: Mounted (2MB)"
    "Sensor rail: OFF"
    "Comms rail: ON"
    "STM32 handshake: OK"
    "Boot SMS sent: OK"
    "Initial GPRS upload: OK (1 record)"
    "SMS check: 0 unread commands"
    "Comms rail: OFF"
    "Mode: NORMAL | Sample: 30min | Transmit: 480min"
    "Entering deep sleep..."
```

---

### PHASE 9 — STEP 2: Complete Unified Wake Cycle

```
📋 STEP OBJECTIVE:
Implement the FINAL unified wake cycle that integrates EVERYTHING:
    This is the main loop that runs for the lifetime of the device.
    
    1. Wake from deep sleep
    2. Check wake reason (timer vs ext0 vs reset)
    3. Read battery (hibernate if critical)
    4. Power on sensors, full read cycle (MPU6050 + BMP180 + DS3231)
    5. Apply calibration, calculate water level
    6. Calculate rate, zone, sustained rise
    7. Apply 24-combination decision matrix
    8. Apply step-down hysteresis
    9. Check underwater, detachment, tamper
    10. Determine adaptive intervals
    11. Determine transmission need + force overrides
    12. Store record to LittleFS
    13. Power off sensors
    14. IF transmission needed:
        a. Power on comms rail
        b. Handshake with STM32
        c. Dump data
        d. Send alerts/SMS/GPS as needed
        e. Deep sleep with ext0 wake
        f. (Wakes on STM32 interrupt)
        g. Receive results
        h. Process commands
        i. Shutdown STM32
        j. Power off comms rail
    15. Set deep sleep timer to sample_interval
    16. Enter deep sleep

Handle ALL transitions between steps cleanly.
Ensure no feature from any previous phase is missing.

🔬 OBSERVABLE OUTPUT:
A complete cycle with transmission:
    "═══ VARUNA Wake #47 (TIMER) ═══"
    "Battery: 3.80V (55%)"
    "── Sensors ──"
    "  θ=22.15° H=92.71cm D=37.66cm"
    "  BMP: 1013.55hPa 29.0°C"
    "  Time: 2025-01-15 18:30:07"
    "── Algorithm ──"
    "  Rate: +1.20 cm/15min (SLOW)"
    "  Zone: NORMAL | Sustained: YES | Response: NORMAL"
    "  Hysteresis: stable×3"
    "── Detection ──"
    "  UW:NO DT:NO TMP:NO"
    "── Intervals ──"
    "  Sample:30min TX:480min Mode:GPRS"
    "── Storage ──"
    "  Record #47 written (32 bytes)"
    "── Transmission ──"
    "  Due: YES (480 min elapsed)"
    "  Pending records: 16"
    "  Powering on comms..."
    "  Handshake: OK"
    "  Data dump: 16 records"
    "  Sleeping for STM32..."
    "  ...woke (STM32 interrupt)"
    "  TX:OK — 16 records marked transmitted"
    "  SMS:OK"
    "  SIGNAL:18"
    "  Comms: OFF"
    "═══ Sleep 1800s (30min) ═══"
```

---

### PHASE 9 — STEP 3: Fault Recovery & Restart Handling

```
📋 STEP OBJECTIVE:
Implement robust fault recovery:
    - Sensor rail power cycle on I2C failure (retry 3 times)
    - MPU6050 re-init if WHO_AM_I fails
    - BMP180 graceful degradation (continue without pressure)
    - DS3231 fallback to boot_count if RTC time invalid
    - STM32 comms retry (3 power cycles) before giving up
    - SIM800L consecutive failure tracking (after 10 failures,
      reduce transmit attempts to every 4 hours)
    - Flash write error recovery (re-mount LittleFS)
    - NVS corruption detection (validate ranges on load)
    - Brownout recovery (check wake reason UNDEFINED → full init)

Add sensor_status string tracking:
    "ALL_OK", "MPU6050_FAIL", "BMP180_FAIL", "RTC_UNSET",
    "COMMS_FAIL", "MULTI_FAIL"

Send sensor fault SMS when new fault is detected.

🔬 OBSERVABLE OUTPUT:
Normal operation:
    "Sensor status: ALL_OK"

MPU6050 disconnected:
    "MPU6050: WHO_AM_I read failed — retry 1/3"
    "MPU6050: WHO_AM_I read failed — retry 2/3"
    "MPU6050: WHO_AM_I read failed — retry 3/3"
    "MPU6050: FAULT — using last known reading"
    "Sensor status: MPU6050_FAIL"
    "Fault SMS queued: [VARUNA-FAULT] Sensor:MPU6050 Status:FAIL"

BMP180 disconnected:
    "BMP180: Not responding — disabled"
    "Underwater detection: DISABLED"
    "Sensor status: BMP180_FAIL"
```

---

# PHASE 10: PROCESSING VISUALIZATION (PC Dashboard)

**Major milestone achieved at end of Phase 10:**
*A Processing IDE sketch connects to the XIAO via USB serial, displays real-time water level, tilt angles, system status, and a scrolling graph. The visualization includes the animated buoy-rope-anchor diagram, the data panel, the OLP input box, and the recalibrate button. This provides a complete visual dashboard for development testing and field calibration verification.*

---

### PHASE 10 — STEP 1: Processing Serial Connection & Data Parsing

```
📋 STEP OBJECTIVE:
Create Processing sketch that:
    - Connects to XIAO USB serial port at 115200 baud
    - Reads incoming debug lines
    - Parses the "═══ VARUNA Reading ═══" blocks
    - Extracts: theta, water height, tilt X, tilt Y, OLP,
      horizontal drift, pressure, temperature, battery,
      zone, response level, rate, sustained
    - Stores values in global variables for display
    - Handles STATUS: and ERROR: prefixed lines separately

Processing 4.x with Serial library.

🔬 OBSERVABLE OUTPUT:
    Processing console shows parsed values updating in real-time
    No visual display yet — just console parsing verification
```

---

### PHASE 10 — STEP 2: System Diagram (Animated Buoy-Rope-Anchor)

```
📋 STEP OBJECTIVE:
Draw the real-time animated system diagram:
    - Ground/riverbed at bottom
    - Anchor point on ground
    - OLP rope (dashed yellow line) at angle θ from vertical
    - Buoy at top of rope (moves with θ)
    - MPU6050 chip drawn on buoy surface
    - Water surface at buoy Y level with animated sine waves
    - Water body gradient fill
    - Vertical normal (dashed gray) from buoy straight down
    - θ arc (red) between normal and rope at buoy position
    - H measurement arrow (cyan) from anchor to buoy level
    - D horizontal drift line

All elements update in real-time from parsed serial data.

🔬 OBSERVABLE OUTPUT:
    Processing window shows animated buoy-anchor diagram
    Tilting the MPU6050 sensor causes buoy position to change
    Water level number updates, waves animate
```

---

### PHASE 10 — STEP 3: Data Panel, Graph, OLP Input & Recalibrate Button

```
📋 STEP OBJECTIVE:
Add to Processing sketch:

Data Panel (right side):
    - θ angle with large text
    - ★ Water Height H with large text
    - OLP Length, Pitch, Roll, Drift, Rate
    - Zone and Response level with color coding
    - Battery voltage and percentage
    - Pressure and temperature
    - Formula display: H = OLP × cos(θ)

Scrolling Graph (bottom):
    - 400-point circular history buffer
    - Real-time line chart of H over time
    - Filled area under curve
    - Warning level line at warning_level
    - Danger level line at danger_level
    - Y-axis: 0 to OLP length
    - Grid lines with labels

OLP Input Box:
    - Click to activate, type value, Enter to confirm
    - Sends value to serial (for testing — would be SMS in field)

Recalibrate Button:
    - Sends "CALIBRATE" marker (for development testing)

Status display area with color-coded messages.

🔬 OBSERVABLE OUTPUT:
    Complete Processing dashboard with all panels
    Real-time data updates from sensor
    Graph scrolls with water height history
    Interactive OLP input and recalibrate button work
    Zone/response level shows correct color (green/yellow/orange/red)
```

---

# SUMMARY: COMPLETE PHASE/STEP MAP

```
╔═══════════════════════════════════════════════════════════════════════╗
║ PHASE │ STEPS │ MAJOR MILESTONE                                      ║
╠═══════╪═══════╪══════════════════════════════════════════════════════╣
║   1   │   7   │ All sensors reading, deep sleep works, MOSFET works ║
║   2   │   3   │ Calibration + NVS persistence across reboots        ║
║   3   │   4   │ Full flood detection algorithm (24-matrix)          ║
║   4   │   2   │ Underwater + detachment + tamper detection          ║
║   5   │   3   │ Adaptive sampling + power management               ║
║   6   │   2   │ LittleFS 32-byte record storage                    ║
║   7   │   6   │ STM32 firmware complete (SIM800L+GPS+SMS)           ║
║   8   │   4   │ XIAO↔STM32 full transmission cycle working         ║
║   9   │   3   │ Complete autonomous system + fault recovery         ║
║  10   │   3   │ Processing visualization dashboard                  ║
╠═══════╪═══════╪══════════════════════════════════════════════════════╣
║ TOTAL │  37   │ Complete deployable water level monitoring system   ║
╚═══════╧═══════╧══════════════════════════════════════════════════════╝

IDEs Used:
    Arduino IDE        → XIAO ESP32-C3 (Phases 1-6, 8-9)
    STM32CubeIDE       → STM32 Blue Pill (Phase 7)
    Processing IDE     → PC Dashboard (Phase 10)
```

---

# DEPENDENCY FLOW

```
Phase 1 (Sensors)
    ↓
Phase 2 (Calibration + NVS)
    ↓
Phase 3 (Flood Algorithm)
    ↓
Phase 4 (Underwater + Detach)
    ↓
Phase 5 (Adaptive Sampling)
    ↓
Phase 6 (Data Storage)                Phase 7 (STM32 Firmware)
    ↓                                      ↓
    └──────────── Phase 8 (Integration) ───┘
                       ↓
                 Phase 9 (Complete System)
                       ↓
                 Phase 10 (Visualization)
```























































WORKING EXPLANATION :P
```
You are helping with a WATER LEVEL MEASURING SYSTEM that uses an MCU (microcontroller), an MPU6050 IMU sensor, and Processing IDE for visualization. The specific MCU platform is NOT fixed and may change. Below is a complete detailed explanation of how the system operates mechanically, electronically, mathematically, and in software. Focus on the LOGIC and GEOMETRY, not on any specific MCU platform.

═══════════════════════════════════════════════════════════════
SECTION 1: PHYSICAL SETUP AND MECHANICAL DESIGN
═══════════════════════════════════════════════════════════════

The system measures water level (height) in a tank, river, well, or any water body using a simple mechanical arrangement.

COMPONENTS:

1. ANCHOR: A fixed mounting point bolted/welded to the GROUND or RIVER BED at the bottom of the water body. This point NEVER moves. It is the lowest reference point of the system.

2. OLP ROPE: A rope, chain, or cable of FIXED KNOWN LENGTH (called "OLP") connecting the anchor at the bottom to a floating buoy at the top. This length is CONSTANT and is provided as an input parameter by the user (e.g., 100 cm, 200 cm, etc.). The OLP length does not change during operation.

3. FLOATING BUOY: A buoyant object (foam, plastic, sealed container) that floats on the water surface. It rises and falls with the water level. The buoy is connected to the anchor via the OLP rope from its underside.

4. MPU6050 SENSOR: An Inertial Measurement Unit with a 3-axis accelerometer and 3-axis gyroscope. It is mounted FLAT (TANGENTIALLY) on TOP of the buoy surface. This mounting orientation is critical to the entire system and affects all angle calculations.

PHYSICAL ARRANGEMENT (SIDE VIEW):

    When water is HIGH (buoy directly above anchor):

              Z↑ (MPU6050 Z-axis, points UP perpendicular to buoy)
              |
        ══════╬══════  MPU6050 (flat on buoy surface)
        ║   BUOY    ║  floating on water surface
        ╚════════════╝
    ~~~~~~~~~~~|~~~~~~~~~~  WATER SURFACE (high level)
               |
               |  OLP rope (vertical, straight down)
               |  Length = L (constant, user input)
               |
               |
               ● ANCHOR (fixed to ground)
    ═══════════════════════  GROUND / RIVER BED

        In this state:
        - Rope hangs perfectly vertical
        - Buoy is level (horizontal surface)
        - MPU6050 Z-axis aligns with true vertical
        - θ (theta) = 0°
        - Water height H = L × cos(0°) = L (maximum possible)


    When water is LOWER (buoy closer to anchor, drifts sideways):

                Z (tilted with buoy surface)
               ↗
             ╱
        ════╱═════  MPU6050 (tilted because buoy tilted)
        ║  BUOY  ║  buoy tilts because rope pulls at angle
        ╚════╲════╝
    ~~~~~~~~~~╲~~~~~~~~  WATER SURFACE (lower level)
               ╲
                ╲  OLP rope (now at angle from vertical)
                 ╲  Length = L (same, constant)
                  ╲
                   ╲  
                    ● ANCHOR (fixed to ground)
    ════════════════════════  GROUND / RIVER BED

        In this state:
        - Rope tilts at angle θ from vertical
        - Buoy surface tilts by same angle θ
        - MPU6050 Z-axis tilts θ from true vertical
        - Water height H = L × cos(θ) (less than L)


    When water is VERY LOW (buoy close to anchor, large tilt):

           Z (very tilted)
          ↗
        ╱
    ═══╱════  MPU6050
    ║ BUOY ║
    ╚══╲════╝
    ~~~~╲~~~~~  WATER SURFACE (very low)
         ╲
          ╲  OLP rope (steep angle)
           ╲
            ╲
             ● ANCHOR
    ═══════════  GROUND

        - θ is large
        - H = L × cos(θ) is small
        - Water level is low


WHY THE BUOY TILTS:
The buoy floats on the water surface. The rope connects the bottom of the buoy to the anchor on the ground. When the water level drops, the vertical distance between the buoy and anchor decreases, but the rope length stays the same. The excess rope length forces the rope to go at an angle rather than straight down. This angled tension pulls the buoy sideways and tilts it. The tilt angle of the buoy surface equals the tilt angle of the rope from vertical. Since the MPU6050 is mounted flat on the buoy, it directly measures this tilt through gravity sensing.

═══════════════════════════════════════════════════════════════
SECTION 2: GEOMETRY AND MATHEMATICS
═══════════════════════════════════════════════════════════════

The core geometry forms a right triangle:

    BUOY position (at water surface)
        ◯ ← MPU6050 is here, θ is measured HERE
       /|
      / |
     /  |  ← IMAGINARY VERTICAL NORMAL
    /   |     This is an imaginary reference line going
   /    |     STRAIGHT DOWN from the buoy toward the ground.
  / θ   |     It represents the direction of gravity.
 /      |     The MPU6050 senses gravity, so it "knows" this line.
/       |  
        |  ← H = vertical height = OLP × cos(θ)
        |     This is the WATER HEIGHT we want to measure.
        |     It is the vertical distance from anchor up to buoy.
●───────┘  
ANCHOR      ← D = horizontal distance = OLP × sin(θ)
               This is the sideways drift of the buoy.

KEY GEOMETRIC POINTS:

- The angle θ (theta) is measured AT THE BUOY position
- θ is the angle BETWEEN:
    1. The imaginary vertical normal (pointing straight DOWN from buoy, i.e., gravity direction)
    2. The OLP rope (going from buoy down to anchor)
- The vertical normal is NOT a physical object. It is an imaginary reference axis representing the direction of gravity (straight down). The MPU6050 accelerometer senses gravity, which defines this axis.
- θ = 0° means the rope is perfectly vertical (buoy directly above anchor, maximum water height)
- θ = 90° means the rope is horizontal (theoretical limit, buoy at same level as anchor)

FORMULAS:

    H = OLP × cos(θ)       ← WATER HEIGHT (primary output)
    D = OLP × sin(θ)       ← HORIZONTAL DRIFT (secondary output)

    Where:
    - OLP = fixed rope length (user input, constant, in cm)
    - θ = tilt angle from vertical (measured by MPU6050, in degrees)
    - H = vertical distance from anchor to buoy = water level height above anchor
    - D = horizontal displacement of buoy from anchor position

EXAMPLES (with OLP = 100 cm):

    θ = 0°   → H = 100 × cos(0°)  = 100.00 cm  (max water height)
    θ = 5°   → H = 100 × cos(5°)  = 99.62 cm
    θ = 10°  → H = 100 × cos(10°) = 98.48 cm
    θ = 15°  → H = 100 × cos(15°) = 96.59 cm
    θ = 20°  → H = 100 × cos(20°) = 93.97 cm
    θ = 30°  → H = 100 × cos(30°) = 86.60 cm
    θ = 45°  → H = 100 × cos(45°) = 70.71 cm
    θ = 60°  → H = 100 × cos(60°) = 50.00 cm
    θ = 75°  → H = 100 × cos(75°) = 25.88 cm
    θ = 90°  → H = 100 × cos(90°) = 0.00 cm   (theoretical minimum)

═══════════════════════════════════════════════════════════════
SECTION 3: MPU6050 TANGENTIAL MOUNTING AND ANGLE CALCULATION
═══════════════════════════════════════════════════════════════

The MPU6050 is mounted FLAT (tangentially) on the buoy surface. This means the sensor board lies parallel to the buoy's top surface. This mounting orientation is CRITICAL because it determines which mathematical formulas are used to extract the tilt angle from raw sensor data.

MPU6050 AXIS ORIENTATION ON BUOY:

    TOP VIEW OF BUOY (looking down from above):
    ┌─────────────────────┐
    │                     │
    │    ┌───────────┐    │
    │    │  MPU6050  │    │
    │    │           │    │
    │    │  X →      │    │
    │    │  Y ↑      │    │
    │    │  Z ⊙(out) │    │ Z comes out of the screen = UP
    │    └───────────┘    │
    │                     │
    └─────────────────────┘
           BUOY

    SIDE VIEW:
    Z↑ (perpendicular to buoy surface, points to SKY)
    |
    |  Y (lies along buoy surface)
    | /
    |/_____ X (lies along buoy surface)
    ═══════════════ buoy flat surface
    ║     BUOY    ║
    ╚═════════════╝
         |
         | rope goes down
         ● anchor

    KEY: Z-axis = UP (away from water, toward sky)
         X-axis = FLAT (along buoy surface, forward)
         Y-axis = FLAT (along buoy surface, sideways)

ACCELEROMETER BEHAVIOR FOR TANGENTIAL MOUNTING:

    When buoy is PERFECTLY LEVEL (θ = 0°):
        accX ≈ 0g      (no gravity component along buoy surface X)
        accY ≈ 0g      (no gravity component along buoy surface Y)
        accZ ≈ +1g     (full gravity along Z = pointing straight up)
        
        Gravity is entirely along Z because Z = vertical when level.

    When buoy TILTS around Y-axis (pitch / forward tilt):
        accX increases  (gravity "leaks" into X as surface tilts)
        accZ decreases  (Z no longer fully aligned with gravity)
        accY ≈ 0g      (no tilt in this direction)

    When buoy TILTS around X-axis (roll / sideways tilt):
        accY increases  (gravity "leaks" into Y)
        accZ decreases  (Z moves away from gravity direction)
        accX ≈ 0g

    When buoy TILTS in BOTH directions:
        accX and accY both have values
        accZ decreases
        Total acceleration magnitude = sqrt(X² + Y² + Z²) ≈ 1g always

TILT ANGLE FORMULAS FOR TANGENTIAL (FLAT) MOUNTING:

    tiltX = atan2(accY, accZ)   → tilt around X-axis (roll)
    tiltY = atan2(accX, accZ)   → tilt around Y-axis (pitch)

    ★ CRITICAL: These formulas use accZ as the denominator 
      because Z is the UP axis of the flat-mounted sensor.

    ★ WARNING: These formulas are DIFFERENT from a sensor mounted 
      with Z along the rope or along gravity direction. 
      A non-tangential mounting would use:
          atan2(accX, sqrt(accY² + accZ²))  ← WRONG for this system
      
      Our system uses tangential mounting, so we use:
          atan2(accY, accZ) and atan2(accX, accZ)  ← CORRECT

    Total tilt angle θ from vertical:
        θ = sqrt(tiltX² + tiltY²)
        (This combines tilts in both axes into one angle from vertical)
        (Valid for small to moderate angles, sufficient for this application)

RAW DATA TO PHYSICAL UNITS CONVERSION:
    - Accelerometer raw value at ±2g range:
      acceleration_in_g = raw_value / 16384.0
    - Gyroscope raw value at ±250°/s range:
      angular_rate_in_deg_per_sec = raw_value / 131.0

COMPLEMENTARY FILTER:
    Raw accelerometer angles are noisy (vibration, waves).
    Raw gyroscope integration drifts over time.
    The complementary filter combines both for stable output:

    filteredAngle = α × (filteredAngle + gyroRate × dt) + (1 - α) × accelAngle

    Where:
    - α = 0.98 (filter coefficient)
    - α close to 1 = smoother, slower response (trusts gyro more)
    - α close to 0 = noisier, faster response (trusts accel more)
    - dt = time between readings in seconds
    - gyroRate = angular velocity from gyroscope (°/s)
    - accelAngle = absolute angle from accelerometer (°)
    
    0.98 is chosen because water level changes slowly, so smooth 
    response is preferred over fast response.

═══════════════════════════════════════════════════════════════
SECTION 4: CALIBRATION (ZERO REFERENCE SYSTEM)
═══════════════════════════════════════════════════════════════

PROBLEM:
After powering on, the MPU6050 may show a non-zero angle even 
when the buoy is perfectly level and the rope hangs straight down.
This happens because of:
    - Manufacturing bias in the accelerometer
    - Imperfect physical mounting (sensor not perfectly aligned on buoy)
    - Temperature-dependent offset
    - PCB-level misalignment

Without calibration, the system might show θ = 2.5° when it 
should show θ = 0°, causing a constant error in water height.

SOLUTION:
At startup (and on-demand), the system captures the current sensor 
readings as the ZERO REFERENCE. All future measurements are then 
relative to this reference. If calibrated when the buoy is level 
and rope is vertical, all subsequent tilt measurements represent 
the actual deviation from that level position.

CALIBRATION PROCEDURE:

    STEP 1 - GYROSCOPE OFFSET CALIBRATION:
        Purpose: Find the gyro's "zero rate" bias
        Method:
        - Keep sensor completely still
        - Read gyroscope 1000 times (over ~2 seconds)
        - Average all X and Y readings
        - Store as gyroOffsetX, gyroOffsetY
        - Subtract these from ALL future gyro readings
        Result: Gyro reports 0°/s when sensor is stationary

    STEP 2 - ACCELEROMETER ZERO ANGLE REFERENCE:
        Purpose: Define the current orientation as θ = 0°
        Method:
        - Keep sensor still and in the desired "zero" position
        - Read accelerometer 500 times (over ~1.5 seconds)
        - Reject samples where total acceleration ≠ 1g (sensor moved)
        - Average valid samples to get reference gravity vector:
            refAccX = average of accX readings
            refAccY = average of accY readings
            refAccZ = average of accZ readings
        - Calculate reference tilt angles:
            refTiltX = atan2(refAccY, refAccZ) in degrees
            refTiltY = atan2(refAccX, refAccZ) in degrees
        - Store refTiltX and refTiltY as the ZERO reference
        Result: These reference values are subtracted from all 
                future measurements, making the calibration 
                position read exactly 0°

    APPLICATION IN MAIN LOOP:
        correctedTiltX = measuredTiltX - refTiltX
        correctedTiltY = measuredTiltY - refTiltY
        θ = sqrt(correctedTiltX² + correctedTiltY²)

        At calibration moment: measured = ref, so corrected = 0  ✓
        After tilting by 30°: measured = ref + 30, so corrected = 30°  ✓

    RECALIBRATION (ON-DEMAND):
        - Can be triggered anytime via serial command "CAL\n"
        - Performs a quick version of both calibration steps
        - Re-zeros the angle to the current position
        - Useful when:
            • Sensor has drifted over time
            • Buoy was repositioned
            • System was disturbed and needs re-zeroing
            • Environmental conditions changed

    VALIDITY CHECK DURING CALIBRATION:
        - Total acceleration magnitude is checked: sqrt(ax² + ay² + az²)
        - Only samples where magnitude is between 0.9g and 1.1g are accepted
        - This ensures the sensor was stationary (not accelerating/vibrating)
        - If fewer than minimum valid samples, fallback to single reading

═══════════════════════════════════════════════════════════════
SECTION 5: ELECTRONIC HARDWARE (PLATFORM AGNOSTIC)
═══════════════════════════════════════════════════════════════

The system requires ANY microcontroller that supports:
    - I2C communication (for MPU6050)
    - UART/Serial communication (for sending data to PC)
    - Basic math operations (float arithmetic, trigonometry)
    - Millisecond-level timing

The specific MCU platform is NOT fixed. It could be:
    - Arduino (ATmega328P)
    - STM32 (Blue Pill, Nucleo, etc.)
    - ESP32 / ESP8266
    - Raspberry Pi Pico
    - Any other MCU with I2C and UART

REQUIRED CONNECTIONS (platform agnostic):
    
    MPU6050 Pin    →    MCU Pin
    ─────────────────────────────
    VCC            →    3.3V power output
    GND            →    Ground
    SCL            →    I2C Clock pin (platform dependent)
    SDA            →    I2C Data pin (platform dependent)

    MCU UART TX    →    Computer (via USB-Serial or direct USB)

COMMUNICATION PROTOCOLS:
    - MPU6050 ↔ MCU:  I2C at standard speed (100kHz) or fast (400kHz)
    - MCU ↔ Computer: UART Serial at 9600 baud
    - MPU6050 I2C slave address: 0x68 (AD0 pin low) or 0x69 (AD0 high)

MPU6050 REGISTER MAP (platform independent):
    Register  Hex   Purpose                Setting
    ────────  ────  ─────────────────────  ──────────
    PWR_MGMT1 0x6B  Power management       0x00 (wake up)
    SMPLRT    0x19  Sample rate divider    0x07 (125 Hz)
    CONFIG    0x1A  DLPF configuration     0x03 (~44 Hz BW)
    GYRO_CFG  0x1B  Gyroscope range        0x00 (±250°/s)
    ACCEL_CFG 0x1C  Accelerometer range    0x00 (±2g)
    WHO_AM_I  0x75  Device ID (read only)  Returns 0x68
    ACCEL_OUT 0x3B  Data start register    Read 14 bytes from here

DATA BYTE ORDER (read 14 bytes from register 0x3B):
    Byte 0:  AccX High byte  ┐
    Byte 1:  AccX Low byte   ┘ → int16: (byte0 << 8) | byte1
    Byte 2:  AccY High byte  ┐
    Byte 3:  AccY Low byte   ┘ → int16: (byte2 << 8) | byte3
    Byte 4:  AccZ High byte  ┐
    Byte 5:  AccZ Low byte   ┘ → int16: (byte4 << 8) | byte5
    Byte 6:  Temp High byte  ┐
    Byte 7:  Temp Low byte   ┘ → SKIP (not used)
    Byte 8:  GyroX High byte ┐
    Byte 9:  GyroX Low byte  ┘ → int16: (byte8 << 8) | byte9
    Byte 10: GyroY High byte ┐
    Byte 11: GyroY Low byte  ┘ → int16: (byte10 << 8) | byte11
    Byte 12: GyroZ High byte ┐
    Byte 13: GyroZ Low byte  ┘ → int16: (byte12 << 8) | byte13

    All values are signed 16-bit integers (int16), big-endian.

═══════════════════════════════════════════════════════════════
SECTION 6: MCU FIRMWARE LOGIC (PLATFORM AGNOSTIC PSEUDOCODE)
═══════════════════════════════════════════════════════════════

The MCU firmware follows this logic regardless of which 
microcontroller platform is used:

INITIALIZATION:
    1. Initialize serial/UART at 9600 baud
    2. Initialize I2C bus
    3. Write MPU6050 configuration registers:
       - PWR_MGMT1 = 0x00 (wake up)
       - Wait 100ms
       - SMPLRT = 0x07 (sample rate 125Hz)
       - CONFIG = 0x03 (DLPF ~44Hz)
       - GYRO_CFG = 0x00 (±250°/s)
       - ACCEL_CFG = 0x00 (±2g)
    4. Read WHO_AM_I register, verify = 0x68
       If not 0x68: send error, halt
    5. Perform GYRO calibration:
       - Read gyro 1000 times, average X and Y
       - Store as gyroOffsetX, gyroOffsetY
    6. Perform ZERO ANGLE calibration:
       - Read accelerometer 500 times
       - Filter for valid samples (total accel ≈ 1g)
       - Average to get refAccX, refAccY, refAccZ
       - Calculate refTiltX = atan2(refAccY, refAccZ) × 180/π
       - Calculate refTiltY = atan2(refAccX, refAccZ) × 180/π
       - Initialize filtTiltX = refTiltX
       - Initialize filtTiltY = refTiltY
    7. Record start time
    8. Send "STATUS:READY" over serial

MAIN LOOP (repeats every ~50ms = 20Hz):
    1. CHECK SERIAL INPUT:
       - If received "OLP:xxx.x\n" → update olpLength variable
       - If received "CAL\n" → run recalibration function

    2. READ MPU6050:
       - Read 14 bytes starting from register 0x3B
       - Parse into accX, accY, accZ, gyroX, gyroY, gyroZ (int16)

    3. CALCULATE TIME DELTA:
       - dt = (currentTime - previousTime) / 1000.0 (in seconds)
       - previousTime = currentTime
       - If dt is invalid (≤0 or >0.5): set dt = 0.008 as safety

    4. CONVERT RAW TO PHYSICAL:
       - ax = accX / 16384.0  (in g)
       - ay = accY / 16384.0  (in g)
       - az = accZ / 16384.0  (in g)
       - gx = (gyroX - gyroOffsetX) / 131.0  (in °/s)
       - gy = (gyroY - gyroOffsetY) / 131.0  (in °/s)

    5. CALCULATE ACCELEROMETER ANGLES (tangential mounting):
       - accTiltX = atan2(ay, az) × 180/π  (degrees)
       - accTiltY = atan2(ax, az) × 180/π  (degrees)

    6. APPLY COMPLEMENTARY FILTER:
       - filtTiltX = 0.98 × (filtTiltX + gx × dt) + 0.02 × accTiltX
       - filtTiltY = 0.98 × (filtTiltY + gy × dt) + 0.02 × accTiltY

    7. SUBTRACT ZERO REFERENCE:
       - correctedTiltX = filtTiltX - refTiltX
       - correctedTiltY = filtTiltY - refTiltY

    8. CALCULATE TOTAL TILT ANGLE θ:
       - theta = sqrt(correctedTiltX² + correctedTiltY²)
       - Clamp: if theta > 90 then theta = 90
       - Clamp: if theta < 0 then theta = 0

    9. CALCULATE WATER HEIGHT AND HORIZONTAL DISTANCE:
       - thetaRad = theta × π / 180
       - waterHeight = olpLength × cos(thetaRad)
       - horizontalDist = olpLength × sin(thetaRad)

    10. SEND DATA OVER SERIAL:
        Format: "theta,waterHeight,correctedTiltX,correctedTiltY,olpLength,horizontalDist\n"
        Example: "15.23,96.47,12.50,8.80,100.00,26.27\n"

    11. WAIT 50ms (to maintain ~20Hz update rate)

RECALIBRATION FUNCTION (triggered by "CAL\n" command):
    1. Send "STATUS:RECALIBRATING"
    2. Read gyro 300 times, re-average offsets
    3. Read accelerometer 300 times, re-average reference
    4. Recalculate refTiltX and refTiltY
    5. Reset filtTiltX = refTiltX, filtTiltY = refTiltY
    6. Send "STATUS:RECALIBRATED_ZERO"

═══════════════════════════════════════════════════════════════
SECTION 7: SERIAL COMMUNICATION PROTOCOL
═══════════════════════════════════════════════════════════════

MCU → PC (DATA OUTPUT):
    Format: CSV text, newline terminated
    "theta,waterHeight,correctedTiltX,correctedTiltY,olpLength,horizontalDist\n"
    
    Field 0: theta           - Total tilt angle from vertical (degrees, 0.00-90.00)
    Field 1: waterHeight     - H = OLP × cos(θ) (cm, 2 decimal places)
    Field 2: correctedTiltX  - Zero-corrected tilt around X (degrees, signed)
    Field 3: correctedTiltY  - Zero-corrected tilt around Y (degrees, signed)
    Field 4: olpLength       - Current OLP rope length setting (cm)
    Field 5: horizontalDist  - D = OLP × sin(θ) (cm, 2 decimal places)
    
    Rate: ~20 lines per second (every 50ms)
    Baud: 9600

MCU → PC (STATUS MESSAGES):
    These are NOT CSV data. They are prefixed with "STATUS:" or "ERROR:"
    
    "STATUS:MPU6050_OK\n"              - Sensor detected successfully
    "STATUS:CALIBRATING\n"             - Calibration started
    "STATUS:CALIBRATING_GYRO\n"        - Gyro calibration in progress
    "STATUS:CALIBRATING_REFERENCE\n"   - Zero angle calibration in progress
    "STATUS:CALIBRATION_DONE\n"        - Calibration completed
    "STATUS:ANGLE_IS_NOW_ZERO\n"       - Zero reference set
    "STATUS:READY\n"                   - System operational
    "STATUS:RECALIBRATING\n"           - Recalibration in progress
    "STATUS:RECALIBRATED_ZERO\n"       - Recalibration completed
    "STATUS:REF_GRAVITY=x,y,z\n"       - Reference gravity vector values
    "STATUS:REF_TILTX=xx.xx\n"         - Reference X tilt value
    "STATUS:REF_TILTY=xx.xx\n"         - Reference Y tilt value
    "STATUS:NEW_REF_TILTX=xx.xx\n"     - New reference after recalibration
    "STATUS:NEW_REF_TILTY=xx.xx\n"     - New reference after recalibration
    "ERROR:MPU6050_NOT_FOUND\n"        - Sensor not detected on I2C bus

PC → MCU (COMMANDS):
    "OLP:xxx.xx\n"   - Set OLP rope length to xxx.xx cm
                       Example: "OLP:150.0\n" sets length to 150 cm
                       Valid range: 0 < value < 10000
    
    "CAL\n"          - Trigger recalibration
                       MCU will re-zero the angle using current position
                       Sensor must be held still during this process

═══════════════════════════════════════════════════════════════
SECTION 8: PROCESSING VISUALIZATION SOFTWARE
═══════════════════════════════════════════════════════════════

The Processing sketch runs on the computer and provides 
real-time visualization of the water level system.

SERIAL CONNECTION:
    - Connects to MCU serial port at 9600 baud
    - Port index must be set manually in setup()
    - Uses bufferUntil('\n') for line-by-line reading
    - Parses CSV data and status messages separately

DISPLAY LAYOUT (1300×850 pixel window):

    ┌──────────────────────────────┬─────────────────────────────┐
    │                              │                             │
    │   SYSTEM DIAGRAM             │   DATA PANEL                │
    │   (animated, real-time)      │   - θ angle value           │
    │                              │   - ★ Water Height H        │
    │   Shows:                     │   - OLP Length               │
    │   - Ground/river bed         │   - Pitch (corrected)       │
    │   - Anchor on ground         │   - Roll (corrected)        │
    │   - OLP rope (dashed yellow) │   - Horizontal drift        │
    │   - Floating buoy + MPU6050  │   - Formula display         │
    │   - Animated water waves     │                             │
    │   - Water body (gradient)    │   INPUT PANEL               │
    │   - Vertical normal from buoy│   - OLP length text input   │
    │   - θ arc at buoy position   │   - Click → type → Enter    │
    │   - H measurement arrow      │                             │
    │   - Horizontal drift line    │   RECALIBRATE BUTTON        │
    │                              │   - Sends "CAL\n" to MCU    │
    │                              │   - Status message display   │
    │                              │                             │
    ├──────────────────────────────┴─────────────────────────────┤
    │                                                            │
    │   WATER HEIGHT GRAPH (full width, scrolling)               │
    │   - Real-time line chart of H over time                    │
    │   - 400-point history buffer (circular)                    │
    │   - Filled area under curve (semi-transparent)             │
    │   - Warning level line at 75% of OLP                       │
    │   - Danger level line at 90% of OLP                        │
    │   - Y-axis: 0 to OLP length                                │
    │   - Grid lines with labels                                 │
    │                                                            │
    └────────────────────────────────────────────────────────────┘

SYSTEM DIAGRAM DYNAMIC BEHAVIOR:
    The diagram updates every frame based on received data:
    
    - BUOY POSITION: Calculated from θ
      buoyX = anchorX + ropeVisualLength × sin(θ)
      buoyY = anchorY - ropeVisualLength × cos(θ)
      (buoy moves right and down as θ increases)
    
    - WATER SURFACE: Drawn at buoy Y level with animated sine waves
      Two wave layers with different frequencies create realistic water
      waveY = baseY + sin(x × freq1 + phase) × amp1 + sin(x × freq2 + phase × 1.7) × amp2
    
    - WATER BODY: Gradient fill from water surface down to ground
    
    - OLP ROPE: Dashed yellow line from buoy to anchor
    
    - VERTICAL NORMAL: Dashed gray line from buoy straight down
      (This is the imaginary reference axis)
    
    - ANGLE ARC: Red arc at buoy position between normal and rope
      Shows θ value with label
    
    - HEIGHT ARROW: Cyan vertical arrow from buoy to anchor level
      Shows H value with formula

INTERACTIVE FEATURES:

    1. OLP INPUT BOX:
       - Click on the input field to activate
       - Type numerical value (digits and decimal point)
       - Press Enter to confirm
       - Sends "OLP:value\n" to MCU
       - Press Escape to cancel
       - Backspace to delete characters
    
    2. RECALIBRATE BUTTON:
       - Click button labeled "🔄 RECALIBRATE"
       - Sends "CAL\n" to MCU
       - Status message updates through calibration stages
       - Keep sensor still while recalibrating
    
    3. STATUS DISPLAY:
       - Shows real-time status messages from MCU
       - Updates during calibration, errors, and operation
       - Color-coded: ✅ green for OK, ⏳ yellow for working, ❌ red for error

SERIAL EVENT HANDLING:
    The serialEvent() function processes incoming data:
    
    1. If line starts with "STATUS:" → update statusMsg display
    2. If line starts with "ERROR:" → show error in status
    3. Otherwise → parse as CSV data:
       - Split by comma
       - Extract 6 float values
       - Update global variables
       - Add waterHeight to history buffer
       - History buffer is circular (overwrites oldest)

═══════════════════════════════════════════════════════════════
SECTION 9: COMPLETE DATA FLOW
═══════════════════════════════════════════════════════════════

    ┌──────────┐   I2C Bus   ┌──────────┐   UART/USB   ┌────────────┐
    │ MPU6050  │ ──────────→ │   MCU    │ ───────────→ │ PROCESSING │
    │ (sensor) │  raw bytes  │(firmware)│  CSV text    │  (display) │
    │          │             │          │              │            │
    │ accX,Y,Z │             │ Performs:│              │ Shows:     │
    │ gyrX,Y,Z │             │ -convert │              │ -diagram   │
    │          │             │ -filter  │              │ -data      │
    └──────────┘             │ -calibrate             │ -graph     │
                             │ -calc θ  │              │ -controls  │
                             │ -calc H  │              │            │
                             └──────────┘              └────────────┘
                                  ↑                          │
                                  │   Commands:              │
                                  │   "OLP:100.0\n"          │
                                  │   "CAL\n"                │
                                  └──────────────────────────┘

    TIMING:
    - MPU6050 samples at 125 Hz (internal)
    - MCU reads and processes at ~200 Hz (5ms loop)
    - MCU sends data at 20 Hz (50ms interval)
    - Processing displays at 60 fps (screen refresh)
    - Graph stores 400 data points (20 seconds of history)

═══════════════════════════════════════════════════════════════
SECTION 10: IMPORTANT CONSTRAINTS AND EDGE CASES
═══════════════════════════════════════════════════════════════

1. TANGENTIAL MOUNTING IS NON-NEGOTIABLE:
   The entire angle calculation relies on Z being perpendicular 
   to the buoy surface (pointing up). The formulas 
   atan2(accY, accZ) and atan2(accX, accZ) are ONLY correct 
   for this orientation. Changing the mounting changes everything.

2. CALIBRATION POSITION DEFINES ZERO:
   Whatever orientation the sensor is in during calibration 
   becomes θ = 0°. The buoy MUST be level and the rope MUST 
   hang vertically during calibration for accurate readings.

3. θ IS ALWAYS POSITIVE (0° to 90°):
   The system calculates θ = sqrt(tiltX² + tiltY²) which is 
   always positive. Direction of tilt is encoded in the 
   individual correctedTiltX and correctedTiltY values.

4. OLP LENGTH MUST BE PHYSICALLY ACCURATE:
   H = OLP × cos(θ) is only as accurate as the OLP measurement.
   A 1% error in OLP = 1% error in height. Measure carefully.

5. COMPLEMENTARY FILTER α = 0.98:
   Tuned for slow water level changes. For faster response 
   (turbulent water), decrease α. For calmer water, α can 
   stay at 0.98 or increase to 0.99.

6. WATER CURRENT AFFECTS READINGS:
   Horizontal water flow pushes the buoy sideways, adding 
   tilt that is not due to water level change. The 
   horizontalDist output helps identify this. In practice, 
   shielding the buoy from current improves accuracy.

7. WAVE ACTION CAUSES OSCILLATION:
   Surface waves cause the buoy to bob and tilt rapidly. 
   The complementary filter smooths this, but large waves 
   may still cause reading fluctuation. The Processing graph 
   helps visualize and average out wave effects.

8. ROPE MUST BE TAUT:
   The geometry assumes the rope is straight (taut). If the 
   rope has slack (water too deep, buoy directly above anchor), 
   the tilt reading becomes unreliable. The system works best 
   when the rope has some tension pulling the buoy down/sideways.

9. MAXIMUM MEASURABLE HEIGHT = OLP LENGTH:
   When θ = 0°, H = OLP. The system cannot measure water 
   heights greater than the rope length. Choose OLP length 
   to exceed the maximum expected water level.

10. SERIAL BAUD RATE = 9600:
    Sufficient for 20Hz updates of ~50-byte CSV lines.
    Higher baud rates can be used if the platform supports 
    them, but 9600 is universal and reliable.

11. GYRO DRIFT:
    Over very long periods (hours), the gyro component of 
    the complementary filter may cause slight drift. The 
    accelerometer component (2% weight) corrects this 
    continuously, but periodic recalibration is recommended 
    for long-duration monitoring.

12. TEMPERATURE SENSITIVITY:
    MPU6050 readings shift slightly with temperature changes.
    For outdoor deployment, recalibrate periodically or after 
    significant temperature changes.
