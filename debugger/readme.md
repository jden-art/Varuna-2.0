


---

You are building a fullscreen kiosk-mode diagnostic application for the VARUNA flood monitoring system. This application is developed on a Fedora Linux workstation with a desktop environment for iterative UI work and testing. Once the interface and all functionality are verified and polished on Fedora, the application is deployed directly onto a Raspberry Pi 4 Model B (4GB RAM) running Ubuntu Server with no desktop environment. On the Pi, the UI is built in QML with a PySide6 backend, launched at boot via systemd, rendered directly to the framebuffer using the EGLFS platform plugin. There is no window manager, no compositor, no desktop on the deployment target. The application is the only thing on screen.

During development on Fedora, the application runs under Wayland or X11 — whichever the Fedora desktop session provides — using the default Qt platform plugin. The QML layout, animations, and all visual behaviour must be identical between the development machine and the deployment target. No code path may depend on X11 or Wayland APIs — the application must be platform-agnostic at the code level. The only difference between development and deployment is the `QT_QPA_PLATFORM` environment variable. On Fedora during development, the developer may resize the window freely to test different 16:9 resolutions. On the Pi at deployment, the application runs fullscreen on whatever display is connected.

---

## Display and layout philosophy

The application must be fully responsive across all 16:9 aspect ratio resolutions. The minimum supported resolution is 800×480 (a common embedded touchscreen size). The maximum is 1920×1080 (a full HD monitor the developer uses on Fedora). The application must also render correctly at 1024×600, 1280×720, 1366×768, and 1600×900. Every dimension in the QML layout must be expressed relative to the root window dimensions — use proportional sizing, `anchors`, layout managers, and binding expressions against `Window.width` and `Window.height`. No hardcoded pixel values for layout geometry. The only hardcoded pixel values permitted are minimum tap target sizes (44dp), minimum font sizes (11sp), border widths (1–2px), and icon sizes (which scale with a global `dp` function).

Define a global scaling unit at the root QML level:

```qml
readonly property real dp: Math.max(1, Math.min(Window.width / 1280, Window.height / 720))
```

All dimensions in QML reference this unit. For example, a card that was previously 188px wide becomes `188 * dp`. Font sizes reference a similar `sp` unit that tracks `dp` but can be independently overridden in the settings for accessibility. This means a layout designed at 1280×720 as the reference resolution scales down cleanly to 800×480 and up to 1920×1080 without any element overflowing, overlapping, or becoming illegibly small.

Touch input is the primary input method on the deployment target. There may also be a USB keyboard attached for text entry on configuration screens and during development. All tap targets must be a minimum of 44×44dp. There are no hover states since the deployment target is a touch device — but during development on Fedora with a mouse, a subtle cursor change on interactive elements is acceptable as long as it does not affect the touch experience. All interactive elements have a visual pressed state — a brief opacity reduction to 0.85 with a 100ms ease-out transition, plus a Material-style radial ripple effect emanating from the touch point (see the animation system section below).

The application is a kiosk on deployment — it should never show a window border, title bar, taskbar, or any desktop chrome. The root QML element fills the screen completely. On Fedora during development, the window may have standard window decorations provided by the desktop environment for convenience, but the root QML element still fills the entire client area. There is no way to exit the application from the UI on the deployment target — the systemd service owns the process lifecycle. On Fedora, the developer closes the window normally.

---

## Visual design system — Google Material Design language

The entire application follows the Google Material Design 3 (Material You) visual language. Every screen, every component, every transition must feel like it belongs inside a polished Google application — think Google Home, Google Fi diagnostics, or the Android system settings app. The design principles are:

**Clean hierarchy.** Every screen has a single clear focal point. Information is grouped into clearly delineated cards with generous internal padding (16dp minimum). Cards never touch each other — minimum 8dp gap between all cards. Cards never touch the screen edge — minimum 12dp margin from all screen edges.

**Typographic scale.** Follow the Material type scale strictly. Display Large (28sp) for hero values like water height and battery percentage. Title Medium (18sp) for section labels, card titles, and field names. Body Medium (14sp) for secondary values, descriptions, and table content. Label Small (11sp) for timestamps, the serial log strip, and tertiary information. Font weight: Display values use SemiBold (600). Titles use Medium (500). Body and labels use Regular (400). Never use Bold (700) except for the overall verdict word on Screen 6. Font family: Noto Sans for all proportional text. Noto Sans Mono for all monospaced text (serial logs, values, coordinates, file paths). These are available on both Fedora and Ubuntu Server via the `fonts-noto` and `fonts-noto-mono` packages.

**Colour system — dual theme.** The application supports two themes — dark and light — switchable at runtime from the settings panel. The default on first launch is dark. The active theme is persisted to disk at `/home/varuna/.config/varuna-debugger/settings.json` and restored on next launch.

Dark theme palette:
- Background: `#0f1318` — deep near-black with a cool undertone, not pure black (avoids OLED smearing and gives depth)
- Surface: `#1a1f28` — card backgrounds, panels, input fields
- Surface variant: `#242a35` — alternate row backgrounds, slightly raised surfaces
- Border / outline: `#2e3642` — subtle card borders, dividers
- Primary: `#8ab4f8` — interactive elements, selected states, links, primary buttons (Google Blue, desaturated for dark backgrounds)
- Primary container: `#1a3a5c` — filled button backgrounds, selected card tints
- Secondary: `#81c995` — success states, pass indicators, connected status (Google Green)
- Error: `#f28b82` — failure states, disconnected, critical alerts (Google Red, softened)
- Warning: `#fdd663` — amber/warning states, degraded, needs attention (Google Yellow)
- Tertiary: `#f29b74` — flood-related data, orange accents (Google Orange, softened)
- Info: `#8ab4f8` — same as primary, used for informational badges
- On-background (text primary): `#e8eaed` — high emphasis text on background
- On-surface (text secondary): `#9aa0a6` — medium emphasis text
- On-surface-variant (text muted): `#5f6368` — low emphasis text, disabled states

Light theme palette:
- Background: `#f8f9fa` — Google's standard light grey
- Surface: `#ffffff` — pure white card backgrounds
- Surface variant: `#f1f3f4` — alternate rows, slightly depressed surfaces
- Border / outline: `#dadce0` — Google's standard border grey
- Primary: `#1a73e8` — Google Blue
- Primary container: `#d2e3fc` — light blue tint for filled containers
- Secondary: `#1e8e3e` — Google Green
- Error: `#d93025` — Google Red
- Warning: `#f9ab00` — Google Yellow/Amber
- Tertiary: `#e8710a` — Google Orange
- Info: `#1a73e8` — same as primary
- On-background (text primary): `#202124` — Google's standard dark text
- On-surface (text secondary): `#5f6368` — medium emphasis
- On-surface-variant (text muted): `#9aa0a6` — low emphasis, disabled states

