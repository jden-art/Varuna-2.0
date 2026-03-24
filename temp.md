// WRONG (what I wrote):

#define OTA_BOOT_PIN   8   // ❌ This is not the BOOT pin on C3

#define OTA_RESET_PIN  10

// CORRECT:

#define OTA_BOOT_PIN   9   // ✅ GPIO9 = BOOT pin on ESP32-C3

#define OTA_RESET_PIN  10  // ✅ EN = reset



Physical wireFromToNormal useDuring OTAWire 1C3 GPIO2S3 GPIO14CSV RX @ 9600Bootloader RX @ 115200Wire 2C3 GPIO3S3 GPIO44$CFG TX @ 9600Bootloader TX @ 115200Wire 3C3 GPIO8S3 GPIO0HIGH (idle)LOW = enter download modeWire 4C3 GPIO10S3 ENHIGH (idle)Pulse LOW = resetWire 5C3 GPIO20SIM800L TXDUART0 RXUntouchedWire 6C3 GPIO21SIM800L RXDUART0 TXUntouched




## What the S3 is

The S3 is the **primary brain** of the VARUNA buoy. It is sealed inside a waterproof capsule deployed in the river. Every piece of intelligence — flood detection, sensor fusion, alert classification — runs entirely on this chip with no cloud dependency whatsoever. If the internet goes down, the S3 keeps monitoring and alerting independently.

---

## Physical sensing

The S3 directly drives all sensors through two I2C buses and one hardware UART.

**MPU6050 — 6-axis IMU (I2C bus 0, GPIO 8/9)**

This is the core measurement sensor. The buoy floats on the water surface, tethered to the riverbed anchor by a rope of known length. As the water rises, the buoy rises, the tether pulls taut, and the buoy tilts. The MPU6050 measures that tilt. The S3 reads raw 16-bit accelerometer and gyroscope values at 200Hz, converts them to physical units, and runs a complementary filter that fuses gyroscope integration with accelerometer correction to produce a stable tilt angle θ.

Water height H is then computed geometrically: H = L × cos(θ), where L is the tether length. This is the fundamental measurement from which everything else derives.

**BMP280 — Barometric pressure and temperature (I2C bus 1, GPIO 4/5)**

Serves two purposes. At deployment, the S3 samples atmospheric pressure 50 times and stores the average as the baseline. During operation, any positive gauge pressure above that baseline indicates the sensor is submerged — water is pressing down on it. This enables the S3 to detect and quantify submersion depth even when the tilt angle saturates. Temperature is also streamed as environmental context.

**DS1307 — Real-time clock (I2C bus 1, GPIO 4/5, same bus as BMP280)**

Provides accurate timestamps for all sensor readings, diagnostic reports, and CSV output. The RTC retains time through power cycles via its coin cell backup. The S3 reads from it at every transmit cycle and includes the formatted datetime string in all output streams.

**GPS module — NEO series (UART1, GPIO 6/7, 9600 baud)**

Provides GPS coordinates, altitude, satellite count, and UTC time. The S3 parses NMEA sentences (GGA and RMC) from the GPS module continuously in the main loop. GPS coordinates are embedded in every data frame so each reading is geolocated. If the RTC is unavailable, GPS time is used as the timestamp source.

**Battery ADC (GPIO 2)**

16-sample averaged ADC reading through a voltage divider converts the LiPo cell voltage to a percentage. Battery voltage and percentage are included in every output frame so the website and debugger can monitor power health remotely.

---

## Sensor fusion — the complementary filter

Raw gyroscope data drifts over time. Raw accelerometer data is noisy. The S3 runs a complementary filter that combines both:

```
filteredAngle = 0.98 × (filteredAngle + gyro_dps × dt)
              + 0.02 × accelDerivedAngle
```

This runs at 100Hz (every 10ms) and produces a stable, drift-free tilt estimate. Before deployment, the S3 runs a calibration routine that samples 1000 gyroscope readings at rest to compute offset corrections, and 500 accelerometer readings to establish the gravity reference direction. All subsequent readings are corrected against these baselines.

---

## The flood classification algorithm

The S3 classifies the physical state into four modes using the sensor fusion output plus pressure data.

**MODE_SLACK (0):** Lateral acceleration below threshold OR tilt below threshold. The tether is slack — water level is below the tether length. No flood risk.

