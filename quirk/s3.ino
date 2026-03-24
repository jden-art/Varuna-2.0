// ============================================================================
// VARUNA ESP32-S3 MAIN FIRMWARE
// File: varuna_s3_main.ino
// 
// Complete sensor brain firmware for flood detection buoy.
// Reads MPU6050, BMP280, DS1307 RTC, GPS module.
// Performs sensor fusion, flood classification, dynamic sampling.
// Outputs 39-field CSV on GPIO14 (software UART to C3) and USB Serial.
// Receives configuration commands from C3 (Serial2) and USB debugger.
// Runs autonomous diagnostics every 24 hours.
//
// NO WiFi. NO HTTP. NO Firebase. Pure edge intelligence.
// ============================================================================

#include <Wire.h>
#include <math.h>

// ============================================================================
// PIN DEFINITIONS
// ============================================================================
#define PIN_I2C0_SDA          8
#define PIN_I2C0_SCL          9
#define PIN_I2C1_SDA          4
#define PIN_I2C1_SCL          5

#define PIN_GPS_RX            7    // S3 receives FROM GPS
#define PIN_GPS_TX            6    // S3 transmits TO GPS (usually unused)

#define PIN_C3_UART_RX        44   // S3 receives commands FROM C3
#define PIN_C3_UART_TX        43   // S3 sends responses TO C3

#define PIN_C3_CSV_OUT        14   // Software UART TX for 39-field CSV to C3

#define PIN_BATTERY_ADC       2

// ============================================================================
// I2C ADDRESSES
// ============================================================================
#define MPU6050_ADDR          0x68
#define BMP280_ADDR           0x76
#define DS1307_ADDR           0x68  // Same as MPU but on different I2C bus

// ============================================================================
// MPU6050 REGISTER DEFINITIONS
// ============================================================================
#define MPU6050_REG_WHO_AM_I       0x75
#define MPU6050_REG_PWR_MGMT_1    0x6B
#define MPU6050_REG_PWR_MGMT_2    0x6C
#define MPU6050_REG_SMPLRT_DIV    0x19
#define MPU6050_REG_CONFIG        0x1A
#define MPU6050_REG_GYRO_CONFIG   0x1B
#define MPU6050_REG_ACCEL_CONFIG  0x1C
#define MPU6050_REG_INT_ENABLE    0x38
#define MPU6050_REG_ACCEL_XOUT_H  0x3B
#define MPU6050_REG_GYRO_XOUT_H   0x43
#define MPU6050_REG_TEMP_OUT_H    0x41
#define MPU6050_WHO_AM_I_VAL      0x68

// ============================================================================
// BMP280 REGISTER DEFINITIONS
// ============================================================================
#define BMP280_REG_CHIP_ID        0xD0
#define BMP280_REG_RESET          0xE0
#define BMP280_REG_STATUS         0xF3
#define BMP280_REG_CTRL_MEAS      0xF4
#define BMP280_REG_CONFIG_REG     0xF5
#define BMP280_REG_PRESS_MSB      0xF7
#define BMP280_REG_TEMP_MSB       0xFA
#define BMP280_REG_CALIB_START    0x88
#define BMP280_CHIP_ID_VAL        0x58

// ============================================================================
// DS1307 REGISTER DEFINITIONS
// ============================================================================
#define DS1307_REG_SECONDS        0x00
#define DS1307_REG_MINUTES        0x01
#define DS1307_REG_HOURS          0x02
#define DS1307_REG_DAY            0x03
#define DS1307_REG_DATE           0x04
#define DS1307_REG_MONTH          0x05
#define DS1307_REG_YEAR           0x06
#define DS1307_REG_CONTROL        0x07

// ============================================================================
// SENSOR FUSION CONSTANTS
// ============================================================================
#define ALPHA                     0.98f    // Complementary filter gyro weight
#define GYRO_SENSITIVITY_250DPS   131.0f   // LSB per degree/sec at ±250°/s
#define ACCEL_SENSITIVITY_2G      16384.0f // LSB per g at ±2g
#define GRAVITY_MPS2              9.80665f
#define DEG_TO_RAD                0.017453292519943f
#define RAD_TO_DEG                57.29577951308232f

// ============================================================================
// CALIBRATION CONSTANTS
// ============================================================================
#define GYRO_CALIBRATION_SAMPLES  1000
#define ACCEL_CALIBRATION_SAMPLES 500
#define BASELINE_PRESSURE_SAMPLES 50
#define ACCEL_MAG_MIN             0.9f     // Minimum valid |g| during calibration
#define ACCEL_MAG_MAX             1.1f     // Maximum valid |g| during calibration

// ============================================================================
// FLOOD DETECTION THRESHOLDS (defaults, can be updated via commands)
// ============================================================================
#define DEFAULT_LATERAL_ACCEL_THRESHOLD  0.15f   // m/s² — tether taut detection
#define DEFAULT_LATERAL_ACCEL_RELEASE    0.10f   // m/s² — tether slack (hysteresis)
#define DEFAULT_TILT_THRESHOLD_DEG       3.0f    // degrees — minimum tilt for taut
#define DEFAULT_FLOOD_TILT_DEG           10.0f   // degrees — below this = near flood
#define DEFAULT_FLOOD_RATIO              0.95f   // H/L ratio threshold for MODE 2
#define DEFAULT_SUBMERSION_PRESSURE_PA   500.0f  // Pa above atmospheric = submerged
#define DEFAULT_OLP_LENGTH_CM            200.0f  // Tether length in cm
#define DEFAULT_H_MAX_CM                 200.0f  // Maximum flood height = OLP length

// ============================================================================
// MODE PERSISTENCE REQUIREMENTS
// ============================================================================
#define PERSISTENCE_NORMAL        10   // Consecutive readings before mode commit
#define PERSISTENCE_SUBMERGE      3    // Fewer for urgent submersion detection

// ============================================================================
// DYNAMIC SAMPLING DEFAULTS
// ============================================================================
#define DEFAULT_NORMAL_RATE_SEC   900   // 15 minutes during safe conditions
#define DEFAULT_HIGH_RATE_SEC     60    // 1 minute during high water
#define TRANSITION_RATIO_LOW      0.50f // Below this: normal rate
#define TRANSITION_RATIO_HIGH     0.80f // Above this: high rate

// ============================================================================
// DIAGNOSTIC TIMING
// ============================================================================
#define DIAG_INTERVAL_MS          86400000UL  // 24 hours in milliseconds
#define DIAG_PING_TIMEOUT_MS      2000        // Wait 2 seconds for C3 pong

// ============================================================================
// SENSOR FUSION TIMING
// ============================================================================
#define FUSION_INTERVAL_US        10000  // 10ms = 100Hz fusion rate

// ============================================================================
// SOFTWARE UART TIMING (9600 baud)
// ============================================================================
#define SOFT_UART_BIT_DURATION_US 104    // 1/9600 ≈ 104.17 µs

// ============================================================================
// GPS BUFFER
// ============================================================================
#define GPS_BUFFER_SIZE           256

// ============================================================================
// WATER DENSITY
// ============================================================================
#define WATER_DENSITY_KGM3        1000.0f

// ============================================================================
// GLOBAL ENUMERATIONS
// ============================================================================
enum FloodMode {
    MODE_SLACK     = 0,
    MODE_TAUT      = 1,
    MODE_FLOOD     = 2,
    MODE_SUBMERGED = 3
};

enum AlertLevel {
    ALERT_GREEN  = 0,
    ALERT_YELLOW = 1,
    ALERT_RED    = 2,
    ALERT_BLACK  = 3
};

enum ResponseLevel {
    RESPONSE_NONE      = 0,
    RESPONSE_MONITOR   = 1,
    RESPONSE_WARNING   = 2,
    RESPONSE_ALERT     = 3,
    RESPONSE_EMERGENCY = 4
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
    int32_t  t_fine;
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
    int      hour;
    int      minute;
    int      second;
    int      day;
    int      month;
    int      year;
    bool     timeValid;
    bool     available;
};

// ============================================================================
// RTC DATE/TIME STRUCTURE
// ============================================================================
struct RTCDateTime {
    int year;
    int month;
    int day;
    int hour;
    int minute;
    int second;
    bool valid;
};

// ============================================================================
// DIAGNOSTIC RESULTS STRUCTURE
// ============================================================================
struct DiagResults {
    // MPU6050
    bool     mpuWhoAmIOk;
    bool     mpuAccelMagOk;
    float    mpuAccelMag;
    bool     mpuGyroDriftOk;
    float    mpuGyroDrift;
    bool     mpuOverallOk;

    // BMP280
    bool     bmpChipIdOk;
    bool     bmpPressureRangeOk;
    float    bmpPressure;
    bool     bmpTempRangeOk;
    float    bmpTemperature;
    bool     bmpOverallOk;

    // RTC
    bool     rtcOscRunning;
    bool     rtcTimeValid;
    bool     rtcOverallOk;

    // GPS
    bool     gpsUartActive;
    bool     gpsOverallOk;

    // Battery
    bool     batteryVoltageOk;
    float    batteryVoltage;

    // C3 Communication
    bool     c3PongReceived;

    // Summary
    int      totalFaults;
    int      healthScore;
};

// ============================================================================
// GLOBAL STATE VARIABLES
// ============================================================================

// --- I2C bus instances ---
TwoWire I2C_Bus0 = TwoWire(0);
TwoWire I2C_Bus1 = TwoWire(1);

// --- Sensor availability flags ---
bool mpuAvailable   = false;
bool bmpAvailable   = false;
bool rtcAvailable   = false;
bool gpsAvailable   = false;

// --- MPU6050 calibration offsets ---
float gyroOffsetX   = 0.0f;
float gyroOffsetY   = 0.0f;
float gyroOffsetZ   = 0.0f;
float refAccX       = 0.0f;
float refAccY       = 0.0f;
float refAccZ       = 0.0f;
float refTiltX      = 0.0f;
float refTiltY      = 0.0f;

// --- Sensor fusion state ---
float filteredTiltX = 0.0f;
float filteredTiltY = 0.0f;
float correctedTiltX = 0.0f;
float correctedTiltY = 0.0f;
float combinedTheta = 0.0f;
unsigned long lastFusionMicros = 0;

// --- Raw sensor values ---
int16_t rawAccX, rawAccY, rawAccZ;
int16_t rawGyroX, rawGyroY, rawGyroZ;
int16_t rawTemp;
float accXg, accYg, accZg;         // In g units
float accXms2, accYms2, accZms2;   // In m/s²
float gyroXdps, gyroYdps, gyroZdps; // In degrees/sec
float lateralAccel = 0.0f;         // sqrt(ax² + ay²) in m/s²

// --- BMP280 state ---
BMP280CalibData bmpCalib;
float currentPressure    = 0.0f;   // hPa
float currentTemperature = 0.0f;   // °C
float baselinePressure   = 0.0f;   // hPa (atmospheric at deployment)
float pressureDeviation  = 0.0f;   // hPa
float gaugePressurePa    = 0.0f;   // Pa (above atmospheric)

// --- RTC state ---
RTCDateTime rtcTime;
char dateTimeString[32] = "0000-00-00 00:00:00";
unsigned long unixTime   = 0;

// --- GPS state ---
GPSData gpsData;
char gpsBuffer[GPS_BUFFER_SIZE];
int  gpsBufferIdx = 0;

// --- Flood detection state ---
FloodMode     currentMode       = MODE_SLACK;
FloodMode     pendingMode       = MODE_SLACK;
int           persistenceCount  = 0;
AlertLevel    floodAlertLevel   = ALERT_GREEN;
ResponseLevel responseLevel     = RESPONSE_NONE;