All colour references in QML go through a `Theme` singleton QML object that exposes every colour as a property. Components never hardcode colour values. The `Theme` object has a `dark` boolean property — when toggled, all colour properties update and every bound element transitions smoothly over 200ms using `Behavior on color { ColorAnimation { duration: 200 } }`.

**Elevation and depth.** Cards sit at elevation 1 (subtle shadow: `0dp 1dp 3dp rgba(0,0,0,0.12)` in dark mode, `0dp 1dp 3dp rgba(0,0,0,0.08)` in light mode). Modal overlays (numeric keypad, text keyboard, settings panel) sit at elevation 4 with a stronger shadow and a scrim behind them. The scrim is `rgba(0,0,0,0.5)` in dark mode, `rgba(0,0,0,0.32)` in light mode, and fades in over 200ms.

**Corner radius.** Cards and panels: `12dp` — slightly rounder than strict Material to give a softer, more modern feel similar to Google's recent Pixel UI. Buttons: `20dp` — fully rounded pill shape for primary action buttons, `8dp` for secondary/outlined buttons. Badges and pills: `16dp` — fully rounded. Input fields: `8dp`. The settings panel and overlay sheets: `16dp` on the top-left and top-right corners only (since they attach to the bottom edge), or `16dp` on the bottom-left and bottom-right if they slide down from the top.

**Iconography.** Use simple geometric shapes rendered in QML (circles, rounded rectangles, triangles, checkmarks built from two lines) rather than icon font files or image assets. This avoids font-server dependencies and image-loading issues on EGLFS. A checkmark is two `Rectangle` items rotated and positioned. An X is two rotated rectangles. A signal bar is 4–5 `Rectangle` items of increasing height. A battery icon is a bordered rectangle with a fill rectangle inside. An arrow is a triangle drawn with `Canvas` or three-point `Shape`. Keep all icons monochromatic, tinted with the appropriate semantic colour from the theme.

---

## Animation system

Animations are central to the application feeling modern and Google-like. However, on the Raspberry Pi 4 the GPU (VideoCore VI) is limited, so the animation system must be designed to be globally toggleable without breaking any layout or functionality.

A global `Settings.animationsEnabled` boolean (default `true`, persisted to disk) controls all animations. When animations are disabled, every `Transition`, `Behavior`, `NumberAnimation`, `ColorAnimation`, `PropertyAnimation`, and `ParallelAnimation` in the application completes instantly (duration 0). Implement this by binding every animation's `duration` property to `Settings.animationsEnabled ? <designed_duration> : 0`. This means the application is fully functional with animations off — every state change still happens, just instantly.

When animations are enabled, the following animation standards apply throughout:

- **Screen transitions** in the `StackView`: new screens slide in from the right with a `300ms` ease-out-cubic curve. The outgoing screen slides left and fades to 80% opacity simultaneously. Going back reverses the direction. The push/pop transitions are defined in the `StackView`'s `pushEnter`, `pushExit`, `popEnter`, `popExit` transition properties.
- **Card appearance on screen load**: cards fade in with a staggered delay — the first card appears at 50ms, each subsequent card 40ms later. Each card scales from 0.95 to 1.0 and fades from 0 to 1 over 250ms with an ease-out curve.
- **Value changes** on live data cards: numeric values cross-fade (old value fades out, new value fades in) over 150ms. If the value changes significantly (more than 10% relative change), the card border briefly flashes the appropriate colour (green for improvement, red for degradation) over 400ms.
- **Chart drawing**: new data points are added with a smooth scroll — the entire chart translates left by one data point width over 200ms rather than jumping.
- **Ripple effect on buttons**: Material-style radial ripple from the touch point. A circle expands from the touch coordinates to fill the button bounds over 300ms with ease-out, opacity fading from 0.15 to 0 during the last 100ms of the expansion. Ripple colour is `Theme.primary` at 15% opacity.
- **Settings panel slide**: see the dedicated section below.
- **Theme change**: all colours transition simultaneously over 200ms using `ColorAnimation`. No jarring flash.
- **Progress bars**: animate their width with a 300ms ease-in-out.
- **Status badge transitions**: when a check item changes from waiting to pass/fail, the badge scales from 0.8 to 1.0 with a slight overshoot (elastic ease, overshoot 1.2) over 300ms and the colour fades in simultaneously.
- **Toast notifications**: slide up from the bottom by 60dp, hold for 3 seconds, slide back down. Used for confirmations like "Thresholds applied" or "Report exported."

---

## Settings panel — swipe-down overlay

The application has a settings and quick-controls panel that mirrors the Android notification shade interaction pattern. The user swipes downward from the top 40dp of the screen (a hidden touch zone — the nav bar area) to reveal the panel. The panel slides down from the top edge of the screen, pushing nothing — it overlays on top of the current screen content with a scrim behind it.

**Trigger gesture:** A vertical swipe starting within the top 40dp of the screen, moving downward by at least 30dp, reveals the panel. The panel tracks the finger position during the swipe — it is draggable. Releasing the finger when the panel is more than 30% revealed snaps it fully open (with a 250ms ease-out spring animation). Releasing when less than 30% revealed snaps it closed. Tapping the scrim area below the panel closes it. Swiping up on the panel closes it.

**Panel dimensions:** Full screen width, 55% of screen height. Rounded corners on the bottom-left and bottom-right (16dp radius). Background is `Theme.surface` with elevation 4 shadow. A small drag handle indicator (32dp wide, 4dp tall, rounded pill, `Theme.onSurfaceVariant` colour) is centred at the top of the panel with 8dp top margin, mimicking the Android bottom sheet handle.

**Panel layout:** The panel has a header and a body.

The header is 48dp tall, contains the title "Settings" in Title Medium (18sp) `Theme.onBackground`, left-aligned with 16dp left padding. On the right side of the header, a "Close" text button in `Theme.primary` at 14sp.

The body is a scrollable vertical list of settings items. Each item follows the Material Design list item pattern: 56dp tall, full width, with 16dp horizontal padding. Left side: icon (24dp, built from QML shapes, `Theme.onSurfaceVariant`), 16dp gap, then the label in Body Medium (14sp) `Theme.onBackground` with an optional subtitle below in Label Small (11sp) `Theme.onSurfaceVariant`. Right side: the control widget.

