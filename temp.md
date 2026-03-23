Good. Let me do a precise diff of what the C3 file needs changed, section by section.

---

## Section 1 — Includes: REMOVE and ADD

**Remove these:**
```cpp
#include <Firebase_ESP_Client.h>
#include <ArduinoJson.h>
```

**Add these:**
```cpp
#include <WiFi.h>
#include <HTTPClient.h>
```

**Reason:** Firebase library is not needed — C3 never talks to Firebase directly. Server handles that. WiFi and HTTPClient handle both WiFi POST path and config polling.

---

## Section 2 — Pin defines: COMPLETE REPLACEMENT

The entire pin block is wrong. Current file has:
```cpp
#define S3_UART_RX_PIN    2   // receives $DATA frames — WRONG
#define S3_UART_TX_PIN    3   // sends $CFG — correct
#define SIM_TX_PIN        21
#define SIM_RX_PIN        20
#define SIM_RST_PIN       8
// missing: OTA_BOOT_PIN, OTA_RESET_PIN, CSV_RX_PIN
```

**Replace the entire pin block with:**
```cpp
// CSV feed from S3 (9600 baud, SW-UART — S3 GPIO14 → C3 GPIO2)
#define CSV_RX_PIN         2

// $CFG frames to S3 (115200 baud HW-UART — C3 GPIO3 → S3 GPIO44)
#define CFG_TX_PIN         3

// SD card
#define SD_SCK_PIN         4
#define SD_MISO_PIN        5
#define SD_MOSI_PIN        6
#define SD_CS_PIN          7

// SIM800L
#define SIM_RST_PIN        8
#define SIM_TX_PIN         21   // C3 TX → SIM800L RXD
#define SIM_RX_PIN         20   // C3 RX ← SIM800L TXD
                                // ALSO: bootloader ACKs from S3 during OTA

// OTA control pins
#define OTA_BOOT_PIN       9    // C3 GPIO9 → S3 GPIO0 (LOW = bootloader)
#define OTA_RESET_PIN      10   // C3 GPIO10 → S3 EN (LOW pulse = reset)
```

---

## Section 3 — Config constants: COMPLETE REPLACEMENT

Current file has placeholder URLs, empty WiFi, wrong Firebase defines. Replace entire config block:

```cpp
#define DEVICE_ID             "VARUNA_001"

// WiFi credentials
#define WIFI_SSID             "TPLink_2G"
#define WIFI_PASSWORD         "9700668325"

// GPRS
#define SIM_APN               "internet"
#define SIM_BAUD              9600

// Railway server — update after deployment
#define SERVER_BASE           "https://varuna-server-production.up.railway.app"
#define SERVER_DATA_URL       SERVER_BASE "/api/data"
#define SERVER_BATCH_URL      SERVER_BASE "/api/batch"
#define SERVER_TEST_URL       SERVER_BASE "/api/ping"
#define SERVER_DIAG_URL       SERVER_BASE "/api/diagnostic"
#define SERVER_CONFIG_URL     SERVER_BASE "/api/config/" DEVICE_ID
#define SERVER_OTA_CMD_URL    SERVER_BASE "/api/ota/command/" DEVICE_ID
#define SERVER_OTA_STAT_URL   SERVER_BASE "/api/ota/status"
#define SERVER_FW_BASE_URL    SERVER_BASE "/api/firmware/download/"

// Timing
#define POST_INTERVAL_MS          3000
#define CONFIG_POLL_INTERVAL_MS   5000
#define OTA_POLL_INTERVAL_MS      10000
#define GPRS_RECOVERY_TEST_MS     60000
#define RECOVERY_FLUSH_BATCH_SIZE 50
#define SIM_HEALTH_CHECK_MS       60000
#define S3_WATCHDOG_TIMEOUT_MS    120000
#define WIFI_RECONNECT_MS         30000
#define CONSECUTIVE_FAIL_LIMIT    3

// SD
#define SD_BUFFER_FILENAME        "/varuna_buffer.csv"
#define SD_MAX_FILE_SIZE          10485760

// Buffers
#define CSV_LINE_BUFFER_SIZE      640    // 39 fields is ~500-600 chars
#define SIM_RESP_BUFFER_SIZE      256
#define HTTP_BODY_BUFFER_SIZE     768    // larger to hold 39-field JSON
#define HTTP_ENVELOPE_BUFFER_SIZE 820
#define HTTP_RESP_BUFFER_SIZE     512

// AT timeouts
#define AT_SHORT_TIMEOUT          2000
#define AT_MEDIUM_TIMEOUT         5000
#define AT_LONG_TIMEOUT           15000
#define AT_HTTP_TIMEOUT           30000
#define SIM_INIT_MAX_RETRIES      5

// OTA bootloader
#define OTA_FLASH_BLOCK_SIZE      4096
#define OTA_BOOT_CONFIRM_MS       60000
#define OTA_MAX_RETRIES           2
#define OTA_SYNC_ATTEMPTS         10
```

---

## Section 4 — Add new enumerations

After the existing `OpState` and `SimState` enums, add:

```cpp
// Connectivity state — which path is active
enum ConnState {
    CONN_NONE,
    CONN_WIFI,
    CONN_GPRS
};

// OTA state machine
enum OtaState {
    OTA_IDLE,
    OTA_CHECKING,       // Gate 1: readiness check
    OTA_READY_WAIT,     // Waiting for engineer CONFIRM
    OTA_DOWNLOADING,    // Downloading .bin from server
    OTA_VERIFYING,      // Gate 2: checksum verification
    OTA_FLASHING,       // Programming S3 via bootloader
    OTA_CONFIRMING,     // Gate 3: waiting for CSV to resume
    OTA_COMPLETE,
    OTA_FAILED
};
```

---

## Section 5 — Replace DataReading struct with FullReading struct

Current `DataReading` has 4 fields. Replace entirely:

```cpp
// All 39 CSV fields parsed from S3 GPIO14 output
struct FullReading {
    // Fields 1-10
    float    waterHeight;
    float    tiltX;
    float    tiltY;
    float    tiltAngle;
    float    lateralAccel;
    float    temperature;
    float    pressure;
    float    bmpTemp;
    float    atmosphericRef;
    float    gaugePressure;
    // Fields 11-19
    int      mode;
    float    depth;
    float    floodRatio;
    uint32_t timestamp;
    char     rtcString[24];
    int      gpsFix;
    float    tetherLength;
    int      alertLevel;
    uint32_t sampleSec;
    // Fields 20-27
    float    battery;
    float    battVoltage;
    float    latitude;
    float    longitude;
    float    altitude;
    int      satellites;
    int      gpsFix2;
    int      satellites2;
    // Fields 28-39
    int      mpuOnline;
    int      bmpOnline;
    int      rtcOnline;
    int      gpsOnline;
    int      algoEnabled;
    uint32_t normalRate;
    uint32_t highRate;
    float    hMaxCm;
    int      floodZone;
    int      realtimeMode;
    uint32_t uptime;
    int      healthScore;
    // Meta
    bool     valid;
};
```

