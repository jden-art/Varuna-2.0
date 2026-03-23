Let me systematically go through the HTML and identify every edit needed.

---

## Problem 1 — Firebase field name mismatches

The website reads from `varuna/live` but uses field names that don't match what the C3 now pushes. The C3 sends the exact field names from `buildFullPostBody()`. Every place the website reads `d.fieldName`, the name must match exactly.

**Current wrong names → correct names:**

```
d.theta          → d.tiltAngle
d.olpLength      → d.tetherLength
d.horizDist      → remove (not in new schema)
d.bmpAvailable   → d.bmpOnline
d.simAvailable   → always derive from d.healthScore or remove
d.simRSSI        → remove (not sent by C3)
d.wifiRSSI       → remove (not in new schema — C3 doesn't know S3's WiFi)
d.peakHeight     → remove (computed locally in website, not from Firebase)
d.minHeight      → remove
d.baselinePressure → d.atmosphericRef
d.pressureDeviation → d.gaugePressure
d.depth          → d.depth (correct already)
d.floodZone      → d.floodZone (correct)
d.floodResponse  → d.alertLevel
d.sustainedRise  → remove (not in new schema)
d.sessionDuration → d.uptime
d.rate           → remove (not computed by S3 — remove rise rate card or derive locally)
d.pushCount      → remove (not in schema)
d.c3Uptime       → d.uptime
d.gpsFix         → d.gpsFix (correct)
d.satellites     → d.satellites (correct)
d.algorithmEnabled → d.algorithmEnabled (correct)
d.obLightEnabled → remove (not in schema)
d.debugEnabled   → remove
d.sampleInterval → d.sampleSec (multiply by 1000 for ms)
d.transmitInterval → remove
d.mpuAvailable   → d.mpuOnline
d.hcsrAvailable  → remove
```

---

## Problem 2 — Rise rate must be computed locally in the website

The S3 doesn't send rise rate — it sends water height. The website must compute rise rate itself from the history array. Add this after `pushH(histWater, ...)`:

```javascript
// Add this global at the top with other state variables:
let localRiseRate = 0;

// Add this inside processFirebaseData(), after pushH calls:
// Compute rise rate locally — change in water height over last 15 readings
// (at ~3s per reading, 15 readings ≈ 45 seconds, scale to cm/15min)
if (histWater.length >= 2) {
    const newest = histWater[histWater.length - 1];
    const lookback = Math.min(15, histWater.length - 1);
    const oldest = histWater[histWater.length - 1 - lookback];
    // Scale: lookback readings at 3s each = lookback*3 seconds
    // rate in cm/15min = delta * (900 / (lookback * 3))
    const scaler = 900 / (lookback * 3);
    localRiseRate = (newest - oldest) * scaler;
}
```

Then everywhere `sn(d.rate)` appears, replace with `localRiseRate`.

---

## Problem 3 — updateOverview() field name fixes

Find `updateOverview(d)` and make these replacements:

```javascript
// REMOVE these lines entirely (fields don't exist):
setText('mn-water', sn(d.minHeight).toFixed(1));
setText('mx-water', sn(d.peakHeight).toFixed(1));
setText('mn-tilt', sn(d.olpLength).toFixed(0));
setText('mx-tilt', sn(d.horizDist).toFixed(1));
setText('mn-temp', d.bmpAvailable ? 'Yes' : 'No');
setText('mx-press', sn(d.pressureDeviation).toFixed(1));
setText('mn-batt', String(sn(d.wifiRSSI)));
setText('mn-rate', sn(d.depth).toFixed(1));

// REPLACE WITH:
setText('mn-water', '—');       // no min tracking in new schema
setText('mx-water', '—');       // no peak tracking — could compute from histWater
setText('mn-tilt', sn(d.tetherLength).toFixed(2) + 'm');
setText('mx-tilt', '—');
setText('mn-temp', d.bmpOnline ? 'BMP OK' : 'BMP FAIL');
setText('mn-press', sn(d.atmosphericRef).toFixed(0));
setText('mx-press', sn(d.gaugePressure).toFixed(1));
setText('mn-batt', d.battVoltage ? sn(d.battVoltage).toFixed(2) + 'V' : '—');
setText('mn-rate', sn(d.depth).toFixed(2) + 'm');
```

Also fix theta reference:
```javascript
// CHANGE:
const th = sn(d.theta);
// TO:
const th = sn(d.tiltAngle);

// And all subsequent references to th stay the same — just the source changes
```

---

## Problem 4 — updateFloodStatus() fixes

