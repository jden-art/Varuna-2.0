// ============================================================
// VARUNA FLOOD MONITORING SYSTEM — ESP32-C3 COMMS CONTROLLER
// Complete rewrite aligned to system specification
// All 7 gaps fixed per review
// ============================================================

#include <SPI.h>
#include <SD.h>
#include <WiFi.h>
#include <Firebase_ESP_Client.h>
#include <ArduinoJson.h>
#include <HardwareSerial.h>

// ============================================================
// PIN DEFINITIONS — XIAO ESP32-C3
// ============================================================
// D0/GPIO2  → S3 TX (receives $DATA)
// D1/GPIO3  → S3 RX (sends $CFG)
// D2/GPIO4  → SD SCK
// D3/GPIO5  → SD MISO
// D4/GPIO6  → SD MOSI
// D5/GPIO7  → SD CS
// D6/GPIO21 → SIM800L RX (C3 sends AT commands)
// D7/GPIO20 → SIM800L TX (C3 receives responses)
// D8/GPIO8  → SIM800L RST
// D9/GPIO9  → FREE
// D10/GPIO10 → FREE

#define S3_UART_RX_PIN    2
#define S3_UART_TX_PIN    3

#define SD_SCK_PIN        4
#define SD_MISO_PIN       5
#define SD_MOSI_PIN       6
#define SD_CS_PIN         7

#define SIM_TX_PIN        21    // C3 TX → SIM800L RXD
#define SIM_RX_PIN        20    // C3 RX ← SIM800L TXD
#define SIM_RST_PIN       8

// ============================================================
// CONFIGURATION CONSTANTS
// ============================================================

// Device identity
#define DEVICE_ID         "VARUNA_001"

// GPRS / SIM800L
#define SIM_APN           "internet"
#define SIM_BAUD          9600
#define SERVER_URL        "http://your-server.com/api/flood/data"
#define SERVER_BATCH_URL  "http://your-server.com/api/flood/batch"
#define SERVER_TEST_URL   "http://your-server.com/api/flood/ping"

// Firebase (WiFi used only if available — primary path is GPRS)
// For SIM800L-only deployments, Firebase commands arrive via
// server polling instead. See note at bottom.
#define WIFI_SSID         ""
#define WIFI_PASSWORD     ""
#define FIREBASE_HOST     "your-project.firebaseio.com"
#define FIREBASE_AUTH     "your-database-secret"
#define FIREBASE_PATH     "/devices/" DEVICE_ID "/config"

// Timing
#define GPRS_RECOVERY_TEST_MS     60000     // 60s between recovery pings
#define RECOVERY_FLUSH_BATCH_SIZE 50        // Lines per batch POST
#define SIM_HEALTH_CHECK_MS       60000
#define S3_WATCHDOG_TIMEOUT_MS    120000    // 2 min — S3 may sample every 15min
#define CONSECUTIVE_FAIL_LIMIT    3

// SD
#define SD_BUFFER_FILENAME        "/varuna_buffer.csv"
#define SD_MAX_FILE_SIZE          10485760  // 10 MB

// UART buffers
#define S3_RX_BUFFER_SIZE         128       // $DATA frames are short
#define SIM_RESP_BUFFER_SIZE      256
#define HTTP_BODY_BUFFER_SIZE     512

// AT command timeouts
#define AT_SHORT_TIMEOUT          2000
#define AT_MEDIUM_TIMEOUT         5000
#define AT_LONG_TIMEOUT           15000
#define AT_HTTP_TIMEOUT           30000

// SIM init
#define SIM_INIT_MAX_RETRIES      5

// ============================================================
// OPERATIONAL STATE MACHINE (the system's brain)
// ============================================================
enum OpState {
    OP_NORMAL,
    OP_OFFLINE,
    OP_RECOVERY,
    OP_REALTIME
};

// ============================================================
// SIM800L STATE MACHINE (low-level GPRS management)
// ============================================================
enum SimState {
    SIM_OFF,
    SIM_RESETTING,
    SIM_WAIT_READY,
    SIM_CHECK_PIN,
    SIM_REGISTERING,
    SIM_GPRS_ATTACH,
    SIM_GPRS_BEARER,
    SIM_IDLE,
    SIM_HTTP_INIT,
    SIM_HTTP_SET_URL,
    SIM_HTTP_SET_CONTENT,
    SIM_HTTP_SET_DATA_SIZE,
    SIM_HTTP_SEND_BODY,
    SIM_HTTP_ACTION,
    SIM_HTTP_READ_RESPONSE,
    SIM_HTTP_TERM,
    SIM_ERROR
};

// ============================================================
// ASYNC AT ENGINE
// ============================================================
enum ATState {
    AT_IDLE,
    AT_WAITING
};

struct AsyncAT {
    ATState state;
    char expected[32];
    char errorStr[16];
    char response[SIM_RESP_BUFFER_SIZE];
    uint16_t respIndex;
    unsigned long sentTime;
    unsigned long timeout;
    bool complete;
    bool success;
};

// ============================================================
// PARSED $DATA FRAME
// ============================================================
struct DataReading {
    uint32_t timestamp;
    float    h_cm;
    uint8_t  mode;       // 0=slack, 1=taut, 2=flood, 3=submerged
    uint8_t  bat_pct;
    bool     valid;
};

// ============================================================
// DEVICE CONFIG (from Firebase / server)
// ============================================================
struct DeviceConfig {
    uint16_t normal_sec;    // Sampling interval below 50%
    uint16_t high_sec;      // Sampling interval above 80%
    uint16_t h_max_cm;      // Flood threshold
    bool     realtime;      // Realtime mode toggle
    bool     dirty;         // Needs to be sent to S3
};

// ============================================================
// FORWARD DECLARATIONS (Gap 2 + Gap 5 fix)
// ============================================================
void httpFinish(bool success, int code);
void handleRealtimeCommand(bool enable);
void checkServerConfigResponse(const char *responseBody);

// ============================================================
// GLOBAL STATE
// ============================================================

// Hardware serial for SIM800L (UART0 remapped)
HardwareSerial SimSerial(0);

// --- Operational state ---
OpState opState = OP_NORMAL;
OpState opPrevState = OP_NORMAL;

// --- S3 UART reception ---
volatile char    s3RxBuffer[S3_RX_BUFFER_SIZE];
volatile uint8_t s3RxIndex = 0;
volatile bool    s3RxReady = false;

char             s3FrameBuffer[S3_RX_BUFFER_SIZE];   // Copied from ISR buffer
DataReading      lastReading;
bool             newReadingAvailable = false;
unsigned long    lastS3DataTime = 0;

// --- SIM800L ---
SimState  simState = SIM_OFF;
unsigned long simStateEntry = 0;
uint8_t  simRetryCount = 0;
bool     simReady = false;
bool     gprsConnected = false;

AsyncAT  asyncAT;

// --- SIM sub-step globals (Gap 7 fix — promoted from static) ---
uint8_t  attachPhase = 0;
uint8_t  bearerStep = 0;
uint8_t  actionPhase = 0;
uint8_t  httpReadPhase = 0;

