// ============================================================================
// VARUNA BUOY — ESP32-S3 SENSOR BRAIN FIRMWARE
// ============================================================================
// This firmware runs entirely on the ESP32-S3. It reads all sensors,
// performs sensor fusion, classifies flood state, and outputs data on
// two serial streams. It has ZERO WiFi capability — that is the C3's job.
//
// Sensors:
//   MPU6050 (I2C bus 0) — 6-axis IMU for tilt/water level
//   BMP280  (I2C bus 1) — Barometric pressure for submersion detection
//   DS1307  (I2C bus 1) — Real-time clock for timestamps
//   GPS NEO (UART1)     — Location and backup time
//   Battery (ADC GPIO2) — Power monitoring
//
// Outputs:
//   USB Serial (115200) — 39-field CSV + debug/status lines → debugger
//   GPIO 14 (SW UART 9600) — 39-field CSV → C3 → Firebase
//   Serial2 (9600) — Command responses → C3
//
// Inputs:
//   Serial2 (9600) — Commands from C3 ($CFG, $DIAGRUN, $PING)
//   USB Serial — Commands from debugger (PING, GETCONFIG, etc.)
// ============================================================================

#include <Wire.h>
#include <math.h>

// ============================================================================
// PIN DEFINITIONS
// ============================================================================
#define MPU_SDA          8
#define MPU_SCL          9
#define SENS_SDA         4
#define SENS_SCL         5
#define GPS_RX_PIN       6   // GPS TX connects here (S3 receives)
#define GPS_TX_PIN       7   // GPS RX connects here (S3 transmits)
#define C3_DATA_PIN      14  // Software UART TX → C3 (CSV data)
#define C3_CMD_TX_PIN    43  // Serial2 TX → C3 (command responses)
#define C3_CMD_RX_PIN    44  // Serial2 RX ← C3 (commands)
#define BATTERY_ADC_PIN  2

// ============================================================================
// I2C ADDRESSES
// ============================================================================
#define MPU6050_ADDR     0x68
#define BMP280_ADDR      0x76  // or 0x77 depending on SDO pin
#define DS1307_ADDR      0x68  // Same as MPU but on different bus!

// ============================================================================
// MPU6050 REGISTER MAP
// ============================================================================
#define MPU_REG_WHO_AM_I       0x75
#define MPU_REG_PWR_MGMT_1     0x6B
#define MPU_REG_PWR_MGMT_2     0x6C
#define MPU_REG_SMPLRT_DIV     0x19
#define MPU_REG_CONFIG         0x1A
#define MPU_REG_GYRO_CONFIG    0x1B
#define MPU_REG_ACCEL_CONFIG   0x1C
#define MPU_REG_ACCEL_XOUT_H   0x3B
#define MPU_REG_GYRO_XOUT_H    0x43
#define MPU_REG_TEMP_OUT_H     0x41
#define MPU_WHO_AM_I_VAL       0x68

// ============================================================================
// BMP280 REGISTER MAP
// ============================================================================
#define BMP280_REG_CHIP_ID     0xD0
#define BMP280_REG_RESET       0xE0
#define BMP280_REG_STATUS      0xF3
#define BMP280_REG_CTRL_MEAS   0xF4
#define BMP280_REG_CONFIG_REG  0xF5
#define BMP280_REG_PRESS_MSB   0xF7
#define BMP280_REG_TEMP_MSB    0xFA
#define BMP280_REG_CALIB_START 0x88
#define BMP280_CHIP_ID_VAL     0x58

// ============================================================================
// DS1307 REGISTER MAP
// ============================================================================
#define DS1307_REG_SECONDS     0x00
#define DS1307_REG_CONTROL     0x07

// ============================================================================
// SENSOR FUSION CONSTANTS
// ============================================================================
#define ALPHA                  0.98f    // Complementary filter weight (gyro)
#define GYRO_SENSITIVITY       131.0f   // LSB/(°/s) for ±250°/s range
#define ACCEL_SENSITIVITY      16384.0f // LSB/g for ±2g range
#define GRAVITY_MS2            9.80665f
#define FUSION_RATE_HZ         100      // 100Hz sensor fusion loop
#define FUSION_INTERVAL_US     10000    // 10ms = 1/100Hz

// ============================================================================
// CALIBRATION CONSTANTS
// ============================================================================
#define GYRO_CAL_SAMPLES       1000
#define ACCEL_CAL_SAMPLES      500
#define PRESSURE_CAL_SAMPLES   50
#define ACCEL_G_LOW            0.9f     // Accept gravity reading if |g| in range
#define ACCEL_G_HIGH           1.1f

// ============================================================================
// FLOOD DETECTION THRESHOLDS (defaults, can be overridden via commands)
// ============================================================================
#define DEFAULT_LATERAL_ACCEL_TAUT   0.15f  // m/s² threshold for tether taut
#define DEFAULT_LATERAL_ACCEL_SLACK  0.10f  // m/s² hysteresis for going slack
#define DEFAULT_TILT_TAUT_DEG        3.0f   // degrees minimum for taut detection
#define DEFAULT_FLOOD_THETA_DEG      10.0f  // degrees threshold for flood mode
#define DEFAULT_FLOOD_RATIO          0.95f  // H/L ratio for flood mode
#define DEFAULT_SUBMERSION_PA        500.0f // Pa above baseline for submersion
#define WATER_DENSITY_KGM3           1000.0f
#define MODE_PERSISTENCE_COUNT       10     // Readings before mode commit
#define SUBMERGE_PERSISTENCE_COUNT   3      // Faster for emergencies

// ============================================================================
// SAMPLING RATE DEFAULTS (overridden from website via C3)
// ============================================================================
#define DEFAULT_NORMAL_RATE_SEC      900    // 15 minutes
#define DEFAULT_HIGH_RATE_SEC        60     // 1 minute
#define RATE_INTERP_LOW              0.50f  // Below 50% H_max → normal rate
#define RATE_INTERP_HIGH             0.80f  // Above 80% H_max → high rate

// ============================================================================
// CONTINUOUS MONITORING INTERVAL
// ============================================================================
#define MONITOR_INTERVAL_MS          2000   // 2 seconds — continuous flood monitoring

// ============================================================================
// DIAGNOSTIC CONSTANTS
// ============================================================================
#define DIAG_INTERVAL_MS       86400000UL  // 24 hours
#define PONG_TIMEOUT_MS        2000

// ============================================================================
// SOFTWARE UART CONSTANTS (GPIO 14 → C3)
// ============================================================================
#define SW_UART_BAUD           9600
#define SW_UART_BIT_TIME_US    104  // 1000000 / 9600 ≈ 104.17µs

// ============================================================================
// GPS PARSING BUFFER
// ============================================================================
#define GPS_BUFFER_SIZE        256
#define NMEA_MAX_FIELDS        20

// ============================================================================
// OPERATING MODES
// ============================================================================
enum FloodMode {
    MODE_SLACK     = 0,  // Tether slack, water below threshold, SAFE
    MODE_TAUT      = 1,  // Tether taut, water approaching threshold
    MODE_FLOOD     = 2,  // Water at flood level, θ→0
    MODE_SUBMERGED = 3   // Buoy underwater, pressure-based measurement
};

// ============================================================================
// ALERT LEVELS (for field 18)
// ============================================================================
enum AlertLevel {
    ALERT_GREEN    = 0,  // Safe
    ALERT_YELLOW   = 1,  // Elevated
    ALERT_RED      = 2,  // Flood
    ALERT_BLACK    = 3   // Critical / submerged
};

// ============================================================================
// ZONE CLASSIFICATION (for field 30) — derived from flood ratio
// ============================================================================
enum WaterZone {
    ZONE_SAFE      = 0,  // < 50% of H_max
    ZONE_WATCH     = 1,  // 50-80%
    ZONE_WARNING   = 2,  // 80-95%
    ZONE_CRITICAL  = 3   // > 95% or submerged
};

// ============================================================================
// RESPONSE LEVELS (for field 31)
// ============================================================================
enum ResponseLevel {
    RESPONSE_NONE     = 0,
    RESPONSE_MONITOR  = 1,
    RESPONSE_PREPARE  = 2,
    RESPONSE_ACT      = 3,
    RESPONSE_EVACUATE = 4
};

// ============================================================================
// BMP280 CALIBRATION DATA STRUCTURE
// ============================================================================
struct BMP280CalibData {
    uint16_t dig_T1;
    int16_t  dig_T2;
    int16_t  dig_T3;
    uint16_t dig_P1;
    int16_t  dig_P2;
    int16_t  dig_P3;
    int16_t  dig_P4;
    int16_t  dig_P5;
    int16_t  dig_P6;
    int16_t  dig_P7;
    int16_t  dig_P8;
    int16_t  dig_P9;
    int32_t  t_fine;  // Used internally during compensation
};

// ============================================================================
// GPS DATA STRUCTURE
// ============================================================================
struct GPSData {
    float    latitude;
    float    longitude;
    float    altitude;
    int      satellites;
    bool     fixValid;
    int      fixQuality;     // 0=no fix, 1=GPS, 2=DGPS
    uint8_t  hour, minute, second;
    uint8_t  day, month;
    uint16_t year;
    bool     timeValid;
    bool     dateValid;
    unsigned long lastUpdateMs;
};

// ============================================================================
// DIAGNOSTIC RESULTS STRUCTURE
// ============================================================================
struct DiagResult {
    // MPU6050 tests
    bool mpuWhoAmIOk;
    bool mpuAccelMagnitudeOk;
    bool mpuGyroDriftOk;
    float mpuAccelMagnitude;
    float mpuGyroDrift;

    // BMP280 tests
    bool bmpChipIdOk;
    bool bmpPressureRangeOk;
    bool bmpTempRangeOk;
    float bmpPressure;
    float bmpTemperature;

    // DS1307 tests
    bool rtcOscRunning;
    bool rtcTimeValid;

    // GPS tests
    bool gpsDataRecent;
    int  gpsSatCount;

    // Battery test
    bool batteryVoltageOk;
    float batteryVoltage;

    // C3 communication test
    bool c3PongReceived;

    // Summary
    int totalFaults;
    int healthScore;
};

// ============================================================================
// GLOBAL STATE — TWO I2C BUS INSTANCES
// ============================================================================
TwoWire I2C_MPU  = TwoWire(0);  // Bus 0: MPU6050 only
TwoWire I2C_SENS = TwoWire(1);  // Bus 1: BMP280 + DS1307

// ============================================================================
// GLOBAL STATE — SENSOR AVAILABILITY FLAGS
// ============================================================================
bool mpuAvailable  = false;
bool bmpAvailable  = false;
bool rtcAvailable  = false;
bool gpsAvailable  = false;

// ============================================================================
// GLOBAL STATE — MPU6050 CALIBRATION
// ============================================================================
float gyroOffsetX  = 0.0f;
float gyroOffsetY  = 0.0f;
float gyroOffsetZ  = 0.0f;
float refAccX      = 0.0f;
float refAccY      = 0.0f;
float refAccZ      = 1.0f;  // Default: gravity along Z
float refTiltX     = 0.0f;  // Reference tilt X (degrees)
float refTiltY     = 0.0f;  // Reference tilt Y (degrees)
bool  calibrated   = false;

// ============================================================================
// GLOBAL STATE — SENSOR FUSION OUTPUT
// ============================================================================
float filtTiltX    = 0.0f;  // Filtered tilt around X axis (degrees)
float filtTiltY    = 0.0f;  // Filtered tilt around Y axis (degrees)
float correctedTiltX = 0.0f;
float correctedTiltY = 0.0f;
float combinedTheta  = 0.0f;  // sqrt(corrTiltX² + corrTiltY²)
float lateralAccel   = 0.0f;  // sqrt(ax² + ay²) in m/s²
unsigned long lastFusionUs = 0;

// ============================================================================
// GLOBAL STATE — RAW SENSOR VALUES (latest reading)
// ============================================================================
int16_t rawAccX, rawAccY, rawAccZ;
int16_t rawGyroX, rawGyroY, rawGyroZ;
int16_t rawMpuTemp;
float   accXg, accYg, accZg;       // In g units
float   gyroXdps, gyroYdps, gyroZdps; // In degrees/second

// ============================================================================
// GLOBAL STATE — BMP280
// ============================================================================
BMP280CalibData bmpCalib;
float currentPressure    = 0.0f;    // hPa
float currentTemperature = 0.0f;    // °C
float baselinePressure   = 0.0f;    // hPa (set at deployment)
float pressureDeviation  = 0.0f;    // hPa (current - baseline)
float gaugePressurePa    = 0.0f;    // Pa (for submersion)
float estimatedDepthCm   = 0.0f;    // cm below surface (MODE 3)
bool  baselineSet        = false;