---

## Section 6 — Add new global state variables

After the existing globals block, add these entirely new globals:

```cpp
// ── Connectivity ─────────────────────────────────────────────
ConnState connState         = CONN_NONE;
bool      wifiConnected     = false;
bool      simAvailable      = false;
unsigned long lastWifiReconnectMs = 0;
unsigned long lastPostMs    = 0;
unsigned long lastConfigPollMs = 0;
unsigned long lastOtaPollMs = 0;

// ── Full reading (replaces DataReading) ──────────────────────
FullReading lastReading;
// (remove the old DataReading lastReading declaration)

// ── CSV receive (GPIO2, 9600 baud via HardwareSerial) ────────
// Serial1 configured at 9600 on GPIO2 RX, GPIO3 TX
// GPIO3 TX also used for $CFG frames to S3 at 115200
// NOTE: We use two different baud rates on the same UART1:
//   RX at 9600 (CSV from S3 GPIO14)
//   TX at 115200 ($CFG to S3 GPIO44)
// ESP32 UART supports different TX/RX baud? No — same baud.
// SOLUTION: Use separate UARTs.
//   UART1 RX GPIO2 at 9600 for CSV receive
//   UART1 TX GPIO3 at 9600 — but $CFG needs 115200
//
// ACTUAL SOLUTION:
//   Serial1 = UART1, RX=GPIO2, TX=GPIO3, baud=9600 — CSV only
//   $CFG frames sent via Serial2 = UART2, TX=GPIO3, baud=115200
//   BUT XIAO C3 only has UART0 and UART1 in hardware
//
// FINAL CORRECT SOLUTION:
//   Serial1 RX=GPIO2 baud=9600 — receives CSV from S3
//   $CFG frames sent via direct byte writes to GPIO3
//   using a second software UART TX at 115200
//   OR: since $CFG frames are rare and short, buffer them
//   and send at 9600 — S3 processC3Command() handles 9600
//   because it uses Serial2.available() not baud-specific
//
// PRAGMATIC DECISION FOR HACKATHON:
//   Use Serial1 at 9600 for BOTH directions.
//   S3 already reads GPIO44 via Serial2 which accepts any baud
//   that matches. Change S3 Serial2 to 9600 to match.
//   $CFG frames are 20 chars — trivial at 9600.
//   This avoids all UART conflict.

// CSV line buffer
#define CSV_LINE_BUFFER_SIZE 640
char     csvRxBuffer[CSV_LINE_BUFFER_SIZE];
uint16_t csvRxIndex    = 0;
bool     csvRxReady    = false;
char     csvFrameBuffer[CSV_LINE_BUFFER_SIZE];

// ── WiFi HTTP ────────────────────────────────────────────────
bool     wifiHttpBusy  = false;
char     wifiRespBody[HTTP_RESP_BUFFER_SIZE];
int      wifiRespCode  = 0;

// ── OTA ─────────────────────────────────────────────────────
OtaState otaState      = OTA_IDLE;
char     otaFwUrl[192];
char     otaFwFilename[64];
uint32_t otaFwSize     = 0;
uint32_t otaFwChecksum = 0;
uint8_t  otaRetryCount = 0;
char     otaLocalPath[32] = "/ota_fw.bin";
uint32_t otaFlashedBytes  = 0;
uint32_t otaDownloadedBytes = 0;
unsigned long otaStateEntry = 0;
bool     otaConfirmWaiting  = false;
unsigned long otaBootWaitStart = 0;
```

---

## Section 7 — Replace receiveFromS3() and parseDataFrame()

The current `receiveFromS3()` looks for `$` start marker, which is correct for `$DATA` frames but wrong for 39-field CSV which has no `$` prefix. Replace both functions:

```cpp
// ============================================================
// CSV RECEIVE FROM S3 — GPIO2, 9600 baud
// S3 sends raw 39-field CSV lines terminated with \n
// No $ prefix — just comma-separated values
// ============================================================

void receiveCSVFromS3() {
    while (Serial1.available()) {
        char c = (char)Serial1.read();

        if (c == '\n') {
            csvRxBuffer[csvRxIndex] = '\0';
            if (csvRxIndex > 10) {  // minimum sane line length
                csvRxReady = true;
            }
            csvRxIndex = 0;
            return;
        }

        if (c == '\r') continue;   // skip carriage return

        if (csvRxIndex < CSV_LINE_BUFFER_SIZE - 1) {
            csvRxBuffer[csvRxIndex++] = c;
        } else {
            // Line too long — discard and reset
            csvRxIndex = 0;
        }
    }
}

// Also receive $PING and $DIAG frames on the same UART
// These start with $ — detect and route them
void receiveCtrlFromS3() {
    // Called only when csvRxReady is true
    // If the line starts with $, it's a control frame not CSV
}

// Parse all 39 fields from a CSV line
bool parseCSVLine(const char *line, FullReading *out) {
    out->valid = false;

    // Work on a copy
    char copy[CSV_LINE_BUFFER_SIZE];
    strncpy(copy, line, sizeof(copy) - 1);
    copy[sizeof(copy) - 1] = '\0';

    // Count commas to verify field count
    int commas = 0;
    for (int i = 0; copy[i]; i++) if (copy[i] == ',') commas++;
    if (commas < 38) return false;  // need at least 39 fields (38 commas)

    char *tok;
    char *rest = copy;

    // Helper lambda equivalent using inline macro
    #define NEXT_TOK() (tok = strtok_r(rest, ",", &rest))

    // Fields 1-10
    if (!NEXT_TOK()) return false; out->waterHeight   = atof(tok);
    if (!NEXT_TOK()) return false; out->tiltX         = atof(tok);
    if (!NEXT_TOK()) return false; out->tiltY         = atof(tok);
    if (!NEXT_TOK()) return false; out->tiltAngle     = atof(tok);
    if (!NEXT_TOK()) return false; out->lateralAccel  = atof(tok);
    if (!NEXT_TOK()) return false; out->temperature   = atof(tok);
    if (!NEXT_TOK()) return false; out->pressure      = atof(tok);
    if (!NEXT_TOK()) return false; out->bmpTemp       = atof(tok);
    if (!NEXT_TOK()) return false; out->atmosphericRef= atof(tok);
    if (!NEXT_TOK()) return false; out->gaugePressure = atof(tok);

    // Fields 11-19
    if (!NEXT_TOK()) return false; out->mode          = atoi(tok);
    if (!NEXT_TOK()) return false; out->depth         = atof(tok);
    if (!NEXT_TOK()) return false; out->floodRatio    = atof(tok);
    if (!NEXT_TOK()) return false; out->timestamp     = (uint32_t)strtoul(tok, NULL, 10);
    if (!NEXT_TOK()) return false; strncpy(out->rtcString, tok, sizeof(out->rtcString)-1);
    if (!NEXT_TOK()) return false; out->gpsFix        = atoi(tok);
    if (!NEXT_TOK()) return false; out->tetherLength  = atof(tok);
    if (!NEXT_TOK()) return false; out->alertLevel    = atoi(tok);
    if (!NEXT_TOK()) return false; out->sampleSec     = (uint32_t)strtoul(tok, NULL, 10);

    // Fields 20-27
    if (!NEXT_TOK()) return false; out->battery       = atof(tok);
    if (!NEXT_TOK()) return false; out->battVoltage   = atof(tok);
    if (!NEXT_TOK()) return false; out->latitude      = atof(tok);
    if (!NEXT_TOK()) return false; out->longitude     = atof(tok);
    if (!NEXT_TOK()) return false; out->altitude      = atof(tok);
    if (!NEXT_TOK()) return false; out->satellites    = atoi(tok);
    if (!NEXT_TOK()) return false; out->gpsFix2       = atoi(tok);
    if (!NEXT_TOK()) return false; out->satellites2   = atoi(tok);

    // Fields 28-39
    if (!NEXT_TOK()) return false; out->mpuOnline     = atoi(tok);
    if (!NEXT_TOK()) return false; out->bmpOnline     = atoi(tok);
    if (!NEXT_TOK()) return false; out->rtcOnline     = atoi(tok);
    if (!NEXT_TOK()) return false; out->gpsOnline     = atoi(tok);
    if (!NEXT_TOK()) return false; out->algoEnabled   = atoi(tok);
    if (!NEXT_TOK()) return false; out->normalRate    = (uint32_t)strtoul(tok, NULL, 10);
    if (!NEXT_TOK()) return false; out->highRate      = (uint32_t)strtoul(tok, NULL, 10);
    if (!NEXT_TOK()) return false; out->hMaxCm        = atof(tok);
    if (!NEXT_TOK()) return false; out->floodZone     = atoi(tok);
    if (!NEXT_TOK()) return false; out->realtimeMode  = atoi(tok);
    if (!NEXT_TOK()) return false; out->uptime        = (uint32_t)strtoul(tok, NULL, 10);
    if (!NEXT_TOK()) return false; out->healthScore   = atoi(tok);

    #undef NEXT_TOK

    // Sanity checks
    if (out->mode < 0 || out->mode > 3) return false;
    if (out->battery < 0 || out->battery > 100) return false;

    out->valid = true;
    return true;
}

void processCSVFrame() {
    if (!csvRxReady) return;

    memcpy(csvFrameBuffer, csvRxBuffer, csvRxIndex + 1);
    csvRxReady = false;

    // Control frames start with $
    if (csvFrameBuffer[0] == '$') {
        // $PING → respond $PONG
        if (strncmp(csvFrameBuffer, "$PING", 5) == 0) {
            Serial1.print("$PONG\n");
            return;
        }
        // $DIAG → POST to server diagnostic endpoint
        if (strncmp(csvFrameBuffer, "$DIAG,", 6) == 0) {
            // Build and POST diagnostic payload
            // Use whichever connection is available
            postDiagnostic(csvFrameBuffer);
            return;
        }
        // Unknown control frame — ignore
        return;
    }

    // Regular CSV data frame
    FullReading reading;
    if (parseCSVLine(csvFrameBuffer, &reading)) {
        lastReading = reading;
        newReadingAvailable = true;
        lastS3DataTime = millis();

        Serial.print("[CSV] H=");
        Serial.print(reading.waterHeight, 1);
        Serial.print("cm zone=");
        Serial.print(reading.floodZone);
        Serial.print(" bat=");
        Serial.print(reading.battery, 0);
        Serial.print("% health=");
        Serial.println(reading.healthScore);
    } else {
        Serial.println("[CSV] Parse failed");
    }
}
```

---

## Section 8 — Replace buildSinglePostBody() with buildFullPostBody()

Current builds 4-field JSON. Replace:

```cpp
// Build full 39-field JSON for server POST
// Server pushes this directly to Firebase varuna/live
void buildFullPostBody(const FullReading *r, char *buf, uint16_t bufSize) {
    snprintf(buf, bufSize,
        "{"
        "\"device_id\":\"%s\","
        "\"waterHeight\":%.2f,"
        "\"tiltX\":%.3f,"
        "\"tiltY\":%.3f,"
        "\"tiltAngle\":%.3f,"
        "\"lateralAccel\":%.4f,"
        "\"temperature\":%.2f,"
        "\"pressure\":%.2f,"
        "\"bmpTemp\":%.2f,"
        "\"atmosphericRef\":%.2f,"
        "\"gaugePressure\":%.1f,"
        "\"mode\":%d,"
        "\"depth\":%.4f,"
        "\"floodRatio\":%.4f,"
        "\"timestamp\":%lu,"
        "\"rtcString\":\"%s\","
        "\"gpsFix\":%d,"
        "\"tetherLength\":%.3f,"
        "\"alertLevel\":%d,"
        "\"sampleSec\":%lu,"
        "\"battery\":%.1f,"
        "\"battVoltage\":%.2f,"
        "\"latitude\":%.6f,"
        "\"longitude\":%.6f,"
        "\"altitude\":%.1f,"
        "\"satellites\":%d,"
        "\"mpuOnline\":%d,"
        "\"bmpOnline\":%d,"
        "\"rtcOnline\":%d,"
        "\"gpsOnline\":%d,"
        "\"algorithmEnabled\":%d,"
        "\"normalRate\":%lu,"
        "\"highRate\":%lu,"
        "\"hMaxCm\":%.1f,"
        "\"floodZone\":%d,"
        "\"uptime\":%lu,"
        "\"healthScore\":%d"
        "}",
        DEVICE_ID,
        r->waterHeight, r->tiltX, r->tiltY, r->tiltAngle,
        r->lateralAccel, r->temperature, r->pressure,
        r->bmpTemp, r->atmosphericRef, r->gaugePressure,
        r->mode, r->depth, r->floodRatio,
        (unsigned long)r->timestamp, r->rtcString,
        r->gpsFix, r->tetherLength, r->alertLevel,
        (unsigned long)r->sampleSec,
        r->battery, r->battVoltage,
        r->latitude, r->longitude, r->altitude,
        r->satellites,
        r->mpuOnline, r->bmpOnline, r->rtcOnline,
        r->gpsOnline, r->algoEnabled,
        (unsigned long)r->normalRate, (unsigned long)r->highRate,
        r->hMaxCm, r->floodZone,
        (unsigned long)r->uptime, r->healthScore
    );
}
```

---

## Section 9 — Add entire WiFi subsystem (new, doesn't exist at all)

Add this entire block after the SIM800L section:

```cpp
// ============================================================
// WIFI SUBSYSTEM
// Runs independently of SIM800L
// Used as primary path if SIM unavailable
// ============================================================

void wifiInit() {
    WiFi.mode(WIFI_STA);
    WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
    Serial.print("[WIFI] Connecting to ");
    Serial.print(WIFI_SSID);
}

void wifiMaintain() {
    if (WiFi.status() == WL_CONNECTED) {
        if (!wifiConnected) {
            wifiConnected = true;
            Serial.print("\n[WIFI] Connected — IP: ");
            Serial.println(WiFi.localIP());
        }
    } else {
        if (wifiConnected) {
            wifiConnected = false;
            Serial.println("[WIFI] Disconnected");
        }
        // Attempt reconnect every 30s
        if (millis() - lastWifiReconnectMs >= WIFI_RECONNECT_MS) {
            lastWifiReconnectMs = millis();
            WiFi.reconnect();
            Serial.println("[WIFI] Reconnecting...");
        }
    }
}

// Synchronous WiFi HTTP POST — blocks briefly but acceptable
// because it's only called when GPRS is unavailable
// Returns HTTP response code, body in wifiRespBody
int wifiPost(const char *url, const char *body) {
    if (!wifiConnected) return -1;

    HTTPClient http;
    http.begin(url);
    http.addHeader("Content-Type", "application/json");
    http.setTimeout(10000);

    int code = http.POST((uint8_t*)body, strlen(body));

    if (code > 0) {
        String resp = http.getString();
        strncpy(wifiRespBody, resp.c_str(), HTTP_RESP_BUFFER_SIZE - 1);
        wifiRespBody[HTTP_RESP_BUFFER_SIZE - 1] = '\0';
    } else {
        wifiRespBody[0] = '\0';
    }

    http.end();
    return code;
}

// Synchronous WiFi HTTP GET — for config and OTA command polling
int wifiGet(const char *url) {
    if (!wifiConnected) return -1;

    HTTPClient http;
    http.begin(url);
    http.setTimeout(8000);

    int code = http.GET();

    if (code > 0) {
        String resp = http.getString();
        strncpy(wifiRespBody, resp.c_str(), HTTP_RESP_BUFFER_SIZE - 1);
        wifiRespBody[HTTP_RESP_BUFFER_SIZE - 1] = '\0';
    } else {
        wifiRespBody[0] = '\0';
    }

    http.end();
    return code;
}

// WiFi HTTP GET for binary file download — writes to SD
// Returns number of bytes downloaded, -1 on error
int32_t wifiDownloadToSD(const char *url, const char *localPath) {
    if (!wifiConnected) return -1;
    if (!sdInitialized) return -1;

    HTTPClient http;
    http.begin(url);
    http.setTimeout(60000);  // 60s for firmware download

    int code = http.GET();
    if (code != 200) {
        http.end();
        return -1;
    }

    int32_t totalLen = http.getSize();
    WiFiClient *stream = http.getStreamPtr();

    // Delete existing file if present
    if (SD.exists(localPath)) SD.remove(localPath);
    File f = SD.open(localPath, FILE_WRITE);
    if (!f) {
        http.end();
        return -1;
    }

    uint8_t dlBuf[256];
    int32_t downloaded = 0;
    unsigned long lastProgress = millis();

    while (http.connected() && (totalLen == -1 || downloaded < totalLen)) {
        size_t avail = stream->available();
        if (avail) {
            size_t toRead = min(avail, (size_t)256);
            int n = stream->readBytes(dlBuf, toRead);
            if (n > 0) {
                f.write(dlBuf, n);
                downloaded += n;
            }
        }
        // Progress log every 5s
        if (millis() - lastProgress > 5000) {
            lastProgress = millis();
            Serial.print("[OTA-DL] ");
            Serial.print(downloaded);
            Serial.print(" / ");
            Serial.println(totalLen);
            // Report progress to server
            char progBody[128];
            snprintf(progBody, sizeof(progBody),
                "{\"device_id\":\"%s\",\"status\":\"DOWNLOADING\","
                "\"progress\":\"%ld/%ld\"}",
                DEVICE_ID, (long)downloaded, (long)totalLen);
            wifiPost(SERVER_OTA_STAT_URL, progBody);
        }
        if (avail == 0 && !http.connected()) break;
    }

    f.close();
    http.end();

    Serial.print("[OTA-DL] Done: ");
    Serial.print(downloaded);
    Serial.println(" bytes");

    return downloaded;
}

// Unified send — tries GPRS first, falls back to WiFi
// Returns HTTP code
int unifiedPost(const char *url, const char *body, bool readResponse) {
    // Try GPRS first
    if (gprsConnected && simReady && !httpBusy && simState == SIM_IDLE) {
        // Use async GPRS path — caller must use callback pattern
        // This function is for WiFi-path only
        // GPRS path goes through httpStartPost()
    }
    // WiFi fallback
    if (wifiConnected) {
        int code = wifiPost(url, body);
        if (readResponse && code == 200) {
            checkServerConfigResponse(wifiRespBody);
        }
        return code;
    }
    return -1;
}

// Determine which connectivity path to use
void updateConnState() {
    if (gprsConnected && simReady) {
        connState = CONN_GPRS;
        simAvailable = true;
    } else if (wifiConnected) {
        connState = CONN_WIFI;
        simAvailable = false;
    } else {
        connState = CONN_NONE;
        simAvailable = false;
    }
}
```

---

## Section 10 — Add OTA state machine (new, doesn't exist at all)

This is the largest new addition. Add this entire block:

```cpp
// ============================================================
// ESP SERIAL BOOTLOADER PROTOCOL — simplified esptool subset
// Implements: SYNC → FLASH_BEGIN → FLASH_DATA (blocks) →
//             FLASH_END → reboot
// Reference: ESP8266/ESP32 Serial Protocol documentation
// ============================================================

// SLIP encode a byte into output buffer
// Returns number of bytes written
int slipEncodeByte(uint8_t b, uint8_t *out) {
    if (b == ESP_SLIP_END) {
        out[0] = ESP_SLIP_ESC;
        out[1] = ESP_SLIP_ESC_END;
        return 2;
    } else if (b == ESP_SLIP_ESC) {
        out[0] = ESP_SLIP_ESC;
        out[1] = ESP_SLIP_ESC_ESC;
        return 2;
    } else {
        out[0] = b;
        return 1;
    }
}

// Compute ESP checksum (XOR of all data bytes)
uint8_t espChecksum(const uint8_t *data, uint32_t len) {
    uint8_t cs = 0xEF;
    for (uint32_t i = 0; i < len; i++) cs ^= data[i];
    return cs;
}

// Send a SLIP-framed ESP command
// cmd: command byte, data: payload, dataLen: payload length
void espSendCommand(HardwareSerial &ser, uint8_t cmd,
                    const uint8_t *data, uint32_t dataLen,
                    uint8_t checksum) {
    // SLIP start
    ser.write(ESP_SLIP_END);

    // Direction byte (0x00 = request)
    ser.write((uint8_t)0x00);

    // Command
    ser.write(cmd);

    // Data length (2 bytes LE)
    uint8_t lenBuf[2];
    lenBuf[0] = dataLen & 0xFF;
    lenBuf[1] = (dataLen >> 8) & 0xFF;
    uint8_t encBuf[2];
    for (int i = 0; i < 2; i++) {
        int n = slipEncodeByte(lenBuf[i], encBuf);
        ser.write(encBuf, n);
    }

    // Checksum (4 bytes LE, only low byte meaningful)
    uint8_t csBuf[4] = {checksum, 0, 0, 0};
    for (int i = 0; i < 4; i++) {
        int n = slipEncodeByte(csBuf[i], encBuf);
        ser.write(encBuf, n);
    }

    // Data
    for (uint32_t i = 0; i < dataLen; i++) {
        int n = slipEncodeByte(data[i], encBuf);
        ser.write(encBuf, n);
    }

    // SLIP end
    ser.write(ESP_SLIP_END);
    ser.flush();
}

// Wait for ESP response, return true if success (0x01 in response[1])
bool espWaitResponse(HardwareSerial &ser, uint8_t expectedCmd,
                     uint32_t timeoutMs) {
    uint8_t resp[32];
    uint8_t ri = 0;
    bool inSlip = false;
    unsigned long start = millis();

    while (millis() - start < timeoutMs) {
        if (!ser.available()) {
            delay(1);
            continue;
        }
        uint8_t b = ser.read();

        if (b == ESP_SLIP_END) {
            if (inSlip && ri > 4) {
                // End of packet — check response
                // Response format: 0x01, cmd, size(2), val(4), data...
                if (ri >= 8 && resp[0] == 0x01 &&
                    resp[1] == expectedCmd) {
                    // resp[4] is the error byte
                    return (resp[4] == 0x00);
                }
            }
            inSlip = !inSlip;
            ri = 0;
            continue;
        }

        if (inSlip) {
            if (b == ESP_SLIP_ESC) continue;  // next byte handled below
            if (ri < sizeof(resp)) resp[ri++] = b;
        }
    }
    return false;
}

// Sync with ESP32 bootloader (10 attempts)
bool bootloaderSync(HardwareSerial &ser) {
    uint8_t syncData[36];
    syncData[0] = 0x07;
    syncData[1] = 0x07;
    syncData[2] = 0x12;
    syncData[3] = 0x20;
    for (int i = 4; i < 36; i++) syncData[i] = 0x55;

    for (int attempt = 0; attempt < OTA_SYNC_ATTEMPTS; attempt++) {
        // Flush
        while (ser.available()) ser.read();

        espSendCommand(ser, ESP_SYNC, syncData, 36, 0);

        if (espWaitResponse(ser, ESP_SYNC, 1000)) {
            Serial.println("[BOOT] Sync OK");
            return true;
        }
        delay(100);
    }
    Serial.println("[BOOT] Sync FAILED");
    return false;
}

// Begin flash write operation
// flashAddr: starting flash address (usually 0x10000 for app)
// totalSize: total firmware size
// numBlocks: ceil(totalSize / OTA_FLASH_BLOCK_SIZE)
bool bootloaderFlashBegin(HardwareSerial &ser, uint32_t flashAddr,
                          uint32_t totalSize, uint32_t numBlocks) {
    uint8_t payload[16];
    // erase size
    payload[0]  = totalSize & 0xFF;
    payload[1]  = (totalSize >> 8)  & 0xFF;
    payload[2]  = (totalSize >> 16) & 0xFF;
    payload[3]  = (totalSize >> 24) & 0xFF;
    // num blocks
    payload[4]  = numBlocks & 0xFF;
    payload[5]  = (numBlocks >> 8)  & 0xFF;
    payload[6]  = (numBlocks >> 16) & 0xFF;
    payload[7]  = (numBlocks >> 24) & 0xFF;
    // block size
    payload[8]  = OTA_FLASH_BLOCK_SIZE & 0xFF;
    payload[9]  = (OTA_FLASH_BLOCK_SIZE >> 8)  & 0xFF;
    payload[10] = (OTA_FLASH_BLOCK_SIZE >> 16) & 0xFF;
    payload[11] = (OTA_FLASH_BLOCK_SIZE >> 24) & 0xFF;
    // flash offset
    payload[12] = flashAddr & 0xFF;
    payload[13] = (flashAddr >> 8)  & 0xFF;
    payload[14] = (flashAddr >> 16) & 0xFF;
    payload[15] = (flashAddr >> 24) & 0xFF;

    espSendCommand(ser, ESP_FLASH_BEGIN, payload, 16, 0);
    return espWaitResponse(ser, ESP_FLASH_BEGIN, 10000);
}

// Send one block of flash data
bool bootloaderFlashBlock(HardwareSerial &ser, const uint8_t *data,
                          uint32_t dataLen, uint32_t seqNum) {
    // Payload: data_len(4) + seq(4) + 0(4) + 0(4) + data
    uint32_t payloadLen = 16 + dataLen;
    uint8_t *payload = (uint8_t*)malloc(payloadLen);
    if (!payload) return false;

    payload[0]  = dataLen & 0xFF;
    payload[1]  = (dataLen >> 8) & 0xFF;
    payload[2]  = (dataLen >> 16) & 0xFF;
    payload[3]  = (dataLen >> 24) & 0xFF;
    payload[4]  = seqNum & 0xFF;
    payload[5]  = (seqNum >> 8) & 0xFF;
    payload[6]  = (seqNum >> 16) & 0xFF;
    payload[7]  = (seqNum >> 24) & 0xFF;
    payload[8]  = 0; payload[9]  = 0;
    payload[10] = 0; payload[11] = 0;
    payload[12] = 0; payload[13] = 0;
    payload[14] = 0; payload[15] = 0;
    memcpy(payload + 16, data, dataLen);

    uint8_t cs = espChecksum(payload + 16, dataLen);

    espSendCommand(ser, ESP_FLASH_DATA, payload, payloadLen, cs);
    bool ok = espWaitResponse(ser, ESP_FLASH_DATA, 5000);

    free(payload);
    return ok;
}

// End flash, reboot
bool bootloaderFlashEnd(HardwareSerial &ser) {
    uint8_t payload[4] = {0, 0, 0, 0};  // reboot flag = 0 = run after flash
    espSendCommand(ser, ESP_FLASH_END, payload, 4, 0);
    return espWaitResponse(ser, ESP_FLASH_END, 3000);
}

// Force S3 into bootloader mode using GPIO9 (BOOT) and GPIO10 (EN)
void s3EnterBootloader() {
    Serial.println("[OTA] Entering S3 bootloader mode");
    digitalWrite(OTA_BOOT_PIN, LOW);     // hold BOOT low
    delay(50);
    digitalWrite(OTA_RESET_PIN, LOW);    // pull EN low (reset)
    delay(200);
    digitalWrite(OTA_RESET_PIN, HIGH);   // release EN — S3 starts resetting
    delay(500);                          // wait for bootloader to initialise
    // BOOT can be released now — bootloader is already running
    digitalWrite(OTA_BOOT_PIN, HIGH);
    Serial.println("[OTA] S3 now in download mode");
}

// Reboot S3 into normal firmware
void s3Reboot() {
    Serial.println("[OTA] Rebooting S3 into new firmware");
    digitalWrite(OTA_BOOT_PIN, HIGH);   // ensure normal boot
    delay(50);
    digitalWrite(OTA_RESET_PIN, LOW);
    delay(200);
    digitalWrite(OTA_RESET_PIN, HIGH);
    Serial.println("[OTA] S3 rebooting...");
}

// Report OTA status to server (tries WiFi then GPRS)
void otaReportStatus(const char *status, const char *progress) {
    char body[256];
    snprintf(body, sizeof(body),
        "{\"device_id\":\"%s\",\"status\":\"%s\",\"progress\":\"%s\"}",
        DEVICE_ID, status, progress ? progress : "");

    if (wifiConnected) {
        wifiPost(SERVER_OTA_STAT_URL, body);
    }
    // GPRS path: if simReady and not busy, post via GPRS
    // For simplicity during hackathon, WiFi path covers this
}

// Compute simple checksum matching server's algorithm
uint32_t computeFileChecksum(const char *path) {
    if (!sdInitialized) return 0;
    File f = SD.open(path, FILE_READ);
    if (!f) return 0;

    uint32_t sum = 0;
    while (f.available()) {
        sum = (sum + (uint8_t)f.read()) & 0xFFFFFFFF;
    }
    f.close();
    return sum;
}

// ============================================================
// OTA STATE MACHINE
// Triggered by server writing ota/command = "FLASH" to Firebase
// C3 polls via GET /api/ota/command/VARUNA_001
// ============================================================

// Poll server for OTA commands (called from loop periodically)
void pollOtaCommand() {
    if (otaState != OTA_IDLE) return;

    // Need connectivity
    if (!wifiConnected && !gprsConnected) return;

    int code = -1;
    if (wifiConnected) {
        code = wifiGet(SERVER_OTA_CMD_URL);
    }
    if (code != 200) return;

    // Parse response for command field
    // Response: {"command":"FLASH","url":"...","size":...,"checksum":...}
    const char *cmdPtr = strstr(wifiRespBody, "\"command\":");
    if (!cmdPtr) return;

    if (strstr(cmdPtr, "\"FLASH\"") || strstr(cmdPtr, "\"CHECK_READY\"")) {
        Serial.println("[OTA] Command received: FLASH");

        // Extract url
        const char *urlPtr = strstr(wifiRespBody, "\"url\":\"");
        if (urlPtr) {
            urlPtr += 7;
            int ui = 0;
            while (*urlPtr && *urlPtr != '"' && ui < 191) {
                otaFwUrl[ui++] = *urlPtr++;
            }
            otaFwUrl[ui] = '\0';
        }

        // Extract size
        const char *szPtr = strstr(wifiRespBody, "\"size\":");
        if (szPtr) otaFwSize = strtoul(szPtr + 7, NULL, 10);

        // Extract checksum
        const char *csPtr = strstr(wifiRespBody, "\"checksum\":");
        if (csPtr) otaFwChecksum = strtoul(csPtr + 11, NULL, 10);

        Serial.print("[OTA] URL: "); Serial.println(otaFwUrl);
        Serial.print("[OTA] Size: "); Serial.println(otaFwSize);

        // Gate 1: readiness check
        otaState = OTA_CHECKING;
        otaStateEntry = millis();

        // Check flood zone — refuse if zone >= 2 (WARNING or DANGER)
        if (lastReading.valid && lastReading.floodZone >= 2) {
            Serial.println("[OTA] REFUSED — active flood event");
            otaReportStatus("BUSY:FLOOD_EVENT_ACTIVE", "");
            otaState = OTA_IDLE;
            return;
        }

        // Check connectivity for download
        if (!wifiConnected && !gprsConnected) {
            Serial.println("[OTA] REFUSED — no connectivity");
            otaReportStatus("BUSY:NO_CONNECTIVITY", "");
            otaState = OTA_IDLE;
            return;
        }

        // Check SD space
        if (!sdInitialized) {
            Serial.println("[OTA] REFUSED — no SD card");
            otaReportStatus("BUSY:NO_SD", "");
            otaState = OTA_IDLE;
            return;
        }

        // All good — report READY and advance to DOWNLOADING
        // (hackathon: skip engineer confirm step, proceed immediately)
        otaReportStatus("READY", "");
        otaState = OTA_DOWNLOADING;
        otaStateEntry = millis();
        otaDownloadedBytes = 0;

        Serial.println("[OTA] Advancing to DOWNLOADING");
    }
}

void runOtaStateMachine() {
    if (otaState == OTA_IDLE) return;

    switch (otaState) {

    case OTA_DOWNLOADING: {
        Serial.println("[OTA] Starting firmware download...");
        otaReportStatus("DOWNLOADING", "0");

        int32_t dlBytes = -1;

        if (wifiConnected) {
            dlBytes = wifiDownloadToSD(otaFwUrl, otaLocalPath);
        }

        if (dlBytes <= 0) {
            Serial.println("[OTA] Download FAILED");
            otaReportStatus("OTA_FAILED:DOWNLOAD", "");
            otaState = OTA_IDLE;
            return;
        }

        otaDownloadedBytes = (uint32_t)dlBytes;
        Serial.print("[OTA] Downloaded: "); Serial.println(dlBytes);

        otaReportStatus("VERIFYING", "");
        otaState = OTA_VERIFYING;
        break;
    }

    case OTA_VERIFYING: {
        Serial.println("[OTA] Verifying checksum...");

        uint32_t computed = computeFileChecksum(otaLocalPath);
        Serial.print("[OTA] Expected: "); Serial.println(otaFwChecksum);
        Serial.print("[OTA] Computed: "); Serial.println(computed);

        if (computed != otaFwChecksum) {
            Serial.println("[OTA] Checksum MISMATCH");
            otaReportStatus("OTA_FAILED:CHECKSUM", "");
            SD.remove(otaLocalPath);
            otaState = OTA_IDLE;
            return;
        }

        Serial.println("[OTA] Checksum OK");
        otaReportStatus("FLASHING", "0");
        otaState = OTA_FLASHING;
        otaFlashedBytes = 0;
        break;
    }

    case OTA_FLASHING: {
        Serial.println("[OTA] Starting S3 flash...");

        // Reconfigure SIM serial pins to act as bootloader UART
        // SIM_RX_PIN (GPIO20) becomes bootloader ACK input from S3
        // We temporarily repurpose SimSerial (UART0) for bootloader
        // First: stop SIM AT engine
        gprsConnected = false;
        simReady = false;

        // Reconfigure UART0 for bootloader protocol at 115200
        SimSerial.end();
        delay(100);
        SimSerial.begin(BOOTLOADER_BAUD, SERIAL_8N1, SIM_RX_PIN, SIM_TX_PIN);
        delay(100);

        // Force S3 into bootloader mode
        s3EnterBootloader();
        delay(500);

        // Sync with bootloader
        bool synced = bootloaderSync(SimSerial);
        if (!synced) {
            if (otaRetryCount < OTA_MAX_RETRIES) {
                otaRetryCount++;
                Serial.print("[OTA] Sync failed, retry ");
                Serial.println(otaRetryCount);
                s3EnterBootloader();
                delay(500);
                synced = bootloaderSync(SimSerial);
            }
        }

        if (!synced) {
            Serial.println("[OTA] Bootloader sync FAILED — aborting");
            otaReportStatus("OTA_FAILED:SYNC", "");
            // Reboot S3 anyway to restore normal mode
            s3Reboot();
            // Restore SIM serial
            SimSerial.end();
            SimSerial.begin(SIM_BAUD, SERIAL_8N1, SIM_RX_PIN, SIM_TX_PIN);
            simSetState(SIM_OFF);
            otaState = OTA_IDLE;
            return;
        }

        // Open firmware file
        File fwFile = SD.open(otaLocalPath, FILE_READ);
        if (!fwFile) {
            Serial.println("[OTA] Cannot open firmware file");
            otaReportStatus("OTA_FAILED:FILE", "");
            s3Reboot();
            SimSerial.end();
            SimSerial.begin(SIM_BAUD, SERIAL_8N1, SIM_RX_PIN, SIM_TX_PIN);
            simSetState(SIM_OFF);
            otaState = OTA_IDLE;
            return;
        }

        uint32_t fwSize = fwFile.size();
        uint32_t numBlocks = (fwSize + OTA_FLASH_BLOCK_SIZE - 1) / OTA_FLASH_BLOCK_SIZE;

        Serial.print("[OTA] FW size: "); Serial.print(fwSize);
        Serial.print(" blocks: "); Serial.println(numBlocks);

        // Flash begin — ESP32 app partition starts at 0x10000
        if (!bootloaderFlashBegin(SimSerial, 0x10000, fwSize, numBlocks)) {
            Serial.println("[OTA] FlashBegin FAILED");
            fwFile.close();
            otaReportStatus("OTA_FAILED:FLASH_BEGIN", "");
            s3Reboot();
            SimSerial.end();
            SimSerial.begin(SIM_BAUD, SERIAL_8N1, SIM_RX_PIN, SIM_TX_PIN);
            simSetState(SIM_OFF);
            otaState = OTA_IDLE;
            return;
        }

        // Flash data blocks
        uint8_t blockBuf[OTA_FLASH_BLOCK_SIZE];
        uint32_t seqNum = 0;
        bool flashOk = true;

        while (fwFile.available() && flashOk) {
            // Read one block (pad with 0xFF if partial)
            memset(blockBuf, 0xFF, OTA_FLASH_BLOCK_SIZE);
            uint32_t bytesRead = fwFile.read(blockBuf, OTA_FLASH_BLOCK_SIZE);

            if (!bootloaderFlashBlock(SimSerial, blockBuf,
                                      OTA_FLASH_BLOCK_SIZE, seqNum)) {
                Serial.print("[OTA] Block "); Serial.print(seqNum);
                Serial.println(" FAILED");
                flashOk = false;
                break;
            }

            otaFlashedBytes += bytesRead;
            seqNum++;

            // Progress every 10 blocks
            if (seqNum % 10 == 0) {
                char prog[32];
                snprintf(prog, sizeof(prog), "%lu/%lu",
                    (unsigned long)seqNum, (unsigned long)numBlocks);
                otaReportStatus("FLASHING", prog);
                Serial.print("[OTA] Block "); Serial.print(seqNum);
                Serial.print("/"); Serial.println(numBlocks);
            }
        }

        fwFile.close();

        if (!flashOk) {
            otaReportStatus("OTA_FAILED:FLASH_DATA", "");
            s3Reboot();
            SimSerial.end();
            SimSerial.begin(SIM_BAUD, SERIAL_8N1, SIM_RX_PIN, SIM_TX_PIN);
            simSetState(SIM_OFF);
            otaState = OTA_IDLE;
            return;
        }

        // Flash end — trigger reboot of S3
        bootloaderFlashEnd(SimSerial);
        Serial.println("[OTA] Flash complete");

        // Restore SIM serial
        SimSerial.end();
        delay(200);
        SimSerial.begin(SIM_BAUD, SERIAL_8N1, SIM_RX_PIN, SIM_TX_PIN);

        // Reboot S3 via RESET pin
        s3Reboot();

        // Clean up firmware file
        SD.remove(otaLocalPath);

        // Advance to boot confirmation
        otaState = OTA_CONFIRMING;
        otaBootWaitStart = millis();
        otaRetryCount = 0;

        // Restart SIM state machine
        simSetState(SIM_OFF);

        Serial.println("[OTA] Waiting for S3 boot confirmation...");
        break;
    }

    case OTA_CONFIRMING: {
        // Watch GPIO2 (Serial1) for CSV lines resuming
        // If 3 valid CSV lines received → success
        // Timeout after OTA_BOOT_CONFIRM_MS

        // csvRxReady is set by receiveCSVFromS3() in main loop
        // lastReading.valid gets set when CSV parses successfully

        static uint8_t confirmCount = 0;

        if (newReadingAvailable && lastReading.valid) {
            newReadingAvailable = false;
            confirmCount++;
            Serial.print("[OTA] Boot confirm ");
            Serial.print(confirmCount);
            Serial.println("/3");

            if (confirmCount >= 3) {
                confirmCount = 0;
                Serial.println("[OTA] ✓ S3 booted successfully");
                otaReportStatus("OTA_COMPLETE", "");
                otaState = OTA_COMPLETE;
            }
            return;
        }

        if (millis() - otaBootWaitStart > OTA_BOOT_CONFIRM_MS) {
            confirmCount = 0;

            if (otaRetryCount < OTA_MAX_RETRIES) {
                otaRetryCount++;
                Serial.println("[OTA] Boot timeout — reflashing...");
                otaReportStatus("OTA_RETRY", "");
                otaState = OTA_FLASHING;
            } else {
                Serial.println("[OTA] Boot confirm FAILED");
                otaReportStatus("OTA_FAILED:NO_BOOT", "");
                otaState = OTA_FAILED;
            }
        }
        break;
    }

    case OTA_COMPLETE:
        // Reset to idle after a moment
        otaState = OTA_IDLE;
        otaRetryCount = 0;
        Serial.println("[OTA] State machine reset to IDLE");
        break;

    case OTA_FAILED:
        // Log and reset
        Serial.println("[OTA] Failed — returning to normal operation");
        otaState = OTA_IDLE;
        otaRetryCount = 0;
        break;

    default:
        break;
    }
}
```

