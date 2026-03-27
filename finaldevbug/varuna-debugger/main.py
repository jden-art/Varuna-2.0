import sys
import os

from PySide6.QtCore import QThread, QTimer
from PySide6.QtGui import QGuiApplication
from PySide6.QtQml import QQmlApplicationEngine
from PySide6.QtQuickControls2 import QQuickStyle

from backend.settings_manager import SettingsManager
from backend.theme_manager import ThemeManager
from backend.serial_worker import SerialWorker
from backend.serial_commander import SerialCommander
from backend.device_model import DeviceModel
from backend.report_exporter import ReportExporter

APP_VERSION = "1.12.0"


def main():
    print(f"VARUNA Debugger v{APP_VERSION} starting...")

    app = QGuiApplication(sys.argv)

    QQuickStyle.setStyle("Material")

    settings_manager = SettingsManager(APP_VERSION)
    print("Current settings:")
    settings_manager._print_all()

    theme_manager = ThemeManager(settings_manager)
    print(f"Theme initialized: dark={theme_manager.dark}")

    device_model = DeviceModel(settings_manager)

    serial_thread = QThread()
    serial_worker = SerialWorker(settings_manager)
    serial_worker.moveToThread(serial_thread)

    serial_commander = SerialCommander(serial_worker)

    report_exporter = ReportExporter(device_model)

    serial_worker.csvReceived.connect(device_model.onCsvReceived)
    serial_worker.statusReceived.connect(device_model.onStatusReceived)
    serial_worker.connectionChanged.connect(device_model.onConnectionChanged)

    serial_thread.started.connect(serial_worker.start)

    app.aboutToQuit.connect(serial_worker.stop)
    app.aboutToQuit.connect(serial_thread.quit)
    app.aboutToQuit.connect(serial_thread.wait)

    engine = QQmlApplicationEngine()

    settings_manager.setParent(engine)
    theme_manager.setParent(engine)
    device_model.setParent(engine)
    serial_commander.setParent(engine)
    report_exporter.setParent(engine)

    engine.rootContext().setContextProperty("Settings", settings_manager)
    engine.rootContext().setContextProperty("Theme", theme_manager)
    engine.rootContext().setContextProperty("Device", device_model)
    engine.rootContext().setContextProperty("Commander", serial_commander)
    engine.rootContext().setContextProperty("Exporter", report_exporter)

    qml_file = os.path.join(os.path.dirname(os.path.abspath(__file__)), "qml", "Main.qml")
    print(f"Loading QML from: {qml_file}")

    if not os.path.exists(qml_file):
        print(f"ERROR: QML file does not exist at: {qml_file}")
        sys.exit(1)

    engine.load(qml_file)

    if not engine.rootObjects():
        print("ERROR: Failed to load QML file.")
        sys.exit(1)

    print("QML loaded successfully. Window should be visible.")

    serial_thread.start()
    print("Serial worker thread started.")

    def send_test_ping():
        print("Sending test PING command...")
        serial_commander.sendCommand("PING")

    QTimer.singleShot(2000, send_test_ping)

    exit_code = app.exec()

    serial_thread.quit()
    serial_thread.wait(5000)
    print("Serial worker thread stopped.")

    sys.exit(exit_code)


if __name__ == "__main__":
    main()
