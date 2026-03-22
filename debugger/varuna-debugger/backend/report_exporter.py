import os
import json
import io
import base64
from datetime import datetime, timezone, timedelta

from PySide6.QtCore import QObject, Signal, Slot, QThread


class ReportExporter(QObject):

    exportStarted = Signal()
    exportComplete = Signal(str, str, str)
    exportFailed = Signal(str)

    def __init__(self, device_model, parent=None):
        super().__init__(parent)
        self._device = device_model
        self._reports_dir = self._resolve_reports_dir()
        print(f"ReportExporter: Reports directory = {self._reports_dir}")
        print("ReportExporter: Initialized.")

    def _resolve_reports_dir(self):
        varuna_path = os.path.expanduser("/home/varuna/reports")
        if os.path.isdir(os.path.dirname(varuna_path)):
            return varuna_path
        return os.path.join(os.path.expanduser("~"), "varuna-reports")

    @Slot(str, str)
    def exportReport(self, engineer_name, engineer_id):
        self.exportStarted.emit()
        print(f"ReportExporter: Starting export for {engineer_name} ({engineer_id})...")

        try:
            os.makedirs(self._reports_dir, exist_ok=True)

            now_ist = datetime.now(timezone(timedelta(hours=5, minutes=30)))
            timestamp_str = now_ist.strftime("%d-%b-%Y %H:%M:%S IST")
            file_timestamp = now_ist.strftime("%Y%m%d_%H%M%S")

            report_data = self._build_report_data(engineer_name, engineer_id, timestamp_str)

            json_filename = f"varuna_report_{file_timestamp}.json"
            json_path = os.path.join(self._reports_dir, json_filename)
            self._write_json(report_data, json_path)
            print(f"ReportExporter: JSON saved to {json_path}")

            pdf_filename = f"varuna_report_{file_timestamp}.pdf"
            pdf_path = os.path.join(self._reports_dir, pdf_filename)
            self._write_pdf(report_data, pdf_path)
            print(f"ReportExporter: PDF saved to {pdf_path}")

            qr_base64 = self._generate_qr_base64(json_path)
            print(f"ReportExporter: QR code generated ({len(qr_base64)} chars base64).")

            self.exportComplete.emit(pdf_path, json_path, qr_base64)
            print("ReportExporter: Export complete.")

        except Exception as e:
            error_msg = f"Export failed: {str(e)}"
            print(f"ReportExporter: {error_msg}")
            self.exportFailed.emit(error_msg)

    def _build_report_data(self, engineer_name, engineer_id, timestamp_str):
        d = self._device

        verdict_checks_raw = d.verdictChecks
        sensor_health = {}
        for vc in verdict_checks_raw:
            key = vc.get("name", "unknown").lower().replace(" ", "_").replace("/", "_")
            sensor_health[key] = {
                "result": vc.get("result", "WAITING"),
                "detail": vc.get("detail", ""),
            }

        report = {
            "report_version": "1.0",
            "device_id": "varuna-field-unit-01",
            "deployment_timestamp_ist": timestamp_str,
            "deployment_gps": {
                "latitude": d.latitude,
                "longitude": d.longitude,
                "altitude_m": d.altitude,
                "satellites": d.gpsSatellites,
                "fix_valid": bool(d.gpsFixValid),
            },
            "engineer": {
                "name": engineer_name,
                "id": engineer_id,
            },
            "session": {
                "duration_seconds": d.sessionDuration,
                "screens_completed": d.screensCompleted,
                "connection_status": "connected" if d.connected else "disconnected",
            },
            "configuration": {
                "threshold_alert_cm": d.thresholdAlert,
                "threshold_warning_cm": d.thresholdWarning,
                "threshold_danger_cm": d.thresholdDanger,
                "olp_length_cm": d.olpLength,
                "hcsr04_mount_height_cm": d.horizontalDist,
                "apn": "airtelgprs.com",
                "server_url": "http://varuna-server.in/api/upload",
            },
            "sensor_health": sensor_health,
            "calibration": {
                "who_am_i": d.calWhoAmI,
                "total_g": d.calTotalG,
                "gyro_offset_x": d.calGyroOffsetX,
                "gyro_offset_y": d.calGyroOffsetY,
                "gyro_offset_z": d.calGyroOffsetZ,
                "gyro_samples": d.calGyroSamples,
                "accel_samples": d.calAccelSamples,
                "data_received": d.calDataReceived,
            },
            "connectivity": {
                "txrx_confirmed": d.txrxConfirmed,
                "gprs_test_passed": d.gprsTestPassed,
                "gprs_http_code": d.gprsHttpCode,
                "gprs_rtt_ms": d.gprsRttMs,
                "signal_rssi_at_location": d.simSignalRSSI,
                "signal_quality": d.rssiQuality,
            },
            "battery_at_verdict": d.batteryPercent,
            "overall_verdict": d.overallVerdict,
            "verdict_reasons": d.verdictReasons,
        }

        return report

    def _write_json(self, report_data, path):
        with open(path, "w", encoding="utf-8") as f:
            json.dump(report_data, f, indent=2, ensure_ascii=False)

    def _write_pdf(self, report_data, path):
        try:
            from reportlab.lib.pagesizes import A4
            from reportlab.lib.units import mm
            from reportlab.lib.colors import HexColor
            from reportlab.platypus import SimpleDocTemplate, Paragraph, Spacer, Table, TableStyle
            from reportlab.lib.styles import getSampleStyleSheet, ParagraphStyle
            from reportlab.lib.enums import TA_CENTER, TA_LEFT, TA_RIGHT

            doc = SimpleDocTemplate(path, pagesize=A4,
                                    leftMargin=20 * mm, rightMargin=20 * mm,
                                    topMargin=15 * mm, bottomMargin=15 * mm)

            styles = getSampleStyleSheet()

            title_style = ParagraphStyle(
                "VarunaTitle", parent=styles["Title"],
                fontSize=22, textColor=HexColor("#1a73e8"),
                spaceAfter=6 * mm,
            )

            heading_style = ParagraphStyle(
                "VarunaHeading", parent=styles["Heading2"],
                fontSize=13, textColor=HexColor("#202124"),
                spaceBefore=6 * mm, spaceAfter=3 * mm,
            )

            body_style = ParagraphStyle(
                "VarunaBody", parent=styles["Normal"],
                fontSize=10, textColor=HexColor("#5f6368"),
                spaceAfter=2 * mm,
            )

            verdict_text = report_data.get("overall_verdict", "UNKNOWN")
            if verdict_text == "DEPLOY":
                verdict_color = HexColor("#1e8e3e")
            elif verdict_text == "CAUTION":
                verdict_color = HexColor("#f9ab00")
                verdict_text = "DEPLOY WITH CAUTION"
            else:
                verdict_color = HexColor("#d93025")
                verdict_text = "DO NOT DEPLOY"

            verdict_style = ParagraphStyle(
                "VarunaVerdict", parent=styles["Title"],
                fontSize=20, textColor=verdict_color,
                alignment=TA_CENTER, spaceBefore=4 * mm, spaceAfter=6 * mm,
            )

            elements = []

            elements.append(Paragraph("VARUNA Deployment Report", title_style))
            elements.append(Paragraph(
                f"Generated: {report_data.get('deployment_timestamp_ist', 'N/A')}", body_style))
            elements.append(Spacer(1, 3 * mm))

            elements.append(Paragraph(f"Overall Verdict: {verdict_text}", verdict_style))

            elements.append(Paragraph("Engineer Information", heading_style))
            eng = report_data.get("engineer", {})
            elements.append(Paragraph(f"Name: {eng.get('name', 'N/A')}", body_style))
            elements.append(Paragraph(f"ID: {eng.get('id', 'N/A')}", body_style))

            elements.append(Paragraph("Deployment Location", heading_style))
            gps = report_data.get("deployment_gps", {})
            elements.append(Paragraph(
                f"Lat: {gps.get('latitude', 0):.6f}, Lon: {gps.get('longitude', 0):.6f}, "
                f"Alt: {gps.get('altitude_m', 0):.1f}m, Sats: {gps.get('satellites', 0)}", body_style))

            elements.append(Paragraph("Configuration", heading_style))
            cfg = report_data.get("configuration", {})
            config_data = [
                ["Parameter", "Value"],
                ["Alert threshold", f"{cfg.get('threshold_alert_cm', 0):.1f} cm"],
                ["Warning threshold", f"{cfg.get('threshold_warning_cm', 0):.1f} cm"],
                ["Danger threshold", f"{cfg.get('threshold_danger_cm', 0):.1f} cm"],
                ["OLP length", f"{cfg.get('olp_length_cm', 0):.1f} cm"],
                ["APN", cfg.get("apn", "N/A")],
            ]
            config_table = Table(config_data, colWidths=[80 * mm, 80 * mm])
            config_table.setStyle(TableStyle([
                ("BACKGROUND", (0, 0), (-1, 0), HexColor("#e8eaed")),
                ("TEXTCOLOR", (0, 0), (-1, 0), HexColor("#202124")),
                ("FONTSIZE", (0, 0), (-1, -1), 9),
                ("GRID", (0, 0), (-1, -1), 0.5, HexColor("#dadce0")),
                ("ROWBACKGROUNDS", (0, 1), (-1, -1), [HexColor("#ffffff"), HexColor("#f8f9fa")]),
                ("VALIGN", (0, 0), (-1, -1), "MIDDLE"),
                ("LEFTPADDING", (0, 0), (-1, -1), 6),
                ("RIGHTPADDING", (0, 0), (-1, -1), 6),
                ("TOPPADDING", (0, 0), (-1, -1), 4),
                ("BOTTOMPADDING", (0, 0), (-1, -1), 4),
            ]))
            elements.append(config_table)

            elements.append(Paragraph("Sensor Health & Checks", heading_style))
            health = report_data.get("sensor_health", {})
            health_data = [["Check", "Result", "Detail"]]
            for key, val in health.items():
                display_name = key.replace("_", " ").title()
                health_data.append([display_name, val.get("result", "N/A"), val.get("detail", "")])

            health_table = Table(health_data, colWidths=[50 * mm, 25 * mm, 85 * mm])
            health_table.setStyle(TableStyle([
                ("BACKGROUND", (0, 0), (-1, 0), HexColor("#e8eaed")),
                ("TEXTCOLOR", (0, 0), (-1, 0), HexColor("#202124")),
                ("FONTSIZE", (0, 0), (-1, -1), 8),
                ("GRID", (0, 0), (-1, -1), 0.5, HexColor("#dadce0")),
                ("ROWBACKGROUNDS", (0, 1), (-1, -1), [HexColor("#ffffff"), HexColor("#f8f9fa")]),
                ("VALIGN", (0, 0), (-1, -1), "MIDDLE"),
                ("LEFTPADDING", (0, 0), (-1, -1), 4),
                ("RIGHTPADDING", (0, 0), (-1, -1), 4),
                ("TOPPADDING", (0, 0), (-1, -1), 3),
                ("BOTTOMPADDING", (0, 0), (-1, -1), 3),
            ]))
            elements.append(health_table)

            elements.append(Paragraph("Connectivity", heading_style))
            conn = report_data.get("connectivity", {})
            elements.append(Paragraph(
                f"TX/RX: {'Confirmed' if conn.get('txrx_confirmed') else 'Not tested'}, "
                f"GPRS: {'PASS' if conn.get('gprs_test_passed') else 'FAIL/Not tested'}, "
                f"RSSI: {conn.get('signal_rssi_at_location', 0)} ({conn.get('signal_quality', 'N/A')})",
                body_style))

            elements.append(Paragraph(
                f"Battery at verdict: {report_data.get('battery_at_verdict', 0):.1f}%", body_style))

            vr = report_data.get("verdict_reasons", [])
            if vr:
                elements.append(Paragraph("Verdict Reasons", heading_style))
                for reason in vr:
                    elements.append(Paragraph(f"\u2022 {reason}", body_style))

            doc.build(elements)
            print(f"ReportExporter: PDF built successfully at {path}")

        except ImportError as e:
            print(f"ReportExporter: reportlab not available ({e}), writing placeholder PDF.")
            with open(path, "w", encoding="utf-8") as f:
                f.write(f"VARUNA Deployment Report\n")
                f.write(f"========================\n\n")
                f.write(f"Verdict: {report_data.get('overall_verdict', 'N/A')}\n")
                f.write(f"Engineer: {report_data.get('engineer', {}).get('name', 'N/A')}\n")
                f.write(f"Timestamp: {report_data.get('deployment_timestamp_ist', 'N/A')}\n")
                f.write(f"\n(Install reportlab for proper PDF: pip install reportlab)\n")

    def _generate_qr_base64(self, data_to_encode):
        try:
            import qrcode
            from PIL import Image

            qr = qrcode.QRCode(
                version=None,
                error_correction=qrcode.constants.ERROR_CORRECT_M,
                box_size=8,
                border=2,
            )
            qr.add_data(data_to_encode)
            qr.make(fit=True)

            img = qr.make_image(fill_color="black", back_color="white")

            if not isinstance(img, Image.Image):
                img = img.get_image()

            buffer = io.BytesIO()
            img.save(buffer, format="PNG")
            buffer.seek(0)
            b64 = base64.b64encode(buffer.getvalue()).decode("ascii")
            return b64

        except ImportError as e:
            print(f"ReportExporter: qrcode/Pillow not available ({e}), generating placeholder.")
            return self._generate_placeholder_qr_base64()

    def _generate_placeholder_qr_base64(self):
        try:
            from PIL import Image, ImageDraw

            img = Image.new("RGB", (140, 140), "white")
            draw = ImageDraw.Draw(img)
            draw.rectangle([10, 10, 130, 130], outline="black", width=2)
            draw.text((35, 55), "QR CODE", fill="black")
            draw.text((25, 75), "(install qrcode)", fill="gray")

            buffer = io.BytesIO()
            img.save(buffer, format="PNG")
            buffer.seek(0)
            return base64.b64encode(buffer.getvalue()).decode("ascii")

        except ImportError:
            print("ReportExporter: Pillow not available either, returning empty QR.")
            return ""