// --- HTTP transaction ---
char     httpBody[HTTP_BODY_BUFFER_SIZE];
char     httpUrl[128];
char     httpResponseBody[HTTP_BODY_BUFFER_SIZE];    // Server response body
int      httpResponseCode = 0;
bool     httpBusy = false;

typedef void (*HttpCallback)(bool success, int code);
HttpCallback httpDoneCallback = NULL;

// --- Operational counters ---
uint8_t  consecutiveFails = 0;
uint32_t totalTransmissions = 0;
uint32_t successfulTransmissions = 0;
uint32_t failedTransmissions = 0;

// --- SD card ---
bool     sdInitialized = false;
uint32_t sdLinesWritten = 0;

// --- Config ---
DeviceConfig config = {
    .normal_sec = 900,
    .high_sec   = 60,
    .h_max_cm   = 200,
    .realtime   = false,
    .dirty      = false
};

bool     realtimeQueued = false;    // RT command received while OFFLINE

// --- Timers ---
unsigned long lastRecoveryTestMs = 0;
unsigned long lastSimHealthMs = 0;
unsigned long lastRecoveryFlushMs = 0;

// --- Recovery flush state ---
File     recoveryFile;
bool     recoveryFlushActive = false;
uint32_t recoveryLinesRead = 0;
char     recoveryBatch[HTTP_BODY_BUFFER_SIZE];
uint32_t recoveryBytesConsumed = 0;


// ############################################################
//                    ASYNC AT ENGINE
// ############################################################

void atInit() {
    asyncAT.state = AT_IDLE;
    asyncAT.complete = false;
    asyncAT.success = false;
    asyncAT.respIndex = 0;
}

// Flush SIM serial RX buffer
void simFlushRx() {
    while (SimSerial.available()) SimSerial.read();
}

// Send AT command and begin async wait
void atSendAsync(const char *cmd, const char *expected,
                 unsigned long timeout) {
    simFlushRx();

    SimSerial.print(cmd);
    SimSerial.print("\r\n");

    strncpy(asyncAT.expected, expected, sizeof(asyncAT.expected) - 1);
    asyncAT.expected[sizeof(asyncAT.expected) - 1] = '\0';
    strncpy(asyncAT.errorStr, "ERROR", sizeof(asyncAT.errorStr) - 1);

    asyncAT.respIndex = 0;
    asyncAT.response[0] = '\0';
    asyncAT.sentTime = millis();
    asyncAT.timeout = timeout;
    asyncAT.state = AT_WAITING;
    asyncAT.complete = false;
    asyncAT.success = false;
}

// Send raw data (no \r\n appended) — used for HTTP body
void atSendRawAsync(const char *data, const char *expected,
                    unsigned long timeout) {
    asyncAT.respIndex = 0;
    asyncAT.response[0] = '\0';

    SimSerial.print(data);

    strncpy(asyncAT.expected, expected, sizeof(asyncAT.expected) - 1);
    asyncAT.expected[sizeof(asyncAT.expected) - 1] = '\0';

    asyncAT.sentTime = millis();
    asyncAT.timeout = timeout;
    asyncAT.state = AT_WAITING;
    asyncAT.complete = false;
    asyncAT.success = false;
}

// Poll the async AT engine — call every loop iteration
// Returns true when the command has completed (success or failure)
bool atPoll() {
    if (asyncAT.state != AT_WAITING) return false;

    // Read all available bytes
    while (SimSerial.available()) {
        char c = SimSerial.read();
        if (asyncAT.respIndex < SIM_RESP_BUFFER_SIZE - 1) {
            asyncAT.response[asyncAT.respIndex++] = c;
            asyncAT.response[asyncAT.respIndex] = '\0';
        }

        // Check for expected response
        if (strstr(asyncAT.response, asyncAT.expected) != NULL) {
            asyncAT.complete = true;
            asyncAT.success = true;
            asyncAT.state = AT_IDLE;
            return true;
        }

        // Check for error
        if (strstr(asyncAT.response, "ERROR") != NULL) {
            asyncAT.complete = true;
            asyncAT.success = false;
            asyncAT.state = AT_IDLE;
            return true;
        }
    }

    // Check timeout
    if (millis() - asyncAT.sentTime > asyncAT.timeout) {
        asyncAT.complete = true;
        asyncAT.success = false;
        asyncAT.state = AT_IDLE;
        return true;
    }

    return false;   // Still waiting
}

bool atIsBusy() {
    return (asyncAT.state == AT_WAITING);
}


// ############################################################
//                    S3 UART RECEPTION
// ############################################################

// Called from main loop (not ISR on ESP32 — HardwareSerial
// uses internal FIFO with interrupt, we just drain it here)
void receiveFromS3() {
    while (Serial1.available()) {
        char c = Serial1.read();

        // Start of frame resets buffer
        if (c == '$') {
            s3RxIndex = 0;
        }

        // Accumulate
        if (s3RxIndex < S3_RX_BUFFER_SIZE - 1) {
            s3RxBuffer[s3RxIndex++] = c;
        }

        // End of frame
        if (c == '\n') {
            s3RxBuffer[s3RxIndex] = '\0';
            s3RxReady = true;
        }
    }
}

// Parse $DATA,<ts>,<h_cm>,<mode>,<bat_pct>\n
bool parseDataFrame(const char *frame, DataReading *out) {
    out->valid = false;

    if (strncmp(frame, "$DATA,", 6) != 0) return false;

    // Tokenize after "$DATA,"
    // We work on a copy because strtok is destructive
    char copy[S3_RX_BUFFER_SIZE];
    strncpy(copy, frame + 6, sizeof(copy) - 1);
    copy[sizeof(copy) - 1] = '\0';

    // Remove trailing \r \n
    char *nl = strchr(copy, '\r');
    if (nl) *nl = '\0';
    nl = strchr(copy, '\n');
    if (nl) *nl = '\0';

    // Field 1: timestamp
    char *tok = strtok(copy, ",");
    if (!tok) return false;
    out->timestamp = strtoul(tok, NULL, 10);

    // Field 2: h_cm
    tok = strtok(NULL, ",");
    if (!tok) return false;
    out->h_cm = atof(tok);

    // Field 3: mode
    tok = strtok(NULL, ",");
    if (!tok) return false;
    out->mode = (uint8_t)atoi(tok);

    // Field 4: bat_pct
    tok = strtok(NULL, ",");
    if (!tok) return false;
    out->bat_pct = (uint8_t)atoi(tok);

    // Validate ranges
    if (out->timestamp == 0) return false;
    if (out->mode > 3) return false;
    if (out->bat_pct > 100) return false;

    out->valid = true;
    return true;
}

void processS3Frame() {
    if (!s3RxReady) return;

    // Copy ISR buffer to working buffer
    memcpy(s3FrameBuffer, (const char *)s3RxBuffer, s3RxIndex + 1);
    s3RxReady = false;
    s3RxIndex = 0;

    // Parse
    DataReading reading;
    if (parseDataFrame(s3FrameBuffer, &reading)) {
        lastReading = reading;
        newReadingAvailable = true;
        lastS3DataTime = millis();

        Serial.print("[S3] ts=");
        Serial.print(reading.timestamp);
        Serial.print(" H=");
        Serial.print(reading.h_cm, 2);
        Serial.print("cm mode=");
        Serial.print(reading.mode);
        Serial.print(" bat=");
        Serial.print(reading.bat_pct);
        Serial.println("%");
    } else {
        Serial.print("[S3] Bad frame: ");
        Serial.println(s3FrameBuffer);
    }
}


