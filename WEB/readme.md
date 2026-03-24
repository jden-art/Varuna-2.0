

## PROMPT

You are recreating a production-grade, single-file HTML dashboard called **Project Saraswati — National Flood Early Warning System**. This is a real-time IoT monitoring web app for a river water-level sensor network (VARUNA) built on ESP32-S3 + ESP32-C3, connecting to Firebase Realtime Database.

The entire app — HTML, CSS, JavaScript — must be in one `.html` file. No external files except CDN libraries.

---

### EXTERNAL DEPENDENCIES (load these exactly)

```html
<link href="https://fonts.googleapis.com/css2?family=Inter:wght@300;400;500;600;700;800;900&family=JetBrains+Mono:wght@400;500;600;700&display=swap" rel="stylesheet">
<link rel="stylesheet" href="https://unpkg.com/leaflet@1.9.4/dist/leaflet.css"/>
```

Load these scripts in `<body>` before the closing tag:
```html
<script src="https://unpkg.com/leaflet@1.9.4/dist/leaflet.js"></script>
<script src="https://cdn.jsdelivr.net/npm/chart.js@4.4.0/dist/chart.umd.min.js"></script>
<script src="https://cdn.sheetjs.com/xlsx-0.20.1/package/dist/xlsx.full.min.js"></script>
```

---

### FIREBASE INTEGRATION

Use Firebase SDK v10.12.2 as an ES module in a `<script type="module">` tag in `<head>`. Connect to:
- `apiKey: "AIzaSyBzpgemPlO1eG5SRNuXaOVQDECMqY5bDts"`
- `authDomain: "varuna-flood.firebaseapp.com"`
- `databaseURL: "https://varuna-flood-default-rtdb.asia-southeast1.firebasedatabase.app"`
- `projectId: "varuna-flood"`
- Listen on path `varuna/live` using `onValue`
- Store result in `window._fbLiveData`
- Call `window.onFirebaseLive(val)` if that function is defined
- Call `window.onFirebaseError(err)` on error

---

### CSS DESIGN SYSTEM

**CSS variables (define on `:root`):**
```
--bg0:#030712  --bg1:#0a0f1e  --bg2:#111827  --bg3:#1a2332  --bg4:#1f2937
--surface:#0f1629  --surface2:#151d2e
--accent:#3b82f6  --accent2:#06b6d4  --accent3:#8b5cf6
--green:#10b981  --green2:#34d399  --amber:#f59e0b  --red:#ef4444  --orange:#f97316
--text:#f1f5f9  --text2:#94a3b8  --text3:#64748b  --text4:#475569
--border:#1e293b  --border2:#2d3a4f
--mono:'JetBrains Mono',monospace  --sans:'Inter',system-ui,sans-serif
--radius:12px  --radius-sm:8px  --radius-xs:6px
--transition-fast:0.15s ease  --transition-normal:0.25s ease
```

**Global reset:** `box-sizing:border-box`, zero margins/padding, `overflow-x:hidden`, custom 6px scrollbar using border colors.

**Keyframe animations to define:**
- `blink`: opacity 1→0.25→1 at 50% (for status dots)
- `fadeUp`: translateY(28px)→0 + opacity 0→1
- `fadeRight`: translateX(-20px)→0 + opacity 0→1
- `nodeOnline`: green pulse ring animation (box-shadow expand/fade)
- `nodeDanger`: red pulse ring, `nodeWarn`: amber pulse ring
- `scanMove`: top -2px → 100% (hero scan line)
- `tickerScroll`: translateX(0) → translateX(-50%) (news ticker)
- `shimmer`: background-position slide
- `slideDown`, `slideIn`, `countUp`
- `pulse-badge`: for notification badge

---

### PAGE ARCHITECTURE

The page has **two top-level states** toggled by JavaScript:
- `#landingPage` (default: `display:block`) — marketing/info site
- `#dashboardPage` (default: `display:none`) — operational dashboard

`showDashboard()` hides landing, shows dashboard.
`showLanding()` does the reverse.

---

### LANDING PAGE — Full Structure