// ============================================================================
// GLOBAL STATE — DS1307 RTC
// ============================================================================
uint8_t  rtcSeconds, rtcMinutes, rtcHours;
uint8_t  rtcDay, rtcMonth;
uint16_t rtcYear;
uint32_t rtcUnixTime    = 0;
bool     rtcTimeValid   = false;
char     dateTimeStr[24] = "0000-00-00 00:00:00";

// ============================================================================
// GLOBAL STATE — GPS
// ============================================================================
GPSData gps;
char    gpsBuffer[GPS_BUFFER_SIZE];
int     gpsBufferIdx = 0;

// ============================================================================
// GLOBAL STATE — BATTERY
// ============================================================================
float batteryVoltage  = 0.0f;
float batteryPercent  = 0.0f;

// ============================================================================
// GLOBAL STATE — FLOOD DETECTION
// ============================================================================
FloodMode    currentMode        = MODE_SLACK;
FloodMode    pendingMode        = MODE_SLACK;
int          modePersistCount   = 0;

float        olpLengthCm        = 200.0f;  // Tether length (default 2m)
float        hMaxCm             = 200.0f;  // Same as OLP length initially
float        waterHeightCm      = 0.0f;
float        horizontalDistCm   = 0.0f;
float        floodRatio         = 0.0f;    // H / H_max

AlertLevel   floodAlertLevel    = ALERT_GREEN;
WaterZone    currentZone        = ZONE_SAFE;
ResponseLevel currentResponse   = RESPONSE_NONE;

// ============================================================================
// GLOBAL STATE — CONFIGURABLE THRESHOLDS
// ============================================================================
float threshLateralAccelTaut  = DEFAULT_LATERAL_ACCEL_TAUT;
float threshLateralAccelSlack = DEFAULT_LATERAL_ACCEL_SLACK;
float threshTiltTautDeg       = DEFAULT_TILT_TAUT_DEG;
float threshFloodThetaDeg     = DEFAULT_FLOOD_THETA_DEG;
float threshFloodRatio        = DEFAULT_FLOOD_RATIO;
float threshSubmersionPa      = DEFAULT_SUBMERSION_PA;

// ============================================================================
// GLOBAL STATE — DYNAMIC SAMPLING
// ============================================================================
int          normalRateSec      = DEFAULT_NORMAL_RATE_SEC;
int          highRateSec        = DEFAULT_HIGH_RATE_SEC;
int          currentIntervalSec = DEFAULT_NORMAL_RATE_SEC;
unsigned long lastTransmitMs    = 0;
unsigned long lastMonitorMs     = 0;

// ============================================================================
// GLOBAL STATE — SESSION STATISTICS
// ============================================================================
unsigned long sessionStartMs   = 0;
float         peakHeightCm     = 0.0f;
float         minHeightCm      = 99999.0f;
float         prevHeightCm     = 0.0f;
unsigned long prevHeightTimeMs = 0;
float         rateOfRiseCmPer15Min = 0.0f;
bool          sustainedRise    = false;
int           riseConsecutive  = 0;

// ============================================================================
// GLOBAL STATE — DIAGNOSTIC SCHEDULING
// ============================================================================
unsigned long lastDiagMs       = 0;
bool          diagRequested    = false;

// ============================================================================
// GLOBAL STATE — HEALTH SCORE
// ============================================================================
int healthScore = 0;

// ============================================================================
// GLOBAL STATE — FEATURE FLAGS (from website via C3)
// ============================================================================
bool obLightEnabled     = true;   // Field 36 — OB light status
bool algorithmEnabled   = false;  // Field 37 — advanced algorithm flag

// ============================================================================
// GLOBAL STATE — SIM FIELDS (read from C3 or defaults)
// ============================================================================
int  simSignalRSSI   = 0;
bool simRegistered   = false;
bool simAvailable    = false;

// ============================================================================
// FORWARD DECLARATIONS
// ============================================================================

// Initialization
void initMPU6050();
void initBMP280();
void initDS1307();
void initGPS();
void initBatteryADC();
void initSoftwareUART();

// MPU6050 operations
bool mpuWriteRegister(uint8_t reg, uint8_t value);
uint8_t mpuReadRegister(uint8_t reg);
void mpuReadSensorData();
void mpuConvertToPhysical();

// BMP280 operations
bool bmpWriteRegister(uint8_t reg, uint8_t value);
uint8_t bmpReadRegister(uint8_t reg);
void bmpReadCalibration();
void bmpReadRaw(int32_t &rawTemp, int32_t &rawPress);
float bmpCompensateTemperature(int32_t rawTemp);
float bmpCompensatePressure(int32_t rawPress);
void bmpUpdate();

// DS1307 operations
uint8_t bcdToDec(uint8_t val);
uint8_t decToBcd(uint8_t val);
void rtcReadTime();
uint32_t rtcToUnixTime(uint16_t year, uint8_t month, uint8_t day,
                        uint8_t hour, uint8_t min, uint8_t sec);
void rtcFormatDateTime();

// GPS operations
void gpsProcessIncoming();
void gpsParseNMEA(const char* sentence);
void gpsParseGGA(const char* sentence);
void gpsParseRMC(const char* sentence);
float nmeaToDecimalDegrees(const char* nmeaCoord, const char* hemisphere);
int  nmeaSplitFields(const char* sentence, char fields[][20], int maxFields);

// Battery
void readBattery();

// Calibration
void recalibrate();
void calibrateGyro();
void calibrateAccel();
void calibratePressureBaseline();

// Sensor fusion
void runSensorFusion();

// Flood detection
void classifyFloodMode();
void updateModePersistence(FloodMode detected);
void computeWaterHeight();
void computeFloodRatio();
void computeAlertLevel();
void computeZone();
void computeResponseLevel();
void computeDynamicSamplingRate();
void updateSessionStats();

// Output
void transmitData();
void buildCSVString(char* buffer, int bufSize);
void c3UartSendByte(uint8_t b);
void c3UartSendString(const char* str);

// Command processing
void processC3Commands();
void processDebuggerCommands();
void handleC3Command(const char* cmd);
void handleDebuggerCommand(const char* cmd);

// Diagnostics
void runDiagnostics();
void sendDiagToC3(DiagResult &diag);

// Health score
int computeHealthScore();

// Utilities
void statusPrint(const char* msg);
void errorPrint(const char* msg);
void warningPrint(const char* msg);


// ============================================================================
// ============================================================================
//                              SETUP
// ============================================================================
// ============================================================================

void setup() {
    // ========================================================================
    // 1. Initialize USB Serial (debugger connection)
    // ========================================================================
    Serial.begin(115200);
    unsigned long serialWait = millis();
    while (!Serial && (millis() - serialWait < 3000)) {
        // Wait up to 3s for USB serial — don't hang if no USB connected
    }
    delay(500);

    statusPrint("VARUNA S3 Sensor Brain — Firmware v2.0");
    statusPrint("Initializing...");

    // ========================================================================
    // 2. Initialize Serial2 (command channel to/from C3)
    // ========================================================================
    Serial2.begin(9600, SERIAL_8N1, C3_CMD_RX_PIN, C3_CMD_TX_PIN);
    statusPrint("Serial2 (C3 cmd channel) initialized on GPIO 43/44 @ 9600 baud");

    // ========================================================================
    // 3. Initialize UART1 (GPS)
    // ========================================================================
    Serial1.begin(9600, SERIAL_8N1, GPS_RX_PIN, GPS_TX_PIN);
    statusPrint("Serial1 (GPS) initialized on GPIO 6/7 @ 9600 baud");

    // ========================================================================
    // 4. Initialize Software UART TX pin (GPIO 14 → C3 data)
    // ========================================================================
    initSoftwareUART();
    statusPrint("Software UART TX initialized on GPIO 14 @ 9600 baud");

    // ========================================================================
    // 5. Initialize I2C Bus 0 (MPU6050)
    // ========================================================================
    I2C_MPU.begin(MPU_SDA, MPU_SCL, 400000);  // 400kHz for MPU
    statusPrint("I2C Bus 0 initialized (MPU6050) on GPIO 8/9 @ 400kHz");

    // ========================================================================
    // 6. Initialize I2C Bus 1 (BMP280 + DS1307)
    // ========================================================================
    I2C_SENS.begin(SENS_SDA, SENS_SCL, 100000);  // 100kHz for RTC compatibility
    statusPrint("I2C Bus 1 initialized (BMP280+DS1307) on GPIO 4/5 @ 100kHz");

    // ========================================================================
    // 7. Initialize each sensor
    // ========================================================================
    initMPU6050();
    initBMP280();
    initDS1307();
    initGPS();
    initBatteryADC();

    // ========================================================================
    // 8. Run calibration
    // ========================================================================
    if (mpuAvailable) {
        statusPrint("Starting sensor calibration...");
        recalibrate();
        statusPrint("Calibration complete");
    } else {
        errorPrint("MPU6050 not available — cannot calibrate");
    }

    // ========================================================================
    // 9. Set pressure baseline
    // ========================================================================
    if (bmpAvailable) {
        calibratePressureBaseline();
    } else {
        warningPrint("BMP280 not available — pressure baseline not set");
    }

    // ========================================================================
    // 10. Initialize session tracking
    // ========================================================================
    sessionStartMs   = millis();
    lastTransmitMs   = millis();
    lastMonitorMs    = millis();
    lastDiagMs       = millis();
    lastFusionUs     = micros();
    prevHeightTimeMs = millis();

    // ========================================================================
    // 11. Compute initial health score
    // ========================================================================
    healthScore = computeHealthScore();

    // ========================================================================
    // 12. Report ready state
    // ========================================================================
    char readyMsg[128];
    snprintf(readyMsg, sizeof(readyMsg),
             "READY — MPU:%d BMP:%d RTC:%d GPS:%d Health:%d%%",
             mpuAvailable, bmpAvailable, rtcAvailable, gpsAvailable, healthScore);
    statusPrint(readyMsg);
    statusPrint("Entering main loop");
}


// ============================================================================
// ============================================================================
//                             MAIN LOOP
// ============================================================================
// ============================================================================

void loop() {
    unsigned long nowMs = millis();
    unsigned long nowUs = micros();

    // ========================================================================
    // TASK 1: Sensor Fusion at 100Hz (every 10ms)
    // ========================================================================
    if ((nowUs - lastFusionUs) >= FUSION_INTERVAL_US) {
        lastFusionUs = nowUs;

        if (mpuAvailable) {
            mpuReadSensorData();
            mpuConvertToPhysical();
            runSensorFusion();
        }
    }

    // ========================================================================
    // TASK 2: GPS continuous parsing (non-blocking)
    // ========================================================================
    gpsProcessIncoming();

    // ========================================================================
    // TASK 3: Process incoming commands from C3 (non-blocking)
    // ========================================================================
    processC3Commands();

    // ========================================================================
    // TASK 4: Process incoming commands from debugger (non-blocking)
    // ========================================================================
    processDebuggerCommands();

    // ========================================================================
    // TASK 5A: Continuous Flood Monitoring (every 2 seconds, always)
    //
    // This runs on a short fixed interval regardless of flood state.
    // It evaluates the current sensor fusion output, updates all flood
    // state variables, and adjusts the dynamic sampling rate in real time.
    // This ensures the system is never blind to rapid water rise between
    // transmissions.
    // ========================================================================
    if ((nowMs - lastMonitorMs) >= MONITOR_INTERVAL_MS) {
        lastMonitorMs = nowMs;

        // Update pressure sensor for submersion detection
        if (bmpAvailable) {
            bmpUpdate();
        }

        // Store previous mode to detect escalation
        FloodMode previousMode = currentMode;

        // Compute water height and flood classification
        computeWaterHeight();
        computeFloodRatio();
        classifyFloodMode();
        computeAlertLevel();
        computeZone();
        computeResponseLevel();
        computeDynamicSamplingRate();

        // Emergency immediate transmission on mode escalation
        // If the mode transitioned to a MORE dangerous state, transmit
        // immediately without waiting for the regular transmit timer.
        if (currentMode > previousMode) {
            char escMsg[128];
            snprintf(escMsg, sizeof(escMsg),
                     "EMERGENCY TRANSMIT: Mode escalated %d → %d",
                     previousMode, currentMode);
            statusPrint(escMsg);

            // Refresh supporting data for the emergency packet
            if (rtcAvailable) {
                rtcReadTime();
                rtcFormatDateTime();
            }
            readBattery();
            updateSessionStats();
            healthScore = computeHealthScore();

            // Transmit immediately on both channels
            transmitData();

            // Reset transmit timer so next regular transmit is one full
            // interval from now (avoids double-send moments later)
            lastTransmitMs = nowMs;
        }
    }

    // ========================================================================
    // TASK 5B: Data Transmission at dynamic interval
    //
    // This only packages and sends the current state. All flood detection,
    // classification, alert levels, zone, response level, and dynamic
    // sampling rate are already current from Task 5A.
    // ========================================================================
    unsigned long intervalMs = (unsigned long)currentIntervalSec * 1000UL;
    if ((nowMs - lastTransmitMs) >= intervalMs) {
        lastTransmitMs = nowMs;

        // Update slower sensors before transmit
        if (rtcAvailable) {
            rtcReadTime();
            rtcFormatDateTime();
        }
        readBattery();

        // Update session statistics
        updateSessionStats();

        // Update health score
        healthScore = computeHealthScore();

        // Transmit on both channels
        transmitData();
    }

    // ========================================================================
    // TASK 6: Periodic diagnostics (every 24 hours)
    // ========================================================================
    if ((nowMs - lastDiagMs) >= DIAG_INTERVAL_MS || diagRequested) {
        lastDiagMs    = nowMs;
        diagRequested = false;
        runDiagnostics();
    }
}