```javascript
// CHANGE:
const r = sn(d.floodResponse);
setText('predResp', RESP_NAMES[r] || '—');

// TO:
const r = sn(d.alertLevel);
setText('predResp', RESP_NAMES[r] || '—');

// CHANGE:
setText('predSustained', d.sustainedRise ? 'YES ▲' : 'No');
const ps = document.getElementById('predSustained');
if (ps) ps.style.color = d.sustainedRise ? 'var(--amber)' : 'var(--green)';

// TO:
const rising = localRiseRate > 0.5;
setText('predSustained', rising ? 'YES ▲' : 'No');
const ps = document.getElementById('predSustained');
if (ps) ps.style.color = rising ? 'var(--amber)' : 'var(--green)';

// CHANGE:
setText('predPeak', sn(d.peakHeight).toFixed(1) + ' cm');

// TO — compute from history:
const peakH = histWater.length ? Math.max(...histWater) : 0;
setText('predPeak', peakH.toFixed(1) + ' cm');

// CHANGE:
setText('predUptime', formatDuration(sn(d.sessionDuration)));

// TO:
setText('predUptime', formatDuration(sn(d.uptime)));

// CHANGE:
setText('algoBadge', d.algorithmEnabled ? 'ALGO ON' : 'ALGO OFF');

// stays the same — algorithmEnabled is correct field name
```

---

## Problem 5 — updateSidebarInfo() fixes

```javascript
// CHANGE:
setText('sideDP', sn(d.pushCount));
setText('sideUptime', formatDuration(sn(d.c3Uptime)));

// TO:
setText('sideDP', '—');                              // pushCount removed from schema
setText('sideUptime', formatDuration(sn(d.uptime)));
```

---

## Problem 6 — updateMapDetail() fixes

```javascript
// CHANGE:
setText('mnd-tl', sn(d.theta).toFixed(1) + '°');
// TO:
setText('mnd-tl', sn(d.tiltAngle).toFixed(1) + '°');

// CHANGE:
setText('mnd-sim', d.simAvailable ? 'RSSI: ' + sn(d.simRSSI) : 'No SIM');
// TO:
setText('mnd-sim', '—');   // SIM info not in new schema from C3

// CHANGE:
setText('mnd-fz', ZONE_NAMES[sn(d.floodZone)] || '—');
// stays correct

// CHANGE:
setText('mapWifi', sn(d.wifiRSSI) + ' dBm');
// TO:
setText('mapWifi', '—');    // not in new schema

// CHANGE:
setText('mapC3Up', formatDuration(sn(d.c3Uptime)));
// TO:
setText('mapC3Up', formatDuration(sn(d.uptime)));

// CHANGE:
setText('mapPush', String(sn(d.pushCount)));
// TO:
setText('mapPush', '—');

// CHANGE:
setText('mapAlgo', d.algorithmEnabled ? 'Enabled' : 'Disabled');
// correct — stays the same
```

---

## Problem 7 — updateAnalytics() fixes

```javascript
// CHANGE:
setText('statRate', sn(d.rate).toFixed(2));
setText('statRateSub', d.sustainedRise ? '▲ Rising' : '→ Stable');
// TO:
setText('statRate', localRiseRate.toFixed(2));
setText('statRateSub', localRiseRate > 0.5 ? '▲ Rising' : '→ Stable');

// CHANGE:
setText('statZone', ZONE_NAMES[sn(d.floodZone)] || '—');
// correct — stays

// CHANGE:
setText('statWaterSub', 'Peak: ' + sn(d.peakHeight).toFixed(1) + ' cm');
// TO:
const peakH2 = histWater.length ? Math.max(...histWater) : 0;
setText('statWaterSub', 'Peak: ' + peakH2.toFixed(1) + ' cm');
```

---

## Problem 8 — updateNodes() fixes

```javascript
// CHANGE:
setText('nodGps', d.gpsFix ? '● Online (' + sn(d.satellites) + ' sats)' : '○ No Fix');
setColor('nodGps', d.gpsFix ? '#10b981' : '#ef4444');
// stays correct — gpsFix and satellites are correct field names

// CHANGE:
setText('nodSim', d.simAvailable ? '● Available (RSSI ' + sn(d.simRSSI) + ')' : '○ Unavailable');
setColor('nodSim', d.simAvailable ? '#10b981' : '#ef4444');
// TO:
setText('nodSim', '—');    // SIM info not available in new schema

// CHANGE:
setText('nodBmp', d.bmpAvailable ? '● Online' : '○ Offline');
setColor('nodBmp', d.bmpAvailable ? '#10b981' : '#ef4444');
// TO:
setText('nodBmp', d.bmpOnline ? '● Online' : '○ Offline');
setColor('nodBmp', d.bmpOnline ? '#10b981' : '#ef4444');

// CHANGE:
setText('nodLight', d.obLightEnabled ? '● On' : '○ Off');
setText('nodDebug', d.debugEnabled ? '● On' : '○ Off');
// TO:
setText('nodLight', '—');   // not in schema
setText('nodDebug', '—');

// CHANGE:
setText('nodSample', sn(d.sampleInterval) / 1000 + 's');
setText('nodTransmit', sn(d.transmitInterval) / 1000 + 's');
// TO:
setText('nodSample', sn(d.sampleSec) + 's');
setText('nodTransmit', '—');

// CHANGE (nodeHealth doughnut chart):
const g = d.gpsFix ? 1 : 0, s = d.simAvailable ? 1 : 0,
      bp = d.bmpAvailable ? 1 : 0, a = d.algorithmEnabled ? 1 : 0;
const on = g + s + bp + a;
// TO — use healthScore directly:
const healthPct = sn(d.healthScore);
const on = Math.round(healthPct / 25);   // 0-4 scale (25pts each sensor)
charts.nodeHealth.data.datasets[0].data = [on, 4 - on];
charts.nodeHealth.update('none');
```

