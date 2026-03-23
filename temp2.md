

## What the handheld debugger is

It is a **PySide6 Python application** running on a **Raspberry Pi** with a touchscreen. It connects to the S3 via USB serial at 115200 baud. It is used **on-site before deployment** to calibrate, test, and certify the VARUNA node. It is a completely separate system from the website.

---

## What the debugger sends and receives — the critical difference

**The debugger receives a completely different CSV format from the S3.**

The debugger's `CSV_FIELD_NAMES` has **38 fields** in this order:

```
theta, waterHeight, correctedTiltX, correctedTiltY,
olpLength, horizontalDist, currentPressure, currentTemperature,
baselinePressure, pressureDeviation, submersionState, estimatedDepth,
bmpAvailable, unixTime, dateTimeString, rtcValid,
ratePer15Min, floodAlertLevel, sessionDuration, peakHeight,
minHeight, latitude, longitude, altitude,
gpsSatellites, gpsFixValid, simSignalRSSI, simRegistered,
simAvailable, currentZone, currentResponseLevel, sustainedRise,
batteryPercent, sampleInterval, transmitInterval, obLightEnabled,
debugEnabled, algorithmEnabled
```

**This is the OLD S3 CSV format sent over USB Serial (Serial0) at 115200 baud.**

**Our NEW 39-field CSV** is sent over GPIO14 (SW-UART, 9600 baud) to the C3. These are two completely separate UART channels with different field orders and different field counts.

---

## The two channels now on the S3

```
USB Serial (Serial0, 115200)  → Debugger (RPi touchscreen)
  Format: 38-field CSV, old field order
  Also: STATUS:, ERROR:, WARNING:, FLOOD: prefixed lines
  Commands received: PING, GETCONFIG, RECALIBRATE, GETTHRESH,
                     SETTHRESH=A,W,D, RESETTHRESH, SETAPN=,
                     REINITSIM, TESTGPRS

GPIO14 (SW-UART, 9600)        → C3 (Firebase/server path)
  Format: 39-field CSV, new field order
  One-way TX only from S3
```

These do not conflict. The S3 currently already prints to `Serial.println()` for the debugger AND separately to `buildAndTransmitCSVFrame()` for the C3. They use separate pins.

---

## What the debugger's commands do — and whether they still work

| Command | What it does | Still works? |
|---|---|---|
| `PING` | S3 replies `STATUS:PONG` | Yes — S3 processes serial commands |
| `GETCONFIG` | S3 replies with calibration data | Yes — `$DBG,GET_STATUS` equivalent |
| `RECALIBRATE` | Triggers full sensor recalibration | Yes — `$DBG,RECAL` |
| `GETTHRESH` | Returns current threshold values | **Missing from new S3** — needs to be added |
| `SETTHRESH=A,W,D` | Sets alert/warning/danger thresholds | **Missing from new S3** |
| `RESETTHRESH` | Resets thresholds to defaults | **Missing from new S3** |
| `SETAPN=<apn>` | Sets GPRS APN | Irrelevant — C3 handles GPRS now |
| `REINITSIM` | Reinitialises SIM800L | Irrelevant — C3 handles SIM |
| `TESTGPRS` | Tests GPRS connection | Irrelevant — C3 handles GPRS |

---

## The critical problem: field order mismatch

The **old debugger CSV** has `theta` as field 1 and `waterHeight` as field 2.

The **new C3 CSV** has `waterHeight` as field 1 and `tiltX` as field 2.

The S3 currently sends **two separate CSV lines** — one per channel. The `buildAndTransmitDataFrame()` (old format) goes to `Serial2` (the C3 UART). The `buildAndTransmitCSVFrame()` (new 39-field format) goes to `Serial` (USB, the debugger) AND via GPIO14 to the C3.

Wait — that is the bug. Currently `buildAndTransmitCSVFrame()` sends to the C3 via GPIO14. But the debugger also reads from USB Serial. What format does USB Serial currently output?

Looking at the S3 loop:

```cpp
buildAndTransmitDataFrame();    // Serial2 $DATA frame (4 fields) → C3 old
buildAndTransmitCSVFrame();     // GPIO14 39-field CSV → C3 new
// Serial.println() in loop also prints a debug line
```

The USB serial (`Serial`) currently outputs the **debug print line** like `[TAUT] H=45.2cm θ=12.3° ...` — not a CSV at all. The debugger expects a CSV with 38 specific fields.

**This means the debugger is currently broken with the new S3 firmware.** The S3 needs to output the **old 38-field CSV format** on USB Serial for the debugger, while outputting the **new 39-field CSV format** on GPIO14 for the C3.

---

## The complete fix needed

### S3 needs to output two different CSV formats simultaneously