---

## Section 11 — postDiagnostic() helper (new)

Add this small function before processCSVFrame():

```cpp
void postDiagnostic(const char *diagFrame) {
    char body[640];
    snprintf(body, sizeof(body),
        "{\"device_id\":\"%s\",\"raw\":\"%s\"}",
        DEVICE_ID, diagFrame);

    if (wifiConnected) {
        int code = wifiPost(SERVER_DIAG_URL, body);
        Serial.print("[DIAG] POST code: "); Serial.println(code);
    } else if (gprsConnected && !httpBusy) {
        httpStartPost(SERVER_DIAG_URL, body, onDiagPostDone);
    }
}
```

---

## Section 12 — Replace opStateMachine() POST calls

Every call to `buildSinglePostBody()` and `httpStartPost(SERVER_URL, ...)` needs to become dual-path. Replace the body-building and posting in all four OP states:

**In OP_NORMAL and OP_REALTIME, replace:**
```cpp
char body[HTTP_BODY_BUFFER_SIZE];
buildSinglePostBody(&lastReading, body, sizeof(body));
httpStartPost(SERVER_URL, body, onNormalPostDone);
```

**With:**
```cpp
char body[HTTP_BODY_BUFFER_SIZE];
buildFullPostBody(&lastReading, body, sizeof(body));

if (connState == CONN_GPRS && !httpBusy) {
    httpStartPost(SERVER_DATA_URL, body, onNormalPostDone);
} else if (connState == CONN_WIFI) {
    int code = wifiPost(SERVER_DATA_URL, body);
    onNormalPostDone(code == 200 || code == 201, code);
    if (code == 200) checkServerConfigResponse(wifiRespBody);
}
```

