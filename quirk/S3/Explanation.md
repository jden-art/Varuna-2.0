

# Complete Detailed Breakdown — EVERYTHING

---

## Why Are The Four Modes Necessary?

You're right that the sampling rate increases as water rises. But the modes aren't just about sampling rate. **Each mode changes HOW the water height is calculated**, because the physics is different in each situation:

### The Problem: One Formula Doesn't Work For All Situations

```
MODE_SLACK:
    Tether is dangling loose. There is NO tension.
    The buoy is just floating freely.
    The tilt angle means NOTHING here — it's just waves/wind.
    You CANNOT use H = L × cos(θ) because the tether isn't straight.
    → Water height is reported as 0
    → The angle is MEANINGLESS for measurement

MODE_TAUT:
    Water has risen enough to pull the tether tight.
    NOW the tether is a straight line at angle θ.
    NOW H = L × cos(θ) works.
    → This is the normal measurement mode
    → Water height is calculated geometrically

MODE_FLOOD:
    Water is so high the buoy is almost directly above the anchor.
    θ is approaching 0°, meaning cos(θ) ≈ 1, meaning H ≈ L.
    The tether is nearly vertical.
    → Water height ≈ tether length
    → The ratio H/H_max is above 95%

MODE_SUBMERGED:
    Water went ABOVE the buoy. The buoy is underwater.
    The tilt angle is now USELESS again — buoy is floating underwater.
    The BMP280 pressure sensor takes over.
    Water depth = (pressure above baseline) / (water density × gravity)
    → Water height = tether length + depth below surface
    → Completely different formula, completely different sensor
```

### So the modes exist because:

| Mode | Which sensor matters | Which formula is used | Why it's different |
|---|---|---|---|
| SLACK | None useful | H = 0 | No tension, angle is noise |
| TAUT | MPU6050 (tilt) | H = L × cos(θ) | Tether is straight, geometry works |
| FLOOD | MPU6050 (tilt) | H = L × cos(θ) ≈ L | θ is small, nearly vertical |
| SUBMERGED | BMP280 (pressure) | H = L + depth from pressure | Buoy is underwater, tilt is meaningless |

**Without modes**, you'd be applying `H = L × cos(θ)` when the tether is slack (giving garbage numbers) or when the buoy is underwater (also garbage).

### And YES, the modes ALSO affect sampling rate, alert level, zone, and response:

```
MODE_SLACK:
    → Alert: GREEN
    → Zone: SAFE
    → Response: NONE
    → Sampling: every 15 minutes (save battery)

MODE_TAUT:
    → Alert: GREEN or YELLOW (depends on ratio)
    → Zone: SAFE or WATCH or WARNING
    → Response: NONE or MONITOR or PREPARE
    → Sampling: interpolated between 15min and 1min based on flood ratio

MODE_FLOOD:
    → Alert: RED
    → Zone: WARNING or CRITICAL
    → Response: ACT
    → Sampling: every 1 minute (maximum data)

MODE_SUBMERGED:
    → Alert: BLACK
    → Zone: CRITICAL
    → Response: EVACUATE
    → Sampling: every 1 minute
    → Persistence filter is only 3 readings (not 10) because it's urgent
```

---

## The Dynamic Sampling Rate — How It Actually Works

```
floodRatio = waterHeight / maxHeight

If floodRatio < 0.50 (below 50%):
    → Sample every 900 seconds (15 minutes)

If floodRatio > 0.80 (above 80%):
    → Sample every 60 seconds (1 minute)

If floodRatio is between 0.50 and 0.80:
    → Linear interpolation between 900s and 60s

Example:
    floodRatio = 0.50 → 900 seconds
    floodRatio = 0.55 → 900 - (0.167 × 840) = 760 seconds
    floodRatio = 0.65 → 900 - (0.500 × 840) = 480 seconds
    floodRatio = 0.75 → 900 - (0.833 × 840) = 200 seconds
    floodRatio = 0.80 → 60 seconds
    floodRatio = 0.95 → 60 seconds
```

---

## ALL 39 CSV Fields — Complete Detail

Here is every single field, what it contains, where it comes from, and an example value:

```
FIELD  NAME                    SOURCE                  EXAMPLE         EXPLANATION
─────  ──────────────────────  ──────────────────────  ──────────────  ──────────────────────────────────────
 1     combinedTheta           Sensor fusion output    12.45           Total tilt angle from vertical (degrees)
                                                                       = sqrt(correctedTiltX² + correctedTiltY²)

 2     waterHeightCm           Flood detection engine  187.34          Computed water height in cm
                                                                       SLACK: 0
                                                                       TAUT/FLOOD: L × cos(θ)
                                                                       SUBMERGED: L + depth

 3     correctedTiltX          Sensor fusion output    8.23            Tilt around X axis minus reference (degrees)
                                                                       = filteredTiltX - refTiltX

 4     correctedTiltY          Sensor fusion output    -3.11           Tilt around Y axis minus reference (degrees)
                                                                       = filteredTiltY - refTiltY

 5     olpLengthCm             Configuration           200.00          Tether (OLP) length in cm
                                                                       Set by $CFG command or SETHMAX

 6     horizontalDistCm        Flood detection engine  43.12           Horizontal displacement of buoy from anchor
                                                                       = L × sin(θ)

 7     currentPressure         BMP280 sensor           1013.25         Atmospheric pressure in hPa
                                                                       Read from BMP280 compensation algorithm

 8     currentTemperature      BMP280 sensor           28.50           Temperature in °C
                                                                       From BMP280 temperature compensation

 9     baselinePressure        Calibration             1013.00         Pressure baseline set at deployment (hPa)
                                                                       Average of 50 samples during calibration

10     pressureDeviation       Computed                0.25            Current pressure minus baseline (hPa)
                                                                       = currentPressure - baselinePressure
                                                                       Large positive = submerged

11     submersionState         Flood mode enum         1               Current operating mode (integer)
                                                                       0 = SLACK
                                                                       1 = TAUT
                                                                       2 = FLOOD
                                                                       3 = SUBMERGED

12     estimatedDepthCm        Pressure computation    0.00            Depth below water surface in cm
                                                                       Only nonzero in MODE_SUBMERGED
                                                                       = (gaugePressurePa / (ρ × g)) × 100

13     bmpAvailable            Sensor init flag        1               Is BMP280 sensor working?
                                                                       0 = no, 1 = yes

14     rtcUnixTime             RTC computation         1719532800      Unix timestamp (seconds since 1970-01-01)
                                                                       From DS1307 RTC or GPS fallback

15     dateTimeStr             RTC formatting          2025-01-15 14:30:00
                                                                       Human-readable datetime string
                                                                       From RTC, or GPS if RTC fails,
                                                                       or "0000-00-00 00:00:00" if both fail

16     rtcValid                RTC status              1               Is RTC time considered valid?
                                                                       Checks: sec<60, min<60, hr<24,
                                                                       day 1-31, month 1-12, year 2024-2099

17     rateOfRiseCmPer15Min    Session statistics      2.340           Water level rise rate (cm per 15 minutes)
                                                                       = (currentHeight - previousHeight) / dt × 15

18     floodAlertLevel         Alert computation       1               Alert color code (integer)
                                                                       0 = GREEN (safe)
                                                                       1 = YELLOW (elevated)
                                                                       2 = RED (flood)
                                                                       3 = BLACK (critical/submerged)

19     sessionDuration         Timer                   3600            Seconds since firmware booted
                                                                       = (millis() - sessionStartMs) / 1000

20     peakHeightCm            Session statistics      192.50          Highest water level recorded this session
                                                                       Only increases, never decreases

21     minHeightCm             Session statistics      45.20           Lowest nonzero water level this session
                                                                       Initialized to 99999, reported as 0 until
                                                                       first real reading

22     latitude                GPS parser              14.583210       GPS latitude in decimal degrees
                                                                       Parsed from $GPGGA or $GPRMC sentences
                                                                       Negative = South

23     longitude               GPS parser              120.984530      GPS longitude in decimal degrees
                                                                       Parsed from $GPGGA or $GPRMC sentences
                                                                       Negative = West

24     altitude                GPS parser              15.2            GPS altitude in meters above sea level
                                                                       From $GPGGA field 9

25     gpsSatellites           GPS parser              8               Number of GPS satellites in view
                                                                       From $GPGGA field 7

26     gpsFixValid             GPS parser              1               Does GPS have a valid position fix?
                                                                       0 = no fix, 1 = valid fix
                                                                       From GGA fix quality or RMC status

27     simSignalRSSI           C3 forwarded data       -67             SIM module signal strength (dBm)
                                                                       Received from C3 via $SIMSTAT command
                                                                       S3 has no SIM — C3 reports this

28     simRegistered           C3 forwarded data       1               Is SIM registered to a network?
                                                                       0 = no, 1 = yes
                                                                       From C3 via $SIMSTAT

29     simAvailable            C3 forwarded data       1               Is SIM module physically present/working?
                                                                       0 = no, 1 = yes
                                                                       From C3 via $SIMSTAT

30     currentZone             Zone computation        1               Water zone classification (integer)
                                                                       0 = SAFE (< 50% of max)
                                                                       1 = WATCH (50-80%)
                                                                       2 = WARNING (80-95%)
                                                                       3 = CRITICAL (> 95% or submerged)

31     currentResponse         Response computation    1               Recommended response action (integer)
                                                                       0 = NONE
                                                                       1 = MONITOR
                                                                       2 = PREPARE
                                                                       3 = ACT
                                                                       4 = EVACUATE

32     sustainedRise           Session statistics      0               Is water rising consistently?
                                                                       0 = no, 1 = yes
                                                                       Set to 1 after 3 consecutive readings
                                                                       where rise > 1 cm/15min

33     batteryPercent          ADC reading             85.3            Battery charge percentage
                                                                       LiPo: 3.0V = 0%, 4.2V = 100%
                                                                       16-sample averaged ADC reading
                                                                       Through voltage divider (÷2)

34     sampleInterval          Dynamic rate            60              Current sampling interval in seconds
                                                                       Interpolated based on flood ratio
                                                                       Range: highRateSec to normalRateSec

35     transmitInterval        Dynamic rate            60              Current transmit interval in seconds
                                                                       Currently same as sampleInterval

36     obLightEnabled          Feature flag            1               Is the onboard light enabled?
                                                                       Set from website via C3 → $SETFLAG

37     algorithmEnabled        Feature flag            0               Is advanced algorithm mode enabled?
                                                                       Set from website via C3 → $SETFLAG

38     currentMode             Flood mode enum         1               Same as field 11 (redundant for clarity)
                                                                       0 = SLACK, 1 = TAUT
                                                                       2 = FLOOD, 3 = SUBMERGED

39     healthScore             Health computation      100             System health percentage (0-100)
                                                                       MPU working = +40 points
                                                                       BMP working = +30 points
                                                                       RTC working + valid time = +20 points
                                                                       GPS working + fix = +10 points
```

