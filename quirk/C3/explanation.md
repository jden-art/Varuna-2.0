

# Complete Detailed Breakdown — ESP32-C3 Communication Bridge

---

## What The ESP32-C3 Is Doing — In One Sentence

The C3 is a **dumb communication pipe** — it takes the 39-field CSV from the S3, pushes it to Firebase over WiFi, buffers data on an SD card when WiFi is down, forwards configuration commands from the website back to the S3, and can remotely re-flash the S3's firmware over-the-air.

---

## The C3 Has ZERO Intelligence About Floods

```
The C3 does NOT:
    ✗ Read any sensors
    ✗ Calculate water height
    ✗ Classify flood modes
    ✗ Decide alert levels
    ✗ Know what the 39 CSV fields mean
    ✗ Make any decisions about the data

The C3 ONLY:
    ✓ Receives CSV strings from S3
    ✓ Converts CSV to JSON
    ✓ Pushes JSON to Firebase
    ✓ Buffers to SD card when WiFi dies
    ✓ Flushes SD buffer when WiFi returns
    ✓ Polls Firebase for commands from the website
    ✓ Forwards those commands to the S3
    ✓ Can re-flash the S3's firmware remotely
```

---

## All 10 Jobs The C3 Does

```
JOB    WHAT                                            WHY
─────  ──────────────────────────────────────────────  ─────────────────────────────────────
 1     Receive 39-field CSV from S3 (GPIO 10)          S3 has no WiFi — C3 is its internet
 2     Push CSV to Firebase RTDB over WiFi             Website needs the data
 3     Buffer CSV to SD card when WiFi is down          Don't lose data during outages
 4     Flush SD buffer when WiFi reconnects            Recover all offline data
 5     Receive config commands from Firebase            Website changes settings
 6     Forward config commands to S3 (Serial1)         S3 needs the new settings
 7     Download OTA firmware from Firebase Storage     Remote firmware update
 8     Flash S3 via UART bootloader (BOOT/EN pins)    Program S3 without physical access
 9     Respond to S3 diagnostic pings ($PING→$PONG)   S3 health check
10     Real-time mode: bypass SD when server wants it  Low-latency monitoring
```

---

## All Communication Channels — What Connects Where

```
CHANNEL                  C3 PIN       BAUD      DIRECTION     CONTENT
───────────────────────  ───────────  ────────  ────────────  ────────────────────────────────────
Software Serial RX       GPIO 10      9600      S3 → C3       39-field CSV data
                                                               Diagnostic $DIAG frames
                                                               (interrupt-driven bit-bang receive)

Hardware Serial1 RX      GPIO 20      9600*     S3 → C3       $PONG, $CFG_ACK, $DIAG_ACK
                                                               $FLAG_ACK, diagnostic frames
                                                               Forwarded debugger commands

Hardware Serial1 TX      GPIO 21      9600*     C3 → S3       $CFG, $DIAGRUN, $PING
                                                               $SIMSTAT, $SETFLAG

USB Serial               USB          115200    C3 → PC       Log messages [timestamp][LEVEL] msg

WiFi (HTTPS)             —            —         C3 → Firebase  JSON data push (POST/PUT)
                                                               Status updates
                                                               Diagnostic uploads

WiFi (HTTPS)             —            —         Firebase → C3  Config commands (GET polling)
                                                               OTA commands
                                                               Real-time mode flag
                                                               Diagnostic requests

SPI                      GPIO 4-7     —         C3 ↔ SD Card  CSV buffer file read/write
                                                               OTA firmware temp storage

S3 BOOT control          GPIO 2       —         C3 → S3       Pull LOW to enter bootloader
S3 RESET control         GPIO 3       —         C3 → S3       Pulse LOW to reset S3

* During OTA flash, Serial1 switches to 115200 baud for bootloader protocol
```

---

## The State Machine — 6 States

