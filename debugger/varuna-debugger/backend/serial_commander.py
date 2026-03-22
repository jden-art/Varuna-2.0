# -----------------------------------------------------------------
# File: backend/serial_commander.py
# Phase: Phase 13 (added RECALIBRATE and RESETTHRESH confirmation
#         patterns; added isBusy and pendingCount as notify signals)
# -----------------------------------------------------------------

from collections import deque
from PySide6.QtCore import QObject, Signal, Slot, QTimer, Property


class SerialCommander(QObject):
    """
    Handles sending commands to the ESP32-S3 device over serial.
    Commands are queued and sent one at a time.
    Each command waits for a STATUS confirmation or times out after 5 seconds.
    In simulated mode, commands auto-confirm after 500ms.

    Full command set recognised:
        PING            → waits for PONG
        GETCONFIG       → waits for CONFIG...WHO...
        RECALIBRATE     → waits for CALIBRAT...DONE/COMPLETE/ZERO
        GETTHRESH       → waits for THRESH=...
        SETTHRESH=A,W,D → waits for THRESH...OK
        RESETTHRESH     → waits for THRESH...RESET or THRESH...OK
        SETAPN=<apn>    → waits for APN...OK
        REINITSIM       → waits for SIM...READY or SIM...OK
        TESTGPRS        → waits for GPRS...
    """

    commandQueued    = Signal(str)
    commandSent      = Signal(str)
    commandConfirmed = Signal(str)
    commandFailed    = Signal(str, str)
    busyChanged      = Signal()

    def __init__(self, serial_worker, parent=None):
        super().__init__(parent)
        self._serial_worker = serial_worker
        self._command_queue = deque()
        self._current_command = None
        self._is_waiting = False
        self._worker_ready = False

        self._timeout_timer = QTimer(self)
        self._timeout_timer.setSingleShot(True)
        self._timeout_timer.setInterval(5000)
        self._timeout_timer.timeout.connect(self._on_timeout)

        self._sim_confirm_timer = QTimer(self)
        self._sim_confirm_timer.setSingleShot(True)
        self._sim_confirm_timer.setInterval(500)
        self._sim_confirm_timer.timeout.connect(self._on_sim_confirm)

        self._serial_worker.statusReceived.connect(self._on_status_received)
        self._serial_worker.connectionChanged.connect(self._on_worker_connection_changed)

        print("SerialCommander: Initialized.")

    # ─── Connection tracking ────────────────────────────────────────────────

    @Slot(bool)
    def _on_worker_connection_changed(self, connected):
        self._worker_ready = connected
        if connected:
            print("SerialCommander: Worker is now ready.")
            if len(self._command_queue) > 0 and not self._is_waiting:
                self._send_next()

    # ─── Public API ─────────────────────────────────────────────────────────

    @Slot(str)
    def sendCommand(self, command):
        """Queue a command to be sent to the device."""
        command = command.strip()
        if not command:
            print("SerialCommander: Empty command ignored.")
            return

        self._command_queue.append(command)
        self.commandQueued.emit(command)
        print(f"SerialCommander: Command queued: {command}")

        if not self._is_waiting and self._worker_ready:
            self._send_next()

    @Slot()
    def clearQueue(self):
        """Clear all pending commands."""
        count = len(self._command_queue)
        self._command_queue.clear()
        print(f"SerialCommander: Queue cleared ({count} commands removed).")

    @Property(int, notify=busyChanged)
    def pendingCount(self):
        """Number of commands waiting in the queue (not including in-flight)."""
        return len(self._command_queue)

    @Property(bool, notify=busyChanged)
    def isBusy(self):
        """True if currently waiting for a command response."""
        return self._is_waiting

    # ─── Internal send logic ────────────────────────────────────────────────

    def _send_next(self):
        if len(self._command_queue) == 0:
            self._current_command = None
            self._is_waiting = False
            return

        if not self._worker_ready:
            print("SerialCommander: Worker not ready — command will send on connect.")
            return

        self._current_command = self._command_queue.popleft()
        self._is_waiting = True
        self.busyChanged.emit()

        if self._serial_worker._simulated:
            print(f"SerialCommander: [SIM] Sending command: {self._current_command}")
            self.commandSent.emit(self._current_command)
            # RECALIBRATE takes longer to simulate — give it 1.5 s
            if self._current_command.upper() == "RECALIBRATE":
                self._sim_confirm_timer.setInterval(1500)
            else:
                self._sim_confirm_timer.setInterval(500)
            self._sim_confirm_timer.start()

        elif self._serial_worker._serial_port is not None:
            try:
                cmd_bytes = (self._current_command + "\n").encode("utf-8")
                self._serial_worker._serial_port.write(cmd_bytes)
                self._serial_worker._serial_port.flush()
                print(f"SerialCommander: Sent command: {self._current_command}")
                self.commandSent.emit(self._current_command)
                self._timeout_timer.start()
            except Exception as e:
                print(f"SerialCommander: Write error: {e}")
                self.commandFailed.emit(self._current_command, f"Write error: {e}")
                self._finish_command(success=False)
        else:
            print(f"SerialCommander: No serial port — failing command: {self._current_command}")
            self.commandFailed.emit(self._current_command, "No serial port")
            self._finish_command(success=False)

    def _finish_command(self, success=True):
        self._is_waiting = False
        self._current_command = None
        self.busyChanged.emit()
        self._send_next()

    # ─── Confirmation matching ──────────────────────────────────────────────

    def _on_status_received(self, prefix, message):
        if not self._is_waiting or self._current_command is None:
            return

        cmd_upper = self._current_command.upper()
        msg_upper = message.upper()

        confirmed = False

        if cmd_upper == "PING":
            confirmed = "PONG" in msg_upper

        elif cmd_upper == "GETCONFIG":
            confirmed = "CONFIG" in msg_upper and "WHO" in msg_upper

        elif cmd_upper == "RECALIBRATE":
            # Device must reply with STATUS:CALIBRATE_DONE, STATUS:CALIBRATING_COMPLETE,
            # STATUS:RECALIBRATED_ZERO, or any STATUS:CALIBRAT...OK
            confirmed = ("CALIBRAT" in msg_upper and
                         ("DONE" in msg_upper or "COMPLETE" in msg_upper or
                          "ZERO" in msg_upper or "OK" in msg_upper))

        elif cmd_upper == "GETTHRESH":
            confirmed = "THRESH" in msg_upper and "=" in message

        elif cmd_upper.startswith("SETTHRESH"):
            confirmed = "THRESH" in msg_upper and "OK" in msg_upper

        elif cmd_upper == "RESETTHRESH":
            # Device replies STATUS:THRESH_RESET_OK or STATUS:THRESH_RESET or STATUS:THRESH...OK
            confirmed = ("THRESH" in msg_upper and
                         ("RESET" in msg_upper or "OK" in msg_upper))

        elif cmd_upper.startswith("SETAPN"):
            confirmed = "APN" in msg_upper and "OK" in msg_upper

        elif cmd_upper == "REINITSIM":
            confirmed = "SIM" in msg_upper and ("READY" in msg_upper or "OK" in msg_upper)

        elif cmd_upper == "TESTGPRS":
            confirmed = "GPRS" in msg_upper

        else:
            # Generic fallback: first 4 chars of the command (minus GET/SET prefix)
            # must appear in the message alongside OK
            base = cmd_upper.replace("GET", "").replace("SET", "")[:4]
            if base and base in msg_upper and "OK" in msg_upper:
                confirmed = True

        if confirmed:
            self._timeout_timer.stop()
            self._sim_confirm_timer.stop()
            print(f"SerialCommander: Command confirmed: {self._current_command}")
            self.commandConfirmed.emit(self._current_command)
            self._finish_command(success=True)

    # ─── Timeout / sim-confirm ──────────────────────────────────────────────

    def _on_timeout(self):
        if self._current_command is not None:
            print(f"SerialCommander: Command timed out: {self._current_command}")
            self.commandFailed.emit(self._current_command, "Timeout (5s)")
            self._finish_command(success=False)

    def _on_sim_confirm(self):
        if self._current_command is not None and self._serial_worker._simulated:
            print(f"SerialCommander: [SIM] Command auto-confirmed: {self._current_command}")
            self.commandConfirmed.emit(self._current_command)
            self._finish_command(success=True)