// ============================================================================
// ============================================================================
//                       SENSOR INITIALIZATION
// ============================================================================
// ============================================================================

// ----------------------------------------------------------------------------
// MPU6050 INITIALIZATION
// ----------------------------------------------------------------------------
void initMPU6050() {
    statusPrint("Initializing MPU6050...");

    // Check WHO_AM_I register
    uint8_t whoAmI = mpuReadRegister(MPU_REG_WHO_AM_I);
    if (whoAmI != MPU_WHO_AM_I_VAL) {
        char msg[64];
        snprintf(msg, sizeof(msg), "MPU6050 WHO_AM_I failed: got 0x%02X, expected 0x%02X", whoAmI, MPU_WHO_AM_I_VAL);
        errorPrint(msg);
        mpuAvailable = false;
        return;
    }

    // Wake up MPU6050 (clear sleep bit, select PLL with X-axis gyro ref)
    mpuWriteRegister(MPU_REG_PWR_MGMT_1, 0x01);  // Clock = PLL with X gyro
    delay(10);

    // Enable all axes
    mpuWriteRegister(MPU_REG_PWR_MGMT_2, 0x00);
    delay(10);

    // Sample rate divider: 200Hz (8kHz / (1 + 39) = 200Hz)
    mpuWriteRegister(MPU_REG_SMPLRT_DIV, 39);

    // DLPF config: bandwidth 44Hz, delay 4.9ms (good for buoy — removes high freq)
    mpuWriteRegister(MPU_REG_CONFIG, 0x03);

    // Gyro config: ±250°/s (highest sensitivity: 131 LSB/°/s)
    mpuWriteRegister(MPU_REG_GYRO_CONFIG, 0x00);

    // Accel config: ±2g (highest sensitivity: 16384 LSB/g)
    mpuWriteRegister(MPU_REG_ACCEL_CONFIG, 0x00);

    delay(100);  // Allow sensor to stabilize

    mpuAvailable = true;
    statusPrint("MPU6050 initialized — ±250°/s gyro, ±2g accel, 200Hz, DLPF 44Hz");
}

// ----------------------------------------------------------------------------
// BMP280 INITIALIZATION
// ----------------------------------------------------------------------------
void initBMP280() {
    statusPrint("Initializing BMP280...");

    // Check chip ID
    uint8_t chipId = bmpReadRegister(BMP280_REG_CHIP_ID);
    if (chipId != BMP280_CHIP_ID_VAL) {
        char msg[64];
        snprintf(msg, sizeof(msg), "BMP280 chip ID failed: got 0x%02X, expected 0x%02X", chipId, BMP280_CHIP_ID_VAL);
        errorPrint(msg);
        bmpAvailable = false;
        return;
    }

    // Soft reset
    bmpWriteRegister(BMP280_REG_RESET, 0xB6);
    delay(50);

    // Read calibration data
    bmpReadCalibration();

    // Config register: standby 0.5ms, filter coeff 16, SPI disabled
    // Filter coefficient 16 gives heavy smoothing — good for slow pressure changes
    bmpWriteRegister(BMP280_REG_CONFIG_REG, (0x00 << 5) | (0x04 << 2) | 0x00);

    // Ctrl_meas: temperature oversampling x2, pressure oversampling x16, normal mode
    bmpWriteRegister(BMP280_REG_CTRL_MEAS, (0x02 << 5) | (0x05 << 2) | 0x03);

    delay(100);

    bmpAvailable = true;
    statusPrint("BMP280 initialized — pressure x16 OS, temp x2 OS, filter x16, normal mode");
}

// ----------------------------------------------------------------------------
// DS1307 INITIALIZATION
// ----------------------------------------------------------------------------
void initDS1307() {
    statusPrint("Initializing DS1307 RTC...");

    // Try to read the seconds register
    I2C_SENS.beginTransmission(DS1307_ADDR);
    I2C_SENS.write(DS1307_REG_SECONDS);
    uint8_t err = I2C_SENS.endTransmission();

    if (err != 0) {
        char msg[64];
        snprintf(msg, sizeof(msg), "DS1307 I2C error: %d", err);
        errorPrint(msg);
        rtcAvailable = false;
        return;
    }

    I2C_SENS.requestFrom((uint8_t)DS1307_ADDR, (uint8_t)1);
    if (I2C_SENS.available()) {
        uint8_t secReg = I2C_SENS.read();
        // Check clock halt bit (bit 7 of seconds register)
        if (secReg & 0x80) {
            warningPrint("DS1307 clock halted — enabling oscillator");
            // Clear the clock halt bit to start the oscillator
            I2C_SENS.beginTransmission(DS1307_ADDR);
            I2C_SENS.write(DS1307_REG_SECONDS);
            I2C_SENS.write(secReg & 0x7F);  // Clear bit 7
            I2C_SENS.endTransmission();
        }
        rtcAvailable = true;
        statusPrint("DS1307 initialized — oscillator running");

        // Do an initial time read
        rtcReadTime();
        rtcFormatDateTime();
        char msg[64];
        snprintf(msg, sizeof(msg), "RTC time: %s", dateTimeStr);
        statusPrint(msg);
    } else {
        errorPrint("DS1307 no response to read request");
        rtcAvailable = false;
    }
}

// ----------------------------------------------------------------------------
// GPS INITIALIZATION
// ----------------------------------------------------------------------------
void initGPS() {
    statusPrint("Initializing GPS module...");

    // Initialize GPS data structure
    memset(&gps, 0, sizeof(gps));
    gps.fixValid    = false;
    gps.timeValid   = false;
    gps.dateValid   = false;
    gps.lastUpdateMs = 0;
    gpsBufferIdx = 0;

    // GPS is on Serial1, already initialized in setup()
    // Check if we get any data within 2 seconds
    unsigned long start = millis();
    bool gotData = false;
    while (millis() - start < 2000) {
        if (Serial1.available()) {
            gotData = true;
            break;
        }
        delay(10);
    }

    if (gotData) {
        gpsAvailable = true;
        statusPrint("GPS module responding — NMEA data detected");
    } else {
        gpsAvailable = false;
        warningPrint("GPS module not responding — will retry in main loop");
    }
}

// ----------------------------------------------------------------------------
// BATTERY ADC INITIALIZATION
// ----------------------------------------------------------------------------
void initBatteryADC() {
    statusPrint("Initializing battery ADC on GPIO 2...");
    analogReadResolution(12);  // 12-bit ADC (0-4095)
    analogSetAttenuation(ADC_11db);  // Full range ~0-3.3V
    // Do an initial read
    readBattery();
    char msg[64];
    snprintf(msg, sizeof(msg), "Battery: %.2fV (%.1f%%)", batteryVoltage, batteryPercent);
    statusPrint(msg);
}

// ----------------------------------------------------------------------------
// SOFTWARE UART TX INITIALIZATION
// ----------------------------------------------------------------------------
void initSoftwareUART() {
    pinMode(C3_DATA_PIN, OUTPUT);
    digitalWrite(C3_DATA_PIN, HIGH);  // Idle state for UART is HIGH
}


// ============================================================================
// ============================================================================
//                       MPU6050 LOW-LEVEL OPERATIONS
// ============================================================================
// ============================================================================

bool mpuWriteRegister(uint8_t reg, uint8_t value) {
    I2C_MPU.beginTransmission(MPU6050_ADDR);
    I2C_MPU.write(reg);
    I2C_MPU.write(value);
    return (I2C_MPU.endTransmission() == 0);
}

uint8_t mpuReadRegister(uint8_t reg) {
    I2C_MPU.beginTransmission(MPU6050_ADDR);
    I2C_MPU.write(reg);
    I2C_MPU.endTransmission(false);
    I2C_MPU.requestFrom((uint8_t)MPU6050_ADDR, (uint8_t)1);
    if (I2C_MPU.available()) {
        return I2C_MPU.read();
    }
    return 0xFF;
}

void mpuReadSensorData() {
    // Read 14 bytes starting from ACCEL_XOUT_H (0x3B)
    // Layout: AccX(H,L), AccY(H,L), AccZ(H,L), Temp(H,L), GyroX(H,L), GyroY(H,L), GyroZ(H,L)
    I2C_MPU.beginTransmission(MPU6050_ADDR);
    I2C_MPU.write(MPU_REG_ACCEL_XOUT_H);
    I2C_MPU.endTransmission(false);
    I2C_MPU.requestFrom((uint8_t)MPU6050_ADDR, (uint8_t)14);

    if (I2C_MPU.available() >= 14) {
        rawAccX    = (I2C_MPU.read() << 8) | I2C_MPU.read();
        rawAccY    = (I2C_MPU.read() << 8) | I2C_MPU.read();
        rawAccZ    = (I2C_MPU.read() << 8) | I2C_MPU.read();
        rawMpuTemp = (I2C_MPU.read() << 8) | I2C_MPU.read();
        rawGyroX   = (I2C_MPU.read() << 8) | I2C_MPU.read();
        rawGyroY   = (I2C_MPU.read() << 8) | I2C_MPU.read();
        rawGyroZ   = (I2C_MPU.read() << 8) | I2C_MPU.read();
    }
}

void mpuConvertToPhysical() {
    // Convert accelerometer to g units
    accXg = (float)rawAccX / ACCEL_SENSITIVITY;
    accYg = (float)rawAccY / ACCEL_SENSITIVITY;
    accZg = (float)rawAccZ / ACCEL_SENSITIVITY;

    // Convert gyroscope to degrees/second, with offset correction
    gyroXdps = ((float)rawGyroX / GYRO_SENSITIVITY) - gyroOffsetX;
    gyroYdps = ((float)rawGyroY / GYRO_SENSITIVITY) - gyroOffsetY;
    gyroZdps = ((float)rawGyroZ / GYRO_SENSITIVITY) - gyroOffsetZ;

    // Compute lateral acceleration in m/s² (used for tether taut detection)
    // Lateral = perpendicular to the buoy's vertical axis
    // In buoy frame, if Z is along capsule axis: lateral = sqrt(ax² + ay²)
    lateralAccel = sqrtf(accXg * accXg + accYg * accYg) * GRAVITY_MS2;
}


// ============================================================================
// ============================================================================
//                       BMP280 LOW-LEVEL OPERATIONS
// ============================================================================
// ============================================================================

bool bmpWriteRegister(uint8_t reg, uint8_t value) {
    I2C_SENS.beginTransmission(BMP280_ADDR);
    I2C_SENS.write(reg);
    I2C_SENS.write(value);
    return (I2C_SENS.endTransmission() == 0);
}

uint8_t bmpReadRegister(uint8_t reg) {
    I2C_SENS.beginTransmission(BMP280_ADDR);
    I2C_SENS.write(reg);
    I2C_SENS.endTransmission(false);
    I2C_SENS.requestFrom((uint8_t)BMP280_ADDR, (uint8_t)1);
    if (I2C_SENS.available()) {
        return I2C_SENS.read();
    }
    return 0xFF;
}