// --- Water level computation ---
float olpLengthCm        = DEFAULT_OLP_LENGTH_CM;
float hMaxCm             = DEFAULT_H_MAX_CM;
float waterHeightCm      = 0.0f;
float horizontalDistCm   = 0.0f;
float floodRatio          = 0.0f;
float estimatedDepthCm   = 0.0f;
int   submersionState    = 0;

// --- Configurable thresholds ---
float threshLateralAccel  = DEFAULT_LATERAL_ACCEL_THRESHOLD;
float threshLateralRelease = DEFAULT_LATERAL_ACCEL_RELEASE;
float threshTiltDeg       = DEFAULT_TILT_THRESHOLD_DEG;
float threshFloodTiltDeg  = DEFAULT_FLOOD_TILT_DEG;
float threshFloodRatio    = DEFAULT_FLOOD_RATIO;
float threshSubmersionPa  = DEFAULT_SUBMERSION_PRESSURE_PA;

// --- Dynamic sampling state ---
int   normalRateSec       = DEFAULT_NORMAL_RATE_SEC;
int   highRateSec         = DEFAULT_HIGH_RATE_SEC;
int   currentSampleIntervalSec = DEFAULT_NORMAL_RATE_SEC;
int   currentTransmitIntervalSec = DEFAULT_NORMAL_RATE_SEC;
unsigned long lastTransmitMs = 0;

// --- Session statistics ---
unsigned long sessionStartMs = 0;
float peakHeightCm        = 0.0f;
float minHeightCm         = 99999.0f;
float prevWaterHeightCm   = 0.0f;
float rateOfChangePer15Min = 0.0f;
unsigned long lastRateCalcMs = 0;
float lastRateCalcHeight  = 0.0f;
bool  sustainedRise       = false;
int   riseConsecutiveCount = 0;

// --- Zone classification ---
int   currentZone         = 0;   // 0=safe, 1=watch, 2=warning, 3=danger

// --- Battery ---
float batteryVoltage      = 0.0f;
float batteryPercent      = 0.0f;

// --- Health ---
int   healthScore         = 0;

// --- OB Light (obstacle light indicator) ---
bool  obLightEnabled      = true;

// --- Algorithm enable ---
bool  algorithmEnabled    = true;

// --- SIM/GPRS status (received from C3, stored for CSV output) ---
int   simSignalRSSI       = 0;
bool  simRegistered       = false;
bool  simModuleAvailable  = false;

// --- Diagnostic state ---
unsigned long lastDiagMs  = 0;
DiagResults   lastDiag;
bool          diagRequested = false;

// --- C3 ping/pong state ---
bool  waitingForPong      = false;
unsigned long pongWaitStartMs = 0;

// --- Command parsing buffers ---
char  c3CmdBuffer[256];
int   c3CmdIdx = 0;
char  usbCmdBuffer[256];
int   usbCmdIdx = 0;

// ============================================================================
// FORWARD DECLARATIONS
// ============================================================================

// --- Initialization ---
void initI2CBuses();
bool initMPU6050();
bool initBMP280();
bool initDS1307();
void initGPS();
void initBatteryADC();
void initSoftwareUART();

// --- MPU6050 functions ---
void  mpu6050WriteReg(uint8_t reg, uint8_t val);
uint8_t mpu6050ReadReg(uint8_t reg);
void  mpu6050ReadSensors();
void  mpu6050ReadAccel();
void  mpu6050ReadGyro();

// --- BMP280 functions ---
void  bmp280WriteReg(uint8_t reg, uint8_t val);
uint8_t bmp280ReadReg(uint8_t reg);
void  bmp280ReadCalibration();
float bmp280CompensateTemp(int32_t adc_T);
float bmp280CompensatePress(int32_t adc_P);
void  bmp280ReadAll();
void  bmp280EstablishBaseline();

// --- DS1307 functions ---
uint8_t bcdToDec(uint8_t bcd);
uint8_t decToBcd(uint8_t dec);
void  ds1307Read();
unsigned long dateTimeToUnix(int yr, int mo, int dy, int hr, int mn, int sc);

// --- GPS functions ---
void  gpsProcessIncoming();
void  gpsParseNMEA(const char* sentence);
void  gpsParseGGA(const char* sentence);
void  gpsParseRMC(const char* sentence);
float gpsParseCoord(const char* field, char dir);
int   gpsParseSatellites(const char* field);
float gpsParseAltitude(const char* field);

// --- Calibration ---
void  calibrateGyroscope();
void  calibrateAccelerometer();
void  recalibrate();

// --- Sensor fusion ---
void  runSensorFusion();

// --- Flood detection ---
void  classifyFloodMode();
void  updateAlertLevel();
void  computeWaterHeight();
void  computeRateOfChange();
void  classifyZone();
void  computeResponseLevel();

// --- Dynamic sampling ---
void  updateSamplingRate();
int   computeDynamicInterval();

// --- Battery ---
void  readBattery();

// --- Health ---
void  computeHealthScore();

// --- Output ---
void  buildCSVString(char* buf, int bufSize);
void  transmitCSVtoC3(const char* csv);
void  transmitCSVtoUSB(const char* csv);
void  c3UartSendByte(uint8_t b);
void  c3UartSendString(const char* str);

// --- Command handling ---
void  processC3Commands();
void  processUSBCommands();
void  handleC3Command(const char* cmd);
void  handleUSBCommand(const char* cmd);

// --- Diagnostics ---
void  runDiagnostics();
void  sendDiagToC3();

// --- Utility ---
void  sendStatusUSB(const char* msg);
void  sendErrorUSB(const char* msg);
void  sendWarningUSB(const char* msg);


// ============================================================================
//
//                         SETUP
//
// ============================================================================
void setup() {
    // ── USB Serial for debugger ──
    Serial.begin(115200);
    delay(500);
    Serial.println("STATUS: VARUNA S3 Sensor Brain booting...");
    Serial.println("STATUS: Firmware version 2.0.0");
    Serial.println("STATUS: Build date " __DATE__ " " __TIME__);

    // ── Serial2 for C3 command channel ──
    Serial2.begin(9600, SERIAL_8N1, PIN_C3_UART_RX, PIN_C3_UART_TX);
    Serial.println("STATUS: Serial2 (C3 command channel) initialized on GPIO43/44 at 9600 baud");

    // ── GPS UART ──
    Serial1.begin(9600, SERIAL_8N1, PIN_GPS_RX, PIN_GPS_TX);
    Serial.println("STATUS: Serial1 (GPS) initialized on GPIO6/7 at 9600 baud");

    // ── Software UART output pin for CSV ──
    initSoftwareUART();
    Serial.println("STATUS: Software UART TX on GPIO14 initialized");

    // ── I2C buses ──
    initI2CBuses();

    // ── Initialize sensors ──
    Serial.println("STATUS: Initializing sensors...");

    mpuAvailable = initMPU6050();
    if (mpuAvailable) {
        Serial.println("STATUS: MPU6050 initialized successfully");
    } else {
        Serial.println("ERROR: MPU6050 initialization FAILED");
    }

    bmpAvailable = initBMP280();
    if (bmpAvailable) {
        Serial.println("STATUS: BMP280 initialized successfully");
    } else {
        Serial.println("ERROR: BMP280 initialization FAILED");
    }

    rtcAvailable = initDS1307();
    if (rtcAvailable) {
        Serial.println("STATUS: DS1307 RTC initialized successfully");
    } else {
        Serial.println("WARNING: DS1307 RTC initialization FAILED — using GPS time");
    }

    initGPS();
    Serial.println("STATUS: GPS module initialized");

    initBatteryADC();
    Serial.println("STATUS: Battery ADC initialized");

    // ── Calibration ──
    if (mpuAvailable) {
        Serial.println("STATUS: Starting gyroscope calibration (1000 samples)...");
        calibrateGyroscope();
        Serial.println("STATUS: Gyroscope calibration complete");

        Serial.println("STATUS: Starting accelerometer calibration (500 samples)...");
        calibrateAccelerometer();
        Serial.println("STATUS: Accelerometer calibration complete");

        Serial.print("STATUS: Gyro offsets X=");
        Serial.print(gyroOffsetX, 4);
        Serial.print(" Y=");
        Serial.print(gyroOffsetY, 4);
        Serial.print(" Z=");
        Serial.println(gyroOffsetZ, 4);

        Serial.print("STATUS: Reference tilt X=");
        Serial.print(refTiltX, 2);
        Serial.print("° Y=");
        Serial.print(refTiltY, 2);
        Serial.println("°");
    }

    // ── BMP280 baseline pressure ──
    if (bmpAvailable) {
        Serial.println("STATUS: Establishing atmospheric baseline pressure (50 samples)...");
        bmp280EstablishBaseline();
        Serial.print("STATUS: Baseline pressure = ");
        Serial.print(baselinePressure, 2);
        Serial.println(" hPa");
    }

    // ── Initialize timing ──
    lastFusionMicros = micros();
    sessionStartMs   = millis();
    lastTransmitMs   = millis();
    lastDiagMs       = millis();
    lastRateCalcMs   = millis();
    lastRateCalcHeight = 0.0f;

    // ── Initialize GPS data struct ──
    memset(&gpsData, 0, sizeof(GPSData));

    Serial.println("STATUS: ══════════════════════════════════════");
    Serial.println("STATUS: VARUNA S3 initialization complete");
    Serial.print("STATUS: OLP length = ");
    Serial.print(olpLengthCm, 1);
    Serial.println(" cm");
    Serial.print("STATUS: Normal sample rate = ");
    Serial.print(normalRateSec);
    Serial.println(" sec");
    Serial.print("STATUS: High sample rate = ");
    Serial.print(highRateSec);
    Serial.println(" sec");
    Serial.println("STATUS: Entering main loop...");
    Serial.println("STATUS: ══════════════════════════════════════");
}


// ============================================================================
//
//                         MAIN LOOP
//
// ============================================================================
void loop() {
    unsigned long nowMs = millis();
    unsigned long nowUs = micros();

    // ──────────────────────────────────────────────────────────────
    // 1. SENSOR FUSION at 100Hz (every 10ms)
    // ──────────────────────────────────────────────────────────────
    if ((nowUs - lastFusionMicros) >= FUSION_INTERVAL_US) {
        if (mpuAvailable) {
            runSensorFusion();
        }
        lastFusionMicros = nowUs;
    }

    // ──────────────────────────────────────────────────────────────
    // 2. GPS PROCESSING — continuous NMEA parsing
    // ──────────────────────────────────────────────────────────────
    gpsProcessIncoming();

    // ──────────────────────────────────────────────────────────────
    // 3. COMMAND RECEPTION — check both channels
    // ──────────────────────────────────────────────────────────────
    processC3Commands();
    processUSBCommands();

    // ──────────────────────────────────────────────────────────────
    // 4. C3 PONG TIMEOUT CHECK
    // ──────────────────────────────────────────────────────────────
    if (waitingForPong && (nowMs - pongWaitStartMs > DIAG_PING_TIMEOUT_MS)) {
        waitingForPong = false;
        lastDiag.c3PongReceived = false;
        // Finish and send diagnostic
        sendDiagToC3();
    }

    // ──────────────────────────────────────────────────────────────
    // 5. PERIODIC DIAGNOSTICS — every 24 hours or on demand
    // ──────────────────────────────────────────────────────────────
    if (diagRequested || (nowMs - lastDiagMs >= DIAG_INTERVAL_MS)) {
        if (!waitingForPong) {
            runDiagnostics();
            lastDiagMs = nowMs;
            diagRequested = false;
        }
    }

    // ──────────────────────────────────────────────────────────────
    // 6. TRANSMIT CYCLE — dynamic interval based on water level
    // ──────────────────────────────────────────────────────────────
    int dynamicIntervalMs = computeDynamicInterval() * 1000;

    if ((nowMs - lastTransmitMs) >= (unsigned long)dynamicIntervalMs) {
        lastTransmitMs = nowMs;

        // ── Read all slow sensors ──
        if (bmpAvailable) {
            bmp280ReadAll();
            pressureDeviation = currentPressure - baselinePressure;
            gaugePressurePa   = pressureDeviation * 100.0f; // hPa to Pa
        }

        if (rtcAvailable) {
            ds1307Read();
        }

        readBattery();

        // ── Compute water height ──
        computeWaterHeight();

        // ── Classify flood mode ──
        classifyFloodMode();
        updateAlertLevel();
        classifyZone();
        computeResponseLevel();

        // ── Rate of change ──
        computeRateOfChange();

        // ── Session statistics ──
        if (waterHeightCm > peakHeightCm) peakHeightCm = waterHeightCm;
        if (waterHeightCm < minHeightCm && waterHeightCm > 0.0f) {
            minHeightCm = waterHeightCm;
        }

        // ── Update dynamic sampling ──
        updateSamplingRate();

        // ── Compute health ──
        computeHealthScore();

        // ── Build CSV ──
        char csvBuf[1024];
        buildCSVString(csvBuf, sizeof(csvBuf));

        // ── Transmit on both streams ──
        transmitCSVtoC3(csvBuf);
        transmitCSVtoUSB(csvBuf);
    }
}