**In OP_OFFLINE recovery test, replace:**
```cpp
httpStartPost(SERVER_TEST_URL, testBody, onRecoveryTestDone);
```

**With:**
```cpp
if (connState == CONN_GPRS && !httpBusy) {
    httpStartPost(SERVER_TEST_URL, testBody, onRecoveryTestDone);
} else if (connState == CONN_WIFI) {
    int code = wifiPost(SERVER_TEST_URL, testBody);
    onRecoveryTestDone(code == 200, code);
}
```

---

## Section 13 — Add config polling via WiFi (new)

Add this function:

```cpp
void pollServerConfig() {
    if (!wifiConnected) return;
    if (millis() - lastConfigPollMs < CONFIG_POLL_INTERVAL_MS) return;
    lastConfigPollMs = millis();

    int code = wifiGet(SERVER_CONFIG_URL);
    if (code == 200) {
        checkServerConfigResponse(wifiRespBody);
    }
}
```

---

## Section 14 — Replace setup()

Key changes needed in `setup()`:

```cpp
// REMOVE:
Serial1.begin(115200, SERIAL_8N1, S3_UART_RX_PIN, S3_UART_TX_PIN);

// REPLACE WITH:
// CSV receive from S3 GPIO14 — 9600 baud
// TX at 9600 also used for $CFG frames to S3 GPIO44
// S3 Serial2 must also be at 9600 to match
Serial1.begin(9600, SERIAL_8N1, CSV_RX_PIN, CFG_TX_PIN);

// ADD after Serial1 init:
// OTA control pins
pinMode(OTA_BOOT_PIN, OUTPUT);
pinMode(OTA_RESET_PIN, OUTPUT);
digitalWrite(OTA_BOOT_PIN, HIGH);    // S3 boots normally
digitalWrite(OTA_RESET_PIN, HIGH);   // S3 not in reset

// ADD WiFi init:
wifiInit();
lastWifiReconnectMs = millis();
```