#### 1. Government Authority Strip (`.l-govstrip`)
- `position:fixed; top:0; height:40px; z-index:300`
- Background: `#06111f`, bottom border: `2px solid #b45309`
- Left side: Three `.l-govstrip-org` items with inline SVG icons:
  - "Ministry of Jal Shakti" (circle/sun icon)
  - "National Disaster Management Authority" (house icon)
  - "Central Water Commission" (eye icon)
  - Each separated by `border-right: 1px solid rgba(255,255,255,.08)`
- Right side: "NETWORK OPERATIONAL" in green with blinking dot + IST clock (updates every 30s via `toLocaleTimeString` with `timeZone:'Asia/Kolkata'`)

#### 2. News Ticker (`.l-ticker`)
- `position:fixed; top:40px; height:28px; z-index:299`
- Blue label pill "NETWORK" on left
- Scrolling `.l-ticker-track` with `animation:tickerScroll 60s linear infinite`
- Pauses on hover
- 8 ticker items (duplicated for seamless loop), each with a colored 4px dot:
  - `ti-ok` (green): "Ganga Basin — 247 nodes online"
  - `ti-warn` (amber): "Brahmaputra Corridor — 3 nodes ALERT"
  - `ti-ok`: "Krishna–Godavari — 189 nodes nominal"
  - `ti-crit` (red): "Kosi River Zone — 1 node DANGER · SMS dispatched"
  - `ti-ok`: "Mahanadi Basin — 134 nodes online"
  - `ti-warn`: "Luni Basin, Rajasthan — 2 nodes ALERT"
  - `ti-ok`: "Narmada Corridor — 98 nodes nominal"
  - `ti-ok`: "Cauvery–Tungabhadra — 156 nodes online"

#### 3. Main Navigation (`.l-nav`)
- `position:fixed; top:68px; height:56px; z-index:200`
- Background: `rgba(3,7,18,.96)` with `backdrop-filter:blur(24px)`
- Left: Logo button (water drop SVG with `#60a5fa` stroke, gradient `#0c2a5e→#1d4ed8` background mark) — "Project Saraswati" bold, sub-label "NATIONAL FLOOD EARLY WARNING NETWORK"
- Clicking logo calls `showDashboard()`
- Nav links: Challenge, Network Scale, Capabilities, Deployment, Interoperability (anchor links) + "Access Dashboard →" CTA button (blue, calls `showDashboard()`)
- Hamburger button (hidden by default, shown on mobile) — toggles nav links to `flex-direction:column` overlay

#### 4. Hero Section (`.l-hero`)
- `min-height:100vh; padding:200px 40px 100px`
- Background: `#030712` with three overlays: grid (64px blue lines at `.018` opacity), radial vignette, and animated scan line
- Grid layout: `1fr 500px` with 64px gap

**Hero Left:**
- Classification badge: amber border box reading "Autonomous Hydro-Meteorological Sensor Network · 1,000–5,000 Node Scale"
- `<h1>` with three spans:
  - `.h1-pre` block: "Project" (tiny, uppercase, `#1e3a5f`, letter-spacing 5px)
  - `.h1-name` block: "Saraswati" (gradient `#bfdbfe→#60a5fa→#38bdf8→#93c5fd`, `-webkit-text-fill-color:transparent`)
  - `.h1-tagline` block: "National Flood Early Warning Network" (small, uppercase, `#1e3a5f`)
- Description paragraph (border-left: `2px solid #1d4ed8`, color `#334155`): "A government-grade, nationally deployable network of autonomous flood monitoring nodes. From a 50-node district pilot to a 5,000-node national grid..."
- 4-cell KPI grid (1px gap, `rgba(255,255,255,.06)` background separator):
  - "1,000–5,000" / Sensor Nodes
  - "45min+" / Warning Lead
  - "<2s" / Cloud Latency
  - "24/7" / Autonomous
- Two buttons: "Access Node Dashboard" (primary blue, calls `showDashboard()`) + "Deployment Playbook" (outline, anchor `#deployment`)