// ============================================================================
//
//                    INITIALIZATION FUNCTIONS
//
// ============================================================================

void initI2CBuses() {
    I2C_Bus0.begin(PIN_I2C0_SDA, PIN_I2C0_SCL, 400000);
    Serial.println("STATUS: I2C Bus 0 initialized (SDA=8, SCL=9, 400kHz)");

    I2C_Bus1.begin(PIN_I2C1_SDA, PIN_I2C1_SCL, 400000);
    Serial.println("STATUS: I2C Bus 1 initialized (SDA=4, SCL=5, 400kHz)");
}

void initSoftwareUART() {
    pinMode(PIN_C3_CSV_OUT, OUTPUT);
    digitalWrite(PIN_C3_CSV_OUT, HIGH); // UART idle state is HIGH
}

void initBatteryADC() {
    analogSetAttenuation(ADC_11db);
    analogReadResolution(12);
    pinMode(PIN_BATTERY_ADC, INPUT);
}


// ============================================================================
//
//                    MPU6050 DRIVER
//
// ============================================================================

void mpu6050WriteReg(uint8_t reg, uint8_t val) {
    I2C_Bus0.beginTransmission(MPU6050_ADDR);
    I2C_Bus0.write(reg);
    I2C_Bus0.write(val);
    I2C_Bus0.endTransmission();
}

uint8_t mpu6050ReadReg(uint8_t reg) {
    I2C_Bus0.beginTransmission(MPU6050_ADDR);
    I2C_Bus0.write(reg);
    I2C_Bus0.endTransmission(false);
    I2C_Bus0.requestFrom(MPU6050_ADDR, (uint8_t)1);
    if (I2C_Bus0.available()) {
        return I2C_Bus0.read();
    }
    return 0xFF;
}

bool initMPU6050() {
    // Check WHO_AM_I
    uint8_t whoami = mpu6050ReadReg(MPU6050_REG_WHO_AM_I);
    if (whoami != MPU6050_WHO_AM_I_VAL) {
        Serial.print("ERROR: MPU6050 WHO_AM_I returned 0x");
        Serial.print(whoami, HEX);
        Serial.print(" (expected 0x");
        Serial.print(MPU6050_WHO_AM_I_VAL, HEX);
        Serial.println(")");
        return false;
    }

    // Wake up (clear sleep bit), use internal 8MHz oscillator
    mpu6050WriteReg(MPU6050_REG_PWR_MGMT_1, 0x00);
    delay(100);

    // Set sample rate divider: 200Hz (8kHz / (1 + 39))
    mpu6050WriteReg(MPU6050_REG_SMPLRT_DIV, 39);

    // Set DLPF to ~44Hz bandwidth (register value 3)
    mpu6050WriteReg(MPU6050_REG_CONFIG, 0x03);

    // Gyroscope: ±250°/s (most sensitive range)
    mpu6050WriteReg(MPU6050_REG_GYRO_CONFIG, 0x00);

    // Accelerometer: ±2g (most sensitive range)
    mpu6050WriteReg(MPU6050_REG_ACCEL_CONFIG, 0x00);

    // Disable interrupts (we poll)
    mpu6050WriteReg(MPU6050_REG_INT_ENABLE, 0x00);

    delay(50);
    return true;
}

void mpu6050ReadSensors() {
    // Read 14 bytes starting from ACCEL_XOUT_H (accel[6] + temp[2] + gyro[6])
    I2C_Bus0.beginTransmission(MPU6050_ADDR);
    I2C_Bus0.write(MPU6050_REG_ACCEL_XOUT_H);
    I2C_Bus0.endTransmission(false);
    I2C_Bus0.requestFrom(MPU6050_ADDR, (uint8_t)14);

    if (I2C_Bus0.available() >= 14) {
        rawAccX  = (I2C_Bus0.read() << 8) | I2C_Bus0.read();
        rawAccY  = (I2C_Bus0.read() << 8) | I2C_Bus0.read();
        rawAccZ  = (I2C_Bus0.read() << 8) | I2C_Bus0.read();
        rawTemp  = (I2C_Bus0.read() << 8) | I2C_Bus0.read();
        rawGyroX = (I2C_Bus0.read() << 8) | I2C_Bus0.read();
        rawGyroY = (I2C_Bus0.read() << 8) | I2C_Bus0.read();
        rawGyroZ = (I2C_Bus0.read() << 8) | I2C_Bus0.read();
    }

    // Convert accelerometer to g
    accXg = (float)rawAccX / ACCEL_SENSITIVITY_2G;
    accYg = (float)rawAccY / ACCEL_SENSITIVITY_2G;
    accZg = (float)rawAccZ / ACCEL_SENSITIVITY_2G;

    // Convert accelerometer to m/s²
    accXms2 = accXg * GRAVITY_MPS2;
    accYms2 = accYg * GRAVITY_MPS2;
    accZms2 = accZg * GRAVITY_MPS2;

    // Convert gyroscope to degrees/sec (with offset correction)
    gyroXdps = ((float)rawGyroX / GYRO_SENSITIVITY_250DPS) - gyroOffsetX;
    gyroYdps = ((float)rawGyroY / GYRO_SENSITIVITY_250DPS) - gyroOffsetY;
    gyroZdps = ((float)rawGyroZ / GYRO_SENSITIVITY_250DPS) - gyroOffsetZ;

    // Compute lateral acceleration magnitude
    lateralAccel = sqrtf(accXms2 * accXms2 + accYms2 * accYms2);
}


// ============================================================================
//
//                    BMP280 DRIVER
//
// ============================================================================

void bmp280WriteReg(uint8_t reg, uint8_t val) {
    I2C_Bus1.beginTransmission(BMP280_ADDR);
    I2C_Bus1.write(reg);
    I2C_Bus1.write(val);
    I2C_Bus1.endTransmission();
}

uint8_t bmp280ReadReg(uint8_t reg) {
    I2C_Bus1.beginTransmission(BMP280_ADDR);
    I2C_Bus1.write(reg);
    I2C_Bus1.endTransmission(false);
    I2C_Bus1.requestFrom(BMP280_ADDR, (uint8_t)1);
    if (I2C_Bus1.available()) {
        return I2C_Bus1.read();
    }
    return 0xFF;
}

uint16_t bmp280Read16LE(uint8_t reg) {
    I2C_Bus1.beginTransmission(BMP280_ADDR);
    I2C_Bus1.write(reg);
    I2C_Bus1.endTransmission(false);
    I2C_Bus1.requestFrom(BMP280_ADDR, (uint8_t)2);
    uint8_t lo = I2C_Bus1.read();
    uint8_t hi = I2C_Bus1.read();
    return (uint16_t)((hi << 8) | lo);
}

int16_t bmp280ReadS16LE(uint8_t reg) {
    return (int16_t)bmp280Read16LE(reg);
}

void bmp280ReadCalibration() {
    bmpCalib.dig_T1 = bmp280Read16LE(0x88);
    bmpCalib.dig_T2 = bmp280ReadS16LE(0x8A);
    bmpCalib.dig_T3 = bmp280ReadS16LE(0x8C);
    bmpCalib.dig_P1 = bmp280Read16LE(0x8E);
    bmpCalib.dig_P2 = bmp280ReadS16LE(0x90);
    bmpCalib.dig_P3 = bmp280ReadS16LE(0x92);
    bmpCalib.dig_P4 = bmp280ReadS16LE(0x94);
    bmpCalib.dig_P5 = bmp280ReadS16LE(0x96);
    bmpCalib.dig_P6 = bmp280ReadS16LE(0x98);
    bmpCalib.dig_P7 = bmp280ReadS16LE(0x9A);
    bmpCalib.dig_P8 = bmp280ReadS16LE(0x9C);
    bmpCalib.dig_P9 = bmp280ReadS16LE(0x9E);
}

float bmp280CompensateTemp(int32_t adc_T) {
    int32_t var1, var2;
    var1 = ((((adc_T >> 3) - ((int32_t)bmpCalib.dig_T1 << 1))) *
            ((int32_t)bmpCalib.dig_T2)) >> 11;
    var2 = (((((adc_T >> 4) - ((int32_t)bmpCalib.dig_T1)) *
              ((adc_T >> 4) - ((int32_t)bmpCalib.dig_T1))) >> 12) *
            ((int32_t)bmpCalib.dig_T3)) >> 14;
    bmpCalib.t_fine = var1 + var2;
    float T = (bmpCalib.t_fine * 5 + 128) >> 8;
    return T / 100.0f;
}

float bmp280CompensatePress(int32_t adc_P) {
    int64_t var1, var2, p;
    var1 = ((int64_t)bmpCalib.t_fine) - 128000;
    var2 = var1 * var1 * (int64_t)bmpCalib.dig_P6;
    var2 = var2 + ((var1 * (int64_t)bmpCalib.dig_P5) << 17);
    var2 = var2 + (((int64_t)bmpCalib.dig_P4) << 35);
    var1 = ((var1 * var1 * (int64_t)bmpCalib.dig_P3) >> 8) +
           ((var1 * (int64_t)bmpCalib.dig_P2) << 12);
    var1 = (((((int64_t)1) << 47) + var1)) * ((int64_t)bmpCalib.dig_P1) >> 33;

    if (var1 == 0) {
        return 0.0f; // Avoid divide by zero
    }

    p = 1048576 - adc_P;
    p = (((p << 31) - var2) * 3125) / var1;
    var1 = (((int64_t)bmpCalib.dig_P9) * (p >> 13) * (p >> 13)) >> 25;
    var2 = (((int64_t)bmpCalib.dig_P8) * p) >> 19;
    p = ((p + var1 + var2) >> 8) + (((int64_t)bmpCalib.dig_P7) << 4);

    return (float)p / 25600.0f; // Return in hPa
}