**MODE_TAUT (1):** Lateral acceleration above threshold AND tilt above 3°. The tether is pulling taut — water is approaching the tether length. Rising risk.

**MODE_FLOOD (2):** Tilt approaches zero (buoy nearly vertical, tether fully taut) AND flood ratio > 0.95. Water has reached the tether anchor depth. Active flood condition.

**MODE_SUBMERGED (3):** Gauge pressure above 500 Pa threshold. The entire buoy capsule is underwater. Severe flood, maximum alert.

A **persistence counter** prevents spurious mode transitions — the S3 requires 10 consecutive readings agreeing on a mode before committing the transition. For submersion it requires only 3 due to urgency.

---

## Dynamic sampling

Rather than sampling at a fixed rate, the S3 adjusts its own transmit interval based on how close the water level is to the flood threshold.

Below 50% of H-max: transmit every `normalRateSec` (default 900 seconds, 15 minutes). Below 80%: linearly interpolate between normal and high rate. Above 80%: transmit every `highRateSec` (default 60 seconds). This conserves battery during normal conditions while providing dense data during approaching floods.

---

## Dual output streams

The S3 simultaneously outputs two completely different data streams every transmit cycle.

**Stream 1 → USB Serial (115200 baud) → handheld debugger**

The 39-field CSV containing all sensor values. This is what the RPi handheld debugger reads when physically connected on-site. Also outputs `STATUS:`, `ERROR:`, `WARNING:` prefixed lines that the debugger's boot checker and status monitor consume.

**Stream 2 → GPIO 14 (software UART, 9600 baud) → C3**

The same 39-field CSV format, bit-banged out at 9600 baud. This reaches the C3 which forwards it to Firebase via WiFi . The S3 firmware implements a software UART transmitter in the `c3UartSendByte()` function — it disables interrupts, drives GPIO 14 manually at precise 104µs bit intervals, then re-enables interrupts.

---

## Command reception

The S3 listens for incoming commands on two separate channels simultaneously in the main loop.

**From the C3 (Serial2, GPIO 43/44, 9600 baud):** Receives `$CFG,normal_sec,high_sec,h_max_cm` frames that update the sampling rate parameters. Also receives `$DIAGRUN` which triggers a manual diagnostic. Responds to `$PING` with `$PONG` for diagnostic connectivity verification.

**From the debugger (USB Serial, 115200 baud):** Receives plain-text commands — `PING`, `GETCONFIG`, `RECALIBRATE`, `GETTHRESH`, `SETTHRESH=A,W,D`, `RESETTHRESH`, `SETAPN`, `REINITSIM`, `TESTGPRS`. Each command produces a `STATUS:` prefixed response that the debugger's serial commander parser can confirm.

---

## Diagnostic subsystem

Every 24 hours (or on demand via `$DIAGRUN` from C3 or the website), the S3 runs a full sensor health check. It tests each sensor independently — MPU6050 WHO_AM_I register, accelerometer magnitude sanity, gyro drift measurement, BMP280 chip ID, pressure and temperature range validity, RTC oscillator running bit, RTC time validity, GPS UART activity, battery voltage range. It counts total faults.

It then sends `$PING` to the C3 via Serial2 and waits up to 2 seconds for `$PONG`. This tests the S3↔C3 communication link as part of the diagnostic. Once the pong arrives (or times out), the S3 builds a `$DIAG,` frame containing all results and sends it to the C3. The C3 forwards it to the server which stores it in Firebase. The website displays it in the diagnostic viewer panel.

---

## Health score

At every transmit cycle, the S3 computes a single `healthScore` integer from 0–100 using weighted sensor status:

- MPU6050 online: 40 points (most critical — without it there is no water measurement)
- BMP280 online: 30 points (pressure/depth/submersion detection)
- RTC online: 20 points (timestamp accuracy)
- GPS online: 10 points (location data)

This is included as field 39 of the CSV. The website displays it as a percentage gauge in the device config panel.

---

## What the S3 does NOT do

It never connects to WiFi. It never speaks HTTP. It never touches Firebase. I. It never knows whether the C3 is alive or dead. It never knows whether the website is online. It just measures, classifies, and shouts data out both serial ports every transmit cycle, completely independently of whether anyone is listening.

This is intentional. If the C3 fails completely, the S3 continues monitoring. If deployed in a location with no connectivity at all, the S3 still classifies the flood state correctly on-device. The flood detection is a fully autonomous edge function that requires no cloud infrastructure to operate.