void bmpReadCalibration() {
    // Read 26 bytes of calibration data starting at 0x88
    uint8_t calData[26];
    I2C_SENS.beginTransmission(BMP280_ADDR);
    I2C_SENS.write(BMP280_REG_CALIB_START);
    I2C_SENS.endTransmission(false);
    I2C_SENS.requestFrom((uint8_t)BMP280_ADDR, (uint8_t)26);

    for (int i = 0; i < 26 && I2C_SENS.available(); i++) {
        calData[i] = I2C_SENS.read();
    }

    // Parse calibration coefficients (little-endian format)
    bmpCalib.dig_T1 = (uint16_t)(calData[1]  << 8 | calData[0]);
    bmpCalib.dig_T2 = (int16_t)(calData[3]   << 8 | calData[2]);
    bmpCalib.dig_T3 = (int16_t)(calData[5]   << 8 | calData[4]);
    bmpCalib.dig_P1 = (uint16_t)(calData[7]  << 8 | calData[6]);
    bmpCalib.dig_P2 = (int16_t)(calData[9]   << 8 | calData[8]);
    bmpCalib.dig_P3 = (int16_t)(calData[11]  << 8 | calData[10]);
    bmpCalib.dig_P4 = (int16_t)(calData[13]  << 8 | calData[12]);
    bmpCalib.dig_P5 = (int16_t)(calData[15]  << 8 | calData[14]);
    bmpCalib.dig_P6 = (int16_t)(calData[17]  << 8 | calData[16]);
    bmpCalib.dig_P7 = (int16_t)(calData[19]  << 8 | calData[18]);
    bmpCalib.dig_P8 = (int16_t)(calData[21]  << 8 | calData[20]);
    bmpCalib.dig_P9 = (int16_t)(calData[23]  << 8 | calData[22]);

    statusPrint("BMP280 calibration data loaded");
}

void bmpReadRaw(int32_t &rawTemp, int32_t &rawPress) {
    // Read 6 bytes: pressure MSB, LSB, XLSB, temp MSB, LSB, XLSB
    uint8_t data[6];
    I2C_SENS.beginTransmission(BMP280_ADDR);
    I2C_SENS.write(BMP280_REG_PRESS_MSB);
    I2C_SENS.endTransmission(false);
    I2C_SENS.requestFrom((uint8_t)BMP280_ADDR, (uint8_t)6);

    for (int i = 0; i < 6 && I2C_SENS.available(); i++) {
        data[i] = I2C_SENS.read();
    }

    // 20-bit raw pressure
    rawPress = ((int32_t)data[0] << 12) | ((int32_t)data[1] << 4) | ((int32_t)data[2] >> 4);
    // 20-bit raw temperature
    rawTemp  = ((int32_t)data[3] << 12) | ((int32_t)data[4] << 4) | ((int32_t)data[5] >> 4);
}

float bmpCompensateTemperature(int32_t rawTemp) {
    // BMP280 datasheet compensation formula
    int32_t var1, var2;

    var1 = ((((rawTemp >> 3) - ((int32_t)bmpCalib.dig_T1 << 1))) *
            ((int32_t)bmpCalib.dig_T2)) >> 11;

    var2 = (((((rawTemp >> 4) - ((int32_t)bmpCalib.dig_T1)) *
              ((rawTemp >> 4) - ((int32_t)bmpCalib.dig_T1))) >> 12) *
            ((int32_t)bmpCalib.dig_T3)) >> 14;

    bmpCalib.t_fine = var1 + var2;
    float T = (bmpCalib.t_fine * 5 + 128) >> 8;
    return T / 100.0f;  // Returns °C
}

float bmpCompensatePressure(int32_t rawPress) {
    // Must call bmpCompensateTemperature first to set t_fine!
    int64_t var1, var2, p;

    var1 = ((int64_t)bmpCalib.t_fine) - 128000;
    var2 = var1 * var1 * (int64_t)bmpCalib.dig_P6;
    var2 = var2 + ((var1 * (int64_t)bmpCalib.dig_P5) << 17);
    var2 = var2 + (((int64_t)bmpCalib.dig_P4) << 35);
    var1 = ((var1 * var1 * (int64_t)bmpCalib.dig_P3) >> 8) +
           ((var1 * (int64_t)bmpCalib.dig_P2) << 12);
    var1 = (((((int64_t)1) << 47) + var1)) * ((int64_t)bmpCalib.dig_P1) >> 33;

    if (var1 == 0) {
        return 0;  // Avoid division by zero
    }

    p = 1048576 - rawPress;
    p = (((p << 31) - var2) * 3125) / var1;
    var1 = (((int64_t)bmpCalib.dig_P9) * (p >> 13) * (p >> 13)) >> 25;
    var2 = (((int64_t)bmpCalib.dig_P8) * p) >> 19;

    p = ((p + var1 + var2) >> 8) + (((int64_t)bmpCalib.dig_P7) << 4);
    return (float)p / 25600.0f;  // Returns hPa
}

void bmpUpdate() {
    int32_t rawTemp, rawPress;
    bmpReadRaw(rawTemp, rawPress);

    currentTemperature = bmpCompensateTemperature(rawTemp);
    currentPressure    = bmpCompensatePressure(rawPress);

    // Compute deviation from baseline
    if (baselineSet) {
        pressureDeviation = currentPressure - baselinePressure;
        // Gauge pressure in Pa (for submersion detection)
        gaugePressurePa = pressureDeviation * 100.0f;  // hPa → Pa

        // Estimate submersion depth from pressure
        if (gaugePressurePa > threshSubmersionPa) {
            estimatedDepthCm = (gaugePressurePa / (WATER_DENSITY_KGM3 * GRAVITY_MS2)) * 100.0f;
        } else {
            estimatedDepthCm = 0.0f;
        }
    }
}


// ============================================================================
// ============================================================================
//                       DS1307 RTC OPERATIONS
// ============================================================================
// ============================================================================

uint8_t bcdToDec(uint8_t val) {
    return ((val >> 4) * 10) + (val & 0x0F);
}

uint8_t decToBcd(uint8_t val) {
    return ((val / 10) << 4) | (val % 10);
}

void rtcReadTime() {
    if (!rtcAvailable) return;

    I2C_SENS.beginTransmission(DS1307_ADDR);
    I2C_SENS.write(DS1307_REG_SECONDS);
    if (I2C_SENS.endTransmission() != 0) {
        rtcTimeValid = false;
        return;
    }

    I2C_SENS.requestFrom((uint8_t)DS1307_ADDR, (uint8_t)7);
    if (I2C_SENS.available() < 7) {
        rtcTimeValid = false;
        return;
    }

    uint8_t secReg = I2C_SENS.read();
    rtcSeconds = bcdToDec(secReg & 0x7F);  // Mask out clock halt bit
    rtcMinutes = bcdToDec(I2C_SENS.read() & 0x7F);
    uint8_t hourReg = I2C_SENS.read();
    // Handle 12/24 hour mode
    if (hourReg & 0x40) {
        // 12-hour mode
        rtcHours = bcdToDec(hourReg & 0x1F);
        if (hourReg & 0x20) rtcHours += 12;  // PM
    } else {
        // 24-hour mode
        rtcHours = bcdToDec(hourReg & 0x3F);
    }
    I2C_SENS.read();  // Day of week (skip)
    rtcDay   = bcdToDec(I2C_SENS.read() & 0x3F);
    rtcMonth = bcdToDec(I2C_SENS.read() & 0x1F);
    rtcYear  = 2000 + bcdToDec(I2C_SENS.read());

    // Validate
    rtcTimeValid = (rtcSeconds < 60 && rtcMinutes < 60 && rtcHours < 24 &&
                    rtcDay >= 1 && rtcDay <= 31 && rtcMonth >= 1 && rtcMonth <= 12 &&
                    rtcYear >= 2024 && rtcYear <= 2099);

    if (rtcTimeValid) {
        rtcUnixTime = rtcToUnixTime(rtcYear, rtcMonth, rtcDay,
                                     rtcHours, rtcMinutes, rtcSeconds);
    }
}

uint32_t rtcToUnixTime(uint16_t year, uint8_t month, uint8_t day,
                        uint8_t hour, uint8_t min, uint8_t sec) {
    // Simple Unix timestamp calculation
    // Days from 1970-01-01 to start of each month (non-leap year)
    static const uint16_t monthDays[] = {0, 31, 59, 90, 120, 151, 181, 212, 243, 273, 304, 334};

    uint32_t y = year;
    uint32_t m = month;
    uint32_t d = day;

    // Years since 1970
    uint32_t days = 0;
    for (uint32_t yr = 1970; yr < y; yr++) {
        days += 365;
        if ((yr % 4 == 0 && yr % 100 != 0) || (yr % 400 == 0)) {
            days += 1;  // Leap year
        }
    }

    // Add months
    if (m > 0 && m <= 12) {
        days += monthDays[m - 1];
    }
    // Add leap day if past February in a leap year
    if (m > 2 && ((y % 4 == 0 && y % 100 != 0) || (y % 400 == 0))) {
        days += 1;
    }

    // Add days
    days += (d - 1);

    uint32_t timestamp = days * 86400UL + hour * 3600UL + min * 60UL + sec;
    return timestamp;
}

void rtcFormatDateTime() {
    if (rtcTimeValid) {
        snprintf(dateTimeStr, sizeof(dateTimeStr), "%04d-%02d-%02d %02d:%02d:%02d",
                 rtcYear, rtcMonth, rtcDay, rtcHours, rtcMinutes, rtcSeconds);
    } else if (gps.timeValid && gps.dateValid) {
        // Fallback to GPS time
        snprintf(dateTimeStr, sizeof(dateTimeStr), "%04d-%02d-%02d %02d:%02d:%02d",
                 gps.year, gps.month, gps.day, gps.hour, gps.minute, gps.second);
        rtcUnixTime = rtcToUnixTime(gps.year, gps.month, gps.day,
                                     gps.hour, gps.minute, gps.second);
    } else {
        snprintf(dateTimeStr, sizeof(dateTimeStr), "0000-00-00 00:00:00");
    }
}


// ============================================================================
// ============================================================================
//                       GPS PARSING
// ============================================================================
// ============================================================================

void gpsProcessIncoming() {
    while (Serial1.available()) {
        char c = Serial1.read();

        if (c == '\n' || c == '\r') {
            if (gpsBufferIdx > 0) {
                gpsBuffer[gpsBufferIdx] = '\0';
                gpsParseNMEA(gpsBuffer);
                gpsBufferIdx = 0;
            }
        } else {
            if (gpsBufferIdx < GPS_BUFFER_SIZE - 1) {
                gpsBuffer[gpsBufferIdx++] = c;
            } else {
                gpsBufferIdx = 0;  // Buffer overflow — reset
            }
        }

        // If we hadn't detected GPS before but now we're getting data
        if (!gpsAvailable) {
            gpsAvailable = true;
            statusPrint("GPS module detected — receiving NMEA data");
        }
    }
}

void gpsParseNMEA(const char* sentence) {
    if (sentence[0] != '$') return;

    // Verify checksum
    int len = strlen(sentence);
    int starPos = -1;
    for (int i = len - 1; i >= 0; i--) {
        if (sentence[i] == '*') {
            starPos = i;
            break;
        }
    }

    if (starPos > 0 && starPos + 2 < len) {
        uint8_t computedChecksum = 0;
        for (int i = 1; i < starPos; i++) {
            computedChecksum ^= sentence[i];
        }
        char checksumStr[3] = {sentence[starPos + 1], sentence[starPos + 2], '\0'};
        uint8_t receivedChecksum = (uint8_t)strtol(checksumStr, NULL, 16);

        if (computedChecksum != receivedChecksum) {
            return;  // Checksum mismatch — discard
        }
    }

    // Route to appropriate parser
    if (strncmp(sentence + 3, "GGA", 3) == 0) {
        gpsParseGGA(sentence);
    } else if (strncmp(sentence + 3, "RMC", 3) == 0) {
        gpsParseRMC(sentence);
    }
}

void gpsParseGGA(const char* sentence) {
    // $GPGGA,hhmmss.ss,ddmm.mmmmm,N,dddmm.mmmmm,E,q,ss,hdop,alt,M,geoid,M,,*cs
    char fields[NMEA_MAX_FIELDS][20];
    int numFields = nmeaSplitFields(sentence, fields, NMEA_MAX_FIELDS);

    if (numFields < 12) return;

    // Field 1: Time (hhmmss.ss)
    if (strlen(fields[1]) >= 6) {
        gps.hour   = (fields[1][0] - '0') * 10 + (fields[1][1] - '0');
        gps.minute = (fields[1][2] - '0') * 10 + (fields[1][3] - '0');
        gps.second = (fields[1][4] - '0') * 10 + (fields[1][5] - '0');
        gps.timeValid = true;
    }

    // Field 2,3: Latitude
    if (strlen(fields[2]) > 0 && strlen(fields[3]) > 0) {
        gps.latitude = nmeaToDecimalDegrees(fields[2], fields[3]);
    }

    // Field 4,5: Longitude
    if (strlen(fields[4]) > 0 && strlen(fields[5]) > 0) {
        gps.longitude = nmeaToDecimalDegrees(fields[4], fields[5]);
    }

    // Field 6: Fix quality
    if (strlen(fields[6]) > 0) {
        gps.fixQuality = atoi(fields[6]);
        gps.fixValid = (gps.fixQuality > 0);
    }

    // Field 7: Number of satellites
    if (strlen(fields[7]) > 0) {
        gps.satellites = atoi(fields[7]);
    }

    // Field 9: Altitude
    if (strlen(fields[9]) > 0) {
        gps.altitude = atof(fields[9]);
    }

    gps.lastUpdateMs = millis();
}