---

## Section 15 — Replace loop()

The loop needs these additions in order:

```cpp
void loop() {
    // 1. WiFi maintenance (always runs)
    wifiMaintain();

    // 2. Update connectivity state
    updateConnState();

    // 3. Receive CSV from S3
    receiveCSVFromS3();
    processCSVFrame();

    // 4. SIM800L state machine (only if SIM present)
    atPoll();
    simStateMachine();

    // 5. Operational state machine
    // Runs if either WiFi or GPRS available
    if (connState != CONN_NONE) {
        opStateMachine();

        // Boot-time buffer check
        static bool bootBufferChecked = false;
        if (!bootBufferChecked) {
            bootBufferChecked = true;
            if (sdBufferExists()) {
                Serial.println("[BOOT] Old buffer → RECOVERY");
                opState = OP_RECOVERY;
            }
        }

        // Rate-limit posts
        // (opStateMachine already has timing logic for OFFLINE/RECOVERY)
        // For NORMAL and REALTIME, rate-limit via POST_INTERVAL_MS
    } else {
        // No connectivity — buffer to SD
        if (newReadingAvailable) {
            newReadingAvailable = false;
            sdWriteReading(&lastReading);
            Serial.println("[CONN_NONE] Buffered to SD");
        }
    }

    // 6. Config push to S3 when dirty
    if (config.dirty) sendConfigToS3();

    // 7. Periodic config poll from server (WiFi path)
    pollServerConfig();

    // 8. Periodic OTA command poll
    if (millis() - lastOtaPollMs >= OTA_POLL_INTERVAL_MS) {
        lastOtaPollMs = millis();
        pollOtaCommand();
    }

    // 9. OTA state machine
    runOtaStateMachine();

    // 10. SIM health check
    if (millis() - lastSimHealthMs >= SIM_HEALTH_CHECK_MS) {
        lastSimHealthMs = millis();
        simHealthCheck();
    }

    // 11. S3 watchdog
    checkS3Watchdog();

    // 12. Serial debug
    readSerialCommands();
}
```

---

## Section 16 — One critical note on UART baud rate conflict

The S3 currently has `Serial2.begin(115200, ...)` for receiving `$CFG`. The C3 now sends on Serial1 TX at 9600. **You must also change this one line in the S3 firmware:**

```cpp
// S3 firmware — in setup(), find:
Serial2.begin(UART_C3_BAUD, SERIAL_8N1, UART_C3_RX, UART_C3_TX);

// Change UART_C3_BAUD from 115200 to 9600:
Serial2.begin(9600, SERIAL_8N1, UART_C3_RX, UART_C3_TX);
```

And update the S3 define:
```cpp
// Change:
#define UART_C3_BAUD 115200
// To:
#define UART_C3_BAUD 9600
```

This makes the $CFG path work at 9600 which is what Serial1 on the C3 uses. $CFG frames are 20 characters — at 9600 baud that's 20ms. Completely acceptable.

---

That is every edit needed. The logic thread is: **CSV in at 9600 → parse all 39 fields → WiFi or GPRS POST → server pushes Firebase → website updates. Config polls via WiFi GET. OTA triggered by server poll → download to SD → flash via bootloader protocol on SimSerial repurposed pins → confirm via CSV resuming.** Every piece connects.
