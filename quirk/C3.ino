// ============================================================================
// VARUNA BUOY — ESP32-C3 COMMUNICATION & OTA BRIDGE FIRMWARE
// ============================================================================
//
// This firmware runs on the XIAO ESP32-C3. It handles:
//
// 1. RECEIVING 39-field CSV data from S3 (via software serial on GPIO 10)
// 2. PUSHING CSV data to Firebase Realtime Database over WiFi
// 3. BUFFERING data to SD card when WiFi is unavailable
// 4. FLUSHING buffered SD data when WiFi reconnects
// 5. RECEIVING configuration commands from Firebase (website)
// 6. FORWARDING configuration commands to S3 via hardware serial
// 7. RECEIVING OTA firmware binary from Firebase Storage
// 8. PROGRAMMING the S3 via its UART bootloader (BOOT/EN pins)
// 9. RESPONDING to S3 diagnostic pings ($PING → $PONG)
// 10. REAL-TIME MODE: bypassing SD buffer when server requests it
//
// The C3 NEVER processes sensor data. It is a dumb pipe + OTA programmer.
// All intelligence is on the S3.
//
// ============================================================================

#include <WiFi.h>
#include <HTTPClient.h>
#include <SPI.h>
#include <SD.h>

// We implement a minimal software serial receiver for GPIO 10
// (S3's GPIO 14 software UART TX at 9600 baud)

// ============================================================================
// WiFi CREDENTIALS
// ============================================================================
#define WIFI_SSID          "YOUR_WIFI_SSID"
#define WIFI_PASSWORD      "YOUR_WIFI_PASSWORD"

// ============================================================================
// FIREBASE CONFIGURATION
// ============================================================================
// Firebase Realtime Database (RTDB) REST API
// Using REST API with database secret (legacy) or ID token
// This avoids the need for ArduinoJson or Firebase libraries
#define FIREBASE_HOST      "your-project-id.firebaseio.com"
#define FIREBASE_AUTH      "YOUR_FIREBASE_DATABASE_SECRET"
// Alternative: use Firebase Auth ID token flow

// Firebase Storage for OTA binary
#define FIREBASE_STORAGE_BUCKET  "your-project-id.appspot.com"

// Device ID — unique per buoy deployment
#define DEVICE_ID          "buoy_001"

// ============================================================================
// PIN DEFINITIONS
// ============================================================================
// Hardware Serial to S3 (command channel)
#define S3_CMD_RX_PIN      20   // C3 RX ← S3 TX (GPIO 43)
#define S3_CMD_TX_PIN      21   // C3 TX → S3 RX (GPIO 44)

// Software Serial from S3 (CSV data channel)
#define S3_DATA_RX_PIN     10   // C3 RX ← S3 GPIO 14 (SW UART)

// S3 Programming pins
#define S3_BOOT_PIN        2    // C3 GPIO 2 → S3 GPIO 0 (BOOT)
#define S3_RESET_PIN       3    // C3 GPIO 3 → S3 EN (RESET)

// SD Card SPI pins
#define SD_CS_PIN          4
#define SD_SCK_PIN         5
#define SD_MOSI_PIN        6
#define SD_MISO_PIN        7

// ============================================================================
// SOFTWARE SERIAL CONSTANTS (receiving from S3 GPIO 14)
// ============================================================================
#define SW_SERIAL_BAUD     9600
#define SW_SERIAL_BIT_US   104   // 1000000 / 9600
#define SW_RX_BUFFER_SIZE  1200  // Large enough for 39-field CSV

// ============================================================================
// SD CARD BUFFER FILE
// ============================================================================
#define SD_BUFFER_FILE     "/buffer.csv"
#define SD_BUFFER_INDEX    "/buf_idx.txt"
#define SD_MAX_BUFFERED    10000  // Maximum lines before oldest are dropped

// ============================================================================
// FIREBASE POLL INTERVAL
// ============================================================================
#define FIREBASE_POLL_INTERVAL_MS    5000   // Check for commands every 5 seconds
#define FIREBASE_POLL_FAST_MS        1000   // During real-time mode
#define WIFI_RECONNECT_INTERVAL_MS   30000  // Retry WiFi every 30 seconds
#define SD_FLUSH_BATCH_SIZE          10     // Lines per flush batch
#define SD_FLUSH_INTERVAL_MS         2000   // Between flush batches

// ============================================================================
// OTA CONSTANTS
// ============================================================================
#define OTA_CHUNK_SIZE     1024   // Bytes per UART write to S3 bootloader
#define S3_BOOTLOADER_BAUD 115200 // ESP32-S3 ROM bootloader baud rate
// ESP32 ROM bootloader sync sequence
#define SLIP_END           0xC0
#define SLIP_ESC           0xDB
#define SLIP_ESC_END       0xDC
#define SLIP_ESC_ESC       0xDD

// ============================================================================
// STATE MACHINE STATES
// ============================================================================
enum C3State {
    STATE_NORMAL,          // Normal operation — receive CSV, push to Firebase
    STATE_BUFFERING,       // WiFi down — buffer to SD
    STATE_FLUSHING,        // WiFi restored — flushing SD buffer
    STATE_OTA_DOWNLOAD,    // Downloading OTA binary from Firebase Storage
    STATE_OTA_FLASH,       // Flashing S3 via bootloader
    STATE_OTA_VERIFY       // Verifying S3 came back after flash
};

enum RealTimeMode {
    RT_OFF = 0,            // Normal buffered operation
    RT_ON  = 1             // Real-time passthrough (skip SD buffer)
};

// ============================================================================
// GLOBAL STATE
// ============================================================================

// ---- WiFi ----
bool wifiConnected       = false;
unsigned long lastWifiAttemptMs = 0;

// ---- Firebase ----
unsigned long lastFirebasePollMs = 0;

// ---- State machine ----
C3State currentState     = STATE_NORMAL;
RealTimeMode realTimeMode = RT_OFF;

// ---- SD Card ----
bool sdAvailable          = false;
unsigned long sdBufferedCount = 0;
bool sdFlushInProgress    = false;
unsigned long lastFlushMs = 0;
File sdFlushFile;
bool sdFlushFileOpen      = false;

// ---- Software serial receive buffer (from S3 GPIO 14) ----
volatile char    swRxBuffer[SW_RX_BUFFER_SIZE];
volatile int     swRxWriteIdx = 0;
volatile int     swRxReadIdx  = 0;
volatile bool    swRxOverflow = false;

// ---- CSV line assembly buffer ----
char csvLineBuffer[SW_RX_BUFFER_SIZE];
int  csvLineBufIdx = 0;
bool csvLineReady  = false;
char lastCSVLine[SW_RX_BUFFER_SIZE];  // Last complete CSV for Firebase push

// ---- Command channel buffer (from S3 via HW Serial) ----
char cmdRxBuffer[512];
int  cmdRxBufIdx = 0;

// ---- S3 status tracking ----
bool s3Active            = true;   // False during OTA flash
unsigned long s3LastDataMs = 0;

// ---- OTA state ----
bool otaPending          = false;
char otaUrl[512]         = {0};     // URL to download firmware binary
uint32_t otaExpectedSize = 0;
uint32_t otaDownloadedSize = 0;
bool otaInProgress       = false;

// ---- SIM status (if SIM module connected to C3 — placeholder) ----
int  simRSSI             = 0;
bool simRegistered       = false;
bool simAvailable        = false;

// ---- Diagnostic forwarding ----
bool diagPending         = false;

// ---- Config received from Firebase ----
int  cfgNormalRateSec    = 900;
int  cfgHighRateSec      = 60;
float cfgHMaxCm          = 200.0f;
bool cfgDirty            = false;  // True when new config received from Firebase

// ============================================================================
// FORWARD DECLARATIONS
// ============================================================================

// WiFi
void wifiConnect();
void wifiCheck();

// Firebase REST API
bool firebasePush(const char* path, const char* jsonPayload);
bool firebasePut(const char* path, const char* jsonPayload);
String firebaseGet(const char* path);
bool firebaseDelete(const char* path);

// CSV processing
void processIncomingCSVData();
void assembleCSVLine(char c);
void handleCompleteCSVLine(const char* line);

// Firebase data push
void pushCSVToFirebase(const char* csvLine);
String csvToFirebaseJson(const char* csvLine);

// Firebase command polling
void pollFirebaseCommands();
void parseFirebaseConfig(const String& response);
void parseFirebaseOTACommand(const String& response);
void parseFirebaseRealtimeFlag(const String& response);
void parseFirebaseDiagRequest(const String& response);

// SD Card operations
void initSDCard();
void bufferToSD(const char* csvLine);
void flushSDToFirebase();
void flushSDOneBatch();
unsigned long countSDBufferedLines();

// Command channel with S3
void processS3Commands();
void handleS3Message(const char* msg);
void sendConfigToS3();
void sendPingToS3();
void sendDiagRunToS3();

// OTA programming
void startOTAProcess();
void downloadOTABinary();
void flashS3WithBinary(const char* filePath, uint32_t fileSize);
void enterS3BootloadMode();
void exitS3BootloadMode();
bool s3BootloaderSync();
bool s3BootloaderBeginFlash(uint32_t size, uint32_t numBlocks,
                             uint32_t blockSize, uint32_t offset);
bool s3BootloaderWriteBlock(const uint8_t* data, uint32_t dataLen,
                             uint32_t seqNum);
bool s3BootloaderEndFlash(bool reboot);
void slipSend(const uint8_t* data, uint32_t len);
bool slipReceive(uint8_t* buf, uint32_t maxLen, uint32_t* receivedLen,
                  uint32_t timeoutMs);
uint8_t calculateChecksum(const uint8_t* data, uint32_t len);

// Software serial (GPIO 10 bit-bang receive via interrupt)
void initSoftwareSerialRx();
void IRAM_ATTR swSerialStartBitISR();
void swSerialReadByte();