The settings items are:

1. **Dark mode** — Toggle switch (Material-style, 36×20dp track with 16dp diameter thumb). Label: "Dark mode." Subtitle: "Reduce eye strain in low light." Default: on. When toggled, the theme transitions immediately with the 200ms colour animation. The toggle thumb slides with a 150ms ease-out animation.

2. **Animations** — Toggle switch. Label: "Animations." Subtitle: "Disable for better performance on slow hardware." Default: on. When toggled off, all animations globally switch to 0ms duration. When toggled on, they restore to their designed durations. This toggle itself always animates (its own thumb slide is exempt from the global disable).

3. **Font scale** — A segmented button row with three options: "Small" (0.85×), "Default" (1.0×), "Large" (1.15×). Label: "Text size." Subtitle: "Adjust for readability." The segmented button is a row of three pill-shaped segments, the selected one filled with `Theme.primaryContainer` and text in `Theme.primary`, unselected ones outlined. Changing this scales the global `sp` unit and all text in the application reflows instantly.

4. **Screen brightness** — A horizontal slider, Material-style (4dp tall track, 20dp diameter thumb, `Theme.primary` for the active portion, `Theme.onSurfaceVariant` at 30% for the inactive portion). Label: "Brightness." Subtitle: "Adjust display brightness." This writes to `/sys/class/backlight/*/brightness` on the Pi (if available) or does nothing on Fedora. Range: 10% to 100%. On systems where the backlight file is not writable, this item shows "Not available on this display" as the subtitle and the slider is disabled (greyed out).

5. **Serial port override** — A row of three selectable chips: "Auto," "/dev/ttyUSB0," "/dev/ttyACM0," "/dev/ttyS0." Label: "Serial port." Subtitle: "Override auto-detection if needed." Default: "Auto." Chips use the same visual style as the font scale segmented buttons. Selecting a specific port forces the `SerialWorker` to use only that port. Selecting "Auto" restores the auto-detection behaviour.

6. **Data logging** — Toggle switch. Label: "Log raw serial data." Subtitle: "Save all incoming data to a timestamped file." Default: off. When enabled, the `SerialWorker` additionally writes every raw line it receives to `/home/varuna/logs/serial_YYYYMMDD_HHMMSS.log`. This is useful for post-deployment analysis.

7. **Chart history** — A segmented button row with three options: "1 min" (60 samples), "3 min" (180 samples), "5 min" (300 samples). Label: "Chart history." Subtitle: "Duration of rolling chart data." Default: "3 min." Changing this resizes the `deque` maxlen in the `DeviceModel` and clears existing chart data.

8. **About** — Not a toggle, but a tappable list item that expands inline (accordion-style, 200ms ease-out) to show: application version ("VARUNA Debugger v1.0.0"), build date, Python version, PySide6 version, Qt version, platform plugin in use, display resolution, and serial port status. This is useful for field debugging.

All settings are persisted to `/home/varuna/.config/varuna-debugger/settings.json` (or `~/.config/varuna-debugger/settings.json` on Fedora). The settings file is read on launch and written on every change. The `Settings` object is a QML singleton backed by a Python `QObject` registered as a context property.

---

## Navigation system

The application has seven screens. Navigation between them is managed by a `StackView` in `Main.qml`. Screen transitions use the animation standards described above.

The primary navigation method is a bottom navigation bar — not a top nav bar. This follows modern Google Material Design conventions for touch devices where the thumb naturally rests near the bottom of the screen. The top area is reserved for the status strip and the swipe-down settings gesture.

**Top status strip:** 36dp tall, fixed at the top of every screen, background `Theme.surface` with a 1dp bottom border in `Theme.border`. Contains from left to right:
- Application name "VARUNA" in 13sp `Theme.onSurfaceVariant` SemiBold, followed by the screen title in 13sp `Theme.onBackground` Medium, separated by a " / " divider in `Theme.onSurfaceVariant`.
- Spacer.
- Connection indicator: a 6dp diameter circle (green/`Theme.secondary` when connected, red/`Theme.error` when disconnected) with a subtle pulse animation (scale 1.0 to 1.2 to 1.0 over 2 seconds, repeating) when connected. Next to it, "LIVE" in 10sp `Theme.secondary` when connected, or "NO DATA" in 10sp `Theme.error` when disconnected.
- RSSI mini indicator: 4 vertical bars (3dp wide, heights 4/8/12/16dp, 2dp gaps) coloured based on signal level — bars below the threshold are `Theme.onSurfaceVariant` at 30%, bars at or above are `Theme.secondary` (good), `Theme.warning` (medium), or `Theme.error` (poor).
- Session timer in 12sp Noto Sans Mono `Theme.onSurfaceVariant`, format `MM:SS`.
- Screen progress: 7 small circles (6dp diameter, 4dp gaps). Completed screens are filled with `Theme.secondary`. Current screen is filled with `Theme.primary`. Incomplete screens are outlined in `Theme.onSurfaceVariant` at 30%.

**Bottom navigation bar:** 56dp tall, fixed at the bottom of every screen, background `Theme.surface` with a 1dp top border in `Theme.border`. Contains 7 navigation items evenly spaced. Each item is a vertical stack: icon (20dp, built from QML shapes) on top, label in 9sp below. The active item's icon and label are coloured `Theme.primary` and the icon sits on a filled pill-shaped indicator (`Theme.primaryContainer`, 48×28dp, centred behind the icon) — this is the standard Material 3 bottom navigation treatment. Inactive items are `Theme.onSurfaceVariant`. Tapping an item navigates to that screen.

The 7 items are: Boot (icon: shield with check), Live (icon: pulse/waveform), Cal (icon: crosshair/target), Conn (icon: signal bars), Config (icon: sliders), Verdict (icon: clipboard with check), Export (icon: download/arrow-out).

Tapping the current screen's nav item does nothing (no re-push). The `StackView` replaces the current screen — it does not stack indefinitely.

**Bottom serial log strip:** This is repositioned. Instead of a fixed bottom strip, the serial log is now a collapsible mini-strip above the bottom navigation bar, 28dp tall when collapsed (showing only the most recent log entry as a single scrolling line in 10sp Noto Sans Mono), expandable to 120dp on tap to show the last 10 entries in a scrollable list. The expanded state has a `Theme.surface` background with elevation 2. Tapping the expanded log collapses it. The strip has a small expand/collapse chevron icon (12dp) on the right side. Log entries are colour-coded: STATUS in `Theme.onSurfaceVariant`, ERROR in `Theme.error`, WARNING in `Theme.warning`, FLOOD in `Theme.tertiary`.

