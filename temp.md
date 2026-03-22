```

The current algorithm is too heavy for the buoy. The buoy is running a 3D decision matrix, sustained rise detection, step-down hysteresis, time gates, cold start logic, and ACK tracking — all of that belongs on the server. The buoy's CPU should not be doing flood intelligence. It should be doing one thing: **measuring accurately and transmitting urgently based on how alarming the raw numbers look.**

Here is the principle: **the buoy does not decide the flood level. It decides the transmission urgency.**

---

## What stays on the buoy

Only three things:

**1. Raw measurement** — water height, rate of change, battery, GPS, sensor health. No interpretation beyond the physics.

**2. Transmission urgency** — a simple 4-level escalation matrix that answers only one question: *how fast must I send this right now?* The buoy has no memory of previous states, no hysteresis, no time gates. It looks at the current reading and current rate, picks a transmission interval, and sends.

**3. Immediate trigger transmits** — certain conditions bypass the interval entirely and transmit right now regardless of when the last transmit was.

---

## The Escalation Matrix — Buoy Side OnlyGood. Now let me build the complete answer with the diagram.

The key insight is this: the current code treats the buoy as a **decision maker**. It decides what level of emergency exists, holds state, manages hysteresis, and dispatches alerts. That is wrong. The buoy is a **sensor node**. Its only intelligence should be: *given what I am reading right now, how urgently must I transmit?*

The server does everything else — pattern recognition, sustained rise, hysteresis, de-escalation, alert routing, cooldowns.

Here is the buoy-side escalation matrix:Now the full explanation of the logic and why each decision was made this way.

---

## The core principle

The buoy has exactly two variables it looks at to decide its transmit tier: **current water height** relative to the three thresholds, and **current rate of change** per 15 minutes. That is it. No memory of previous tiers, no step-down counters, no time gates. Every reading is evaluated fresh. The tier is stateless on the buoy.

The rate of change calculation stays on the buoy because it requires two consecutive readings and arithmetic — that is trivial computation the buoy must do anyway to know its own urgency. Everything beyond that — sustained rise detection, hysteresis, de-escalation logic, alert dispatch — lives on the server.

---

## Why the thresholds for each tier are OR conditions, not AND

In the current firmware the matrix requires both zone AND rate to be elevated before escalating. This is appropriate for a system that manages its own alerts and wants to avoid false alarms. But since the server now handles false alarm suppression, the buoy should be conservative in the opposite direction — it should transmit urgently whenever **either** condition is alarming, not only when both are. A fast rate in a normal zone is dangerous. A high level with a slow rate is dangerous. The buoy should not wait for both to be true simultaneously before increasing transmit frequency.

---

## The four transmit intervals and their reasoning

**30 minutes** for Tier 0 is a heartbeat. The server needs to know the device is alive and the river is stable. This is not flood monitoring interval — it is a watchdog interval.

**10 minutes** for Tier 1 gives the server enough data points to build a credible trend before anything dangerous happens. The alert threshold is the first sign of trouble. At 10-minute intervals over the course of a rising river, the server gets 3-4 readings before the warning threshold is reached — enough to establish whether the rise is genuine or a transient.

**2 minutes** for Tier 2 is where flood intelligence on the server side becomes meaningful. At 2-minute intervals the server can calculate its own rate of change, detect sustained rise across multiple readings, and make confident escalation decisions. If the rate is 5 cm/15 min, the buoy crosses 15 cm of rise in 45 minutes — at 2-minute intervals the server sees 22 readings in that window.

**30 seconds** for Tier 3 is the absolute maximum urgency the SIM800L can sustain without overloading the GPRS session. At danger threshold with an extreme rate, the situation can become catastrophic within minutes. 30-second transmits give the server near-real-time data and allow it to escalate every tier of authorities without waiting.

---

## The immediate triggers — why each one exists

**Tier jump** — when a single reading pushes the buoy from Tier 0 to Tier 2 without passing through Tier 1, the server would not know about it until the next scheduled transmit on the new interval. Transmitting immediately on any upward jump ensures the server sees the step-change in the same reading that caused it.

**Rate spike** — a jump of more than 15 cm/15min in a single step is physically unusual. It suggests either a dam opening upstream, a levee breach, or a flash flood front arriving. This is the most time-critical scenario and must reach the server immediately regardless of which tier the height puts the buoy in.

**Submersion** — when the BMP280 confirms the buoy is going underwater, the tilt-based measurement has already become unreliable. The buoy needs to transmit its last valid reading and its submersion state before the data quality degrades further. After this point the pressure-based depth calculation takes over, and those readings must also arrive at the server in near real time.

**Sensor fault** — if the MPU6050 fails and the HC-SR04 takes over, the server must know immediately. A reading gap followed by resumed data on a different sensor looks like a data anomaly to the server's algorithm. An explicit sensor fault transmission tells the server to update its model of the device's data quality.

**Battery critical** — when battery drops below 10% the buoy may shut down within hours. The server must receive a final valid state packet so it knows the last confirmed water level before the device goes silent. Without this the server cannot distinguish between "device went offline because the flood destroyed it" and "battery died during a calm period."

**Post-sleep wake** — after any light sleep period, the first reading must transmit immediately rather than waiting for the next interval. The sleep may have lasted 25 minutes. Conditions could have changed significantly. The server should not be extrapolating across a 25-minute gap when it can have a fresh reading the moment the buoy wakes.

---

## What the buoy's payload must always contain

Since the server is doing all the intelligence now, the payload from every transmit must be complete. The server cannot ask follow-up questions. Every POST must include: water height, rate of change, current tier (so the server knows why this transmit happened), battery percentage, GPS coordinates, active sensor flag (MPU6050 or HC-SR04), submersion state, pressure deviation, timestamp, and a sensor health byte that encodes the status of all sensors in a single field. The server stores all of this and runs its own flood detection algorithm across the incoming stream.

---

## What gets removed from the firmware entirely

The `sustainedBuffer` and all associated logic. The `lookupDecisionMatrix()` function. The `evaluateStepDown()` function. The `canStepDown()` function. All the step-down consecutive reading counters and time gates. The `RESP_WATCH`, `RESP_WARNING`, `RESP_FLOOD`, `RESP_CRITICAL` response levels as internal state — the buoy no longer tracks its own response level. The `currentZone` classification. The `floodAlertLevel` variable. The cold-start level detection logic. All the `dispatchAlerts()`, `dispatchDeescalation()`, and `dispatchAllClear()` functions. The ACK system entirely.

What replaces all of that is one function: `classifyTransmitTier(waterHeight, ratePer15Min)` that returns 0, 1, 2, or 3, and one function: `shouldTransmitImmediately(previousTier, currentTier, ratePrevious, rateCurrent, submersionState, sensorChanged, batteryPercent)` that returns true or false. These two functions are the entire flood intelligence remaining on the buoy. Everything else moves to the server.
```
## 37. Serial CSV Output Format — 38 Fields

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
37     debugEnabled (0/1)         0
38     algorithmEnabled (0/1)     0