// Utility
void logMsg(const char* level, const char* msg);
String urlEncode(const String& str);
void extractJsonStringValue(const String& json, const char* key,
                             char* outBuf, int outBufSize);
int extractJsonIntValue(const String& json, const char* key, int defaultVal);
float extractJsonFloatValue(const String& json, const char* key, float defaultVal);
bool extractJsonBoolValue(const String& json, const char* key, bool defaultVal);


// ============================================================================
// ============================================================================
//                              SETUP
// ============================================================================
// ============================================================================

void setup() {
    // ========================================================================
    // 1. Initialize USB Serial for debug output
    // ========================================================================
    Serial.begin(115200);
    delay(1000);
    logMsg("INFO", "VARUNA C3 Communication Bridge — Firmware v2.0");
    logMsg("INFO", "Initializing...");

    // ========================================================================
    // 2. Initialize S3 programming pins (BOOT and RESET)
    //    Set them to HIGH (inactive) initially
    // ========================================================================
    pinMode(S3_BOOT_PIN, OUTPUT);
    digitalWrite(S3_BOOT_PIN, HIGH);   // HIGH = normal boot (not flash mode)

    pinMode(S3_RESET_PIN, OUTPUT);
    digitalWrite(S3_RESET_PIN, HIGH);  // HIGH = not in reset

    logMsg("INFO", "S3 BOOT/RESET pins initialized (both HIGH/inactive)");

    // ========================================================================
    // 3. Initialize Hardware Serial to S3 (command channel)
    //    C3 RX=GPIO20 ← S3 TX=GPIO43
    //    C3 TX=GPIO21 → S3 RX=GPIO44
    // ========================================================================
    Serial1.begin(9600, SERIAL_8N1, S3_CMD_RX_PIN, S3_CMD_TX_PIN);
    logMsg("INFO", "Serial1 (S3 cmd channel) initialized @ 9600 baud");

    // ========================================================================
    // 4. Initialize Software Serial RX on GPIO 10 (CSV data from S3)
    // ========================================================================
    initSoftwareSerialRx();
    logMsg("INFO", "Software Serial RX initialized on GPIO 10 @ 9600 baud");

    // ========================================================================
    // 5. Initialize SPI for SD Card
    // ========================================================================
    SPI.begin(SD_SCK_PIN, SD_MISO_PIN, SD_MOSI_PIN, SD_CS_PIN);
    initSDCard();

    // ========================================================================
    // 6. Connect to WiFi
    // ========================================================================
    wifiConnect();

    // ========================================================================
    // 7. Initialize timing
    // ========================================================================
    lastFirebasePollMs = millis();
    lastWifiAttemptMs  = millis();
    s3LastDataMs       = millis();

    logMsg("INFO", "C3 initialization complete — entering main loop");
}


// ============================================================================
// ============================================================================
//                             MAIN LOOP
// ============================================================================
// ============================================================================

void loop() {
    unsigned long nowMs = millis();

    // ========================================================================
    // TASK 0: WiFi monitoring and reconnection
    // ========================================================================
    wifiCheck();

    // ========================================================================
    // TASK 1: Receive CSV data from S3 (software serial GPIO 10)
    //         Assemble complete lines and process them
    // ========================================================================
    if (s3Active) {
        processIncomingCSVData();
    }

    // ========================================================================
    // TASK 2: Process command channel messages from S3 (HW Serial1)
    // ========================================================================
    if (s3Active) {
        processS3Commands();
    }

    // ========================================================================
    // TASK 3: Poll Firebase for commands from website
    // ========================================================================
    if (wifiConnected) {
        unsigned long pollInterval = (realTimeMode == RT_ON) ?
                                      FIREBASE_POLL_FAST_MS :
                                      FIREBASE_POLL_INTERVAL_MS;

        if ((nowMs - lastFirebasePollMs) >= pollInterval) {
            lastFirebasePollMs = nowMs;
            pollFirebaseCommands();
        }
    }

    // ========================================================================
    // TASK 4: If new config received from Firebase, forward to S3
    // ========================================================================
    if (cfgDirty && s3Active) {
        sendConfigToS3();
        cfgDirty = false;
    }

    // ========================================================================
    // TASK 5: If diagnostic requested from Firebase, trigger on S3
    // ========================================================================
    if (diagPending && s3Active) {
        sendDiagRunToS3();
        diagPending = false;
    }

    // ========================================================================
    // TASK 6: Flush SD buffer to Firebase when WiFi is available
    //         (only when NOT in real-time mode)
    // ========================================================================
    if (wifiConnected && sdAvailable && sdBufferedCount > 0 &&
        realTimeMode == RT_OFF && currentState != STATE_OTA_DOWNLOAD &&
        currentState != STATE_OTA_FLASH) {

        if ((nowMs - lastFlushMs) >= SD_FLUSH_INTERVAL_MS) {
            lastFlushMs = nowMs;
            flushSDOneBatch();
        }
    }

    // ========================================================================
    // TASK 7: OTA process
    // ========================================================================
    if (otaPending && wifiConnected) {
        otaPending = false;
        startOTAProcess();
    }

    // ========================================================================
    // TASK 8: Small yield to prevent watchdog
    // ========================================================================
    yield();
    delay(1);
}


// ============================================================================
// ============================================================================
//                       WiFi MANAGEMENT
// ============================================================================
// ============================================================================

void wifiConnect() {
    logMsg("INFO", "Connecting to WiFi...");

    WiFi.mode(WIFI_STA);
    WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

    unsigned long startMs = millis();
    while (WiFi.status() != WL_CONNECTED && (millis() - startMs < 15000)) {
        delay(500);
        Serial.print(".");
    }
    Serial.println();

    if (WiFi.status() == WL_CONNECTED) {
        wifiConnected = true;
        char msg[128];
        snprintf(msg, sizeof(msg), "WiFi connected — IP: %s  RSSI: %d dBm",
                 WiFi.localIP().toString().c_str(), WiFi.RSSI());
        logMsg("INFO", msg);

        // If we were buffering, switch to flushing
        if (currentState == STATE_BUFFERING) {
            currentState = STATE_NORMAL;
            logMsg("INFO", "WiFi restored — will flush SD buffer");
        }
    } else {
        wifiConnected = false;
        logMsg("WARN", "WiFi connection failed — will buffer to SD");
        currentState = STATE_BUFFERING;
    }

    lastWifiAttemptMs = millis();
}

void wifiCheck() {
    if (WiFi.status() == WL_CONNECTED) {
        if (!wifiConnected) {
            wifiConnected = true;
            logMsg("INFO", "WiFi reconnected");
            if (currentState == STATE_BUFFERING) {
                currentState = STATE_NORMAL;
            }
        }
    } else {
        if (wifiConnected) {
            wifiConnected = false;
            logMsg("WARN", "WiFi connection lost — switching to SD buffering");
            currentState = STATE_BUFFERING;
        }

        // Attempt reconnection periodically
        if ((millis() - lastWifiAttemptMs) >= WIFI_RECONNECT_INTERVAL_MS) {
            lastWifiAttemptMs = millis();
            logMsg("INFO", "Attempting WiFi reconnection...");
            WiFi.disconnect();
            delay(100);
            WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

            unsigned long start = millis();
            while (WiFi.status() != WL_CONNECTED && (millis() - start < 10000)) {
                delay(250);
            }

            if (WiFi.status() == WL_CONNECTED) {
                wifiConnected = true;
                logMsg("INFO", "WiFi reconnected successfully");
                if (currentState == STATE_BUFFERING) {
                    currentState = STATE_NORMAL;
                }
            }
        }
    }
}


// ============================================================================
// ============================================================================
//                       SOFTWARE SERIAL RX (GPIO 10)
// ============================================================================
// ============================================================================
//
// The S3 sends the 39-field CSV on GPIO 14 via software UART at 9600 baud.
// The C3 receives it on GPIO 10 using interrupt-driven bit-banging.
//
// When a falling edge (start bit) is detected on GPIO 10, an ISR fires.
// The ISR samples 8 data bits at the correct timing, assembles the byte,
// and pushes it into a ring buffer.
//
// The main loop reads from this ring buffer to assemble CSV lines.
//

// Timer handle for bit sampling
hw_timer_t* swSerialTimer = NULL;
volatile uint8_t swBitCount = 0;
volatile uint8_t swCurrentByte = 0;
volatile bool swReceiving = false;

void initSoftwareSerialRx() {
    pinMode(S3_DATA_RX_PIN, INPUT_PULLUP);

    // Attach interrupt on falling edge (start bit)
    attachInterrupt(digitalPinToInterrupt(S3_DATA_RX_PIN),
                    swSerialStartBitISR, FALLING);

    // Initialize hardware timer for bit sampling
    // Timer 0, prescaler 80 (1MHz tick = 1µs per tick)
    swSerialTimer = timerBegin(0, 80, true);
    timerAttachInterrupt(swSerialTimer, &swSerialBitTimerISR, true);
    // Don't start the timer yet — it starts on each start bit
    timerAlarmWrite(swSerialTimer, SW_SERIAL_BIT_US, true);
    timerAlarmDisable(swSerialTimer);

    swRxWriteIdx = 0;
    swRxReadIdx = 0;
    swRxOverflow = false;
}

void IRAM_ATTR swSerialStartBitISR() {
    if (swReceiving) return;  // Already receiving a byte

    // Disable the falling edge interrupt while receiving
    detachInterrupt(digitalPinToInterrupt(S3_DATA_RX_PIN));

    swReceiving = true;
    swBitCount = 0;
    swCurrentByte = 0;

    // Start timer — first alarm at 1.5 bit times to sample middle of first data bit
    timerRestart(swSerialTimer);
    timerAlarmWrite(swSerialTimer, SW_SERIAL_BIT_US + (SW_SERIAL_BIT_US / 2), false);
    timerAlarmEnable(swSerialTimer);
}

