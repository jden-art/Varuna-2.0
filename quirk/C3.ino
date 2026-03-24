// ============================================================================
// VARUNA BUOY — ESP32-C3 COMMUNICATION BRIDGE FIRMWARE
// ============================================================================
// ZERO external libraries — uses only ESP32 Arduino core
// All JSON built manually with snprintf
// ============================================================================

#include <WiFi.h>
#include <HTTPClient.h>
#include <WiFiClientSecure.h>
#include <SPI.h>
#include <SD.h>

// ============================================================================
// PIN DEFINITIONS
// ============================================================================

#define S3_DATA_RX_PIN        3
#define S3_DATA_BAUD          9600

#define S3_CMD_TX_PIN         0
#define S3_CMD_RX_PIN         1
#define S3_CMD_BAUD           9600

#define SD_CS_PIN             4
#define SD_SCK_PIN            5
#define SD_MOSI_PIN           6
#define SD_MISO_PIN           7

#define S3_RESET_PIN          10
#define S3_BOOT_PIN           8

// ============================================================================
// FIREBASE CONFIGURATION
// ============================================================================

#define FIREBASE_HOST         "varuna-buoy-default-rtdb.firebaseio.com"
#define FIREBASE_AUTH         "YOUR_FIREBASE_DATABASE_SECRET"
#define FIREBASE_PROJECT_ID   "varuna-buoy"
#define FIREBASE_STORAGE_BUCKET "varuna-buoy.appspot.com"

// Firebase paths (format with device ID using snprintf)
#define DEVICE_ID             "VARUNA_001"

// ============================================================================
// WIFI CONFIGURATION
// ============================================================================

struct WiFiCredential {
    const char* ssid;
    const char* password;
};

WiFiCredential wifiCredentials[] = {
    {"VARUNA_FIELD_AP",     "varuna_secure_2024"},
    {"VARUNA_BACKUP_AP",    "varuna_backup_2024"},
    {"VARUNA_MOBILE_AP",    "varuna_mobile_2024"},
};
#define NUM_WIFI_CREDENTIALS  3

// ============================================================================
// TIMING CONSTANTS
// ============================================================================

#define WIFI_CONNECT_TIMEOUT_MS       15000
#define WIFI_RECONNECT_INTERVAL_MS    30000
#define FIREBASE_POLL_INTERVAL_MS     10000
#define FIREBASE_PUSH_TIMEOUT_MS      10000
#define SD_FLUSH_BATCH_SIZE           20
#define SD_FLUSH_INTERVAL_MS          5000
#define HEARTBEAT_INTERVAL_MS         60000
#define S3_RESET_PULSE_MS             100
#define S3_BOOT_WAIT_MS               500
#define SOFTWARE_UART_BIT_TIME_US     104

// ============================================================================
// SD CARD FILE PATHS
// ============================================================================

#define SD_DATA_DIR           "/data"
#define SD_BUFFER_FILE        "/data/buffer.csv"
#define SD_OTA_DIR            "/ota"
#define SD_OTA_FILE           "/ota/s3_firmware.bin"

// ============================================================================
// SOFTWARE UART RECEIVER
// ============================================================================

#define SOFT_RX_BUFFER_SIZE   1200

volatile char softRxBuffer[SOFT_RX_BUFFER_SIZE];
volatile uint16_t softRxHead = 0;
volatile uint16_t softRxTail = 0;
volatile bool softRxBusy = false;
volatile uint8_t softRxBitCount = 0;
volatile uint8_t softRxByte = 0;

hw_timer_t* softUartTimer = NULL;

// ============================================================================
// GLOBAL STATE STRUCTURES
// ============================================================================

struct WiFiState {
    bool connected;
    bool wasConnected;
    unsigned long lastReconnectAttempt;
    int currentCredentialIdx;
    int rssi;
    char ipAddress[16];
} wifi_state;

struct FirebaseState {
    bool authenticated;
    unsigned long lastPollMs;
    unsigned long lastPushMs;
    unsigned long lastHeartbeatMs;
    int consecutiveFailures;
    bool pushInProgress;
} fb_state;

struct SDCardState {
    bool available;
    bool bufferHasData;
    uint32_t bufferedRecordCount;
    uint32_t totalRecordsWritten;
    uint32_t totalRecordsFlushed;
    bool flushInProgress;
    unsigned long lastFlushBatchMs;
} sd_state;

// ============================================================================
// SENSOR DATA — parsed from S3 CSV (39 fields)
// ============================================================================

struct SensorData {
    float theta;                    // 1
    float waterHeight;              // 2
    float correctedTiltX;           // 3
    float correctedTiltY;           // 4
    float olpLength;                // 5
    float horizontalDist;           // 6
    float currentPressure;          // 7
    float currentTemperature;       // 8
    float baselinePressure;         // 9
    float pressureDeviation;        // 10
    int   submersionState;          // 11
    float estimatedDepth;           // 12
    int   bmpAvailable;             // 13
    unsigned long unixTime;         // 14
    char  dateTimeString[24];       // 15
    int   rtcValid;                 // 16
    float ratePer15Min;             // 17
    int   floodAlertLevel;          // 18
    unsigned long sessionDuration;  // 19
    float peakHeight;               // 20
    float minHeight;                // 21
    float latitude;                 // 22
    float longitude;                // 23
    float altitude;                 // 24
    int   gpsSatellites;            // 25
    int   gpsFixValid;              // 26
    int   simSignalRSSI;            // 27
    int   simRegistered;            // 28
    int   simAvailable;             // 29
    int   currentZone;              // 30
    int   currentResponseLevel;     // 31
    int   sustainedRise;            // 32
    float batteryPercent;           // 33
    unsigned long sampleInterval;   // 34
    unsigned long transmitInterval; // 35
    int   obLightEnabled;           // 36
    int   algorithmEnabled;         // 37
    int   currentMode;              // 38
    int   healthScore;              // 39

    bool  valid;
    unsigned long receivedMs;
    char  rawCSV[1024];
} sensorData;

// ============================================================================
// CONFIG STATE
// ============================================================================

struct ConfigState {
    uint32_t normalRateSec;
    uint32_t highRateSec;
    float    hMaxCm;
    bool     realtimeMode;
    bool     pendingConfigChange;
    bool     configSentToS3;

    uint32_t s3NormalRateSec;
    uint32_t s3HighRateSec;
    float    s3HMaxCm;
} config;

// ============================================================================
// OTA STATE
// ============================================================================

enum OTAPhase {
    OTA_IDLE = 0,
    OTA_DOWNLOAD_PENDING,
    OTA_DOWNLOADING,
    OTA_DOWNLOAD_COMPLETE,
    OTA_PROGRAMMING_S3,
    OTA_VERIFY,
    OTA_COMPLETE,
    OTA_FAILED
};

struct OTAState {
    OTAPhase phase;
    char firmwareURL[256];
    char firmwareVersion[32];
    uint32_t firmwareSize;
    uint32_t downloadedBytes;
    uint32_t writtenBytes;
    bool s3WasActive;
    int  retryCount;
    char errorMsg[128];
    unsigned long phaseStartMs;
} ota_state;

// ============================================================================
// S3 COMMUNICATION STATE
// ============================================================================

struct S3CommState {
    bool s3Active;
    bool pongReceived;
    unsigned long lastDataReceived;
    unsigned long lastPingSent;
} s3_comm;

// ============================================================================
// BUFFERS
// ============================================================================

static char csvLineBuf[1200];
static uint16_t csvLineIdx = 0;

static char s3CmdBuf[256];
static uint8_t s3CmdIdx = 0;

// Reusable JSON buffer — large enough for sensor data JSON
static char jsonBuffer[3072];

// Reusable URL buffer
static char urlBuffer[512];

// ============================================================================
// FORWARD DECLARATIONS
// ============================================================================

void initPins();
void initSDCard();
void initWiFi();
void initFirebase();
void initSoftwareUARTReceiver();
void initConfig();
void initOTA();

void wifiConnectLoop();
bool wifiTryConnect(const char* ssid, const char* password);
void wifiCheckConnection();

void buildFirebaseURL(char* buf, size_t bufSize, const char* path);
bool firebaseHTTPPut(const char* path, const char* json);
bool firebaseHTTPPatch(const char* path, const char* json);
bool firebaseHTTPGet(const char* path, char* responseBuffer, size_t responseSize);
bool firebaseHTTPPost(const char* path, const char* json);

void buildSensorJSON(char* buf, size_t bufSize, const SensorData& data);
void firebasePushSensorData(const SensorData& data);
void firebasePushLatest(const SensorData& data);
void firebasePollConfig();
void firebasePollOTA();
void firebasePollRealtimeMode();
void firebaseUpdateOTAStatus(const char* status, int progress);
void firebaseUpdateDeviceStatus();
void firebasePushDiagnostic(const char* diagFrame);
void firebaseUpdateSDFlushStatus(uint32_t flushed, uint32_t remaining);

void sdWriteBufferRecord(const char* csvLine);
void sdFlushToFirebase();
void sdCreateDirectories();
bool sdSaveOTABinary(uint8_t* data, size_t len, bool append);
void sdDeleteOTABinary();
size_t sdGetOTABinarySize();

void IRAM_ATTR softUartStartBitISR();
void IRAM_ATTR softUartBitSampleISR();
void softUartProcessReceived();

bool parseCSVLine(const char* csv, SensorData& data);
int csvSplitFields(const char* csv, char fields[][32], int maxFields);

void sendConfigToS3(uint32_t normalSec, uint32_t highSec, float hMaxCm);
void sendPingToS3();
void sendDiagRunToS3();
void processS3Commands();
void handleS3Response(const char* response);

