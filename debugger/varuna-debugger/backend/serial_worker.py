# -----------------------------------------------------------------
# File: backend/serial_worker.py
# Phase: Phase 5 Step 3
# -----------------------------------------------------------------

import time
import math
import random
import os
from datetime import datetime, timezone, timedelta

from PySide6.QtCore import QObject, Signal, Slot


CSV_FIELD_NAMES = [
    "theta", "waterHeight", "correctedTiltX", "correctedTiltY",
    "olpLength", "horizontalDist", "currentPressure", "currentTemperature",
    "baselinePressure", "pressureDeviation", "submersionState", "estimatedDepth",
    "bmpAvailable", "unixTime", "dateTimeString", "rtcValid",
    "ratePer15Min", "floodAlertLevel", "sessionDuration", "peakHeight",
    "minHeight", "latitude", "longitude", "altitude",
    "gpsSatellites", "gpsFixValid", "simSignalRSSI", "simRegistered",
    "simAvailable", "currentZone", "currentResponseLevel", "sustainedRise",
    "batteryPercent", "sampleInterval", "transmitInterval", "obLightEnabled",
    "debugEnabled", "algorithmEnabled",
]

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

    def _parse_and_emit_csv(self, line):
        parts = line.split(",")
        if len(parts) != CSV_FIELD_COUNT:
            print(f"SerialWorker: CSV field count mismatch: expected {CSV_FIELD_COUNT}, got {len(parts)}")
            return

        data = {}
        for i, name in enumerate(CSV_FIELD_NAMES):
            raw = parts[i].strip()
            if name == "dateTimeString":
                data[name] = raw
            elif name in ("bmpAvailable", "rtcValid", "gpsFixValid", "simRegistered",
                          "simAvailable", "obLightEnabled", "debugEnabled", "algorithmEnabled",
                          "sustainedRise"):
                try:
                    data[name] = int(raw)
                except ValueError:
                    data[name] = 0
            elif name in ("unixTime", "submersionState", "floodAlertLevel", "sessionDuration",
                          "gpsSatellites", "simSignalRSSI", "currentZone", "currentResponseLevel",
                          "sampleInterval", "transmitInterval"):
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

        wh = data.get("waterHeight", 0.0)
        theta = data.get("theta", 0.0)
        bat = data.get("batteryPercent", 0.0)
        rssi = data.get("simSignalRSSI", 0)
        pres = data.get("currentPressure", 0.0)
        temp = data.get("currentTemperature", 0.0)
        sats = data.get("gpsSatellites", 0)
        gps_fix = data.get("gpsFixValid", 0)
        lat = data.get("latitude", 0.0)
        lon = data.get("longitude", 0.0)
        dt_str = data.get("dateTimeString", "")

        print(
            f"SerialWorker: CSV #{self._csv_parse_count:04d} | "
            f"water={wh:.1f}cm theta={theta:.1f}° bat={bat:.1f}% "
            f"rssi={rssi} pres={pres:.1f}hPa temp={temp:.1f}°C "
            f"gps={'FIX' if gps_fix else 'NOFIX'}({sats}sats) "
            f"pos={lat:.6f},{lon:.6f} | {dt_str}"
        )

        self.csvReceived.emit(data)

    def _generate_simulated_csv(self):
        t = self._sim_tick
        theta = 12.5 + 3.0 * math.sin(t * 0.05)
        water_height = 45.0 + 8.0 * math.sin(t * 0.03) + random.uniform(-0.5, 0.5)
        tilt_x = 2.1 + 1.5 * math.sin(t * 0.07) + random.uniform(-0.2, 0.2)
        tilt_y = -0.8 + 1.0 * math.cos(t * 0.06) + random.uniform(-0.2, 0.2)
        olp_length = 120.0
        h_dist = olp_length * math.cos(math.radians(theta))
        pressure = 1013.25 + 0.5 * math.sin(t * 0.02) + random.uniform(-0.1, 0.1)
        temperature = 28.5 + 2.0 * math.sin(t * 0.01) + random.uniform(-0.3, 0.3)
        baseline_pressure = 1013.25
        pressure_dev = pressure - baseline_pressure
        submersion = 0
        estimated_depth = 0.0
        bmp_available = 1

        now = datetime.now(timezone(timedelta(hours=5, minutes=30)))
        unix_time = int(now.timestamp())
        dt_string = now.strftime("%Y-%m-%d %H:%M:%S")
        rtc_valid = 1

        rate_15 = 0.3 + random.uniform(-0.1, 0.1)
        flood_alert = 0
        session_dur = t
        peak = max(45.0, water_height)
        min_h = min(45.0, water_height)

        lat = 19.076090 + random.uniform(-0.000005, 0.000005)
        lon = 72.877426 + random.uniform(-0.000005, 0.000005)
        alt = 14.2 + random.uniform(-0.5, 0.5)
        sats = random.randint(6, 12)
        gps_fix = 1

        rssi = max(0, min(31, 18 + int(5 * math.sin(t * 0.04)) + random.randint(-2, 2)))
        sim_reg = 1
        sim_avail = 1

        zone = 0
        response_level = 0
        sustained = 0

        battery = max(0.0, min(100.0, 85.0 - t * 0.02 + random.uniform(-0.5, 0.5)))

        sample_int = 1
        transmit_int = 300
        ob_light = 0
        debug_en = 1
        algo_en = 1

        fields = [
            f"{theta:.2f}", f"{water_height:.2f}", f"{tilt_x:.2f}", f"{tilt_y:.2f}",
            f"{olp_length:.2f}", f"{h_dist:.2f}", f"{pressure:.2f}", f"{temperature:.2f}",
            f"{baseline_pressure:.2f}", f"{pressure_dev:.2f}", str(submersion), f"{estimated_depth:.2f}",
            str(bmp_available), str(unix_time), dt_string, str(rtc_valid),
            f"{rate_15:.2f}", str(flood_alert), str(session_dur), f"{peak:.2f}",
            f"{min_h:.2f}", f"{lat:.6f}", f"{lon:.6f}", f"{alt:.2f}",
            str(sats), str(gps_fix), str(rssi), str(sim_reg),
            str(sim_avail), str(zone), str(response_level), str(sustained),
            f"{battery:.2f}", str(sample_int), str(transmit_int), str(ob_light),
            str(debug_en), str(algo_en),
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
# Phase: Phase 5 Step 3
# ----------------------------END----------------------------------