// ############################################################
//                    SEND $CFG TO S3
// ############################################################

void sendConfigToS3() {
    char cfgFrame[64];
    snprintf(cfgFrame, sizeof(cfgFrame),
             "$CFG,%u,%u,%u\n",
             config.normal_sec,
             config.high_sec,
             config.h_max_cm);

    Serial1.print(cfgFrame);

    Serial.print("[→S3] ");
    Serial.print(cfgFrame);

    config.dirty = false;
}


// ############################################################
//                    SD CARD
// ############################################################

void sdInit() {
    SPI.begin(SD_SCK_PIN, SD_MISO_PIN, SD_MOSI_PIN, SD_CS_PIN);

    if (!SD.begin(SD_CS_PIN, SPI, 4000000)) {
        Serial.println("[SD] Init failed");
        sdInitialized = false;
        return;
    }

    uint8_t cardType = SD.cardType();
    if (cardType == CARD_NONE) {
        Serial.println("[SD] No card");
        sdInitialized = false;
        return;
    }

    Serial.print("[SD] Card OK, size=");
    Serial.print((uint32_t)(SD.totalBytes() / 1048576));
    Serial.println("MB");

    sdInitialized = true;
}

// Append one reading as CSV line to buffer file
void sdWriteReading(const DataReading *r) {
    if (!sdInitialized) return;

    File f = SD.open(SD_BUFFER_FILENAME, FILE_APPEND);
    if (!f) {
        Serial.println("[SD] Open failed");
        return;
    }

    char line[64];
    snprintf(line, sizeof(line), "%lu,%.2f,%u,%u",
             r->timestamp, r->h_cm, r->mode, r->bat_pct);
    f.println(line);
    f.close();

    sdLinesWritten++;
}

// Check if buffer file exists and has data
bool sdBufferExists() {
    if (!sdInitialized) return false;
    if (!SD.exists(SD_BUFFER_FILENAME)) return false;

    File f = SD.open(SD_BUFFER_FILENAME, FILE_READ);
    if (!f) return false;
    uint32_t sz = f.size();
    f.close();
    return (sz > 0);
}

// Delete the buffer file
void sdDeleteBuffer() {
    if (!sdInitialized) return;
    SD.remove(SD_BUFFER_FILENAME);
    Serial.println("[SD] Buffer deleted");
}

// Read up to `count` lines from the beginning of the buffer file.
// Returns number of lines read. Lines are packed into `outBuf`
// as a JSON array string for batch POST.
// Example output: [{"ts":123,"h":10.5,"m":1,"b":80},{...}]
uint16_t sdReadBatch(uint16_t count, char *outBuf, uint16_t outBufSize,
                     uint32_t *bytesConsumed) {
    if (!sdInitialized) return 0;

    File f = SD.open(SD_BUFFER_FILENAME, FILE_READ);
    if (!f) return 0;

    uint16_t linesRead = 0;
    uint16_t outIdx = 0;
    *bytesConsumed = 0;

    // Start JSON array
    if (outIdx < outBufSize - 1) outBuf[outIdx++] = '[';

    char lineBuf[64];

    while (f.available() && linesRead < count) {
        // Read one line
        uint8_t li = 0;
        while (f.available() && li < sizeof(lineBuf) - 1) {
            char c = f.read();
            (*bytesConsumed)++;
            if (c == '\n') break;
            if (c == '\r') continue;
            lineBuf[li++] = c;
        }
        lineBuf[li] = '\0';

        if (li == 0) continue;     // Empty line

        // Parse CSV: ts,h_cm,mode,bat_pct
        // We need a copy because strtok is destructive
        char parseBuf[64];
        strncpy(parseBuf, lineBuf, sizeof(parseBuf) - 1);
        parseBuf[sizeof(parseBuf) - 1] = '\0';

        uint32_t ts = 0;
        float h = 0;
        uint8_t m = 0, b = 0;

        char *tok = strtok(parseBuf, ",");
        if (tok) ts = strtoul(tok, NULL, 10);
        tok = strtok(NULL, ",");
        if (tok) h = atof(tok);
        tok = strtok(NULL, ",");
        if (tok) m = atoi(tok);
        tok = strtok(NULL, ",");
        if (tok) b = atoi(tok);

        // Build JSON object
        char jsonObj[96];
        int jsonLen = snprintf(jsonObj, sizeof(jsonObj),
            "%s{\"ts\":%lu,\"h\":%.2f,\"m\":%u,\"b\":%u}",
            (linesRead > 0) ? "," : "",
            ts, h, m, b);

        if (outIdx + jsonLen < outBufSize - 2) {
            memcpy(outBuf + outIdx, jsonObj, jsonLen);
            outIdx += jsonLen;
            linesRead++;
        } else {
            break;      // Buffer full, stop here
        }
    }

    f.close();

    // Close JSON array
    if (outIdx < outBufSize - 1) outBuf[outIdx++] = ']';
    outBuf[outIdx] = '\0';

    return linesRead;
}

// Remove the first `bytesToRemove` bytes from the buffer file.
// This is done by rewriting the remainder to a temp file.
void sdTrimFront(uint32_t bytesToRemove) {
    if (!sdInitialized) return;

    const char *tmpName = "/varuna_tmp.csv";

    File src = SD.open(SD_BUFFER_FILENAME, FILE_READ);
    if (!src) return;

    uint32_t totalSize = src.size();

    if (bytesToRemove >= totalSize) {
        // Everything was consumed — just delete
        src.close();
        sdDeleteBuffer();
        return;
    }

    // Seek past the consumed bytes
    src.seek(bytesToRemove);

    // Write remainder to temp file
    File dst = SD.open(tmpName, FILE_WRITE);
    if (!dst) {
        src.close();
        return;
    }

    uint8_t copyBuf[128];
    while (src.available()) {
        int n = src.read(copyBuf, sizeof(copyBuf));
        if (n > 0) dst.write(copyBuf, n);
    }

    src.close();
    dst.close();

    // Swap files
    SD.remove(SD_BUFFER_FILENAME);
    SD.rename(tmpName, SD_BUFFER_FILENAME);
}


// ############################################################
//            SIM800L STATE MACHINE (non-blocking)
// ############################################################

// Gap 7 fix: reset sub-step globals when entering states
void simSetState(SimState ns) {
    simState = ns;
    simStateEntry = millis();

    // Reset sub-step counters for states that use them
    if (ns == SIM_GPRS_ATTACH) {
        attachPhase = 0;
    }
    if (ns == SIM_GPRS_BEARER) {
        bearerStep = 0;
    }
    if (ns == SIM_HTTP_ACTION) {
        actionPhase = 0;
    }
    if (ns == SIM_HTTP_READ_RESPONSE) {
        httpReadPhase = 0;
    }
}

void simHardReset() {
    Serial.println("[SIM] Hard reset");
    digitalWrite(SIM_RST_PIN, LOW);
    delay(200);                         // Brief pulse — acceptable
    digitalWrite(SIM_RST_PIN, HIGH);
}