void startOTADownload();
void startS3OTA();
void s3EnterBootloader();
void s3ExitBootloader();
bool s3BootloaderSync();
void s3HardReset();
void slipSendByte(uint8_t b);
void slipSendFrame(uint8_t* data, size_t len);
int slipReceiveFrame(uint8_t* buf, size_t maxLen, unsigned long timeoutMs);
void espSendCommand(uint8_t op, uint8_t* data, uint16_t dataLen, uint32_t checksum);
bool espReceiveResponse(uint8_t expectedOp, unsigned long timeoutMs);
bool s3BootloaderFlashBegin(uint32_t totalSize, uint32_t blockSize, uint32_t blockCount, uint32_t offset);
bool s3BootloaderFlashData(uint8_t* data, size_t dataLen, uint32_t seqNum);
bool s3BootloaderFlashEnd(bool reboot);

void handleNewSensorData();
void handleRealtimeData();
void handleBufferedData();

void processDataPipeline();
void processConfigPipeline();
void processOTAPipeline();
void processHeartbeat();

void logMessage(const char* level, const char* msg);

// ============================================================================
// MANUAL JSON PARSING HELPERS (replacing ArduinoJson)
// ============================================================================

// Extract a string value from JSON for a given key
// Input:  json = {"key1":"val1","key2":123,"key3":true}
// Returns: true if found, copies value to outVal
bool jsonExtractString(const char* json, const char* key, char* outVal, size_t outSize) {
    // Build search pattern: "key":"
    char pattern[64];
    snprintf(pattern, sizeof(pattern), "\"%s\":\"", key);

    const char* pos = strstr(json, pattern);
    if (!pos) return false;

    pos += strlen(pattern);

    size_t i = 0;
    while (pos[i] != '"' && pos[i] != '\0' && i < outSize - 1) {
        outVal[i] = pos[i];
        i++;
    }
    outVal[i] = '\0';
    return true;
}

// Extract a numeric value (int or float) from JSON as a long
bool jsonExtractLong(const char* json, const char* key, long* outVal) {
    // Try "key": pattern (number without quotes)
    char pattern[64];
    snprintf(pattern, sizeof(pattern), "\"%s\":", key);

    const char* pos = strstr(json, pattern);
    if (!pos) return false;

    pos += strlen(pattern);

    // Skip whitespace
    while (*pos == ' ' || *pos == '\t') pos++;

    // Handle quoted numbers too
    if (*pos == '"') pos++;

    char numBuf[32];
    size_t i = 0;
    while ((pos[i] >= '0' && pos[i] <= '9') || pos[i] == '-' || pos[i] == '.') {
        if (i < sizeof(numBuf) - 1) numBuf[i] = pos[i];
        i++;
    }
    numBuf[i < sizeof(numBuf) ? i : sizeof(numBuf) - 1] = '\0';

    if (i == 0) return false;

    *outVal = atol(numBuf);
    return true;
}

// Extract a float value from JSON
bool jsonExtractFloat(const char* json, const char* key, float* outVal) {
    char pattern[64];
    snprintf(pattern, sizeof(pattern), "\"%s\":", key);

    const char* pos = strstr(json, pattern);
    if (!pos) return false;

    pos += strlen(pattern);
    while (*pos == ' ' || *pos == '\t') pos++;
    if (*pos == '"') pos++;

    char numBuf[32];
    size_t i = 0;
    while ((pos[i] >= '0' && pos[i] <= '9') || pos[i] == '-' || pos[i] == '.') {
        if (i < sizeof(numBuf) - 1) numBuf[i] = pos[i];
        i++;
    }
    numBuf[i < sizeof(numBuf) ? i : sizeof(numBuf) - 1] = '\0';

    if (i == 0) return false;

    *outVal = atof(numBuf);
    return true;
}

// Extract a boolean value from JSON
bool jsonExtractBool(const char* json, const char* key, bool* outVal) {
    char pattern[64];
    snprintf(pattern, sizeof(pattern), "\"%s\":", key);

    const char* pos = strstr(json, pattern);
    if (!pos) return false;

    pos += strlen(pattern);
    while (*pos == ' ' || *pos == '\t') pos++;

    if (strncmp(pos, "true", 4) == 0) {
        *outVal = true;
        return true;
    } else if (strncmp(pos, "false", 5) == 0) {
        *outVal = false;
        return true;
    }

    return false;
}

// Check if JSON response is "null" or empty
bool jsonIsNull(const char* json) {
    if (!json) return true;
    if (json[0] == '\0') return true;
    if (strcmp(json, "null") == 0) return true;
    if (strcmp(json, "\"null\"") == 0) return true;
    return false;
}

// ============================================================================
// IMPLEMENTATION — INITIALIZATION
// ============================================================================

void initPins() {
    pinMode(S3_RESET_PIN, OUTPUT);
    digitalWrite(S3_RESET_PIN, HIGH);

    pinMode(S3_BOOT_PIN, OUTPUT);
    digitalWrite(S3_BOOT_PIN, HIGH);

    pinMode(S3_DATA_RX_PIN, INPUT);

    logMessage("INFO", "Control pins initialized");
}

void initSDCard() {
    sd_state.available = false;
    sd_state.bufferHasData = false;
    sd_state.bufferedRecordCount = 0;
    sd_state.totalRecordsWritten = 0;
    sd_state.totalRecordsFlushed = 0;
    sd_state.flushInProgress = false;
    sd_state.lastFlushBatchMs = 0;

    SPI.begin(SD_SCK_PIN, SD_MISO_PIN, SD_MOSI_PIN, SD_CS_PIN);

    if (SD.begin(SD_CS_PIN, SPI)) {
        sd_state.available = true;

        uint64_t cardSize = SD.cardSize() / (1024 * 1024);
        char msg[80];
        snprintf(msg, sizeof(msg), "SD card initialized — %llu MB", cardSize);
        logMessage("INFO", msg);

        sdCreateDirectories();

        // Check for existing buffered data
        if (SD.exists(SD_BUFFER_FILE)) {
            File f = SD.open(SD_BUFFER_FILE, FILE_READ);
            if (f) {
                uint32_t count = 0;
                while (f.available()) {
                    String line = f.readStringUntil('\n');
                    if (line.length() > 10) count++;
                }
                f.close();

                if (count > 0) {
                    sd_state.bufferHasData = true;
                    sd_state.bufferedRecordCount = count;
                    char msg2[80];
                    snprintf(msg2, sizeof(msg2), "Found %lu buffered records", (unsigned long)count);
                    logMessage("INFO", msg2);
                }
            }
        }
    } else {
        logMessage("ERROR", "SD card initialization FAILED");
    }
}

void sdCreateDirectories() {
    if (!sd_state.available) return;
    if (!SD.exists(SD_DATA_DIR)) SD.mkdir(SD_DATA_DIR);
    if (!SD.exists(SD_OTA_DIR)) SD.mkdir(SD_OTA_DIR);
}

void initWiFi() {
    wifi_state.connected = false;
    wifi_state.wasConnected = false;
    wifi_state.lastReconnectAttempt = 0;
    wifi_state.currentCredentialIdx = 0;
    wifi_state.rssi = 0;
    memset(wifi_state.ipAddress, 0, sizeof(wifi_state.ipAddress));

    WiFi.mode(WIFI_STA);
    WiFi.setAutoReconnect(true);
    logMessage("INFO", "WiFi initialized in STA mode");
}

void initFirebase() {
    fb_state.authenticated = false;
    fb_state.lastPollMs = 0;
    fb_state.lastPushMs = 0;
    fb_state.lastHeartbeatMs = 0;
    fb_state.consecutiveFailures = 0;
    fb_state.pushInProgress = false;
    logMessage("INFO", "Firebase state initialized");
}

void initConfig() {
    config.normalRateSec = 900;
    config.highRateSec = 60;
    config.hMaxCm = 200.0f;
    config.realtimeMode = false;
    config.pendingConfigChange = false;
    config.configSentToS3 = false;
    config.s3NormalRateSec = 900;
    config.s3HighRateSec = 60;
    config.s3HMaxCm = 200.0f;
    logMessage("INFO", "Config initialized — normal=900s, high=60s, L=200cm");
}

void initOTA() {
    ota_state.phase = OTA_IDLE;
    memset(ota_state.firmwareURL, 0, sizeof(ota_state.firmwareURL));
    memset(ota_state.firmwareVersion, 0, sizeof(ota_state.firmwareVersion));
    ota_state.firmwareSize = 0;
    ota_state.downloadedBytes = 0;
    ota_state.writtenBytes = 0;
    ota_state.s3WasActive = true;
    ota_state.retryCount = 0;
    memset(ota_state.errorMsg, 0, sizeof(ota_state.errorMsg));
    ota_state.phaseStartMs = 0;
    logMessage("INFO", "OTA state initialized");
}

// ============================================================================
// SOFTWARE UART RECEIVER (GPIO 3, 9600 baud)
// ============================================================================

void initSoftwareUARTReceiver() {
    softRxHead = 0;
    softRxTail = 0;
    softRxBusy = false;

    attachInterrupt(digitalPinToInterrupt(S3_DATA_RX_PIN), softUartStartBitISR, FALLING);

    softUartTimer = timerBegin(0, 80, true);  // 1µs per tick
    timerAttachInterrupt(softUartTimer, &softUartBitSampleISR, true);

    logMessage("INFO", "Software UART receiver on GPIO 3 initialized");
}

void IRAM_ATTR softUartStartBitISR() {
    if (softRxBusy) return;

    softRxBusy = true;
    softRxBitCount = 0;
    softRxByte = 0;

    detachInterrupt(digitalPinToInterrupt(S3_DATA_RX_PIN));

    timerAlarmWrite(softUartTimer, 156, true);  // 1.5 bit periods
    timerAlarmEnable(softUartTimer);
    timerRestart(softUartTimer);
}