```
STATE              WHAT'S HAPPENING                          TRANSITIONS TO
─────────────────  ────────────────────────────────────────  ──────────────────────
STATE_NORMAL       WiFi is up. CSV goes directly to          → STATE_BUFFERING
                   Firebase. Also saved to SD as backup.       (if WiFi drops)
                                                             → STATE_OTA_DOWNLOAD
                                                               (if OTA command received)

STATE_BUFFERING    WiFi is down. CSV goes to SD card only.   → STATE_NORMAL
                   Retries WiFi every 30 seconds.              (if WiFi reconnects)

STATE_FLUSHING     WiFi restored. Reading old CSV lines      → STATE_NORMAL
                   from SD and pushing them to Firebase        (when SD buffer empty)
                   in batches of 10 every 2 seconds.

STATE_OTA_DOWNLOAD Downloading firmware .bin from Firebase   → STATE_OTA_FLASH
                   Storage to SD card.                         (when download complete)
                                                             → STATE_NORMAL
                                                               (if download fails)

STATE_OTA_FLASH    S3 is in bootloader mode. C3 is sending   → STATE_OTA_VERIFY
                   firmware blocks via SLIP protocol.          (when flash complete)

STATE_OTA_VERIFY   S3 has been reset. C3 sends $PING and     → STATE_NORMAL
                   waits for $PONG to confirm S3 is alive.     (success or failure)
```

---

## The Main Loop — All 8 Tasks

```
TASK    WHAT                              FREQUENCY           CONDITION
──────  ────────────────────────────────  ──────────────────  ──────────────────────
0       WiFi monitoring & reconnection    Every loop()        Always
        Check if WiFi dropped             iteration           Retry every 30s if down
        Attempt reconnection

1       Receive CSV from S3               Continuous          Only if s3Active=true
        Read software serial ring buffer  Every loop()        (false during OTA)
        Assemble bytes into lines         iteration
        Process complete CSV lines

2       Process S3 command responses      Continuous          Only if s3Active=true
        Read Hardware Serial1             Every loop()
        Handle $PONG, $CFG_ACK, etc.      iteration

3       Poll Firebase for commands        Every 5 seconds     Only if WiFi connected
        GET config, OTA, realtime, diag   (1 second in        
        Update device status              real-time mode)     

4       Forward new config to S3          When cfgDirty=true  Only if s3Active=true
        Send $CFG command on Serial1      After Firebase      One-shot, then
                                          config received     cfgDirty=false

5       Forward diagnostic request to S3  When diagPending    Only if s3Active=true
        Send $DIAGRUN on Serial1          After Firebase      One-shot, then
                                          diag request        diagPending=false

6       Flush SD buffer to Firebase       Every 2 seconds     Only if:
        Read 10 lines at a time           Between batches     - WiFi connected
        Push each as JSON to Firebase                         - SD has buffered data
        Track flush position in index file                    - NOT in real-time mode
                                                              - NOT doing OTA

7       OTA firmware update               When otaPending     Only if WiFi connected
        Download → Flash → Verify         One-shot process    Blocks everything else

8       Yield + 1ms delay                 Every loop()        Prevent watchdog reset
```

---

## Firebase Database Structure — What Gets Written Where