void IRAM_ATTR swSerialBitTimerISR() {
    if (!swReceiving) {
        timerAlarmDisable(swSerialTimer);
        return;
    }

    if (swBitCount < 8) {
        // Sample data bit
        int bitVal = digitalRead(S3_DATA_RX_PIN);
        if (bitVal) {
            swCurrentByte |= (1 << swBitCount);
        }
        swBitCount++;

        if (swBitCount < 8) {
            // Set timer for next bit (1 bit time from now)
            timerAlarmWrite(swSerialTimer, SW_SERIAL_BIT_US, false);
            timerAlarmEnable(swSerialTimer);
        } else {
            // All 8 bits received — wait for stop bit duration then finish
            timerAlarmWrite(swSerialTimer, SW_SERIAL_BIT_US, false);
            timerAlarmEnable(swSerialTimer);
        }
    } else {
        // Stop bit period — byte is complete
        timerAlarmDisable(swSerialTimer);
        swReceiving = false;

        // Push byte into ring buffer
        int nextWriteIdx = (swRxWriteIdx + 1) % SW_RX_BUFFER_SIZE;
        if (nextWriteIdx != swRxReadIdx) {
            swRxBuffer[swRxWriteIdx] = swCurrentByte;
            swRxWriteIdx = nextWriteIdx;
        } else {
            swRxOverflow = true;
        }

        // Re-attach falling edge interrupt for next byte
        attachInterrupt(digitalPinToInterrupt(S3_DATA_RX_PIN),
                        swSerialStartBitISR, FALLING);
    }
}

// Read one byte from the software serial ring buffer
// Returns -1 if no data available
int swSerialRead() {
    if (swRxReadIdx == swRxWriteIdx) return -1;

    char c = swRxBuffer[swRxReadIdx];
    swRxReadIdx = (swRxReadIdx + 1) % SW_RX_BUFFER_SIZE;
    return (int)(uint8_t)c;
}

int swSerialAvailable() {
    int avail = swRxWriteIdx - swRxReadIdx;
    if (avail < 0) avail += SW_RX_BUFFER_SIZE;
    return avail;
}


// ============================================================================
// ============================================================================
//                       CSV DATA PROCESSING
// ============================================================================
// ============================================================================

void processIncomingCSVData() {
    // Read bytes from software serial and assemble into lines
    while (swSerialAvailable() > 0) {
        int c = swSerialRead();
        if (c < 0) break;

        assembleCSVLine((char)c);
    }

    // If a complete CSV line is ready, process it
    if (csvLineReady) {
        csvLineReady = false;
        handleCompleteCSVLine(lastCSVLine);
    }
}

void assembleCSVLine(char c) {
    if (c == '\n' || c == '\r') {
        if (csvLineBufIdx > 0) {
            csvLineBuffer[csvLineBufIdx] = '\0';

            // Copy to lastCSVLine
            strncpy(lastCSVLine, csvLineBuffer, sizeof(lastCSVLine) - 1);
            lastCSVLine[sizeof(lastCSVLine) - 1] = '\0';

            csvLineReady = true;
            csvLineBufIdx = 0;
        }
    } else {
        if (csvLineBufIdx < (int)sizeof(csvLineBuffer) - 1) {
            csvLineBuffer[csvLineBufIdx++] = c;
        } else {
            // Buffer overflow — reset
            csvLineBufIdx = 0;
        }
    }
}

void handleCompleteCSVLine(const char* line) {
    s3LastDataMs = millis();

    // Check if it's a diagnostic frame
    if (strncmp(line, "$DIAG,", 6) == 0) {
        // Forward diagnostic to Firebase
        if (wifiConnected) {
            pushDiagToFirebase(line);
        }
        logMsg("INFO", "Received diagnostic frame from S3");
        return;
    }

    // It's a normal CSV data line — validate it has enough commas (38 for 39 fields)
    int commaCount = 0;
    for (int i = 0; line[i] != '\0'; i++) {
        if (line[i] == ',') commaCount++;
    }

    if (commaCount < 30) {
        // Not enough fields — likely corrupted
        char msg[64];
        snprintf(msg, sizeof(msg), "CSV rejected: only %d commas (need 38)", commaCount);
        logMsg("WARN", msg);
        return;
    }

    // Decide what to do with the CSV based on current state
    if (realTimeMode == RT_ON && wifiConnected) {
        // REAL-TIME MODE: push directly to Firebase, skip SD
        pushCSVToFirebase(line);
    }
    else if (wifiConnected && currentState == STATE_NORMAL) {
        // Normal mode with WiFi: push to Firebase
        pushCSVToFirebase(line);

        // Also save to SD for local backup if SD is available
        if (sdAvailable) {
            bufferToSD(line);
        }
    }
    else {
        // WiFi is down or we're in a non-normal state: buffer to SD
        if (sdAvailable) {
            bufferToSD(line);
            char msg[64];
            snprintf(msg, sizeof(msg), "Buffered to SD (total: %lu)", sdBufferedCount);
            logMsg("INFO", msg);
        } else {
            logMsg("ERROR", "WiFi down AND SD unavailable — DATA LOST");
        }
    }
}


// ============================================================================
// ============================================================================
//                       FIREBASE REST API
// ============================================================================
// ============================================================================
//
// We use raw HTTP REST calls to Firebase RTDB instead of any library.
// Firebase RTDB REST API:
//   PUT    https://<host>/<path>.json?auth=<secret>  — Set value
//   POST   https://<host>/<path>.json?auth=<secret>  — Push (auto-key)
//   GET    https://<host>/<path>.json?auth=<secret>  — Read value
//   DELETE https://<host>/<path>.json?auth=<secret>  — Delete
//
// All payloads are raw JSON strings built manually (no ArduinoJson).
//

bool firebasePush(const char* path, const char* jsonPayload) {
    if (!wifiConnected) return false;

    HTTPClient http;
    String url = String("https://") + FIREBASE_HOST + "/" + path +
                 ".json?auth=" + FIREBASE_AUTH;

    http.begin(url);
    http.addHeader("Content-Type", "application/json");
    http.setTimeout(10000);

    int httpCode = http.POST(jsonPayload);
    bool success = (httpCode >= 200 && httpCode < 300);

    if (!success) {
        char msg[128];
        snprintf(msg, sizeof(msg), "Firebase POST failed: HTTP %d at %s", httpCode, path);
        logMsg("ERROR", msg);
    }

    http.end();
    return success;
}

bool firebasePut(const char* path, const char* jsonPayload) {
    if (!wifiConnected) return false;

    HTTPClient http;
    String url = String("https://") + FIREBASE_HOST + "/" + path +
                 ".json?auth=" + FIREBASE_AUTH;

    http.begin(url);
    http.addHeader("Content-Type", "application/json");
    http.setTimeout(10000);

    int httpCode = http.PUT(jsonPayload);
    bool success = (httpCode >= 200 && httpCode < 300);

    if (!success) {
        char msg[128];
        snprintf(msg, sizeof(msg), "Firebase PUT failed: HTTP %d at %s", httpCode, path);
        logMsg("ERROR", msg);
    }

    http.end();
    return success;
}

String firebaseGet(const char* path) {
    if (!wifiConnected) return "";

    HTTPClient http;
    String url = String("https://") + FIREBASE_HOST + "/" + path +
                 ".json?auth=" + FIREBASE_AUTH;

    http.begin(url);
    http.setTimeout(10000);

    int httpCode = http.GET();
    String payload = "";

    if (httpCode >= 200 && httpCode < 300) {
        payload = http.getString();
    } else {
        char msg[128];
        snprintf(msg, sizeof(msg), "Firebase GET failed: HTTP %d at %s", httpCode, path);
        logMsg("ERROR", msg);
    }

    http.end();
    return payload;
}

bool firebaseDelete(const char* path) {
    if (!wifiConnected) return false;

    HTTPClient http;
    String url = String("https://") + FIREBASE_HOST + "/" + path +
                 ".json?auth=" + FIREBASE_AUTH;

    http.begin(url);
    http.setTimeout(10000);

    int httpCode = http.sendRequest("DELETE");
    bool success = (httpCode >= 200 && httpCode < 300);

    http.end();
    return success;
}


// ============================================================================
// ============================================================================
//                       CSV → FIREBASE JSON CONVERSION
// ============================================================================
// ============================================================================
//
// Convert the 39-field CSV into a JSON string manually.
// No ArduinoJson used — pure string formatting.
//

void pushCSVToFirebase(const char* csvLine) {
    String json = csvToFirebaseJson(csvLine);

    if (json.length() == 0) {
        logMsg("ERROR", "Failed to convert CSV to JSON");
        return;
    }

    // Push to devices/<DEVICE_ID>/data (auto-generated key)
    char path[128];
    snprintf(path, sizeof(path), "devices/%s/data", DEVICE_ID);

    bool success = firebasePush(path, json.c_str());

    if (success) {
        // Also update the "latest" node for real-time dashboard
        char latestPath[128];
        snprintf(latestPath, sizeof(latestPath), "devices/%s/latest", DEVICE_ID);
        firebasePut(latestPath, json.c_str());
    }
}

void pushDiagToFirebase(const char* diagLine) {
    // Push diagnostic frame as-is (or convert to JSON)
    char json[1024];
    snprintf(json, sizeof(json),
             "{\"raw\":\"%s\",\"timestamp\":{\".sv\":\"timestamp\"}}",
             diagLine);

    char path[128];
    snprintf(path, sizeof(path), "devices/%s/diagnostics", DEVICE_ID);
    firebasePush(path, json);
}