---

## Problem 9 — recordSensorStatus() fixes

```javascript
// CHANGE:
const statuses = [
    d.gpsFix ? 1 : 0,
    d.simAvailable ? 1 : 0,
    d.bmpAvailable ? 1 : 0,
    (sn(d.waterHeight) > 0 || d.hcsrAvailable) ? 1 : (fbConnected ? 1 : 0),
    (sn(d.tiltX) !== 0 || sn(d.tiltY) !== 0 || d.mpuAvailable) ? 1 : (fbConnected ? 1 : 0),
    (sn(d.wifiRSSI) < 0) ? 1 : 0,
    d.algorithmEnabled ? 1 : 0,
    d.obLightEnabled ? 1 : 0,
];

// TO:
const statuses = [
    d.gpsFix ? 1 : 0,
    d.gpsOnline ? 1 : 0,          // GPS module online (from S3)
    d.bmpOnline ? 1 : 0,          // BMP sensor
    sn(d.waterHeight) > 0 ? 1 : 0, // water level (tether-based)
    d.mpuOnline ? 1 : 0,          // MPU6050
    fbConnected ? 1 : 0,          // Firebase (connectivity indicator)
    d.algorithmEnabled ? 1 : 0,   // flood algorithm
    d.rtcOnline ? 1 : 0,          // RTC
];
```

Also update `SENSOR_KEYS` to match:

```javascript
// CHANGE:
const SENSOR_KEYS = [
    'GPS Module', 'SIM Module', 'BMP Sensor', 'HC-SR04 (Water)',
    'MPU6050 (IMU)', 'WiFi', 'Algorithm', 'OB Light'
];

// TO:
const SENSOR_KEYS = [
    'GPS Fix', 'GPS Module', 'BMP Sensor', 'Water Level (Tether)',
    'MPU6050 (IMU)', 'Firebase', 'Algorithm', 'RTC Module'
];
```

---

## Problem 10 — recordSensorSnapshot() fixes

```javascript
// CHANGE several field references:
tiltAngle: sn(d.tiltAngle || Math.sqrt(sn(d.tiltX)**2 + sn(d.tiltY)**2)),
// TO:
tiltAngle: sn(d.tiltAngle),

// CHANGE:
wifiRSSI: sn(d.wifiRSSI),
// TO:
wifiRSSI: 0,    // not in new schema

// CHANGE:
simSignal: sn(d.simSignal || d.simRSSI || 0),
// TO:
simSignal: 0,

// CHANGE:
riseRate: sn(d.rate),
// TO:
riseRate: localRiseRate,

// CHANGE:
depth: sn(d.depth || d.waterHeight),
// TO:
depth: sn(d.depth),

// CHANGE:
sustainedRise: d.sustainedRise ? 1 : 0,
// TO:
sustainedRise: localRiseRate > 0.5 ? 1 : 0,

// CHANGE:
responseLevel: sn(d.responseLevel || d.floodZone),
// TO:
responseLevel: sn(d.alertLevel),

// CHANGE:
c3Uptime: sn(d.c3Uptime || d.uptime || 0),
pushCount: sn(d.pushCount || 0),
// TO:
c3Uptime: sn(d.uptime),
pushCount: 0,
```

---

## Problem 11 — OTA modal: complete replacement

The current OTA modal uploads via Web Serial. It needs to upload to Railway server and then watch Firebase for progress. Replace `startOtaUpload()` and `handleFirmwareFile()` in the VarunaConsole class:

```javascript
// Replace handleFirmwareFile:
handleFirmwareFile(file) {
    if (!file) return;
    if (!file.name.endsWith('.bin')) {
        this.addLine('Only .bin files accepted.', 'error');
        return;
    }
    this.firmwareFile = file;
    const lb = document.getElementById('otaLabel');
    const btn = document.getElementById('otaUploadBtn');
    if (lb) lb.textContent = file.name + ' (' + (file.size / 1024).toFixed(1) + ' KB)';
    if (btn) btn.disabled = false;
    this.addLine('Firmware selected: ' + file.name + ' (' + file.size + ' bytes)', 'system');
}

// Replace startOtaUpload:
async startOtaUpload() {
    if (!this.firmwareFile) {
        this.addLine('No firmware file selected.', 'error');
        return;
    }

    this.otaInProgress = true;
    document.getElementById('otaProgress')?.classList.add('show');
    document.getElementById('otaUploadBtn').style.display = 'none';
    document.getElementById('otaCancelBtn').style.display = '';

    this.addLine('Uploading firmware to server...', 'system');
    this.setOtaProgress(5, 'Uploading to server...');

    try {
        // Upload .bin to Railway server
        const formData = new FormData();
        formData.append('firmware', this.firmwareFile);

        const resp = await fetch(
            'https://varuna-server-production.up.railway.app/api/firmware',
            { method: 'POST', body: formData }
        );

        if (!resp.ok) {
            throw new Error('Server upload failed: ' + resp.status);
        }

        const result = await resp.json();
        this.addLine('Firmware uploaded. Size: ' + (result.size / 1024).toFixed(1) + ' KB', 'system');
        this.addLine('Checksum: ' + result.checksum, 'system');
        this.addLine('Waiting for C3 to pick up OTA command...', 'system');
        this.setOtaProgress(15, 'Waiting for device...');

        // Now watch Firebase varuna/devices/VARUNA_001/ota for status updates
        this.watchOtaProgress();

    } catch (e) {
        this.addLine('OTA upload error: ' + e.message, 'error');
        this.otaInProgress = false;
        this.otaCleanup();
    }
}

// Add new method watchOtaProgress:
watchOtaProgress() {
    // Poll Firebase varuna/devices/VARUNA_001/ota every 3 seconds
    const RAILWAY_URL = 'https://varuna-server-production.up.railway.app';
    let pollCount = 0;
    const maxPolls = 200;  // 200 * 3s = 10 minutes max

    const interval = setInterval(async () => {
        if (!this.otaInProgress) {
            clearInterval(interval);
            return;
        }

        pollCount++;
        if (pollCount > maxPolls) {
            clearInterval(interval);
            this.addLine('OTA timeout — check device manually.', 'warning');
            this.otaCleanup();
            return;
        }

        try {
            const resp = await fetch(
                RAILWAY_URL + '/api/ota/command/VARUNA_001'
            );
            if (!resp.ok) return;
            const data = await resp.json();

            const status = data.status || 'UNKNOWN';
            const progress = data.progress || '';

            this.addLine('[OTA] Status: ' + status + (progress ? ' ' + progress : ''), 'info');

            // Map status to progress bar
            const progressMap = {
                'AWAITING_DEVICE': 15,
                'READY': 20,
                'DOWNLOADING': 35,
                'VERIFYING': 60,
                'FLASHING': 75,
                'OTA_RETRY': 80,
                'OTA_COMPLETE': 100,
                'OTA_FAILED:DOWNLOAD': -1,
                'OTA_FAILED:CHECKSUM': -1,
                'OTA_FAILED:SYNC': -1,
                'OTA_FAILED:FLASH_BEGIN': -1,
                'OTA_FAILED:FLASH_DATA': -1,
                'OTA_FAILED:NO_BOOT': -1,
            };

            const pct = progressMap[status];

            if (pct === 100) {
                clearInterval(interval);
                this.setOtaProgress(100, 'Complete!');
                this.addLine('✓ OTA complete — S3 running new firmware.', 'status');
                this.addStatusEntry('OTA firmware update complete', 'status');
                setTimeout(() => this.otaCleanup(), 3000);
            } else if (pct === -1) {
                clearInterval(interval);
                this.setOtaProgress(0, 'Failed');
                this.addLine('✗ OTA failed: ' + status, 'error');
                this.addStatusEntry('OTA failed: ' + status, 'error');
                setTimeout(() => this.otaCleanup(), 3000);
            } else if (pct) {
                // For FLASHING with progress like "42/175"
                let displayPct = pct;
                if (status === 'FLASHING' && progress.includes('/')) {
                    const parts = progress.split('/');
                    const done = parseInt(parts[0]);
                    const total = parseInt(parts[1]);
                    if (total > 0) {
                        // Scale flashing progress between 75-95%
                        displayPct = 75 + Math.round((done / total) * 20);
                    }
                }
                this.setOtaProgress(displayPct, status);
            }

        } catch (e) {
            // Ignore poll errors — keep trying
        }
    }, 3000);
}

// Add helper:
setOtaProgress(pct, label) {
    const fill = document.getElementById('otaProgressFill');
    const text = document.getElementById('otaProgressText');
    const size = document.getElementById('otaProgressSize');
    if (fill) fill.style.width = Math.max(0, pct) + '%';
    if (text) text.textContent = Math.max(0, pct) + '%';
    if (size) size.textContent = label || '';
}

otaCleanup() {
    this.otaInProgress = false;
    setTimeout(() => {
        document.getElementById('otaProgress')?.classList.remove('show');
        document.getElementById('otaUploadBtn').style.display = '';
        document.getElementById('otaCancelBtn').style.display = 'none';
    }, 2000);
}

// Update cancelOta:
cancelOta() {
    this.otaInProgress = false;
    this.addLine('OTA cancelled.', 'warning');
    this.otaCleanup();
}
```