void simStateMachine() {
    unsigned long elapsed = millis() - simStateEntry;

    switch (simState) {

    // ---- Power on / reset ----
    case SIM_OFF:
        simHardReset();
        simSetState(SIM_RESETTING);
        break;

    case SIM_RESETTING:
        // Wait 3s for SIM800L to boot
        if (elapsed > 3000) {
            simSetState(SIM_WAIT_READY);
        }
        break;

    case SIM_WAIT_READY:
        if (!atIsBusy()) {
            if (asyncAT.complete && asyncAT.success) {
                // AT responded OK — disable echo
                atSendAsync("ATE0", "OK", AT_SHORT_TIMEOUT);
                simSetState(SIM_CHECK_PIN);
                return;
            }
            // Send AT probe
            atSendAsync("AT", "OK", AT_SHORT_TIMEOUT);
        }
        if (elapsed > 10000) {
            simRetryCount++;
            if (simRetryCount > SIM_INIT_MAX_RETRIES) {
                Serial.println("[SIM] Max retries, error");
                simSetState(SIM_ERROR);
            } else {
                simSetState(SIM_OFF);
            }
        }
        break;

    case SIM_CHECK_PIN:
        if (!atIsBusy()) {
            if (asyncAT.complete) {
                if (asyncAT.success) {
                    // ATE0 done or CPIN check done
                    if (strstr(asyncAT.response, "READY") != NULL) {
                        Serial.println("[SIM] SIM card ready");
                        atSendAsync("AT+CMEE=2", "OK", AT_SHORT_TIMEOUT);
                        simSetState(SIM_REGISTERING);
                        return;
                    }
                    // Send CPIN check
                    atSendAsync("AT+CPIN?", "READY", AT_MEDIUM_TIMEOUT);
                } else {
                    Serial.println("[SIM] SIM card not ready");
                    simSetState(SIM_ERROR);
                }
            }
        }
        if (elapsed > 15000) {
            simSetState(SIM_ERROR);
        }
        break;

    case SIM_REGISTERING:
        if (!atIsBusy()) {
            if (asyncAT.complete) {
                // Check registration response
                if (strstr(asyncAT.response, ",1") != NULL ||
                    strstr(asyncAT.response, ",5") != NULL) {
                    Serial.println("[SIM] Network registered");
                    simSetState(SIM_GPRS_ATTACH);
                    return;
                }
            }
            // Poll registration
            atSendAsync("AT+CREG?", "OK", AT_MEDIUM_TIMEOUT);
        }
        if (elapsed > 60000) {
            Serial.println("[SIM] Registration timeout");
            simSetState(SIM_ERROR);
        }
        break;

    // ---- Gap 6 fix: SIM_GPRS_ATTACH rewritten with phased approach ----
    case SIM_GPRS_ATTACH:
        if (!atIsBusy()) {
            if (attachPhase == 0) {
                // Phase 0: Send attach command
                atSendAsync("AT+CGATT=1", "OK", AT_LONG_TIMEOUT);
                attachPhase = 1;
            } else if (attachPhase == 1 && asyncAT.complete) {
                // Phase 1: Attach command returned, now query status
                atSendAsync("AT+CGATT?", "OK", AT_MEDIUM_TIMEOUT);
                attachPhase = 2;
            } else if (attachPhase == 2 && asyncAT.complete) {
                // Phase 2: Check query result
                if (strstr(asyncAT.response, "+CGATT: 1") != NULL) {
                    Serial.println("[SIM] GPRS attached");
                    simSetState(SIM_GPRS_BEARER);
                } else {
                    Serial.println("[SIM] GPRS attach failed");
                    simSetState(SIM_ERROR);
                }
            }
        }
        if (elapsed > 40000) {
            simSetState(SIM_ERROR);
        }
        break;

    // ---- Gap 7 fix: bearerStep is now a global, reset by simSetState() ----
    case SIM_GPRS_BEARER:
        if (!atIsBusy()) {
            if (asyncAT.complete && !asyncAT.success && bearerStep > 0) {
                // A bearer command failed
                Serial.println("[SIM] Bearer setup failed");
                simSetState(SIM_ERROR);
                return;
            }

            switch (bearerStep) {
            case 0:
                atSendAsync("AT+SAPBR=3,1,\"Contype\",\"GPRS\"",
                           "OK", AT_MEDIUM_TIMEOUT);
                bearerStep = 1;
                break;
            case 1:
                if (asyncAT.success) {
                    char cmd[80];
                    snprintf(cmd, sizeof(cmd),
                        "AT+SAPBR=3,1,\"APN\",\"%s\"", SIM_APN);
                    atSendAsync(cmd, "OK", AT_MEDIUM_TIMEOUT);
                    bearerStep = 2;
                }
                break;
            case 2:
                if (asyncAT.success) {
                    atSendAsync("AT+SAPBR=1,1", "OK", AT_LONG_TIMEOUT);
                    bearerStep = 3;
                }
                break;
            case 3:
                if (asyncAT.success) {
                    atSendAsync("AT+SAPBR=2,1", "+SAPBR: 1,1",
                               AT_MEDIUM_TIMEOUT);
                    bearerStep = 4;
                }
                break;
            case 4:
                if (asyncAT.success) {
                    Serial.println("[SIM] Bearer open — GPRS ready");
                    gprsConnected = true;
                    simReady = true;
                    simRetryCount = 0;
                    simSetState(SIM_IDLE);
                } else {
                    simSetState(SIM_ERROR);
                }
                break;
            }
        }

        if (elapsed > 45000) {
            simSetState(SIM_ERROR);
        }
        break;

    // ---- Idle — ready for HTTP ----
    case SIM_IDLE:
        // Nothing — waiting for httpStartPost() to kick us
        break;

    // ---- HTTP POST sequence ----
    case SIM_HTTP_INIT:
        if (!atIsBusy()) {
            if (asyncAT.complete) {
                if (asyncAT.success) {
                    atSendAsync("AT+HTTPPARA=\"CID\",1", "OK",
                               AT_SHORT_TIMEOUT);
                    simSetState(SIM_HTTP_SET_URL);
                } else {
                    // HTTPINIT failed — maybe prior session still open
                    // Terminate and retry once
                    atSendAsync("AT+HTTPTERM", "OK", AT_SHORT_TIMEOUT);
                    // On next entry we'll try HTTPINIT again
                    // Use actionPhase temporarily to track retry
                    if (actionPhase == 0) {
                        actionPhase = 1;
                        // Stay in SIM_HTTP_INIT — will re-enter
                    } else {
                        actionPhase = 0;
                        httpFinish(false, 0);
                        simSetState(SIM_IDLE);
                    }
                }
                return;
            }
            atSendAsync("AT+HTTPINIT", "OK", AT_MEDIUM_TIMEOUT);
        }
        break;

    case SIM_HTTP_SET_URL:
        if (!atIsBusy()) {
            if (asyncAT.complete && asyncAT.success) {
                char cmd[192];
                snprintf(cmd, sizeof(cmd),
                    "AT+HTTPPARA=\"URL\",\"%s\"", httpUrl);
                atSendAsync(cmd, "OK", AT_MEDIUM_TIMEOUT);
                simSetState(SIM_HTTP_SET_CONTENT);
            } else if (asyncAT.complete) {
                simSetState(SIM_HTTP_TERM);
            }
        }
        break;

    case SIM_HTTP_SET_CONTENT:
        if (!atIsBusy()) {
            if (asyncAT.complete && asyncAT.success) {
                atSendAsync(
                    "AT+HTTPPARA=\"CONTENT\",\"application/json\"",
                    "OK", AT_SHORT_TIMEOUT);
                simSetState(SIM_HTTP_SET_DATA_SIZE);
            } else if (asyncAT.complete) {
                simSetState(SIM_HTTP_TERM);
            }
        }
        break;

    case SIM_HTTP_SET_DATA_SIZE:
        if (!atIsBusy()) {
            if (asyncAT.complete && asyncAT.success) {
                char cmd[40];
                snprintf(cmd, sizeof(cmd), "AT+HTTPDATA=%u,10000",
                         (unsigned int)strlen(httpBody));
                atSendAsync(cmd, "DOWNLOAD", AT_MEDIUM_TIMEOUT);
                simSetState(SIM_HTTP_SEND_BODY);
            } else if (asyncAT.complete) {
                simSetState(SIM_HTTP_TERM);
            }
        }
        break;

    case SIM_HTTP_SEND_BODY:
        if (!atIsBusy()) {
            if (asyncAT.complete && asyncAT.success) {
                // SIM800L said DOWNLOAD — now send the body
                atSendRawAsync(httpBody, "OK", 10000);
                simSetState(SIM_HTTP_ACTION);
            } else if (asyncAT.complete) {
                simSetState(SIM_HTTP_TERM);
            }
        }
        break;

    // ---- Gap 1 fix: SIM_HTTP_ACTION completely rewritten ----
    case SIM_HTTP_ACTION:
        if (actionPhase == 0) {
            // Body was accepted (OK received from SEND_BODY)
            // Now fire the actual POST
            atSendAsync("AT+HTTPACTION=1", "+HTTPACTION:", AT_HTTP_TIMEOUT);
            actionPhase = 1;
        } else if (actionPhase == 1) {
            // Waiting for +HTTPACTION: response
            if (atPoll()) {
                if (asyncAT.success) {
                    // Parse HTTP response code from "+HTTPACTION: 1,200,len"
                    int code = 0;
                    char *p = strstr(asyncAT.response, "+HTTPACTION:");
                    if (p) {
                        p = strchr(p, ',');         // skip past method
                        if (p) code = atoi(p + 1);  // status code
                    }
                    httpResponseCode = code;

                    // If success, try to read response body (Gap 5 fix)
                    if (code == 200 || code == 201) {
                        simSetState(SIM_HTTP_READ_RESPONSE);
                    } else {
                        simSetState(SIM_HTTP_TERM);
                    }
                } else {
                    httpResponseCode = 0;
                    simSetState(SIM_HTTP_TERM);
                }
            }
        }
        break;

    // ---- Gap 5 fix: New state to read server response body ----
    case SIM_HTTP_READ_RESPONSE:
        if (httpReadPhase == 0) {
            // Request to read the response body
            atSendAsync("AT+HTTPREAD", "+HTTPREAD:", AT_MEDIUM_TIMEOUT);
            httpReadPhase = 1;
        } else if (httpReadPhase == 1) {
            if (atPoll()) {
                if (asyncAT.success) {
                    // Response is in asyncAT.response
                    // Format: +HTTPREAD: <len>\r\n<body>\r\nOK
                    // Extract body after the first \n following +HTTPREAD:
                    char *bodyStart = strstr(asyncAT.response, "+HTTPREAD:");
                    if (bodyStart) {
                        bodyStart = strchr(bodyStart, '\n');
                        if (bodyStart) {
                            bodyStart++;    // skip \n
                            // Copy body to httpResponseBody, stopping at \r\nOK
                            uint16_t bi = 0;
                            while (*bodyStart != '\0' &&
                                   bi < HTTP_BODY_BUFFER_SIZE - 1) {
                                // Stop if we hit the trailing OK
                                if (bodyStart[0] == '\r' && bodyStart[1] == '\n' &&
                                    bodyStart[2] == 'O' && bodyStart[3] == 'K') {
                                    break;
                                }
                                httpResponseBody[bi++] = *bodyStart++;
                            }
                            httpResponseBody[bi] = '\0';

                            // Parse for config updates
                            checkServerConfigResponse(httpResponseBody);
                        }
                    }
                }
                // Whether read succeeded or failed, proceed to TERM
                simSetState(SIM_HTTP_TERM);
            }
        }
        break;

    case SIM_HTTP_TERM:
        if (!atIsBusy()) {
            if (asyncAT.complete) {
                bool ok = (httpResponseCode == 200 ||
                          httpResponseCode == 201);
                httpFinish(ok, httpResponseCode);
                simSetState(SIM_IDLE);
            } else {
                atSendAsync("AT+HTTPTERM", "OK", AT_SHORT_TIMEOUT);
            }
        }
        break;

    case SIM_ERROR:
        Serial.println("[SIM] ERROR — will reset in 5s");
        gprsConnected = false;
        simReady = false;
        if (elapsed > 5000) {
            simRetryCount = 0;
            simSetState(SIM_OFF);
        }
        break;
    }
}