**Hero Right (Network Map Panel):**
- Panel: dark `rgba(6,17,31,.95)` background, border `rgba(255,255,255,.07)`, `fadeRight` animation
- Header: "Saraswati Node Network — India" + "ILLUSTRATIVE" green dot badge
- `<canvas id="networkMapCanvas" width="500" height="240">` — animated India network map (see canvas animation below)
- Legend row: Normal (green), Alert (amber), Warning (orange), Danger (red), Offline (dark)
- 3-stat row: `id="nmpOnline"` (green), `id="nmpAlert"` (amber), `id="nmpDanger"` (red)
- Zone distribution bars: 4 rows (NORMAL/ALERT/WARNING/DANGER) each with label, bar, percentage (ids: `zb1`–`zb4`, `zp1`–`zp4`)

**Canvas Network Map Animation:**
Use `requestAnimationFrame`. Draw on a 500×240 canvas:
1. Fill background `#04090f`
2. Draw 0.5px blue grid lines every 32px at `.04` opacity
3. Draw 4 river polylines as bezier paths (relative coordinates mapped to canvas W×H):
   - Ganga: `[.18,.12]→[.24,.22]→[.30,.32]→[.38,.40]→[.44,.46]→[.52,.48]→[.60,.45]→[.68,.42]` at `rgba(29,78,216,.18)`
   - Brahmaputra: `[.55,.10]→[.58,.20]→[.60,.30]→[.62,.40]→[.60,.50]→[.58,.60]→[.55,.70]` at `.14`
   - Krishna-Godavari: `[.20,.55]→[.28,.58]→[.36,.60]→[.44,.58]→[.52,.56]→[.60,.55]` at `.12`
   - South: `[.25,.65]→[.30,.70]→[.36,.74]→[.44,.72]→[.50,.68]` at `.10`
4. Connect nodes within 48px distance with `rgba(29,78,216,.08)` lines
5. Draw pulsing red rings on danger nodes (`zone===3`)
6. Draw glowing colored dots for all nodes (radial gradient glow + 3px dot center). Zone colors: green/amber/orange/red/`#1e293b`

**Node array** (format `[relX, relY, zone]`, zone 0=normal 1=alert 2=warning 3=danger 4=offline): Define ~60 nodes spread across India representing Ganga basin, Brahmaputra, Northeast coast, Mahanadi, Krishna-Godavari, West coast, Indus/Northwest, Rajasthan, and South. Include: 2 alert nodes, 1 warning node, 1 danger node, 1 offline node for realism.

After canvas init, compute zone counts, set `nmpOnline`, `nmpAlert`, `nmpDanger` text content, and animate zone bars after 300ms timeout.

#### 5. Impact Strip (`.l-impact-strip`)
- Full-width band between hero and challenge section
- 5 cells with large mono numbers and labels:
  - "33M+" / Displaced Annually / "Indians affected by floods each monsoon"
  - "412" / High-Risk Districts / "NDMA-identified — only 30% currently monitored"
  - "<30min" / Legacy Warning Time / "Manual gauging station lead time"
  - "45min+" / Saraswati Lead Time / "On-device edge classification, no cloud needed"
  - "₹1.8LCr" / Annual Flood Damage / "Estimated economic losses, India"

#### 6. Challenge Section (`#challenge`)
- Section label: "The National Infrastructure Gap"
- H2: "India's Flood Warning Gap Is a Solvable Engineering Problem"
- Description paragraph
- 4-card gap grid (`.l-gap-grid`):
  - "412" / Districts at High Risk / NDMA-identified, 8–12 nodes each
  - "288" / Districts with Zero Automation / highlighted card
  - "82%" / Deaths in Unmonitored Areas / highlighted card
  - "5,000" / Target Network Size / Phase 3, 12–18 months

#### 7. Network Architecture Section (`#network`)
- H2: "One Architecture. Scales from 50 to 5,000 Nodes."
- Two-column layout:

**Left — Phased Deployment:**
- 3 phase items (badge + title + description + meta tags):
  - Phase 1: "District Pilot — 50–200 Nodes" | 0–6 months | 2–3 districts
  - Phase 2: "State Rollout — 500–1,000 Nodes" | 6–12 months | Full state
  - Phase 3: "National Grid — 1,000–5,000 Nodes" | 12–18 months | 412 districts

