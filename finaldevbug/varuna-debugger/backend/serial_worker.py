# -----------------------------------------------------------------
# File: backend/serial_worker.py
# Phase: Phase 5 Step 3 — Updated field protocol (39 fields)
# -----------------------------------------------------------------

import time
import math
import random
import os
from datetime import datetime, timezone, timedelta

from PySide6.QtCore import QObject, Signal, Slot


# ── Change 1: Replace CSV_FIELD_NAMES (38 → 39 fields) ──────────
CSV_FIELD_NAMES = [
    "waterHeight_cm", "tiltX_deg", "tiltY_deg", "tiltMag_deg", "lateralAccel_g",
    "temperature_C", "pressure_hPa", "bmpTemp_C", "atmosphericRef_hPa", "gaugePressure_Pa",
    "mode", "subDepth_m", "floodRatio", "timestamp_unix", "rtcString",
    "gpsFix", "tetherLength_m", "alertLevel", "activeSampleSec", "battery_pct",
    "battVoltage_V", "latitude", "longitude", "altitude_m", "satellites",
    "gpsFix2", "satellites2", "mpuOnline", "bmpOnline", "rtcOnline",
    "gpsOnline", "algoEnabled", "normalRateSec", "highRateSec", "hMaxCm",
    "floodZone", "realtimeMode", "uptimeSec", "healthScore",
]

# ── Change 2: Automatic ─────────────────────────────────────────
CSV_FIELD_COUNT = len(CSV_FIELD_NAMES)