### Example Complete CSV Output Line:

```
12.45,187.34,8.23,-3.11,200.00,43.12,1013.25,28.50,1013.00,0.25,1,0.00,1,1719532800,2025-01-15 14:30:00,1,2.340,1,3600,192.50,45.20,14.583210,120.984530,15.2,8,1,-67,1,1,1,1,0,85.3,60,60,1,0,1,100
```

---

## All Commands The S3 Accepts

### From C3 (Serial2, 9600 baud, GPIO 43/44):

```
COMMAND                              RESPONSE              WHAT IT DOES
───────────────────────────────────  ────────────────────  ─────────────────────────────────────────
$CFG,900,60,200.0                    $CFG_ACK              Sets normalRate=900s, highRate=60s,
                                                            hMax=200cm, olpLength=200cm
                                                            Recomputes dynamic sampling rate

$DIAGRUN                             $DIAG_ACK             Triggers a full 11-test diagnostic
                                                            Results sent back as $DIAG frame

$PING                                $PONG                 Connectivity check

$SIMSTAT,-67,1,1                     (none)                Updates SIM status fields:
                                                            rssi=-67, registered=true, available=true
                                                            These get included in CSV fields 27-29

$SETFLAG,OB_LIGHT,1                  $FLAG_ACK             Sets obLightEnabled = true (field 36)

$SETFLAG,ALGORITHM,1                 $FLAG_ACK             Sets algorithmEnabled = true (field 37)
```

### From Debugger (USB Serial, 115200 baud):