**USB Serial (Serial0) → debugger:** old 38-field format at 115200 baud
**GPIO14 (SW-UART) → C3:** new 39-field format at 9600 baud

Add this function to the S3 that outputs the old format the debugger expects:

```cpp
// ============================================================================
// DEBUGGER CSV — old 38-field format sent via USB Serial
// This is what the handheld RPi debugger reads
// Field order matches CSV_FIELD_NAMES in serial_worker.py exactly
// ============================================================================
void buildAndTransmitDebuggerCSV() {
    // Compute session duration
    unsigned long sessionSec = (millis() - bootMillis) / 1000;

    // Peak and min height tracking
    static float peakHeight  = 0.0f;
    static float minHeight   = 9999.0f;
    float wh_cm = (waterLevel.waterLevel_m < 0.0) ? 0.0f : (float)(waterLevel.waterLevel_m * 100.0);
    if (wh_cm > peakHeight) peakHeight = wh_cm;
    if (wh_cm < minHeight && wh_cm > 0.0f) minHeight = wh_cm;
    if (minHeight == 9999.0f) minHeight = 0.0f;

    // Rate per 15 min — simple rolling approximation
    // (debugger computes this from history, we just send 0 unless computed)
    float rate15 = 0.0f;

    // DateTime string
    char dtStr[22];
    if (rtcTime.valid) {
        snprintf(dtStr, sizeof(dtStr), "20%02d-%02d-%02d %02d:%02d:%02d",
            rtcTime.year, rtcTime.month, rtcTime.date,
            rtcTime.hours, rtcTime.minutes, rtcTime.seconds);
    } else if (gpsData.fix_valid) {
        snprintf(dtStr, sizeof(dtStr), "20%02d-%02d-%02d %02d:%02d:%02d",
            gpsData.year, gpsData.month, gpsData.day,
            gpsData.hour, gpsData.minute, gpsData.second);
    } else {
        snprintf(dtStr, sizeof(dtStr), "1970-01-01 00:00:00");
    }

    // Unix time approximate
    uint32_t unixTs = 0;
    if (rtcTime.valid) {
        uint32_t days = (uint32_t)rtcTime.year * 365
                      + (uint32_t)rtcTime.month * 30
                      + rtcTime.date;
        unixTs = 946684800UL + days * 86400UL
               + rtcTime.hours * 3600UL
               + rtcTime.minutes * 60UL
               + rtcTime.seconds;
    }

    float press_hPa = pressureData.valid
        ? (float)(pressureData.pressure_Pa / 100.0) : 0.0f;
    float atm_hPa   = (float)(atmosphericPressureBaseline_Pa / 100.0);
    float pressDeviation = press_hPa - atm_hPa;

    // currentZone: map mode to zone (0=NORMAL,1=ALERT,2=WARNING,3=DANGER)
    int zone = modeToAlertLevel(currentMode);

    Serial.print((float)filteredTilt.correctedCombined_deg, 2);  Serial.print(",");  // theta
    Serial.print(wh_cm, 2);                                       Serial.print(",");  // waterHeight
    Serial.print((float)filteredTilt.correctedTiltX_deg, 2);      Serial.print(",");  // correctedTiltX
    Serial.print((float)filteredTilt.correctedTiltY_deg, 2);      Serial.print(",");  // correctedTiltY
    Serial.print((float)(tetherLength_m * 100.0), 2);             Serial.print(",");  // olpLength (in cm)
    Serial.print((float)(waterLevel.waterLevel_m * 100.0), 2);    Serial.print(",");  // horizontalDist (approx H)
    Serial.print(press_hPa, 2);                                   Serial.print(",");  // currentPressure
    Serial.print(pressureData.valid ? (float)pressureData.temperature_C : 0.0f, 2); Serial.print(","); // currentTemperature
    Serial.print(atm_hPa, 2);                                     Serial.print(",");  // baselinePressure
    Serial.print(pressDeviation, 2);                              Serial.print(",");  // pressureDeviation
    Serial.print((int)currentMode);                               Serial.print(",");  // submersionState
    Serial.print((float)waterLevel.submersionDepth_m, 2);         Serial.print(",");  // estimatedDepth
    Serial.print(sysStatus.bmpOnline ? 1 : 0);                    Serial.print(",");  // bmpAvailable
    Serial.print((unsigned long)unixTs);                          Serial.print(",");  // unixTime
    Serial.print(dtStr);                                          Serial.print(",");  // dateTimeString
    Serial.print(rtcTime.valid ? 1 : 0);                          Serial.print(",");  // rtcValid
    Serial.print(rate15, 2);                                      Serial.print(",");  // ratePer15Min
    Serial.print(zone);                                           Serial.print(",");  // floodAlertLevel
    Serial.print((unsigned long)sessionSec);                      Serial.print(",");  // sessionDuration
    Serial.print(peakHeight, 2);                                  Serial.print(",");  // peakHeight
    Serial.print(minHeight, 2);                                   Serial.print(",");  // minHeight
    Serial.print((float)gpsData.latitude, 6);                     Serial.print(",");  // latitude
    Serial.print((float)gpsData.longitude, 6);                    Serial.print(",");  // longitude
    Serial.print((float)gpsData.altitude_m, 2);                   Serial.print(",");  // altitude
    Serial.print(gpsData.satellites);                             Serial.print(",");  // gpsSatellites
    Serial.print(gpsData.fix_valid ? 1 : 0);                      Serial.print(",");  // gpsFixValid
    Serial.print(0);                                              Serial.print(",");  // simSignalRSSI (C3 handles SIM)
    Serial.print(0);                                              Serial.print(",");  // simRegistered
    Serial.print(0);                                              Serial.print(",");  // simAvailable
    Serial.print(zone);                                           Serial.print(",");  // currentZone
    Serial.print(zone);                                           Serial.print(",");  // currentResponseLevel
    Serial.print(0);                                              Serial.print(",");  // sustainedRise
    Serial.print((float)sysStatus.batteryPercent, 2);             Serial.print(",");  // batteryPercent
    Serial.print((unsigned long)(activeSampleMs / 1000UL));        Serial.print(",");  // sampleInterval
    Serial.print((unsigned long)normalRateSec);                   Serial.print(",");  // transmitInterval
    Serial.print(0);                                              Serial.print(",");  // obLightEnabled
    Serial.print(0);                                              Serial.print(",");  // debugEnabled
    Serial.println(sysStatus.algorithmEnabled ? 1 : 0);                               // algorithmEnabled
}
```