```
Firebase Realtime Database Structure:

devices/
  buoy_001/
    ├── data/                          ← POST (auto-generated keys)
    │   ├── -Nabc123/                  ← One reading
    │   │   ├── theta: 12.45
    │   │   ├── waterHeight: 187.34
    │   │   ├── corrTiltX: 8.23
    │   │   ├── corrTiltY: -3.11
    │   │   ├── olpLength: 200.00
    │   │   ├── horizDist: 43.12
    │   │   ├── pressure: 1013.25
    │   │   ├── temperature: 28.50
    │   │   ├── baselinePressure: 1013.00
    │   │   ├── pressureDev: 0.25
    │   │   ├── submersionState: 1
    │   │   ├── estDepth: 0.00
    │   │   ├── bmpAvail: 1
    │   │   ├── unixTime: 1719532800
    │   │   ├── dateTime: "2025-01-15 14:30:00"
    │   │   ├── rtcValid: 1
    │   │   ├── rateOfRise: 2.340
    │   │   ├── alertLevel: 1
    │   │   ├── sessionDur: 3600
    │   │   ├── peakHeight: 192.50
    │   │   ├── minHeight: 45.20
    │   │   ├── latitude: 14.583210
    │   │   ├── longitude: 120.984530
    │   │   ├── altitude: 15.2
    │   │   ├── satellites: 8
    │   │   ├── gpsFix: 1
    │   │   ├── simRSSI: -67
    │   │   ├── simReg: 1
    │   │   ├── simAvail: 1
    │   │   ├── zone: 1
    │   │   ├── responseLevel: 1
    │   │   ├── sustainedRise: 0
    │   │   ├── battery: 85.3
    │   │   ├── sampleInterval: 60
    │   │   ├── txInterval: 60
    │   │   ├── obLight: 1
    │   │   ├── algoEnabled: 0
    │   │   ├── mode: 1
    │   │   ├── healthScore: 100
    │   │   └── serverTimestamp: 1719532800000  ← Firebase server sets this
    │   └── -Ndef456/
    │       └── ...
    │
    ├── latest/                        ← PUT (overwritten each time)
    │   ├── theta: 12.45              ← Same 39 fields as above
    │   ├── waterHeight: 187.34       ← Website reads THIS for dashboard
    │   └── ...                       ← Always the most recent reading
    │
    ├── buffered_data/                 ← POST (data recovered from SD)
    │   ├── -Nghi789/                  ← Same format as data/
    │   └── ...                        ← Uploaded when WiFi comes back
    │
    ├── diagnostics/                   ← POST (diagnostic frames)
    │   ├── -Njkl012/
    │   │   ├── raw: "$DIAG,MPU_ID=1,..."
    │   │   └── timestamp: 1719532800000
    │   └── ...
    │
    ├── status/                        ← PUT (updated every poll cycle)
    │   ├── online: true
    │   ├── wifiRSSI: -45
    │   ├── sdAvail: true
    │   ├── sdBuffered: 0
    │   ├── s3Active: true
    │   ├── realTimeMode: false
    │   ├── freeHeap: 180000
    │   ├── uptime: 3600
    │   └── lastUpdate: 1719532800000  ← Firebase server timestamp
    │
    └── commands/                      ← Website WRITES here, C3 READS here
        ├── config/                    ← GET then DELETE after reading
        │   ├── normalRate: 300
        │   ├── highRate: 30
        │   └── hMaxCm: 250.0
        │
        ├── ota/                       ← GET, then PUT status updates
        │   ├── url: "https://storage.../firmware.bin"
        │   ├── size: 123456
        │   ├── pending: true          ← C3 sets to false after reading
        │   ├── status: "downloading"  ← C3 updates: downloading/flashing/success/failed
        │   └── message: "Success"
        │
        ├── realtime/                  ← GET (not deleted, just read)
        │   └── enabled: true
        │
        └── diagnostic/               ← GET then DELETE after reading
            └── run: true
```

---

## Firebase REST API — How Each Call Works

```
OPERATION    HTTP METHOD    URL                                                           BODY
───────────  ─────────────  ────────────────────────────────────────────────────────────  ──────────────
Push data    POST           https://project.firebaseio.com/devices/buoy_001/data.json     {JSON payload}
                            ?auth=DATABASE_SECRET

Update       PUT            https://project.firebaseio.com/devices/buoy_001/latest.json   {JSON payload}
latest                      ?auth=DATABASE_SECRET

Read         GET            https://project.firebaseio.com/devices/buoy_001/commands/      (none)
commands                    config.json?auth=DATABASE_SECRET

Delete       DELETE         https://project.firebaseio.com/devices/buoy_001/commands/      (none)
command                     config.json?auth=DATABASE_SECRET

Each push to data/ gets a Firebase auto-generated key like "-Nabc123"
Each put to latest/ overwrites the previous value completely
Each get returns the JSON value at that path, or "null" if empty
Each delete removes the node so the command doesn't re-trigger
```

---

## CSV → JSON Conversion — Field By Field

