# -----------------------------------------------------------------
# File : backend/settings_manager.py
# Phase : Phase 3 Step 3
# -----------------------------------------------------------------

import os
import json
import sys

from PySide6.QtCore import QObject, Property, Signal, Slot


class SettingsManager(QObject):

    darkModeChanged = Signal()
    animationsEnabledChanged = Signal()
    fontScaleChanged = Signal()
    brightnessChanged = Signal()
    serialPortOverrideChanged = Signal()
    dataLoggingEnabledChanged = Signal()
    chartHistorySecondsChanged = Signal()

    def __init__(self, app_version="1.0.0", parent=None):
        super().__init__(parent)

        self._app_version = app_version
        self._dark_mode = True
        self._animations_enabled = True
        self._font_scale = 1.0
        self._brightness = 100
        self._serial_port_override = "auto"
        self._data_logging_enabled = False
        self._chart_history_seconds = 180

        self._settings_dir = self._resolve_settings_dir()
        self._settings_file = os.path.join(self._settings_dir, "settings.json")

        self._load()

    def _resolve_settings_dir(self):
        varuna_home = os.path.expanduser("/home/varuna/.config/varuna-debugger")
        if os.path.isdir(os.path.dirname(os.path.dirname(varuna_home))):
            return varuna_home
        return os.path.join(os.path.expanduser("~"), ".config", "varuna-debugger")

    def _load(self):
        if not os.path.exists(self._settings_file):
            print(f"Settings file not found at {self._settings_file}, using defaults.")
            self._save()
            return

        try:
            with open(self._settings_file, "r") as f:
                data = json.load(f)
            print(f"Settings loaded from {self._settings_file}")

            self._dark_mode = data.get("darkMode", self._dark_mode)
            self._animations_enabled = data.get("animationsEnabled", self._animations_enabled)
            self._font_scale = data.get("fontScale", self._font_scale)
            self._brightness = data.get("brightness", self._brightness)
            self._serial_port_override = data.get("serialPortOverride", self._serial_port_override)
            self._data_logging_enabled = data.get("dataLoggingEnabled", self._data_logging_enabled)
            self._chart_history_seconds = data.get("chartHistorySeconds", self._chart_history_seconds)

        except (json.JSONDecodeError, OSError) as e:
            print(f"Warning: Could not read settings file: {e}. Using defaults.")
            self._save()

    def _save(self):
        try:
            os.makedirs(self._settings_dir, exist_ok=True)
            data = {
                "darkMode": self._dark_mode,
                "animationsEnabled": self._animations_enabled,
                "fontScale": self._font_scale,
                "brightness": self._brightness,
                "serialPortOverride": self._serial_port_override,
                "dataLoggingEnabled": self._data_logging_enabled,
                "chartHistorySeconds": self._chart_history_seconds,
            }
            with open(self._settings_file, "w") as f:
                json.dump(data, f, indent=2)
        except OSError as e:
            print(f"Warning: Could not write settings file: {e}")

    def _print_all(self):
        print(f"  darkMode            = {self._dark_mode}")
        print(f"  animationsEnabled   = {self._animations_enabled}")
        print(f"  fontScale           = {self._font_scale}")
        print(f"  brightness          = {self._brightness}")
        print(f"  serialPortOverride  = {self._serial_port_override}")
        print(f"  dataLoggingEnabled  = {self._data_logging_enabled}")
        print(f"  chartHistorySeconds = {self._chart_history_seconds}")

    @Property(str, constant=True)
    def appVersion(self):
        return self._app_version

    @Property(str, constant=True)
    def pythonVersion(self):
        return sys.version.split()[0]

    @Property(str, constant=True)
    def pyside6Version(self):
        try:
            import PySide6
            return PySide6.__version__
        except Exception:
            return "unknown"

    @Property(str, constant=True)
    def qtVersion(self):
        try:
            from PySide6.QtCore import qVersion
            return qVersion()
        except Exception:
            return "unknown"

    @Property(str, constant=True)
    def platformName(self):
        try:
            from PySide6.QtGui import QGuiApplication
            app_instance = QGuiApplication.instance()
            if app_instance is not None:
                return app_instance.platformName()
        except Exception:
            pass
        return "unknown"

    @Property(str, constant=True)
    def displayResolution(self):
        try:
            from PySide6.QtGui import QGuiApplication
            app_instance = QGuiApplication.instance()
            if app_instance is not None:
                screen = app_instance.primaryScreen()
                if screen is not None:
                    size = screen.size()
                    return str(size.width()) + "x" + str(size.height())
        except Exception:
            pass
        return "unknown"

    # --- darkMode ---
    @Property(bool, notify=darkModeChanged)
    def darkMode(self):
        return self._dark_mode

    @Slot(bool)
    def setDarkMode(self, value):
        if self._dark_mode != value:
            self._dark_mode = value
            self.darkModeChanged.emit()
            self._save()

    # --- animationsEnabled ---
    @Property(bool, notify=animationsEnabledChanged)
    def animationsEnabled(self):
        return self._animations_enabled

    @Slot(bool)
    def setAnimationsEnabled(self, value):
        if self._animations_enabled != value:
            self._animations_enabled = value
            self.animationsEnabledChanged.emit()
            self._save()

    # --- fontScale ---
    @Property(float, notify=fontScaleChanged)
    def fontScale(self):
        return self._font_scale

    @Slot(float)
    def setFontScale(self, value):
        if value in (0.85, 1.0, 1.15) and self._font_scale != value:
            self._font_scale = value
            self.fontScaleChanged.emit()
            self._save()

    # --- brightness ---
    @Property(int, notify=brightnessChanged)
    def brightness(self):
        return self._brightness

    @Slot(int)
    def setBrightness(self, value):
        clamped = max(10, min(100, value))
        if self._brightness != clamped:
            self._brightness = clamped
            self.brightnessChanged.emit()
            self._save()
            self._apply_brightness(clamped)

    def _apply_brightness(self, value):
        backlight_paths = [
            "/sys/class/backlight/rpi_backlight/brightness",
            "/sys/class/backlight/acpi_video0/brightness",
        ]
        for path in backlight_paths:
            max_path = os.path.join(os.path.dirname(path), "max_brightness")
            if os.path.exists(path) and os.path.exists(max_path):
                try:
                    with open(max_path, "r") as f:
                        max_val = int(f.read().strip())
                    actual = max(1, int(max_val * value / 100))
                    with open(path, "w") as f:
                        f.write(str(actual))
                    print(f"Brightness set to {value}% ({actual}/{max_val}) via {path}")
                    return
                except OSError:
                    pass

    # --- serialPortOverride ---
    @Property(str, notify=serialPortOverrideChanged)
    def serialPortOverride(self):
        return self._serial_port_override

    @Slot(str)
    def setSerialPortOverride(self, value):
        valid = ("auto", "/dev/ttyUSB0", "/dev/ttyACM0", "/dev/ttyS0")
        if value in valid and self._serial_port_override != value:
            self._serial_port_override = value
            self.serialPortOverrideChanged.emit()
            self._save()

    # --- dataLoggingEnabled ---
    @Property(bool, notify=dataLoggingEnabledChanged)
    def dataLoggingEnabled(self):
        return self._data_logging_enabled

    @Slot(bool)
    def setDataLoggingEnabled(self, value):
        if self._data_logging_enabled != value:
            self._data_logging_enabled = value
            self.dataLoggingEnabledChanged.emit()
            self._save()

    # --- chartHistorySeconds ---
    @Property(int, notify=chartHistorySecondsChanged)
    def chartHistorySeconds(self):
        return self._chart_history_seconds

    @Slot(int)
    def setChartHistorySeconds(self, value):
        if value in (60, 180, 300) and self._chart_history_seconds != value:
            self._chart_history_seconds = value
            self.chartHistorySecondsChanged.emit()
            self._save()

# -----------------------------------------------------------------
# File : backend/settings_manager.py
# Phase : Phase 3 Step 3
# ----------------------------END----------------------------------