void gpsParseRMC(const char* sentence) {
    // $GPRMC,hhmmss.ss,A,ddmm.mmmmm,N,dddmm.mmmmm,E,spd,crs,ddmmyy,mv,mvE*cs
    char fields[NMEA_MAX_FIELDS][20];
    int numFields = nmeaSplitFields(sentence, fields, NMEA_MAX_FIELDS);

    if (numFields < 10) return;

    // Field 1: Time
    if (strlen(fields[1]) >= 6) {
        gps.hour   = (fields[1][0] - '0') * 10 + (fields[1][1] - '0');
        gps.minute = (fields[1][2] - '0') * 10 + (fields[1][3] - '0');
        gps.second = (fields[1][4] - '0') * 10 + (fields[1][5] - '0');
        gps.timeValid = true;
    }

    // Field 2: Status (A=active/valid, V=void)
    if (strlen(fields[2]) > 0) {
        gps.fixValid = (fields[2][0] == 'A');
    }

    // Field 3,4: Latitude
    if (strlen(fields[3]) > 0 && strlen(fields[4]) > 0) {
        gps.latitude = nmeaToDecimalDegrees(fields[3], fields[4]);
    }

    // Field 5,6: Longitude
    if (strlen(fields[5]) > 0 && strlen(fields[6]) > 0) {
        gps.longitude = nmeaToDecimalDegrees(fields[5], fields[6]);
    }

    // Field 9: Date (ddmmyy)
    if (strlen(fields[9]) >= 6) {
        gps.day   = (fields[9][0] - '0') * 10 + (fields[9][1] - '0');
        gps.month = (fields[9][2] - '0') * 10 + (fields[9][3] - '0');
        gps.year  = 2000 + (fields[9][4] - '0') * 10 + (fields[9][5] - '0');
        gps.dateValid = true;
    }

    gps.lastUpdateMs = millis();
}

float nmeaToDecimalDegrees(const char* nmeaCoord, const char* hemisphere) {
    // NMEA format: ddmm.mmmmm or dddmm.mmmmm
    float raw = atof(nmeaCoord);
    int degrees = (int)(raw / 100);
    float minutes = raw - (degrees * 100);
    float decimal = degrees + (minutes / 60.0f);

    if (hemisphere[0] == 'S' || hemisphere[0] == 'W') {
        decimal = -decimal;
    }
    return decimal;
}

int nmeaSplitFields(const char* sentence, char fields[][20], int maxFields) {
    int fieldIdx = 0;
    int charIdx = 0;

    for (int i = 0; sentence[i] != '\0' && sentence[i] != '*' && fieldIdx < maxFields; i++) {
        if (sentence[i] == ',') {
            fields[fieldIdx][charIdx] = '\0';
            fieldIdx++;
            charIdx = 0;
        } else {
            if (charIdx < 19) {
                fields[fieldIdx][charIdx++] = sentence[i];
            }
        }
    }
    // Terminate last field
    if (fieldIdx < maxFields) {
        fields[fieldIdx][charIdx] = '\0';
        fieldIdx++;
    }
    return fieldIdx;
}


// ============================================================================
// ============================================================================
//                       BATTERY MONITORING
// ============================================================================
// ============================================================================

void readBattery() {
    // 16-sample averaging for stable ADC reading
    uint32_t adcSum = 0;
    for (int i = 0; i < 16; i++) {
        adcSum += analogRead(BATTERY_ADC_PIN);
        delayMicroseconds(100);
    }
    float adcAvg = (float)adcSum / 16.0f;

    // Convert ADC to voltage
    // Assuming voltage divider: battery → R1 → ADC_PIN → R2 → GND
    // With R1 = R2 = 100k: Vadc = Vbat / 2
    // ESP32 ADC: 12-bit (0-4095), range 0-3.3V with 11dB attenuation
    float adcVoltage = (adcAvg / 4095.0f) * 3.3f;
    batteryVoltage = adcVoltage * 2.0f;  // Voltage divider factor

    // LiPo percentage (3.0V = 0%, 4.2V = 100%)
    if (batteryVoltage >= 4.2f) {
        batteryPercent = 100.0f;
    } else if (batteryVoltage <= 3.0f) {
        batteryPercent = 0.0f;
    } else {
        batteryPercent = ((batteryVoltage - 3.0f) / 1.2f) * 100.0f;
    }
}


// ============================================================================
// ============================================================================
//                       CALIBRATION ROUTINES
// ============================================================================
// ============================================================================

void recalibrate() {
    statusPrint("CALIBRATION: Starting full sensor calibration");
    statusPrint("CALIBRATION: Device must be stationary and floating freely");

    calibrateGyro();
    calibrateAccel();

    calibrated = true;

    char msg[128];
    snprintf(msg, sizeof(msg),
             "CALIBRATION COMPLETE: GyroOffset=(%.3f, %.3f, %.3f) RefTilt=(%.2f, %.2f)",
             gyroOffsetX, gyroOffsetY, gyroOffsetZ, refTiltX, refTiltY);
    statusPrint(msg);
}

void calibrateGyro() {
    statusPrint("CALIBRATION: Sampling gyroscope offsets (1000 samples)...");

    double sumX = 0, sumY = 0, sumZ = 0;
    int validSamples = 0;

    for (int i = 0; i < GYRO_CAL_SAMPLES; i++) {
        mpuReadSensorData();
        float gx = (float)rawGyroX / GYRO_SENSITIVITY;
        float gy = (float)rawGyroY / GYRO_SENSITIVITY;
        float gz = (float)rawGyroZ / GYRO_SENSITIVITY;

        // Reject outliers (should be near zero at rest)
        if (fabsf(gx) < 20.0f && fabsf(gy) < 20.0f && fabsf(gz) < 20.0f) {
            sumX += gx;
            sumY += gy;
            sumZ += gz;
            validSamples++;
        }

        delay(2);  // ~500Hz sampling during calibration
    }

    if (validSamples > 100) {
        gyroOffsetX = sumX / validSamples;
        gyroOffsetY = sumY / validSamples;
        gyroOffsetZ = sumZ / validSamples;
    } else {
        errorPrint("CALIBRATION: Too few valid gyro samples");
        gyroOffsetX = 0;
        gyroOffsetY = 0;
        gyroOffsetZ = 0;
    }

    char msg[128];
    snprintf(msg, sizeof(msg), "CALIBRATION: Gyro offsets = (%.4f, %.4f, %.4f) °/s from %d samples",
             gyroOffsetX, gyroOffsetY, gyroOffsetZ, validSamples);
    statusPrint(msg);
}

void calibrateAccel() {
    statusPrint("CALIBRATION: Sampling accelerometer reference (500 samples)...");

    double sumX = 0, sumY = 0, sumZ = 0;
    int validSamples = 0;

    for (int i = 0; i < ACCEL_CAL_SAMPLES; i++) {
        mpuReadSensorData();
        float ax = (float)rawAccX / ACCEL_SENSITIVITY;
        float ay = (float)rawAccY / ACCEL_SENSITIVITY;
        float az = (float)rawAccZ / ACCEL_SENSITIVITY;

        // Check that magnitude is close to 1g (valid gravity reading)
        float mag = sqrtf(ax * ax + ay * ay + az * az);
        if (mag >= ACCEL_G_LOW && mag <= ACCEL_G_HIGH) {
            sumX += ax;
            sumY += ay;
            sumZ += az;
            validSamples++;
        }

        delay(4);  // ~250Hz during calibration
    }

    if (validSamples > 50) {
        refAccX = sumX / validSamples;
        refAccY = sumY / validSamples;
        refAccZ = sumZ / validSamples;

        // Compute reference tilt angles from gravity direction
        refTiltX = atan2f(refAccY, sqrtf(refAccX * refAccX + refAccZ * refAccZ)) * 180.0f / M_PI;
        refTiltY = atan2f(-refAccX, sqrtf(refAccY * refAccY + refAccZ * refAccZ)) * 180.0f / M_PI;

        // Initialize the complementary filter with the reference angles
        filtTiltX = refTiltX;
        filtTiltY = refTiltY;
    } else {
        errorPrint("CALIBRATION: Too few valid accel samples — using defaults");
        refAccX = 0.0f;
        refAccY = 0.0f;
        refAccZ = 1.0f;
        refTiltX = 0.0f;
        refTiltY = 0.0f;
    }

    char msg[128];
    snprintf(msg, sizeof(msg), "CALIBRATION: RefAcc=(%.4f, %.4f, %.4f) RefTilt=(%.2f, %.2f) from %d samples",
             refAccX, refAccY, refAccZ, refTiltX, refTiltY, validSamples);
    statusPrint(msg);
}

void calibratePressureBaseline() {
    statusPrint("CALIBRATION: Sampling pressure baseline (50 samples)...");

    double sumP = 0;
    int validSamples = 0;

    for (int i = 0; i < PRESSURE_CAL_SAMPLES; i++) {
        int32_t rawT, rawP;
        bmpReadRaw(rawT, rawP);
        float temp = bmpCompensateTemperature(rawT);
        float pres = bmpCompensatePressure(rawP);

        // Sanity check: atmospheric pressure should be 300-1100 hPa
        if (pres > 300.0f && pres < 1100.0f) {
            sumP += pres;
            validSamples++;
        }

        delay(50);
    }

    if (validSamples > 10) {
        baselinePressure = sumP / validSamples;
        baselineSet = true;

        char msg[64];
        snprintf(msg, sizeof(msg), "CALIBRATION: Pressure baseline = %.2f hPa from %d samples",
                 baselinePressure, validSamples);
        statusPrint(msg);
    } else {
        errorPrint("CALIBRATION: Too few valid pressure samples");
        baselineSet = false;
    }
}


// ============================================================================
// ============================================================================
//                       SENSOR FUSION — COMPLEMENTARY FILTER
// ============================================================================
// ============================================================================

void runSensorFusion() {
    unsigned long nowUs = micros();
    float dt = (float)(nowUs - lastFusionUs) / 1000000.0f;

    // Clamp dt to prevent huge jumps (e.g., after pause/debug)
    if (dt <= 0.0f || dt > 0.1f) {
        dt = 0.01f;  // Default to expected 10ms
    }

    // ---- Accelerometer-derived angles ----
    // Tilt X = rotation around X axis = depends on Y and Z accelerometer
    float accelTiltX = atan2f(accYg, sqrtf(accXg * accXg + accZg * accZg)) * 180.0f / M_PI;

    // Tilt Y = rotation around Y axis = depends on X and Z accelerometer
    float accelTiltY = atan2f(-accXg, sqrtf(accYg * accYg + accZg * accZg)) * 180.0f / M_PI;

    // ---- Complementary filter ----
    // Gyro integration for short-term (98%), accel correction for long-term (2%)
    filtTiltX = ALPHA * (filtTiltX + gyroXdps * dt) + (1.0f - ALPHA) * accelTiltX;
    filtTiltY = ALPHA * (filtTiltY + gyroYdps * dt) + (1.0f - ALPHA) * accelTiltY;

    // ---- Corrected tilt (subtract reference/zero) ----
    correctedTiltX = filtTiltX - refTiltX;
    correctedTiltY = filtTiltY - refTiltY;

    // ---- Combined angle from vertical ----
    combinedTheta = sqrtf(correctedTiltX * correctedTiltX + correctedTiltY * correctedTiltY);
}


// ============================================================================
// ============================================================================
//                       FLOOD DETECTION ENGINE
// ============================================================================
// ============================================================================

void computeWaterHeight() {
    if (currentMode == MODE_SUBMERGED) {
        // In submersion mode, height = tether length + depth from pressure
        waterHeightCm = olpLengthCm + estimatedDepthCm;
    } else if (currentMode == MODE_TAUT || currentMode == MODE_FLOOD) {
        // Geometric computation: H = L × cos(θ)
        float thetaRad = combinedTheta * M_PI / 180.0f;
        waterHeightCm = olpLengthCm * cosf(thetaRad);

        // Compute horizontal displacement too (for reference)
        horizontalDistCm = olpLengthCm * sinf(thetaRad);
    } else {
        // MODE_SLACK: tether is slack, water is below threshold
        // We cannot compute exact height — report 0 or last known
        waterHeightCm = 0.0f;
        horizontalDistCm = 0.0f;
    }

    // Clamp to reasonable range
    if (waterHeightCm < 0.0f) waterHeightCm = 0.0f;
}

void computeFloodRatio() {
    if (hMaxCm > 0.0f) {
        floodRatio = waterHeightCm / hMaxCm;
    } else {
        floodRatio = 0.0f;
    }
    if (floodRatio > 9.99f) floodRatio = 9.99f;  // Clamp for display
}