**Right — Network Topology Diagram:**
Static visual showing 3 layers:
- Layer 01: Edge Nodes (×5,000) — a 4×4 grid of 16 animated `.l-node-dot` divs with statuses (12 ok, 2 warn, 1 crit, 1 off), each with a `.l-node-core` inner div
- Arrow: "↓ 38-field CSV telemetry at 1 Hz over WiFi / GSM ↓"
- Layer 02: Cloud block — cloud SVG + "Firebase RTDB · Asia Southeast-1 · 99.9% SLA · Sub-2s latency · Horizontal scaling"
- Arrow: "↓ Real-time WebSocket · Role-based access control ↓"
- Layer 03: 3 dashboard blocks — District, State, National (highlighted)
- Sublabel: "SDMA · NDMA · CWC · IMD · NDRF · Revenue Dept · District Collector"

Node dot animations: `.ok` uses `nodeOnline` animation, `.warn` uses `nodeWarn`, `.crit` uses `nodeDanger`.

#### 8. Capabilities Section (`#capabilities`)
- H2: "What Every Node in the Network Must Do"
- 8-card grid (`.l-cap-grid`), each card has numbered badge, icon circle, h3, description, tech tag:
  1. Real-Time Water Level — HC-SR04, blue icon, "HC-SR04 · 2–400 cm · ±1 cm · 1 Hz"
  2. On-Device Flood Classification — lightning icon, red, "4-tier: Normal/Alert/Warning/Danger · Offline-capable · 45 min+ lead"
  3. Multi-Channel Community Alerting — bell icon, green, "SIM800L · SMS + Voice · SDMA + Revenue Dept routing"
  4. GPS-Georeferenced Telemetry — send/arrow icon, purple, "NEO GPS · <2.5m CEP · 10 Hz · NMEA 0183"
  5. 38-Parameter Telemetry Stream — layers icon, cyan, "38 fields · 1 Hz · Firebase RTDB · CSV/Excel export"
  6. Structural Integrity (IMU) — box icon, amber, "MPU6050 · 6-axis · ±250°/s · ±2g · Tilt + displacement alerts"
  7. Remote OTA Firmware — card/server icon, orange, "Web Serial API · .bin push · All nodes simultaneously"
  8. Dual-Link Connectivity — globe icon, green, "WiFi 2.4GHz + GSM/GPRS fallback · SMS offline mode"

#### 9. Deployment Section (`#deployment`)
- H2: "From Factory to Field in Under 72 Hours"
- 4 numbered steps (`.l-steps`):
  1. "Site Survey & Node Registration" — District Engineer / SDMA Technical Cell
  2. "Physical Installation" — <2 hours · Tools: Standard installation kit
  3. "Commissioning & Calibration" — Dashboard commissioning checklist · <30 min
  4. "Alert Routing & Integration" — SDMA · NDMA · Revenue Dept · NDRF
- 3 "after" cards: "Remote Network Health" + "Centralised OTA Firmware" + "Automated Archiving & Reporting"

#### 10. Interoperability Section (`#interop`)
- H2: "Plugs Into India's Existing Disaster Management Ecosystem"
- Two-column layout:

**Left — Integration list** (5 rows with left accent color bar):
- NDMA (blue): NDRRP + IDRN compatibility
- CWC (green): FloodWatch CSV compatible with WRIS
- IMD (purple): BMP280 surface observation network
- SDMAs (amber): Per-state dashboard tier
- NDRF (red): Critical zone mapping for pre-positioning
- CAP v1.2 (cyan): Doordarshan Emergency Warning, All India Radio