void IRAM_ATTR softUartBitSampleISR() {
    if (softRxBitCount < 8) {
        int bitVal = digitalRead(S3_DATA_RX_PIN);
        if (bitVal) {
            softRxByte |= (1 << softRxBitCount);
        }
        softRxBitCount++;

        if (softRxBitCount == 1) {
            timerAlarmWrite(softUartTimer, SOFTWARE_UART_BIT_TIME_US, true);
        }
    } else {
        timerAlarmDisable(softUartTimer);

        uint16_t nextHead = (softRxHead + 1) % SOFT_RX_BUFFER_SIZE;
        if (nextHead != softRxTail) {
            softRxBuffer[softRxHead] = (char)softRxByte;
            softRxHead = nextHead;
        }

        softRxBusy = false;
        attachInterrupt(digitalPinToInterrupt(S3_DATA_RX_PIN), softUartStartBitISR, FALLING);
    }
}

void softUartProcessReceived() {
    while (softRxTail != softRxHead) {
        char c = softRxBuffer[softRxTail];
        softRxTail = (softRxTail + 1) % SOFT_RX_BUFFER_SIZE;

        if (c == '\n' || c == '\r') {
            if (csvLineIdx > 10) {
                csvLineBuf[csvLineIdx] = '\0';

                if (parseCSVLine(csvLineBuf, sensorData)) {
                    sensorData.valid = true;
                    sensorData.receivedMs = millis();
                    strncpy(sensorData.rawCSV, csvLineBuf, sizeof(sensorData.rawCSV) - 1);
                    sensorData.rawCSV[sizeof(sensorData.rawCSV) - 1] = '\0';

                    s3_comm.lastDataReceived = millis();
                    s3_comm.s3Active = true;

                    handleNewSensorData();
                }
            }
            csvLineIdx = 0;
        } else if (csvLineIdx < sizeof(csvLineBuf) - 1) {
            csvLineBuf[csvLineIdx++] = c;
        }
    }
}

// ============================================================================
// CSV PARSER (39 Fields) — No library needed
// ============================================================================

int csvSplitFields(const char* csv, char fields[][32], int maxFields) {
    int fieldIdx = 0;
    int charIdx = 0;

    for (int i = 0; csv[i] != '\0' && fieldIdx < maxFields; i++) {
        if (csv[i] == ',') {
            fields[fieldIdx][charIdx] = '\0';
            fieldIdx++;
            charIdx = 0;
        } else if (charIdx < 31) {
            fields[fieldIdx][charIdx++] = csv[i];
        }
    }
    if (fieldIdx < maxFields) {
        fields[fieldIdx][charIdx] = '\0';
        fieldIdx++;
    }
    return fieldIdx;
}

bool parseCSVLine(const char* csv, SensorData& data) {
    char fields[40][32];
    memset(fields, 0, sizeof(fields));

    int numFields = csvSplitFields(csv, fields, 40);

    if (numFields < 39) {
        char msg[64];
        snprintf(msg, sizeof(msg), "CSV parse: only %d fields (need 39)", numFields);
        logMessage("WARN", msg);
        return false;
    }

    data.theta              = atof(fields[0]);
    data.waterHeight        = atof(fields[1]);
    data.correctedTiltX     = atof(fields[2]);
    data.correctedTiltY     = atof(fields[3]);
    data.olpLength          = atof(fields[4]);
    data.horizontalDist     = atof(fields[5]);
    data.currentPressure    = atof(fields[6]);
    data.currentTemperature = atof(fields[7]);
    data.baselinePressure   = atof(fields[8]);
    data.pressureDeviation  = atof(fields[9]);
    data.submersionState    = atoi(fields[10]);
    data.estimatedDepth     = atof(fields[11]);
    data.bmpAvailable       = atoi(fields[12]);
    data.unixTime           = strtoul(fields[13], NULL, 10);
    strncpy(data.dateTimeString, fields[14], sizeof(data.dateTimeString) - 1);
    data.dateTimeString[sizeof(data.dateTimeString) - 1] = '\0';
    data.rtcValid           = atoi(fields[15]);
    data.ratePer15Min       = atof(fields[16]);
    data.floodAlertLevel    = atoi(fields[17]);
    data.sessionDuration    = strtoul(fields[18], NULL, 10);
    data.peakHeight         = atof(fields[19]);
    data.minHeight          = atof(fields[20]);
    data.latitude           = atof(fields[21]);
    data.longitude          = atof(fields[22]);
    data.altitude           = atof(fields[23]);
    data.gpsSatellites      = atoi(fields[24]);
    data.gpsFixValid        = atoi(fields[25]);
    data.simSignalRSSI      = atoi(fields[26]);
    data.simRegistered      = atoi(fields[27]);
    data.simAvailable       = atoi(fields[28]);
    data.currentZone        = atoi(fields[29]);
    data.currentResponseLevel = atoi(fields[30]);
    data.sustainedRise      = atoi(fields[31]);
    data.batteryPercent     = atof(fields[32]);
    data.sampleInterval     = strtoul(fields[33], NULL, 10);
    data.transmitInterval   = strtoul(fields[34], NULL, 10);
    data.obLightEnabled     = atoi(fields[35]);
    data.algorithmEnabled   = atoi(fields[36]);
    data.currentMode        = atoi(fields[37]);
    data.healthScore        = atoi(fields[38]);

    // Populate C3-known fields
    data.simSignalRSSI = wifi_state.rssi;
    data.simRegistered = wifi_state.connected ? 1 : 0;
    data.simAvailable  = 1;

    return true;
}

// ============================================================================
// WIFI
// ============================================================================

void wifiConnectLoop() {
    if (wifi_state.connected) return;

    unsigned long now = millis();
    if (now - wifi_state.lastReconnectAttempt < WIFI_RECONNECT_INTERVAL_MS) return;
    wifi_state.lastReconnectAttempt = now;

    for (int i = 0; i < NUM_WIFI_CREDENTIALS; i++) {
        int idx = (wifi_state.currentCredentialIdx + i) % NUM_WIFI_CREDENTIALS;
        char msg[80];
        snprintf(msg, sizeof(msg), "Trying WiFi: %s", wifiCredentials[idx].ssid);
        logMessage("INFO", msg);

        if (wifiTryConnect(wifiCredentials[idx].ssid, wifiCredentials[idx].password)) {
            wifi_state.currentCredentialIdx = idx;
            return;
        }
    }
    logMessage("WARN", "All WiFi credentials failed");
}

bool wifiTryConnect(const char* ssid, const char* password) {
    WiFi.disconnect(true);
    delay(100);
    WiFi.begin(ssid, password);

    unsigned long startAttempt = millis();
    while (WiFi.status() != WL_CONNECTED) {
        if (millis() - startAttempt >= WIFI_CONNECT_TIMEOUT_MS) return false;
        delay(500);
    }

    wifi_state.connected = true;
    wifi_state.rssi = WiFi.RSSI();
    IPAddress ip = WiFi.localIP();
    snprintf(wifi_state.ipAddress, sizeof(wifi_state.ipAddress),
             "%d.%d.%d.%d", ip[0], ip[1], ip[2], ip[3]);

    char msg[128];
    snprintf(msg, sizeof(msg), "WiFi connected: %s IP=%s RSSI=%d",
             ssid, wifi_state.ipAddress, wifi_state.rssi);
    logMessage("INFO", msg);

    fb_state.authenticated = true;
    return true;
}

void wifiCheckConnection() {
    bool currentlyConnected = (WiFi.status() == WL_CONNECTED);

    if (currentlyConnected) {
        wifi_state.rssi = WiFi.RSSI();
    }

    if (wifi_state.wasConnected && !currentlyConnected) {
        wifi_state.connected = false;
        logMessage("WARN", "WiFi connection LOST");
    }

    if (!wifi_state.wasConnected && currentlyConnected) {
        wifi_state.connected = true;
        logMessage("INFO", "WiFi connection RESTORED");

        if (sd_state.bufferHasData && sd_state.bufferedRecordCount > 0) {
            char msg[80];
            snprintf(msg, sizeof(msg), "WiFi back — %lu records buffered",
                     (unsigned long)sd_state.bufferedRecordCount);
            logMessage("INFO", msg);
        }
    }

    wifi_state.wasConnected = currentlyConnected;
    wifi_state.connected = currentlyConnected;
}

// ============================================================================
// FIREBASE HTTP OPERATIONS — Manual JSON, no libraries
// ============================================================================

void buildFirebaseURL(char* buf, size_t bufSize, const char* path) {
    snprintf(buf, bufSize, "https://%s%s.json?auth=%s",
             FIREBASE_HOST, path, FIREBASE_AUTH);
}

bool firebaseHTTPPut(const char* path, const char* json) {
    if (!wifi_state.connected) return false;

    char fullPath[256];
    snprintf(fullPath, sizeof(fullPath), "/devices/%s%s", DEVICE_ID, path);
    buildFirebaseURL(urlBuffer, sizeof(urlBuffer), fullPath);

    WiFiClientSecure client;
    client.setInsecure();

    HTTPClient http;
    http.begin(client, urlBuffer);
    http.addHeader("Content-Type", "application/json");
    http.setTimeout(FIREBASE_PUSH_TIMEOUT_MS);

    int httpCode = http.PUT(json);
    http.end();

    if (httpCode == 200) {
        fb_state.consecutiveFailures = 0;
        return true;
    } else {
        fb_state.consecutiveFailures++;
        char msg[80];
        snprintf(msg, sizeof(msg), "Firebase PUT failed: HTTP %d", httpCode);
        logMessage("ERROR", msg);
        return false;
    }
}