String csvToFirebaseJson(const char* csvLine) {
    // Parse the 39 CSV fields into separate values
    // Field names match the documentation
    char fields[40][32];
    int fieldCount = 0;

    // Split by comma
    int charIdx = 0;
    for (int i = 0; csvLine[i] != '\0' && fieldCount < 40; i++) {
        if (csvLine[i] == ',') {
            fields[fieldCount][charIdx] = '\0';
            fieldCount++;
            charIdx = 0;
        } else {
            if (charIdx < 31) {
                fields[fieldCount][charIdx++] = csvLine[i];
            }
        }
    }
    // Terminate last field
    if (fieldCount < 40) {
        fields[fieldCount][charIdx] = '\0';
        fieldCount++;
    }

    if (fieldCount < 39) {
        char msg[64];
        snprintf(msg, sizeof(msg), "CSV parse: only %d fields (need 39)", fieldCount);
        logMsg("WARN", msg);
        return "";
    }

    // Build JSON manually
    // Field 15 (dateTimeString) needs to be quoted as a string
    // All others are numeric

    // Use a large buffer
    char json[2048];
    int pos = 0;

    pos += snprintf(json + pos, sizeof(json) - pos, "{");

    // Field 1-14 (numeric)
    pos += snprintf(json + pos, sizeof(json) - pos,
        "\"theta\":%s,"           // 1
        "\"waterHeight\":%s,"     // 2
        "\"corrTiltX\":%s,"       // 3
        "\"corrTiltY\":%s,"       // 4
        "\"olpLength\":%s,"       // 5
        "\"horizDist\":%s,"       // 6
        "\"pressure\":%s,"        // 7
        "\"temperature\":%s,"     // 8
        "\"baselinePressure\":%s," // 9
        "\"pressureDev\":%s,"     // 10
        "\"submersionState\":%s," // 11
        "\"estDepth\":%s,"        // 12
        "\"bmpAvail\":%s,"        // 13
        "\"unixTime\":%s,",       // 14
        fields[0],  fields[1],  fields[2],  fields[3],
        fields[4],  fields[5],  fields[6],  fields[7],
        fields[8],  fields[9],  fields[10], fields[11],
        fields[12], fields[13]
    );

    // Field 15: dateTimeString (string — needs quotes)
    pos += snprintf(json + pos, sizeof(json) - pos,
        "\"dateTime\":\"%s\",", fields[14]);

    // Field 16-39 (numeric)
    pos += snprintf(json + pos, sizeof(json) - pos,
        "\"rtcValid\":%s,"       // 16
        "\"rateOfRise\":%s,"     // 17
        "\"alertLevel\":%s,"     // 18
        "\"sessionDur\":%s,"     // 19
        "\"peakHeight\":%s,"     // 20
        "\"minHeight\":%s,"      // 21
        "\"latitude\":%s,"       // 22
        "\"longitude\":%s,"      // 23
        "\"altitude\":%s,"       // 24
        "\"satellites\":%s,"     // 25
        "\"gpsFix\":%s,"         // 26
        "\"simRSSI\":%s,"        // 27
        "\"simReg\":%s,"         // 28
        "\"simAvail\":%s,"       // 29
        "\"zone\":%s,"           // 30
        "\"responseLevel\":%s,"  // 31
        "\"sustainedRise\":%s,"  // 32
        "\"battery\":%s,"        // 33
        "\"sampleInterval\":%s," // 34
        "\"txInterval\":%s,"     // 35
        "\"obLight\":%s,"        // 36
        "\"algoEnabled\":%s,"    // 37
        "\"mode\":%s,"           // 38
        "\"healthScore\":%s,",   // 39
        fields[15], fields[16], fields[17], fields[18],
        fields[19], fields[20], fields[21], fields[22],
        fields[23], fields[24], fields[25], fields[26],
        fields[27], fields[28], fields[29], fields[30],
        fields[31], fields[32], fields[33], fields[34],
        fields[35], fields[36], fields[37], fields[38]
    );

    // Add server timestamp
    pos += snprintf(json + pos, sizeof(json) - pos,
        "\"serverTimestamp\":{\".sv\":\"timestamp\"}"
    );

    pos += snprintf(json + pos, sizeof(json) - pos, "}");

    return String(json);
}


// ============================================================================
// ============================================================================
//                       FIREBASE COMMAND POLLING
// ============================================================================
// ============================================================================
//
// The C3 periodically polls Firebase for commands from the website:
//
//   devices/<DEVICE_ID>/commands/config
//     → { normalRate: 300, highRate: 30, hMaxCm: 250 }
//
//   devices/<DEVICE_ID>/commands/ota
//     → { url: "https://storage.../firmware.bin", size: 123456, pending: true }
//
//   devices/<DEVICE_ID>/commands/realtime
//     → { enabled: true }
//
//   devices/<DEVICE_ID>/commands/diagnostic
//     → { run: true }
//

void pollFirebaseCommands() {
    // ---- 1. Check for config updates ----
    {
        char path[128];
        snprintf(path, sizeof(path), "devices/%s/commands/config", DEVICE_ID);
        String response = firebaseGet(path);
        if (response.length() > 2 && response != "null") {
            parseFirebaseConfig(response);
        }
    }

    // ---- 2. Check for OTA command ----
    {
        char path[128];
        snprintf(path, sizeof(path), "devices/%s/commands/ota", DEVICE_ID);
        String response = firebaseGet(path);
        if (response.length() > 2 && response != "null") {
            parseFirebaseOTACommand(response);
        }
    }

    // ---- 3. Check for real-time mode flag ----
    {
        char path[128];
        snprintf(path, sizeof(path), "devices/%s/commands/realtime", DEVICE_ID);
        String response = firebaseGet(path);
        if (response.length() > 2 && response != "null") {
            parseFirebaseRealtimeFlag(response);
        }
    }

    // ---- 4. Check for diagnostic request ----
    {
        char path[128];
        snprintf(path, sizeof(path), "devices/%s/commands/diagnostic", DEVICE_ID);
        String response = firebaseGet(path);
        if (response.length() > 2 && response != "null") {
            parseFirebaseDiagRequest(response);
        }
    }

    // ---- 5. Update device status in Firebase ----
    {
        char statusJson[512];
        snprintf(statusJson, sizeof(statusJson),
                 "{"
                 "\"online\":true,"
                 "\"wifiRSSI\":%d,"
                 "\"sdAvail\":%s,"
                 "\"sdBuffered\":%lu,"
                 "\"s3Active\":%s,"
                 "\"realTimeMode\":%s,"
                 "\"freeHeap\":%lu,"
                 "\"uptime\":%lu,"
                 "\"lastUpdate\":{\".sv\":\"timestamp\"}"
                 "}",
                 WiFi.RSSI(),
                 sdAvailable ? "true" : "false",
                 sdBufferedCount,
                 s3Active ? "true" : "false",
                 realTimeMode == RT_ON ? "true" : "false",
                 (unsigned long)ESP.getFreeHeap(),
                 millis() / 1000UL
        );

        char path[128];
        snprintf(path, sizeof(path), "devices/%s/status", DEVICE_ID);
        firebasePut(path, statusJson);
    }
}

void parseFirebaseConfig(const String& response) {
    // Response format: {"normalRate":300,"highRate":30,"hMaxCm":250.0}
    // Parse without ArduinoJson using manual string extraction

    int newNormal = extractJsonIntValue(response, "normalRate", -1);
    int newHigh   = extractJsonIntValue(response, "highRate", -1);
    float newHMax = extractJsonFloatValue(response, "hMaxCm", -1.0f);

    bool changed = false;

    if (newNormal > 0 && newNormal != cfgNormalRateSec) {
        cfgNormalRateSec = newNormal;
        changed = true;
        char msg[64];
        snprintf(msg, sizeof(msg), "Config: normalRate = %d sec", cfgNormalRateSec);
        logMsg("INFO", msg);
    }

    if (newHigh > 0 && newHigh != cfgHighRateSec) {
        cfgHighRateSec = newHigh;
        changed = true;
        char msg[64];
        snprintf(msg, sizeof(msg), "Config: highRate = %d sec", cfgHighRateSec);
        logMsg("INFO", msg);
    }

    if (newHMax > 0 && fabsf(newHMax - cfgHMaxCm) > 0.01f) {
        cfgHMaxCm = newHMax;
        changed = true;
        char msg[64];
        snprintf(msg, sizeof(msg), "Config: hMaxCm = %.2f", cfgHMaxCm);
        logMsg("INFO", msg);
    }

    if (changed) {
        cfgDirty = true;

        // Clear the config command from Firebase so it doesn't re-trigger
        char path[128];
        snprintf(path, sizeof(path), "devices/%s/commands/config", DEVICE_ID);
        firebaseDelete(path);
    }
}

void parseFirebaseOTACommand(const String& response) {
    // Response format: {"url":"https://...","size":123456,"pending":true}

    bool pending = extractJsonBoolValue(response, "pending", false);

    if (!pending) return;  // Not a new OTA request

    extractJsonStringValue(response, "url", otaUrl, sizeof(otaUrl));
    otaExpectedSize = (uint32_t)extractJsonIntValue(response, "size", 0);

    if (strlen(otaUrl) > 10 && otaExpectedSize > 0) {
        logMsg("INFO", "OTA command received from Firebase");
        char msg[128];
        snprintf(msg, sizeof(msg), "OTA URL: %s  Size: %lu bytes", otaUrl, otaExpectedSize);
        logMsg("INFO", msg);

        otaPending = true;

        // Mark OTA as acknowledged in Firebase
        char path[128];
        snprintf(path, sizeof(path), "devices/%s/commands/ota", DEVICE_ID);
        firebasePut(path, "{\"pending\":false,\"status\":\"downloading\"}");
    }
}