```
COMMAND                              RESPONSE              WHAT IT DOES
───────────────────────────────────  ────────────────────  ─────────────────────────────────────────
PING                                 STATUS:PONG           Connectivity check

GETCONFIG                            STATUS:CONFIG ...     Dumps all configuration:
                                                            normalRate, highRate, hMax, olpLen,
                                                            mode, interval, healthScore,
                                                            MPU/BMP/RTC/GPS availability,
                                                            calibration status, baseline status

RECALIBRATE                          STATUS:Recalibration  Runs full calibration:
                                     complete               1000 gyro samples → offset
                                                            500 accel samples → reference tilt
                                                            50 pressure samples → baseline

GETTHRESH                            STATUS:THRESH ...     Shows all threshold values:
                                                            accelTaut, accelSlack, tiltTaut,
                                                            floodTheta, floodRatio, submersionPa

SETTHRESH=0.15,10.0,500.0            STATUS:THRESH         Sets three thresholds:
                                     updated                accelTaut=0.15 m/s²
                                                            floodTheta=10.0°
                                                            submersionPa=500.0 Pa
                                                            Auto-sets accelSlack = accelTaut × 0.67

RESETTHRESH                          STATUS:THRESH reset   Resets all thresholds to defaults:
                                     to defaults            accelTaut=0.15, accelSlack=0.10
                                                            tiltTaut=3.0°, floodTheta=10.0°
                                                            floodRatio=0.95, submersionPa=500.0

SETHMAX=300                          STATUS:H_MAX set      Sets maximum flood height to 300cm
                                     to 300.00 cm           Also updates olpLength to match

SETRATE=900,60                       STATUS:RATE set       Sets normalRate=900s, highRate=60s
                                                            Recomputes dynamic sampling rate

GETSTATUS                            STATUS:MODE=1 ...     Full real-time status dump:
                                                            mode, alert, zone, response,
                                                            height, theta, ratio,
                                                            pressure, depth, lateral accel,
                                                            battery%, interval, sustainedRise

SETAPN / REINITSIM / TESTGPRS        Forwarded to C3       S3 has no SIM module
                                     via Serial2            Prepends "$" and sends to C3
```

---

## All Diagnostic Tests (11 Tests)

```
TEST   WHAT IT CHECKS                    PASS CONDITION                    POINTS
─────  ────────────────────────────────  ──────────────────────────────    ──────
 1     MPU6050 WHO_AM_I register         Returns 0x68                      —
 2     Accelerometer magnitude           0.8g < |accel| < 1.2g            Part of 40
 3     Gyroscope drift at rest           < 5°/s residual after offset      Part of 40
 4     BMP280 Chip ID register           Returns 0x58                      —
 5     BMP280 pressure range             300 < pressure < 1200 hPa        Part of 30
 6     BMP280 temperature range          -40°C < temp < 85°C              Part of 30
 7     DS1307 oscillator running         Clock halt bit = 0                Part of 20
 8     DS1307 time validity              Valid date/time ranges            Part of 20
 9     GPS data recency                  Last update < 10 seconds ago      Part of 10
10     Battery voltage range             2.8V < voltage < 4.3V             —
11     C3 PING/PONG                      Receives $PONG within 2 seconds   —
```

### Diagnostic output sent to C3:

```
$DIAG,MPU_ID=1,MPU_ACCEL=0.998:1,MPU_GYRO=0.234:1,BMP_ID=1,BMP_P=1013.25:1,BMP_T=28.50:1,RTC_OSC=1,RTC_TIME=1,GPS_RECENT=1,GPS_SATS=8,BATT=3.95:1,C3_PONG=1,FAULTS=0,HEALTH=100
```

---

## All Output Channels — What Goes Where

```
CHANNEL                  PIN          BAUD     DIRECTION    CONTENT
───────────────────────  ───────────  ───────  ──────────   ─────────────────────────────
USB Serial               USB          115200   S3 → PC      39-field CSV every interval
                                                             STATUS: messages
                                                             ERROR: messages
                                                             WARNING: messages
                                                             Diagnostic results

USB Serial               USB          115200   PC → S3      Debugger commands (PING, etc.)

Software UART TX         GPIO 14      9600     S3 → C3      39-field CSV every interval
                                                             Diagnostic $DIAG frames
                                                             (bit-banged, interrupts disabled
                                                              during each byte for timing)

Serial2 TX               GPIO 43      9600     S3 → C3      Command responses ($PONG, etc.)
                                                             $CFG_ACK, $DIAG_ACK, $FLAG_ACK
                                                             Diagnostic $DIAG frames
                                                             Forwarded debugger commands

Serial2 RX               GPIO 44      9600     C3 → S3      $CFG, $DIAGRUN, $PING
                                                             $SIMSTAT, $SETFLAG

Serial1 RX               GPIO 6       9600     GPS → S3     NMEA sentences ($GPGGA, $GPRMC)
Serial1 TX               GPIO 7       9600     S3 → GPS     (not used, GPS is receive-only)
```