**Content area:** The area between the top status strip and the bottom serial log strip / bottom nav bar. Its height is `Window.height - 36dp (top strip) - 28dp (collapsed log) - 56dp (bottom nav) = Window.height - 120dp`. All screen content renders in this area. When the log is expanded, the content area does not resize — the log overlays on top of the content from above the bottom nav bar.

---

## What this application is

This is a go/no-go deployment instrument for a field engineer standing at a riverbank. The engineer has 5 to 10 minutes. They plug the Raspberry Pi into the ESP32-S3 via a USB-to-UART cable (or direct UART on GPIO pins 14/15 of the RPi, which maps to `/dev/ttyS0`; or USB serial which appears as `/dev/ttyUSB0` or `/dev/ttyACM0`). The application auto-detects which serial port has data arriving and connects to it at 115200 baud, 8N1.

The ESP32-S3 sends two types of output continuously:

First, a 38-field comma-separated CSV line every 1 second with no header. The fields in order are: theta (degrees), waterHeight (cm), correctedTiltX (degrees), correctedTiltY (degrees), olpLength (cm), horizontalDist (cm), currentPressure (hPa), currentTemperature (°C), baselinePressure (hPa), pressureDeviation (hPa), submersionState (0–3), estimatedDepth (cm), bmpAvailable (0 or 1), unixTime (uint32), dateTimeString (YYYY-MM-DD HH:MM:SS), rtcValid (0 or 1), ratePer15Min (cm/15min), floodAlertLevel (0–3), sessionDuration (seconds), peakHeight (cm), minHeight (cm), latitude (degrees to 6 decimal places), longitude (degrees to 6 decimal places), altitude (metres), gpsSatellites (integer), gpsFixValid (0 or 1), simSignalRSSI (integer 0–31), simRegistered (0 or 1), simAvailable (0 or 1), currentZone (0–3), currentResponseLevel (0–4), sustainedRise (0 or 1), batteryPercent (float), sampleInterval (seconds), transmitInterval (seconds), obLightEnabled (0 or 1), debugEnabled (0 or 1), algorithmEnabled (0 or 1).

Second, named status messages prefixed with `STATUS:`, `ERROR:`, `WARNING:`, or `FLOOD:` that arrive at any time interleaved with the CSV lines. Examples: `STATUS:MPU6050_OK`, `ERROR:MPU6050_NOT_FOUND`, `STATUS:BMP280_OK`, `STATUS:DS1307_OK`, `STATUS:HCSR04_OK`, `STATUS:SIM_READY`, `STATUS:GPS_UART_INIT`, `STATUS:CALIBRATING`, `STATUS:CALIBRATING_GYRO`, `STATUS:CALIBRATING_REFERENCE`, `STATUS:RECALIBRATED_ZERO`, `STATUS:BASELINE_INIT=<value>`, `STATUS:GPRS_CONNECTED`, `STATUS:GPRS_HTTP_CODE=<code>`, `STATUS:GPRS_UPLOAD_OK`, `STATUS:GPRS_UPLOAD_FAIL_COUNT=<n>`.

The Python backend reads the serial port in a dedicated thread, parses every incoming line, and exposes the parsed data to QML via PySide6 properties and signals. The QML layer is pure presentation — it never touches the serial port directly.

---

## Application architecture

The Python backend has the following structure:

A `SerialWorker` class runs in a `QThread`. It opens the serial port, reads lines, and classifies each line as CSV, STATUS, ERROR, WARNING, or FLOOD. It emits signals: `csvReceived(dict)` with the parsed 38-field dictionary, `statusReceived(str, str)` with the prefix and message, and `connectionChanged(bool)` when the port opens or closes. It also handles auto-detection by attempting to open `/dev/ttyUSB0`, `/dev/ttyACM0`, and `/dev/ttyS0` in that order, using the first port that produces a valid CSV line within 3 seconds. When data logging is enabled in settings, it additionally writes every raw line to a timestamped log file.

A `DeviceModel` class inherits `QObject` and is registered as a singleton context property in QML under the name `Device`. It receives signals from `SerialWorker`, maintains all application state, and exposes it to QML via `@Property` decorated attributes and `@Slot` decorated methods. It maintains rolling buffers of CSV readings for the chart data (buffer size controlled by the chart history setting — 60, 180, or 300 samples). It tracks which screens have been completed for the session progress logic. It assembles the deployment report data structure.

A `SerialCommander` class handles sending commands to the device. It has a queue and sends one command at a time, waiting for a STATUS confirmation or timing out after 5 seconds. It exposes a `sendCommand(cmd: str)` slot and emits `commandConfirmed(cmd: str)`, `commandFailed(cmd: str, reason: str)` signals.

A `ReportExporter` class handles PDF and JSON generation. It uses ReportLab for PDF generation. It writes to `/home/varuna/reports/` creating the directory if it does not exist. It exposes an `export(reportData: dict)` slot and emits `exportComplete(pdfPath: str, jsonPath: str)` and `exportFailed(reason: str)`.

A `SettingsManager` class inherits `QObject` and is registered as a singleton context property in QML under the name `Settings`. It reads and writes `/home/varuna/.config/varuna-debugger/settings.json` (falling back to `~/.config/varuna-debugger/settings.json` on non-Pi systems). It exposes all settings as `@Property` attributes with change signals: `darkMode` (bool, default true), `animationsEnabled` (bool, default true), `fontScale` (float, default 1.0, valid values 0.85/1.0/1.15), `brightness` (int, default 100, range 10–100), `serialPortOverride` (str, default "auto"), `dataLoggingEnabled` (bool, default false), `chartHistorySeconds` (int, default 180, valid values 60/180/300). Every property setter writes the settings file to disk immediately after updating the value in memory.

The main Python file creates a `QGuiApplication` with `sys.argv`, sets `QQuickStyle` to `"Material"`, creates a `QQmlApplicationEngine`, registers all backend objects as context properties, loads the root QML file, and calls `app.exec()`. The application does not use `QApplication` — it uses `QGuiApplication` since there are no widgets.

---

## Screen 1 — Boot summary

This screen activates automatically when the application first connects to the serial port. It does not require any user action.

The screen title in the status strip reads "Boot check."