void parseFirebaseRealtimeFlag(const String& response) {
    bool enabled = extractJsonBoolValue(response, "enabled", false);

    RealTimeMode newMode = enabled ? RT_ON : RT_OFF;

    if (newMode != realTimeMode) {
        realTimeMode = newMode;

        if (realTimeMode == RT_ON) {
            logMsg("INFO", "REAL-TIME MODE ENABLED — bypassing SD buffer");
        } else {
            logMsg("INFO", "REAL-TIME MODE DISABLED — normal buffered operation");
            // Note: SD buffered data stays on SD, will be flushed normally
        }
    }
}

void parseFirebaseDiagRequest(const String& response) {
    bool run = extractJsonBoolValue(response, "run", false);

    if (run) {
        logMsg("INFO", "Diagnostic request received from Firebase");
        diagPending = true;

        // Clear the command
        char path[128];
        snprintf(path, sizeof(path), "devices/%s/commands/diagnostic", DEVICE_ID);
        firebaseDelete(path);
    }
}


// ============================================================================
// ============================================================================
//                       SD CARD OPERATIONS
// ============================================================================
// ============================================================================

void initSDCard() {
    logMsg("INFO", "Initializing SD card...");

    if (!SD.begin(SD_CS_PIN)) {
        logMsg("ERROR", "SD card initialization failed");
        sdAvailable = false;
        return;
    }

    uint8_t cardType = SD.cardType();
    if (cardType == CARD_NONE) {
        logMsg("ERROR", "No SD card inserted");
        sdAvailable = false;
        return;
    }

    sdAvailable = true;

    // Count existing buffered lines
    sdBufferedCount = countSDBufferedLines();

    char msg[128];
    snprintf(msg, sizeof(msg), "SD card initialized — Type: %s  Size: %lluMB  Buffered: %lu lines",
             (cardType == CARD_SD) ? "SD" :
             (cardType == CARD_SDHC) ? "SDHC" : "Unknown",
             SD.cardSize() / (1024 * 1024),
             sdBufferedCount);
    logMsg("INFO", msg);
}

void bufferToSD(const char* csvLine) {
    if (!sdAvailable) return;

    File f = SD.open(SD_BUFFER_FILE, FILE_APPEND);
    if (!f) {
        logMsg("ERROR", "Failed to open SD buffer file for writing");
        return;
    }

    f.println(csvLine);
    f.close();

    sdBufferedCount++;
}

unsigned long countSDBufferedLines() {
    if (!sdAvailable) return 0;

    File f = SD.open(SD_BUFFER_FILE, FILE_READ);
    if (!f) return 0;

    unsigned long count = 0;
    while (f.available()) {
        char c = f.read();
        if (c == '\n') count++;
    }
    f.close();
    return count;
}

void flushSDOneBatch() {
    if (!sdAvailable || sdBufferedCount == 0 || !wifiConnected) return;

    // Open the buffer file for reading
    File f = SD.open(SD_BUFFER_FILE, FILE_READ);
    if (!f) {
        logMsg("ERROR", "Failed to open SD buffer file for flushing");
        return;
    }

    // Read and push up to SD_FLUSH_BATCH_SIZE lines
    int flushed = 0;
    char lineBuf[SW_RX_BUFFER_SIZE];
    int lineIdx = 0;
    long lastGoodPos = 0;  // Track position of last successfully flushed line

    // We need to track lines to skip (already flushed ones)
    // Strategy: read the flush index file to know where we left off
    long startPos = 0;
    File idxFile = SD.open(SD_BUFFER_INDEX, FILE_READ);
    if (idxFile) {
        char idxBuf[32];
        int idxLen = idxFile.readBytesUntil('\n', idxBuf, sizeof(idxBuf) - 1);
        idxBuf[idxLen] = '\0';
        startPos = atol(idxBuf);
        idxFile.close();
    }

    f.seek(startPos);

    while (f.available() && flushed < SD_FLUSH_BATCH_SIZE) {
        char c = f.read();

        if (c == '\n' || c == '\r') {
            if (lineIdx > 0) {
                lineBuf[lineIdx] = '\0';

                // Push to Firebase
                bool success = false;

                // Skip diagnostic frames during flush
                if (strncmp(lineBuf, "$DIAG,", 6) == 0) {
                    pushDiagToFirebase(lineBuf);
                    success = true;
                } else {
                    String json = csvToFirebaseJson(lineBuf);
                    if (json.length() > 0) {
                        char path[128];
                        snprintf(path, sizeof(path),
                                 "devices/%s/buffered_data", DEVICE_ID);
                        success = firebasePush(path, json.c_str());
                    }
                }

                if (success) {
                    flushed++;
                    lastGoodPos = f.position();
                } else {
                    // Firebase push failed — stop flushing, try again later
                    logMsg("WARN", "SD flush: Firebase push failed, will retry");
                    break;
                }

                lineIdx = 0;
            }
        } else {
            if (lineIdx < (int)sizeof(lineBuf) - 1) {
                lineBuf[lineIdx++] = c;
            }
        }
    }

    bool reachedEnd = !f.available();
    f.close();

    // Update the flush index
    if (flushed > 0) {
        File idxW = SD.open(SD_BUFFER_INDEX, FILE_WRITE);
        if (idxW) {
            idxW.println(lastGoodPos);
            idxW.close();
        }

        if (sdBufferedCount >= (unsigned long)flushed) {
            sdBufferedCount -= flushed;
        } else {
            sdBufferedCount = 0;
        }

        char msg[64];
        snprintf(msg, sizeof(msg), "SD flush: pushed %d lines, %lu remaining",
                 flushed, sdBufferedCount);
        logMsg("INFO", msg);
    }

    // If we've flushed everything, clean up the files
    if (reachedEnd && sdBufferedCount == 0) {
        SD.remove(SD_BUFFER_FILE);
        SD.remove(SD_BUFFER_INDEX);
        sdBufferedCount = 0;
        logMsg("INFO", "SD buffer fully flushed — files cleaned up");
    }
}


// ============================================================================
// ============================================================================
//                       S3 COMMAND CHANNEL (HW Serial1)
// ============================================================================
// ============================================================================

void processS3Commands() {
    while (Serial1.available()) {
        char c = Serial1.read();

        if (c == '\n' || c == '\r') {
            if (cmdRxBufIdx > 0) {
                cmdRxBuffer[cmdRxBufIdx] = '\0';
                handleS3Message(cmdRxBuffer);
                cmdRxBufIdx = 0;
            }
        } else {
            if (cmdRxBufIdx < (int)sizeof(cmdRxBuffer) - 1) {
                cmdRxBuffer[cmdRxBufIdx++] = c;
            } else {
                cmdRxBufIdx = 0;
            }
        }
    }
}

void handleS3Message(const char* msg) {
    // Messages from S3 on the command channel:
    //   $PONG          — Response to our $PING
    //   $CFG_ACK       — Config update acknowledged
    //   $DIAG_ACK      — Diagnostic trigger acknowledged
    //   $FLAG_ACK      — Flag set acknowledged
    //   $DIAG,...      — Diagnostic results (also comes via GPIO14)
    //   $SETAPN        — Forwarded SIM command from debugger
    //   $REINITSIM     — Forwarded SIM command from debugger
    //   $TESTGPRS      — Forwarded SIM command from debugger

    if (strcmp(msg, "$PONG") == 0) {
        logMsg("INFO", "S3 PONG received — communication OK");
    }
    else if (strcmp(msg, "$CFG_ACK") == 0) {
        logMsg("INFO", "S3 acknowledged config update");
    }
    else if (strcmp(msg, "$DIAG_ACK") == 0) {
        logMsg("INFO", "S3 acknowledged diagnostic request");
    }
    else if (strcmp(msg, "$FLAG_ACK") == 0) {
        logMsg("INFO", "S3 acknowledged flag update");
    }
    else if (strncmp(msg, "$DIAG,", 6) == 0) {
        // Diagnostic frame on command channel — push to Firebase
        if (wifiConnected) {
            pushDiagToFirebase(msg);
        }
    }
    else if (strcmp(msg, "$SETAPN") == 0 || strcmp(msg, "$REINITSIM") == 0 ||
             strcmp(msg, "$TESTGPRS") == 0) {
        // SIM commands forwarded from debugger via S3
        // Handle locally (C3 could have SIM in future)
        char logBuf[64];
        snprintf(logBuf, sizeof(logBuf), "SIM command from debugger (via S3): %s", msg);
        logMsg("INFO", logBuf);
    }
    else {
        char logBuf[128];
        snprintf(logBuf, sizeof(logBuf), "Unknown S3 message: %s", msg);
        logMsg("WARN", logBuf);
    }
}

void sendConfigToS3() {
    // Send $CFG,normalRate,highRate,hMaxCm to S3
    char cmd[128];
    snprintf(cmd, sizeof(cmd), "$CFG,%d,%d,%.2f",
             cfgNormalRateSec, cfgHighRateSec, cfgHMaxCm);

    Serial1.println(cmd);

    char msg[128];
    snprintf(msg, sizeof(msg), "Sent to S3: %s", cmd);
    logMsg("INFO", msg);
}

void sendPingToS3() {
    Serial1.println("$PING");
    logMsg("INFO", "Sent $PING to S3");
}

void sendDiagRunToS3() {
    Serial1.println("$DIAGRUN");
    logMsg("INFO", "Sent $DIAGRUN to S3");
}

void sendSimStatusToS3() {
    // Forward SIM status to S3 so it can include in CSV
    char cmd[64];
    snprintf(cmd, sizeof(cmd), "$SIMSTAT,%d,%d,%d",
             simRSSI, simRegistered ? 1 : 0, simAvailable ? 1 : 0);
    Serial1.println(cmd);
}