bool initBMP280() {
    uint8_t chipId = bmp280ReadReg(BMP280_REG_CHIP_ID);
    if (chipId != BMP280_CHIP_ID_VAL) {
        Serial.print("ERROR: BMP280 chip ID returned 0x");
        Serial.print(chipId, HEX);
        Serial.print(" (expected 0x");
        Serial.print(BMP280_CHIP_ID_VAL, HEX);
        Serial.println(")");
        return false;
    }

    // Reset
    bmp280WriteReg(BMP280_REG_RESET, 0xB6);
    delay(100);

    // Wait for calibration data to be ready
    while (bmp280ReadReg(BMP280_REG_STATUS) & 0x01) {
        delay(10);
    }

    // Read calibration
    bmp280ReadCalibration();

    // Configure: 16x pressure oversampling, 2x temperature oversampling, normal mode
    // ctrl_meas: osrs_t[7:5]=010(×2), osrs_p[4:2]=101(×16), mode[1:0]=11(normal)
    bmp280WriteReg(BMP280_REG_CTRL_MEAS, 0x57);

    // Config: 500ms standby, filter coeff 16, no SPI
    // config: t_sb[7:5]=100(500ms), filter[4:2]=100(coeff 16), spi3w_en=0
    bmp280WriteReg(BMP280_REG_CONFIG_REG, 0x90);

    delay(100);
    return true;
}

void bmp280ReadAll() {
    // Read 6 bytes: pressure (3) + temperature (3)
    I2C_Bus1.beginTransmission(BMP280_ADDR);
    I2C_Bus1.write(BMP280_REG_PRESS_MSB);
    I2C_Bus1.endTransmission(false);
    I2C_Bus1.requestFrom(BMP280_ADDR, (uint8_t)6);

    if (I2C_Bus1.available() >= 6) {
        uint8_t p_msb  = I2C_Bus1.read();
        uint8_t p_lsb  = I2C_Bus1.read();
        uint8_t p_xlsb = I2C_Bus1.read();
        uint8_t t_msb  = I2C_Bus1.read();
        uint8_t t_lsb  = I2C_Bus1.read();
        uint8_t t_xlsb = I2C_Bus1.read();

        int32_t adc_P = ((int32_t)p_msb << 12) | ((int32_t)p_lsb << 4) | (p_xlsb >> 4);
        int32_t adc_T = ((int32_t)t_msb << 12) | ((int32_t)t_lsb << 4) | (t_xlsb >> 4);

        currentTemperature = bmp280CompensateTemp(adc_T);
        currentPressure    = bmp280CompensatePress(adc_P);
    }
}

void bmp280EstablishBaseline() {
    float sum = 0.0f;
    int validCount = 0;

    for (int i = 0; i < BASELINE_PRESSURE_SAMPLES; i++) {
        bmp280ReadAll();
        if (currentPressure > 800.0f && currentPressure < 1200.0f) {
            sum += currentPressure;
            validCount++;
        }
        delay(100);
    }

    if (validCount > 0) {
        baselinePressure = sum / (float)validCount;
    } else {
        baselinePressure = 1013.25f; // Standard atmosphere fallback
        sendWarningUSB("Baseline pressure unreliable — using 1013.25 hPa");
    }
}


// ============================================================================
//
//                    DS1307 RTC DRIVER
//
// ============================================================================

uint8_t bcdToDec(uint8_t bcd) {
    return ((bcd >> 4) * 10) + (bcd & 0x0F);
}

uint8_t decToBcd(uint8_t dec) {
    return ((dec / 10) << 4) | (dec % 10);
}

bool initDS1307() {
    // Try to read seconds register
    I2C_Bus1.beginTransmission(DS1307_ADDR);
    I2C_Bus1.write(DS1307_REG_SECONDS);
    uint8_t err = I2C_Bus1.endTransmission();

    if (err != 0) {
        return false;
    }

    I2C_Bus1.requestFrom(DS1307_ADDR, (uint8_t)1);
    if (!I2C_Bus1.available()) {
        return false;
    }

    uint8_t seconds = I2C_Bus1.read();

    // Check if oscillator is halted (bit 7 of seconds register)
    if (seconds & 0x80) {
        // Oscillator halted — try to start it
        I2C_Bus1.beginTransmission(DS1307_ADDR);
        I2C_Bus1.write(DS1307_REG_SECONDS);
        I2C_Bus1.write(seconds & 0x7F); // Clear CH bit
        I2C_Bus1.endTransmission();
        sendWarningUSB("RTC oscillator was halted — restarted");
    }

    // Enable square wave output at 1Hz (optional, useful for timing verification)
    I2C_Bus1.beginTransmission(DS1307_ADDR);
    I2C_Bus1.write(DS1307_REG_CONTROL);
    I2C_Bus1.write(0x10); // SQWE=1, RS=00 (1Hz)
    I2C_Bus1.endTransmission();

    return true;
}

void ds1307Read() {
    I2C_Bus1.beginTransmission(DS1307_ADDR);
    I2C_Bus1.write(DS1307_REG_SECONDS);
    I2C_Bus1.endTransmission(false);
    I2C_Bus1.requestFrom(DS1307_ADDR, (uint8_t)7);

    if (I2C_Bus1.available() >= 7) {
        uint8_t ss = I2C_Bus1.read();
        uint8_t mm = I2C_Bus1.read();
        uint8_t hh = I2C_Bus1.read();
        uint8_t dow = I2C_Bus1.read(); // day of week (unused)
        uint8_t dd = I2C_Bus1.read();
        uint8_t mo = I2C_Bus1.read();
        uint8_t yy = I2C_Bus1.read();

        // Check oscillator halt bit
        bool oscillatorRunning = !(ss & 0x80);

        rtcTime.second = bcdToDec(ss & 0x7F);
        rtcTime.minute = bcdToDec(mm & 0x7F);

        // Handle 12/24 hour mode
        if (hh & 0x40) {
            // 12-hour mode
            uint8_t hr12 = bcdToDec(hh & 0x1F);
            bool pm = (hh & 0x20) != 0;
            if (pm && hr12 != 12) hr12 += 12;
            if (!pm && hr12 == 12) hr12 = 0;
            rtcTime.hour = hr12;
        } else {
            // 24-hour mode
            rtcTime.hour = bcdToDec(hh & 0x3F);
        }

        rtcTime.day   = bcdToDec(dd);
        rtcTime.month = bcdToDec(mo);
        rtcTime.year  = 2000 + bcdToDec(yy);
        rtcTime.valid = oscillatorRunning &&
                        (rtcTime.year >= 2024 && rtcTime.year <= 2099) &&
                        (rtcTime.month >= 1 && rtcTime.month <= 12) &&
                        (rtcTime.day >= 1 && rtcTime.day <= 31) &&
                        (rtcTime.hour <= 23) &&
                        (rtcTime.minute <= 59) &&
                        (rtcTime.second <= 59);

        // Format datetime string
        snprintf(dateTimeString, sizeof(dateTimeString),
                 "%04d-%02d-%02d %02d:%02d:%02d",
                 rtcTime.year, rtcTime.month, rtcTime.day,
                 rtcTime.hour, rtcTime.minute, rtcTime.second);

        // Compute Unix timestamp
        if (rtcTime.valid) {
            unixTime = dateTimeToUnix(rtcTime.year, rtcTime.month, rtcTime.day,
                                      rtcTime.hour, rtcTime.minute, rtcTime.second);
        }
    }
}

unsigned long dateTimeToUnix(int yr, int mo, int dy, int hr, int mn, int sc) {
    // Simplified Unix timestamp calculation
    // Days from 1970-01-01 to the given date
    unsigned long days = 0;

    for (int y = 1970; y < yr; y++) {
        bool leap = (y % 4 == 0 && (y % 100 != 0 || y % 400 == 0));
        days += leap ? 366 : 365;
    }

    int daysInMonth[] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
    bool leapYear = (yr % 4 == 0 && (yr % 100 != 0 || yr % 400 == 0));
    if (leapYear) daysInMonth[1] = 29;

    for (int m = 0; m < mo - 1; m++) {
        days += daysInMonth[m];
    }
    days += dy - 1;

    return days * 86400UL + (unsigned long)hr * 3600UL +
           (unsigned long)mn * 60UL + (unsigned long)sc;
}


// ============================================================================
//
//                    GPS NMEA PARSER
//
// ============================================================================

void initGPS() {
    memset(&gpsData, 0, sizeof(GPSData));
    gpsData.available = false;
    gpsData.fixValid  = false;
    gpsData.timeValid = false;
    gpsBufferIdx = 0;
}

void gpsProcessIncoming() {
    while (Serial1.available()) {
        char c = Serial1.read();
        gpsData.available = true; // We're at least receiving data

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
                gpsBufferIdx = 0; // Buffer overflow, reset
            }
        }
    }
}

// Helper: extract field N from comma-separated NMEA sentence
// Returns pointer to field start within the provided working buffer
static char nmeaFieldBuf[64];

const char* nmeaGetField(const char* sentence, int fieldNum) {
    const char* p = sentence;
    int currentField = 0;

    // Skip the sentence identifier (e.g., "$GPGGA")
    // Fields are 0-indexed AFTER the sentence type
    // But we want field 0 to be the first field after the comma following the sentence ID
    while (*p && *p != ',') p++;
    if (*p == ',') p++; // skip first comma

    while (currentField < fieldNum) {
        while (*p && *p != ',' && *p != '*') p++;
        if (*p == ',') {
            p++;
            currentField++;
        } else {
            nmeaFieldBuf[0] = '\0';
            return nmeaFieldBuf;
        }
    }

    // Copy field content
    int i = 0;
    while (*p && *p != ',' && *p != '*' && i < 63) {
        nmeaFieldBuf[i++] = *p++;
    }
    nmeaFieldBuf[i] = '\0';
    return nmeaFieldBuf;
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
        uint8_t calcChecksum = 0;
        for (int i = 1; i < starPos; i++) {
            calcChecksum ^= sentence[i];
        }
        char checksumStr[3] = { sentence[starPos + 1], sentence[starPos + 2], '\0' };
        uint8_t rxChecksum = (uint8_t)strtoul(checksumStr, NULL, 16);

        if (calcChecksum != rxChecksum) {
            return; // Checksum mismatch, discard
        }
    }

    if (strncmp(sentence + 3, "GGA", 3) == 0) {
        gpsParseGGA(sentence);
    } else if (strncmp(sentence + 3, "RMC", 3) == 0) {
        gpsParseRMC(sentence);
    }
}

void gpsParseGGA(const char* sentence) {
    // $GPGGA,hhmmss.ss,lat,N/S,lon,E/W,fix,sats,hdop,alt,M,geoid,M,,*cs
    // Fields: 0=time, 1=lat, 2=N/S, 3=lon, 4=E/W, 5=fix, 6=sats, 7=hdop, 8=alt, 9=M

    const char* timeField = nmeaGetField(sentence, 0);
    if (strlen(timeField) >= 6) {
        char hh[3] = { timeField[0], timeField[1], '\0' };
        char mm[3] = { timeField[2], timeField[3], '\0' };
        char ss[3] = { timeField[4], timeField[5], '\0' };
        gpsData.hour   = atoi(hh);
        gpsData.minute = atoi(mm);
        gpsData.second = atoi(ss);
    }

    const char* latField = nmeaGetField(sentence, 1);
    const char* nsField  = nmeaGetField(sentence, 2);
    char nsChar = nsField[0];
    // Need to store nsChar before next call overwrites buffer
    char latBuf[20];
    strncpy(latBuf, latField, sizeof(latBuf) - 1);
    latBuf[sizeof(latBuf) - 1] = '\0';

    const char* lonField = nmeaGetField(sentence, 3);
    char lonBuf[20];
    strncpy(lonBuf, lonField, sizeof(lonBuf) - 1);
    lonBuf[sizeof(lonBuf) - 1] = '\0';

    const char* ewField = nmeaGetField(sentence, 4);
    char ewChar = ewField[0];

    if (strlen(latBuf) > 0 && nsChar != '\0') {
        gpsData.latitude = gpsParseCoord(latBuf, nsChar);
    }
    if (strlen(lonBuf) > 0 && ewChar != '\0') {
        gpsData.longitude = gpsParseCoord(lonBuf, ewChar);
    }

    const char* fixField = nmeaGetField(sentence, 5);
    int fixQuality = atoi(fixField);
    gpsData.fixValid = (fixQuality > 0);

    const char* satField = nmeaGetField(sentence, 6);
    gpsData.satellites = atoi(satField);

    const char* altField = nmeaGetField(sentence, 8);
    if (strlen(altField) > 0) {
        gpsData.altitude = atof(altField);
    }
}