bool firebaseHTTPPatch(const char* path, const char* json) {
    if (!wifi_state.connected) return false;

    char fullPath[256];
    snprintf(fullPath, sizeof(fullPath), "/devices/%s%s", DEVICE_ID, path);
    buildFirebaseURL(urlBuffer, sizeof(urlBuffer), fullPath);

    WiFiClientSecure client;
    client.setInsecure();

    HTTPClient http;
    http.begin(client, urlBuffer);
    http.addHeader("Content-Type", "application/json");
    http.addHeader("X-HTTP-Method-Override", "PATCH");
    http.setTimeout(FIREBASE_PUSH_TIMEOUT_MS);

    int httpCode = http.PATCH(json);
    http.end();

    if (httpCode == 200) {
        fb_state.consecutiveFailures = 0;
        return true;
    } else {
        fb_state.consecutiveFailures++;
        char msg[80];
        snprintf(msg, sizeof(msg), "Firebase PATCH failed: HTTP %d", httpCode);
        logMessage("ERROR", msg);
        return false;
    }
}

bool firebaseHTTPPost(const char* path, const char* json) {
    if (!wifi_state.connected) return false;

    char fullPath[256];
    snprintf(fullPath, sizeof(fullPath), "/devices/%s%s", DEVICE_ID, path);
    buildFirebaseURL(urlBuffer, sizeof(urlBuffer), fullPath);

    WiFiClientSecure client;
    client.setInsecure();

    HTTPClient http;
    http.begin(client, urlBuffer);
    http.addHeader("Content-Type", "application/json");
    http.setTimeout(FIREBASE_PUSH_TIMEOUT_MS);

    int httpCode = http.POST(json);
    http.end();

    if (httpCode == 200) {
        fb_state.consecutiveFailures = 0;
        fb_state.lastPushMs = millis();
        return true;
    } else {
        fb_state.consecutiveFailures++;
        char msg[80];
        snprintf(msg, sizeof(msg), "Firebase POST failed: HTTP %d", httpCode);
        logMessage("ERROR", msg);
        return false;
    }
}

bool firebaseHTTPGet(const char* path, char* responseBuffer, size_t responseSize) {
    if (!wifi_state.connected) {
        responseBuffer[0] = '\0';
        return false;
    }

    char fullPath[256];
    snprintf(fullPath, sizeof(fullPath), "/devices/%s%s", DEVICE_ID, path);
    buildFirebaseURL(urlBuffer, sizeof(urlBuffer), fullPath);

    WiFiClientSecure client;
    client.setInsecure();

    HTTPClient http;
    http.begin(client, urlBuffer);
    http.setTimeout(FIREBASE_PUSH_TIMEOUT_MS);

    int httpCode = http.GET();

    if (httpCode == 200) {
        String payload = http.getString();
        strncpy(responseBuffer, payload.c_str(), responseSize - 1);
        responseBuffer[responseSize - 1] = '\0';
        fb_state.consecutiveFailures = 0;
        http.end();
        return true;
    } else {
        fb_state.consecutiveFailures++;
        responseBuffer[0] = '\0';
        http.end();
        return false;
    }
}

// ============================================================================
// JSON BUILDER — Manual string construction
// ============================================================================

void buildSensorJSON(char* buf, size_t bufSize, const SensorData& data) {
    snprintf(buf, bufSize,
        "{"
        "\"theta\":%.2f,"
        "\"waterHeight\":%.2f,"
        "\"correctedTiltX\":%.2f,"
        "\"correctedTiltY\":%.2f,"
        "\"olpLength\":%.2f,"
        "\"horizontalDist\":%.2f,"
        "\"currentPressure\":%.2f,"
        "\"currentTemperature\":%.2f,"
        "\"baselinePressure\":%.2f,"
        "\"pressureDeviation\":%.2f,"
        "\"submersionState\":%d,"
        "\"estimatedDepth\":%.2f,"
        "\"bmpAvailable\":%d,"
        "\"unixTime\":%lu,"
        "\"dateTimeString\":\"%s\","
        "\"rtcValid\":%d,"
        "\"ratePer15Min\":%.3f,"
        "\"floodAlertLevel\":%d,"
        "\"sessionDuration\":%lu,"
        "\"peakHeight\":%.2f,"
        "\"minHeight\":%.2f,"
        "\"latitude\":%.6f,"
        "\"longitude\":%.6f,"
        "\"altitude\":%.1f,"
        "\"gpsSatellites\":%d,"
        "\"gpsFixValid\":%d,"
        "\"simSignalRSSI\":%d,"
        "\"simRegistered\":%d,"
        "\"simAvailable\":%d,"
        "\"currentZone\":%d,"
        "\"currentResponseLevel\":%d,"
        "\"sustainedRise\":%d,"
        "\"batteryPercent\":%.1f,"
        "\"sampleInterval\":%lu,"
        "\"transmitInterval\":%lu,"
        "\"obLightEnabled\":%d,"
        "\"algorithmEnabled\":%d,"
        "\"currentMode\":%d,"
        "\"healthScore\":%d,"
        "\"deviceId\":\"%s\","
        "\"wifiRSSI\":%d,"
        "\"c3Timestamp\":%lu"
        "}",
        data.theta,
        data.waterHeight,
        data.correctedTiltX,
        data.correctedTiltY,
        data.olpLength,
        data.horizontalDist,
        data.currentPressure,
        data.currentTemperature,
        data.baselinePressure,
        data.pressureDeviation,
        data.submersionState,
        data.estimatedDepth,
        data.bmpAvailable,
        (unsigned long)data.unixTime,
        data.dateTimeString,
        data.rtcValid,
        data.ratePer15Min,
        data.floodAlertLevel,
        (unsigned long)data.sessionDuration,
        data.peakHeight,
        data.minHeight,
        data.latitude,
        data.longitude,
        data.altitude,
        data.gpsSatellites,
        data.gpsFixValid,
        data.simSignalRSSI,
        data.simRegistered,
        data.simAvailable,
        data.currentZone,
        data.currentResponseLevel,
        data.sustainedRise,
        data.batteryPercent,
        (unsigned long)data.sampleInterval,
        (unsigned long)data.transmitInterval,
        data.obLightEnabled,
        data.algorithmEnabled,
        data.currentMode,
        data.healthScore,
        DEVICE_ID,
        wifi_state.rssi,
        (unsigned long)millis()
    );
}

void buildBufferedSensorJSON(char* buf, size_t bufSize, const SensorData& data, unsigned long bufferTimestamp) {
    // Same as buildSensorJSON but with buffered flag
    int len = 0;

    // Build main JSON
    buildSensorJSON(buf, bufSize, data);

    // Find the closing brace and insert buffered fields before it
    len = strlen(buf);
    if (len > 1 && buf[len - 1] == '}') {
        buf[len - 1] = '\0';  // Remove closing brace
        snprintf(buf + len - 1, bufSize - len + 1,
                 ",\"buffered\":true,\"bufferTimestamp\":%lu}",
                 bufferTimestamp);
    }
}

// ============================================================================
// FIREBASE DATA PUSH
// ============================================================================

void firebasePushSensorData(const SensorData& data) {
    if (!wifi_state.connected || !fb_state.authenticated) return;

    fb_state.pushInProgress = true;

    buildSensorJSON(jsonBuffer, sizeof(jsonBuffer), data);

    // POST to sensorData (creates unique keys)
    firebaseHTTPPost("/sensorData", jsonBuffer);

    // PUT to latest (overwrites)
    firebasePushLatest(data);

    fb_state.pushInProgress = false;
}

void firebasePushLatest(const SensorData& data) {
    buildSensorJSON(jsonBuffer, sizeof(jsonBuffer), data);
    firebaseHTTPPut("/latest", jsonBuffer);
}

// ============================================================================
// FIREBASE CONFIG POLLING
// ============================================================================

void firebasePollConfig() {
    if (!wifi_state.connected || !fb_state.authenticated) return;

    unsigned long now = millis();
    if (now - fb_state.lastPollMs < FIREBASE_POLL_INTERVAL_MS) return;
    fb_state.lastPollMs = now;

    // Poll pending config
    char response[512];
    if (!firebaseHTTPGet("/config/pending", response, sizeof(response))) return;

    if (!jsonIsNull(response) && strlen(response) > 5) {
        long val;
        float fval;
        bool changed = false;

        if (jsonExtractLong(response, "normalRateSec", &val)) {
            if ((uint32_t)val != config.normalRateSec && val >= 10 && val <= 3600) {
                config.normalRateSec = (uint32_t)val;
                changed = true;
            }
        }

        if (jsonExtractLong(response, "highRateSec", &val)) {
            if ((uint32_t)val != config.highRateSec && val >= 5 && val <= 3600) {
                config.highRateSec = (uint32_t)val;
                changed = true;
            }
        }

        if (jsonExtractFloat(response, "hMaxCm", &fval)) {
            if (fval != config.hMaxCm && fval >= 10.0f && fval <= 10000.0f) {
                config.hMaxCm = fval;
                changed = true;
            }
        }

        if (changed) {
            config.pendingConfigChange = true;
            config.configSentToS3 = false;

            char msg[128];
            snprintf(msg, sizeof(msg), "New config: normal=%lus, high=%lus, hMax=%.1fcm",
                     (unsigned long)config.normalRateSec,
                     (unsigned long)config.highRateSec,
                     config.hMaxCm);
            logMessage("INFO", msg);

            // Clear pending
            firebaseHTTPPut("/config/pending", "null");
        }
    }

    // Poll real-time mode
    firebasePollRealtimeMode();
}