The content area renders a 2×3 grid of check items (two columns, three rows) using a `GridLayout` that sizes proportionally to the available content area. Each item is a card with `Theme.surface` background, elevation 1, `12dp` corner radius. The grid has 10dp row and column spacing. Each card fills its grid cell with 12dp internal padding on all sides.

Inside each card: a large icon area at the top (36dp, either a checkmark circle in `Theme.secondary` or an X circle in `Theme.error` or a spinning indicator in `Theme.primary` if still waiting), the sensor name in 15sp `Theme.onBackground` Medium centred below the icon with 8dp top spacing, and a short status description in 12sp `Theme.onSurfaceVariant` below that with 4dp top spacing.

The six items are MPU6050 (shows "Tilt sensor"), BMP280 (shows "Pressure / temp"), DS1307 (shows "Real-time clock"), HC-SR04 (shows "Ultrasonic backup"), SIM800L (shows "GSM module"), and GPS (shows "Location fix").

Each item starts in a waiting/spinning state. When the corresponding STATUS line arrives — `STATUS:MPU6050_OK` or `ERROR:MPU6050_NOT_FOUND` — the item transitions to its result state. If animations are enabled: the icon scales from 0.85 to 1.0 with a slight overshoot over 250ms, and the card border briefly flashes the result colour (green or red) over 400ms. If animations are disabled: the state change is instant.

Below the grid, centred, a single large action button using the `ActionButton` component: pill-shaped (fully rounded), 70% of content width, 48dp tall. In its default state: `Theme.primary` background, "Continue to live readout" in 15sp white text. If all six items are green: `Theme.secondary` background, label changes to "All systems nominal — Continue." If any item is red: a warning text appears above the button in `Theme.warning` at 12sp: "One or more subsystems failed. Review before deploying." The button is always enabled — the engineer may continue even if some checks failed.

---

## Screen 2 — Live sensor readout

The screen title in the status strip reads "Live sensors."

The content area is divided vertically: the top 55% for the value cards and the bottom 45% for the three charts.

The top section uses a responsive grid layout. On wider screens (above 1024px width) it renders as two rows: four cards on the first row and three cards on the second row. On narrower screens (800–1024px) it renders as a 2×4 grid (two columns, four rows, with the last cell empty). Cards size proportionally to fill the available width with 8dp gaps and 12dp edge margins. Minimum card height is 80dp.

Each value card follows the `ValueCard` component design: `Theme.surface` background, `12dp` radius, elevation 1. Internal layout with 12dp padding: the field name in 12sp `Theme.onSurfaceVariant` Medium at the top-left, the current value in 26sp `Theme.onBackground` SemiBold centred vertically, and a trend indicator or sub-label in 11sp at the bottom-left.

Row 1 cards: **Water height** (cm) with a trend arrow — an upward-pointing triangle in `Theme.error` if rising (current > previous + 0.5), a downward-pointing triangle in `Theme.primary` if falling (current < previous − 0.5), a horizontal dash in `Theme.onSurfaceVariant` if stable; **Tilt angle** (degrees, showing the theta value); **Battery** (%) with a horizontal bar below the value (full card width minus padding, 4dp tall, rounded, filled proportionally and coloured `Theme.secondary` above 80%, `Theme.warning` 20–80%, `Theme.error` below 20%); **SIM RSSI** showing the raw number and a plain-language quality label in the sub-area ("Excellent" / "Good" / "Fair" / "Poor" / "No signal").

Row 2 cards: **Pressure** (hPa); **Temperature** (°C); **GPS** showing "FIX — N sats" in `Theme.secondary` or "NO FIX — N sats" in `Theme.warning`/`Theme.error`, with the coordinate pair in the sub-label in 10sp.

The bottom section contains three `MiniChart` components stacked vertically with 6dp gaps and 12dp horizontal margins. Each chart fills the available width and shares the height equally.

Each `MiniChart` has a `Theme.surfaceVariant` background (slightly darker than surface in dark mode, slightly lighter in light mode for contrast), `8dp` corner radius, `1dp` border in `Theme.border`. Internal padding 8dp on all sides.

Chart 1: Water height over the rolling history period. Y axis auto-scales with 10% padding above and below the data range. The line is `Theme.primary` at 2dp width with rounded line caps. The line is drawn with anti-aliasing. A faint horizontal dashed line in `Theme.warning` at 0.5dp shows the alert threshold if available. Axis labels: the min and max Y values in 9sp `Theme.onSurfaceVariant` at the top-left and bottom-left of the chart area. The chart title "Water Height (cm)" in 9sp `Theme.onSurfaceVariant` at the top-right.

Chart 2: Tilt X (line in `#06b6d4` / a cyan accent) and tilt Y (line in `Theme.tertiary`) on the same axes. Y axis auto-scales. A small legend in the top-right corner: two short coloured line segments with labels in 9sp.

Chart 3: RSSI over the rolling period. The line colour changes segment by segment: `Theme.error` below 10, `Theme.warning` 10–15, `Theme.secondary` above 15. Y axis fixed 0 to 31 with faint grid lines at 10 and 15 in `Theme.border` at 50% opacity. Axis labels at the top-left (31) and bottom-left (0).

All three charts are implemented as QML `Canvas` elements. The Python backend emits a `chartDataUpdated()` signal after appending to the rolling buffers. The `Canvas` `onPaint` handler redraws from the buffer array on each signal. The chart scrolls smoothly if animations are enabled (the paint handler interpolates positions between frames). Do not use any third-party charting library.

---

## Screen 3 — Calibration verification

The screen title in the status strip reads "Calibration."

When this screen becomes active it sends `GETCONFIG` over serial. It also begins the 30-second static stability test automatically.

The content area is divided into two panels side by side on screens wider than 900dp. On narrower screens, the panels stack vertically and each gets 50% of the content height. Panels have `Theme.surface` background, `12dp` radius, elevation 1, 12dp internal padding, 8dp gap between them.

**Left panel — Calibration results.** A vertical list of result rows using the `CheckRow` component. Each row is 48dp tall with a left colour bar (3dp wide, `Theme.secondary` / `Theme.warning` / `Theme.error`), the metric name in 13sp `Theme.onBackground`, the measured value in 13sp Noto Sans Mono `Theme.onBackground`, and a `StatusBadge` on the right.