---

## Problem 12 — Add config control panel to the dashboard

Add a new sidebar nav item and panel for device config. In the sidebar HTML, after the Nodes nav item add:

```html
<button class="d-nav-item" data-panel="config">
  <span class="nav-icon">
    <svg viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2" width="16" height="16">
      <circle cx="12" cy="12" r="3"/>
      <path d="M19.4 15a1.65 1.65 0 00.33 1.82l.06.06a2 2 0 010 2.83 2 2 0 01-2.83 0l-.06-.06a1.65 1.65 0 00-1.82-.33 1.65 1.65 0 00-1 1.51V21a2 2 0 01-4 0v-.09A1.65 1.65 0 009 19.4a1.65 1.65 0 00-1.82.33l-.06.06a2 2 0 01-2.83-2.83l.06-.06A1.65 1.65 0 004.68 15a1.65 1.65 0 00-1.51-1H3a2 2 0 010-4h.09A1.65 1.65 0 004.6 9a1.65 1.65 0 00-.33-1.82l-.06-.06a2 2 0 012.83-2.83l.06.06A1.65 1.65 0 009 4.68a1.65 1.65 0 001-1.51V3a2 2 0 014 0v.09a1.65 1.65 0 001 1.51 1.65 1.65 0 001.82-.33l.06-.06a2 2 0 012.83 2.83l-.06.06A1.65 1.65 0 0019.4 9a1.65 1.65 0 001.51 1H21a2 2 0 010 4h-.09a1.65 1.65 0 00-1.51 1z"/>
    </svg>
  </span> Device Config
</button>
```

Then add the config panel HTML before the closing `</main>` tag:

```html
<!-- CONFIG PANEL -->
<div class="d-panel" id="panel-config">
<div style="margin-bottom:20px">
  <div class="d-card-title" style="font-size:.9rem;margin-bottom:4px">Device Configuration</div>
  <div style="font-size:.78rem;color:var(--text3)">Changes write to Firebase and are picked up by the C3 within 5 seconds</div>
</div>

<div class="d-row d-row-2e">

<!-- Sampling config -->
<div class="d-card">
  <div class="d-card-header"><span class="d-card-title">Sampling Rate Control</span></div>

  <div style="margin-bottom:20px">
    <div style="display:flex;justify-content:space-between;margin-bottom:6px">
      <label style="font-size:.78rem;color:var(--text2)">Normal Rate (below 50% flood level)</label>
      <span style="font-family:var(--mono);font-size:.78rem;color:var(--accent)" id="normalRateLabel">900s</span>
    </div>
    <input type="range" id="normalRateSlider" min="10" max="3600" step="10" value="900"
      style="width:100%;accent-color:var(--accent)"
      oninput="document.getElementById('normalRateLabel').textContent=this.value+'s'">
    <div style="display:flex;justify-content:space-between;font-size:.65rem;color:var(--text4);margin-top:2px">
      <span>10s (fast)</span><span>3600s (1hr)</span>
    </div>
  </div>

  <div style="margin-bottom:20px">
    <div style="display:flex;justify-content:space-between;margin-bottom:6px">
      <label style="font-size:.78rem;color:var(--text2)">High Rate (above 80% flood level)</label>
      <span style="font-family:var(--mono);font-size:.78rem;color:var(--amber)" id="highRateLabel">60s</span>
    </div>
    <input type="range" id="highRateSlider" min="10" max="600" step="10" value="60"
      style="width:100%;accent-color:var(--amber)"
      oninput="document.getElementById('highRateLabel').textContent=this.value+'s'">
    <div style="display:flex;justify-content:space-between;font-size:.65rem;color:var(--text4);margin-top:2px">
      <span>10s (fastest)</span><span>600s (10min)</span>
    </div>
  </div>

  <div style="margin-bottom:24px">
    <div style="display:flex;justify-content:space-between;margin-bottom:6px">
      <label style="font-size:.78rem;color:var(--text2)">H-Max — Flood Threshold (cm)</label>
      <span style="font-family:var(--mono);font-size:.78rem;color:var(--red)" id="hMaxLabel">200cm</span>
    </div>
    <input type="range" id="hMaxSlider" min="20" max="1000" step="10" value="200"
      style="width:100%;accent-color:var(--red)"
      oninput="document.getElementById('hMaxLabel').textContent=this.value+'cm'">
    <div style="display:flex;justify-content:space-between;font-size:.65rem;color:var(--text4);margin-top:2px">
      <span>20cm</span><span>1000cm</span>
    </div>
  </div>

  <button onclick="pushConfigToFirebase()" style="
    width:100%;padding:10px;border-radius:var(--radius-sm);
    background:var(--accent);color:#fff;border:none;
    font-family:var(--sans);font-size:.82rem;font-weight:700;cursor:pointer">
    Push Config to Device
  </button>
  <div id="configPushStatus" style="font-size:.72rem;color:var(--text3);margin-top:8px;text-align:center"></div>
</div>

<!-- Device commands -->
<div class="d-card">
  <div class="d-card-header"><span class="d-card-title">Device Commands</span></div>

  <!-- Realtime mode -->
  <div style="display:flex;align-items:center;justify-content:space-between;padding:14px 0;border-bottom:1px solid var(--border)">
    <div>
      <div style="font-size:.82rem;color:var(--text2);font-weight:600">Realtime Mode</div>
      <div style="font-size:.72rem;color:var(--text3);margin-top:2px">Posts every reading immediately — use sparingly</div>
    </div>
    <button id="realtimeBtn" onclick="toggleRealtimeMode()" style="
      padding:6px 18px;border-radius:20px;border:1px solid var(--green);
      background:rgba(16,185,129,.1);color:var(--green);
      font-family:var(--sans);font-size:.75rem;font-weight:700;cursor:pointer">
      OFF
    </button>
  </div>

  <!-- Run diagnostic -->
  <div style="display:flex;align-items:center;justify-content:space-between;padding:14px 0;border-bottom:1px solid var(--border)">
    <div>
      <div style="font-size:.82rem;color:var(--text2);font-weight:600">Run Sensor Diagnostic</div>
      <div style="font-size:.72rem;color:var(--text3);margin-top:2px">Triggers full sensor health check on S3</div>
    </div>
    <button onclick="triggerDiagnostic()" style="
      padding:6px 18px;border-radius:20px;border:1px solid var(--accent2);
      background:rgba(6,182,212,.1);color:var(--accent2);
      font-family:var(--sans);font-size:.75rem;font-weight:700;cursor:pointer">
      Run
    </button>
  </div>

  <!-- Current config display -->
  <div style="margin-top:16px">
    <div style="font-size:.7rem;font-weight:700;letter-spacing:1px;text-transform:uppercase;color:var(--text3);margin-bottom:10px">Current Device Config (from Firebase)</div>
    <div class="mnd-row"><span class="mnd-k">Normal Rate</span><span class="mnd-v" id="cfgNormalRate">—</span></div>
    <div class="mnd-row"><span class="mnd-k">High Rate</span><span class="mnd-v" id="cfgHighRate">—</span></div>
    <div class="mnd-row"><span class="mnd-k">H-Max</span><span class="mnd-v" id="cfgHMax">—</span></div>
    <div class="mnd-row"><span class="mnd-k">Mode</span><span class="mnd-v" id="cfgMode">—</span></div>
    <div class="mnd-row"><span class="mnd-k">Health Score</span><span class="mnd-v" id="cfgHealth">—</span></div>
    <div class="mnd-row"><span class="mnd-k">Sample Interval</span><span class="mnd-v" id="cfgSample">—</span></div>
  </div>
</div>

</div>

<!-- Diagnostic report viewer -->
<div class="d-card" style="margin-top:14px">
  <div class="d-card-header">
    <span class="d-card-title">Last Diagnostic Report</span>
    <span class="d-card-badge" id="diagTimestamp">Never run</span>
  </div>
  <div id="diagReportBody" style="font-family:var(--mono);font-size:.72rem;color:var(--text3);line-height:1.8;white-space:pre-wrap">
    No diagnostic data yet. Use "Run Sensor Diagnostic" above.
  </div>
</div>
</div>
```

---

## Problem 13 — Add Firebase config write functions to JavaScript

Add these functions in the `<script>` block:

```javascript
// ── Firebase imports for writing (already have read SDK loaded) ──
// The existing Firebase SDK in the module script is read-only listener.
// For writes, use Firebase REST API with the database secret.
const FIREBASE_DB_URL = 'https://varuna-flood-default-rtdb.asia-southeast1.firebasedatabase.app';
const FIREBASE_SECRET = 'YOUR_DATABASE_SECRET_HERE';
// Get this from Firebase Console → Project Settings → Service Accounts → Database secrets

async function firebaseWrite(path, data) {
    const url = FIREBASE_DB_URL + path + '.json?auth=' + FIREBASE_SECRET;
    const resp = await fetch(url, {
        method: 'PATCH',
        headers: { 'Content-Type': 'application/json' },
        body: JSON.stringify(data)
    });
    return resp.ok;
}

async function pushConfigToFirebase() {
    const normal = parseInt(document.getElementById('normalRateSlider').value);
    const high   = parseInt(document.getElementById('highRateSlider').value);
    const hmax   = parseInt(document.getElementById('hMaxSlider').value);

    const statusEl = document.getElementById('configPushStatus');
    if (statusEl) statusEl.textContent = 'Pushing...';

    try {
        const ok = await firebaseWrite('/devices/VARUNA_001/config', {
            normal_sec: normal,
            high_sec:   high,
            h_max_cm:   hmax
        });

        if (ok) {
            if (statusEl) statusEl.textContent = '✓ Config pushed — device will update within 5s';
            if (typeof varunaConsole !== 'undefined')
                varunaConsole.addStatusEntry('Config pushed: normal=' + normal + 's high=' + high + 's hmax=' + hmax + 'cm', 'status');
        } else {
            if (statusEl) statusEl.textContent = '✗ Push failed — check Firebase secret';
        }
    } catch (e) {
        if (statusEl) statusEl.textContent = '✗ Error: ' + e.message;
    }

    setTimeout(() => { if (statusEl) statusEl.textContent = ''; }, 5000);
}

let realtimeEnabled = false;
async function toggleRealtimeMode() {
    realtimeEnabled = !realtimeEnabled;
    const btn = document.getElementById('realtimeBtn');

    try {
        const ok = await firebaseWrite('/devices/VARUNA_001/config', {
            realtime: realtimeEnabled
        });

        if (ok) {
            if (btn) {
                btn.textContent = realtimeEnabled ? 'ON' : 'OFF';
                btn.style.borderColor = realtimeEnabled ? 'var(--red)' : 'var(--green)';
                btn.style.background = realtimeEnabled ? 'rgba(239,68,68,.1)' : 'rgba(16,185,129,.1)';
                btn.style.color = realtimeEnabled ? 'var(--red)' : 'var(--green)';
            }
            if (typeof varunaConsole !== 'undefined')
                varunaConsole.addStatusEntry('Realtime mode: ' + (realtimeEnabled ? 'ON' : 'OFF'), 'status');
        }
    } catch (e) {
        realtimeEnabled = !realtimeEnabled;  // revert
    }
}

async function triggerDiagnostic() {
    try {
        const ok = await firebaseWrite('/devices/VARUNA_001/config', {
            run_diagnostic: true
        });
        if (ok) {
            if (typeof varunaConsole !== 'undefined')
                varunaConsole.addStatusEntry('Diagnostic trigger sent to device', 'status');
            // Auto-clear after 30s so it doesn't re-trigger
            setTimeout(() => {
                firebaseWrite('/devices/VARUNA_001/config', { run_diagnostic: false });
            }, 30000);
        }
    } catch (e) {
        console.error('Diagnostic trigger failed:', e);
    }
}

// Update config panel with live data from Firebase
function updateConfigPanel(d) {
    if (!d) return;
    setText('cfgNormalRate', sn(d.normalRate) + 's');
    setText('cfgHighRate',   sn(d.highRate) + 's');
    setText('cfgHMax',       sn(d.hMaxCm) + 'cm');
    setText('cfgMode',       ['SLACK','TAUT','FLOOD','SUBMERGED'][sn(d.mode)] || '—');
    setText('cfgHealth',     sn(d.healthScore) + '/100');
    setText('cfgSample',     sn(d.sampleSec) + 's');

    // Sync sliders to current device values if user hasn't touched them
    const ns = document.getElementById('normalRateSlider');
    const hs = document.getElementById('highRateSlider');
    const hm = document.getElementById('hMaxSlider');
    if (ns && sn(d.normalRate) > 0) {
        ns.value = sn(d.normalRate);
        document.getElementById('normalRateLabel').textContent = sn(d.normalRate) + 's';
    }
    if (hs && sn(d.highRate) > 0) {
        hs.value = sn(d.highRate);
        document.getElementById('highRateLabel').textContent = sn(d.highRate) + 's';
    }
    if (hm && sn(d.hMaxCm) > 0) {
        hm.value = sn(d.hMaxCm);
        document.getElementById('hMaxLabel').textContent = sn(d.hMaxCm) + 'cm';
    }
}

// Watch Firebase diagnostic node
function watchDiagnosticReport() {
    // Uses the existing Firebase SDK (read-only module)
    // Add listener in the module script for varuna/diagnostic
    // Since we can't easily extend the module script, poll via REST
    setInterval(async () => {
        try {
            const resp = await fetch(
                FIREBASE_DB_URL + '/varuna/diagnostic.json'
            );
            if (!resp.ok) return;
            const data = await resp.json();
            if (!data) return;

            const el = document.getElementById('diagReportBody');
            const ts = document.getElementById('diagTimestamp');

            if (data.raw) {
                // Parse the $DIAG frame into readable format
                const raw = data.raw;
                const formatted = raw
                    .replace('$DIAG,', '')
                    .replace(/,/g, '\n')
                    .replace(/=/g, ': ');
                if (el) el.textContent = formatted;
            }
            if (data.receivedAt && ts) {
                ts.textContent = new Date(data.receivedAt).toLocaleTimeString('en-GB');
            }
        } catch (e) {
            // Silent fail — diagnostic is non-critical
        }
    }, 10000);  // Poll every 10s
}
```

