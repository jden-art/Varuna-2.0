

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

I will continue with Section 17 onward when you say **end 09**.