Rows and thresholds:
- WHO_AM_I register — pass if 0x68 or 0x72, fail otherwise
- Total G magnitude — pass if 0.90–1.10, warn if 0.85–0.90 or 1.10–1.15, fail outside that
- Gyro offset X — pass within ±300 LSB, warn ±300–800, fail above ±800
- Gyro offset Y — same
- Gyro offset Z — same
- Gyro sample count — pass above 900/1000, warn 500–900, fail below 500
- Accel sample count — pass above 450/500, warn 250–450, fail below 250

**Right panel — Static stability test.** A header label in 14sp `Theme.onBackground` Medium: "Static stability test." A subtitle in 12sp `Theme.onSurfaceVariant`: "Keep buoy still on flat surface." Below, a progress bar: full panel width, 8dp tall, `12dp` corner radius, `Theme.surfaceVariant` track, `Theme.primary` fill that animates left to right over 30 seconds. Below the progress bar, a live readout of the current standard deviation in 14sp Noto Sans Mono updating every second. When complete: the final value in 28sp SemiBold coloured `Theme.secondary` / `Theme.warning` / `Theme.error` based on the thresholds (below 0.5°, 0.5–1.5°, above 1.5°). Below the value, interpretation text in 12sp `Theme.onSurfaceVariant`. Below that, a "Rerun test" action button (outlined style, pill shape, `Theme.primary` border and text, 140dp wide, 40dp tall).

---

## Screen 4 — Connectivity and TX/RX test

The screen title in the status strip reads "Connectivity."

The content area is divided into three sections stacked vertically with 8dp gaps. Each section is a card with `Theme.surface` background, `12dp` radius, elevation 1, 12dp internal padding.

**TX/RX section** (top card, flexible height ~30% of content area): The card title "Serial TX/RX" in 14sp `Theme.onBackground` Medium. Below, a row: "Send PING" `ActionButton` (filled, `Theme.primary`, pill shape, 140dp wide, 40dp tall) on the left. To its right, a result area showing the outgoing and incoming bytes in 12sp Noto Sans Mono `Theme.onBackground`. Below the row, a 5-line scrollable area in 10sp Noto Sans Mono showing recent messages, colour-coded by prefix. On successful ping, the card border briefly flashes `Theme.secondary` over 400ms. On timeout, it flashes `Theme.error`.

**GPRS section** (middle card, ~35%): Card title "GPRS / Server." A "Test connection" `ActionButton` (filled, `Theme.primary`, pill shape). To its right, three inline result chips: Status (filled green pill "PASS" or filled red pill "FAIL"), HTTP code (outlined chip), Round-trip ms (outlined chip). Below, a cross-reference line visible only on failure: "Current RSSI: N — <quality>" in 12sp `Theme.warning`. A second button "Reinitialise SIM" (outlined style, disabled/greyed until a GPRS test has run).

**APN section** (bottom card, ~35%): Card title "APN Configuration." The current APN in 18sp Noto Sans Mono `Theme.onBackground`. Below, a reference row of three common Indian APNs as tappable chips: `airtelgprs.com`, `internet` (Jio), `bsnlnet` (BSNL). Each chip is outlined, 36dp tall, `8dp` radius. A note in 11sp `Theme.onSurfaceVariant`: "Verify this matches the SIM card in the device."

---

## Screen 5 — Threshold configuration

The screen title in the status strip reads "Thresholds."

When this screen becomes active it sends `GETTHRESH` and waits. Current values show "Reading..." in `Theme.onSurfaceVariant` until received.

The content area has a header row 28dp tall with column labels "Current on device" and "Set new value" in 12sp `Theme.onSurfaceVariant`.

Below, three configuration rows with 8dp gaps. Each row is a card (`Theme.surface`, `12dp` radius, elevation 1) containing a left half and right half separated by a 1dp vertical divider in `Theme.border`.

Left half: the label in 14sp `Theme.onBackground` (e.g., "Alert threshold") with "cm" unit, and the current device value in 20sp Noto Sans Mono `Theme.onBackground` on `Theme.surfaceVariant` background.

Right half: a numeric input field. `Theme.surfaceVariant` background, `8dp` radius, 1dp border that changes to `Theme.primary` on focus, `Theme.error` on validation failure. Tapping the field opens the `NumericKeypad` overlay.

The `NumericKeypad` is a modal half-sheet overlay, rising from the bottom of the screen (sliding up with a 250ms ease-out animation if animations are enabled). It spans full screen width, 220dp tall, `Theme.surface` background, `16dp` top-left and top-right corner radius, elevation 4, with a scrim behind it. The keypad has a grid of digit buttons (0–9), backspace (icon), decimal point, and a confirm/checkmark button. Buttons are 56dp×48dp, `Theme.surfaceVariant` background, `8dp` radius, with ripple effect on press. The confirm button is `Theme.primary` filled. Above the keypad grid, a display row showing the current input value in 22sp Noto Sans Mono `Theme.onBackground` with a blinking cursor.

Validation is live: alert < warning < danger, all positive integers. Invalid fields get `Theme.error` border, and a 10sp `Theme.error` inline message appears below the row.

Bottom action bar: 48dp tall, 8dp top margin. "Reset to defaults" (outlined pill button, left-aligned) and "Apply to device" (filled pill button, `Theme.primary`, right-aligned, disabled if validation errors exist). On apply: each row gets a spinning indicator on the left, which resolves to a checkmark or X with the badge animation. If any command times out, remaining commands are skipped and a toast notification appears in `Theme.error`: "Configuration incomplete — retry."

---

## Screen 6 — Go / No-Go verdict

The screen title in the status strip reads "Verdict."

The content area is split into two panels side by side on wide screens, stacking vertically (scrollable) on narrow screens. Each panel is a card with `Theme.surface`, `12dp` radius, elevation 1, 12dp internal padding.

**Left panel — Checklist.** Ten `CheckRow` items, each 34dp tall. Left colour bar (4dp wide), check name in 13sp `Theme.onBackground`, `StatusBadge` on the right. Below each name, in 10sp `Theme.onSurfaceVariant`, the specific measured values (e.g., "totalG=0.98, deviation=0.4°").

The ten checks: MPU6050 sensor health, BMP280 sensor health, HC-SR04 sensor health, GPS fix quality, DS1307 RTC validity, SIM and GPRS connectivity, Battery level, Calibration quality, Signal at deployment location, TX/RX channel confirmed.

Below the checklist, the overall verdict banner: full panel width, 48dp tall, `20dp` corner radius. Background and text:
- All pass: `Theme.secondary` background, "DEPLOY" in white 20sp Bold
- Any warn, no fail: `Theme.warning` background, "DEPLOY WITH CAUTION" in `#202124` 18sp Bold
- Any fail: `Theme.error` background, "DO NOT DEPLOY" in white 18sp Bold