Then in `loop()`, replace `buildAndTransmitDataFrame()` and `buildAndTransmitCSVFrame()` calls with:

```cpp
if (now - lastTransmitMillis >= activeSampleMs) {
    lastTransmitMillis = now;

    if (sysStatus.rtcOnline) rtcRead(rtcTime);
    checkSensorHealth();

    // USB Serial → handheld debugger (old 38-field format)
    buildAndTransmitDebuggerCSV();

    // GPIO14 SW-UART → C3 → server → Firebase (new 39-field format)
    buildAndTransmitCSVFrame();
}
```

The old `buildAndTransmitDataFrame()` writing `$DATA` to Serial2 can now be removed entirely — the C3 no longer reads `$DATA` frames, it reads the 39-field CSV from GPIO14.

---

## What the debugger commands need on the S3

The S3's `processC3Command()` currently handles `$CFG`, `$DBG`, `$PING`, `$DIAGRUN` coming from the C3 side on GPIO44/Serial2. But the debugger sends commands on USB Serial (`Serial0`).

The S3 needs a **separate command reader for USB Serial** that handles the debugger's command set. Looking at what's currently in the S3, the serial debug interface via USB probably already exists in some form. Add this to the S3:

```cpp
// ============================================================================
// DEBUGGER COMMAND HANDLER — USB Serial (Serial0) from handheld RPi
// Commands from SerialCommander in the debugger app
// ============================================================================

char dbgCmdBuf[64];
uint8_t dbgCmdIdx = 0;

void readDebuggerCommands() {
    while (Serial.available()) {
        char c = Serial.read();
        if (c == '\n' || c == '\r') {
            if (dbgCmdIdx > 0) {
                dbgCmdBuf[dbgCmdIdx] = '\0';
                processDebuggerCommand(dbgCmdBuf);
                dbgCmdIdx = 0;
            }
        } else {
            if (dbgCmdIdx < sizeof(dbgCmdBuf) - 1) {
                dbgCmdBuf[dbgCmdIdx++] = c;
            }
        }
    }
}

void processDebuggerCommand(const char *cmd) {
    String c = String(cmd);
    c.trim();
    String cu = c;
    cu.toUpperCase();

    // PING
    if (cu == "PING") {
        Serial.println("STATUS:PONG");
        return;
    }

    // GETCONFIG — returns calibration data in format debugger expects
    if (cu == "GETCONFIG") {
        char resp[256];
        snprintf(resp, sizeof(resp),
            "STATUS:CONFIG,WHO_AM_I=%s,TOTAL_G=%.3f,"
            "GYRO_X=%d,GYRO_Y=%d,GYRO_Z=%d,"
            "GYRO_SAMPLES=%d,ACCEL_SAMPLES=%d",
            sysStatus.mpuOnline ? "0x68" : "NONE",
            (float)(accelRef.valid ? sqrt(accelRef.ax*accelRef.ax + accelRef.ay*accelRef.ay + accelRef.az*accelRef.az) : 0.0),
            (int)gyroOffset.x,
            (int)gyroOffset.y,
            (int)gyroOffset.z,
            CALIBRATION_GYRO_SAMPLES,
            CALIBRATION_ACCEL_SAMPLES
        );
        Serial.println(resp);
        return;
    }

    // RECALIBRATE — full sensor recalibration
    if (cu == "RECALIBRATE") {
        Serial.println("STATUS:CALIBRATING");
        recalibrate();
        Serial.println("STATUS:CALIBRATE_DONE");
        return;
    }

    // GETTHRESH — return current threshold values
    // These map to hMaxCm (danger), and derived alert/warning levels
    if (cu == "GETTHRESH") {
        char resp[96];
        // alert = 50% of hMaxCm, warning = 80% of hMaxCm, danger = hMaxCm
        float alert   = hMaxCm * 0.50f;
        float warning = hMaxCm * 0.80f;
        float danger  = hMaxCm;
        snprintf(resp, sizeof(resp),
            "STATUS:THRESH,ALERT=%.1f,WARNING=%.1f,DANGER=%.1f",
            alert, warning, danger);
        Serial.println(resp);
        return;
    }

    // SETTHRESH=alert,warning,danger
    if (cu.startsWith("SETTHRESH=")) {
        String payload = c.substring(10);
        float a = 0, w = 0, d = 0;
        int c1 = payload.indexOf(',');
        int c2 = payload.lastIndexOf(',');
        if (c1 > 0 && c2 > c1) {
            a = payload.substring(0, c1).toFloat();
            w = payload.substring(c1 + 1, c2).toFloat();
            d = payload.substring(c2 + 1).toFloat();
        }
        if (d > 10.0f && d < 2000.0f) {
            // Danger level IS H_max — set it
            hMaxCm = d;
            updateSamplingInterval();
            char resp[64];
            snprintf(resp, sizeof(resp),
                "STATUS:THRESH_OK,ALERT=%.1f,WARNING=%.1f,DANGER=%.1f",
                a, w, d);
            Serial.println(resp);
        } else {
            Serial.println("WARNING:THRESH_INVALID");
        }
        return;
    }

    // RESETTHRESH — reset to defaults
    if (cu == "RESETTHRESH") {
        hMaxCm = 200.0f;
        normalRateSec = 900;
        highRateSec   = 60;
        updateSamplingInterval();
        Serial.println("STATUS:THRESH_RESET_OK");
        return;
    }

    // SETAPN — not handled by S3 anymore (C3 owns SIM)
    if (cu.startsWith("SETAPN=")) {
        Serial.println("STATUS:APN_IGNORED_C3_HANDLES_SIM");
        return;
    }

    // REINITSIM — not handled by S3 anymore
    if (cu == "REINITSIM") {
        Serial.println("STATUS:SIM_REINIT_IGNORED_C3_HANDLES_SIM");
        return;
    }

    // TESTGPRS — not handled by S3 anymore
    if (cu == "TESTGPRS") {
        Serial.println("STATUS:GPRS_TEST_IGNORED_C3_HANDLES_COMMS");
        return;
    }

    // Unknown
    Serial.print("WARNING:UNKNOWN_CMD=");
    Serial.println(cmd);
}
```