void firebasePollRealtimeMode() {
    char response[32];
    if (!firebaseHTTPGet("/config/realtimeMode", response, sizeof(response))) return;

    if (!jsonIsNull(response)) {
        bool newMode = (strstr(response, "true") != NULL);
        if (newMode != config.realtimeMode) {
            config.realtimeMode = newMode;
            char msg[64];
            snprintf(msg, sizeof(msg), "Real-time mode: %s",
                     config.realtimeMode ? "ENABLED" : "DISABLED");
            logMessage("INFO", msg);
        }
    }
}

// ============================================================================
// FIREBASE OTA POLLING
// ============================================================================

void firebasePollOTA() {
    if (!wifi_state.connected || !fb_state.authenticated) return;
    if (ota_state.phase != OTA_IDLE) return;

    char response[512];
    if (!firebaseHTTPGet("/ota", response, sizeof(response))) return;

    if (jsonIsNull(response) || strlen(response) < 10) return;

    bool trigger = false;
    if (!jsonExtractBool(response, "trigger", &trigger)) return;
    if (!trigger) return;

    // OTA triggered
    jsonExtractString(response, "url", ota_state.firmwareURL, sizeof(ota_state.firmwareURL));
    jsonExtractString(response, "version", ota_state.firmwareVersion, sizeof(ota_state.firmwareVersion));

    long size = 0;
    if (jsonExtractLong(response, "size", &size)) {
        ota_state.firmwareSize = (uint32_t)size;
    }

    char msg[256];
    snprintf(msg, sizeof(msg), "OTA triggered — version: %s, size: %lu",
             ota_state.firmwareVersion, (unsigned long)ota_state.firmwareSize);
    logMessage("INFO", msg);

    ota_state.phase = OTA_DOWNLOAD_PENDING;
    ota_state.phaseStartMs = millis();
    ota_state.downloadedBytes = 0;
    ota_state.writtenBytes = 0;
    ota_state.retryCount = 0;
    ota_state.s3WasActive = s3_comm.s3Active;

    // Clear trigger
    firebaseHTTPPatch("/ota", "{\"trigger\":false}");
    firebaseUpdateOTAStatus("DOWNLOAD_PENDING", 0);
}

void firebaseUpdateOTAStatus(const char* status, int progress) {
    char json[256];
    snprintf(json, sizeof(json),
             "{\"status\":\"%s\",\"progress\":%d,\"phase\":%d,\"version\":\"%s\",\"timestamp\":%lu}",
             status, progress, (int)ota_state.phase,
             ota_state.firmwareVersion, (unsigned long)millis());

    firebaseHTTPPatch("/ota/status", json);
}

void firebasePushDiagnostic(const char* diagFrame) {
    if (!wifi_state.connected) return;

    int vals[12];
    int parsed = sscanf(diagFrame, "$DIAG,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d",
                        &vals[0], &vals[1], &vals[2], &vals[3], &vals[4], &vals[5],
                        &vals[6], &vals[7], &vals[8], &vals[9], &vals[10], &vals[11]);

    if (parsed == 12) {
        snprintf(jsonBuffer, sizeof(jsonBuffer),
            "{\"mpuWhoAmI\":%d,\"mpuAccel\":%d,\"mpuGyro\":%d,"
            "\"bmpChipId\":%d,\"bmpPressure\":%d,\"bmpTemp\":%d,"
            "\"rtcOsc\":%d,\"rtcTime\":%d,\"gpsUart\":%d,"
            "\"battery\":%d,\"c3Pong\":%d,\"totalFaults\":%d,"
            "\"timestamp\":%lu,\"deviceId\":\"%s\"}",
            vals[0], vals[1], vals[2], vals[3], vals[4], vals[5],
            vals[6], vals[7], vals[8], vals[9], vals[10], vals[11],
            (unsigned long)millis(), DEVICE_ID);

        firebaseHTTPPost("/diagnostics", jsonBuffer);
        logMessage("INFO", "Diagnostic report pushed to Firebase");
    }
}

void firebaseUpdateDeviceStatus() {
    if (!wifi_state.connected) return;

    unsigned long now = millis();
    if (now - fb_state.lastHeartbeatMs < HEARTBEAT_INTERVAL_MS) return;
    fb_state.lastHeartbeatMs = now;

    snprintf(jsonBuffer, sizeof(jsonBuffer),
        "{\"online\":true,\"wifiRSSI\":%d,\"ip\":\"%s\","
        "\"s3Active\":%s,\"sdAvailable\":%s,"
        "\"sdBuffered\":%lu,\"otaPhase\":%d,"
        "\"uptime\":%lu,\"realtimeMode\":%s,"
        "\"configNormal\":%lu,\"configHigh\":%lu,"
        "\"configHMax\":%.1f,\"freeHeap\":%lu,"
        "\"timestamp\":%lu}",
        wifi_state.rssi,
        wifi_state.ipAddress,
        s3_comm.s3Active ? "true" : "false",
        sd_state.available ? "true" : "false",
        (unsigned long)sd_state.bufferedRecordCount,
        (int)ota_state.phase,
        (unsigned long)(millis() / 1000),
        config.realtimeMode ? "true" : "false",
        (unsigned long)config.normalRateSec,
        (unsigned long)config.highRateSec,
        config.hMaxCm,
        (unsigned long)ESP.getFreeHeap(),
        (unsigned long)millis());

    firebaseHTTPPut("/status", jsonBuffer);
}

void firebaseUpdateSDFlushStatus(uint32_t flushed, uint32_t remaining) {
    char json[128];
    snprintf(json, sizeof(json),
             "{\"totalFlushed\":%lu,\"remaining\":%lu,\"timestamp\":%lu}",
             (unsigned long)flushed, (unsigned long)remaining, (unsigned long)millis());

    firebaseHTTPPatch("/sdFlushStatus", json);
}

// ============================================================================
// SD CARD OPERATIONS
// ============================================================================

void sdWriteBufferRecord(const char* csvLine) {
    if (!sd_state.available) return;

    File f = SD.open(SD_BUFFER_FILE, FILE_APPEND);
    if (f) {
        unsigned long ts = millis();
        f.print(ts);
        f.print("|");
        f.println(csvLine);
        f.close();

        sd_state.bufferedRecordCount++;
        sd_state.totalRecordsWritten++;
        sd_state.bufferHasData = true;

        if (sd_state.bufferedRecordCount % 100 == 0) {
            char msg[64];
            snprintf(msg, sizeof(msg), "SD buffer: %lu records",
                     (unsigned long)sd_state.bufferedRecordCount);
            logMessage("INFO", msg);
        }
    } else {
        logMessage("ERROR", "Failed to open SD buffer file");
    }
}

void sdFlushToFirebase() {
    if (!sd_state.available || !sd_state.bufferHasData) return;
    if (!wifi_state.connected || !fb_state.authenticated) return;
    if (config.realtimeMode) return;
    if (ota_state.phase != OTA_IDLE) return;

    unsigned long now = millis();
    if (now - sd_state.lastFlushBatchMs < SD_FLUSH_INTERVAL_MS) return;
    sd_state.lastFlushBatchMs = now;

    File readFile = SD.open(SD_BUFFER_FILE, FILE_READ);
    if (!readFile) {
        logMessage("ERROR", "Cannot open SD buffer for flush");
        return;
    }

    String remainingLines = "";
    int flushedThisBatch = 0;
    bool errorOccurred = false;

    while (readFile.available() && !errorOccurred) {
        String line = readFile.readStringUntil('\n');
        line.trim();
        if (line.length() < 10) continue;

        if (flushedThisBatch < SD_FLUSH_BATCH_SIZE) {
            int separatorIdx = line.indexOf('|');
            if (separatorIdx > 0) {
                String csvPart = line.substring(separatorIdx + 1);
                unsigned long bufTs = line.substring(0, separatorIdx).toInt();

                SensorData bufferedData;
                memset(&bufferedData, 0, sizeof(bufferedData));

                if (parseCSVLine(csvPart.c_str(), bufferedData)) {
                    bufferedData.valid = true;

                    buildBufferedSensorJSON(jsonBuffer, sizeof(jsonBuffer), bufferedData, bufTs);

                    if (firebaseHTTPPost("/sensorData", jsonBuffer)) {
                        flushedThisBatch++;
                        sd_state.totalRecordsFlushed++;
                    } else {
                        errorOccurred = true;
                        remainingLines += line + "\n";
                    }
                } else {
                    flushedThisBatch++;  // Skip bad records
                }
            }
        } else {
            remainingLines += line + "\n";
        }
    }

    // Collect remaining unprocessed lines
    while (readFile.available()) {
        String line = readFile.readStringUntil('\n');
        line.trim();
        if (line.length() > 10) {
            remainingLines += line + "\n";
        }
    }

    readFile.close();

    // Rewrite buffer file
    if (remainingLines.length() > 10) {
        File writeFile = SD.open(SD_BUFFER_FILE, FILE_WRITE);
        if (writeFile) {
            writeFile.print(remainingLines);
            writeFile.close();
        }

        uint32_t remaining = 0;
        for (unsigned int i = 0; i < remainingLines.length(); i++) {
            if (remainingLines[i] == '\n') remaining++;
        }
        sd_state.bufferedRecordCount = remaining;
        sd_state.bufferHasData = (remaining > 0);
    } else {
        SD.remove(SD_BUFFER_FILE);
        sd_state.bufferedRecordCount = 0;
        sd_state.bufferHasData = false;
        logMessage("INFO", "SD buffer fully flushed");
    }

    if (flushedThisBatch > 0) {
        char msg[100];
        snprintf(msg, sizeof(msg), "SD flush: %d records pushed, %lu remaining",
                 flushedThisBatch, (unsigned long)sd_state.bufferedRecordCount);
        logMessage("INFO", msg);

        firebaseUpdateSDFlushStatus(sd_state.totalRecordsFlushed, sd_state.bufferedRecordCount);
    }
}