// ============================================================================
// ============================================================================
//                       OTA FIRMWARE UPDATE — S3 PROGRAMMING
// ============================================================================
// ============================================================================
//
// The OTA process:
//
// 1. Website uploads new firmware .bin to Firebase Storage
// 2. Website writes OTA command to Firebase RTDB with download URL + size
// 3. C3 polls Firebase, detects OTA command
// 4. C3 downloads .bin from Firebase Storage to SD card
// 5. C3 puts S3 into bootloader mode:
//    a. Pull S3 GPIO0 (BOOT) LOW via C3 GPIO2
//    b. Pulse S3 EN (RESET) LOW then HIGH via C3 GPIO3
//    c. S3 enters UART bootloader (ROM boot mode)
// 6. C3 switches Serial1 to 115200 baud (bootloader speed)
// 7. C3 uses ESP32 serial bootloader protocol (SLIP framing):
//    a. Sync with bootloader
//    b. SPI_FLASH_BEGIN
//    c. SPI_FLASH_DATA (send firmware in blocks)
//    d. SPI_FLASH_END (with reboot flag)
// 8. C3 releases BOOT pin (HIGH)
// 9. S3 resets and boots new firmware
// 10. C3 switches Serial1 back to 9600 baud
// 11. C3 verifies S3 responds to $PING
// 12. C3 reports OTA result to Firebase
//

void startOTAProcess() {
    logMsg("INFO", "========== OTA UPDATE STARTING ==========");

    currentState = STATE_OTA_DOWNLOAD;
    s3Active = false;  // S3 will be inactive during OTA

    // Update Firebase status
    {
        char path[128];
        snprintf(path, sizeof(path), "devices/%s/commands/ota", DEVICE_ID);
        firebasePut(path, "{\"pending\":false,\"status\":\"downloading\"}");
    }

    // ---- Step 1: Download firmware binary to SD card ----
    logMsg("INFO", "OTA Step 1: Downloading firmware binary...");

    if (!sdAvailable) {
        logMsg("ERROR", "OTA ABORTED: SD card not available for firmware storage");
        reportOTAResult(false, "SD card not available");
        currentState = STATE_NORMAL;
        s3Active = true;
        return;
    }

    bool downloadOk = downloadOTABinaryToSD("/ota_firmware.bin");

    if (!downloadOk) {
        logMsg("ERROR", "OTA ABORTED: Firmware download failed");
        reportOTAResult(false, "Download failed");
        currentState = STATE_NORMAL;
        s3Active = true;
        return;
    }

    logMsg("INFO", "OTA Step 1 complete: Firmware downloaded to SD");

    // ---- Step 2: Verify file size ----
    File fwFile = SD.open("/ota_firmware.bin", FILE_READ);
    if (!fwFile) {
        logMsg("ERROR", "OTA ABORTED: Cannot open downloaded firmware file");
        reportOTAResult(false, "File open failed");
        currentState = STATE_NORMAL;
        s3Active = true;
        return;
    }

    uint32_t actualSize = fwFile.size();
    fwFile.close();

    char msg[128];
    snprintf(msg, sizeof(msg), "OTA firmware size: %lu bytes (expected %lu)",
             actualSize, otaExpectedSize);
    logMsg("INFO", msg);

    if (otaExpectedSize > 0 && actualSize != otaExpectedSize) {
        logMsg("ERROR", "OTA ABORTED: Size mismatch");
        reportOTAResult(false, "Size mismatch");
        SD.remove("/ota_firmware.bin");
        currentState = STATE_NORMAL;
        s3Active = true;
        return;
    }

    // ---- Step 3: Enter S3 bootloader mode ----
    logMsg("INFO", "OTA Step 3: Entering S3 bootloader mode...");
    currentState = STATE_OTA_FLASH;

    {
        char path[128];
        snprintf(path, sizeof(path), "devices/%s/commands/ota", DEVICE_ID);
        firebasePut(path, "{\"pending\":false,\"status\":\"flashing\"}");
    }

    enterS3BootloadMode();

    // ---- Step 4: Flash the firmware ----
    logMsg("INFO", "OTA Step 4: Flashing firmware to S3...");
    flashS3WithBinary("/ota_firmware.bin", actualSize);

    // ---- Step 5: Exit bootloader mode ----
    logMsg("INFO", "OTA Step 5: Exiting bootloader mode...");
    exitS3BootloadMode();

    // ---- Step 6: Verify S3 is running ----
    logMsg("INFO", "OTA Step 6: Verifying S3 response...");
    currentState = STATE_OTA_VERIFY;

    delay(3000);  // Wait for S3 to boot new firmware

    bool verified = verifyS3Running();

    if (verified) {
        logMsg("INFO", "========== OTA UPDATE SUCCESSFUL ==========");
        reportOTAResult(true, "Success");
    } else {
        logMsg("ERROR", "========== OTA UPDATE FAILED — S3 NOT RESPONDING ==========");
        reportOTAResult(false, "S3 not responding after flash");
    }

    // Clean up
    SD.remove("/ota_firmware.bin");
    currentState = STATE_NORMAL;
    s3Active = true;  // Assume S3 is active (or will be)
}

bool downloadOTABinaryToSD(const char* sdPath) {
    // Remove old file if exists
    if (SD.exists(sdPath)) {
        SD.remove(sdPath);
    }

    HTTPClient http;
    http.begin(String(otaUrl));
    http.setTimeout(60000);  // 60 second timeout for large files

    int httpCode = http.GET();

    if (httpCode != 200) {
        char msg[128];
        snprintf(msg, sizeof(msg), "OTA download HTTP error: %d", httpCode);
        logMsg("ERROR", msg);
        http.end();
        return false;
    }

    int totalSize = http.getSize();
    if (totalSize <= 0) {
        logMsg("ERROR", "OTA download: Content-Length unknown or zero");
        http.end();
        return false;
    }

    File outFile = SD.open(sdPath, FILE_WRITE);
    if (!outFile) {
        logMsg("ERROR", "OTA download: Failed to create file on SD");
        http.end();
        return false;
    }

    WiFiClient* stream = http.getStreamPtr();
    uint8_t dlBuf[1024];
    int downloaded = 0;
    int lastPercent = -1;

    while (http.connected() && downloaded < totalSize) {
        size_t avail = stream->available();
        if (avail > 0) {
            size_t toRead = (avail > sizeof(dlBuf)) ? sizeof(dlBuf) : avail;
            size_t bytesRead = stream->readBytes(dlBuf, toRead);

            outFile.write(dlBuf, bytesRead);
            downloaded += bytesRead;

            int percent = (downloaded * 100) / totalSize;
            if (percent != lastPercent && percent % 10 == 0) {
                lastPercent = percent;
                char msg[64];
                snprintf(msg, sizeof(msg), "OTA download: %d%%", percent);
                logMsg("INFO", msg);
            }
        }
        delay(1);
    }

    outFile.close();
    http.end();

    char msg[128];
    snprintf(msg, sizeof(msg), "OTA download complete: %d / %d bytes", downloaded, totalSize);
    logMsg("INFO", msg);

    return (downloaded == totalSize);
}

void enterS3BootloadMode() {
    // ESP32-S3 enters UART download mode when:
    //   GPIO0 is held LOW during reset
    //
    // Sequence:
    //   1. Pull GPIO0 (BOOT) LOW
    //   2. Pulse EN (RESET) LOW for 100ms then HIGH
    //   3. S3 resets and enters ROM bootloader because GPIO0 is LOW
    //   4. Wait 500ms for bootloader to initialize

    logMsg("INFO", "Entering S3 bootloader: BOOT=LOW, RESET pulse");

    // Step 1: Pull BOOT low
    digitalWrite(S3_BOOT_PIN, LOW);
    delay(50);

    // Step 2: Reset pulse
    digitalWrite(S3_RESET_PIN, LOW);
    delay(100);
    digitalWrite(S3_RESET_PIN, HIGH);

    // Step 3: Wait for bootloader to start
    delay(500);

    // Step 4: Switch command serial to bootloader baud rate
    Serial1.end();
    delay(50);
    Serial1.begin(S3_BOOTLOADER_BAUD, SERIAL_8N1, S3_CMD_RX_PIN, S3_CMD_TX_PIN);
    delay(100);

    logMsg("INFO", "S3 should now be in ROM bootloader mode (115200 baud)");
}

void exitS3BootloadMode() {
    // Release BOOT pin and reset S3 to boot normally
    logMsg("INFO", "Exiting bootloader: BOOT=HIGH, RESET pulse");

    // Release BOOT
    digitalWrite(S3_BOOT_PIN, HIGH);
    delay(50);

    // Reset S3 to boot normally
    digitalWrite(S3_RESET_PIN, LOW);
    delay(100);
    digitalWrite(S3_RESET_PIN, HIGH);

    // Switch serial back to normal baud rate
    delay(500);
    Serial1.end();
    delay(50);
    Serial1.begin(9600, SERIAL_8N1, S3_CMD_RX_PIN, S3_CMD_TX_PIN);
    delay(100);

    logMsg("INFO", "S3 reset to normal boot mode (9600 baud)");
}

bool verifyS3Running() {
    // Send $PING and wait for $PONG
    int attempts = 5;

    for (int i = 0; i < attempts; i++) {
        // Clear serial buffer
        while (Serial1.available()) Serial1.read();

        Serial1.println("$PING");

        unsigned long start = millis();
        char pongBuf[32];
        int pongIdx = 0;

        while (millis() - start < 3000) {
            if (Serial1.available()) {
                char c = Serial1.read();
                if (c == '\n' || c == '\r') {
                    pongBuf[pongIdx] = '\0';
                    if (strcmp(pongBuf, "$PONG") == 0) {
                        logMsg("INFO", "S3 verified — $PONG received");
                        return true;
                    }
                    pongIdx = 0;
                } else if (pongIdx < 30) {
                    pongBuf[pongIdx++] = c;
                }
            }
        }

        char msg[64];
        snprintf(msg, sizeof(msg), "S3 verify attempt %d/%d — no PONG", i + 1, attempts);
        logMsg("WARN", msg);
        delay(2000);
    }

    return false;
}