class SerialWorker(QObject):
    csvReceived = Signal(dict)
    statusReceived = Signal(str, str)
    connectionChanged = Signal(bool)
    rawLineReceived = Signal(str)

    def __init__(self, settings_manager, parent=None):
        super().__init__(parent)
        self._settings = settings_manager
        self._running = False
        self._connected = False
        self._serial_port = None
        self._simulated = False
        self._sim_tick = 0
        self._log_file = None
        self._log_file_path = ""

        self._boot_messages_sent = False
        self._boot_message_index = 0
        self._boot_messages = [
            ("STATUS", "MPU6050_OK"),
            ("STATUS", "BMP280_OK"),
            ("STATUS", "DS1307_OK"),
            ("STATUS", "HCSR04_OK"),
            ("STATUS", "SIM_READY"),
            ("STATUS", "GPS_UART_INIT"),
            ("STATUS", "CALIBRATING"),
            ("STATUS", "CALIBRATING_GYRO"),
            ("STATUS", "CALIBRATING_REFERENCE"),
            ("STATUS", "BASELINE_INIT=1013.25"),
        ]

        self._csv_parse_count = 0

    @Slot()
    def start(self):
        self._running = True
        print("SerialWorker: Starting...")
        port = self._try_open_serial()
        if port is not None:
            self._serial_port = port
            self._simulated = False
            self._set_connected(True)
            print(f"SerialWorker: Connected to {port.port}")
            self._read_loop_serial()
        else:
            print("SerialWorker: No serial device found — starting simulated data mode.")
            self._simulated = True
            self._set_connected(True)
            self._read_loop_simulated()

    @Slot()
    def stop(self):
        print("SerialWorker: Stop requested.")
        self._running = False
        self._set_connected(False)
        self._close_log_file()
        if self._serial_port is not None:
            try:
                self._serial_port.close()
            except Exception:
                pass
            self._serial_port = None

    def _set_connected(self, value):
        if self._connected != value:
            self._connected = value
            self.connectionChanged.emit(value)
            print(f"SerialWorker: Connection state -> {value}")

    def _try_open_serial(self):
        override = self._settings.serialPortOverride
        if override != "auto":
            ports_to_try = [override]
        else:
            ports_to_try = ["/dev/ttyUSB0", "/dev/ttyACM0", "/dev/ttyS0"]

        print(f"SerialWorker: Scanning serial ports: {ports_to_try}")

        try:
            import serial
        except ImportError:
            print("SerialWorker: pyserial not installed. Cannot open serial ports.")
            return None

        for port_name in ports_to_try:
            if not os.path.exists(port_name):
                print(f"SerialWorker:   {port_name} — does not exist, skipping.")
                continue
            try:
                print(f"SerialWorker:   {port_name} — attempting to open...")
                ser = serial.Serial(
                    port=port_name,
                    baudrate=115200,
                    bytesize=serial.EIGHTBITS,
                    parity=serial.PARITY_NONE,
                    stopbits=serial.STOPBITS_ONE,
                    timeout=3.0,
                )
                line = ser.readline()
                if line and len(line.strip()) > 0:
                    print(f"SerialWorker:   {port_name} — data received, using this port.")
                    ser.timeout = 1.0
                    return ser
                else:
                    print(f"SerialWorker:   {port_name} — no data within 3s, closing.")
                    ser.close()
            except Exception as e:
                print(f"SerialWorker:   {port_name} — error: {e}")

        return None

    def _read_loop_serial(self):
        while self._running and self._serial_port is not None:
            try:
                raw_bytes = self._serial_port.readline()
                if not raw_bytes:
                    continue
                line = raw_bytes.decode("utf-8", errors="replace").strip()
                if not line:
                    continue
                self.rawLineReceived.emit(line)
                self._maybe_log_line(line)
                self._classify_and_emit(line)
            except Exception as e:
                print(f"SerialWorker: Serial read error: {e}")
                self._set_connected(False)
                time.sleep(1.0)
                port = self._try_open_serial()
                if port is not None:
                    self._serial_port = port
                    self._set_connected(True)
                elif not self._running:
                    break
                else:
                    time.sleep(2.0)

    def _read_loop_simulated(self):
        while self._running:
            if not self._boot_messages_sent:
                if self._boot_message_index < len(self._boot_messages):
                    prefix, message = self._boot_messages[self._boot_message_index]
                    line = f"{prefix}:{message}"
                    self.rawLineReceived.emit(line)
                    self._maybe_log_line(line)
                    self.statusReceived.emit(prefix, message)
                    print(f"SerialWorker [SIM]: {line}")
                    self._boot_message_index += 1
                    time.sleep(0.4)
                    continue
                else:
                    self._boot_messages_sent = True
                    print("SerialWorker [SIM]: Boot messages complete. Starting CSV stream.")

            csv_line = self._generate_simulated_csv()
            self.rawLineReceived.emit(csv_line)
            self._maybe_log_line(csv_line)
            self._parse_and_emit_csv(csv_line)

            if self._sim_tick % 7 == 0 and self._sim_tick > 0:
                status_line = self._generate_simulated_status()
                self.rawLineReceived.emit(status_line)
                self._maybe_log_line(status_line)
                prefix, message = status_line.split(":", 1)
                self.statusReceived.emit(prefix, message)
                print(f"SerialWorker [SIM]: {status_line}")

            self._sim_tick += 1
            time.sleep(1.0)

    # ── Change 3: No change needed — len check is automatic ──────

    def _classify_and_emit(self, line):
        for prefix in ("STATUS:", "ERROR:", "WARNING:", "FLOOD:"):
            if line.startswith(prefix):
                tag = prefix[:-1]
                message = line[len(prefix):]
                self.statusReceived.emit(tag, message)
                print(f"SerialWorker: [{tag}] {message}")
                return

        parts = line.split(",")
        if len(parts) == CSV_FIELD_COUNT:
            self._parse_and_emit_csv(line)
        else:
            print(f"SerialWorker: Unrecognized line ({len(parts)} fields): {line[:80]}...")

    # ── Change 4 & 5: Updated _parse_and_emit_csv ────────────────

    def _parse_and_emit_csv(self, line):
        parts = line.split(",")
        if len(parts) != CSV_FIELD_COUNT:
            print(f"SerialWorker: CSV field count mismatch: expected {CSV_FIELD_COUNT}, got {len(parts)}")
            return

        # --- type sets for the new 39-field protocol ---
        STRING_FIELDS = {
            "rtcString",
        }

        INT_FIELDS = {
            "mode", "timestamp_unix", "gpsFix", "alertLevel",
            "activeSampleSec", "satellites", "gpsFix2", "satellites2",
            "mpuOnline", "bmpOnline", "rtcOnline", "gpsOnline",
            "algoEnabled", "normalRateSec", "highRateSec",
            "floodZone", "realtimeMode", "uptimeSec",
        }

        # everything else is float

        data = {}
        for i, name in enumerate(CSV_FIELD_NAMES):
            raw = parts[i].strip()
            if name in STRING_FIELDS:
                data[name] = raw
            elif name in INT_FIELDS:
                try:
                    data[name] = int(raw)
                except ValueError:
                    data[name] = 0
            else:
                try:
                    data[name] = float(raw)
                except ValueError:
                    data[name] = 0.0

        self._csv_parse_count += 1

        # ── Change 5: Updated debug print ────────────────────────
        wh = data.get("waterHeight_cm", 0.0)
        tilt = data.get("tiltMag_deg", 0.0)
        bat = data.get("battery_pct", 0.0)
        bv = data.get("battVoltage_V", 0.0)
        pres = data.get("pressure_hPa", 0.0)
        temp = data.get("temperature_C", 0.0)
        sats = data.get("satellites", 0)
        gps_fix = data.get("gpsFix", 0)
        lat = data.get("latitude", 0.0)
        lon = data.get("longitude", 0.0)
        mode = data.get("mode", 0)
        alert = data.get("alertLevel", 0)
        health = data.get("healthScore", 0.0)
        uptime = data.get("uptimeSec", 0)
        dt_str = data.get("rtcString", "")

        print(
            f"SerialWorker: CSV #{self._csv_parse_count:04d} | "
            f"water={wh:.1f}cm tilt={tilt:.1f}° bat={bat:.1f}%({bv:.2f}V) "
            f"pres={pres:.1f}hPa temp={temp:.1f}°C "
            f"gps={'FIX' if gps_fix else 'NOFIX'}({sats}sats) "
            f"pos={lat:.6f},{lon:.6f} mode={mode} alert={alert} "
            f"health={health:.1f} uptime={uptime}s | {dt_str}"
        )

        self.csvReceived.emit(data)

    # ── Change 6: Rewritten _generate_simulated_csv (39 fields) ──

    def _generate_simulated_csv(self):
        t = self._sim_tick

        # --- sensor data ---
        water_height_cm = 45.0 + 8.0 * math.sin(t * 0.03) + random.uniform(-0.5, 0.5)
        tilt_x_deg = 2.1 + 1.5 * math.sin(t * 0.07) + random.uniform(-0.2, 0.2)
        tilt_y_deg = -0.8 + 1.0 * math.cos(t * 0.06) + random.uniform(-0.2, 0.2)
        tilt_mag_deg = math.sqrt(tilt_x_deg ** 2 + tilt_y_deg ** 2)
        lateral_accel_g = 0.02 + 0.01 * math.sin(t * 0.09) + random.uniform(-0.005, 0.005)

        temperature_c = 28.5 + 2.0 * math.sin(t * 0.01) + random.uniform(-0.3, 0.3)
        pressure_hpa = 1013.25 + 0.5 * math.sin(t * 0.02) + random.uniform(-0.1, 0.1)
        bmp_temp_c = temperature_c + random.uniform(-0.2, 0.2)
        atmospheric_ref_hpa = 1013.25
        gauge_pressure_pa = (pressure_hpa - atmospheric_ref_hpa) * 100.0

        # --- mode / depth / flood ---
        mode = 0
        sub_depth_m = 0.0
        flood_ratio = max(0.0, min(1.0, (water_height_cm - 40.0) / 60.0))

        # --- time ---
        now = datetime.now(timezone(timedelta(hours=5, minutes=30)))
        timestamp_unix = int(now.timestamp())
        rtc_string = now.strftime("%Y-%m-%d %H:%M:%S")

        # --- GPS ---
        gps_fix = 1
        tether_length_m = 1.20
        alert_level = 0
        active_sample_sec = 1

        # --- battery ---
        battery_pct = max(0.0, min(100.0, 85.0 - t * 0.02 + random.uniform(-0.5, 0.5)))
        batt_voltage_v = 3.3 + (battery_pct / 100.0) * 0.9 + random.uniform(-0.02, 0.02)

        # --- position ---
        lat = 19.076090 + random.uniform(-0.000005, 0.000005)
        lon = 72.877426 + random.uniform(-0.000005, 0.000005)
        alt_m = 14.2 + random.uniform(-0.5, 0.5)
        satellites = random.randint(6, 12)

        gps_fix2 = gps_fix
        satellites2 = satellites

        # --- hardware online flags ---
        mpu_online = 1
        bmp_online = 1
        rtc_online = 1
        gps_online = 1
        algo_enabled = 1

        # --- rates / flood ---
        normal_rate_sec = 10
        high_rate_sec = 1
        h_max_cm = max(45.0, water_height_cm)
        flood_zone = 0
        realtime_mode = 0

        # --- uptime / health ---
        uptime_sec = t
        health_score = max(0.0, min(100.0, 95.0 - t * 0.005 + random.uniform(-1.0, 1.0)))

        fields = [
            f"{water_height_cm:.2f}",
            f"{tilt_x_deg:.2f}",
            f"{tilt_y_deg:.2f}",
            f"{tilt_mag_deg:.2f}",
            f"{lateral_accel_g:.4f}",
            f"{temperature_c:.2f}",
            f"{pressure_hpa:.2f}",
            f"{bmp_temp_c:.2f}",
            f"{atmospheric_ref_hpa:.2f}",
            f"{gauge_pressure_pa:.2f}",
            str(mode),
            f"{sub_depth_m:.2f}",
            f"{flood_ratio:.4f}",
            str(timestamp_unix),
            rtc_string,
            str(gps_fix),
            f"{tether_length_m:.2f}",
            str(alert_level),
            str(active_sample_sec),
            f"{battery_pct:.2f}",
            f"{batt_voltage_v:.2f}",
            f"{lat:.6f}",
            f"{lon:.6f}",
            f"{alt_m:.2f}",
            str(satellites),
            str(gps_fix2),
            str(satellites2),
            str(mpu_online),
            str(bmp_online),
            str(rtc_online),
            str(gps_online),
            str(algo_enabled),
            str(normal_rate_sec),
            str(high_rate_sec),
            f"{h_max_cm:.2f}",
            str(flood_zone),
            str(realtime_mode),
            str(uptime_sec),
            f"{health_score:.2f}",
        ]

        return ",".join(fields)

    def _generate_simulated_status(self):
        choices = [
            "STATUS:GPRS_CONNECTED",
            "STATUS:GPRS_UPLOAD_OK",
            "STATUS:RECALIBRATED_ZERO",
            "WARNING:GPS_HDOP_HIGH",
            "STATUS:GPRS_HTTP_CODE=200",
        ]
        return random.choice(choices)

    def _maybe_log_line(self, line):
        if not self._settings.dataLoggingEnabled:
            if self._log_file is not None:
                self._close_log_file()
            return

        if self._log_file is None:
            self._open_log_file()

        if self._log_file is not None:
            try:
                self._log_file.write(line + "\n")
                self._log_file.flush()
            except OSError as e:
                print(f"SerialWorker: Log write error: {e}")
                self._close_log_file()

    def _open_log_file(self):
        log_dir = os.path.expanduser("/home/varuna/logs")
        if not os.path.isdir(os.path.dirname(log_dir)):
            log_dir = os.path.join(os.path.expanduser("~"), "varuna-logs")

        try:
            os.makedirs(log_dir, exist_ok=True)
            timestamp = datetime.now().strftime("%Y%m%d_%H%M%S")
            self._log_file_path = os.path.join(log_dir, f"serial_{timestamp}.log")
            self._log_file = open(self._log_file_path, "a")
            print(f"SerialWorker: Data logging started -> {self._log_file_path}")
        except OSError as e:
            print(f"SerialWorker: Could not open log file: {e}")
            self._log_file = None

    def _close_log_file(self):
        if self._log_file is not None:
            try:
                self._log_file.close()
                print(f"SerialWorker: Data logging stopped. File: {self._log_file_path}")
            except OSError:
                pass
            self._log_file = None
            self._log_file_path = ""

# -----------------------------------------------------------------
# File: backend/serial_worker.py
# Phase: Phase 5 Step 3 — Updated field protocol (39 fields)
# ----------------------------END----------------------------------