bool sdSaveOTABinary(uint8_t* data, size_t len, bool append) {
    if (!sd_state.available) return false;

    File f = SD.open(SD_OTA_FILE, append ? FILE_APPEND : FILE_WRITE);
    if (!f) {
        logMessage("ERROR", "Cannot open OTA file on SD");
        return false;
    }

    size_t written = f.write(data, len);
    f.close();
    return (written == len);
}

void sdDeleteOTABinary() {
    if (sd_state.available && SD.exists(SD_OTA_FILE)) {
        SD.remove(SD_OTA_FILE);
    }
}

size_t sdGetOTABinarySize() {
    if (!sd_state.available || !SD.exists(SD_OTA_FILE)) return 0;
    File f = SD.open(SD_OTA_FILE, FILE_READ);
    if (!f) return 0;
    size_t sz = f.size();
    f.close();
    return sz;
}

// ============================================================================
// S3 COMMUNICATION
// ============================================================================

void sendConfigToS3(uint32_t normalSec, uint32_t highSec, float hMaxCm) {
    char cmd[80];
    snprintf(cmd, sizeof(cmd), "$CFG,%lu,%lu,%.1f",
             (unsigned long)normalSec, (unsigned long)highSec, hMaxCm);

    Serial1.println(cmd);

    char msg[128];
    snprintf(msg, sizeof(msg), "Sent to S3: %s", cmd);
    logMessage("INFO", msg);

    config.configSentToS3 = true;
}

void sendPingToS3() {
    Serial1.println("$PING");
    s3_comm.lastPingSent = millis();
    s3_comm.pongReceived = false;
}

void sendDiagRunToS3() {
    Serial1.println("$DIAGRUN");
    logMessage("INFO", "Sent $DIAGRUN to S3");
}

void processS3Commands() {
    while (Serial1.available()) {
        char c = Serial1.read();
        if (c == '\n' || c == '\r') {
            if (s3CmdIdx > 0) {
                s3CmdBuf[s3CmdIdx] = '\0';
                handleS3Response(s3CmdBuf);
                s3CmdIdx = 0;
            }
        } else if (s3CmdIdx < sizeof(s3CmdBuf) - 1) {
            s3CmdBuf[s3CmdIdx++] = c;
        }
    }
}

void handleS3Response(const char* response) {
    if (strcmp(response, "$PONG") == 0) {
        s3_comm.pongReceived = true;
        logMessage("INFO", "S3 PONG received — link OK");
        return;
    }

    if (strcmp(response, "$CFG_ACK") == 0) {
        config.s3NormalRateSec = config.normalRateSec;
        config.s3HighRateSec = config.highRateSec;
        config.s3HMaxCm = config.hMaxCm;
        config.pendingConfigChange = false;
        logMessage("INFO", "S3 acknowledged config change");
        return;
    }

    if (strncmp(response, "$CFG_ERR", 8) == 0) {
        char msg[128];
        snprintf(msg, sizeof(msg), "S3 config error: %s", response);
        logMessage("ERROR", msg);
        return;
    }

    if (strncmp(response, "$DIAG,", 6) == 0) {
        logMessage("INFO", "Diagnostic frame received from S3");
        firebasePushDiagnostic(response);
        return;
    }

    if (strcmp(response, "$DIAGRUN_ACK") == 0) {
        logMessage("INFO", "S3 acknowledged diagnostic run");
        return;
    }

    char msg[256];
    snprintf(msg, sizeof(msg), "S3 response: %s", response);
    logMessage("DEBUG", msg);
}

// ============================================================================
// DATA FLOW HANDLER
// ============================================================================

void handleNewSensorData() {
    if (config.realtimeMode) {
        handleRealtimeData();
    } else {
        handleBufferedData();
    }
}

void handleRealtimeData() {
    if (wifi_state.connected && fb_state.authenticated) {
        firebasePushSensorData(sensorData);
        logMessage("DEBUG", "Real-time data pushed");
    } else {
        logMessage("WARN", "Real-time mode but WiFi down — buffering");
        sdWriteBufferRecord(sensorData.rawCSV);
    }
}

void handleBufferedData() {
    if (wifi_state.connected && fb_state.authenticated) {
        firebasePushSensorData(sensorData);
    } else {
        sdWriteBufferRecord(sensorData.rawCSV);
        logMessage("DEBUG", "Data buffered to SD");
    }
}

// ============================================================================
// CONFIG PIPELINE
// ============================================================================

static unsigned long configSentTime = 0;

void processConfigPipeline() {
    if (config.pendingConfigChange && !config.configSentToS3) {
        if (s3_comm.s3Active) {
            sendConfigToS3(config.normalRateSec, config.highRateSec, config.hMaxCm);
            configSentTime = millis();
        } else {
            logMessage("WARN", "Config pending but S3 not active");
        }
    }

    if (config.configSentToS3 && config.pendingConfigChange) {
        if (millis() - configSentTime > 10000) {
            logMessage("WARN", "S3 config ACK timeout — resending");
            config.configSentToS3 = false;
            configSentTime = 0;
        }
    }
}

// ============================================================================
// OTA PIPELINE
// ============================================================================

// SLIP framing
#define SLIP_END        0xC0
#define SLIP_ESC        0xDB
#define SLIP_ESC_END    0xDC
#define SLIP_ESC_ESC    0xDD

#define ESP_SYNC        0x08
#define ESP_FLASH_BEGIN 0x0B
#define ESP_FLASH_DATA  0x0C
#define ESP_FLASH_END   0x0D

#define FLASH_WRITE_SIZE      0x400
#define ESP_BOOTLOADER_BAUD   115200

void slipSendByte(uint8_t b) {
    if (b == SLIP_END) {
        Serial1.write(SLIP_ESC);
        Serial1.write(SLIP_ESC_END);
    } else if (b == SLIP_ESC) {
        Serial1.write(SLIP_ESC);
        Serial1.write(SLIP_ESC_ESC);
    } else {
        Serial1.write(b);
    }
}

void slipSendFrame(uint8_t* data, size_t len) {
    Serial1.write(SLIP_END);
    for (size_t i = 0; i < len; i++) {
        slipSendByte(data[i]);
    }
    Serial1.write(SLIP_END);
}

int slipReceiveFrame(uint8_t* buf, size_t maxLen, unsigned long timeoutMs) {
    unsigned long start = millis();
    bool inFrame = false;
    size_t idx = 0;
    bool escaped = false;

    while (millis() - start < timeoutMs) {
        if (!Serial1.available()) { delay(1); continue; }

        uint8_t b = Serial1.read();

        if (b == SLIP_END) {
            if (inFrame && idx > 0) return idx;
            inFrame = true;
            idx = 0;
            continue;
        }

        if (!inFrame) continue;

        if (escaped) {
            if (b == SLIP_ESC_END) b = SLIP_END;
            else if (b == SLIP_ESC_ESC) b = SLIP_ESC;
            escaped = false;
        } else if (b == SLIP_ESC) {
            escaped = true;
            continue;
        }

        if (idx < maxLen) buf[idx++] = b;
    }
    return -1;
}

void espSendCommand(uint8_t op, uint8_t* data, uint16_t dataLen, uint32_t checksum) {
    uint16_t frameLen = 8 + dataLen;
    uint8_t* frame = (uint8_t*)malloc(frameLen);
    if (!frame) return;

    frame[0] = 0x00;
    frame[1] = op;
    frame[2] = dataLen & 0xFF;
    frame[3] = (dataLen >> 8) & 0xFF;
    frame[4] = checksum & 0xFF;
    frame[5] = (checksum >> 8) & 0xFF;
    frame[6] = (checksum >> 16) & 0xFF;
    frame[7] = (checksum >> 24) & 0xFF;

    if (data && dataLen > 0) memcpy(frame + 8, data, dataLen);

    slipSendFrame(frame, frameLen);
    free(frame);
}

bool espReceiveResponse(uint8_t expectedOp, unsigned long timeoutMs) {
    uint8_t respBuf[256];
    int len = slipReceiveFrame(respBuf, sizeof(respBuf), timeoutMs);

    if (len < 8) return false;
    if (respBuf[0] != 0x01) return false;
    if (respBuf[1] != expectedOp) return false;
    if (len > 8 && respBuf[8] != 0) return false;

    return true;
}

void s3EnterBootloader() {
    logMessage("INFO", "Putting S3 into bootloader mode...");
    s3_comm.s3Active = false;

    digitalWrite(S3_BOOT_PIN, LOW);
    delay(100);
    digitalWrite(S3_RESET_PIN, LOW);
    delay(S3_RESET_PULSE_MS);
    digitalWrite(S3_RESET_PIN, HIGH);
    delay(S3_BOOT_WAIT_MS);
    digitalWrite(S3_BOOT_PIN, HIGH);

    Serial1.end();
    Serial1.begin(ESP_BOOTLOADER_BAUD, SERIAL_8N1, S3_CMD_RX_PIN, S3_CMD_TX_PIN);

    logMessage("INFO", "S3 in bootloader mode — Serial1 at 115200");
}

void s3ExitBootloader() {
    logMessage("INFO", "Exiting S3 bootloader mode...");
    digitalWrite(S3_BOOT_PIN, HIGH);
    s3HardReset();

    Serial1.end();
    Serial1.begin(S3_CMD_BAUD, SERIAL_8N1, S3_CMD_RX_PIN, S3_CMD_TX_PIN);

    delay(2000);
    s3_comm.s3Active = true;
    logMessage("INFO", "S3 back to normal mode");
}