void gpsParseRMC(const char* sentence) {
    // $GPRMC,hhmmss.ss,A/V,lat,N/S,lon,E/W,speed,course,ddmmyy,...*cs
    // Fields: 0=time, 1=status, 2=lat, 3=N/S, 4=lon, 5=E/W, 6=speed, 7=course, 8=date

    const char* statusField = nmeaGetField(sentence, 1);
    if (statusField[0] == 'A') {
        gpsData.fixValid = true;
    }

    const char* dateField = nmeaGetField(sentence, 8);
    if (strlen(dateField) >= 6) {
        char dd[3] = { dateField[0], dateField[1], '\0' };
        char mm[3] = { dateField[2], dateField[3], '\0' };
        char yy[3] = { dateField[4], dateField[5], '\0' };
        gpsData.day   = atoi(dd);
        gpsData.month = atoi(mm);
        gpsData.year  = 2000 + atoi(yy);
        gpsData.timeValid = true;
    }
}

float gpsParseCoord(const char* field, char dir) {
    // NMEA format: ddmm.mmmm (lat) or dddmm.mmmm (lon)
    float raw = atof(field);
    int degrees;
    float minutes;

    if (dir == 'N' || dir == 'S') {
        // Latitude: dd mm.mmmm
        degrees = (int)(raw / 100.0f);
        minutes = raw - (degrees * 100.0f);
    } else {
        // Longitude: ddd mm.mmmm
        degrees = (int)(raw / 100.0f);
        minutes = raw - (degrees * 100.0f);
    }

    float decimal = degrees + (minutes / 60.0f);

    if (dir == 'S' || dir == 'W') {
        decimal = -decimal;
    }

    return decimal;
}


// ============================================================================
//
//                    CALIBRATION
//
// ============================================================================

void calibrateGyroscope() {
    float sumX = 0.0f, sumY = 0.0f, sumZ = 0.0f;

    for (int i = 0; i < GYRO_CALIBRATION_SAMPLES; i++) {
        mpu6050ReadSensors();
        // Use raw values before offset correction
        sumX += (float)rawGyroX / GYRO_SENSITIVITY_250DPS;
        sumY += (float)rawGyroY / GYRO_SENSITIVITY_250DPS;
        sumZ += (float)rawGyroZ / GYRO_SENSITIVITY_250DPS;
        delay(2); // ~500Hz sampling during calibration
    }

    gyroOffsetX = sumX / (float)GYRO_CALIBRATION_SAMPLES;
    gyroOffsetY = sumY / (float)GYRO_CALIBRATION_SAMPLES;
    gyroOffsetZ = sumZ / (float)GYRO_CALIBRATION_SAMPLES;
}

void calibrateAccelerometer() {
    float sumX = 0.0f, sumY = 0.0f, sumZ = 0.0f;
    int validCount = 0;

    for (int i = 0; i < ACCEL_CALIBRATION_SAMPLES; i++) {
        mpu6050ReadSensors();

        float mag = sqrtf(accXg * accXg + accYg * accYg + accZg * accZg);

        // Only accept readings where total acceleration is close to 1g
        if (mag >= ACCEL_MAG_MIN && mag <= ACCEL_MAG_MAX) {
            sumX += accXg;
            sumY += accYg;
            sumZ += accZg;
            validCount++;
        }

        delay(4); // ~250Hz sampling during calibration
    }

    if (validCount > 0) {
        refAccX = sumX / (float)validCount;
        refAccY = sumY / (float)validCount;
        refAccZ = sumZ / (float)validCount;

        // Compute reference tilt angles (the "zero" position)
        refTiltX = atan2f(refAccY, sqrtf(refAccX * refAccX + refAccZ * refAccZ)) * RAD_TO_DEG;
        refTiltY = atan2f(-refAccX, sqrtf(refAccY * refAccY + refAccZ * refAccZ)) * RAD_TO_DEG;

        // Initialize filtered angles to reference
        filteredTiltX = refTiltX;
        filteredTiltY = refTiltY;
    } else {
        sendWarningUSB("Accel calibration: no valid samples — using defaults");
        refAccX = 0.0f;
        refAccY = 0.0f;
        refAccZ = 1.0f;
        refTiltX = 0.0f;
        refTiltY = 0.0f;
        filteredTiltX = 0.0f;
        filteredTiltY = 0.0f;
    }
}

void recalibrate() {
    sendStatusUSB("Recalibrating all sensors...");

    if (mpuAvailable) {
        sendStatusUSB("Recalibrating gyroscope...");
        calibrateGyroscope();
        sendStatusUSB("Recalibrating accelerometer...");
        calibrateAccelerometer();

        char buf[128];
        snprintf(buf, sizeof(buf),
                 "Recalibration complete: gyroOff=(%.4f, %.4f, %.4f) refTilt=(%.2f, %.2f)",
                 gyroOffsetX, gyroOffsetY, gyroOffsetZ, refTiltX, refTiltY);
        sendStatusUSB(buf);
    } else {
        sendErrorUSB("Cannot recalibrate — MPU6050 not available");
    }

    if (bmpAvailable) {
        sendStatusUSB("Re-establishing baseline pressure...");
        bmp280EstablishBaseline();
        char buf[64];
        snprintf(buf, sizeof(buf), "New baseline pressure: %.2f hPa", baselinePressure);
        sendStatusUSB(buf);
    }
}


// ============================================================================
//
//                    SENSOR FUSION
//
// ============================================================================

void runSensorFusion() {
    unsigned long nowUs = micros();
    float dt = (float)(nowUs - lastFusionMicros) / 1000000.0f;

    // Safety clamp for dt
    if (dt <= 0.0f || dt > 0.1f) {
        dt = 0.01f;
    }

    // Read sensors
    mpu6050ReadSensors();

    // ── Accelerometer-derived angles ──
    float accelTiltX = atan2f(accYg, sqrtf(accXg * accXg + accZg * accZg)) * RAD_TO_DEG;
    float accelTiltY = atan2f(-accXg, sqrtf(accYg * accYg + accZg * accZg)) * RAD_TO_DEG;

    // ── Complementary filter ──
    // Gyro integration (short-term, drift-prone but noise-immune)
    // + Accel correction (long-term, noisy but drift-free)
    filteredTiltX = ALPHA * (filteredTiltX + gyroXdps * dt) + (1.0f - ALPHA) * accelTiltX;
    filteredTiltY = ALPHA * (filteredTiltY + gyroYdps * dt) + (1.0f - ALPHA) * accelTiltY;

    // ── Apply reference correction ──
    correctedTiltX = filteredTiltX - refTiltX;
    correctedTiltY = filteredTiltY - refTiltY;

    // ── Combined angle from vertical ──
    combinedTheta = sqrtf(correctedTiltX * correctedTiltX + correctedTiltY * correctedTiltY);
}


// ============================================================================
//
//                    WATER LEVEL COMPUTATION
//
// ============================================================================

void computeWaterHeight() {
    // Store previous for rate of change
    prevWaterHeightCm = waterHeightCm;

    if (currentMode == MODE_SUBMERGED) {
        // ── SUBMERGED: pressure-based depth ──
        // gaugePressurePa = (currentPressure - baselinePressure) * 100
        // depth = P / (ρ × g)
        if (gaugePressurePa > 0) {
            estimatedDepthCm = (gaugePressurePa / (WATER_DENSITY_KGM3 * GRAVITY_MPS2)) * 100.0f;
        } else {
            estimatedDepthCm = 0.0f;
        }
        waterHeightCm = olpLengthCm + estimatedDepthCm;
        submersionState = 3;

    } else if (currentMode == MODE_FLOOD) {
        // ── FLOOD: H ≈ L ──
        float thetaRad = combinedTheta * DEG_TO_RAD;
        waterHeightCm = olpLengthCm * cosf(thetaRad);
        estimatedDepthCm = 0.0f;
        submersionState = 2;

    } else if (currentMode == MODE_TAUT) {
        // ── TAUT: H = L × cos(θ) ──
        float thetaRad = combinedTheta * DEG_TO_RAD;
        waterHeightCm = olpLengthCm * cosf(thetaRad);
        estimatedDepthCm = 0.0f;
        submersionState = 1;

    } else {
        // ── SLACK: H < L, exact value unknown ──
        waterHeightCm = 0.0f;  // Cannot measure precisely in slack mode
        estimatedDepthCm = 0.0f;
        submersionState = 0;
    }

    // Compute horizontal distance (informational)
    if (currentMode == MODE_TAUT || currentMode == MODE_FLOOD) {
        float thetaRad = combinedTheta * DEG_TO_RAD;
        horizontalDistCm = olpLengthCm * sinf(thetaRad);
    } else {
        horizontalDistCm = 0.0f;
    }

    // Compute flood ratio
    if (hMaxCm > 0.0f) {
        floodRatio = waterHeightCm / hMaxCm;
        if (floodRatio > 1.0f) floodRatio = 1.0f; // can exceed during submersion
        if (floodRatio < 0.0f) floodRatio = 0.0f;
    } else {
        floodRatio = 0.0f;
    }
}


// ============================================================================
//
//                    FLOOD MODE CLASSIFICATION
//
// ============================================================================

void classifyFloodMode() {
    FloodMode detected = MODE_SLACK;

    // ── Priority 1: Check submersion (highest priority) ──
    if (bmpAvailable && gaugePressurePa > threshSubmersionPa) {
        detected = MODE_SUBMERGED;
    }
    // ── Priority 2: Check if tether is taut ──
    else if (lateralAccel > threshLateralAccel && combinedTheta > threshTiltDeg) {
        // Tether is taut — now determine if flood level or still approaching
        float thetaRad = combinedTheta * DEG_TO_RAD;
        float ratio = cosf(thetaRad); // H/L ratio

        if (combinedTheta < threshFloodTiltDeg && ratio > threshFloodRatio) {
            // Nearly vertical, ratio approaching 1.0 → flood
            detected = MODE_FLOOD;
        } else {
            // Still at an angle → taut but pre-flood
            detected = MODE_TAUT;
        }
    }
    // ── Priority 3: Check slack with hysteresis ──
    else {
        // For returning to slack, use lower threshold (hysteresis)
        if (currentMode == MODE_TAUT || currentMode == MODE_FLOOD) {
            // Currently taut/flood — need lateral accel to drop below release threshold
            if (lateralAccel < threshLateralRelease) {
                detected = MODE_SLACK;
            } else {
                detected = currentMode; // Stay in current mode
            }
        } else {
            detected = MODE_SLACK;
        }
    }

    // ── Persistence filter ──
    int requiredPersistence;
    if (detected == MODE_SUBMERGED) {
        requiredPersistence = PERSISTENCE_SUBMERGE; // Urgent: only 3 readings
    } else {
        requiredPersistence = PERSISTENCE_NORMAL;   // Normal: 10 readings
    }

    if (detected == pendingMode) {
        persistenceCount++;
        if (persistenceCount >= requiredPersistence) {
            if (currentMode != detected) {
                char buf[128];
                snprintf(buf, sizeof(buf),
                         "Mode transition: %d -> %d (after %d consistent readings)",
                         currentMode, detected, persistenceCount);
                sendStatusUSB(buf);
            }
            currentMode = detected;
            persistenceCount = requiredPersistence; // Clamp
        }
    } else {
        pendingMode = detected;
        persistenceCount = 1;
    }
}