**Right — Hardware Spec table** (`.l-spec-wrapper`):
13 spec rows (key/value pairs):
- Microcontroller: ESP32-S3 (master) + ESP32-C3 XIAO (co-processor)
- Water Level: HC-SR04 · 2–400 cm · ±1 cm precision
- IMU / Tilt: MPU6050 · 6-axis · ±250°/s gyro · ±2g accel
- Env. Sensing: BMP280 · 300–1100 hPa · −40 to +85°C · ±1°C
- Cellular Module: SIM800L · Quad-band 850/900/1800/1900 MHz GSM/GPRS
- GNSS: NEO series · <2.5m CEP · 10 Hz · NMEA 0183
- Sample Rate: 1 Hz · 38-field CSV packet · configurable via OTA
- Cloud Backend: Firebase RTDB · Asia SE-1 · 99.9% SLA · sub-2s latency
- Power: Solar + LiPo backup · 72h offline autonomy (target)
- Primary Comms: WiFi 2.4GHz · automatic GSM/GPRS failover
- Flood Algorithm: 4-tier on-device · no cloud dependency for alerts
- Firmware Updates: OTA via central dashboard · .bin · Web Serial API
- Network Scale: 1,000–5,000 nodes per deployment · horizontal scaling

#### 11. CTA Banner (`.l-cta`)
- Tag: "System Live — Node Dashboard Operational"
- H2: "Ready for District-Level Pilot"
- Two buttons: "Launch Operational Dashboard" (primary) + "View Deployment Playbook" (outline)

#### 12. Footer (`.l-footer`)
- Brand: water drop SVG + "Project Saraswati" + divider + copyright text
- "National Flood Early Warning Network · VARUNA Platform v2.0 · © 2025 · Ministry of Jal Shakti, GoI"
- Links: Challenge, Network, Capabilities, Deployment, Dashboard button

---

### DASHBOARD PAGE — Full Structure

The dashboard uses a CSS grid layout:
- `d-topbar` (fixed header, 52px)
- `d-sidebar` (left nav, 220px)
- `d-main` (scrollable content area)

#### Dashboard Topbar (`.d-topbar`)
- Left: hamburger sidebar toggle, logo button ("Saraswati" with water drop SVG, calls `showLanding()`), divider, Firebase status dot + text (`id="fbStatusText"`)
- Right: clock (`id="topClock"`, updates every second), risk badge (`id="topRisk"`, classes: `normal`/`alert`/`warning`/`danger`/`critical`), "Back to Site" button

#### Dashboard Sidebar (`.d-sidebar`)
Sections:

**Monitoring:**
- Overview (active by default) — circle/target icon, `data-panel="overview"`
- Map Center — map polygon icon, `data-panel="mapview"`
- Analytics — bar chart icon, `data-panel="analytics"`

**Management:**
- Alert Center — triangle-warning icon, `data-panel="alerts"`
- Nodes & Devices — grid icon, `data-panel="nodes"`
- Device Config — gear icon, `data-panel="config"`

**Developer:**
- Console — terminal chevron icon, `data-panel="console"`

Sidebar footer: 4 sys-stat rows: Firebase status, Last Sync, Push Count, C3 Uptime.

#### Panel System
Only one `.d-panel` is visible at a time (class `active`). Clicking nav items switches panels. IDs: `panel-overview`, `panel-mapview`, `panel-analytics`, `panel-alerts`, `panel-nodes`, `panel-config`, `panel-console`.

---

### PANEL CONTENT

#### Overview Panel (`panel-overview`)

**Metrics grid** (`.d-metrics-grid`, 4 cols by default, responsive to 3/2/1):
6 metric cards (`.d-metric`), each with: label, trend badge, large value + unit, progress bar, range row:
1. Water Height — `id="mv-water"`, cm, bar `id="mb-water"`, trend `id="mt-water"`, min `id="mn-water"`, peak `id="mx-water"`
2. Tilt Angle — `id="mv-tilt"`, °, bar cyan, "OLP: X cm", "Drift: X cm"
3. Temperature — `id="mv-temp"`, °C, bar amber
4. Pressure — `id="mv-press"`, hPa, bar purple
5. Battery — `id="mv-batt"`, %, bar green, "WiFi: X dBm"
6. Rise Rate — `id="mv-rate"`, cm/15m, bar `#a78bfa`, "Depth: X cm"

**Row 1 (2-col, 1.4:1):**
- Water Height live chart card (`id="chartWater"`, Chart.js line, `LIVE • 2s` badge)
- Sensor Location map card (Leaflet map `id="overviewMap"`, 310px height, `id="gpsFixBadge"`)