// ############################################################
//             HTTP POST INTERFACE (high-level)
// ############################################################

// Called when an HTTP transaction finishes (success or failure)
void httpFinish(bool success, int code) {
    httpBusy = false;

    if (success) {
        Serial.print("[HTTP] OK code=");
        Serial.println(code);
        successfulTransmissions++;
    } else {
        Serial.print("[HTTP] FAIL code=");
        Serial.println(code);
        failedTransmissions++;
    }
    totalTransmissions++;

    if (httpDoneCallback) {
        httpDoneCallback(success, code);
        httpDoneCallback = NULL;
    }
}

// Start an HTTP POST (non-blocking — kicks off state machine)
bool httpStartPost(const char *url, const char *body,
                   HttpCallback callback) {
    if (httpBusy) {
        Serial.println("[HTTP] Busy, rejecting");
        return false;
    }
    if (!simReady || !gprsConnected) {
        Serial.println("[HTTP] Not connected");
        return false;
    }
    if (simState != SIM_IDLE) {
        Serial.println("[HTTP] SIM not idle");
        return false;
    }

    strncpy(httpUrl, url, sizeof(httpUrl) - 1);
    httpUrl[sizeof(httpUrl) - 1] = '\0';
    strncpy(httpBody, body, sizeof(httpBody) - 1);
    httpBody[sizeof(httpBody) - 1] = '\0';

    httpResponseCode = 0;
    httpResponseBody[0] = '\0';
    httpDoneCallback = callback;
    httpBusy = true;

    simSetState(SIM_HTTP_INIT);
    return true;
}


// ############################################################
//          BUILD JSON BODY FOR SINGLE READING
// ############################################################