The banner has a subtle entrance animation if animations are enabled: it scales from 0.9 to 1.0 with a slight overshoot over 400ms and fades in simultaneously.

**Right panel — Configuration summary.** A vertical list of label/value pairs in alternating `Theme.surface` and `Theme.surfaceVariant` row backgrounds. Each row is 26dp tall. Labels in 11sp `Theme.onSurfaceVariant`, values in 11sp Noto Sans Mono `Theme.onBackground`. Rows: Alert threshold, Warning threshold, Danger threshold, OLP length, HC-SR04 mount height, APN, Server URL (truncated 30 chars with ellipsis), GPS coordinates, Battery at verdict time, Timestamp (IST).

Bottom of the right panel: "Generate deployment report" `ActionButton` (filled, `Theme.primary`, pill shape, full panel width, 44dp tall). Navigates to Screen 7.

---

## Screen 7 — Deployment report export

The screen title in the status strip reads "Export report."

If the session is incomplete (screens 2–5 not all visited), the content area shows a warning card listing the incomplete screens with `Theme.warning` left bar, and the export button is disabled.

If complete, the content area has a top form card and a bottom result area.

**Form card:** `Theme.surface`, `12dp` radius, elevation 1. Two input fields stacked vertically with 8dp gap. Each field: 52dp tall, full card width minus 24dp padding, `Theme.surfaceVariant` background, `8dp` radius, 1dp border (`Theme.border`, turning `Theme.primary` on focus). Label above each field in 12sp `Theme.onSurfaceVariant`. Field 1: "Engineer name." Field 2: "Engineer ID / Badge number." Tapping a field opens the `TextKeypad` overlay.

The `TextKeypad` is a full-width half-sheet overlay, 240dp tall, QWERTY layout. Same visual treatment as the `NumericKeypad`: `Theme.surface` background, `16dp` top corners, elevation 4, scrim behind. Keys are 36dp×40dp with `Theme.surfaceVariant` background, `6dp` radius, ripple effect. Special keys (shift, backspace, space, enter) span multiple columns. A display row at the top shows the current input with blinking cursor.

"Export report" `ActionButton`: full width, 48dp, filled `Theme.primary` when enabled, `Theme.onSurfaceVariant` with `Theme.surfaceVariant` background when disabled. Disabled until both fields have at least one character.

On export: the button is replaced by a progress indicator (circular, `Theme.primary`, 32dp diameter, spinning) for the duration.

On success: two result rows with green checkmarks. Row 1: "PDF saved — <path>" in 12sp Noto Sans Mono. Row 2: "JSON saved — <path>" in 12sp Noto Sans Mono.

Below: the QR code as a 140×140dp `Image` element from a base64 PNG generated in Python. Centred. Below the QR: "Scan to store record offline" in 11sp `Theme.onSurfaceVariant`.

To the right of the QR area: "Upload now" `ActionButton` (outlined, `Theme.primary`, 140dp×44dp). On success: "Uploaded ✓" toast. On failure: "No network — transfer via USB" in `Theme.warning` text, button disabled.

---

## Shared QML components

**`Theme.qml`** — Singleton. Exposes all colour properties. Has a `dark` bool property. All colours are bound to `dark` via ternary expressions. Every colour property has a `Behavior on <color> { ColorAnimation { duration: Settings.animationsEnabled ? 200 : 0 } }`.

**`ValueCard.qml`** — Reusable card for displaying a single sensor value. Properties: `label` (string), `value` (string), `unit` (string), `subLabel` (string), `statusColor` (color, optional, for tinting the value). Internal layout as described in Screen 2.

**`CheckRow.qml`** — A single row in a checklist. Properties: `name` (string), `result` (string: "PASS"/"WARN"/"FAIL"/"WAITING"), `detail` (string), `resultColor` (color). Renders the left colour bar, name, detail, and `StatusBadge`.

**`StatusBadge.qml`** — The pass/warn/fail pill. Properties: `text` (string), `color` (color). Renders a rounded rectangle (pill shape) with the text centred inside. Size adapts to text width with 12dp horizontal padding, height 22dp, corner radius 11dp.

**`NumericKeypad.qml`** — The numeric input overlay. Has a `target` property (the ID of the text field being edited). Emits `confirmed(string value)` and `cancelled()`. Handles its own open/close animation and scrim.

**`TextKeypad.qml`** — The QWERTY text input overlay. Same pattern as `NumericKeypad` but with alphabetic keys, shift toggle, and space bar.

**`MiniChart.qml`** — The Canvas-based rolling chart. Properties: `dataModel` (QVariantList of floats), `lineColor` (color), `secondaryDataModel` (optional, for dual-line charts), `secondaryLineColor` (optional), `yMin` (float, NaN for auto), `yMax` (float, NaN for auto), `thresholdValue` (float, NaN for none), `thresholdColor` (color), `title` (string). Handles all drawing internally with proper scaling, anti-aliasing, and axis labels.

**`ActionButton.qml`** — Styled button. Properties: `text` (string), `filled` (bool, default true), `enabled` (bool, default true), `buttonColor` (color, default `Theme.primary`). Renders either a filled pill or an outlined pill. Has the ripple effect on press, opacity reduction on press state, and disabled styling.

**`SettingsPanel.qml`** — The swipe-down settings overlay. Self-contained component that handles its own gesture detection, animation, scrim, and all settings controls. It is placed in `Main.qml` as a direct child of the root item, above the `StackView`, so it overlays everything.

**`ToastNotification.qml`** — A small notification that slides up from the bottom. Properties: `message` (string), `color` (color). Auto-dismisses after 3 seconds.

---

## QML file structure summary

Root: `Main.qml`
Screens: `BootScreen.qml`, `LiveScreen.qml`, `CalibrationScreen.qml`, `ConnectivityScreen.qml`, `ThresholdScreen.qml`, `VerdictScreen.qml`, `ExportScreen.qml`
Components: `Theme.qml`, `ValueCard.qml`, `CheckRow.qml`, `StatusBadge.qml`, `NumericKeypad.qml`, `TextKeypad.qml`, `MiniChart.qml`, `ActionButton.qml`, `SettingsPanel.qml`, `ToastNotification.qml`, `StatusStrip.qml` (the top bar), `BottomNav.qml` (the bottom navigation bar), `SerialLogStrip.qml` (the collapsible log).