void s3HardReset() {
    digitalWrite(S3_RESET_PIN, LOW);
    delay(S3_RESET_PULSE_MS);
    digitalWrite(S3_RESET_PIN, HIGH);
    delay(1000);
    logMessage("INFO", "S3 hard reset performed");
}

bool s3BootloaderSync() {
    logMessage("INFO", "Syncing with S3 bootloader...");

    uint8_t syncPayload[36];
    syncPayload[0] = 0x07;
    syncPayload[1] = 0x07;
    syncPayload[2] = 0x12;
    syncPayload[3] = 0x20;
    for (int i = 4; i < 36; i++) syncPayload[i] = 0x55;

    for (int attempt = 0; attempt < 10; attempt++) {
        while (Serial1.available()) Serial1.read();

        espSendCommand(ESP_SYNC, syncPayload, sizeof(syncPayload), 0);

        if (espReceiveResponse(ESP_SYNC, 1000)) {
            for (int i = 0; i < 7; i++) espReceiveResponse(ESP_SYNC, 200);
            logMessage("INFO", "Bootloader sync successful");
            return true;
        }
        delay(100);
    }
    logMessage("ERROR", "Bootloader sync FAILED");
    return false;
}

bool s3BootloaderFlashBegin(uint32_t totalSize, uint32_t blockSize,
                             uint32_t blockCount, uint32_t offset) {
    uint8_t payload[16];
    payload[0]  = totalSize & 0xFF;  payload[1]  = (totalSize >> 8) & 0xFF;
    payload[2]  = (totalSize >> 16) & 0xFF; payload[3]  = (totalSize >> 24) & 0xFF;
    payload[4]  = blockCount & 0xFF; payload[5]  = (blockCount >> 8) & 0xFF;
    payload[6]  = (blockCount >> 16) & 0xFF; payload[7]  = (blockCount >> 24) & 0xFF;
    payload[8]  = blockSize & 0xFF;  payload[9]  = (blockSize >> 8) & 0xFF;
    payload[10] = (blockSize >> 16) & 0xFF; payload[11] = (blockSize >> 24) & 0xFF;
    payload[12] = offset & 0xFF;     payload[13] = (offset >> 8) & 0xFF;
    payload[14] = (offset >> 16) & 0xFF; payload[15] = (offset >> 24) & 0xFF;

    espSendCommand(ESP_FLASH_BEGIN, payload, sizeof(payload), 0);

    if (espReceiveResponse(ESP_FLASH_BEGIN, 10000)) {
        logMessage("INFO", "Flash begin accepted");
        return true;
    }
    logMessage("ERROR", "Flash begin failed");
    return false;
}

bool s3BootloaderFlashData(uint8_t* data, size_t dataLen, uint32_t seqNum) {
    size_t payloadLen = 16 + dataLen;
    uint8_t* payload = (uint8_t*)malloc(payloadLen);
    if (!payload) return false;

    payload[0]  = dataLen & 0xFF;  payload[1]  = (dataLen >> 8) & 0xFF;
    payload[2]  = (dataLen >> 16) & 0xFF; payload[3]  = (dataLen >> 24) & 0xFF;
    payload[4]  = seqNum & 0xFF;   payload[5]  = (seqNum >> 8) & 0xFF;
    payload[6]  = (seqNum >> 16) & 0xFF; payload[7]  = (seqNum >> 24) & 0xFF;
    memset(payload + 8, 0, 8);
    memcpy(payload + 16, data, dataLen);

    uint32_t checksum = 0xEF;
    for (size_t i = 0; i < dataLen; i++) checksum ^= data[i];

    espSendCommand(ESP_FLASH_DATA, payload, payloadLen, checksum);
    free(payload);

    return espReceiveResponse(ESP_FLASH_DATA, 5000);
}

bool s3BootloaderFlashEnd(bool reboot) {
    uint8_t payload[4] = { (uint8_t)(reboot ? 0 : 1), 0, 0, 0 };
    espSendCommand(ESP_FLASH_END, payload, sizeof(payload), 0);

    if (reboot) { delay(100); return true; }
    return espReceiveResponse(ESP_FLASH_END, 3000);
}

void startOTADownload() {
    logMessage("INFO", "Starting OTA firmware download...");
    ota_state.phase = OTA_DOWNLOADING;
    firebaseUpdateOTAStatus("DOWNLOADING", 10);

    sdDeleteOTABinary();

    if (strlen(ota_state.firmwareURL) == 0) {
        logMessage("ERROR", "No firmware URL");
        ota_state.phase = OTA_FAILED;
        strncpy(ota_state.errorMsg, "No firmware URL", sizeof(ota_state.errorMsg) - 1);
        return;
    }

    WiFiClientSecure client;
    client.setInsecure();

    HTTPClient http;
    http.begin(client, ota_state.firmwareURL);
    http.setTimeout(30000);

    int httpCode = http.GET();

    if (httpCode != 200) {
        char msg[80];
        snprintf(msg, sizeof(msg), "OTA download failed: HTTP %d", httpCode);
        logMessage("ERROR", msg);
        strncpy(ota_state.errorMsg, msg, sizeof(ota_state.errorMsg) - 1);
        ota_state.phase = OTA_FAILED;
        http.end();
        return;
    }

    int contentLength = http.getSize();
    if (contentLength <= 0) {
        logMessage("ERROR", "No content length");
        ota_state.phase = OTA_FAILED;
        http.end();
        return;
    }

    if (ota_state.firmwareSize == 0) ota_state.firmwareSize = contentLength;

    WiFiClient* stream = http.getStreamPtr();
    uint8_t downloadBuf[1024];
    size_t totalDownloaded = 0;
    bool firstChunk = true;
    int lastPercent = 0;

    while (http.connected() && totalDownloaded < (size_t)contentLength) {
        size_t avail = stream->available();
        if (avail == 0) { delay(10); continue; }

        size_t toRead = (avail > sizeof(downloadBuf)) ? sizeof(downloadBuf) : avail;
        size_t bytesRead = stream->readBytes(downloadBuf, toRead);

        if (bytesRead > 0) {
            if (!sdSaveOTABinary(downloadBuf, bytesRead, !firstChunk)) {
                logMessage("ERROR", "SD write failed during OTA download");
                ota_state.phase = OTA_FAILED;
                http.end();
                return;
            }
            firstChunk = false;
            totalDownloaded += bytesRead;
            ota_state.downloadedBytes = totalDownloaded;

            int pct = (totalDownloaded * 100) / contentLength;
            if (pct >= lastPercent + 10) {
                lastPercent = pct;
                char msg[64];
                snprintf(msg, sizeof(msg), "OTA download: %d%%", pct);
                logMessage("INFO", msg);
                firebaseUpdateOTAStatus("DOWNLOADING", 10 + (pct * 40 / 100));
            }
        }
    }

    http.end();

    if (totalDownloaded == (size_t)contentLength) {
        logMessage("INFO", "OTA download complete");
        ota_state.phase = OTA_DOWNLOAD_COMPLETE;
        firebaseUpdateOTAStatus("DOWNLOAD_COMPLETE", 50);
    } else {
        char msg[80];
        snprintf(msg, sizeof(msg), "OTA incomplete: %lu/%d",
                 (unsigned long)totalDownloaded, contentLength);
        logMessage("ERROR", msg);
        ota_state.phase = OTA_FAILED;
    }
}

void startS3OTA() {
    logMessage("INFO", "=== S3 OTA PROGRAMMING START ===");

    s3EnterBootloader();

    if (!s3BootloaderSync()) {
        strncpy(ota_state.errorMsg, "Bootloader sync failed", sizeof(ota_state.errorMsg) - 1);
        ota_state.phase = OTA_FAILED;
        s3ExitBootloader();
        return;
    }

    if (!SD.exists(SD_OTA_FILE)) {
        logMessage("ERROR", "OTA file not found");
        ota_state.phase = OTA_FAILED;
        s3ExitBootloader();
        return;
    }

    File fwFile = SD.open(SD_OTA_FILE, FILE_READ);
    if (!fwFile) {
        logMessage("ERROR", "Cannot open OTA file");
        ota_state.phase = OTA_FAILED;
        s3ExitBootloader();
        return;
    }

    size_t fileSize = fwFile.size();
    uint32_t blockCount = (fileSize + FLASH_WRITE_SIZE - 1) / FLASH_WRITE_SIZE;
    uint32_t flashOffset = 0x10000;

    char msg[80];
    snprintf(msg, sizeof(msg), "Firmware: %lu bytes, %lu blocks",
             (unsigned long)fileSize, (unsigned long)blockCount);
    logMessage("INFO", msg);

    if (!s3BootloaderFlashBegin(fileSize, FLASH_WRITE_SIZE, blockCount, flashOffset)) {
        ota_state.phase = OTA_FAILED;
        fwFile.close();
        s3ExitBootloader();
        return;
    }

    firebaseUpdateOTAStatus("FLASHING", 55);

    uint8_t blockBuf[FLASH_WRITE_SIZE];
    uint32_t seqNum = 0;
    size_t totalWritten = 0;
    int lastPct = 0;
    bool flashError = false;

    while (fwFile.available() && !flashError) {
        size_t bytesRead = fwFile.read(blockBuf, FLASH_WRITE_SIZE);
        if (bytesRead < FLASH_WRITE_SIZE) {
            memset(blockBuf + bytesRead, 0xFF, FLASH_WRITE_SIZE - bytesRead);
        }

        bool written = false;
        for (int retry = 0; retry < 3; retry++) {
            if (s3BootloaderFlashData(blockBuf, FLASH_WRITE_SIZE, seqNum)) {
                written = true;
                break;
            }
            delay(100);
        }

        if (!written) {
            snprintf(msg, sizeof(msg), "Flash failed at block %lu", (unsigned long)seqNum);
            logMessage("ERROR", msg);
            flashError = true;
            break;
        }

        seqNum++;
        totalWritten += bytesRead;
        ota_state.writtenBytes = totalWritten;

        int pct = (totalWritten * 100) / fileSize;
        if (pct >= lastPct + 5) {
            lastPct = pct;
            snprintf(msg, sizeof(msg), "Flashing: %d%%", pct);
            logMessage("INFO", msg);
            firebaseUpdateOTAStatus("FLASHING", 55 + (pct * 40 / 100));
        }
    }

    fwFile.close();

    if (flashError) {
        ota_state.phase = OTA_FAILED;
        s3ExitBootloader();
        return;
    }

    s3BootloaderFlashEnd(true);

    digitalWrite(S3_BOOT_PIN, HIGH);
    Serial1.end();
    Serial1.begin(S3_CMD_BAUD, SERIAL_8N1, S3_CMD_RX_PIN, S3_CMD_TX_PIN);

    logMessage("INFO", "Flash complete — verifying");
    ota_state.phase = OTA_VERIFY;
    ota_state.phaseStartMs = millis();
    firebaseUpdateOTAStatus("VERIFYING", 95);
}