void reportOTAResult(bool success, const char* message) {
    char json[256];
    snprintf(json, sizeof(json),
             "{\"pending\":false,\"status\":\"%s\",\"message\":\"%s\","
             "\"timestamp\":{\".sv\":\"timestamp\"}}",
             success ? "success" : "failed", message);

    char path[128];
    snprintf(path, sizeof(path), "devices/%s/commands/ota", DEVICE_ID);

    if (wifiConnected) {
        firebasePut(path, json);
    }
}


// ============================================================================
// ============================================================================
//                       ESP32 ROM BOOTLOADER PROTOCOL
// ============================================================================
// ============================================================================
//
// The ESP32-S3 ROM bootloader uses a SLIP-framed protocol.
// Reference: esptool.py source code and ESP32 technical reference.
//
// SLIP framing:
//   0xC0 [payload] 0xC0
//   Escape: 0xDB 0xDC = 0xC0 in data
//           0xDB 0xDD = 0xDB in data
//
// Command packet format:
//   [0]    = 0x00 (direction: request)
//   [1]    = command opcode
//   [2-3]  = data length (little-endian)
//   [4-7]  = checksum (for data commands) or 0
//   [8..]  = data
//
// Response packet format:
//   [0]    = 0x01 (direction: response)
//   [1]    = command opcode
//   [2-3]  = data length
//   [4-7]  = value
//   [8]    = status (0 = success)
//   [9]    = error code
//

#define ESP_CMD_SYNC            0x08
#define ESP_CMD_FLASH_BEGIN     0x02
#define ESP_CMD_FLASH_DATA      0x03
#define ESP_CMD_FLASH_END       0x04
#define ESP_FLASH_BLOCK_SIZE    0x4000  // 16KB blocks (for flash begin)
#define ESP_CHECKSUM_MAGIC      0xEF

void flashS3WithBinary(const char* filePath, uint32_t fileSize) {
    // Step 1: Sync with bootloader
    logMsg("INFO", "Bootloader: Syncing...");

    bool synced = false;
    for (int attempt = 0; attempt < 10; attempt++) {
        if (s3BootloaderSync()) {
            synced = true;
            char msg[64];
            snprintf(msg, sizeof(msg), "Bootloader sync OK (attempt %d)", attempt + 1);
            logMsg("INFO", msg);
            break;
        }
        delay(100);
    }

    if (!synced) {
        logMsg("ERROR", "Bootloader sync FAILED after 10 attempts");
        return;
    }

    // Step 2: Calculate flash parameters
    uint32_t numBlocks = (fileSize + ESP_FLASH_BLOCK_SIZE - 1) / ESP_FLASH_BLOCK_SIZE;
    uint32_t eraseSize = fileSize;

    char msg[128];
    snprintf(msg, sizeof(msg), "Flash: %lu bytes in %lu blocks", fileSize, numBlocks);
    logMsg("INFO", msg);

    // Step 3: FLASH_BEGIN
    logMsg("INFO", "Bootloader: FLASH_BEGIN...");
    if (!s3BootloaderBeginFlash(eraseSize, numBlocks, ESP_FLASH_BLOCK_SIZE, 0x0000)) {
        logMsg("ERROR", "FLASH_BEGIN failed");
        return;
    }
    logMsg("INFO", "FLASH_BEGIN OK");

    // Step 4: FLASH_DATA — send file in blocks
    File fwFile = SD.open(filePath, FILE_READ);
    if (!fwFile) {
        logMsg("ERROR", "Cannot open firmware file for flashing");
        return;
    }

    uint8_t blockBuf[ESP_FLASH_BLOCK_SIZE];
    uint32_t seqNum = 0;
    uint32_t remaining = fileSize;
    int lastPercent = -1;

    while (remaining > 0 && fwFile.available()) {
        uint32_t toRead = (remaining > ESP_FLASH_BLOCK_SIZE) ?
                           ESP_FLASH_BLOCK_SIZE : remaining;
        size_t bytesRead = fwFile.read(blockBuf, toRead);

        if (bytesRead == 0) {
            logMsg("ERROR", "File read returned 0 bytes");
            break;
        }

        // Pad last block with 0xFF if needed
        if (bytesRead < ESP_FLASH_BLOCK_SIZE) {
            memset(blockBuf + bytesRead, 0xFF, ESP_FLASH_BLOCK_SIZE - bytesRead);
        }

        if (!s3BootloaderWriteBlock(blockBuf, ESP_FLASH_BLOCK_SIZE, seqNum)) {
            char errMsg[64];
            snprintf(errMsg, sizeof(errMsg), "FLASH_DATA failed at block %lu", seqNum);
            logMsg("ERROR", errMsg);
            fwFile.close();
            return;
        }

        remaining -= bytesRead;
        seqNum++;

        int percent = ((fileSize - remaining) * 100) / fileSize;
        if (percent != lastPercent && percent % 10 == 0) {
            lastPercent = percent;
            char progMsg[64];
            snprintf(progMsg, sizeof(progMsg), "Flashing: %d%%", percent);
            logMsg("INFO", progMsg);
        }

        yield();  // Prevent watchdog timeout
    }

    fwFile.close();

    // Step 5: FLASH_END
    logMsg("INFO", "Bootloader: FLASH_END...");
    if (!s3BootloaderEndFlash(true)) {  // true = reboot after flash
        logMsg("WARN", "FLASH_END response not received (S3 may have rebooted)");
    } else {
        logMsg("INFO", "FLASH_END OK — S3 rebooting with new firmware");
    }
}

bool s3BootloaderSync() {
    // Sync command: opcode 0x08
    // Data: 0x07 0x07 0x12 0x20 followed by 32 bytes of 0x55

    uint8_t syncData[36];
    syncData[0] = 0x07;
    syncData[1] = 0x07;
    syncData[2] = 0x12;
    syncData[3] = 0x20;
    for (int i = 4; i < 36; i++) {
        syncData[i] = 0x55;
    }

    // Build command packet
    uint8_t cmdPacket[44];  // 8 header + 36 data
    cmdPacket[0] = 0x00;    // Direction: request
    cmdPacket[1] = ESP_CMD_SYNC;
    cmdPacket[2] = 36;      // Data length low
    cmdPacket[3] = 0;       // Data length high
    cmdPacket[4] = 0;       // Checksum (not used for sync)
    cmdPacket[5] = 0;
    cmdPacket[6] = 0;
    cmdPacket[7] = 0;
    memcpy(cmdPacket + 8, syncData, 36);

    slipSend(cmdPacket, 44);

    // Read response
    uint8_t respBuf[128];
    uint32_t respLen = 0;

    // Bootloader sends multiple sync responses — read until we get one
    for (int i = 0; i < 8; i++) {
        if (slipReceive(respBuf, sizeof(respBuf), &respLen, 500)) {
            if (respLen >= 10 && respBuf[0] == 0x01 && respBuf[1] == ESP_CMD_SYNC) {
                return true;
            }
        }
    }

    return false;
}

bool s3BootloaderBeginFlash(uint32_t size, uint32_t numBlocks,
                             uint32_t blockSize, uint32_t offset) {
    // FLASH_BEGIN command: opcode 0x02
    // Data: [size(4)] [numBlocks(4)] [blockSize(4)] [offset(4)]

    uint8_t cmdPacket[8 + 16];
    cmdPacket[0] = 0x00;  // Request
    cmdPacket[1] = ESP_CMD_FLASH_BEGIN;
    cmdPacket[2] = 16;    // Data length
    cmdPacket[3] = 0;
    cmdPacket[4] = 0;     // Checksum (not used for begin)
    cmdPacket[5] = 0;
    cmdPacket[6] = 0;
    cmdPacket[7] = 0;

    // Data payload (little-endian)
    cmdPacket[8]  = (size >> 0)  & 0xFF;
    cmdPacket[9]  = (size >> 8)  & 0xFF;
    cmdPacket[10] = (size >> 16) & 0xFF;
    cmdPacket[11] = (size >> 24) & 0xFF;

    cmdPacket[12] = (numBlocks >> 0)  & 0xFF;
    cmdPacket[13] = (numBlocks >> 8)  & 0xFF;
    cmdPacket[14] = (numBlocks >> 16) & 0xFF;
    cmdPacket[15] = (numBlocks >> 24) & 0xFF;

    cmdPacket[16] = (blockSize >> 0)  & 0xFF;
    cmdPacket[17] = (blockSize >> 8)  & 0xFF;
    cmdPacket[18] = (blockSize >> 16) & 0xFF;
    cmdPacket[19] = (blockSize >> 24) & 0xFF;

    cmdPacket[20] = (offset >> 0)  & 0xFF;
    cmdPacket[21] = (offset >> 8)  & 0xFF;
    cmdPacket[22] = (offset >> 16) & 0xFF;
    cmdPacket[23] = (offset >> 24) & 0xFF;

    slipSend(cmdPacket, 24);

    // Read response
    uint8_t respBuf[64];
    uint32_t respLen = 0;
    if (slipReceive(respBuf, sizeof(respBuf), &respLen, 10000)) {
        if (respLen >= 10 && respBuf[0] == 0x01 && respBuf[1] == ESP_CMD_FLASH_BEGIN) {
            // Check status byte
            if (respBuf[8] == 0x00) return true;
        }
    }

    return false;
}