---

## What this means for the debugger — field mapping issues

The debugger's `serial_worker.py` parses 38 fields in the old order. The S3 now outputs the `buildAndTransmitDebuggerCSV()` function which outputs exactly those 38 fields in exactly that order. **The debugger needs zero changes.**

The three SIM-related commands (`SETAPN`, `REINITSIM`, `TESTGPRS`) now return harmless acknowledgements instead of actually doing anything, because the C3 owns the SIM800L. The debugger's Connectivity screen will show the commands as "confirmed" — which is acceptable since the connectivity test is about whether the C3+SIM path works, not the S3 directly.

---

## Summary of all changes needed

**S3 firmware — 3 additions:**
1. Add `buildAndTransmitDebuggerCSV()` — outputs old 38-field format to USB Serial
2. Add `readDebuggerCommands()` and `processDebuggerCommand()` — handles all 9 debugger commands
3. Update `loop()` — call both CSV functions, call `readDebuggerCommands()`, remove old `buildAndTransmitDataFrame()`

**C3 firmware — no changes needed.** It reads GPIO14 which gets the new 39-field format. The debugger's USB connection is entirely separate.

**Debugger app — no changes needed.** It reads the same 38-field CSV format from USB Serial that it always expected. The S3 now regenerates that format explicitly.

**Website — no changes needed.** It reads from Firebase which gets the new 39-field format via C3.

The two tools — debugger and website — use completely separate data paths and never interfere.