**Row 2 (3-col):**
- Pressure & Temperature chart (`id="chartPressTemp"`, 220px)
- System Alerts log (`id="overviewAlerts"`, scrollable 230px, live `aria-live`)
- Flood Status card:
  - Flood Zone: `id="predZone"`
  - Response Level: `id="predResp"`
  - Sustained Rise: `id="predSustained"`
  - Peak Height: `id="predPeak"`
  - Session Duration: `id="predUptime"`
  - Algorithm badge: `id="algoBadge"`

**Row 3 (2-col equal):**
- Tilt X & Y live chart (`id="chartTilt"`, 220px)
- Battery & WiFi RSSI chart (`id="chartBattery"`, 220px)

#### Map Center Panel (`panel-mapview`)
- Grid: `1fr 320px`
- Left: Leaflet full map (`id="fullMap"`, `min-height:500px`)
- Right panel (scrollable): Map legend (Normal/Alert/Warning/Danger/Offline dots), node detail card (`id` fields for name, coordinates, water height, battery, GPS, last update), node filter buttons (All/Normal/Alert/Warning/Danger/Offline)

#### Analytics Panel (`panel-analytics`)
- Header with title + time range buttons (1H / 6H / 24H / 7D)
- 4 stat cards: Total Readings, Flood Events (24h), Peak Height (session), Avg Rise Rate
- Water Level History chart (Chart.js, 280px)
- 2-col row: Tilt Analysis chart + System Health table
- Export bar with "Export CSV" and "Export Excel (.xlsx)" buttons

#### Alert Center Panel (`panel-alerts`)
- Header with alert count badge + "Clear All" button
- Filter buttons: ALL / CRITICAL / WARNING / ALERT / INFO
- Alert log (`.alert-full-entry` rows with severity dot, message, timestamp, ACK/UNACK badge)

#### Nodes & Devices Panel (`panel-nodes`)
- Header with node count + filter buttons
- Table (`.d-table`) with sticky thead: Node ID, Location, Status, Water Height, Battery, Last Ping, GPS, Actions
- Rows populated from Firebase data

#### Device Config Panel (`panel-config`)
Two-col layout:

**Left — Sampling Rate Control card:**
- Normal Rate slider (10–3600s, default 900s), `id="normalRateSlider"`, label `id="normalRateLabel"`
- High Rate slider (10–600s, default 60s), amber, `id="highRateSlider"`, label `id="highRateLabel"`
- H-Max Flood Threshold slider (20–1000cm, default 200cm), red, `id="hMaxSlider"`, label `id="hMaxLabel"`
- "Write to Firebase" button

**Right — Alert Thresholds card:**
- ALERT threshold input (default 80cm)
- WARNING threshold input (default 120cm)
- DANGER threshold input (default 180cm)
- OLP (Offset Length) input
- "Apply Thresholds" button

#### Console Panel (`panel-console`)
Full-height terminal layout:
- Toolbar with buttons: Clear, Pause/Resume, Save Log, "OTA Update" (green), "System Log" (blue, with notification badge `id="logNotifBadge"`)
- Connection status dot + text (right side of toolbar)
- Terminal output area (`.console-output`) — dark `#0a0e14`, JetBrains Mono
- Welcome ASCII art section showing project name
- Input row with green `varuna@c3 ▶` prompt + `id="consoleInput"` text input
- Autocomplete dropdown (shown on typing, with categorized command list)

**Console commands to support:** `help`, `status`, `water`, `tilt`, `temp`, `press`, `batt`, `gps`, `algo`, `zone`, `clear`, `export csv`, `export excel`, `ping`, `uptime`, `history`, `alert`, `setzone [0-3]`, `setalert [level]`, `reboot`, `ota`, `log`, `version`, `network`

**Terminal line types** (different colors): `t-input` (green), `t-output` (blue-grey), `t-status` (green), `t-error` (red), `t-warning` (amber), `t-flood` (orange), `t-system` (purple), `t-info` (blue), `t-csv` (dark grey), `t-log` (violet), `t-signal` (cyan)