void buildSinglePostBody(const DataReading *r, char *buf,
                         uint16_t bufSize) {
    snprintf(buf, bufSize,
        "{\"device_id\":\"%s\","
        "\"ts\":%lu,"
        "\"h_cm\":%.2f,"
        "\"mode\":%u,"
        "\"bat_pct\":%u}",
        DEVICE_ID,
        r->timestamp,
        r->h_cm,
        r->mode,
        r->bat_pct);
}


// ############################################################
//             GPRS HEALTH CHECK (non-blocking)
// ############################################################

// Simple bearer check — only runs when SIM is idle
void simHealthCheck() {
    if (simState != SIM_IDLE) return;
    if (atIsBusy()) return;

    // Query bearer status
    atSendAsync("AT+SAPBR=2,1", "+SAPBR:", AT_MEDIUM_TIMEOUT);

    // We'll check the result next time this function is called
    // For now, just note: if +SAPBR: 1,3 (closed) shows up,
    // the SIM state machine error handling will catch it when
    // an HTTP POST fails.
    //
    // A more robust implementation would use a dedicated health
    // check state, but for now the 3-consecutive-fail logic in
    // the operational state machine handles GPRS loss detection.
}


// ############################################################
//             OPERATIONAL STATE MACHINE
// ############################################################

// ---- Callbacks for HTTP results ----

void onNormalPostDone(bool success, int code) {
    if (success) {
        consecutiveFails = 0;
        Serial.println("[OP] POST delivered");
    } else {
        consecutiveFails++;
        Serial.print("[OP] POST failed, consecutive=");
        Serial.println(consecutiveFails);

        if (consecutiveFails >= CONSECUTIVE_FAIL_LIMIT) {
            Serial.println("[OP] → OFFLINE");
            opState = OP_OFFLINE;
            consecutiveFails = 0;
        }
    }
}

void onRecoveryTestDone(bool success, int code) {
    if (success) {
        Serial.println("[OP] Recovery test OK → RECOVERY");
        opState = OP_RECOVERY;
    } else {
        Serial.println("[OP] Recovery test failed, staying OFFLINE");
    }
}

// Gap 3 fix: removed unnecessary extern, recoveryBytesConsumed is already global
void onRecoveryFlushDone(bool success, int code) {
    if (success) {
        // Trim the successfully-sent lines from the file
        sdTrimFront(recoveryBytesConsumed);

        // Check if more data remains
        if (sdBufferExists()) {
            Serial.println("[OP] Flush batch done, more to send");
            recoveryFlushActive = false;    // Allow next batch
        } else {
            Serial.println("[OP] Flush complete → NORMAL");
            opState = OP_NORMAL;
            recoveryFlushActive = false;

            // Check queued realtime command
            if (realtimeQueued) {
                realtimeQueued = false;
                Serial.println("[OP] Applying queued REALTIME");
                opState = OP_REALTIME;
            }
        }
    } else {
        Serial.println("[OP] Flush batch failed, will retry");
        recoveryFlushActive = false;
    }
}

void onRealtimePostDone(bool success, int code) {
    if (success) {
        consecutiveFails = 0;
    } else {
        consecutiveFails++;
        if (consecutiveFails >= CONSECUTIVE_FAIL_LIMIT) {
            Serial.println("[OP] REALTIME → OFFLINE (GPRS died)");
            opState = OP_OFFLINE;
            config.realtime = false;        // Clear RT flag
            consecutiveFails = 0;
        }
    }
}

void opStateMachine() {
    switch (opState) {

    // ============================================
    // NORMAL: POST every reading, no SD
    // ============================================
    case OP_NORMAL:
        if (newReadingAvailable && !httpBusy) {
            newReadingAvailable = false;

            char body[HTTP_BODY_BUFFER_SIZE];
            buildSinglePostBody(&lastReading, body, sizeof(body));
            httpStartPost(SERVER_URL, body, onNormalPostDone);
        }
        break;

    // ============================================
    // OFFLINE: Write to SD, test GPRS every 60s
    // ============================================
    case OP_OFFLINE:
        // Write any new readings to SD
        if (newReadingAvailable) {
            newReadingAvailable = false;
            sdWriteReading(&lastReading);
            Serial.println("[OP] Wrote to SD buffer");
        }

        // Periodic recovery test
        if (!httpBusy &&
            millis() - lastRecoveryTestMs >= GPRS_RECOVERY_TEST_MS) {
            lastRecoveryTestMs = millis();

            // Minimal test payload
            char testBody[64];
            snprintf(testBody, sizeof(testBody),
                     "{\"device_id\":\"%s\",\"ping\":true}",
                     DEVICE_ID);
            httpStartPost(SERVER_TEST_URL, testBody,
                         onRecoveryTestDone);
        }
        break;

    // ============================================
    // RECOVERY: Flush SD buffer in batches
    // ============================================
    case OP_RECOVERY:
        // Any new readings ALSO go to SD (appended at bottom)
        if (newReadingAvailable) {
            newReadingAvailable = false;
            sdWriteReading(&lastReading);
        }

        // Flush one batch if we're not already mid-POST
        if (!httpBusy && !recoveryFlushActive) {
            char batchBody[HTTP_BODY_BUFFER_SIZE];
            uint16_t linesRead = sdReadBatch(
                RECOVERY_FLUSH_BATCH_SIZE,
                batchBody, sizeof(batchBody),
                &recoveryBytesConsumed);

            if (linesRead == 0) {
                // Nothing left — buffer might have been emptied
                // between check and read, or file deleted
                Serial.println("[OP] Buffer empty → NORMAL");
                sdDeleteBuffer();
                opState = OP_NORMAL;

                if (realtimeQueued) {
                    realtimeQueued = false;
                    opState = OP_REALTIME;
                }
                return;
            }

            // Wrap in device envelope
            char envelope[HTTP_BODY_BUFFER_SIZE];
            snprintf(envelope, sizeof(envelope),
                "{\"device_id\":\"%s\",\"batch\":%s}",
                DEVICE_ID, batchBody);

            recoveryFlushActive = true;
            httpStartPost(SERVER_BATCH_URL, envelope,
                         onRecoveryFlushDone);

            Serial.print("[OP] Flushing batch of ");
            Serial.print(linesRead);
            Serial.println(" lines");
        }
        break;

    // ============================================
    // REALTIME: POST immediately, no SD
    // ============================================
    case OP_REALTIME:
        if (newReadingAvailable && !httpBusy) {
            newReadingAvailable = false;

            char body[HTTP_BODY_BUFFER_SIZE];
            buildSinglePostBody(&lastReading, body, sizeof(body));
            httpStartPost(SERVER_URL, body, onRealtimePostDone);
        }
        break;
    }
}


// ############################################################
//        SERVER CONFIG RESPONSE PARSER (Gap 5 fix)
// ############################################################
//
// NOTE ON ARCHITECTURE:
// The spec says Firebase pushes config to C3. On a WiFi-equipped
// ESP32, you'd use Firebase's RTDB streaming. But the XIAO C3 in
// this system may not have WiFi access in the field (deployed in
// a river). Two implementation paths:
//
// PATH A — WiFi available (lab/urban deployment):
//   Use Firebase_ESP_Client library with stream callback.
//   Can be added later when WiFi is available.
//
// PATH B — No WiFi, SIM800L only (field deployment):
//   The server periodically checks Firebase for config changes
//   and returns pending config in the HTTP POST response body.
//   The C3 parses the response after each successful POST.
//   This is the more robust field architecture.
//   NOW IMPLEMENTED via SIM_HTTP_READ_RESPONSE state.
//
// Both paths converge to the same action: update `config` struct
// and set config.dirty = true.