```
CSV INPUT (one line, 39 fields separated by commas):
12.45,187.34,8.23,-3.11,200.00,43.12,1013.25,28.50,1013.00,0.25,1,0.00,1,1719532800,2025-01-15 14:30:00,1,2.340,1,3600,192.50,45.20,14.583210,120.984530,15.2,8,1,-67,1,1,1,1,0,85.3,60,60,1,0,1,100

JSON OUTPUT (built manually, no ArduinoJson library):
{
  "theta": 12.45,                              ← field[0]  — CSV field 1
  "waterHeight": 187.34,                       ← field[1]  — CSV field 2
  "corrTiltX": 8.23,                           ← field[2]  — CSV field 3
  "corrTiltY": -3.11,                          ← field[3]  — CSV field 4
  "olpLength": 200.00,                         ← field[4]  — CSV field 5
  "horizDist": 43.12,                          ← field[5]  — CSV field 6
  "pressure": 1013.25,                         ← field[6]  — CSV field 7
  "temperature": 28.50,                        ← field[7]  — CSV field 8
  "baselinePressure": 1013.00,                 ← field[8]  — CSV field 9
  "pressureDev": 0.25,                         ← field[9]  — CSV field 10
  "submersionState": 1,                        ← field[10] — CSV field 11
  "estDepth": 0.00,                            ← field[11] — CSV field 12
  "bmpAvail": 1,                               ← field[12] — CSV field 13
  "unixTime": 1719532800,                      ← field[13] — CSV field 14
  "dateTime": "2025-01-15 14:30:00",           ← field[14] — CSV field 15 (ONLY STRING FIELD)
  "rtcValid": 1,                               ← field[15] — CSV field 16
  "rateOfRise": 2.340,                         ← field[16] — CSV field 17
  "alertLevel": 1,                             ← field[17] — CSV field 18
  "sessionDur": 3600,                          ← field[18] — CSV field 19
  "peakHeight": 192.50,                        ← field[19] — CSV field 20
  "minHeight": 45.20,                          ← field[20] — CSV field 21
  "latitude": 14.583210,                       ← field[21] — CSV field 22
  "longitude": 120.984530,                     ← field[22] — CSV field 23
  "altitude": 15.2,                            ← field[23] — CSV field 24
  "satellites": 8,                             ← field[24] — CSV field 25
  "gpsFix": 1,                                 ← field[25] — CSV field 26
  "simRSSI": -67,                              ← field[26] — CSV field 27
  "simReg": 1,                                 ← field[27] — CSV field 28
  "simAvail": 1,                               ← field[28] — CSV field 29
  "zone": 1,                                   ← field[29] — CSV field 30
  "responseLevel": 1,                          ← field[30] — CSV field 31
  "sustainedRise": 0,                          ← field[31] — CSV field 32
  "battery": 85.3,                             ← field[32] — CSV field 33
  "sampleInterval": 60,                        ← field[33] — CSV field 34
  "txInterval": 60,                            ← field[34] — CSV field 35
  "obLight": 1,                                ← field[35] — CSV field 36
  "algoEnabled": 0,                            ← field[36] — CSV field 37
  "mode": 1,                                   ← field[37] — CSV field 38
  "healthScore": 100,                          ← field[38] — CSV field 39
  "serverTimestamp": {".sv": "timestamp"}      ← ADDED BY C3 — Firebase fills in server time
}

VALIDATION:
    CSV must have at least 30 commas (31+ fields) to be accepted
    If fewer → logged as "CSV rejected: only X commas (need 38)" → DROPPED
    Field 15 (dateTime) is the ONLY field wrapped in quotes in JSON
    All other 38 fields are pasted as raw numeric values
```

---

## Software Serial Receiver — How GPIO 10 Receives Data

```
The S3 sends CSV via bit-banged UART on its GPIO 14 at 9600 baud.
The C3 receives it on GPIO 10 using interrupt + hardware timer.

HOW IT WORKS:

1. GPIO 10 has a FALLING EDGE interrupt attached (start bit detection)

2. When S3 starts sending a byte:
   
   S3 GPIO 14:  _____|‾‾‾‾|____|‾‾‾‾|____|‾‾‾‾|____|‾‾‾‾|____|‾‾‾‾|______|‾‾‾‾‾‾‾
                 IDLE  START  D0    D1    D2    D3    D4    D5    D6    D7    STOP
                       bit                                                    bit

3. Falling edge of START BIT triggers swSerialStartBitISR():
   - Disables the falling edge interrupt (prevent re-triggering)
   - Starts hardware timer
   - First timer alarm set to 1.5 bit times (156 µs)
     → This lands in the MIDDLE of the first data bit (D0)

4. Timer ISR (swSerialBitTimerISR) fires at the middle of each data bit:
   - Reads GPIO 10 value (HIGH=1, LOW=0)
   - Stores bit in swCurrentByte (LSB first)
   - Sets timer for next bit (1 bit time = 104 µs)
   - After 8 data bits, waits one more bit time for stop bit

5. After stop bit:
   - Timer disabled
   - Complete byte pushed into ring buffer (swRxBuffer)
   - Falling edge interrupt re-attached for next byte

6. Main loop reads from ring buffer:
   - swSerialRead() returns one byte or -1
   - swSerialAvailable() returns number of bytes in buffer

RING BUFFER:
   Size: 1200 bytes (enough for full 39-field CSV line)
   Write pointer: swRxWriteIdx (written by ISR)
   Read pointer: swRxReadIdx (read by main loop)
   Overflow flag: swRxOverflow (set if buffer full, data lost)

TIMING:
   9600 baud = 104.17 µs per bit
   10 bits per byte (1 start + 8 data + 1 stop)
   1 byte takes ~1.04 ms to receive
   39-field CSV ≈ 500-800 bytes ≈ 520-830 ms to receive
```