void processOTAPipeline() {
    char msg[192];

    switch (ota_state.phase) {
        case OTA_IDLE:
            break;

        case OTA_DOWNLOAD_PENDING:
            startOTADownload();
            break;

        case OTA_DOWNLOADING:
            break;

        case OTA_DOWNLOAD_COMPLETE: {
            size_t fileSize = sdGetOTABinarySize();
            if (fileSize > 0 && (ota_state.firmwareSize == 0 || fileSize == ota_state.firmwareSize)) {
                logMessage("INFO", "OTA binary verified — starting S3 programming");
                ota_state.phase = OTA_PROGRAMMING_S3;
                ota_state.phaseStartMs = millis();
                firebaseUpdateOTAStatus("PROGRAMMING_S3", 50);
                startS3OTA();
            } else {
                snprintf(msg, sizeof(msg), "Size mismatch: expected %lu, got %lu",
                         (unsigned long)ota_state.firmwareSize, (unsigned long)fileSize);
                logMessage("ERROR", msg);
                ota_state.phase = OTA_FAILED;
                firebaseUpdateOTAStatus("FAILED_SIZE_MISMATCH", 0);
            }
            break;
        }

        case OTA_PROGRAMMING_S3:
            break;

        case OTA_VERIFY: {
            logMessage("INFO", "Verifying S3 after OTA...");
            delay(3000);
            sendPingToS3();

            unsigned long verifyStart = millis();
            while (millis() - verifyStart < 5000) {
                processS3Commands();
                if (s3_comm.pongReceived) break;
                delay(100);
            }

            if (s3_comm.pongReceived) {
                logMessage("INFO", "S3 responds — OTA SUCCESSFUL");
                ota_state.phase = OTA_COMPLETE;
                ota_state.phaseStartMs = millis();
                firebaseUpdateOTAStatus("COMPLETE", 100);
                s3_comm.s3Active = true;

                delay(2000);
                sendConfigToS3(config.normalRateSec, config.highRateSec, config.hMaxCm);
                sdDeleteOTABinary();
            } else {
                logMessage("ERROR", "S3 not responding after OTA");
                ota_state.phase = OTA_FAILED;
                strncpy(ota_state.errorMsg, "S3 no response after flash",
                        sizeof(ota_state.errorMsg) - 1);
                firebaseUpdateOTAStatus("FAILED_NO_RESPONSE", 0);
                s3HardReset();
            }
            break;
        }

        case OTA_COMPLETE:
            if (millis() - ota_state.phaseStartMs > 5000) {
                ota_state.phase = OTA_IDLE;
                logMessage("INFO", "OTA process complete — idle");
            }
            break;

        case OTA_FAILED:
            snprintf(msg, sizeof(msg), "OTA FAILED: %s (retry %d)",
                     ota_state.errorMsg, ota_state.retryCount);
            logMessage("ERROR", msg);

            if (ota_state.retryCount < 3) {
                ota_state.retryCount++;
                ota_state.phase = OTA_DOWNLOAD_PENDING;
                logMessage("INFO", "Retrying OTA...");
            } else {
                logMessage("ERROR", "OTA failed after 3 retries — giving up");
                s3ExitBootloader();
                s3HardReset();
                s3_comm.s3Active = true;
                ota_state.phase = OTA_IDLE;
                sdDeleteOTABinary();
                firebaseUpdateOTAStatus("FAILED_FINAL", 0);
            }
            break;
    }
}

// ============================================================================
// LOGGING
// ============================================================================

void logMessage(const char* level, const char* msg) {
    unsigned long uptime = millis() / 1000;
    Serial.printf("[%lu][C3][%s] %s\n", uptime, level, msg);
}

// ============================================================================
// SETUP
// ============================================================================

void setup() {
    Serial.begin(115200);
    unsigned long serialWait = millis();
    while (!Serial && millis() - serialWait < 3000) delay(10);

    Serial.println();
    Serial.println("================================================================");
    Serial.println("  VARUNA BUOY — ESP32-C3 COMMUNICATION BRIDGE");
    Serial.println("  Firmware v1.0.0 (No external libraries)");
    Serial.println("================================================================");
    Serial.println();

    initPins();

    Serial1.begin(S3_CMD_BAUD, SERIAL_8N1, S3_CMD_RX_PIN, S3_CMD_TX_PIN);
    logMessage("INFO", "Serial1 initialized — GPIO 0(TX)/1(RX) at 9600");

    s3_comm.s3Active = true;
    s3_comm.pongReceived = false;
    s3_comm.lastDataReceived = 0;
    s3_comm.lastPingSent = 0;

    initSoftwareUARTReceiver();
    logMessage("INFO", "Initializing SD card...");
    initSDCard();
    initConfig();
    initOTA();

    logMessage("INFO", "Initializing WiFi...");
    initWiFi();

    for (int i = 0; i < NUM_WIFI_CREDENTIALS; i++) {
        char msg[64];
        snprintf(msg, sizeof(msg), "Trying WiFi: %s", wifiCredentials[i].ssid);
        logMessage("INFO", msg);
        if (wifiTryConnect(wifiCredentials[i].ssid, wifiCredentials[i].password)) break;
    }

    if (wifi_state.connected) {
        initFirebase();
        firebaseUpdateDeviceStatus();

        logMessage("INFO", "Reading initial config from Firebase...");
        firebasePollConfig();

        delay(2000);
        sendConfigToS3(config.normalRateSec, config.highRateSec, config.hMaxCm);

        firebasePollOTA();
    } else {
        logMessage("WARN", "WiFi not connected at startup");
    }

    logMessage("INFO", "Pinging S3...");
    sendPingToS3();

    unsigned long pingWait = millis();
    while (millis() - pingWait < 3000) {
        processS3Commands();
        if (s3_comm.pongReceived) {
            logMessage("INFO", "S3 is alive");
            break;
        }
        delay(50);
    }

    if (!s3_comm.pongReceived) {
        logMessage("WARN", "S3 did not respond to initial PING");
    }

    Serial.println();
    logMessage("INFO", "======== INITIALIZATION COMPLETE ========");
    Serial.println();
}

// ============================================================================
// MAIN LOOP
// ============================================================================

void loop() {
    // Phase 1: WiFi
    wifiCheckConnection();
    if (!wifi_state.connected) wifiConnectLoop();

    // Phase 2: Receive data from S3
    softUartProcessReceived();

    // Phase 3: S3 command responses
    processS3Commands();

    // Phase 4: Firebase operations
    if (wifi_state.connected) {
        firebasePollConfig();

        if (ota_state.phase == OTA_IDLE) {
            static unsigned long lastOTAPoll = 0;
            if (millis() - lastOTAPoll > 30000) {
                firebasePollOTA();
                lastOTAPoll = millis();
            }
        }

        if (!config.realtimeMode && sd_state.bufferHasData) {
            sdFlushToFirebase();
        }

        firebaseUpdateDeviceStatus();
    }

    // Phase 5: Config pipeline
    processConfigPipeline();

    // Phase 6: OTA pipeline
    processOTAPipeline();

    // Phase 7: S3 liveness
    if (s3_comm.s3Active && ota_state.phase == OTA_IDLE) {
        unsigned long timeSinceData = millis() - s3_comm.lastDataReceived;
        unsigned long maxExpected = (config.normalRateSec + 60) * 1000UL;

        if (s3_comm.lastDataReceived > 0 && timeSinceData > maxExpected) {
            unsigned long timeSincePing = millis() - s3_comm.lastPingSent;
            if (timeSincePing > 60000) {
                logMessage("WARN", "No data from S3 — sending PING");
                sendPingToS3();
            }
        }
    }

    // Phase 8: Diagnostic trigger check
    static unsigned long lastDiagCheck = 0;
    if (wifi_state.connected && millis() - lastDiagCheck > 60000) {
        lastDiagCheck = millis();

        char response[32];
        if (firebaseHTTPGet("/diagnostics/trigger", response, sizeof(response))) {
            if (strstr(response, "true") != NULL) {
                logMessage("INFO", "Diagnostic trigger from website");
                sendDiagRunToS3();
                firebaseHTTPPut("/diagnostics/trigger", "false");
            }
        }
    }

    delay(1);
}