Python: `main.py`, `serial_worker.py`, `device_model.py`, `serial_commander.py`, `report_exporter.py`, `settings_manager.py`.

A `resources.qrc` file bundles all QML and asset files.

---

## Python backend implementation details

The `DeviceModel` exposes the following to QML as `@Property(type, notify=signal)` attributes: all 38 CSV fields by name, `connected` (bool), `sessionDurationSec` (int), `bootChecks` (a `QVariantList` of dicts with keys `name`, `status`, `detail`), `chartWaterHeight` (QVariantList of floats), `chartTiltX` (same), `chartTiltY` (same), `chartRSSI` (same), `screenCompleted` (QVariantList of 7 bools), `thresholdAlert` (float), `thresholdWarning` (float), `thresholdDanger` (float), `currentConfig` (QVariantMap with all GETCONFIG fields), `verdictChecks` (QVariantList of dicts with keys `name`, `result`, `detail`), `overallVerdict` (str: "DEPLOY", "CAUTION", "FAIL"), `calibrationRows` (QVariantList).

The `DeviceModel` exposes the following `@Slot` methods callable from QML: `sendPing()`, `testGprs()`, `reinitSim()`, `applyThresholds(alert: float, warning: float, danger: float)`, `resetThresholds()`, `getConfig()`, `runStabilityTest()`, `generateVerdict()`, `exportReport(engineerName: str, engineerId: str)`.

The `SettingsManager` exposes all settings as `@Property` with notify signals, and `@Slot` setters that persist to disk.

The serial port reading thread never blocks the main thread. All signal emissions from the worker thread to the model use `Qt.QueuedConnection` for thread safety.

The rolling chart buffers are Python `collections.deque` objects with `maxlen` set from `Settings.chartHistorySeconds`. On each CSV parse the new value is appended and the deque automatically discards the oldest. The `chartDataUpdated` signal triggers Canvas repaints.

The boot check list is populated by watching for specific STATUS and ERROR strings during the first 30 seconds after connection. After 30 seconds any check still in "waiting" state is marked as "not detected" in amber.

---

## Report structure

The deployment report JSON has the following top-level structure:

```json
{
  "report_version": "1.0",
  "device_id": "string — extracted from server URL",
  "deployment_timestamp_ist": "string — DD-MMM-YYYY HH:MM:SS IST",
  "deployment_gps": {
    "latitude": float,
    "longitude": float,
    "altitude_m": float,
    "satellites": int,
    "hdop": float
  },
  "engineer": {
    "name": "string",
    "id": "string"
  },
  "session": {
    "duration_seconds": int,
    "screens_completed": [bool x7],
    "connection_port": "string"
  },
  "configuration": {
    "threshold_alert_cm": float,
    "threshold_warning_cm": float,
    "threshold_danger_cm": float,
    "olp_length_cm": float,
    "hcsr04_mount_height_cm": float,
    "apn": "string",
    "server_url": "string"
  },
  "sensor_health": {
    "mpu6050": { "result": "PASS|WARN|FAIL", "who_am_i": "string", "total_g": float, "gyro_offset_x": int, "gyro_offset_y": int, "gyro_offset_z": int, "gyro_samples": int, "accel_samples": int, "static_deviation_deg": float },
    "bmp280": { "result": "PASS|WARN|FAIL", "chip_id": "string", "temperature_c": float, "pressure_hpa": float, "pressure_deviation_hpa": float },
    "hcsr04": { "result": "PASS|WARN|FAIL", "readings_valid": int, "spread_cm": float },
    "gps": { "result": "PASS|WARN|FAIL", "fix_acquired": bool, "max_satellites": int, "best_hdop": float },
    "ds1307": { "result": "PASS|WARN|FAIL", "year_valid": bool, "incrementing": bool },
    "sim_gprs": { "result": "PASS|WARN|FAIL", "registered": bool, "rssi_at_test": int, "gprs_http_code": int, "gprs_round_trip_ms": int, "apn_used": "string" }
  },
  "calibration": {
    "gyro_sample_count": int,
    "accel_sample_count": int,
    "total_g": float,
    "static_deviation_deg": float,
    "result": "PASS|WARN|FAIL"
  },
  "connectivity": {
    "txrx_confirmed": bool,
    "gprs_test_passed": bool,
    "signal_rssi_at_location": int,
    "signal_quality": "string"
  },
  "battery_at_verdict": float,
  "overall_verdict": "DEPLOY|CAUTION|NO-DEPLOY",
  "verdict_reasons": ["string array of specific reasons for non-DEPLOY verdicts"]
}
```

---

## Systemd service (deployment target only)

The systemd unit file for the application on the Raspberry Pi:

```ini
[Unit]
Description=VARUNA Handheld Debugger
After=multi-user.target

[Service]
User=varuna
WorkingDirectory=/home/varuna/debugger
Environment=QT_QPA_PLATFORM=eglfs
Environment=QT_QPA_EGLFS_ROTATION=0
Environment=QSG_RENDER_LOOP=basic
Environment=PYTHONUNBUFFERED=1
ExecStart=/home/varuna/venv/bin/python main.py
Restart=always
RestartSec=3
StandardOutput=journal
StandardError=journal

[Install]
WantedBy=multi-user.target
```

The `QSG_RENDER_LOOP=basic` environment variable is required on the RPi 4 with EGLFS to prevent the threaded render loop from causing GPU synchronisation issues on the VideoCore VI. The `varuna` user must be in the `dialout` group for serial port access and in the `video` and `render` groups for framebuffer access.

On the Fedora development machine, launch the application directly from the terminal without any platform override:

```bash
cd /path/to/debugger
python main.py
```

The application detects the platform at runtime via `QGuiApplication.platformName()` and adjusts behaviour accordingly (e.g., skipping brightness control if the backlight sysfs path does not exist).

---

## Dependencies

Python packages in the virtualenv: `PySide6`, `pyserial`, `reportlab`, `qrcode[pil]`, `Pillow`.

System packages on both Fedora and Ubuntu Server: `libgl1` or `mesa-libGL` (Fedora equivalent), `libgles2` or `mesa-libGLES`, `fonts-noto` / `google-noto-sans-fonts`, `fonts-noto-mono` / `google-noto-sans-mono-fonts`.

On the Pi: Qt must be built with EGLFS support — the PySide6 wheels for `aarch64` include this. On Fedora `x86_64` the standard PySide6 wheels work with the desktop's OpenGL stack.
