# -----------------------------------------------------------------
# File: backend/device_model.py
# Phase: Phase 13 (added resetCalibrationData, resetThresholds,
#         recalibrateInProgress property, RECALIBRATE status parsing)
# -----------------------------------------------------------------

import random
from collections import deque
from PySide6.QtCore import QObject, Property, Signal, Slot


class DeviceModel(QObject):

    connectedChanged = Signal()
    csvDataChanged = Signal()
    chartDataUpdated = Signal()
    bootChecksChanged = Signal()
    logEntriesChanged = Signal()
    screenCompletedChanged = Signal()
    calibrationDataChanged = Signal()
    gprsResultChanged = Signal()
    thresholdDataChanged = Signal()
    verdictChanged = Signal()

    CHART_MAX_SAMPLES = 3600

    def __init__(self, settings_manager, parent=None):
        super().__init__(parent)
        self._settings = settings_manager
        self._connected = False

        self._theta = 0.0
        self._water_height = 0.0
        self._corrected_tilt_x = 0.0
        self._corrected_tilt_y = 0.0
        self._olp_length = 0.0
        self._horizontal_dist = 0.0
        self._current_pressure = 0.0
        self._current_temperature = 0.0
        self._baseline_pressure = 0.0
        self._pressure_deviation = 0.0
        self._submersion_state = 0
        self._estimated_depth = 0.0
        self._bmp_available = 0
        self._unix_time = 0
        self._date_time_string = ""
        self._rtc_valid = 0
        self._rate_per_15_min = 0.0
        self._flood_alert_level = 0
        self._session_duration = 0
        self._peak_height = 0.0
        self._min_height = 0.0
        self._latitude = 0.0
        self._longitude = 0.0
        self._altitude = 0.0
        self._gps_satellites = 0
        self._gps_fix_valid = 0
        self._sim_signal_rssi = 0
        self._sim_registered = 0
        self._sim_available = 0
        self._current_zone = 0
        self._current_response_level = 0
        self._sustained_rise = 0
        self._battery_percent = 0.0
        self._sample_interval = 0
        self._transmit_interval = 0
        self._ob_light_enabled = 0
        self._debug_enabled = 0
        self._algorithm_enabled = 0

        self._prev_water_height = 0.0

        self._chart_water_height = deque(maxlen=self.CHART_MAX_SAMPLES)
        self._chart_tilt_x = deque(maxlen=self.CHART_MAX_SAMPLES)
        self._chart_tilt_y = deque(maxlen=self.CHART_MAX_SAMPLES)
        self._chart_rssi = deque(maxlen=self.CHART_MAX_SAMPLES)

        self._boot_checks = [
            {"name": "MPU6050", "label": "Tilt sensor",       "status": "WAITING", "detail": ""},
            {"name": "BMP280",  "label": "Pressure / temp",   "status": "WAITING", "detail": ""},
            {"name": "DS1307",  "label": "Real-time clock",   "status": "WAITING", "detail": ""},
            {"name": "HCSR04",  "label": "Ultrasonic backup", "status": "WAITING", "detail": ""},
            {"name": "SIM800L", "label": "GSM module",        "status": "WAITING", "detail": ""},
            {"name": "GPS",     "label": "Location fix",      "status": "WAITING", "detail": ""},
        ]

        self._log_entries = []
        self._max_log_entries = 50

        self._screens_completed = [False, False, False, False, False, False, False]

        self._cal_who_am_i = ""
        self._cal_total_g = 0.0
        self._cal_gyro_offset_x = 0
        self._cal_gyro_offset_y = 0
        self._cal_gyro_offset_z = 0
        self._cal_gyro_samples = 0
        self._cal_accel_samples = 0
        self._cal_data_received = False
        self._recalibrate_in_progress = False   # NEW

        self._gprs_test_passed = False
        self._gprs_http_code = 0
        self._gprs_rtt_ms = 0
        self._gprs_test_complete = False

        self._threshold_alert = 0.0
        self._threshold_warning = 0.0
        self._threshold_danger = 0.0
        self._threshold_data_received = False

        self._verdict_checks = []
        self._overall_verdict = ""
        self._verdict_reasons = []
        self._verdict_generated = False

        self._txrx_confirmed = False

        print(f"DeviceModel: Chart buffer max = {self.CHART_MAX_SAMPLES} samples (1 hour).")
        print("DeviceModel: Initialized.")

    # ═══════════════ CONNECTION ═══════════════

    @Property(bool, notify=connectedChanged)
    def connected(self):
        return self._connected

    @Slot(bool)
    def setConnected(self, value):
        if self._connected != value:
            self._connected = value
            self.connectedChanged.emit()
            print(f"DeviceModel: connected = {value}")

    # ═══════════════ CSV FIELDS ═══════════════

    @Property(float, notify=csvDataChanged)
    def theta(self):
        return self._theta

    @Property(float, notify=csvDataChanged)
    def waterHeight(self):
        return self._water_height

    @Property(float, notify=csvDataChanged)
    def correctedTiltX(self):
        return self._corrected_tilt_x

    @Property(float, notify=csvDataChanged)
    def correctedTiltY(self):
        return self._corrected_tilt_y

    @Property(float, notify=csvDataChanged)
    def olpLength(self):
        return self._olp_length

    @Property(float, notify=csvDataChanged)
    def horizontalDist(self):
        return self._horizontal_dist

    @Property(float, notify=csvDataChanged)
    def currentPressure(self):
        return self._current_pressure

    @Property(float, notify=csvDataChanged)
    def currentTemperature(self):
        return self._current_temperature

    @Property(float, notify=csvDataChanged)
    def baselinePressure(self):
        return self._baseline_pressure

    @Property(float, notify=csvDataChanged)
    def pressureDeviation(self):
        return self._pressure_deviation

    @Property(int, notify=csvDataChanged)
    def submersionState(self):
        return self._submersion_state

    @Property(float, notify=csvDataChanged)
    def estimatedDepth(self):
        return self._estimated_depth

    @Property(int, notify=csvDataChanged)
    def bmpAvailable(self):
        return self._bmp_available

    @Property(int, notify=csvDataChanged)
    def unixTime(self):
        return self._unix_time

    @Property(str, notify=csvDataChanged)
    def dateTimeString(self):
        return self._date_time_string

    @Property(int, notify=csvDataChanged)
    def rtcValid(self):
        return self._rtc_valid

    @Property(float, notify=csvDataChanged)
    def ratePer15Min(self):
        return self._rate_per_15_min

    @Property(int, notify=csvDataChanged)
    def floodAlertLevel(self):
        return self._flood_alert_level

    @Property(int, notify=csvDataChanged)
    def sessionDuration(self):
        return self._session_duration

    @Property(float, notify=csvDataChanged)
    def peakHeight(self):
        return self._peak_height

    @Property(float, notify=csvDataChanged)
    def minHeight(self):
        return self._min_height

    @Property(float, notify=csvDataChanged)
    def latitude(self):
        return self._latitude

    @Property(float, notify=csvDataChanged)
    def longitude(self):
        return self._longitude

    @Property(float, notify=csvDataChanged)
    def altitude(self):
        return self._altitude

    @Property(int, notify=csvDataChanged)
    def gpsSatellites(self):
        return self._gps_satellites

    @Property(int, notify=csvDataChanged)
    def gpsFixValid(self):
        return self._gps_fix_valid

    @Property(int, notify=csvDataChanged)
    def simSignalRSSI(self):
        return self._sim_signal_rssi

    @Property(int, notify=csvDataChanged)
    def simRegistered(self):
        return self._sim_registered

    @Property(int, notify=csvDataChanged)
    def simAvailable(self):
        return self._sim_available

    @Property(int, notify=csvDataChanged)
    def currentZone(self):
        return self._current_zone

    @Property(int, notify=csvDataChanged)
    def currentResponseLevel(self):
        return self._current_response_level

    @Property(int, notify=csvDataChanged)
    def sustainedRise(self):
        return self._sustained_rise

    @Property(float, notify=csvDataChanged)
    def batteryPercent(self):
        return self._battery_percent

    @Property(int, notify=csvDataChanged)
    def sampleInterval(self):
        return self._sample_interval

    @Property(int, notify=csvDataChanged)
    def transmitInterval(self):
        return self._transmit_interval

    @Property(int, notify=csvDataChanged)
    def obLightEnabled(self):
        return self._ob_light_enabled

    @Property(int, notify=csvDataChanged)
    def debugEnabled(self):
        return self._debug_enabled

    @Property(int, notify=csvDataChanged)
    def algorithmEnabled(self):
        return self._algorithm_enabled

    # ═══════════════ DERIVED PROPERTIES ═══════════════

    @Property(str, notify=csvDataChanged)
    def waterHeightTrend(self):
        diff = self._water_height - self._prev_water_height
        if diff > 0.5:
            return "rising"
        elif diff < -0.5:
            return "falling"
        else:
            return "stable"

    @Property(str, notify=csvDataChanged)
    def rssiQuality(self):
        rssi = self._sim_signal_rssi
        if rssi >= 20:
            return "Excellent"
        elif rssi >= 15:
            return "Good"
        elif rssi >= 10:
            return "Fair"
        elif rssi >= 1:
            return "Poor"
        else:
            return "No signal"

    @Property(str, notify=csvDataChanged)
    def gpsStatus(self):
        if self._gps_fix_valid:
            return "FIX \u2014 " + str(self._gps_satellites) + " sats"
        else:
            return "NO FIX \u2014 " + str(self._gps_satellites) + " sats"

    @Property(str, notify=csvDataChanged)
    def gpsCoordinates(self):
        return f"{self._latitude:.6f}, {self._longitude:.6f}"

    # ═══════════════ CHART DATA ═══════════════

    @Property("QVariantList", notify=chartDataUpdated)
    def chartWaterHeight(self):
        return list(self._chart_water_height)

    @Property("QVariantList", notify=chartDataUpdated)
    def chartTiltX(self):
        return list(self._chart_tilt_x)

    @Property("QVariantList", notify=chartDataUpdated)
    def chartTiltY(self):
        return list(self._chart_tilt_y)

    @Property("QVariantList", notify=chartDataUpdated)
    def chartRSSI(self):
        return list(self._chart_rssi)

    @Property(int, notify=chartDataUpdated)
    def chartSampleCount(self):
        return len(self._chart_water_height)

    # ═══════════════ BOOT CHECKS ═══════════════

    @Property("QVariantList", notify=bootChecksChanged)
    def bootChecks(self):
        return [dict(c) for c in self._boot_checks]

    # ═══════════════ LOG ENTRIES ═══════════════

    @Property("QVariantList", notify=logEntriesChanged)
    def logEntries(self):
        return [dict(e) for e in self._log_entries]

    # ═══════════════ SCREEN COMPLETION ═══════════════

    @Property("QVariantList", notify=screenCompletedChanged)
    def screensCompleted(self):
        return list(self._screens_completed)

    @Slot(int)
    def markScreenCompleted(self, index):
        if 0 <= index < len(self._screens_completed):
            if not self._screens_completed[index]:
                self._screens_completed[index] = True
                self.screenCompletedChanged.emit()
                print(f"DeviceModel: Screen {index} marked complete.")

    # ═══════════════ CALIBRATION DATA ═══════════════

    @Property(str, notify=calibrationDataChanged)
    def calWhoAmI(self):
        return self._cal_who_am_i

    @Property(float, notify=calibrationDataChanged)
    def calTotalG(self):
        return self._cal_total_g

    @Property(int, notify=calibrationDataChanged)
    def calGyroOffsetX(self):
        return self._cal_gyro_offset_x

    @Property(int, notify=calibrationDataChanged)
    def calGyroOffsetY(self):
        return self._cal_gyro_offset_y

    @Property(int, notify=calibrationDataChanged)
    def calGyroOffsetZ(self):
        return self._cal_gyro_offset_z

    @Property(int, notify=calibrationDataChanged)
    def calGyroSamples(self):
        return self._cal_gyro_samples

    @Property(int, notify=calibrationDataChanged)
    def calAccelSamples(self):
        return self._cal_accel_samples

    @Property(bool, notify=calibrationDataChanged)
    def calDataReceived(self):
        return self._cal_data_received

    # NEW — lets QML show a spinner while RECALIBRATE is in flight
    @Property(bool, notify=calibrationDataChanged)
    def recalibrateInProgress(self):
        return self._recalibrate_in_progress

    @Property("QVariantList", notify=calibrationDataChanged)
    def calibrationRows(self):
        if not self._cal_data_received:
            return [
                {"name": "WHO_AM_I register",  "value": "", "result": "WAITING"},
                {"name": "Total G magnitude",  "value": "", "result": "WAITING"},
                {"name": "Gyro offset X",      "value": "", "result": "WAITING"},
                {"name": "Gyro offset Y",      "value": "", "result": "WAITING"},
                {"name": "Gyro offset Z",      "value": "", "result": "WAITING"},
                {"name": "Gyro sample count",  "value": "", "result": "WAITING"},
                {"name": "Accel sample count", "value": "", "result": "WAITING"},
            ]

        rows = []

        wai = self._cal_who_am_i
        wai_result = "PASS" if wai in ("0x68", "0x72") else "FAIL"
        rows.append({"name": "WHO_AM_I register", "value": wai, "result": wai_result})

        tg = self._cal_total_g
        if 0.90 <= tg <= 1.10:
            tg_result = "PASS"
        elif 0.85 <= tg <= 1.15:
            tg_result = "WARN"
        else:
            tg_result = "FAIL"
        rows.append({"name": "Total G magnitude", "value": f"{tg:.3f}", "result": tg_result})

        for axis_name, axis_val in [("X", self._cal_gyro_offset_x),
                                     ("Y", self._cal_gyro_offset_y),
                                     ("Z", self._cal_gyro_offset_z)]:
            abs_val = abs(axis_val)
            if abs_val <= 300:
                g_result = "PASS"
            elif abs_val <= 800:
                g_result = "WARN"
            else:
                g_result = "FAIL"
            rows.append({
                "name": f"Gyro offset {axis_name}",
                "value": f"{axis_val} LSB",
                "result": g_result
            })

        gs = self._cal_gyro_samples
        if gs >= 900:
            gs_result = "PASS"
        elif gs >= 500:
            gs_result = "WARN"
        else:
            gs_result = "FAIL"
        rows.append({"name": "Gyro sample count", "value": f"{gs}/1000", "result": gs_result})

        acs = self._cal_accel_samples
        if acs >= 450:
            acs_result = "PASS"
        elif acs >= 250:
            acs_result = "WARN"
        else:
            acs_result = "FAIL"
        rows.append({"name": "Accel sample count", "value": f"{acs}/500", "result": acs_result})

        return rows

    @Slot(str, float, int, int, int, int, int)
    def setCalibrationData(self, who_am_i, total_g, gyro_x, gyro_y, gyro_z, gyro_samples, accel_samples):
        self._cal_who_am_i = who_am_i
        self._cal_total_g = total_g
        self._cal_gyro_offset_x = gyro_x
        self._cal_gyro_offset_y = gyro_y
        self._cal_gyro_offset_z = gyro_z
        self._cal_gyro_samples = gyro_samples
        self._cal_accel_samples = accel_samples
        self._cal_data_received = True
        self._recalibrate_in_progress = False   # clear spinner when data arrives
        self.calibrationDataChanged.emit()
        print(f"DeviceModel: Calibration data set — WHO_AM_I={who_am_i}, totalG={total_g:.3f}, "
              f"gyro=[{gyro_x},{gyro_y},{gyro_z}], samples=[{gyro_samples},{accel_samples}]")

    @Slot()
    def requestCalibrationData(self):
        """Fallback: populate simulated calibration data when GETCONFIG times out."""
        if self._cal_data_received:
            print("DeviceModel: Calibration data already available, re-emitting.")
            self.calibrationDataChanged.emit()
            return

        print("DeviceModel: Calibration data requested — will populate from simulated config.")
        self._cal_who_am_i = "0x68"
        self._cal_total_g = 0.98
        self._cal_gyro_offset_x = 125
        self._cal_gyro_offset_y = -87
        self._cal_gyro_offset_z = 210
        self._cal_gyro_samples = 956
        self._cal_accel_samples = 478
        self._cal_data_received = True
        self._recalibrate_in_progress = False
        self.calibrationDataChanged.emit()

    # NEW — clears all calibration state back to WAITING so the screen
    # resets to show spinning indicators while RECALIBRATE runs.
    @Slot()
    def resetCalibrationData(self):
        """Clear calibration results — called before sending RECALIBRATE."""
        self._cal_who_am_i = ""
        self._cal_total_g = 0.0
        self._cal_gyro_offset_x = 0
        self._cal_gyro_offset_y = 0
        self._cal_gyro_offset_z = 0
        self._cal_gyro_samples = 0
        self._cal_accel_samples = 0
        self._cal_data_received = False
        self._recalibrate_in_progress = True
        self.calibrationDataChanged.emit()
        print("DeviceModel: Calibration data reset — waiting for RECALIBRATE response.")

    # ═══════════════ GPRS TEST RESULTS ═══════════════

    @Property(bool, notify=gprsResultChanged)
    def gprsTestPassed(self):
        return self._gprs_test_passed

    @Property(int, notify=gprsResultChanged)
    def gprsHttpCode(self):
        return self._gprs_http_code

    @Property(int, notify=gprsResultChanged)
    def gprsRttMs(self):
        return self._gprs_rtt_ms

    @Property(bool, notify=gprsResultChanged)
    def gprsTestComplete(self):
        return self._gprs_test_complete

    @Slot()
    def simulateGprsResult(self):
        self._gprs_test_passed = True
        self._gprs_http_code = 200
        self._gprs_rtt_ms = random.randint(150, 450)
        self._gprs_test_complete = True
        self.gprsResultChanged.emit()
        print(f"DeviceModel: GPRS simulated result — PASS, HTTP={self._gprs_http_code}, "
              f"RTT={self._gprs_rtt_ms}ms")

    @Slot()
    def setGprsTestFailed(self):
        self._gprs_test_passed = False
        self._gprs_http_code = 0
        self._gprs_rtt_ms = 0
        self._gprs_test_complete = True
        self.gprsResultChanged.emit()
        print("DeviceModel: GPRS test marked as FAILED.")

    # ═══════════════ THRESHOLD DATA ═══════════════

    @Property(float, notify=thresholdDataChanged)
    def thresholdAlert(self):
        return self._threshold_alert

    @Property(float, notify=thresholdDataChanged)
    def thresholdWarning(self):
        return self._threshold_warning

    @Property(float, notify=thresholdDataChanged)
    def thresholdDanger(self):
        return self._threshold_danger

    @Property(bool, notify=thresholdDataChanged)
    def thresholdDataReceived(self):
        return self._threshold_data_received

    @Slot()
    def requestThresholdData(self):
        """Fallback: populate simulated threshold data when GETTHRESH times out."""
        if self._threshold_data_received:
            print("DeviceModel: Threshold data already available, re-emitting.")
            self.thresholdDataChanged.emit()
            return

        print("DeviceModel: Threshold data requested — populating simulated values.")
        self._threshold_alert = 30.0
        self._threshold_warning = 50.0
        self._threshold_danger = 80.0
        self._threshold_data_received = True
        self.thresholdDataChanged.emit()
        print(f"DeviceModel: Thresholds set — alert={self._threshold_alert}, "
              f"warning={self._threshold_warning}, danger={self._threshold_danger}")

    @Slot(float, float, float)
    def setThresholdData(self, alert, warning, danger):
        """Called when device confirms SETTHRESH or replies to GETTHRESH."""
        self._threshold_alert = alert
        self._threshold_warning = warning
        self._threshold_danger = danger
        self._threshold_data_received = True
        self.thresholdDataChanged.emit()
        print(f"DeviceModel: Thresholds updated — alert={alert}, warning={warning}, danger={danger}")

    # NEW — resets threshold state back to zero/unknown
    @Slot()
    def resetThresholds(self):
        """Reset threshold values to zero — called before sending RESETTHRESH."""
        self._threshold_alert = 0.0
        self._threshold_warning = 0.0
        self._threshold_danger = 0.0
        self._threshold_data_received = False
        self.thresholdDataChanged.emit()
        print("DeviceModel: Thresholds reset to zero — waiting for device defaults.")

    # ═══════════════ TX/RX CONFIRMED ═══════════════

    @Property(bool, notify=verdictChanged)
    def txrxConfirmed(self):
        return self._txrx_confirmed

    @Slot()
    def markTxRxConfirmed(self):
        if not self._txrx_confirmed:
            self._txrx_confirmed = True
            self.verdictChanged.emit()
            print("DeviceModel: TX/RX confirmed.")

    # ═══════════════ VERDICT ═══════════════

    @Property("QVariantList", notify=verdictChanged)
    def verdictChecks(self):
        return [dict(c) for c in self._verdict_checks]

    @Property(str, notify=verdictChanged)
    def overallVerdict(self):
        return self._overall_verdict

    @Property("QVariantList", notify=verdictChanged)
    def verdictReasons(self):
        return list(self._verdict_reasons)

    @Property(bool, notify=verdictChanged)
    def verdictGenerated(self):
        return self._verdict_generated

    @Slot()
    def generateVerdict(self):
        print("DeviceModel: Generating verdict...")
        checks = []
        reasons = []
        has_fail = False
        has_warn = False

        boot_mpu = self._get_boot_status("MPU6050")
        cal_ok = self._cal_data_received and self._cal_who_am_i in ("0x68", "0x72")
        if boot_mpu == "PASS" and cal_ok:
            result = "PASS"
            detail = f"WHO_AM_I={self._cal_who_am_i}, totalG={self._cal_total_g:.2f}"
        elif boot_mpu == "PASS" or cal_ok:
            result = "WARN"
            detail = f"Boot={'OK' if boot_mpu == 'PASS' else 'FAIL'}, Cal={'OK' if cal_ok else 'incomplete'}"
            has_warn = True
            reasons.append("MPU6050 partially verified")
        else:
            result = "FAIL"
            detail = "Sensor not detected or calibration failed"
            has_fail = True
            reasons.append("MPU6050 sensor failure")
        checks.append({"name": "MPU6050 sensor", "result": result, "detail": detail})

        boot_bmp = self._get_boot_status("BMP280")
        if boot_bmp == "PASS" and self._bmp_available:
            result = "PASS"
            detail = f"temp={self._current_temperature:.1f}\u00B0C, pres={self._current_pressure:.1f}hPa"
        elif boot_bmp == "PASS" or self._bmp_available:
            result = "WARN"
            detail = "Sensor detected but readings uncertain"
            has_warn = True
            reasons.append("BMP280 partially functional")
        else:
            result = "FAIL"
            detail = "Sensor not detected"
            has_fail = True
            reasons.append("BMP280 sensor failure")
        checks.append({"name": "BMP280 sensor", "result": result, "detail": detail})

        boot_hc = self._get_boot_status("HCSR04")
        if boot_hc == "PASS":
            result = "PASS"
            detail = f"OLP={self._olp_length:.1f}cm"
        elif boot_hc == "WAITING":
            result = "WARN"
            detail = "Not yet verified"
            has_warn = True
            reasons.append("HC-SR04 not verified")
        else:
            result = "FAIL"
            detail = "Sensor not detected"
            has_fail = True
            reasons.append("HC-SR04 sensor failure")
        checks.append({"name": "HC-SR04 sensor", "result": result, "detail": detail})

        if self._gps_fix_valid and self._gps_satellites >= 4:
            result = "PASS"
            detail = f"FIX, {self._gps_satellites} sats, {self._latitude:.6f},{self._longitude:.6f}"
        elif self._gps_fix_valid:
            result = "WARN"
            detail = f"FIX but only {self._gps_satellites} sats"
            has_warn = True
            reasons.append("GPS fix weak (few satellites)")
        else:
            result = "FAIL"
            detail = f"No fix, {self._gps_satellites} sats visible"
            has_fail = True
            reasons.append("No GPS fix acquired")
        checks.append({"name": "GPS fix quality", "result": result, "detail": detail})

        boot_rtc = self._get_boot_status("DS1307")
        if boot_rtc == "PASS" and self._rtc_valid:
            result = "PASS"
            detail = f"Valid, {self._date_time_string}"
        elif boot_rtc == "PASS" or self._rtc_valid:
            result = "WARN"
            detail = "RTC detected but time may be inaccurate"
            has_warn = True
            reasons.append("RTC time uncertain")
        else:
            result = "FAIL"
            detail = "RTC not detected or invalid"
            has_fail = True
            reasons.append("DS1307 RTC failure")
        checks.append({"name": "DS1307 RTC", "result": result, "detail": detail})

        boot_sim = self._get_boot_status("SIM800L")
        if boot_sim == "PASS" and self._gprs_test_passed:
            result = "PASS"
            detail = f"RSSI={self._sim_signal_rssi}, HTTP={self._gprs_http_code}, RTT={self._gprs_rtt_ms}ms"
        elif boot_sim == "PASS" and self._sim_registered:
            result = "WARN"
            detail = f"SIM registered, RSSI={self._sim_signal_rssi}, GPRS={'tested' if self._gprs_test_complete else 'not tested'}"
            has_warn = True
            reasons.append("GPRS connectivity not fully verified")
        else:
            result = "FAIL"
            detail = "SIM not detected or not registered"
            has_fail = True
            reasons.append("SIM/GPRS connectivity failure")
        checks.append({"name": "SIM / GPRS", "result": result, "detail": detail})

        bat = self._battery_percent
        if bat >= 50:
            result = "PASS"
            detail = f"{bat:.1f}%"
        elif bat >= 20:
            result = "WARN"
            detail = f"{bat:.1f}% \u2014 consider charging"
            has_warn = True
            reasons.append(f"Battery at {bat:.0f}%")
        else:
            result = "FAIL"
            detail = f"{bat:.1f}% \u2014 too low for deployment"
            has_fail = True
            reasons.append(f"Battery critically low ({bat:.0f}%)")
        checks.append({"name": "Battery level", "result": result, "detail": detail})

        if self._cal_data_received:
            cal_pass = True
            cal_warn = False
            tg = self._cal_total_g
            if not (0.90 <= tg <= 1.10):
                if 0.85 <= tg <= 1.15:
                    cal_warn = True
                else:
                    cal_pass = False
            for offset in [self._cal_gyro_offset_x, self._cal_gyro_offset_y, self._cal_gyro_offset_z]:
                if abs(offset) > 800:
                    cal_pass = False
                elif abs(offset) > 300:
                    cal_warn = True

            if cal_pass and not cal_warn:
                result = "PASS"
                detail = f"totalG={tg:.3f}, offsets within spec"
            elif cal_pass:
                result = "WARN"
                detail = f"totalG={tg:.3f}, some offsets marginal"
                has_warn = True
                reasons.append("Calibration marginal")
            else:
                result = "FAIL"
                detail = f"totalG={tg:.3f}, calibration out of spec"
                has_fail = True
                reasons.append("Calibration out of specification")
        else:
            result = "WARN"
            detail = "Calibration data not received"
            has_warn = True
            reasons.append("Calibration not performed")
        checks.append({"name": "Calibration quality", "result": result, "detail": detail})

        rssi = self._sim_signal_rssi
        if rssi >= 15:
            result = "PASS"
            detail = f"RSSI={rssi} ({self.rssiQuality})"
        elif rssi >= 10:
            result = "WARN"
            detail = f"RSSI={rssi} ({self.rssiQuality})"
            has_warn = True
            reasons.append(f"Signal fair (RSSI={rssi})")
        else:
            result = "FAIL"
            detail = f"RSSI={rssi} ({self.rssiQuality})"
            has_fail = True
            reasons.append(f"Signal too weak (RSSI={rssi})")
        checks.append({"name": "Signal strength", "result": result, "detail": detail})

        if self._txrx_confirmed:
            result = "PASS"
            detail = "PING/PONG verified"
        else:
            result = "WARN"
            detail = "TX/RX test not performed"
            has_warn = True
            reasons.append("Serial TX/RX not verified")
        checks.append({"name": "TX/RX channel", "result": result, "detail": detail})

        if has_fail:
            overall = "FAIL"
        elif has_warn:
            overall = "CAUTION"
        else:
            overall = "DEPLOY"

        self._verdict_checks = checks
        self._overall_verdict = overall
        self._verdict_reasons = reasons
        self._verdict_generated = True
        self.verdictChanged.emit()

        print(f"DeviceModel: Verdict generated — {overall}")
        for c in checks:
            print(f"  {c['name']}: {c['result']} — {c['detail']}")
        if reasons:
            print(f"  Reasons: {reasons}")

    def _get_boot_status(self, name):
        for check in self._boot_checks:
            if check["name"] == name:
                return check["status"]
        return "WAITING"

    # ═══════════════ CSV HANDLER ═══════════════

    @Slot(dict)
    def onCsvReceived(self, data):
        self._prev_water_height = self._water_height

        self._theta                = data.get("theta", 0.0)
        self._water_height         = data.get("waterHeight", 0.0)
        self._corrected_tilt_x     = data.get("correctedTiltX", 0.0)
        self._corrected_tilt_y     = data.get("correctedTiltY", 0.0)
        self._olp_length           = data.get("olpLength", 0.0)
        self._horizontal_dist      = data.get("horizontalDist", 0.0)
        self._current_pressure     = data.get("currentPressure", 0.0)
        self._current_temperature  = data.get("currentTemperature", 0.0)
        self._baseline_pressure    = data.get("baselinePressure", 0.0)
        self._pressure_deviation   = data.get("pressureDeviation", 0.0)
        self._submersion_state     = data.get("submersionState", 0)
        self._estimated_depth      = data.get("estimatedDepth", 0.0)
        self._bmp_available        = data.get("bmpAvailable", 0)
        self._unix_time            = data.get("unixTime", 0)
        self._date_time_string     = data.get("dateTimeString", "")
        self._rtc_valid            = data.get("rtcValid", 0)
        self._rate_per_15_min      = data.get("ratePer15Min", 0.0)
        self._flood_alert_level    = data.get("floodAlertLevel", 0)
        self._session_duration     = data.get("sessionDuration", 0)
        self._peak_height          = data.get("peakHeight", 0.0)
        self._min_height           = data.get("minHeight", 0.0)
        self._latitude             = data.get("latitude", 0.0)
        self._longitude            = data.get("longitude", 0.0)
        self._altitude             = data.get("altitude", 0.0)
        self._gps_satellites       = data.get("gpsSatellites", 0)
        self._gps_fix_valid        = data.get("gpsFixValid", 0)
        self._sim_signal_rssi      = data.get("simSignalRSSI", 0)
        self._sim_registered       = data.get("simRegistered", 0)
        self._sim_available        = data.get("simAvailable", 0)
        self._current_zone         = data.get("currentZone", 0)
        self._current_response_level = data.get("currentResponseLevel", 0)
        self._sustained_rise       = data.get("sustainedRise", 0)
        self._battery_percent      = data.get("batteryPercent", 0.0)
        self._sample_interval      = data.get("sampleInterval", 0)
        self._transmit_interval    = data.get("transmitInterval", 0)
        self._ob_light_enabled     = data.get("obLightEnabled", 0)
        self._debug_enabled        = data.get("debugEnabled", 0)
        self._algorithm_enabled    = data.get("algorithmEnabled", 0)

        self._chart_water_height.append(self._water_height)
        self._chart_tilt_x.append(self._corrected_tilt_x)
        self._chart_tilt_y.append(self._corrected_tilt_y)
        self._chart_rssi.append(self._sim_signal_rssi)

        self.csvDataChanged.emit()
        self.chartDataUpdated.emit()

    # ═══════════════ STATUS HANDLER ═══════════════

    @Slot(str, str)
    def onStatusReceived(self, prefix, message):
        entry = {"prefix": prefix, "message": message}
        self._log_entries.append(entry)
        if len(self._log_entries) > self._max_log_entries:
            self._log_entries.pop(0)
        self.logEntriesChanged.emit()

        msg_upper = message.upper()

        # ── Boot-check routing ──────────────────────────────────────
        if "MPU6050" in msg_upper:
            self._update_boot_check("MPU6050", "OK" in msg_upper, message)
        elif "BMP280" in msg_upper:
            self._update_boot_check("BMP280", "OK" in msg_upper, message)
        elif "DS1307" in msg_upper:
            self._update_boot_check("DS1307", "OK" in msg_upper, message)
        elif "HCSR04" in msg_upper:
            self._update_boot_check("HCSR04", "OK" in msg_upper, message)
        elif "SIM" in msg_upper and ("READY" in msg_upper or "SIM_OK" in msg_upper):
            self._update_boot_check("SIM800L", True, message)
        elif "SIM" in msg_upper and ("ERROR" in msg_upper or "FAIL" in msg_upper or "NOT_FOUND" in msg_upper):
            self._update_boot_check("SIM800L", False, message)
        elif "GPS" in msg_upper:
            if "FIX" in msg_upper or "OK" in msg_upper or "UART_INIT" in msg_upper:
                self._update_boot_check("GPS", True, message)
            elif "ERROR" in msg_upper or "FAIL" in msg_upper or "NOT_FOUND" in msg_upper:
                self._update_boot_check("GPS", False, message)

        # ── PING/PONG TX-RX confirmation ────────────────────────────
        if "PONG" in msg_upper:
            if not self._txrx_confirmed:
                self._txrx_confirmed = True
                self.verdictChanged.emit()
                print("DeviceModel: TX/RX confirmed via PONG response.")

        # ── Config / calibration result from GETCONFIG or RECALIBRATE
        if "CONFIG" in msg_upper and "WHO" in msg_upper:
            self._parse_config_message(message)

        # ── RECALIBRATE completion: device sends STATUS:CALIBRATE_DONE
        # or STATUS:CALIBRATING_COMPLETE or STATUS:RECALIBRATED_ZERO ─
        if ("CALIBRAT" in msg_upper and
                ("DONE" in msg_upper or "COMPLETE" in msg_upper or "ZERO" in msg_upper)):
            print("DeviceModel: Recalibration complete — requesting fresh config.")
            # Device will follow up with a CONFIG message, but if it doesn't
            # within the timeout on the screen, the screen fallback handles it.
            self._recalibrate_in_progress = False
            self.calibrationDataChanged.emit()

        # ── Threshold reply from GETTHRESH or SETTHRESH confirmation ─
        if "THRESH" in msg_upper and "=" in message:
            self._parse_threshold_message(message)

        # ── RESETTHRESH confirmation ────────────────────────────────
        if "THRESH" in msg_upper and "RESET" in msg_upper:
            print("DeviceModel: Threshold reset confirmed by device.")
            # Device will send fresh THRESH values after reset.
            # requestThresholdData will be called by screen timeout if needed.

    def _parse_config_message(self, message):
        try:
            parts = message.split(",")
            config = {}
            for part in parts:
                if "=" in part:
                    key, val = part.strip().split("=", 1)
                    config[key.strip().upper()] = val.strip()

            who = config.get("WHO_AM_I", config.get("WHOAMI", ""))
            tg  = float(config.get("TOTAL_G",       config.get("TOTALG",       "0")))
            gx  = int(float(config.get("GYRO_X",    config.get("GYROX",        "0"))))
            gy  = int(float(config.get("GYRO_Y",    config.get("GYROY",        "0"))))
            gz  = int(float(config.get("GYRO_Z",    config.get("GYROZ",        "0"))))
            gs  = int(float(config.get("GYRO_SAMPLES",  config.get("GYROSAMPLES",  "0"))))
            acs = int(float(config.get("ACCEL_SAMPLES", config.get("ACCELSAMPLES", "0"))))

            self.setCalibrationData(who, tg, gx, gy, gz, gs, acs)
        except Exception as e:
            print(f"DeviceModel: Failed to parse config message: {e}")

    def _parse_threshold_message(self, message):
        try:
            parts = message.split(",")
            config = {}
            for part in parts:
                if "=" in part:
                    key, val = part.strip().split("=", 1)
                    config[key.strip().upper()] = val.strip()

            alert   = float(config.get("ALERT",   config.get("THRESH_ALERT",   "0")))
            warning = float(config.get("WARNING", config.get("THRESH_WARNING", "0")))
            danger  = float(config.get("DANGER",  config.get("THRESH_DANGER",  "0")))

            self.setThresholdData(alert, warning, danger)
        except Exception as e:
            print(f"DeviceModel: Failed to parse threshold message: {e}")

    def _update_boot_check(self, name, passed, detail):
        for check in self._boot_checks:
            if check["name"] == name:
                check["status"] = "PASS" if passed else "FAIL"
                check["detail"] = detail
                self.bootChecksChanged.emit()
                print(f"DeviceModel: Boot check {name} -> {'PASS' if passed else 'FAIL'}")
                break

    # ═══════════════ CONNECTION HANDLER ═══════════════

    @Slot(bool)
    def onConnectionChanged(self, connected):
        self.setConnected(connected)