---

## Problem 14 — Hook updateConfigPanel into processFirebaseData

Inside `processFirebaseData()`, add one line at the end:

```javascript
// Add after the existing updateHeroStats(d) call:
updateConfigPanel(d);
```

---

## Problem 15 — Call watchDiagnosticReport in init()

```javascript
// In init(), add:
watchDiagnosticReport();
```

---

## Problem 16 — Update OTA modal description text

```html
<!-- CHANGE: -->
<p class="ota-modal-desc">Upload a compiled <code>.bin</code> firmware file to the connected VARUNA node via Web Serial. Ensure the device is connected before uploading.</p>

<!-- TO: -->
<p class="ota-modal-desc">Upload a compiled <code>.bin</code> firmware file. The file is uploaded to the server, then the C3 companion downloads and flashes it to the S3 automatically over the air. No USB connection needed.</p>
```

---

## Problem 17 — Update the console command list

Replace `VARUNA_COMMANDS` with only the commands that actually exist in the new S3 firmware:

```javascript
const VARUNA_COMMANDS = [
    // $DBG commands (sent via C3 Serial1 to S3)
    {cmd:'$DBG,SET_HMAX=',   desc:'Set H-max flood threshold',  cat:'Config',  arg:'<cm>'},
    {cmd:'$DBG,SET_OLP=',    desc:'Set tether length (OLP)',     cat:'Config',  arg:'<m>'},
    {cmd:'$DBG,RECAL',       desc:'Recalibrate all sensors',     cat:'Config'},
    {cmd:'$DBG,GET_STATUS',  desc:'Print full S3 status',        cat:'Config'},
    {cmd:'$DBG,BASELINE',    desc:'Recalibrate pressure baseline',cat:'Config'},
    {cmd:'$DBG,SETTIME,',    desc:'Set RTC time',                cat:'Config',  arg:'yr,mo,dy,dw,hr,mn,sc'},
    // C3 direct commands (handled by C3 processSerialCommand)
    {cmd:'STATUS',           desc:'C3 system status',            cat:'C3'},
    {cmd:'LASTFRAME',        desc:'Show last received CSV frame', cat:'C3'},
    {cmd:'REINITSD',         desc:'Reinitialise SD card',        cat:'C3'},
    {cmd:'RESETSIM',         desc:'Reset SIM800L',               cat:'C3'},
    {cmd:'FORCETX',          desc:'Force immediate data POST',   cat:'C3'},
    {cmd:'SETRT1',           desc:'Enable realtime mode',        cat:'C3'},
    {cmd:'SETRT0',           desc:'Disable realtime mode',       cat:'C3'},
    {cmd:'DELBUF',           desc:'Delete SD buffer file',       cat:'C3'},
    {cmd:'READBUF',          desc:'Print SD buffer contents',    cat:'C3'},
    {cmd:'SIMSTATE',         desc:'SIM800L debug info',          cat:'C3'},
    {cmd:'CFG:',             desc:'Set config manually',         cat:'C3',  arg:'normal,high,hmax'},
    {cmd:'AT',               desc:'AT command passthrough',      cat:'C3',  arg:'<AT cmd>'},
];
```

---

## Summary of all 17 problems and their edit locations

Every edit is surgical — no structural rewrites of the HTML needed. The landing page, CSS, charts, maps, alerts, sidebar and most of the dashboard stay exactly as they are. The changes are concentrated in six JavaScript functions (`updateOverview`, `updateFloodStatus`, `updateNodes`, `updateMapDetail`, `updateAnalytics`, `recordSensorStatus`), one new panel added to the HTML, three new JavaScript functions (`pushConfigToFirebase`, `toggleRealtimeMode`, `triggerDiagnostic`), a complete replacement of `startOtaUpload`, and the VARUNA_COMMANDS list.