---

## SD Card Buffering — How Offline Data Is Saved And Recovered

```
FILES ON SD CARD:
    /buffer.csv      — CSV lines, one per line, appended during WiFi outage
    /buf_idx.txt     — Single number: byte position of last flushed line
    /ota_firmware.bin — Temporary: firmware binary during OTA update

BUFFERING (WiFi is DOWN):
    1. S3 sends CSV → C3 receives it
    2. C3 opens /buffer.csv in APPEND mode
    3. C3 writes the CSV line + newline
    4. C3 closes the file
    5. sdBufferedCount incremented
    6. Maximum 10,000 lines stored (then oldest would be dropped)

FLUSHING (WiFi comes BACK):
    1. Every 2 seconds, C3 calls flushSDOneBatch()
    2. Reads /buf_idx.txt to find where last flush left off (byte position)
    3. Opens /buffer.csv, seeks to that position
    4. Reads up to 10 lines
    5. For each line:
       a. Convert CSV to JSON
       b. POST to Firebase at devices/buoy_001/buffered_data/
       c. If POST succeeds → update flush position
       d. If POST fails → STOP flushing, try again in 2 seconds
    6. Updates /buf_idx.txt with new position
    7. When all lines flushed:
       a. Delete /buffer.csv
       b. Delete /buf_idx.txt
       c. Set sdBufferedCount = 0
       d. Log "SD buffer fully flushed — files cleaned up"

WHY BATCHES OF 10?
    Each Firebase POST takes ~200-500ms (HTTPS handshake + upload)
    10 lines × 500ms = 5 seconds per batch
    Don't want to block the main loop too long
    New real-time data still needs to flow during flush
```

---

## Real-Time Mode — What Changes

```
NORMAL MODE (realTimeMode = RT_OFF):
    CSV received → Push to Firebase → Also save to SD as backup
    Poll Firebase every 5 seconds
    SD buffer flushed in background

REAL-TIME MODE (realTimeMode = RT_ON):
    CSV received → Push to Firebase IMMEDIATELY → Skip SD entirely
    Poll Firebase every 1 second (faster command response)
    SD buffer flush is PAUSED (don't waste bandwidth)

    Triggered by website writing:
        devices/buoy_001/commands/realtime → {"enabled": true}

    Used when someone is actively watching the dashboard
    and wants minimum latency

WHY SKIP SD IN REAL-TIME?
    SD card write adds ~10-50ms delay per line
    Don't need backup when actively monitoring
    If WiFi drops during real-time mode → data IS lost
    (acceptable trade-off for lower latency)
```

---

## Commands From Firebase → What The C3 Does With Each

```
COMMAND                 FIREBASE PATH                        C3 ACTION
──────────────────────  ───────────────────────────────────  ────────────────────────────────────────
Config update           devices/buoy_001/commands/config     Parse normalRate, highRate, hMaxCm
                        {"normalRate":300,"highRate":30,      Set cfgDirty=true
                         "hMaxCm":250.0}                     Next loop: send $CFG,300,30,250.00 to S3
                                                             DELETE the Firebase node after reading

OTA firmware update     devices/buoy_001/commands/ota        Parse URL and file size
                        {"url":"https://...","size":123456,  Set otaPending=true
                         "pending":true}                     PUT {"pending":false,"status":"downloading"}
                                                             Next loop: start full OTA process
                                                             (download → flash → verify → report)

Real-time mode          devices/buoy_001/commands/realtime   Parse enabled flag
                        {"enabled":true}                     Set realTimeMode = RT_ON or RT_OFF
                                                             Change poll interval (5s → 1s)
                                                             DO NOT delete (persistent flag)

Diagnostic request      devices/buoy_001/commands/diagnostic Parse run flag
                        {"run":true}                         Set diagPending=true
                                                             Next loop: send $DIAGRUN to S3
                                                             DELETE the Firebase node after reading
```

---

## Commands Between C3 and S3