---

### FLOATING UI ELEMENTS

#### Log Drawer (`.log-drawer`)
- `position:fixed; right:0; top:52px; bottom:0; width:380px`
- Slides in from right (`transform:translateX(100%)`, `.open` = `translateX(0)`)
- Header with "System Log" title + pin button (`id="logPinBtn"`) + close button
- Filter buttons: All/Status/Error/Warn/Flood
- Scrollable log body (`id="statusLogOutput"`)

#### OTA Modal (`.ota-modal-overlay`)
- Full-screen backdrop, centered modal (440px wide)
- Title: "Firmware OTA Update" with upload icon
- Description of the OTA process
- File input label (dashed border, accepts `.bin`)
- Upload button (`id="otaUploadBtn"`, disabled until file selected)
- Progress bar (`id="otaProgressFill"`) + text/size labels
- Cancel + Close buttons

#### Connection Banner (`.connection-banner`)
- Fixed below topbar, red background
- "Connection lost — Attempting to reconnect..."
- Hidden by default, shown with class `.show`

---

### JAVASCRIPT — FIREBASE DATA PROCESSING

When `window.onFirebaseLive(val)` is called with Firebase data:

**Expected Firebase data fields:**
`waterHeight`, `tiltX`, `tiltY`, `tiltAngle`, `temperature`, `pressure`, `bmpTemp`, `battery`, `latitude`, `longitude`, `gpsFix`, `satellites`, `floodZone`, `alertLevel`, `uptime`, `depth`, `algorithmEnabled`

**Processing steps:**
1. Increment push counter, update `id="sideDP"`
2. Update all 6 metric values + bars + trends (compare to previous reading)
3. Update topbar risk badge class and text based on `floodZone` (0=normal/green, 1=alert/amber, 2=warning/orange, 3=danger/red, 4=critical/pulsing red)
4. Update sidebar footer: Firebase="Connected", Last Sync=current time, C3 Uptime
5. Update topbar status dot to green "Live · Firebase"
6. Update Flood Status card: zone name, response level, sustained rise, peak, uptime formatted as "Xh Xm Xs"
7. Update Charts — rolling 60-point windows
8. Update Leaflet map markers if GPS valid
9. Add entry to alert log if zone escalated
10. Record snapshot in `sensorHistory[]` array (max 10,000 entries)
11. Emit console line if console is initialized

**Flood zone names:** `['NORMAL', 'ALERT', 'WARNING', 'DANGER']`
**Response level names:** `['NORMAL', 'WATCH', 'WARNING', 'FLOOD', 'CRITICAL']`

---

### CHART.JS CONFIGURATION

All charts use dark theme:
- `backgroundColor: 'transparent'`
- Grid: `rgba(255,255,255,.04)`, ticks color `#475569`
- Font: Inter, 10px

**chartWater**: Line chart, 60 points, gradient fill (blue→transparent), `#3b82f6` border, no point dots, smooth tension 0.4

**chartPressTemp**: Two-dataset line chart (pressure on left Y-axis `#8b5cf6`, temperature on right Y-axis `#f59e0b`)

**chartTilt**: Two-dataset line chart (Tilt X `#06b6d4`, Tilt Y `#a78bfa`)

**chartBattery**: Two-dataset line chart (Battery % `#10b981` left axis, WiFi RSSI `#3b82f6` right axis)

---

### LEAFLET MAP CONFIGURATION

Dark tile: `https://{s}.basemaps.cartocdn.com/dark_all/{z}/{x}/{y}{r}.png`
- `maxZoom: 19`, attribution: CartoDB
- Default center: `[20.5937, 78.9629]` (India), zoom 5
- Custom circular marker (colored by zone) with pulse CSS animation
- Popup on click: Node ID, water height, battery, GPS coordinates, last update
- Duplicate for `overviewMap` and `fullMap`

---

### SENSOR DATA EXPORT