---

## The Main Loop — What Runs When

```
TASK    WHAT                          FREQUENCY        PRIORITY
──────  ────────────────────────────  ───────────────  ────────────────────
1       Sensor fusion                 Every 10ms       Highest (100Hz)
        Read MPU6050 (14 bytes)       (100 Hz)         Tilt must be smooth
        Convert to physical units
        Run complementary filter

2       GPS parsing                   Continuous       Non-blocking
        Read Serial1 characters       Every loop()     Characters buffered
        Parse complete NMEA           iteration        until newline
        sentences

3       C3 command processing         Continuous       Non-blocking
        Read Serial2 characters       Every loop()     Same buffering
        Execute complete commands      iteration

4       Debugger command processing   Continuous       Non-blocking
        Read USB Serial characters    Every loop()     Same buffering
        Execute complete commands      iteration

5       Data transmission             Dynamic          Based on flood ratio
        Update BMP280                 900s to 60s      Triggers all the slow
        Update RTC                                     sensor reads and
        Read battery                                   flood computation
        Compute water height                           then transmits CSV
        Classify flood mode                            on both channels
        Compute alerts/zones
        Update session stats
        Build and send CSV

6       Diagnostics                   Every 24 hours   Lowest
        Run 11 hardware tests         or on command    Takes ~1 second
        Compute health score                           (gyro drift test
        Send results                                    samples 100 readings)
```

---

## The Calibration Process — What Happens At Boot

```
STEP 1: Gyro Calibration (1000 samples over ~2 seconds)
    - Device must be stationary
    - Reads raw gyroscope 1000 times
    - Rejects any reading > ±20°/s (outlier)
    - Averages all valid readings
    - This average becomes the OFFSET subtracted from every future reading
    - Result: gyroOffsetX, gyroOffsetY, gyroOffsetZ (typically < 1°/s each)

STEP 2: Accel Calibration (500 samples over ~2 seconds)
    - Device must be stationary
    - Reads raw accelerometer 500 times
    - Rejects any reading where total magnitude isn't ~1g (0.9 to 1.1)
    - Averages all valid readings
    - Computes reference tilt angles from gravity direction
    - This becomes the ZERO POINT for all future tilt measurements
    - Result: refTiltX, refTiltY (the "this is level" angles)

STEP 3: Pressure Baseline (50 samples over ~2.5 seconds)
    - Device must be in air (not submerged)
    - Reads BMP280 pressure 50 times
    - Rejects readings outside 300-1100 hPa
    - Averages valid readings
    - This becomes the BASELINE for submersion detection
    - If current pressure exceeds baseline by 500+ Pa → SUBMERGED
    - Result: baselinePressure (typically ~1013 hPa at sea level)
```

---

## The Persistence Filter — Why Modes Don't Flicker

```
Without persistence filter:
    Wave hits buoy → lateral accel spikes → MODE_TAUT
    Wave passes    → lateral accel drops  → MODE_SLACK
    Next wave      → MODE_TAUT again
    Result: mode flickers between SLACK and TAUT every second = USELESS

With persistence filter:
    Wave hits buoy → lateral accel spikes → pendingMode = TAUT, count = 1
    Wave passes    → lateral accel drops  → pendingMode = SLACK, count = 1 (RESET)
    Next wave      → pendingMode = TAUT, count = 1 (RESET again)
    Result: currentMode stays SLACK because count never reaches 10

    Actual flood:
    Water rises steadily → every reading says TAUT
    count = 1, 2, 3, 4, 5, 6, 7, 8, 9, 10 → NOW currentMode = TAUT
    Result: only commits after 10 CONSECUTIVE readings agree

    Exception: SUBMERGED only needs 3 consecutive readings (urgent)
```