// Path B: Parse server response for config commands
// Called after every successful HTTP POST response
// (called from SIM_HTTP_READ_RESPONSE state)
void checkServerConfigResponse(const char *responseBody) {
    // Expected format (returned by server in HTTP response):
    // {"cfg":{"normal_sec":300,"high_sec":60,"h_max_cm":200}}
    // or empty/no cfg key if no changes pending

    if (responseBody == NULL || strlen(responseBody) == 0) return;

    // Simple JSON parsing — look for "normal_sec" key as indicator
    const char *cfgPtr = strstr(responseBody, "\"normal_sec\"");
    if (cfgPtr == NULL) return;

    // Extract values with minimal parsing
    const char *p;
    bool configChanged = false;

    p = strstr(responseBody, "\"normal_sec\":");
    if (p) {
        uint16_t val = (uint16_t)atoi(p + 13);
        if (val > 0 && val != config.normal_sec) {
            config.normal_sec = val;
            configChanged = true;
        }
    }

    p = strstr(responseBody, "\"high_sec\":");
    if (p) {
        uint16_t val = (uint16_t)atoi(p + 11);
        if (val > 0 && val != config.high_sec) {
            config.high_sec = val;
            configChanged = true;
        }
    }

    p = strstr(responseBody, "\"h_max_cm\":");
    if (p) {
        uint16_t val = (uint16_t)atoi(p + 11);
        if (val > 0 && val != config.h_max_cm) {
            config.h_max_cm = val;
            configChanged = true;
        }
    }

    p = strstr(responseBody, "\"realtime\":");
    if (p) {
        bool newRT = (strstr(p + 11, "true") != NULL);
        if (newRT != config.realtime) {
            handleRealtimeCommand(newRT);
        }
    }

    if (configChanged) {
        config.dirty = true;
        Serial.print("[CFG] Updated: normal=");
        Serial.print(config.normal_sec);
        Serial.print("s high=");
        Serial.print(config.high_sec);
        Serial.print("s hmax=");
        Serial.print(config.h_max_cm);
        Serial.println("cm");
    }
}


// ############################################################
//             REALTIME COMMAND HANDLER
// ############################################################

void handleRealtimeCommand(bool enable) {
    if (enable) {
        if (opState == OP_OFFLINE) {
            // Cannot enter RT while offline — queue it
            realtimeQueued = true;
            Serial.println("[RT] Queued (currently OFFLINE)");
            return;
        }
        if (opState == OP_RECOVERY) {
            // Cannot enter RT while flushing — queue it
            realtimeQueued = true;
            Serial.println("[RT] Queued (currently RECOVERY)");
            return;
        }
        if (opState == OP_NORMAL) {
            opState = OP_REALTIME;
            Serial.println("[RT] → REALTIME");
        }
        // Already in REALTIME? No-op.
    } else {
        // Disable realtime
        realtimeQueued = false;
        if (opState == OP_REALTIME) {
            // Check for pending buffer
            if (sdBufferExists()) {
                Serial.println("[RT] → RECOVERY (buffer exists)");
                opState = OP_RECOVERY;
            } else {
                Serial.println("[RT] → NORMAL");
                opState = OP_NORMAL;
            }
        }
    }
    config.realtime = enable;
}


// ############################################################
//             S3 WATCHDOG
// ############################################################

void checkS3Watchdog() {
    if (lastS3DataTime == 0) return;

    if (millis() - lastS3DataTime > S3_WATCHDOG_TIMEOUT_MS) {
        Serial.print("[WATCHDOG] No S3 data for ");
        Serial.print((millis() - lastS3DataTime) / 1000);
        Serial.println("s");
        // In production: could trigger an alert to server
    }
}


// ############################################################
//             SERIAL DEBUG INTERFACE
// ############################################################