```
C3 → S3 (Hardware Serial1, GPIO 21 → S3 GPIO 44):
────────────────────────────────────────────────────────────────────
$CFG,300,30,250.00          Set normalRate=300s, highRate=30s, hMax=250cm
$DIAGRUN                    Trigger full diagnostic on S3
$PING                       Connectivity check
$SIMSTAT,-67,1,1            Forward SIM status (rssi, registered, available)
$SETFLAG,OB_LIGHT,1         Set feature flag
$SETFLAG,ALGORITHM,1        Set feature flag

S3 → C3 (Hardware Serial1, S3 GPIO 43 → C3 GPIO 20):
────────────────────────────────────────────────────────────────────
$PONG                       Response to $PING
$CFG_ACK                    Config update acknowledged
$DIAG_ACK                   Diagnostic trigger acknowledged
$FLAG_ACK                   Flag update acknowledged
$DIAG,MPU_ID=1,...          Diagnostic results (also comes on GPIO 14)
$SETAPN                     Forwarded SIM command from debugger
$REINITSIM                  Forwarded SIM command from debugger
$TESTGPRS                   Forwarded SIM command from debugger

S3 → C3 (Software Serial, S3 GPIO 14 → C3 GPIO 10):
────────────────────────────────────────────────────────────────────
12.45,187.34,8.23,...,100   39-field CSV data line
$DIAG,MPU_ID=1,...          Diagnostic results (redundant channel)
```

---

## OTA Firmware Update — Complete Step-By-Step

```
STEP    WHO         WHAT HAPPENS                                         TIME
──────  ──────────  ─────────────────────────────────────────────────    ──────
 1      Website     User uploads firmware.bin to Firebase Storage        —
 2      Website     Writes OTA command to Firebase RTDB:                 —
                    {url: "https://...", size: 123456, pending: true}

 3      C3          Polls Firebase, finds pending=true                   5s poll
 4      C3          Sets pending=false, status="downloading"             —
 5      C3          Downloads .bin via HTTPS to SD: /ota_firmware.bin    10-60s
                    Progress logged every 10%
 6      C3          Verifies file size matches expected                  <1s

 7      C3          ENTERS S3 BOOTLOADER MODE:                          650ms
                    a. GPIO 2 (BOOT) → LOW                              50ms
                    b. GPIO 3 (RESET) → LOW                             100ms
                    c. GPIO 3 (RESET) → HIGH                            —
                    d. Wait for bootloader                              500ms
                    e. Switch Serial1 from 9600 to 115200 baud          100ms

 8      C3          SYNC with bootloader:                               <1s
                    Send: 0x07 0x07 0x12 0x20 + 32×0x55
                    Wait for sync response
                    Up to 10 attempts

 9      C3          FLASH_BEGIN command:                                 <1s
                    Send: erase size, block count, block size, offset=0
                    Wait for OK response (10s timeout)

10      C3          FLASH_DATA commands (loop):                         30-120s
                    Read 16KB block from SD card
                    Pad last block with 0xFF
                    Calculate checksum (XOR all bytes, starting with 0xEF)
                    Send via SLIP framing
                    Wait for OK response (10s timeout per block)
                    Progress logged every 10%

11      C3          FLASH_END command:                                  <1s
                    Send with reboot flag = 0 (means DO reboot)
                    May not get response (S3 reboots immediately)

12      C3          EXIT BOOTLOADER MODE:                               650ms
                    a. GPIO 2 (BOOT) → HIGH                             50ms
                    b. GPIO 3 (RESET) → LOW                             100ms
                    c. GPIO 3 (RESET) → HIGH                            —
                    d. Wait for S3 boot                                 500ms
                    e. Switch Serial1 back to 9600 baud                 100ms

13      C3          Wait 3 seconds for S3 to boot new firmware          3s

14      C3          VERIFY S3:                                          3-15s
                    Send $PING on Serial1
                    Wait 3 seconds for $PONG
                    Up to 5 attempts (with 2s delay between)

15      C3          REPORT to Firebase:                                 <1s
                    PUT status="success" or status="failed"

16      C3          CLEANUP:                                            <1s
                    Delete /ota_firmware.bin from SD
                    Set currentState = STATE_NORMAL
                    Set s3Active = true

TOTAL TIME: approximately 1-3 minutes for a typical firmware
```

---

## SLIP Protocol — How Bootloader Communication Works