bool s3BootloaderWriteBlock(const uint8_t* data, uint32_t dataLen,
                             uint32_t seqNum) {
    // FLASH_DATA command: opcode 0x03
    // Data: [dataLen(4)] [seqNum(4)] [0(4)] [0(4)] [data...]
    // Checksum covers the data portion

    uint32_t totalDataLen = 16 + dataLen;  // 16-byte header + actual data

    // Allocate command packet
    // Header: 8 bytes + data payload
    uint32_t packetLen = 8 + totalDataLen;
    uint8_t* cmdPacket = (uint8_t*)malloc(packetLen);
    if (!cmdPacket) {
        logMsg("ERROR", "OTA: malloc failed for flash data packet");
        return false;
    }

    cmdPacket[0] = 0x00;  // Request
    cmdPacket[1] = ESP_CMD_FLASH_DATA;
    cmdPacket[2] = (totalDataLen >> 0) & 0xFF;
    cmdPacket[3] = (totalDataLen >> 8) & 0xFF;

    // Checksum of the data payload
    uint8_t checksum = ESP_CHECKSUM_MAGIC;
    for (uint32_t i = 0; i < dataLen; i++) {
        checksum ^= data[i];
    }
    cmdPacket[4] = checksum;
    cmdPacket[5] = 0;
    cmdPacket[6] = 0;
    cmdPacket[7] = 0;

    // Data header
    cmdPacket[8]  = (dataLen >> 0)  & 0xFF;
    cmdPacket[9]  = (dataLen >> 8)  & 0xFF;
    cmdPacket[10] = (dataLen >> 16) & 0xFF;
    cmdPacket[11] = (dataLen >> 24) & 0xFF;

    cmdPacket[12] = (seqNum >> 0)  & 0xFF;
    cmdPacket[13] = (seqNum >> 8)  & 0xFF;
    cmdPacket[14] = (seqNum >> 16) & 0xFF;
    cmdPacket[15] = (seqNum >> 24) & 0xFF;

    cmdPacket[16] = 0; cmdPacket[17] = 0; cmdPacket[18] = 0; cmdPacket[19] = 0;
    cmdPacket[20] = 0; cmdPacket[21] = 0; cmdPacket[22] = 0; cmdPacket[23] = 0;

    // Copy actual data
    memcpy(cmdPacket + 24, data, dataLen);

    slipSend(cmdPacket, packetLen);
    free(cmdPacket);

    // Read response
    uint8_t respBuf[64];
    uint32_t respLen = 0;
    if (slipReceive(respBuf, sizeof(respBuf), &respLen, 10000)) {
        if (respLen >= 10 && respBuf[0] == 0x01 && respBuf[1] == ESP_CMD_FLASH_DATA) {
            if (respBuf[8] == 0x00) return true;
        }
    }

    return false;
}

bool s3BootloaderEndFlash(bool reboot) {
    // FLASH_END command: opcode 0x04
    // Data: [flag(4)] — 0=reboot, 1=don't reboot

    uint8_t cmdPacket[12];
    cmdPacket[0] = 0x00;
    cmdPacket[1] = ESP_CMD_FLASH_END;
    cmdPacket[2] = 4;    // Data length
    cmdPacket[3] = 0;
    cmdPacket[4] = 0;    // No checksum
    cmdPacket[5] = 0;
    cmdPacket[6] = 0;
    cmdPacket[7] = 0;

    uint32_t flag = reboot ? 0 : 1;  // 0 = reboot after flash
    cmdPacket[8]  = (flag >> 0) & 0xFF;
    cmdPacket[9]  = (flag >> 8) & 0xFF;
    cmdPacket[10] = (flag >> 16) & 0xFF;
    cmdPacket[11] = (flag >> 24) & 0xFF;

    slipSend(cmdPacket, 12);

    // If rebooting, we may not get a response
    uint8_t respBuf[64];
    uint32_t respLen = 0;
    if (slipReceive(respBuf, sizeof(respBuf), &respLen, 2000)) {
        if (respLen >= 10 && respBuf[0] == 0x01 && respBuf[1] == ESP_CMD_FLASH_END) {
            return true;
        }
    }

    // It's OK if we don't get a response — S3 may have already rebooted
    return true;
}

void slipSend(const uint8_t* data, uint32_t len) {
    // SLIP framing: 0xC0 [escaped data] 0xC0

    Serial1.write(SLIP_END);

    for (uint32_t i = 0; i < len; i++) {
        if (data[i] == SLIP_END) {
            Serial1.write(SLIP_ESC);
            Serial1.write(SLIP_ESC_END);
        } else if (data[i] == SLIP_ESC) {
            Serial1.write(SLIP_ESC);
            Serial1.write(SLIP_ESC_ESC);
        } else {
            Serial1.write(data[i]);
        }
    }

    Serial1.write(SLIP_END);
    Serial1.flush();
}

bool slipReceive(uint8_t* buf, uint32_t maxLen, uint32_t* receivedLen,
                  uint32_t timeoutMs) {
    // Read a SLIP frame from Serial1
    *receivedLen = 0;
    unsigned long startMs = millis();
    bool inFrame = false;
    bool escaped = false;
    uint32_t idx = 0;

    while ((millis() - startMs) < timeoutMs) {
        if (Serial1.available()) {
            uint8_t c = Serial1.read();

            if (!inFrame) {
                if (c == SLIP_END) {
                    inFrame = true;
                    idx = 0;
                }
                continue;
            }

            if (escaped) {
                escaped = false;
                if (c == SLIP_ESC_END) {
                    if (idx < maxLen) buf[idx++] = SLIP_END;
                } else if (c == SLIP_ESC_ESC) {
                    if (idx < maxLen) buf[idx++] = SLIP_ESC;
                } else {
                    if (idx < maxLen) buf[idx++] = c;  // Malformed, but accept
                }
                continue;
            }

            if (c == SLIP_END) {
                // End of frame
                if (idx > 0) {
                    *receivedLen = idx;
                    return true;
                }
                // Empty frame — ignore and continue waiting
                continue;
            }

            if (c == SLIP_ESC) {
                escaped = true;
                continue;
            }

            if (idx < maxLen) {
                buf[idx++] = c;
            }
        }

        yield();
    }

    return false;  // Timeout
}


// ============================================================================
// ============================================================================
//                       JSON PARSING UTILITIES (No ArduinoJson)
// ============================================================================
// ============================================================================
//
// Simple JSON value extractors that search for "key": and parse the value.
// These handle the simple flat JSON objects returned by Firebase.
// They DO NOT handle nested objects, arrays, or escaped strings.
//

void extractJsonStringValue(const String& json, const char* key,
                             char* outBuf, int outBufSize) {
    outBuf[0] = '\0';

    // Search for "key":"value"
    String searchKey = String("\"") + key + "\":\"";
    int keyPos = json.indexOf(searchKey);
    if (keyPos < 0) return;

    int valStart = keyPos + searchKey.length();
    int valEnd = json.indexOf('"', valStart);
    if (valEnd < 0) return;

    String value = json.substring(valStart, valEnd);
    strncpy(outBuf, value.c_str(), outBufSize - 1);
    outBuf[outBufSize - 1] = '\0';
}

int extractJsonIntValue(const String& json, const char* key, int defaultVal) {
    // Search for "key":123 or "key": 123
    String searchKey = String("\"") + key + "\":";
    int keyPos = json.indexOf(searchKey);
    if (keyPos < 0) return defaultVal;

    int valStart = keyPos + searchKey.length();

    // Skip whitespace
    while (valStart < (int)json.length() &&
           (json[valStart] == ' ' || json[valStart] == '\t')) {
        valStart++;
    }

    // Check if it's a string-quoted number "key":"123"
    if (valStart < (int)json.length() && json[valStart] == '"') {
        valStart++;
        int valEnd = json.indexOf('"', valStart);
        if (valEnd < 0) return defaultVal;
        return json.substring(valStart, valEnd).toInt();
    }

    // Extract numeric value
    String numStr = "";
    while (valStart < (int)json.length()) {
        char c = json[valStart];
        if (c == ',' || c == '}' || c == ' ' || c == '\n' || c == '\r') break;
        numStr += c;
        valStart++;
    }

    if (numStr.length() == 0) return defaultVal;
    return numStr.toInt();
}

float extractJsonFloatValue(const String& json, const char* key, float defaultVal) {
    String searchKey = String("\"") + key + "\":";
    int keyPos = json.indexOf(searchKey);
    if (keyPos < 0) return defaultVal;

    int valStart = keyPos + searchKey.length();

    while (valStart < (int)json.length() &&
           (json[valStart] == ' ' || json[valStart] == '\t')) {
        valStart++;
    }

    if (valStart < (int)json.length() && json[valStart] == '"') {
        valStart++;
        int valEnd = json.indexOf('"', valStart);
        if (valEnd < 0) return defaultVal;
        return json.substring(valStart, valEnd).toFloat();
    }

    String numStr = "";
    while (valStart < (int)json.length()) {
        char c = json[valStart];
        if (c == ',' || c == '}' || c == ' ' || c == '\n' || c == '\r') break;
        numStr += c;
        valStart++;
    }

    if (numStr.length() == 0) return defaultVal;
    return numStr.toFloat();
}

bool extractJsonBoolValue(const String& json, const char* key, bool defaultVal) {
    String searchKey = String("\"") + key + "\":";
    int keyPos = json.indexOf(searchKey);
    if (keyPos < 0) return defaultVal;

    int valStart = keyPos + searchKey.length();

    while (valStart < (int)json.length() &&
           (json[valStart] == ' ' || json[valStart] == '\t')) {
        valStart++;
    }

    if (json.substring(valStart, valStart + 4) == "true") return true;
    if (json.substring(valStart, valStart + 5) == "false") return false;

    // Also handle "true" / "false" as strings
    if (valStart < (int)json.length() && json[valStart] == '"') {
        valStart++;
        if (json.substring(valStart, valStart + 4) == "true") return true;
        if (json.substring(valStart, valStart + 5) == "false") return false;
    }

    return defaultVal;
}


// ============================================================================
// ============================================================================
//                       LOGGING UTILITY
// ============================================================================
// ============================================================================

void logMsg(const char* level, const char* msg) {
    unsigned long t = millis() / 1000;
    Serial.printf("[%lu][%s] %s\n", t, level, msg);
}