void processSerialCommand(const char *cmd) {
    if (strcmp(cmd, "STATUS") == 0) {
        Serial.println("======= STATUS =======");

        Serial.print("OpState: ");
        switch (opState) {
            case OP_NORMAL:   Serial.println("NORMAL");   break;
            case OP_OFFLINE:  Serial.println("OFFLINE");  break;
            case OP_RECOVERY: Serial.println("RECOVERY"); break;
            case OP_REALTIME: Serial.println("REALTIME"); break;
        }

        Serial.print("SIM State: ");
        Serial.println(simState);
        Serial.print("GPRS: ");
        Serial.println(gprsConnected ? "UP" : "DOWN");
        Serial.print("SD: ");
        Serial.println(sdInitialized ? "OK" : "FAIL");
        Serial.print("Consec Fails: ");
        Serial.println(consecutiveFails);
        Serial.print("TX ok/fail/total: ");
        Serial.print(successfulTransmissions);
        Serial.print("/");
        Serial.print(failedTransmissions);
        Serial.print("/");
        Serial.println(totalTransmissions);
        Serial.print("SD lines: ");
        Serial.println(sdLinesWritten);
        Serial.print("Buffer exists: ");
        Serial.println(sdBufferExists() ? "YES" : "NO");
        Serial.print("RT queued: ");
        Serial.println(realtimeQueued ? "YES" : "NO");

        if (lastS3DataTime > 0) {
            Serial.print("Last S3: ");
            Serial.print((millis() - lastS3DataTime) / 1000);
            Serial.println("s ago");
            Serial.print("  H=");
            Serial.print(lastReading.h_cm, 2);
            Serial.print("cm mode=");
            Serial.print(lastReading.mode);
            Serial.print(" bat=");
            Serial.print(lastReading.bat_pct);
            Serial.println("%");
        } else {
            Serial.println("Last S3: never");
        }

        Serial.print("Config: normal=");
        Serial.print(config.normal_sec);
        Serial.print("s high=");
        Serial.print(config.high_sec);
        Serial.print("s hmax=");
        Serial.print(config.h_max_cm);
        Serial.print("cm rt=");
        Serial.println(config.realtime ? "ON" : "OFF");

        Serial.println("======================");

    } else if (strcmp(cmd, "LASTFRAME") == 0) {
        Serial.println(s3FrameBuffer);

    } else if (strcmp(cmd, "REINITSD") == 0) {
        sdInit();

    } else if (strcmp(cmd, "RESETSIM") == 0) {
        simSetState(SIM_OFF);

    } else if (strcmp(cmd, "FORCETX") == 0) {
        if (lastReading.valid) {
            char body[HTTP_BODY_BUFFER_SIZE];
            buildSinglePostBody(&lastReading, body, sizeof(body));
            httpStartPost(SERVER_URL, body, onNormalPostDone);
        } else {
            Serial.println("No valid data");
        }

    } else if (strncmp(cmd, "AT", 2) == 0) {
        // Pass through AT command to SIM800L
        if (!atIsBusy()) {
            atSendAsync(cmd, "OK", AT_MEDIUM_TIMEOUT);
            Serial.println("[AT] Sent, waiting...");
        } else {
            Serial.println("[AT] Busy");
        }

    } else if (strncmp(cmd, "CFG:", 4) == 0) {
        // Manual config injection: CFG:normal,high,hmax
        // Example: CFG:300,60,200
        char copy[64];
        strncpy(copy, cmd + 4, sizeof(copy) - 1);
        copy[sizeof(copy) - 1] = '\0';

        char *tok = strtok(copy, ",");
        if (tok) config.normal_sec = atoi(tok);
        tok = strtok(NULL, ",");
        if (tok) config.high_sec = atoi(tok);
        tok = strtok(NULL, ",");
        if (tok) config.h_max_cm = atoi(tok);

        config.dirty = true;
        Serial.println("[CFG] Manual config set");

    } else if (strcmp(cmd, "SETRT1") == 0) {
        handleRealtimeCommand(true);

    } else if (strcmp(cmd, "SETRT0") == 0) {
        handleRealtimeCommand(false);

    } else if (strcmp(cmd, "DELBUF") == 0) {
        sdDeleteBuffer();

    } else if (strcmp(cmd, "READBUF") == 0) {
        // Debug: read and print first 5 lines of buffer
        if (sdBufferExists()) {
            File f = SD.open(SD_BUFFER_FILENAME, FILE_READ);
            if (f) {
                Serial.println("--- SD Buffer (first 5 lines) ---");
                uint8_t count = 0;
                while (f.available() && count < 5) {
                    String line = f.readStringUntil('\n');
                    Serial.println(line);
                    count++;
                }
                Serial.print("Total file size: ");
                Serial.print(f.size());
                Serial.println(" bytes");
                f.close();
                Serial.println("--- End ---");
            }
        } else {
            Serial.println("No buffer file");
        }

    } else if (strcmp(cmd, "SIMSTATE") == 0) {
        Serial.print("SIM state: ");
        Serial.println(simState);
        Serial.print("AT busy: ");
        Serial.println(atIsBusy() ? "YES" : "NO");
        Serial.print("AT complete: ");
        Serial.println(asyncAT.complete ? "YES" : "NO");
        Serial.print("AT success: ");
        Serial.println(asyncAT.success ? "YES" : "NO");
        Serial.print("AT response: ");
        Serial.println(asyncAT.response);
        Serial.print("HTTP busy: ");
        Serial.println(httpBusy ? "YES" : "NO");
        Serial.print("bearerStep: ");
        Serial.println(bearerStep);
        Serial.print("attachPhase: ");
        Serial.println(attachPhase);
        Serial.print("actionPhase: ");
        Serial.println(actionPhase);

    } else if (strcmp(cmd, "HELP") == 0) {
        Serial.println("Commands:");
        Serial.println("  STATUS    - System status");
        Serial.println("  LASTFRAME - Last S3 frame");
        Serial.println("  REINITSD  - Reinit SD card");
        Serial.println("  RESETSIM  - Reset SIM800L");
        Serial.println("  FORCETX   - Force transmit now");
        Serial.println("  AT...     - AT passthrough");
        Serial.println("  CFG:n,h,m - Set config (normal,high,hmax)");
        Serial.println("  SETRT1    - Enable realtime");
        Serial.println("  SETRT0    - Disable realtime");
        Serial.println("  DELBUF    - Delete SD buffer");
        Serial.println("  READBUF   - Show SD buffer contents");
        Serial.println("  SIMSTATE  - SIM800L debug info");
    }
}

char serialCmdBuf[128];
uint8_t serialCmdIdx = 0;

void readSerialCommands() {
    while (Serial.available()) {
        char c = Serial.read();

        if (c == '\n' || c == '\r') {
            if (serialCmdIdx > 0) {
                serialCmdBuf[serialCmdIdx] = '\0';
                processSerialCommand(serialCmdBuf);
                serialCmdIdx = 0;
            }
        } else {
            if (serialCmdIdx < sizeof(serialCmdBuf) - 1) {
                serialCmdBuf[serialCmdIdx++] = c;
            }
        }
    }
}


// ############################################################
//                        SETUP
// ############################################################

void setup() {
    // USB serial for debug
    Serial.begin(115200);
    delay(1000);
    Serial.println("========================================");
    Serial.println("  VARUNA FLOOD SYSTEM — C3 COMMS CTRL  ");
    Serial.println("========================================");

    // SIM800L reset pin
    pinMode(SIM_RST_PIN, OUTPUT);
    digitalWrite(SIM_RST_PIN, HIGH);

    // UART to S3
    Serial.println("[INIT] UART → S3");
    Serial1.begin(115200, SERIAL_8N1, S3_UART_RX_PIN, S3_UART_TX_PIN);

    // UART to SIM800L
    Serial.println("[INIT] UART → SIM800L");
    SimSerial.begin(SIM_BAUD, SERIAL_8N1, SIM_RX_PIN, SIM_TX_PIN);

    // Async AT engine
    atInit();

    // SD card
    Serial.println("[INIT] SD card");
    sdInit();

    // Check for leftover buffer from prior session
    if (sdBufferExists()) {
        Serial.println("[INIT] SD buffer found — will flush after GPRS up");
    }

    // Start SIM800L init
    Serial.println("[INIT] SIM800L starting...");
    simSetState(SIM_OFF);

    // Init timers
    lastRecoveryTestMs = millis();
    lastSimHealthMs = millis();
    lastS3DataTime = 0;

    // Init state
    opState = OP_NORMAL;
    lastReading.valid = false;
    newReadingAvailable = false;
    consecutiveFails = 0;

    Serial.println("[INIT] Ready");
    Serial.println("========================================");
}


// ############################################################
//                      MAIN LOOP
// ############################################################

void loop() {
    // ---- 1. Receive data from S3 (fast, non-blocking) ----
    receiveFromS3();
    processS3Frame();

    // ---- 2. SIM800L state machine (non-blocking) ----
    atPoll();               // Check for AT response bytes
    simStateMachine();      // Advance SIM state

    // ---- 3. Operational state machine ----
    // Only run ops logic when SIM is ready
    if (simReady && gprsConnected) {
        opStateMachine();

        // On first GPRS connection, check for old buffer
        static bool bootBufferChecked = false;
        if (!bootBufferChecked) {
            bootBufferChecked = true;
            if (sdBufferExists()) {
                Serial.println("[BOOT] Old buffer found → RECOVERY");
                opState = OP_RECOVERY;
            }
        }
    } else {
        // GPRS not ready — buffer to SD if we have data
        if (newReadingAvailable) {
            newReadingAvailable = false;
            sdWriteReading(&lastReading);
            Serial.println("[PRE-GPRS] Buffered to SD");
        }
    }

    // ---- 4. Send config to S3 when dirty ----
    if (config.dirty) {
        sendConfigToS3();
    }

    // ---- 5. Periodic health check ----
    if (millis() - lastSimHealthMs >= SIM_HEALTH_CHECK_MS) {
        lastSimHealthMs = millis();
        simHealthCheck();
    }

    // ---- 6. S3 watchdog ----
    checkS3Watchdog();

    // ---- 7. Serial debug commands ----
    readSerialCommands();
}