```
SLIP FRAMING (Serial Line Internet Protocol):
    Every packet wrapped with 0xC0 at start and end
    
    Raw byte 0xC0 in data → escaped as 0xDB 0xDC
    Raw byte 0xDB in data → escaped as 0xDB 0xDD

    Example:
        Data: [0x00, 0x08, 0xC0, 0x24]
        SLIP: [0xC0, 0x00, 0x08, 0xDB, 0xDC, 0x24, 0xC0]
              START              ESCAPED 0xC0          END

COMMAND PACKET FORMAT (C3 → S3 bootloader):
    Byte 0:    0x00 (direction = request)
    Byte 1:    Command opcode
    Byte 2-3:  Data length (little-endian)
    Byte 4-7:  Checksum (XOR of data bytes, starting with 0xEF)
    Byte 8+:   Data payload

RESPONSE PACKET FORMAT (S3 bootloader → C3):
    Byte 0:    0x01 (direction = response)
    Byte 1:    Command opcode (echo)
    Byte 2-3:  Data length
    Byte 4-7:  Value (command-specific)
    Byte 8:    Status (0x00 = success)
    Byte 9:    Error code

COMMANDS USED:
    0x08  SYNC         — Handshake with bootloader
    0x02  FLASH_BEGIN  — Prepare flash (erase sectors)
    0x03  FLASH_DATA   — Write one block of firmware data
    0x04  FLASH_END    — Finish flash, optionally reboot

FLASH_DATA PACKET DATA PAYLOAD:
    Byte 0-3:   Data length (of actual firmware bytes in this block)
    Byte 4-7:   Sequence number (0, 1, 2, ...)
    Byte 8-11:  0x00000000 (reserved)
    Byte 12-15: 0x00000000 (reserved)
    Byte 16+:   Actual firmware data (up to 16KB per block)
```

---

## JSON Parsing — How Config Values Are Extracted Without ArduinoJson

```
The C3 has 4 manual JSON parsers (no library needed):

extractJsonStringValue(json, "key", outBuf, bufSize)
    Searches for "key":"value" pattern
    Extracts characters between the quotes after the colon
    Example: {"url":"https://example.com"} → "https://example.com"

extractJsonIntValue(json, "key", defaultVal)  
    Searches for "key":123 pattern
    Also handles "key":"123" (quoted numbers)
    Skips whitespace after colon
    Extracts digits until comma, brace, or whitespace
    Returns defaultVal if key not found
    Example: {"normalRate":300} → 300

extractJsonFloatValue(json, "key", defaultVal)
    Same as int but calls .toFloat()
    Example: {"hMaxCm":250.5} → 250.5

extractJsonBoolValue(json, "key", defaultVal)
    Searches for "key":true or "key":false
    Also handles "key":"true" (quoted booleans)
    Example: {"enabled":true} → true

LIMITATIONS (documented in code):
    ✗ Cannot handle nested objects
    ✗ Cannot handle arrays
    ✗ Cannot handle escaped quotes in strings
    ✗ Only works for simple flat JSON objects
    ✓ Works perfectly for Firebase's simple command responses
```

---

## WiFi Reconnection Logic

```
INITIAL CONNECTION (setup):
    WiFi.mode(WIFI_STA)
    WiFi.begin(SSID, PASSWORD)
    Wait up to 15 seconds
    If connected → STATE_NORMAL
    If failed → STATE_BUFFERING

MONITORING (every loop iteration):
    If WiFi.status() == WL_CONNECTED:
        If was disconnected → log "WiFi reconnected"
        If was STATE_BUFFERING → switch to STATE_NORMAL

    If WiFi.status() != WL_CONNECTED:
        If was connected → log "WiFi connection lost"
        Switch to STATE_BUFFERING

RECONNECTION (every 30 seconds while disconnected):
    WiFi.disconnect()
    Wait 100ms
    WiFi.begin(SSID, PASSWORD)
    Wait up to 10 seconds
    If connected → STATE_NORMAL, log success
    If failed → stay in STATE_BUFFERING, try again in 30s
```

---

## Device Status Update — What Gets Pushed Every Poll Cycle

```
Every 5 seconds (or 1 second in real-time mode), the C3 writes:

devices/buoy_001/status:
{
    "online": true,                    ← Always true when C3 is running
    "wifiRSSI": -45,                   ← WiFi signal strength in dBm
    "sdAvail": true,                   ← Is SD card working?
    "sdBuffered": 0,                   ← Lines waiting to be flushed
    "s3Active": true,                  ← false only during OTA flash
    "realTimeMode": false,             ← Is real-time bypass active?
    "freeHeap": 180000,                ← C3 free RAM in bytes
    "uptime": 3600,                    ← C3 uptime in seconds
    "lastUpdate": {".sv":"timestamp"}  ← Firebase server timestamp
}

This lets the website show:
    "Buoy is online"
    "WiFi signal: -45 dBm"
    "No data backlog"
    "All systems active"
    "Last seen: 2 seconds ago"
```