void updateAlertLevel() {
    switch (currentMode) {
        case MODE_SLACK:
            floodAlertLevel = ALERT_GREEN;
            break;
        case MODE_TAUT:
            // Yellow when > 50% of max, green when below
            if (floodRatio > 0.50f) {
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

void classifyZone() {
    // Zone 0: Safe (H < 50% of max)
    // Zone 1: Watch (50-70%)
    // Zone 2: Warning (70-95%)
    // Zone 3: Danger (>95% or flood/submerged)
    if (currentMode == MODE_SUBMERGED || currentMode == MODE_FLOOD) {
        currentZone = 3;
    } else if (floodRatio > 0.95f) {
        currentZone = 3;
    } else if (floodRatio > 0.70f) {
        currentZone = 2;
    } else if (floodRatio > 0.50f) {
        currentZone = 1;
    } else {
        currentZone = 0;
    }
}

void computeResponseLevel() {
    // Response levels based on zone and rate of change
    switch (currentZone) {
        case 0:
            responseLevel = RESPONSE_NONE;
            break;
        case 1:
            responseLevel = sustainedRise ? RESPONSE_WARNING : RESPONSE_MONITOR;
            break;
        case 2:
            responseLevel = sustainedRise ? RESPONSE_ALERT : RESPONSE_WARNING;
            break;
        case 3:
            responseLevel = RESPONSE_EMERGENCY;
            break;
        default:
            responseLevel = RESPONSE_NONE;
    }
}


// ============================================================================
//
//                    RATE OF CHANGE COMPUTATION
//
// ============================================================================

void computeRateOfChange() {
    unsigned long nowMs = millis();
    unsigned long elapsedMs = nowMs - lastRateCalcMs;

    // Compute rate of change every transmit cycle
    if (elapsedMs > 0) {
        float elapsedMin = (float)elapsedMs / 60000.0f;
        float deltaH = waterHeightCm - lastRateCalcHeight;

        if (elapsedMin > 0.0f) {
            // Rate in cm per 15 minutes
            rateOfChangePer15Min = (deltaH / elapsedMin) * 15.0f;
        }

        // Track sustained rise (3+ consecutive positive changes)
        if (deltaH > 0.5f) {  // At least 0.5cm rise
            riseConsecutiveCount++;
        } else {
            riseConsecutiveCount = 0;
        }
        sustainedRise = (riseConsecutiveCount >= 3);

        lastRateCalcMs = nowMs;
        lastRateCalcHeight = waterHeightCm;
    }
}


// ============================================================================
//
//                    DYNAMIC SAMPLING RATE
//
// ============================================================================

int computeDynamicInterval() {
    // Below 50% of H-max: normal rate
    // Between 50% and 80%: linear interpolation
    // Above 80%: high rate
    // MODE_FLOOD or MODE_SUBMERGED: always high rate

    if (currentMode == MODE_FLOOD || currentMode == MODE_SUBMERGED) {
        return highRateSec;
    }

    if (floodRatio <= TRANSITION_RATIO_LOW) {
        return normalRateSec;
    }

    if (floodRatio >= TRANSITION_RATIO_HIGH) {
        return highRateSec;
    }

    // Linear interpolation between low and high ratio
    float t = (floodRatio - TRANSITION_RATIO_LOW) / (TRANSITION_RATIO_HIGH - TRANSITION_RATIO_LOW);
    int interval = (int)((float)normalRateSec + t * ((float)highRateSec - (float)normalRateSec));

    // Clamp
    if (interval < highRateSec) interval = highRateSec;
    if (interval > normalRateSec) interval = normalRateSec;

    return interval;
}

void updateSamplingRate() {
    int newInterval = computeDynamicInterval();

    if (newInterval != currentSampleIntervalSec) {
        char buf[128];
        snprintf(buf, sizeof(buf),
                 "Sampling interval changed: %d sec -> %d sec (flood ratio: %.2f)",
                 currentSampleIntervalSec, newInterval, floodRatio);
        sendStatusUSB(buf);
    }

    currentSampleIntervalSec = newInterval;
    currentTransmitIntervalSec = newInterval;
}


// ============================================================================
//
//                    BATTERY MEASUREMENT
//
// ============================================================================

void readBattery() {
    // 16-sample averaged ADC reading
    long sum = 0;
    for (int i = 0; i < 16; i++) {
        sum += analogRead(PIN_BATTERY_ADC);
        delayMicroseconds(100);
    }
    float adcAvg = (float)sum / 16.0f;

    // Convert ADC to voltage
    // ESP32-S3 ADC: 12-bit, 0-3.3V with 11db attenuation (0-3.6V effective)
    // Voltage divider: assuming 2:1 divider (100k/100k)
    // V_batt = V_adc × 2
    float adcVoltage = (adcAvg / 4095.0f) * 3.3f;
    batteryVoltage = adcVoltage * 2.0f; // Voltage divider scaling

    // LiPo percentage mapping
    // 4.2V = 100%, 3.7V = ~50%, 3.0V = 0%
    if (batteryVoltage >= 4.2f) {
        batteryPercent = 100.0f;
    } else if (batteryVoltage <= 3.0f) {
        batteryPercent = 0.0f;
    } else {
        // Non-linear LiPo discharge curve approximation
        if (batteryVoltage >= 3.9f) {
            // 3.9V-4.2V → 70%-100%
            batteryPercent = 70.0f + ((batteryVoltage - 3.9f) / 0.3f) * 30.0f;
        } else if (batteryVoltage >= 3.7f) {
            // 3.7V-3.9V → 30%-70%
            batteryPercent = 30.0f + ((batteryVoltage - 3.7f) / 0.2f) * 40.0f;
        } else {
            // 3.0V-3.7V → 0%-30%
            batteryPercent = ((batteryVoltage - 3.0f) / 0.7f) * 30.0f;
        }
    }
}


// ============================================================================
//
//                    HEALTH SCORE COMPUTATION
//
// ============================================================================

void computeHealthScore() {
    healthScore = 0;
    if (mpuAvailable)  healthScore += 40;
    if (bmpAvailable)  healthScore += 30;
    if (rtcAvailable)  healthScore += 20;
    if (gpsData.available) healthScore += 10;
}


// ============================================================================
//
//                    SOFTWARE UART (GPIO 14, 9600 BAUD)
//
// ============================================================================

void IRAM_ATTR c3UartSendByte(uint8_t b) {
    // Software UART transmitter at 9600 baud
    // Bit duration: 1/9600 = ~104.17 µs
    // Frame: 1 start bit (LOW) + 8 data bits (LSB first) + 1 stop bit (HIGH)

    portDISABLE_INTERRUPTS();

    // Start bit (LOW)
    digitalWrite(PIN_C3_CSV_OUT, LOW);
    delayMicroseconds(SOFT_UART_BIT_DURATION_US);

    // Data bits (LSB first)
    for (int i = 0; i < 8; i++) {
        if (b & (1 << i)) {
            digitalWrite(PIN_C3_CSV_OUT, HIGH);
        } else {
            digitalWrite(PIN_C3_CSV_OUT, LOW);
        }
        delayMicroseconds(SOFT_UART_BIT_DURATION_US);
    }

    // Stop bit (HIGH)
    digitalWrite(PIN_C3_CSV_OUT, HIGH);
    delayMicroseconds(SOFT_UART_BIT_DURATION_US);

    portENABLE_INTERRUPTS();
}

void c3UartSendString(const char* str) {
    while (*str) {
        c3UartSendByte((uint8_t)*str);
        str++;
    }
}


// ============================================================================
//
//                    CSV BUILDER — 39 FIELDS
//
// ============================================================================

void buildCSVString(char* buf, int bufSize) {
    // Build timestamp string
    // Prefer RTC, fallback to GPS time
    if (rtcAvailable && rtcTime.valid) {
        // dateTimeString already formatted from ds1307Read()
    } else if (gpsData.timeValid) {
        snprintf(dateTimeString, sizeof(dateTimeString),
                 "%04d-%02d-%02d %02d:%02d:%02d",
                 gpsData.year, gpsData.month, gpsData.day,
                 gpsData.hour, gpsData.minute, gpsData.second);
        unixTime = dateTimeToUnix(gpsData.year, gpsData.month, gpsData.day,
                                  gpsData.hour, gpsData.minute, gpsData.second);
    } else {
        snprintf(dateTimeString, sizeof(dateTimeString), "0000-00-00 00:00:00");
        unixTime = 0;
    }

    // Session duration in seconds
    unsigned long sessionDurationSec = (millis() - sessionStartMs) / 1000UL;

    // Build the 39-field CSV
    // Fields separated by commas, terminated by newline
    snprintf(buf, bufSize,
        "%.2f,"    // 1:  theta (degrees)
        "%.2f,"    // 2:  waterHeight (cm)
        "%.2f,"    // 3:  correctedTiltX (deg)
        "%.2f,"    // 4:  correctedTiltY (deg)
        "%.2f,"    // 5:  olpLength (cm)
        "%.2f,"    // 6:  horizontalDist (cm)
        "%.2f,"    // 7:  currentPressure (hPa)
        "%.2f,"    // 8:  currentTemperature (°C)
        "%.2f,"    // 9:  baselinePressure (hPa)
        "%.2f,"    // 10: pressureDeviation (hPa)
        "%d,"      // 11: submersionState (0-3)
        "%.2f,"    // 12: estimatedDepth (cm)
        "%d,"      // 13: bmpAvailable (0/1)
        "%lu,"     // 14: unixTime
        "%s,"      // 15: dateTimeString
        "%d,"      // 16: rtcValid (0/1)
        "%.3f,"    // 17: ratePer15Min (cm/15m)
        "%d,"      // 18: floodAlertLevel (0-3)
        "%lu,"     // 19: sessionDuration (sec)
        "%.2f,"    // 20: peakHeight (cm)
        "%.2f,"    // 21: minHeight (cm)
        "%.6f,"    // 22: latitude
        "%.6f,"    // 23: longitude
        "%.1f,"    // 24: altitude (m)
        "%d,"      // 25: gpsSatellites
        "%d,"      // 26: gpsFixValid (0/1)
        "%d,"      // 27: simSignalRSSI
        "%d,"      // 28: simRegistered (0/1)
        "%d,"      // 29: simAvailable (0/1)
        "%d,"      // 30: currentZone (0-3)
        "%d,"      // 31: currentResponseLevel (0-4)
        "%d,"      // 32: sustainedRise (0/1)
        "%.1f,"    // 33: batteryPercent
        "%d,"      // 34: sampleInterval (sec)
        "%d,"      // 35: transmitInterval (sec)
        "%d,"      // 36: obLightEnabled (0/1)
        "%d,"      // 37: algorithmEnabled (0/1)
        "%d,"      // 38: currentMode (0-3)
        "%d\n",    // 39: healthScore (0-100)

        combinedTheta,                          // 1
        waterHeightCm,                          // 2
        correctedTiltX,                         // 3
        correctedTiltY,                         // 4
        olpLengthCm,                            // 5
        horizontalDistCm,                       // 6
        currentPressure,                        // 7
        currentTemperature,                     // 8
        baselinePressure,                       // 9
        pressureDeviation,                      // 10
        submersionState,                        // 11
        estimatedDepthCm,                       // 12
        bmpAvailable ? 1 : 0,                   // 13
        unixTime,                               // 14
        dateTimeString,                         // 15
        (rtcAvailable && rtcTime.valid) ? 1 : 0,// 16
        rateOfChangePer15Min,                   // 17
        (int)floodAlertLevel,                   // 18
        sessionDurationSec,                     // 19
        peakHeightCm,                           // 20
        (minHeightCm < 99999.0f) ? minHeightCm : 0.0f, // 21
        gpsData.latitude,                       // 22
        gpsData.longitude,                      // 23
        gpsData.altitude,                       // 24
        gpsData.satellites,                     // 25
        gpsData.fixValid ? 1 : 0,               // 26
        simSignalRSSI,                          // 27
        simRegistered ? 1 : 0,                  // 28
        simModuleAvailable ? 1 : 0,             // 29
        currentZone,                            // 30
        (int)responseLevel,                     // 31
        sustainedRise ? 1 : 0,                  // 32
        batteryPercent,                         // 33
        currentSampleIntervalSec,               // 34
        currentTransmitIntervalSec,             // 35
        obLightEnabled ? 1 : 0,                 // 36
        algorithmEnabled ? 1 : 0,               // 37
        (int)currentMode,                       // 38
        healthScore                             // 39
    );
}


// ============================================================================
//
//                    TRANSMIT FUNCTIONS
//
// ============================================================================

void transmitCSVtoC3(const char* csv) {
    // Send via software UART on GPIO 14 at 9600 baud
    c3UartSendString(csv);
}

void transmitCSVtoUSB(const char* csv) {
    // Send via USB Serial at 115200 baud
    Serial.print(csv);
}


// ============================================================================
//
//                    COMMAND PROCESSING — C3 CHANNEL
//
// ============================================================================

void processC3Commands() {
    while (Serial2.available()) {
        char c = Serial2.read();

        if (c == '\n' || c == '\r') {
            if (c3CmdIdx > 0) {
                c3CmdBuffer[c3CmdIdx] = '\0';
                handleC3Command(c3CmdBuffer);
                c3CmdIdx = 0;
            }
        } else {
            if (c3CmdIdx < (int)(sizeof(c3CmdBuffer) - 1)) {
                c3CmdBuffer[c3CmdIdx++] = c;
            } else {
                c3CmdIdx = 0; // Buffer overflow, reset
            }
        }
    }
}

void handleC3Command(const char* cmd) {
    // ── $CFG,normal_sec,high_sec,h_max_cm ──
    if (strncmp(cmd, "$CFG,", 5) == 0) {
        int newNormal = 0, newHigh = 0;
        float newHMax = 0.0f;
        int parsed = sscanf(cmd + 5, "%d,%d,%f", &newNormal, &newHigh, &newHMax);

        if (parsed >= 2) {
            // Validate ranges
            if (newNormal >= 10 && newNormal <= 86400) {
                normalRateSec = newNormal;
            }
            if (newHigh >= 5 && newHigh <= 3600) {
                highRateSec = newHigh;
            }
            if (parsed >= 3 && newHMax >= 10.0f && newHMax <= 10000.0f) {
                hMaxCm = newHMax;
                olpLengthCm = newHMax; // OLP length equals flood threshold
            }

            char buf[256];
            snprintf(buf, sizeof(buf),
                     "STATUS: Config updated from C3: normalRate=%d sec, highRate=%d sec, hMax=%.1f cm",
                     normalRateSec, highRateSec, hMaxCm);
            Serial.println(buf);

            // Send acknowledgment back to C3
            Serial2.println("$CFG_ACK");
        } else {
            Serial.println("ERROR: Invalid $CFG format from C3");
            Serial2.println("$CFG_NAK,PARSE_ERROR");
        }
    }
    // ── $DIAGRUN — manual diagnostic request ──
    else if (strcmp(cmd, "$DIAGRUN") == 0) {
        diagRequested = true;
        Serial.println("STATUS: Diagnostic requested by C3");
        Serial2.println("$DIAGRUN_ACK");
    }
    // ── $PING — connectivity check ──
    else if (strcmp(cmd, "$PING") == 0) {
        Serial2.println("$PONG");
        Serial.println("STATUS: Received $PING from C3, sent $PONG");
    }
    // ── $PONG — response to our diagnostic ping ──
    else if (strcmp(cmd, "$PONG") == 0) {
        if (waitingForPong) {
            waitingForPong = false;
            lastDiag.c3PongReceived = true;
            Serial.println("STATUS: Received $PONG from C3 — comm link verified");
            // Now finish and send the diagnostic
            sendDiagToC3();
        }
    }
    // ── $SETSIM,rssi,registered,available — SIM status update from C3 ──
    else if (strncmp(cmd, "$SETSIM,", 8) == 0) {
        int rssi = 0, reg = 0, avail = 0;
        if (sscanf(cmd + 8, "%d,%d,%d", &rssi, &reg, &avail) == 3) {
            simSignalRSSI = rssi;
            simRegistered = (reg != 0);
            simModuleAvailable = (avail != 0);
            Serial.println("STATUS: SIM status updated from C3");
        }
    }
    // ── Unknown command ──
    else {
        char buf[128];
        snprintf(buf, sizeof(buf), "WARNING: Unknown command from C3: %s", cmd);
        Serial.println(buf);
    }
}


// ============================================================================
//
//                    COMMAND PROCESSING — USB DEBUGGER
//
// ============================================================================

void processUSBCommands() {
    while (Serial.available()) {
        char c = Serial.read();

        if (c == '\n' || c == '\r') {
            if (usbCmdIdx > 0) {
                usbCmdBuffer[usbCmdIdx] = '\0';
                handleUSBCommand(usbCmdBuffer);
                usbCmdIdx = 0;
            }
        } else {
            if (usbCmdIdx < (int)(sizeof(usbCmdBuffer) - 1)) {
                usbCmdBuffer[usbCmdIdx++] = c;
            } else {
                usbCmdIdx = 0;
            }
        }
    }
}

void handleUSBCommand(const char* cmd) {
    // ── PING ──
    if (strcmp(cmd, "PING") == 0) {
        sendStatusUSB("PONG");
    }
    // ── GETCONFIG ──
    else if (strcmp(cmd, "GETCONFIG") == 0) {
        char buf[256];
        snprintf(buf, sizeof(buf),
                 "CONFIG: normalRate=%d sec, highRate=%d sec, hMax=%.1f cm, "
                 "olpLen=%.1f cm, mode=%d, interval=%d sec, health=%d",
                 normalRateSec, highRateSec, hMaxCm,
                 olpLengthCm, (int)currentMode, currentSampleIntervalSec, healthScore);
        sendStatusUSB(buf);
    }
    // ── RECALIBRATE ──
    else if (strcmp(cmd, "RECALIBRATE") == 0) {
        recalibrate();
    }
    // ── GETTHRESH ──
    else if (strcmp(cmd, "GETTHRESH") == 0) {
        char buf[256];
        snprintf(buf, sizeof(buf),
                 "THRESH: lateralAccel=%.3f m/s², tiltDeg=%.1f, "
                 "floodTilt=%.1f, floodRatio=%.2f, submersionPa=%.1f, "
                 "lateralRelease=%.3f m/s²",
                 threshLateralAccel, threshTiltDeg,
                 threshFloodTiltDeg, threshFloodRatio, threshSubmersionPa,
                 threshLateralRelease);
        sendStatusUSB(buf);
    }
    // ── SETTHRESH=A,W,D ──
    // A = lateral acceleration threshold
    // W = tilt threshold (taut detection)
    // D = submersion pressure threshold (Pa)
    else if (strncmp(cmd, "SETTHRESH=", 10) == 0) {
        float a = 0, w = 0, d = 0;
        int parsed = sscanf(cmd + 10, "%f,%f,%f", &a, &w, &d);
        if (parsed == 3) {
            if (a >= 0.05f && a <= 2.0f) threshLateralAccel = a;
            if (w >= 1.0f && w <= 45.0f) threshTiltDeg = w;
            if (d >= 100.0f && d <= 10000.0f) threshSubmersionPa = d;

            // Update hysteresis release threshold
            threshLateralRelease = threshLateralAccel * 0.667f;

            char buf[128];
            snprintf(buf, sizeof(buf),
                     "Thresholds set: accel=%.3f, tilt=%.1f, submersion=%.1f Pa",
                     threshLateralAccel, threshTiltDeg, threshSubmersionPa);
            sendStatusUSB(buf);
        } else {
            sendErrorUSB("SETTHRESH format: SETTHRESH=accel,tilt,pressure");
        }
    }
    // ── RESETTHRESH ──
    else if (strcmp(cmd, "RESETTHRESH") == 0) {
        threshLateralAccel   = DEFAULT_LATERAL_ACCEL_THRESHOLD;
        threshLateralRelease = DEFAULT_LATERAL_ACCEL_RELEASE;
        threshTiltDeg        = DEFAULT_TILT_THRESHOLD_DEG;
        threshFloodTiltDeg   = DEFAULT_FLOOD_TILT_DEG;
        threshFloodRatio     = DEFAULT_FLOOD_RATIO;
        threshSubmersionPa   = DEFAULT_SUBMERSION_PRESSURE_PA;
        sendStatusUSB("All thresholds reset to defaults");
    }
    // ── SETOLP=length_cm ──
    else if (strncmp(cmd, "SETOLP=", 7) == 0) {
        float newLen = atof(cmd + 7);
        if (newLen >= 10.0f && newLen <= 10000.0f) {
            olpLengthCm = newLen;
            hMaxCm = newLen;
            char buf[64];
            snprintf(buf, sizeof(buf), "OLP length set to %.1f cm (H_max = %.1f cm)",
                     olpLengthCm, hMaxCm);
            sendStatusUSB(buf);
        } else {
            sendErrorUSB("SETOLP: length must be 10-10000 cm");
        }
    }
    // ── SETNORMAL=seconds ──
    else if (strncmp(cmd, "SETNORMAL=", 10) == 0) {
        int val = atoi(cmd + 10);
        if (val >= 10 && val <= 86400) {
            normalRateSec = val;
            char buf[64];
            snprintf(buf, sizeof(buf), "Normal rate set to %d sec", normalRateSec);
            sendStatusUSB(buf);
        } else {
            sendErrorUSB("SETNORMAL: must be 10-86400 seconds");
        }
    }
    // ── SETHIGH=seconds ──
    else if (strncmp(cmd, "SETHIGH=", 8) == 0) {
        int val = atoi(cmd + 8);
        if (val >= 5 && val <= 3600) {
            highRateSec = val;
            char buf[64];
            snprintf(buf, sizeof(buf), "High rate set to %d sec", highRateSec);
            sendStatusUSB(buf);
        } else {
            sendErrorUSB("SETHIGH: must be 5-3600 seconds");
        }
    }
    // ── GETSTATUS ──
    else if (strcmp(cmd, "GETSTATUS") == 0) {
        char buf[512];
        snprintf(buf, sizeof(buf),
                 "MODE=%d ALERT=%d ZONE=%d RESP=%d "
                 "THETA=%.2f H=%.2f RATIO=%.3f "
                 "LAT_ACCEL=%.3f GAUGE_P=%.1f Pa "
                 "BATT=%.1f%% HEALTH=%d "
                 "RATE=%d/%d sec PERSIST=%d/%d",
                 (int)currentMode, (int)floodAlertLevel, currentZone,
                 (int)responseLevel,
                 combinedTheta, waterHeightCm, floodRatio,
                 lateralAccel, gaugePressurePa,
                 batteryPercent, healthScore,
                 currentSampleIntervalSec, normalRateSec,
                 persistenceCount,
                 (pendingMode == MODE_SUBMERGED) ? PERSISTENCE_SUBMERGE : PERSISTENCE_NORMAL);
        sendStatusUSB(buf);
    }
    // ── SETAPN (placeholder for SIM configuration forwarding) ──
    else if (strncmp(cmd, "SETAPN", 6) == 0) {
        // Forward to C3 via Serial2
        Serial2.print("$");
        Serial2.println(cmd);
        sendStatusUSB("APN command forwarded to C3");
    }
    // ── REINITSIM ──
    else if (strcmp(cmd, "REINITSIM") == 0) {
        Serial2.println("$REINITSIM");
        sendStatusUSB("REINITSIM forwarded to C3");
    }
    // ── TESTGPRS ──
    else if (strcmp(cmd, "TESTGPRS") == 0) {
        Serial2.println("$TESTGPRS");
        sendStatusUSB("TESTGPRS forwarded to C3");
    }
    // ── DIAGRUN ──
    else if (strcmp(cmd, "DIAGRUN") == 0) {
        diagRequested = true;
        sendStatusUSB("Manual diagnostic requested");
    }
    // ── Unknown ──
    else {
        char buf[128];
        snprintf(buf, sizeof(buf), "Unknown command: %s", cmd);
        sendErrorUSB(buf);
    }
}


// ============================================================================
//
//                    DIAGNOSTICS
//
// ============================================================================

void runDiagnostics() {
    sendStatusUSB("══════ DIAGNOSTIC RUN START ══════");
    memset(&lastDiag, 0, sizeof(DiagResults));
    lastDiag.totalFaults = 0;

    // ──────────────────────────────────────────
    // MPU6050 Diagnostics
    // ──────────────────────────────────────────
    sendStatusUSB("Testing MPU6050...");

    // WHO_AM_I check
    uint8_t whoami = mpu6050ReadReg(MPU6050_REG_WHO_AM_I);
    lastDiag.mpuWhoAmIOk = (whoami == MPU6050_WHO_AM_I_VAL);
    if (!lastDiag.mpuWhoAmIOk) {
        lastDiag.totalFaults++;
        sendErrorUSB("MPU6050 WHO_AM_I FAILED");
    }

    // Accelerometer magnitude sanity
    mpu6050ReadSensors();
    lastDiag.mpuAccelMag = sqrtf(accXg * accXg + accYg * accYg + accZg * accZg);
    lastDiag.mpuAccelMagOk = (lastDiag.mpuAccelMag >= 0.8f && lastDiag.mpuAccelMag <= 1.2f);
    if (!lastDiag.mpuAccelMagOk) {
        lastDiag.totalFaults++;
        char buf[64];
        snprintf(buf, sizeof(buf), "MPU6050 accel magnitude out of range: %.3fg", lastDiag.mpuAccelMag);
        sendWarningUSB(buf);
    }

    // Gyro drift measurement (100 samples, check for excessive drift)
    float gyroDriftSum = 0.0f;
    for (int i = 0; i < 100; i++) {
        mpu6050ReadSensors();
        gyroDriftSum += sqrtf(gyroXdps * gyroXdps + gyroYdps * gyroYdps + gyroZdps * gyroZdps);
        delay(2);
    }
    lastDiag.mpuGyroDrift = gyroDriftSum / 100.0f;
    lastDiag.mpuGyroDriftOk = (lastDiag.mpuGyroDrift < 5.0f); // <5°/s average
    if (!lastDiag.mpuGyroDriftOk) {
        lastDiag.totalFaults++;
        sendWarningUSB("MPU6050 gyro drift excessive");
    }

    lastDiag.mpuOverallOk = lastDiag.mpuWhoAmIOk && lastDiag.mpuAccelMagOk && lastDiag.mpuGyroDriftOk;
    mpuAvailable = lastDiag.mpuOverallOk;

    // ──────────────────────────────────────────
    // BMP280 Diagnostics
    // ──────────────────────────────────────────
    sendStatusUSB("Testing BMP280...");

    uint8_t chipId = bmp280ReadReg(BMP280_REG_CHIP_ID);
    lastDiag.bmpChipIdOk = (chipId == BMP280_CHIP_ID_VAL);
    if (!lastDiag.bmpChipIdOk) {
        lastDiag.totalFaults++;
        sendErrorUSB("BMP280 chip ID FAILED");
    }

    if (lastDiag.bmpChipIdOk) {
        bmp280ReadAll();
        lastDiag.bmpPressure = currentPressure;
        lastDiag.bmpTemperature = currentTemperature;

        lastDiag.bmpPressureRangeOk = (currentPressure >= 300.0f && currentPressure <= 1100.0f);
        lastDiag.bmpTempRangeOk = (currentTemperature >= -40.0f && currentTemperature <= 85.0f);

        if (!lastDiag.bmpPressureRangeOk) {
            lastDiag.totalFaults++;
            sendWarningUSB("BMP280 pressure out of range");
        }
        if (!lastDiag.bmpTempRangeOk) {
            lastDiag.totalFaults++;
            sendWarningUSB("BMP280 temperature out of range");
        }
    }

    lastDiag.bmpOverallOk = lastDiag.bmpChipIdOk && lastDiag.bmpPressureRangeOk && lastDiag.bmpTempRangeOk;
    bmpAvailable = lastDiag.bmpOverallOk;

    // ──────────────────────────────────────────
    // DS1307 RTC Diagnostics
    // ──────────────────────────────────────────
    sendStatusUSB("Testing DS1307 RTC...");

    I2C_Bus1.beginTransmission(DS1307_ADDR);
    I2C_Bus1.write(DS1307_REG_SECONDS);
    uint8_t rtcErr = I2C_Bus1.endTransmission();

    if (rtcErr == 0) {
        I2C_Bus1.requestFrom(DS1307_ADDR, (uint8_t)1);
        if (I2C_Bus1.available()) {
            uint8_t sec = I2C_Bus1.read();
            lastDiag.rtcOscRunning = !(sec & 0x80);
        } else {
            lastDiag.rtcOscRunning = false;
        }

        ds1307Read();
        lastDiag.rtcTimeValid = rtcTime.valid;
    } else {
        lastDiag.rtcOscRunning = false;
        lastDiag.rtcTimeValid = false;
    }

    if (!lastDiag.rtcOscRunning) {
        lastDiag.totalFaults++;
        sendWarningUSB("RTC oscillator not running");
    }
    if (!lastDiag.rtcTimeValid) {
        lastDiag.totalFaults++;
        sendWarningUSB("RTC time invalid");
    }

    lastDiag.rtcOverallOk = lastDiag.rtcOscRunning && lastDiag.rtcTimeValid;
    rtcAvailable = lastDiag.rtcOverallOk;

    // ──────────────────────────────────────────
    // GPS Diagnostics
    // ──────────────────────────────────────────
    sendStatusUSB("Testing GPS...");

    // Check if we've received any UART data from GPS recently
    unsigned long gpsTestStart = millis();
    bool gpsHeard = false;
    while (millis() - gpsTestStart < 2000) {
        if (Serial1.available()) {
            Serial1.read(); // Consume
            gpsHeard = true;
            break;
        }
        delay(10);
    }

    lastDiag.gpsUartActive = gpsHeard || gpsData.available;
    if (!lastDiag.gpsUartActive) {
        lastDiag.totalFaults++;
        sendWarningUSB("GPS UART inactive — no data received");
    }
    lastDiag.gpsOverallOk = lastDiag.gpsUartActive;

    // ──────────────────────────────────────────
    // Battery Diagnostics
    // ──────────────────────────────────────────
    sendStatusUSB("Testing battery...");
    readBattery();
    lastDiag.batteryVoltage = batteryVoltage;
    lastDiag.batteryVoltageOk = (batteryVoltage >= 2.8f && batteryVoltage <= 4.3f);
    if (!lastDiag.batteryVoltageOk) {
        lastDiag.totalFaults++;
        sendWarningUSB("Battery voltage out of range");
    }

    // ──────────────────────────────────────────
    // C3 Communication Test (ping/pong)
    // ──────────────────────────────────────────
    sendStatusUSB("Testing C3 communication link...");
    Serial2.println("$PING");
    waitingForPong = true;
    pongWaitStartMs = millis();
    lastDiag.c3PongReceived = false;

    // The pong will be processed in the main loop via handleC3Command
    // When received (or timed out), sendDiagToC3() will be called

    // Compute health score for diagnostic report
    lastDiag.healthScore = 0;
    if (lastDiag.mpuOverallOk) lastDiag.healthScore += 40;
    if (lastDiag.bmpOverallOk) lastDiag.healthScore += 30;
    if (lastDiag.rtcOverallOk) lastDiag.healthScore += 20;
    if (lastDiag.gpsOverallOk) lastDiag.healthScore += 10;

    sendStatusUSB("══════ DIAGNOSTIC SENSORS DONE ══════");
    sendStatusUSB("Waiting for C3 pong response...");

    // If C3 doesn't respond, the timeout in the main loop will call sendDiagToC3()
}

void sendDiagToC3() {
    // Build $DIAG frame and send to C3 via Serial2
    char diagFrame[512];

    snprintf(diagFrame, sizeof(diagFrame),
        "$DIAG,"
        "MPU_WHO=%d,MPU_ACCEL=%.3f,MPU_ACCEL_OK=%d,MPU_DRIFT=%.2f,MPU_DRIFT_OK=%d,MPU_OK=%d,"
        "BMP_ID=%d,BMP_P=%.2f,BMP_P_OK=%d,BMP_T=%.2f,BMP_T_OK=%d,BMP_OK=%d,"
        "RTC_OSC=%d,RTC_TIME=%d,RTC_OK=%d,"
        "GPS_UART=%d,GPS_OK=%d,"
        "BATT_V=%.2f,BATT_OK=%d,"
        "C3_PONG=%d,"
        "FAULTS=%d,HEALTH=%d",

        lastDiag.mpuWhoAmIOk ? 1 : 0,
        lastDiag.mpuAccelMag,
        lastDiag.mpuAccelMagOk ? 1 : 0,
        lastDiag.mpuGyroDrift,
        lastDiag.mpuGyroDriftOk ? 1 : 0,
        lastDiag.mpuOverallOk ? 1 : 0,

        lastDiag.bmpChipIdOk ? 1 : 0,
        lastDiag.bmpPressure,
        lastDiag.bmpPressureRangeOk ? 1 : 0,
        lastDiag.bmpTemperature,
        lastDiag.bmpTempRangeOk ? 1 : 0,
        lastDiag.bmpOverallOk ? 1 : 0,

        lastDiag.rtcOscRunning ? 1 : 0,
        lastDiag.rtcTimeValid ? 1 : 0,
        lastDiag.rtcOverallOk ? 1 : 0,

        lastDiag.gpsUartActive ? 1 : 0,
        lastDiag.gpsOverallOk ? 1 : 0,

        lastDiag.batteryVoltage,
        lastDiag.batteryVoltageOk ? 1 : 0,

        lastDiag.c3PongReceived ? 1 : 0,

        lastDiag.totalFaults,
        lastDiag.healthScore
    );

    Serial2.println(diagFrame);

    // Also output to USB debugger
    Serial.println(diagFrame);

    char summary[128];
    snprintf(summary, sizeof(summary),
             "STATUS: Diagnostic complete — %d faults, health score: %d/100",
             lastDiag.totalFaults, lastDiag.healthScore);
    Serial.println(summary);

    if (!lastDiag.c3PongReceived) {
        sendWarningUSB("C3 did not respond to $PING — communication link may be down");
    }

    Serial.println("STATUS: ══════ DIAGNOSTIC RUN END ══════");
}


// ============================================================================
//
//                    STATUS/ERROR/WARNING OUTPUT
//
// ============================================================================

void sendStatusUSB(const char* msg) {
    Serial.print("STATUS: ");
    Serial.println(msg);
}

void sendErrorUSB(const char* msg) {
    Serial.print("ERROR: ");
    Serial.println(msg);
}

void sendWarningUSB(const char* msg) {
    Serial.print("WARNING: ");
    Serial.println(msg);
}