void classifyFloodMode() {
    FloodMode detected = MODE_SLACK;  // Default assumption

    // ---- Check submersion first (highest priority) ----
    if (bmpAvailable && baselineSet && gaugePressurePa > threshSubmersionPa) {
        detected = MODE_SUBMERGED;
    }
    // ---- Check if tether is taut ----
    else if (lateralAccel > threshLateralAccelTaut && combinedTheta > threshTiltTautDeg) {
        // Tether is taut — now determine if flood or approaching
        if (combinedTheta < threshFloodThetaDeg && floodRatio > threshFloodRatio) {
            detected = MODE_FLOOD;
        } else {
            detected = MODE_TAUT;
        }
    }
    // ---- Check hysteresis for going back to slack ----
    else if (currentMode == MODE_TAUT || currentMode == MODE_FLOOD) {
        // Apply hysteresis: need lateral_accel to drop below slack threshold
        if (lateralAccel > threshLateralAccelSlack || combinedTheta > threshTiltTautDeg) {
            // Still in taut zone — don't drop to slack yet
            if (combinedTheta < threshFloodThetaDeg && floodRatio > threshFloodRatio) {
                detected = MODE_FLOOD;
            } else {
                detected = MODE_TAUT;
            }
        } else {
            detected = MODE_SLACK;  // Truly going slack
        }
    }
    else {
        detected = MODE_SLACK;
    }

    // ---- Apply persistence filter ----
    updateModePersistence(detected);
}

void updateModePersistence(FloodMode detected) {
    if (detected == pendingMode) {
        modePersistCount++;
    } else {
        pendingMode = detected;
        modePersistCount = 1;
    }

    // Determine required persistence count based on urgency
    int requiredCount;
    if (detected == MODE_SUBMERGED) {
        requiredCount = SUBMERGE_PERSISTENCE_COUNT;  // 3 — urgent
    } else {
        requiredCount = MODE_PERSISTENCE_COUNT;  // 10 — normal
    }

    if (modePersistCount >= requiredCount) {
        if (currentMode != detected) {
            char msg[128];
            snprintf(msg, sizeof(msg), "MODE TRANSITION: %d → %d (after %d readings)",
                     currentMode, detected, modePersistCount);
            statusPrint(msg);
        }
        currentMode = detected;
    }
}

void computeAlertLevel() {
    switch (currentMode) {
        case MODE_SLACK:
            floodAlertLevel = ALERT_GREEN;
            break;
        case MODE_TAUT:
            if (floodRatio > 0.80f) {
                floodAlertLevel = ALERT_YELLOW;
            } else {
                floodAlertLevel = ALERT_GREEN;
            }
            break;
        case MODE_FLOOD:
            floodAlertLevel = ALERT_RED;
            break;
        case MODE_SUBMERGED:
            floodAlertLevel = ALERT_BLACK;
            break;
    }
}

void computeZone() {
    if (currentMode == MODE_SUBMERGED || floodRatio > 0.95f) {
        currentZone = ZONE_CRITICAL;
    } else if (floodRatio > 0.80f) {
        currentZone = ZONE_WARNING;
    } else if (floodRatio > 0.50f) {
        currentZone = ZONE_WATCH;
    } else {
        currentZone = ZONE_SAFE;
    }
}

void computeResponseLevel() {
    switch (currentMode) {
        case MODE_SLACK:
            currentResponse = RESPONSE_NONE;
            break;
        case MODE_TAUT:
            if (floodRatio > 0.80f) {
                currentResponse = RESPONSE_PREPARE;
            } else if (floodRatio > 0.50f) {
                currentResponse = RESPONSE_MONITOR;
            } else {
                currentResponse = RESPONSE_NONE;
            }
            break;
        case MODE_FLOOD:
            currentResponse = RESPONSE_ACT;
            break;
        case MODE_SUBMERGED:
            currentResponse = RESPONSE_EVACUATE;
            break;
    }
}


// ============================================================================
// ============================================================================
//                       DYNAMIC SAMPLING RATE
// ============================================================================
// ============================================================================

void computeDynamicSamplingRate() {
    // Based on flood ratio, interpolate between normal and high rate
    //
    // floodRatio < RATE_INTERP_LOW (0.50)  → normalRateSec
    // floodRatio > RATE_INTERP_HIGH (0.80) → highRateSec
    // Between → linear interpolation
    //
    // This conserves battery during safe conditions while providing
    // dense data as flood approaches.

    if (floodRatio <= RATE_INTERP_LOW) {
        currentIntervalSec = normalRateSec;
    } else if (floodRatio >= RATE_INTERP_HIGH) {
        currentIntervalSec = highRateSec;
    } else {
        // Linear interpolation
        float fraction = (floodRatio - RATE_INTERP_LOW) / (RATE_INTERP_HIGH - RATE_INTERP_LOW);
        currentIntervalSec = normalRateSec - (int)(fraction * (float)(normalRateSec - highRateSec));
    }

    // Clamp
    if (currentIntervalSec < 1) currentIntervalSec = 1;
    if (currentIntervalSec > 86400) currentIntervalSec = 86400;
}


// ============================================================================
// ============================================================================
//                       SESSION STATISTICS
// ============================================================================
// ============================================================================

void updateSessionStats() {
    unsigned long nowMs = millis();

    // Track peak and minimum heights
    if (waterHeightCm > peakHeightCm) {
        peakHeightCm = waterHeightCm;
    }
    if (waterHeightCm < minHeightCm && waterHeightCm > 0.01f) {
        minHeightCm = waterHeightCm;
    }

    // Compute rate of rise (cm per 15 minutes)
    unsigned long dtMs = nowMs - prevHeightTimeMs;
    if (dtMs > 0 && prevHeightTimeMs > 0) {
        float dtMin = (float)dtMs / 60000.0f;
        if (dtMin > 0.1f) {  // At least 6 seconds between measurements
            float risePerMin = (waterHeightCm - prevHeightCm) / dtMin;
            rateOfRiseCmPer15Min = risePerMin * 15.0f;

            // Track sustained rise
            if (rateOfRiseCmPer15Min > 1.0f) {  // Rising more than 1cm/15min
                riseConsecutive++;
                if (riseConsecutive >= 3) {
                    sustainedRise = true;
                }
            } else {
                riseConsecutive = 0;
                sustainedRise = false;
            }
        }
    }

    prevHeightCm = waterHeightCm;
    prevHeightTimeMs = nowMs;
}


// ============================================================================
// ============================================================================
//                       HEALTH SCORE
// ============================================================================
// ============================================================================

int computeHealthScore() {
    int score = 0;

    // MPU6050: 40 points (most critical — core measurement)
    if (mpuAvailable) score += 40;

    // BMP280: 30 points (pressure/submersion detection)
    if (bmpAvailable) score += 30;

    // DS1307 RTC: 20 points (timestamp accuracy)
    if (rtcAvailable && rtcTimeValid) score += 20;

    // GPS: 10 points (location data)
    if (gpsAvailable && gps.fixValid) score += 10;

    return score;
}


// ============================================================================
// ============================================================================
//                       DATA TRANSMISSION
// ============================================================================
// ============================================================================

void transmitData() {
    // Build the 39-field CSV string
    char csvBuffer[1024];
    buildCSVString(csvBuffer, sizeof(csvBuffer));

    // ---- Stream 1: USB Serial → debugger (115200 baud) ----
    Serial.println(csvBuffer);

    // ---- Stream 2: GPIO 14 Software UART → C3 (9600 baud) ----
    c3UartSendString(csvBuffer);
    c3UartSendByte('\r');
    c3UartSendByte('\n');
}

void buildCSVString(char* buffer, int bufSize) {
    // Build all 39 fields
    //
    // Field layout (see documentation):
    //  1: theta (deg)              2: waterHeight (cm)
    //  3: correctedTiltX (deg)     4: correctedTiltY (deg)
    //  5: olpLength (cm)           6: horizontalDist (cm)
    //  7: currentPressure (hPa)    8: currentTemperature (°C)
    //  9: baselinePressure (hPa)  10: pressureDeviation (hPa)
    // 11: submersionState (0-3)   12: estimatedDepth (cm)
    // 13: bmpAvailable (0/1)      14: unixTime
    // 15: dateTimeString          16: rtcValid (0/1)
    // 17: ratePer15Min (cm/15m)   18: floodAlertLevel (0-3)
    // 19: sessionDuration (sec)   20: peakHeight (cm)
    // 21: minHeight (cm)          22: latitude
    // 23: longitude               24: altitude (m)
    // 25: gpsSatellites           26: gpsFixValid (0/1)
    // 27: simSignalRSSI           28: simRegistered (0/1)
    // 29: simAvailable (0/1)      30: currentZone (0-3)
    // 31: currentResponseLevel    32: sustainedRise (0/1)
    // 33: batteryPercent          34: sampleInterval (sec)
    // 35: transmitInterval (sec)  36: obLightEnabled (0/1)
    // 37: algorithmEnabled (0/1)  38: currentMode (0-3)
    // 39: healthScore (0-100)

    unsigned long sessionSec = (millis() - sessionStartMs) / 1000UL;

    // Handle minHeight initial state
    float reportedMinHeight = (minHeightCm > 99990.0f) ? 0.0f : minHeightCm;

    snprintf(buffer, bufSize,
        "%.2f,"    // 1:  theta
        "%.2f,"    // 2:  waterHeight
        "%.2f,"    // 3:  correctedTiltX
        "%.2f,"    // 4:  correctedTiltY
        "%.2f,"    // 5:  olpLength
        "%.2f,"    // 6:  horizontalDist
        "%.2f,"    // 7:  currentPressure
        "%.2f,"    // 8:  currentTemperature
        "%.2f,"    // 9:  baselinePressure
        "%.2f,"    // 10: pressureDeviation
        "%d,"      // 11: submersionState (= currentMode)
        "%.2f,"    // 12: estimatedDepth
        "%d,"      // 13: bmpAvailable
        "%lu,"     // 14: unixTime
        "%s,"      // 15: dateTimeString
        "%d,"      // 16: rtcValid
        "%.3f,"    // 17: ratePer15Min
        "%d,"      // 18: floodAlertLevel
        "%lu,"     // 19: sessionDuration
        "%.2f,"    // 20: peakHeight
        "%.2f,"    // 21: minHeight
        "%.6f,"    // 22: latitude
        "%.6f,"    // 23: longitude
        "%.1f,"    // 24: altitude
        "%d,"      // 25: gpsSatellites
        "%d,"      // 26: gpsFixValid
        "%d,"      // 27: simSignalRSSI
        "%d,"      // 28: simRegistered
        "%d,"      // 29: simAvailable
        "%d,"      // 30: currentZone
        "%d,"      // 31: currentResponseLevel
        "%d,"      // 32: sustainedRise
        "%.1f,"    // 33: batteryPercent
        "%d,"      // 34: sampleInterval
        "%d,"      // 35: transmitInterval
        "%d,"      // 36: obLightEnabled
        "%d,"      // 37: algorithmEnabled
        "%d,"      // 38: currentMode
        "%d",      // 39: healthScore
        combinedTheta,              // 1
        waterHeightCm,              // 2
        correctedTiltX,             // 3
        correctedTiltY,             // 4
        olpLengthCm,                // 5
        horizontalDistCm,           // 6
        currentPressure,            // 7
        currentTemperature,         // 8
        baselinePressure,           // 9
        pressureDeviation,          // 10
        (int)currentMode,           // 11
        estimatedDepthCm,           // 12
        (int)bmpAvailable,          // 13
        rtcUnixTime,                // 14
        dateTimeStr,                // 15
        (int)(rtcAvailable && rtcTimeValid), // 16
        rateOfRiseCmPer15Min,       // 17
        (int)floodAlertLevel,       // 18
        sessionSec,                 // 19
        peakHeightCm,               // 20
        reportedMinHeight,          // 21
        gps.latitude,               // 22
        gps.longitude,              // 23
        gps.altitude,               // 24
        gps.satellites,             // 25
        (int)gps.fixValid,          // 26
        simSignalRSSI,              // 27
        (int)simRegistered,         // 28
        (int)simAvailable,          // 29
        (int)currentZone,           // 30
        (int)currentResponse,       // 31
        (int)sustainedRise,         // 32
        batteryPercent,             // 33
        currentIntervalSec,         // 34
        currentIntervalSec,         // 35 (same as sample interval for now)
        (int)obLightEnabled,        // 36
        (int)algorithmEnabled,      // 37
        (int)currentMode,           // 38
        healthScore                 // 39
    );
}