---

## What Happens When A CSV Line Arrives — Complete Flow

```
SCENARIO 1: WiFi UP, Normal Mode
    S3 GPIO 14 → bit-bang → C3 GPIO 10 ring buffer
    → main loop reads ring buffer
    → assembles bytes until \n
    → validates comma count ≥ 30
    → csvToFirebaseJson() converts to JSON
    → HTTP POST to devices/buoy_001/data.json (auto-key)
    → HTTP PUT to devices/buoy_001/latest.json (overwrite)
    → Also appended to SD /buffer.csv (backup)

SCENARIO 2: WiFi DOWN
    S3 GPIO 14 → bit-bang → C3 GPIO 10 ring buffer
    → main loop reads ring buffer
    → assembles bytes until \n
    → validates comma count ≥ 30
    → SD.open("/buffer.csv", FILE_APPEND)
    → write CSV line
    → close file
    → sdBufferedCount++
    → log "Buffered to SD (total: 47)"

SCENARIO 3: WiFi COMES BACK (47 lines buffered)
    State changes to STATE_NORMAL
    Every 2 seconds: flushSDOneBatch()
        Read flush index from /buf_idx.txt (byte position)
        Open /buffer.csv, seek to position
        Read 10 lines
        For each: convert to JSON, POST to buffered_data/
        Update /buf_idx.txt
        sdBufferedCount -= 10
    After 5 batches (10 seconds): all 47 lines flushed
    Delete /buffer.csv and /buf_idx.txt

SCENARIO 4: Real-Time Mode, WiFi UP
    S3 GPIO 14 → bit-bang → C3 GPIO 10 ring buffer
    → main loop reads ring buffer
    → assembles bytes until \n
    → validates comma count ≥ 30
    → csvToFirebaseJson() converts to JSON
    → HTTP POST to devices/buoy_001/data.json
    → HTTP PUT to devices/buoy_001/latest.json
    → SD is SKIPPED entirely (no backup)

SCENARIO 5: Diagnostic Frame (starts with "$DIAG,")
    → NOT treated as CSV data
    → Wrapped in JSON: {"raw":"$DIAG,...","timestamp":{".sv":"timestamp"}}
    → POST to devices/buoy_001/diagnostics/
    → NOT saved to SD

SCENARIO 6: Corrupted CSV (< 30 commas)
    → Logged as warning
    → DROPPED — not saved anywhere
```

---

## The Big Picture — How Both Chips Work Together

```
                    PHYSICAL WORLD
                         │
                    [Water rises]
                         │
                    [Buoy tilts]
                         │
              ┌──────────┴──────────┐
              │     ESP32-S3        │
              │   (Sensor Brain)    │
              │                     │
              │  MPU6050 → tilt     │
              │  BMP280 → pressure  │
              │  DS1307 → time      │
              │  GPS → location     │
              │  Battery → voltage  │
              │                     │
              │  Sensor fusion      │
              │  Flood detection    │
              │  39-field CSV       │
              │                     │
              │  GPIO 14 ──────────────── SW UART 9600 ───┐
              │  GPIO 43 ──────────────── HW UART 9600 ──┐│
              │  GPIO 44 ──────────────── HW UART 9600 ─┐││
              └─────────────────────┘                    │││
                                                         │││
              ┌──────────────────────┐                   │││
              │     ESP32-C3         │                   │││
              │  (Comm Bridge)       │                   │││
              │                      │                   │││
              │  GPIO 10 ←───────────────────────────────┘││  CSV data
              │  GPIO 20 ←────────────────────────────────┘│  S3 responses
              │  GPIO 21 ─────────────────────────────────┘   C3 commands
              │                      │
              │  WiFi ──────────────────── Firebase RTDB
              │  SD Card ───────────────── Offline buffer
              │  GPIO 2 ───────────────── S3 BOOT pin
              │  GPIO 3 ───────────────── S3 RESET pin
              └──────────────────────┘
                         │
                    [Firebase RTDB]
                         │
                    [Website/Dashboard]
                         │
                    [User sees flood warning]
```