**`exportSensorExcel()`**: Uses SheetJS `window.XLSX` to generate a styled `.xlsx` with:
- Sheet 1 "VARUNA Sensor Data": 27 columns including Serial No., ISO Timestamp, Date, Time, Water Height, Tilt X/Y/Magnitude, Temperature, Pressure, BMP Temp, Battery, WiFi RSSI, Latitude, Longitude, GPS Fix, Satellites, SIM Signal, Flood Zone, Zone Label, Rise Rate, Depth, Sustained Rise, Algorithm Active, Response Level, C3 Uptime, Push Count
- Dark blue header row styling (`#1A3A5C` background, white bold text, `#2563EB` border)
- Alternating row backgrounds (`#0D1626` / `#111F33`), `#94A3B8` text
- Column widths set appropriately
- Sheet 2 "Metadata": 18 rows of key/value pairs describing the system, sensors, and field meanings
- Filename: `VARUNA_SensorData_[ISO timestamp].xlsx`

**`exportSensorCSV()`**: Standard CSV download of the same data.

---

### ADDITIONAL JS BEHAVIORS

**`toggleSidebar()`**: Toggle `.open` class on `#dashSidebar`

**Panel navigation**: All `.d-nav-item` buttons with `data-panel` attributes. On click: remove `.active` from all nav items and panels, add to clicked item and corresponding panel. Special handling for `mapview` and `console` panels to initialize Leaflet/charts after first reveal.

**`toggleLogDrawer()`** / **`toggleLogPin()`**: Control log drawer open/pin state. Update `id="logNotifBadge"` with unread count.

**`openOtaModal()`** / **`closeOtaModal()`**: Toggle `.open` on `#otaModalOverlay`

**Topbar clock**: `setInterval` every second updating `id="topClock"` with `toLocaleTimeString('en-IN', {hour12:false})`

**`varunaConsole` object** with methods:
- `init()`: Renders ASCII welcome art, sets `initialized=true`
- `addLine(text, type)`: Appends `.term-line.t-{type}` span with timestamp prefix to `#consoleOutput`
- `processCommand(cmd)`: Switch/case on command strings, outputs appropriate data
- `setFilter(type, btn)`: Filters log drawer entries by type
- `handleFirmwareFile(file)`: Shows filename in OTA label, enables upload button
- `startOtaUpload()`: Simulates OTA progress bar animation
- `cancelOta()`: Resets OTA state

**Console autocomplete**: On `#consoleInput` keyup, show matching commands from a categorized list. Arrow keys navigate, Enter selects. Escape hides.

---

### RESPONSIVE BREAKPOINTS

- `max-width:1400px`: metrics grid → 3 cols
- `max-width:1024px`: sidebar becomes fixed overlay, all 2/3-col grids → 1 col, map height auto
- `max-width:768px`: 2-col metrics, hidden nav links, mobile padding
- `max-width:480px`: single col everything
- `@media print`: hide sidebar/topbar, single column layout

---

### ACCESSIBILITY

- `<a href="#main-content" class="skip-link">` at top of body
- `role="banner"`, `role="navigation"`, `role="main"`, `role="log"` on appropriate elements
- `aria-live="polite"` on status text, clock, alert log
- `aria-label` on all icon-only buttons
- `aria-expanded` on hamburger toggle
- `:focus-visible` ring: `2px solid var(--accent)` with `outline-offset:2px`
- All decorative elements have `aria-hidden="true"`

---

### NOTES FOR IMPLEMENTATION

1. Do not attempt to load any images — all visuals must be inline SVG or CSS
2. The Firebase module script uses `type="module"` — ensure compatibility
3. Charts initialize lazily (only when their panel is first shown)
4. Leaflet maps initialize lazily; do not call `map.invalidateSize()` until the container is visible
5. All color values match the CSS variables exactly — use `var(--accent)` etc. throughout JS-generated HTML
6. The `sensorHistory` array accumulates readings for Excel export; cap at 10,000 entries with `shift()`
7. Use `window.XLSX` check before Excel export; show alert if not yet loaded
8. The `.l-nav.scrolled` class can slightly increase background opacity on scroll (add via scroll listener)
9. Firebase errors should update topbar status to red "Disconnected" and show the connection banner