// ============================================================================
// ============================================================================
//                       SOFTWARE UART TX (GPIO 14 → C3)
// ============================================================================
// ============================================================================
//
// This implements a software UART transmitter at 9600 baud on GPIO 14.
// Each byte takes ~1.04ms to transmit (1 start + 8 data + 1 stop = 10 bits).
// Interrupts are disabled during each byte to ensure precise timing.
//
// This is used to send the 39-field CSV to the C3. The C3's hardware UART
// receives it and forwards to Firebase.
//

void c3UartSendByte(uint8_t b) {
    // Disable interrupts for precise bit timing
    portMUX_TYPE mux = portMUX_INITIALIZER_UNLOCKED;
    portENTER_CRITICAL(&mux);

    // Start bit (LOW)
    digitalWrite(C3_DATA_PIN, LOW);
    delayMicroseconds(SW_UART_BIT_TIME_US);

    // 8 data bits (LSB first)
    for (int i = 0; i < 8; i++) {
        if (b & (1 << i)) {
            digitalWrite(C3_DATA_PIN, HIGH);
        } else {
            digitalWrite(C3_DATA_PIN, LOW);
        }
        delayMicroseconds(SW_UART_BIT_TIME_US);
    }

    // Stop bit (HIGH)
    digitalWrite(C3_DATA_PIN, HIGH);
    delayMicroseconds(SW_UART_BIT_TIME_US);

    portEXIT_CRITICAL(&mux);

    // Small inter-byte gap to let C3 process
    delayMicroseconds(SW_UART_BIT_TIME_US / 2);
}

void c3UartSendString(const char* str) {
    while (*str) {
        c3UartSendByte((uint8_t)*str);
        str++;
    }
}


// ============================================================================
// ============================================================================
//                       COMMAND PROCESSING — FROM C3
// ============================================================================
// ============================================================================
//
// The C3 sends commands to the S3 on Serial2 (9600 baud, GPIO 43/44).
// Supported commands:
//   $CFG,normal_sec,high_sec,h_max_cm   — Update sampling config
//   $DIAGRUN                             — Trigger manual diagnostic
//   $PING                                — Connectivity check (respond $PONG)
//   $SIMSTAT,rssi,registered,available   — SIM status update from C3
//

static char c3CmdBuffer[256];
static int  c3CmdBufIdx = 0;

void processC3Commands() {
    while (Serial2.available()) {
        char c = Serial2.read();

        if (c == '\n' || c == '\r') {
            if (c3CmdBufIdx > 0) {
                c3CmdBuffer[c3CmdBufIdx] = '\0';
                handleC3Command(c3CmdBuffer);
                c3CmdBufIdx = 0;
            }
        } else {
            if (c3CmdBufIdx < (int)sizeof(c3CmdBuffer) - 1) {
                c3CmdBuffer[c3CmdBufIdx++] = c;
            } else {
                c3CmdBufIdx = 0;  // Buffer overflow reset
            }
        }
    }
}

void handleC3Command(const char* cmd) {
    char msg[256];

    // ---- $CFG,normal_sec,high_sec,h_max_cm ----
    if (strncmp(cmd, "$CFG,", 5) == 0) {
        int newNormal = 0, newHigh = 0;
        float newHMax = 0;

        int parsed = sscanf(cmd + 5, "%d,%d,%f", &newNormal, &newHigh, &newHMax);

        if (parsed >= 2) {
            // Validate ranges
            if (newNormal < 1)    newNormal = 1;
            if (newNormal > 86400) newNormal = 86400;
            if (newHigh < 1)      newHigh = 1;
            if (newHigh > 86400)  newHigh = 86400;
            if (newHigh > newNormal) newHigh = newNormal;

            normalRateSec = newNormal;
            highRateSec   = newHigh;

            snprintf(msg, sizeof(msg),
                     "STATUS:CFG updated — normalRate=%ds, highRate=%ds",
                     normalRateSec, highRateSec);
            statusPrint(msg);

            // Send acknowledgment back to C3
            Serial2.println("$CFG_ACK");
        }

        if (parsed >= 3 && newHMax > 0) {
            hMaxCm = newHMax;
            olpLengthCm = newHMax;  // OLP length = flood threshold height

            snprintf(msg, sizeof(msg), "STATUS:H_MAX updated to %.2f cm", hMaxCm);
            statusPrint(msg);
        }

        // Recompute dynamic sampling rate with new config
        computeDynamicSamplingRate();
    }

    // ---- $DIAGRUN ----
    else if (strcmp(cmd, "$DIAGRUN") == 0) {
        statusPrint("STATUS:Diagnostic requested by C3/server");
        diagRequested = true;
        Serial2.println("$DIAG_ACK");
    }

    // ---- $PING ----
    else if (strcmp(cmd, "$PING") == 0) {
        Serial2.println("$PONG");
    }

    // ---- $SIMSTAT,rssi,registered,available ----
    // C3 periodically forwards SIM module status so S3 can include in CSV
    else if (strncmp(cmd, "$SIMSTAT,", 9) == 0) {
        int rssi = 0, reg = 0, avail = 0;
        if (sscanf(cmd + 9, "%d,%d,%d", &rssi, &reg, &avail) == 3) {
            simSignalRSSI  = rssi;
            simRegistered  = (reg != 0);
            simAvailable   = (avail != 0);
        }
    }

    // ---- $SETFLAG,flag_name,value ----
    // For setting feature flags from the website
    else if (strncmp(cmd, "$SETFLAG,", 9) == 0) {
        char flagName[32];
        int flagValue = 0;
        if (sscanf(cmd + 9, "%31[^,],%d", flagName, &flagValue) == 2) {
            if (strcmp(flagName, "OB_LIGHT") == 0) {
                obLightEnabled = (flagValue != 0);
                snprintf(msg, sizeof(msg), "STATUS:OB_LIGHT set to %d", obLightEnabled);
                statusPrint(msg);
            } else if (strcmp(flagName, "ALGORITHM") == 0) {
                algorithmEnabled = (flagValue != 0);
                snprintf(msg, sizeof(msg), "STATUS:ALGORITHM set to %d", algorithmEnabled);
                statusPrint(msg);
            }
            Serial2.println("$FLAG_ACK");
        }
    }

    // ---- Unknown command ----
    else {
        snprintf(msg, sizeof(msg), "WARNING:Unknown C3 command: %s", cmd);
        warningPrint(msg);
    }
}


// ============================================================================
// ============================================================================
//                       COMMAND PROCESSING — FROM DEBUGGER
// ============================================================================
// ============================================================================
//
// The handheld debugger (RPi) connects via USB Serial at 115200 baud.
// Supported commands:
//   PING             → STATUS:PONG
//   GETCONFIG        → Current configuration values
//   RECALIBRATE      → Run full sensor calibration
//   GETTHRESH        → Current threshold values
//   SETTHRESH=A,W,D  → Set accel, warning, depth thresholds
//   RESETTHRESH      → Reset thresholds to defaults
//   SETAPN           → (placeholder — no SIM on S3)
//   REINITSIM        → (placeholder — no SIM on S3)
//   TESTGPRS         → (placeholder — no SIM on S3)
//   GETSTATUS        → Full system status dump
//

static char dbgCmdBuffer[256];
static int  dbgCmdBufIdx = 0;

void processDebuggerCommands() {
    while (Serial.available()) {
        char c = Serial.read();

        if (c == '\n' || c == '\r') {
            if (dbgCmdBufIdx > 0) {
                dbgCmdBuffer[dbgCmdBufIdx] = '\0';
                handleDebuggerCommand(dbgCmdBuffer);
                dbgCmdBufIdx = 0;
            }
        } else {
            if (dbgCmdBufIdx < (int)sizeof(dbgCmdBuffer) - 1) {
                dbgCmdBuffer[dbgCmdBufIdx++] = c;
            } else {
                dbgCmdBufIdx = 0;
            }
        }
    }
}

void handleDebuggerCommand(const char* cmd) {
    char msg[512];

    // ---- PING ----
    if (strcmp(cmd, "PING") == 0) {
        statusPrint("PONG");
    }

    // ---- GETCONFIG ----
    else if (strcmp(cmd, "GETCONFIG") == 0) {
        snprintf(msg, sizeof(msg),
                 "STATUS:CONFIG normalRate=%d highRate=%d hMax=%.2f olpLen=%.2f "
                 "mode=%d interval=%d healthScore=%d "
                 "MPU=%d BMP=%d RTC=%d GPS=%d calibrated=%d baselineSet=%d",
                 normalRateSec, highRateSec, hMaxCm, olpLengthCm,
                 currentMode, currentIntervalSec, healthScore,
                 mpuAvailable, bmpAvailable, rtcAvailable, gpsAvailable,
                 calibrated, baselineSet);
        statusPrint(msg);
    }

    // ---- RECALIBRATE ----
    else if (strcmp(cmd, "RECALIBRATE") == 0) {
        statusPrint("Recalibration starting — keep device stationary");
        if (mpuAvailable) {
            recalibrate();
            statusPrint("Recalibration complete");
        } else {
            errorPrint("MPU6050 not available — cannot recalibrate");
        }
        if (bmpAvailable) {
            calibratePressureBaseline();
        }
    }

    // ---- GETTHRESH ----
    else if (strcmp(cmd, "GETTHRESH") == 0) {
        snprintf(msg, sizeof(msg),
                 "STATUS:THRESH accelTaut=%.3f accelSlack=%.3f tiltTaut=%.1f "
                 "floodTheta=%.1f floodRatio=%.2f submersionPa=%.1f",
                 threshLateralAccelTaut, threshLateralAccelSlack,
                 threshTiltTautDeg, threshFloodThetaDeg,
                 threshFloodRatio, threshSubmersionPa);
        statusPrint(msg);
    }

    // ---- SETTHRESH=accelTaut,floodTheta,submersionPa ----
    else if (strncmp(cmd, "SETTHRESH=", 10) == 0) {
        float a = 0, w = 0, d = 0;
        if (sscanf(cmd + 10, "%f,%f,%f", &a, &w, &d) == 3) {
            if (a > 0.01f && a < 5.0f) {
                threshLateralAccelTaut = a;
                threshLateralAccelSlack = a * 0.67f;  // Hysteresis at 2/3
            }
            if (w > 0.1f && w < 90.0f) {
                threshFloodThetaDeg = w;
            }
            if (d > 10.0f && d < 100000.0f) {
                threshSubmersionPa = d;
            }
            snprintf(msg, sizeof(msg),
                     "STATUS:THRESH updated accelTaut=%.3f floodTheta=%.1f submersionPa=%.1f",
                     threshLateralAccelTaut, threshFloodThetaDeg, threshSubmersionPa);
            statusPrint(msg);
        } else {
            errorPrint("SETTHRESH format: SETTHRESH=accelTaut,floodTheta,submersionPa");
        }
    }

    // ---- RESETTHRESH ----
    else if (strcmp(cmd, "RESETTHRESH") == 0) {
        threshLateralAccelTaut  = DEFAULT_LATERAL_ACCEL_TAUT;
        threshLateralAccelSlack = DEFAULT_LATERAL_ACCEL_SLACK;
        threshTiltTautDeg       = DEFAULT_TILT_TAUT_DEG;
        threshFloodThetaDeg     = DEFAULT_FLOOD_THETA_DEG;
        threshFloodRatio        = DEFAULT_FLOOD_RATIO;
        threshSubmersionPa      = DEFAULT_SUBMERSION_PA;
        statusPrint("STATUS:THRESH reset to defaults");
    }

    // ---- SETHMAX=value_cm ----
    else if (strncmp(cmd, "SETHMAX=", 8) == 0) {
        float newH = atof(cmd + 8);
        if (newH > 1.0f && newH < 10000.0f) {
            hMaxCm = newH;
            olpLengthCm = newH;
            snprintf(msg, sizeof(msg), "STATUS:H_MAX set to %.2f cm (OLP length updated)", hMaxCm);
            statusPrint(msg);
            computeDynamicSamplingRate();
        } else {
            errorPrint("SETHMAX value must be between 1 and 10000 cm");
        }
    }

    // ---- SETRATE=normal_sec,high_sec ----
    else if (strncmp(cmd, "SETRATE=", 8) == 0) {
        int n = 0, h = 0;
        if (sscanf(cmd + 8, "%d,%d", &n, &h) == 2) {
            if (n >= 1 && n <= 86400 && h >= 1 && h <= 86400) {
                normalRateSec = n;
                highRateSec = h;
                if (highRateSec > normalRateSec) highRateSec = normalRateSec;
                computeDynamicSamplingRate();
                snprintf(msg, sizeof(msg), "STATUS:RATE set normal=%ds high=%ds current=%ds",
                         normalRateSec, highRateSec, currentIntervalSec);
                statusPrint(msg);
            }
        }
    }

    // ---- GETSTATUS ----
    else if (strcmp(cmd, "GETSTATUS") == 0) {
        snprintf(msg, sizeof(msg),
                 "STATUS:MODE=%d ALERT=%d ZONE=%d RESP=%d "
                 "H=%.2f THETA=%.2f RATIO=%.3f "
                 "P=%.2f DEPTH=%.2f "
                 "LAT_ACCEL=%.3f "
                 "BATT=%.1f%% "
                 "INTERVAL=%ds SUSTAINED_RISE=%d",
                 currentMode, floodAlertLevel, currentZone, currentResponse,
                 waterHeightCm, combinedTheta, floodRatio,
                 currentPressure, estimatedDepthCm,
                 lateralAccel,
                 batteryPercent,
                 currentIntervalSec, sustainedRise);
        statusPrint(msg);
    }

    // ---- SETAPN / REINITSIM / TESTGPRS — SIM commands (forwarded to C3) ----
    else if (strcmp(cmd, "SETAPN") == 0 || strcmp(cmd, "REINITSIM") == 0 ||
             strcmp(cmd, "TESTGPRS") == 0) {
        // These are SIM-related — S3 has no SIM. Forward to C3 via Serial2.
        Serial2.print("$");
        Serial2.println(cmd);
        snprintf(msg, sizeof(msg), "STATUS:Forwarded '%s' to C3 via Serial2", cmd);
        statusPrint(msg);
    }

    // ---- Unknown command ----
    else {
        snprintf(msg, sizeof(msg), "ERROR:Unknown command: %s", cmd);
        Serial.println(msg);
    }
}


// ============================================================================
// ============================================================================
//                       DIAGNOSTICS
// ============================================================================
// ============================================================================

void runDiagnostics() {
    statusPrint("DIAGNOSTIC: Starting full system diagnostic...");

    DiagResult diag;
    memset(&diag, 0, sizeof(diag));
    diag.totalFaults = 0;

    // ========================================================================
    // TEST 1: MPU6050 WHO_AM_I
    // ========================================================================
    {
        uint8_t whoAmI = mpuReadRegister(MPU_REG_WHO_AM_I);
        diag.mpuWhoAmIOk = (whoAmI == MPU_WHO_AM_I_VAL);
        if (!diag.mpuWhoAmIOk) diag.totalFaults++;
    }

    // ========================================================================
    // TEST 2: Accelerometer magnitude sanity
    // ========================================================================
    if (mpuAvailable) {
        mpuReadSensorData();
        mpuConvertToPhysical();
        diag.mpuAccelMagnitude = sqrtf(accXg * accXg + accYg * accYg + accZg * accZg);
        diag.mpuAccelMagnitudeOk = (diag.mpuAccelMagnitude > 0.8f && diag.mpuAccelMagnitude < 1.2f);
        if (!diag.mpuAccelMagnitudeOk) diag.totalFaults++;
    } else {
        diag.mpuAccelMagnitudeOk = false;
        diag.totalFaults++;
    }

    // ========================================================================
    // TEST 3: Gyroscope drift measurement
    // ========================================================================
    if (mpuAvailable) {
        // Sample gyro at rest, check if drift is within acceptable range
        double sumDrift = 0;
        int samples = 0;
        for (int i = 0; i < 100; i++) {
            mpuReadSensorData();
            mpuConvertToPhysical();
            sumDrift += sqrtf(gyroXdps * gyroXdps + gyroYdps * gyroYdps + gyroZdps * gyroZdps);
            samples++;
            delay(2);
        }
        diag.mpuGyroDrift = (float)(sumDrift / samples);
        diag.mpuGyroDriftOk = (diag.mpuGyroDrift < 5.0f);  // < 5°/s residual after offset
        if (!diag.mpuGyroDriftOk) diag.totalFaults++;
    } else {
        diag.mpuGyroDriftOk = false;
        diag.totalFaults++;
    }

    // ========================================================================
    // TEST 4: BMP280 Chip ID
    // ========================================================================
    {
        uint8_t chipId = bmpReadRegister(BMP280_REG_CHIP_ID);
        diag.bmpChipIdOk = (chipId == BMP280_CHIP_ID_VAL);
        if (!diag.bmpChipIdOk) diag.totalFaults++;
    }

    // ========================================================================
    // TEST 5: BMP280 pressure range
    // ========================================================================
    if (bmpAvailable) {
        bmpUpdate();
        diag.bmpPressure = currentPressure;
        diag.bmpPressureRangeOk = (currentPressure > 300.0f && currentPressure < 1200.0f);
        if (!diag.bmpPressureRangeOk) diag.totalFaults++;
    } else {
        diag.bmpPressureRangeOk = false;
        diag.totalFaults++;
    }

    // ========================================================================
    // TEST 6: BMP280 temperature range
    // ========================================================================
    if (bmpAvailable) {
        diag.bmpTemperature = currentTemperature;
        diag.bmpTempRangeOk = (currentTemperature > -40.0f && currentTemperature < 85.0f);
        if (!diag.bmpTempRangeOk) diag.totalFaults++;
    } else {
        diag.bmpTempRangeOk = false;
        diag.totalFaults++;
    }

    // ========================================================================
    // TEST 7: DS1307 oscillator running
    // ========================================================================
    if (rtcAvailable) {
        I2C_SENS.beginTransmission(DS1307_ADDR);
        I2C_SENS.write(DS1307_REG_SECONDS);
        I2C_SENS.endTransmission(false);
        I2C_SENS.requestFrom((uint8_t)DS1307_ADDR, (uint8_t)1);
        if (I2C_SENS.available()) {
            uint8_t secReg = I2C_SENS.read();
            diag.rtcOscRunning = !(secReg & 0x80);  // Bit 7 = clock halt
        } else {
            diag.rtcOscRunning = false;
        }
        if (!diag.rtcOscRunning) diag.totalFaults++;
    } else {
        diag.rtcOscRunning = false;
        diag.totalFaults++;
    }

    // ========================================================================
    // TEST 8: RTC time validity
    // ========================================================================
    if (rtcAvailable) {
        rtcReadTime();
        diag.rtcTimeValid = rtcTimeValid;
        if (!diag.rtcTimeValid) diag.totalFaults++;
    } else {
        diag.rtcTimeValid = false;
        diag.totalFaults++;
    }

    // ========================================================================
    // TEST 9: GPS data recency
    // ========================================================================
    {
        unsigned long gpsAge = millis() - gps.lastUpdateMs;
        diag.gpsDataRecent = (gps.lastUpdateMs > 0 && gpsAge < 10000);  // Within 10s
        diag.gpsSatCount = gps.satellites;
        if (!diag.gpsDataRecent) diag.totalFaults++;
    }

    // ========================================================================
    // TEST 10: Battery voltage range
    // ========================================================================
    {
        readBattery();
        diag.batteryVoltage = batteryVoltage;
        diag.batteryVoltageOk = (batteryVoltage > 2.8f && batteryVoltage < 4.3f);
        if (!diag.batteryVoltageOk) diag.totalFaults++;
    }

    // ========================================================================
    // TEST 11: C3 communication (PING/PONG)
    // ========================================================================
    {
        // Clear any pending data on Serial2
        while (Serial2.available()) Serial2.read();

        // Send PING to C3
        Serial2.println("$PING");

        // Wait for PONG with timeout
        unsigned long pingStart = millis();
        diag.c3PongReceived = false;
        char pongBuf[32];
        int pongIdx = 0;

        while (millis() - pingStart < PONG_TIMEOUT_MS) {
            if (Serial2.available()) {
                char c = Serial2.read();
                if (c == '\n' || c == '\r') {
                    pongBuf[pongIdx] = '\0';
                    if (strcmp(pongBuf, "$PONG") == 0) {
                        diag.c3PongReceived = true;
                        break;
                    }
                    pongIdx = 0;
                } else if (pongIdx < 30) {
                    pongBuf[pongIdx++] = c;
                }
            }
        }

        if (!diag.c3PongReceived) diag.totalFaults++;
    }

    // ========================================================================
    // Compute diagnostic health score
    // ========================================================================
    diag.healthScore = computeHealthScore();

    // ========================================================================
    // Print diagnostic results to USB Serial (debugger)
    // ========================================================================
    statusPrint("=== DIAGNOSTIC RESULTS ===");

    char msg[256];
    snprintf(msg, sizeof(msg), "  MPU WHO_AM_I: %s", diag.mpuWhoAmIOk ? "PASS" : "FAIL");
    statusPrint(msg);
    snprintf(msg, sizeof(msg), "  MPU Accel Magnitude: %.3f g — %s",
             diag.mpuAccelMagnitude, diag.mpuAccelMagnitudeOk ? "PASS" : "FAIL");
    statusPrint(msg);
    snprintf(msg, sizeof(msg), "  MPU Gyro Drift: %.3f °/s — %s",
             diag.mpuGyroDrift, diag.mpuGyroDriftOk ? "PASS" : "FAIL");
    statusPrint(msg);
    snprintf(msg, sizeof(msg), "  BMP Chip ID: %s", diag.bmpChipIdOk ? "PASS" : "FAIL");
    statusPrint(msg);
    snprintf(msg, sizeof(msg), "  BMP Pressure: %.2f hPa — %s",
             diag.bmpPressure, diag.bmpPressureRangeOk ? "PASS" : "FAIL");
    statusPrint(msg);
    snprintf(msg, sizeof(msg), "  BMP Temperature: %.2f °C — %s",
             diag.bmpTemperature, diag.bmpTempRangeOk ? "PASS" : "FAIL");
    statusPrint(msg);
    snprintf(msg, sizeof(msg), "  RTC Oscillator: %s", diag.rtcOscRunning ? "PASS" : "FAIL");
    statusPrint(msg);
    snprintf(msg, sizeof(msg), "  RTC Time Valid: %s", diag.rtcTimeValid ? "PASS" : "FAIL");
    statusPrint(msg);
    snprintf(msg, sizeof(msg), "  GPS Data Recent: %s (sats=%d)",
             diag.gpsDataRecent ? "PASS" : "FAIL", diag.gpsSatCount);
    statusPrint(msg);
    snprintf(msg, sizeof(msg), "  Battery: %.2fV — %s",
             diag.batteryVoltage, diag.batteryVoltageOk ? "PASS" : "FAIL");
    statusPrint(msg);
    snprintf(msg, sizeof(msg), "  C3 PONG: %s", diag.c3PongReceived ? "PASS" : "FAIL");
    statusPrint(msg);
    snprintf(msg, sizeof(msg), "  Total Faults: %d  Health Score: %d%%",
             diag.totalFaults, diag.healthScore);
    statusPrint(msg);
    statusPrint("=== END DIAGNOSTIC ===");

    // ========================================================================
    // Send diagnostic frame to C3 for Firebase upload
    // ========================================================================
    sendDiagToC3(diag);
}

void sendDiagToC3(DiagResult &diag) {
    // Build $DIAG frame and send to C3 via Serial2
    char diagFrame[512];

    snprintf(diagFrame, sizeof(diagFrame),
             "$DIAG,"
             "MPU_ID=%d,"
             "MPU_ACCEL=%.3f:%d,"
             "MPU_GYRO=%.3f:%d,"
             "BMP_ID=%d,"
             "BMP_P=%.2f:%d,"
             "BMP_T=%.2f:%d,"
             "RTC_OSC=%d,"
             "RTC_TIME=%d,"
             "GPS_RECENT=%d,"
             "GPS_SATS=%d,"
             "BATT=%.2f:%d,"
             "C3_PONG=%d,"
             "FAULTS=%d,"
             "HEALTH=%d",
             diag.mpuWhoAmIOk,
             diag.mpuAccelMagnitude, diag.mpuAccelMagnitudeOk,
             diag.mpuGyroDrift, diag.mpuGyroDriftOk,
             diag.bmpChipIdOk,
             diag.bmpPressure, diag.bmpPressureRangeOk,
             diag.bmpTemperature, diag.bmpTempRangeOk,
             diag.rtcOscRunning,
             diag.rtcTimeValid,
             diag.gpsDataRecent,
             diag.gpsSatCount,
             diag.batteryVoltage, diag.batteryVoltageOk,
             diag.c3PongReceived,
             diag.totalFaults,
             diag.healthScore);

    Serial2.println(diagFrame);

    // Also send via software UART to C3's data channel for redundancy
    c3UartSendString(diagFrame);
    c3UartSendByte('\r');
    c3UartSendByte('\n');

    statusPrint("DIAGNOSTIC: Results sent to C3");
}


// ============================================================================
// ============================================================================
//                       UTILITY FUNCTIONS
// ============================================================================
// ============================================================================

void statusPrint(const char* msg) {
    Serial.print("STATUS:");
    Serial.println(msg);
}

void errorPrint(const char* msg) {
    Serial.print("ERROR:");
    Serial.println(msg);
}

void warningPrint(const char* msg) {
    Serial.print("WARNING:");
    Serial.println(msg);
}
