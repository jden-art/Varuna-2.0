```
/*
 * ============================================================
 *   V A R U N A   F L O O D   M O N I T O R I N G   S Y S T E M
 *   ESP32-S3 Main Firmware — Combined Phase 1 + 2 + 3 + COMMS
 * ============================================================
 *
 *  PHASE 1 : Individual sensor drivers (MPU6050, BMP280, GPS, SIM800L, RTC)
 *  PHASE 2 : Core measurement engine — tilt → water height (H = L·cos θ)
 *            Four operating modes (SLACK / TAUT / FLOOD / SUBMERGED)
 *            Complementary filter (α = 0.98)
 *            Lateral-acceleration tether-tension detection
 *  PHASE 3 : Flood detection state machine
 *            3-D decision matrix [zone][rate][sustained]
 *            Step-down (de-escalation) algorithm with hysteresis
 *            Sustained-rise detection (4-entry circular buffer)
 *            Override mechanisms (cold start, extreme / catastrophic rate)
 *            EEPROM persistence, adaptive sampling, health score
 *  COMMS   : SIM800L GPRS HTTP POST (primary)
 *            ESP32 WiFi + Firebase HTTPS (fallback)
 *            GPS NMEA parsing (UART1)
 *            Dual-path telemetry with local store-and-forward
 *
 *  Board  : ESP32-S3 (any variant)
 *  Author : VARUNA Project
 * ============================================================
 */

// ─────────────────────────────────────────────────────────────────
//  LIBRARIES
// ─────────────────────────────────────────────────────────────────
#include <Wire.h>
#include <math.h>
#include <EEPROM.h>
#include <esp_task_wdt.h>
#include <esp_system.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <HardwareSerial.h>

// ─────────────────────────────────────────────────────────────────
//  GPIO PINOUT  (matches README Section 1 architecture diagram)
// ─────────────────────────────────────────────────────────────────
#define I2C0_SDA      8     // MPU6050
#define I2C0_SCL      9
#define I2C1_SDA      4     // BMP280 + DS1307
#define I2C1_SCL      5

#define GPS_RX        6     // UART1 → GPS TX
#define GPS_TX        7     // UART1 → GPS RX (not used for receive-only GPS)
#define SIM_RX        15    // UART2 → SIM800L TX
#define SIM_TX        16    // UART2 → SIM800L RX
#define SIM_RST       17    // SIM800L reset pin (active LOW)

#define CSV_TX        14    // SW-UART → XIAO C3 (CSV feed)
#define OTA_TX        43    // HW-UART → XIAO C3 (OTA)
#define OTA_RX        44

#define BATT_ADC      2     // Battery voltage divider
#define ALGO_BTN      12    // Algorithm toggle button
#define ALGO_LED      13    // Algorithm status LED
#define STATUS_LED    3     // General status LED

// ─────────────────────────────────────────────────────────────────
//  I2C ADDRESSES
// ─────────────────────────────────────────────────────────────────
#define MPU6050_ADDR  0x68
#define BMP280_ADDR   0x76
#define DS1307_ADDR   0x68   // on I2C Bus 1

// ─────────────────────────────────────────────────────────────────
//  MPU6050 REGISTER MAP
// ─────────────────────────────────────────────────────────────────
#define MPU_PWR_MGMT1   0x6B
#define MPU_SMPLRT_DIV  0x19
#define MPU_CONFIG      0x1A
#define MPU_GYRO_CFG    0x1B
#define MPU_ACCEL_CFG   0x1C
#define MPU_ACCEL_XOUTH 0x3B
#define MPU_GYRO_XOUTH  0x43
#define MPU_WHO_AM_I    0x75
#define MPU_WHOAMI_VAL1 0x68
#define MPU_WHOAMI_VAL2 0x72

// ─────────────────────────────────────────────────────────────────
//  BMP280 REGISTER MAP
// ─────────────────────────────────────────────────────────────────
#define BMP_CHIP_ID     0xD0
#define BMP_RESET       0xE0
#define BMP_CTRL_MEAS   0xF4
#define BMP_CONFIG_REG  0xF5
#define BMP_PRESS_MSB   0xF7
#define BMP_CALIB_START 0x88

// ─────────────────────────────────────────────────────────────────
//  SYSTEM CONSTANTS
// ─────────────────────────────────────────────────────────────────
#define ALPHA                 0.98f
#define GYRO_SENSITIVITY      131.0f
#define ACCEL_SENSITIVITY     16384.0f

#define TETHER_TAUT_THRESH    0.15f
#define TETHER_SLACK_THRESH   0.10f
#define TILT_TAUT_MIN_DEG     3.0f
#define FLOOD_NEAR_ANGLE      10.0f
#define FLOOD_NEAR_FRACTION   0.95f
#define SUBMERSION_THRESH_PA  500.0f

#define WATER_DENSITY         1000.0f
#define GRAVITY               9.81f

#define WDT_TIMEOUT_INIT_S    180
#define WDT_TIMEOUT_OP_S      120

#define EEPROM_SIZE           512
#define EEPROM_MAGIC_BYTE     0xA5
#define EEPROM_WARM_MAX_S     1800

// ─────────────────────────────────────────────────────────────────
//  FLOOD DETECTION THRESHOLDS  (cm)
// ─────────────────────────────────────────────────────────────────
#define DEFAULT_ALERT_CM      120
#define DEFAULT_WARN_CM       180
#define DEFAULT_DANGER_CM     250

// ─────────────────────────────────────────────────────────────────
//  RATE OF CHANGE THRESHOLDS  (cm / 15 min)
// ─────────────────────────────────────────────────────────────────
#define RATE_SLOW_MAX         2.0f
#define RATE_MOD_MAX          5.0f
#define RATE_EXTREME          30.0f
#define RATE_CATASTROPHIC     50.0f
#define RATE_CLAMP            200.0f

// ─────────────────────────────────────────────────────────────────
//  STEP-DOWN HYSTERESIS CONSTANTS
// ─────────────────────────────────────────────────────────────────
#define STEPDOWN_READINGS     4
#define STEPDOWN_TO_NORMAL    8
#define MIN_TIME_CRITICAL_S   900
#define MIN_TIME_FLOOD_S      1800
#define MIN_TIME_WARNING_S    1800
#define MIN_TIME_WATCH_S      900

// ─────────────────────────────────────────────────────────────────
//  BATTERY THRESHOLDS
// ─────────────────────────────────────────────────────────────────
#define BATT_LOW_PCT          20
#define BATT_CRITICAL_PCT     10
#define BATT_R1_OHM           33000.0f
#define BATT_R2_OHM           100000.0f

// ─────────────────────────────────────────────────────────────────
//  COMMUNICATION CONSTANTS
// ─────────────────────────────────────────────────────────────────
#define SIM_BAUD              9600
#define GPS_BAUD              9600
#define SIM_AT_TIMEOUT_MS     5000
#define SIM_HTTP_TIMEOUT_MS   30000
#define SIM_GPRS_TIMEOUT_MS   60000
#define SIM_MAX_RETRIES       3
#define WIFI_CONNECT_TIMEOUT  15000
#define FIREBASE_TIMEOUT_MS   15000

// Payload buffer size — JSON payload is ~700 bytes, allow headroom
#define PAYLOAD_BUF_SIZE      1024

// Store-and-forward: keep up to 5 failed payloads for retry
#define STORE_FORWARD_MAX     5

// ─────────────────────────────────────────────────────────────────
//  NETWORK CONFIGURATION
//  *** CHANGE THESE TO YOUR ACTUAL VALUES BEFORE DEPLOYMENT ***
// ─────────────────────────────────────────────────────────────────
static const char* DEVICE_ID       = "VARUNA-GK-01";
static const char* STATION_NAME    = "Godavari-KG-01";

// SIM800L APN — change for your carrier
static const char* GPRS_APN        = "airtelgprs.com";
static const char* GPRS_USER       = "";
static const char* GPRS_PASS       = "";

// HTTP relay server (SIM800L posts here — HTTP, not HTTPS)
static const char* RELAY_SERVER_URL = "http://your-relay-server.com/api/varuna/data";

// WiFi credentials (fallback path)
static const char* WIFI_SSID       = "TPLink_2G";
static const char* WIFI_PASS       = "9700668325";

// Firebase configuration
static const char* FIREBASE_HOST   = "varuna-ok-default-rtdb.asia-southeast1.firebasedatabase.app";
static const char* FIREBASE_PATH   = "/varuna/live/";
static const char* FIREBASE_SECRET = "HcV2Fs3OAi2ApwddQ0DQGvtdktYvf99vnUb7S1l3";

// ─────────────────────────────────────────────────────────────────
//  ADAPTIVE SAMPLING  (seconds)
// ─────────────────────────────────────────────────────────────────
static const uint32_t SAMPLE_INTERVALS[5][3] = {
  { 1800, 3600,  7200 },
  {  900, 3600,  7200 },
  {  300,  300,   300 },
  {  300,  300,   300 },
  {  120,  300,   300 },
};
static const uint32_t TRANSMIT_INTERVALS[5][3] = {
  { 3600, 7200, 14400 },
  { 1800, 7200, 14400 },
  {  300,  300,   300 },
  {  300,  300,   300 },
  {  120,  300,   300 },
};

// ─────────────────────────────────────────────────────────────────
//  ENUMERATIONS
// ─────────────────────────────────────────────────────────────────
enum OperatingMode  { MODE_0_SLACK, MODE_1_TAUT, MODE_2_FLOOD, MODE_3_SUBMERGED };
enum HeightZone     { ZONE_NORMAL, ZONE_ALERT, ZONE_WARNING, ZONE_DANGER };
enum RateCategory   { RATE_FALLING_E, RATE_SLOW_E, RATE_MOD_E, RATE_FAST_E };
enum ResponseLevel  { LEVEL_NORMAL, LEVEL_WATCH, LEVEL_WARNING, LEVEL_FLOOD, LEVEL_CRITICAL };
enum CommChannel    { COMM_NONE, COMM_SIM_GPRS, COMM_WIFI_FIREBASE };

// ─────────────────────────────────────────────────────────────────
//  DECISION MATRIX  [zone][rate_index][sustained?]
// ─────────────────────────────────────────────────────────────────
static const int DECISION_MATRIX[4][3][2] = {
  { {LEVEL_NORMAL, LEVEL_NORMAL}, {LEVEL_NORMAL, LEVEL_WATCH},  {LEVEL_NORMAL, LEVEL_WATCH}  },
  { {LEVEL_WATCH,  LEVEL_WATCH},  {LEVEL_WATCH,  LEVEL_WARNING},{LEVEL_WATCH,  LEVEL_WARNING} },
  { {LEVEL_WARNING,LEVEL_FLOOD},  {LEVEL_WARNING,LEVEL_FLOOD},  {LEVEL_FLOOD,  LEVEL_CRITICAL}},
  { {LEVEL_FLOOD,  LEVEL_CRITICAL},{LEVEL_CRITICAL,LEVEL_CRITICAL},{LEVEL_CRITICAL,LEVEL_CRITICAL}},
};

// ─────────────────────────────────────────────────────────────────
//  BMP280 CALIBRATION COEFFICIENTS
// ─────────────────────────────────────────────────────────────────
struct BMP280Cal {
  uint16_t dig_T1; int16_t dig_T2; int16_t dig_T3;
  uint16_t dig_P1; int16_t dig_P2; int16_t dig_P3;
  int16_t  dig_P4; int16_t dig_P5; int16_t dig_P6;
  int16_t  dig_P7; int16_t dig_P8; int16_t dig_P9;
  int32_t  t_fine;
};

// ─────────────────────────────────────────────────────────────────
//  GPS DATA STRUCTURE
// ─────────────────────────────────────────────────────────────────
struct GPSData {
  float    latitude;
  float    longitude;
  float    altitude;
  float    speed;       // km/h
  uint8_t  satellites;
  bool     valid;
  uint8_t  hour;
  uint8_t  minute;
  uint8_t  second;
  uint8_t  day;
  uint8_t  month;
  uint16_t year;
  uint32_t lastFixTime; // millis() of last valid fix
};

// ─────────────────────────────────────────────────────────────────
//  STORE-AND-FORWARD STRUCTURE
// ─────────────────────────────────────────────────────────────────
struct StoredPayload {
  char     data[PAYLOAD_BUF_SIZE];
  uint32_t timestamp;
  bool     occupied;
};

// ─────────────────────────────────────────────────────────────────
//  GLOBAL STATE — SENSORS
// ─────────────────────────────────────────────────────────────────
TwoWire I2C0 = TwoWire(0);
TwoWire I2C1 = TwoWire(1);

// UART for GPS and SIM800L
HardwareSerial gpsSerial(1);   // UART1
HardwareSerial simSerial(2);   // UART2

// MPU6050 raw + fused
int16_t rawAx, rawAy, rawAz;
int16_t rawGx, rawGy, rawGz;
float   ax_ms2, ay_ms2, az_ms2;
float   gx_dps, gy_dps, gz_dps;
float   gyroOffsetX = 0, gyroOffsetY = 0, gyroOffsetZ = 0;
float   refAccX = 0, refAccY = 0, refAccZ = 1.0f;
float   refTiltX = 0, refTiltY = 0;
float   filtTiltX = 0, filtTiltY = 0;
float   lateralAccel = 0;
float   tiltAngleDeg = 0;
bool    mpuHealthy  = false;

// BMP280
BMP280Cal bmpCal;
float   currentPressurePa   = 101325.0f;
float   baselinePressurePa  = 101325.0f;
float   currentTempC        = 25.0f;
bool    bmpAvailable        = false;

// Variance buffers
#define ACCEL_VAR_BUF   50
#define PRESS_VAR_BUF   20
float   accelVarBuf[ACCEL_VAR_BUF];
float   pressVarBuf[PRESS_VAR_BUF];
int     accelVarIdx = 0;
int     pressVarIdx = 0;
int     accelVarCount = 0;
int     pressVarCount = 0;
int     mpuFrozenCount    = 0;
int     gravityErrCount   = 0;

// GPS
GPSData gpsData;
char    gpsNmeaBuf[128];    // buffer for one NMEA sentence
int     gpsNmeaIdx = 0;

// ─────────────────────────────────────────────────────────────────
//  GLOBAL STATE — MEASUREMENT ENGINE  (Phase 2)
// ─────────────────────────────────────────────────────────────────
float         olpLengthCm  = 200.0f;
OperatingMode currentMode  = MODE_0_SLACK;
float         waterHeightCm = 0;
float         depthBelowSurfaceCm = 0;

// ─────────────────────────────────────────────────────────────────
//  GLOBAL STATE — FLOOD STATE MACHINE  (Phase 3)
// ─────────────────────────────────────────────────────────────────
int     alertLevelCm  = DEFAULT_ALERT_CM;
int     warnLevelCm   = DEFAULT_WARN_CM;
int     dangerLevelCm = DEFAULT_DANGER_CM;

ResponseLevel currentResponseLevel = LEVEL_NORMAL;
HeightZone    currentZone          = ZONE_NORMAL;

float   prevHeightCm    = 0;
uint32_t prevHeightTime = 0;
float   ratePer15Min    = 0;

#define SUSTAINED_BUF_SIZE 4
float    sustainedBuf[SUSTAINED_BUF_SIZE];
uint32_t sustainedTimeBuf[SUSTAINED_BUF_SIZE];
int      sustainedBufHead = 0;
int      sustainedBufCount = 0;
bool     sustainedRise = false;

int      stepDownConsecutive = 0;
uint32_t stateEntryTime = 0;

uint32_t readingsSinceBoot = 0;

float    peakHeightCm   = 0;
uint32_t peakHeightTime = 0;

// ─────────────────────────────────────────────────────────────────
//  GLOBAL STATE — COMMUNICATION
// ─────────────────────────────────────────────────────────────────
bool     simAvailable     = false;
bool     simGprsConnected = false;
bool     wifiConnected    = false;
CommChannel lastSuccessChannel = COMM_NONE;
uint32_t lastTransmitAttempt = 0;
uint32_t transmitSuccessCount = 0;
uint32_t transmitFailCount    = 0;

// Store-and-forward buffer
StoredPayload storedPayloads[STORE_FORWARD_MAX];
int      storedPayloadCount = 0;

// SIM800L response buffer
char     simResponseBuf[256];

// Payload construction buffer
char     payloadBuf[PAYLOAD_BUF_SIZE];

// ─────────────────────────────────────────────────────────────────
//  GLOBAL STATE — SYSTEM
// ─────────────────────────────────────────────────────────────────
bool     algorithmEnabled = false;
uint32_t lastSampleTime   = 0;
uint32_t lastTransmitTime = 0;
uint32_t sampleInterval   = 1800000UL;
uint32_t transmitInterval = 3600000UL;
uint8_t  batteryPercent   = 100;
int      batteryState     = 0;
int      healthScore      = 100;
uint32_t bootUnixTime     = 0;

uint32_t lastMpuRead  = 0;
uint32_t lastBmpRead  = 0;
uint32_t lastBattRead = 0;
uint32_t lastEepromSave = 0;
uint32_t lastGpsRead  = 0;

uint32_t lastBtnPress = 0;
bool     lastBtnState = HIGH;

// ─────────────────────────────────────────────────────────────────
//  EEPROM LAYOUT
// ─────────────────────────────────────────────────────────────────
#define EE_MAGIC        0
#define EE_SAVE_TIME    1
#define EE_RESP_LEVEL   5
#define EE_SUSTAINED    6
#define EE_OLP_CM       7
#define EE_ALERT_CM     11
#define EE_WARN_CM      13
#define EE_DANGER_CM    15
#define EE_PEAK_CM      17
#define EE_PEAK_TIME    21
#define EE_PREV_H       25
#define EE_BASELINE_P   29

// ─────────────────────────────────────────────────────────────────
//  FORWARD DECLARATIONS
//  Required because several functions reference each other.
// ─────────────────────────────────────────────────────────────────
void forceSaveEeprom();
void updateAdaptiveIntervals();
void flushSustainedBufferIfIntervalChanged(uint32_t newInterval, uint32_t oldInterval);
void printCSV();

// ─────────────────────────────────────────────────────────────────
//  UTILITY: EEPROM helpers
// ─────────────────────────────────────────────────────────────────
void eepromWriteFloat(int addr, float val) {
  uint8_t buf[4];
  memcpy(buf, &val, 4);
  for (int i = 0; i < 4; i++) EEPROM.write(addr + i, buf[i]);
}
float eepromReadFloat(int addr) {
  uint8_t buf[4];
  for (int i = 0; i < 4; i++) buf[i] = EEPROM.read(addr + i);
  float val;
  memcpy(&val, buf, 4);
  return val;
}
void eepromWriteUint32(int addr, uint32_t val) {
  EEPROM.write(addr,   (val >> 24) & 0xFF);
  EEPROM.write(addr+1, (val >> 16) & 0xFF);
  EEPROM.write(addr+2, (val >>  8) & 0xFF);
  EEPROM.write(addr+3,  val        & 0xFF);
}
uint32_t eepromReadUint32(int addr) {
  return ((uint32_t)EEPROM.read(addr)   << 24) |
         ((uint32_t)EEPROM.read(addr+1) << 16) |
         ((uint32_t)EEPROM.read(addr+2) <<  8) |
          (uint32_t)EEPROM.read(addr+3);
}
void eepromWriteInt16(int addr, int16_t val) {
  EEPROM.write(addr,   (val >> 8) & 0xFF);
  EEPROM.write(addr+1,  val       & 0xFF);
}
int16_t eepromReadInt16(int addr) {
  return (int16_t)(((uint16_t)EEPROM.read(addr) << 8) | EEPROM.read(addr+1));
}

// ─────────────────────────────────────────────────────────────────
//  UTILITY: variance
// ─────────────────────────────────────────────────────────────────
float computeVariance(float* buf, int n) {
  if (n <= 1) return 0;
  float mean = 0;
  for (int i = 0; i < n; i++) mean += buf[i];
  mean /= n;
  float var = 0;
  for (int i = 0; i < n; i++) var += (buf[i] - mean) * (buf[i] - mean);
  return var / n;
}

// ─────────────────────────────────────────────────────────────────
//  PHASE 1A — MPU6050 DRIVER
// ─────────────────────────────────────────────────────────────────

void mpuWriteReg(uint8_t reg, uint8_t val) {
  I2C0.beginTransmission(MPU6050_ADDR);
  I2C0.write(reg);
  I2C0.write(val);
  I2C0.endTransmission();
}

uint8_t mpuReadReg(uint8_t reg) {
  I2C0.beginTransmission(MPU6050_ADDR);
  I2C0.write(reg);
  I2C0.endTransmission(false);
  I2C0.requestFrom(MPU6050_ADDR, (uint8_t)1);
  return I2C0.available() ? I2C0.read() : 0xFF;
}

void mpuReadBurst(uint8_t reg, uint8_t* buf, uint8_t len) {
  I2C0.beginTransmission(MPU6050_ADDR);
  I2C0.write(reg);
  I2C0.endTransmission(false);
  I2C0.requestFrom(MPU6050_ADDR, len);
  for (int i = 0; i < len && I2C0.available(); i++) buf[i] = I2C0.read();
}

bool mpuInit() {
  mpuWriteReg(MPU_PWR_MGMT1,  0x00);
  delay(100);
  mpuWriteReg(MPU_SMPLRT_DIV, 0x07);
  mpuWriteReg(MPU_CONFIG,     0x03);
  mpuWriteReg(MPU_GYRO_CFG,   0x00);
  mpuWriteReg(MPU_ACCEL_CFG,  0x00);
  delay(50);

  uint8_t who = mpuReadReg(MPU_WHO_AM_I);
  if (who != MPU_WHOAMI_VAL1 && who != MPU_WHOAMI_VAL2) {
    Serial.printf("[MPU] WHO_AM_I = 0x%02X — NOT recognised. Sensor unavailable.\n", who);
    return false;
  }
  Serial.printf("[MPU] WHO_AM_I = 0x%02X — OK\n", who);
  return true;
}

void i2cBusRecovery() {
  Serial.println("[MPU] Attempting I2C bus recovery (9 SCL clocks)...");
  pinMode(I2C0_SCL, OUTPUT);
  for (int i = 0; i < 9; i++) {
    digitalWrite(I2C0_SCL, HIGH); delayMicroseconds(5);
    digitalWrite(I2C0_SCL, LOW);  delayMicroseconds(5);
  }
  I2C0.begin(I2C0_SDA, I2C0_SCL, 100000);
  delay(50);
}

void mpuRead() {
  uint8_t buf[14];
  mpuReadBurst(MPU_ACCEL_XOUTH, buf, 14);

  rawAx = (int16_t)((buf[0]  << 8) | buf[1]);
  rawAy = (int16_t)((buf[2]  << 8) | buf[3]);
  rawAz = (int16_t)((buf[4]  << 8) | buf[5]);
  rawGx = (int16_t)((buf[8]  << 8) | buf[9]);
  rawGy = (int16_t)((buf[10] << 8) | buf[11]);
  rawGz = (int16_t)((buf[12] << 8) | buf[13]);

  ax_ms2 = (rawAx / ACCEL_SENSITIVITY) * GRAVITY;
  ay_ms2 = (rawAy / ACCEL_SENSITIVITY) * GRAVITY;
  az_ms2 = (rawAz / ACCEL_SENSITIVITY) * GRAVITY;

  gx_dps = (rawGx / GYRO_SENSITIVITY) - gyroOffsetX;
  gy_dps = (rawGy / GYRO_SENSITIVITY) - gyroOffsetY;
  gz_dps = (rawGz / GYRO_SENSITIVITY) - gyroOffsetZ;
}

void mpuCalibrate() {
  Serial.println("[MPU] Starting calibration — keep buoy STILL for 4 seconds...");
  for (int i = 0; i < 6; i++) {
    digitalWrite(STATUS_LED, !digitalRead(STATUS_LED));
    delay(500);
  }
  digitalWrite(STATUS_LED, LOW);

  double sumGx = 0, sumGy = 0, sumGz = 0;
  for (int i = 0; i < 1000; i++) {
    mpuRead();
    sumGx += (rawGx / GYRO_SENSITIVITY);
    sumGy += (rawGy / GYRO_SENSITIVITY);
    sumGz += (rawGz / GYRO_SENSITIVITY);
    delay(2);
  }
  gyroOffsetX = sumGx / 1000.0;
  gyroOffsetY = sumGy / 1000.0;
  gyroOffsetZ = sumGz / 1000.0;
  Serial.printf("[MPU] Gyro offsets: %.4f  %.4f  %.4f °/s\n",
                gyroOffsetX, gyroOffsetY, gyroOffsetZ);

  double sumAx = 0, sumAy = 0, sumAz = 0;
  int    valid = 0;
  for (int i = 0; i < 500; i++) {
    mpuRead();
    float totalG = sqrt(ax_ms2*ax_ms2 + ay_ms2*ay_ms2 + az_ms2*az_ms2) / GRAVITY;
    if (totalG >= 0.9f && totalG <= 1.1f) {
      sumAx += ax_ms2; sumAy += ay_ms2; sumAz += az_ms2;
      valid++;
    }
    delay(3);
  }
  if (valid > 0) {
    refAccX = sumAx / valid;
    refAccY = sumAy / valid;
    refAccZ = sumAz / valid;
  }
  refTiltX = atan2f(refAccY, sqrtf(refAccX*refAccX + refAccZ*refAccZ)) * 180.0f / M_PI;
  refTiltY = atan2f(-refAccX, sqrtf(refAccY*refAccY + refAccZ*refAccZ)) * 180.0f / M_PI;

  filtTiltX = 0;
  filtTiltY = 0;

  Serial.printf("[MPU] Accel ref: %.4f %.4f %.4f g  (valid=%d/500)\n",
                refAccX/GRAVITY, refAccY/GRAVITY, refAccZ/GRAVITY, valid);
  Serial.printf("[MPU] Ref tilt: X=%.2f°  Y=%.2f°\n", refTiltX, refTiltY);
  Serial.println("[MPU] Calibration complete.");
}

bool mpuCheckFrozen() {
  float var = computeVariance(accelVarBuf, min(accelVarCount, ACCEL_VAR_BUF));
  if (accelVarCount < ACCEL_VAR_BUF) return false;

  if (var < 0.0001f) {
    mpuFrozenCount++;
    Serial.printf("[FAULT] MPU6050 variance=%.6f — possible latch-up (count=%d)\n",
                  var, mpuFrozenCount);
    if (mpuFrozenCount >= 3) {
      i2cBusRecovery();
      mpuWriteReg(MPU_PWR_MGMT1, 0x80);
      delay(100);
      mpuWriteReg(MPU_PWR_MGMT1, 0x00);
      delay(50);
      uint8_t who = mpuReadReg(MPU_WHO_AM_I);
      if (who == MPU_WHOAMI_VAL1 || who == MPU_WHOAMI_VAL2) {
        Serial.println("[MPU] Recovery OK — re-calibrating.");
        mpuCalibrate();
        mpuFrozenCount = 0;
        return false;
      } else {
        Serial.println("[FAULT] MPU6050 HARDWARE_FAULT — unrecoverable.");
        mpuHealthy = false;
        return true;
      }
    }
  } else {
    mpuFrozenCount = 0;
  }
  return false;
}

void mpuAutoCalibrateModeZero() {
  float errX = ax_ms2;
  float errY = ay_ms2;
  float errZ = az_ms2 - GRAVITY;
  if (fabsf(errZ) > 0.5f || fabsf(errX) > 0.5f || fabsf(errY) > 0.5f) {
    gyroOffsetX -= errX / GRAVITY * 0.01f;
    gyroOffsetY -= errY / GRAVITY * 0.01f;
    Serial.printf("[AUTOCAL] Offset drift corrected: errX=%.3f errY=%.3f errZ=%.3f\n",
                  errX, errY, errZ);
  }
}

// ─────────────────────────────────────────────────────────────────
//  PHASE 1B — BMP280 DRIVER
// ─────────────────────────────────────────────────────────────────

void bmpWriteReg(uint8_t reg, uint8_t val) {
  I2C1.beginTransmission(BMP280_ADDR);
  I2C1.write(reg);
  I2C1.write(val);
  I2C1.endTransmission();
}

uint8_t bmpReadReg(uint8_t reg) {
  I2C1.beginTransmission(BMP280_ADDR);
  I2C1.write(reg);
  I2C1.endTransmission(false);
  I2C1.requestFrom(BMP280_ADDR, (uint8_t)1);
  return I2C1.available() ? I2C1.read() : 0xFF;
}

void bmpReadBurst(uint8_t reg, uint8_t* buf, uint8_t len) {
  I2C1.beginTransmission(BMP280_ADDR);
  I2C1.write(reg);
  I2C1.endTransmission(false);
  I2C1.requestFrom(BMP280_ADDR, len);
  for (int i = 0; i < len && I2C1.available(); i++) buf[i] = I2C1.read();
}

void bmpReadCalibration() {
  uint8_t buf[24];
  bmpReadBurst(BMP_CALIB_START, buf, 24);
  bmpCal.dig_T1 = (uint16_t)((buf[1]  << 8) | buf[0]);
  bmpCal.dig_T2 = (int16_t) ((buf[3]  << 8) | buf[2]);
  bmpCal.dig_T3 = (int16_t) ((buf[5]  << 8) | buf[4]);
  bmpCal.dig_P1 = (uint16_t)((buf[7]  << 8) | buf[6]);
  bmpCal.dig_P2 = (int16_t) ((buf[9]  << 8) | buf[8]);
  bmpCal.dig_P3 = (int16_t) ((buf[11] << 8) | buf[10]);
  bmpCal.dig_P4 = (int16_t) ((buf[13] << 8) | buf[12]);
  bmpCal.dig_P5 = (int16_t) ((buf[15] << 8) | buf[14]);
  bmpCal.dig_P6 = (int16_t) ((buf[17] << 8) | buf[16]);
  bmpCal.dig_P7 = (int16_t) ((buf[19] << 8) | buf[18]);
  bmpCal.dig_P8 = (int16_t) ((buf[21] << 8) | buf[20]);
  bmpCal.dig_P9 = (int16_t) ((buf[23] << 8) | buf[22]);
}

bool bmpInit() {
  uint8_t id = bmpReadReg(BMP_CHIP_ID);
  if (id != 0x58 && id != 0x56 && id != 0x57 && id != 0x60) {
    Serial.printf("[BMP] Chip ID = 0x%02X — not recognised.\n", id);
    return false;
  }
  Serial.printf("[BMP] Chip ID = 0x%02X — OK\n", id);

  bmpWriteReg(BMP_RESET, 0xB6);
  delay(10);
  bmpReadCalibration();

  bmpWriteReg(BMP_CTRL_MEAS, 0b10110111);
  bmpWriteReg(BMP_CONFIG_REG, 0b10110000);
  delay(100);
  return true;
}

void bmpRead() {
  uint8_t buf[6];
  bmpReadBurst(BMP_PRESS_MSB, buf, 6);

  int32_t adc_P = ((int32_t)buf[0] << 12) | ((int32_t)buf[1] << 4) | (buf[2] >> 4);
  int32_t adc_T = ((int32_t)buf[3] << 12) | ((int32_t)buf[4] << 4) | (buf[5] >> 4);

  int32_t var1 = ((((adc_T >> 3) - ((int32_t)bmpCal.dig_T1 << 1))) *
                  (int32_t)bmpCal.dig_T2) >> 11;
  int32_t var2 = (((((adc_T >> 4) - (int32_t)bmpCal.dig_T1) *
                    ((adc_T >> 4) - (int32_t)bmpCal.dig_T1)) >> 12) *
                  (int32_t)bmpCal.dig_T3) >> 14;
  bmpCal.t_fine = var1 + var2;
  currentTempC  = (float)((bmpCal.t_fine * 5 + 128) >> 8) / 100.0f;

  int64_t p1 = (int64_t)bmpCal.t_fine - 128000;
  int64_t p2 = p1 * p1 * (int64_t)bmpCal.dig_P6;
  p2 = p2 + ((p1 * (int64_t)bmpCal.dig_P5) << 17);
  p2 = p2 + (((int64_t)bmpCal.dig_P4) << 35);
  p1 = ((p1 * p1 * (int64_t)bmpCal.dig_P3) >> 8) +
       ((p1 * (int64_t)bmpCal.dig_P2) << 12);
  p1 = (((((int64_t)1) << 47) + p1)) * ((int64_t)bmpCal.dig_P1) >> 33;

  if (p1 == 0) return;

  int64_t p = 1048576 - adc_P;
  p = (((p << 31) - p2) * 3125) / p1;
  p1 = ((int64_t)bmpCal.dig_P9 * (p >> 13) * (p >> 13)) >> 25;
  p2 = ((int64_t)bmpCal.dig_P8 * p) >> 19;
  p  = ((p + p1 + p2) >> 8) + ((int64_t)bmpCal.dig_P7 << 4);

  currentPressurePa = (float)p / 256.0f;

  if (currentPressurePa < 30000.0f || currentPressurePa > 120000.0f) {
    Serial.printf("[FAULT] BMP280 pressure out of range: %.1f Pa\n", currentPressurePa);
    bmpAvailable = false;
  }

  float gaugePa = currentPressurePa - baselinePressurePa;
  pressVarBuf[pressVarIdx % PRESS_VAR_BUF] = gaugePa;
  pressVarIdx++;
  if (pressVarCount < PRESS_VAR_BUF) pressVarCount++;
}

void bmpEstablishBaseline(int samples) {
  Serial.println("[BMP] Establishing atmospheric baseline...");
  double sum = 0;
  for (int i = 0; i < samples; i++) {
    bmpRead();
    sum += currentPressurePa;
    delay(50);
  }
  baselinePressurePa = sum / samples;
  Serial.printf("[BMP] Baseline = %.2f Pa  (%.4f hPa)\n",
                baselinePressurePa, baselinePressurePa / 100.0f);
}

bool bmpCheckFrozen() {
  if (pressVarCount < PRESS_VAR_BUF) return false;
  float var = computeVariance(pressVarBuf, PRESS_VAR_BUF);
  if (var < 0.01f) {
    Serial.println("[FAULT] BMP280 pressure frozen — attempting re-init.");
    bmpAvailable = bmpInit();
    if (!bmpAvailable) Serial.println("[FAULT] BMP280 unrecoverable.");
    return !bmpAvailable;
  }
  return false;
}

// ─────────────────────────────────────────────────────────────────
//  PHASE 1C — BATTERY ADC DRIVER
// ─────────────────────────────────────────────────────────────────

float readBatteryVoltage() {
  uint32_t raw = 0;
  for (int i = 0; i < 8; i++) raw += analogRead(BATT_ADC);
  float adcV  = (raw / 8.0f) * 3.3f / 4095.0f;
  float ratio = (BATT_R1_OHM + BATT_R2_OHM) / BATT_R2_OHM;
  return adcV * ratio;
}

uint8_t voltageToPct(float v) {
  if (v >= 4.20f) return 100;
  if (v >= 4.10f) return (uint8_t)(90 + (v - 4.10f) / 0.10f * 10.0f);
  if (v >= 3.85f) return (uint8_t)(70 + (v - 3.85f) / 0.25f * 20.0f);
  if (v >= 3.70f) return (uint8_t)(40 + (v - 3.70f) / 0.15f * 30.0f);
  if (v >= 3.50f) return (uint8_t)(20 + (v - 3.50f) / 0.20f * 20.0f);
  if (v >= 3.30f) return (uint8_t)( 5 + (v - 3.30f) / 0.20f * 15.0f);
  if (v >= 3.00f) return (uint8_t)( 0 + (v - 3.00f) / 0.30f *  5.0f);
  return 0;
}

void updateBattery() {
  float v = readBatteryVoltage();
  batteryPercent = voltageToPct(v);
  if      (batteryPercent < BATT_CRITICAL_PCT) batteryState = 2;
  else if (batteryPercent < BATT_LOW_PCT)      batteryState = 1;
  else                                         batteryState = 0;
}

// ─────────────────────────────────────────────────────────────────
//  PHASE 1D — GPS NMEA PARSER (UART1)
// ─────────────────────────────────────────────────────────────────

/*
 * Initialise GPS UART.
 * Most GPS modules (NEO-6M, NEO-7M, NEO-M8N, ATGM336H) output NMEA at 9600 baud
 * by default. We parse $GPRMC and $GPGGA sentences.
 */
void gpsInit() {
  gpsSerial.begin(GPS_BAUD, SERIAL_8N1, GPS_RX, GPS_TX);
  memset(&gpsData, 0, sizeof(GPSData));
  gpsData.valid = false;
  gpsNmeaIdx = 0;
  Serial.println("[GPS] UART1 initialised at 9600 baud.");
}

/*
 * Convert NMEA coordinate format (DDMM.MMMMM) to decimal degrees.
 * hemisphere: 'N','S','E','W'
 */
float nmeaToDecimalDegrees(const char* nmeaCoord, char hemisphere) {
  if (nmeaCoord == NULL || nmeaCoord[0] == '\0') return 0.0f;

  // Find the decimal point to determine how many degree digits there are
  const char* dot = strchr(nmeaCoord, '.');
  if (dot == NULL) return 0.0f;

  int degDigits = (int)(dot - nmeaCoord) - 2;  // degrees are everything before MM.MMMMM
  if (degDigits < 1 || degDigits > 3) return 0.0f;

  char degStr[4] = {0};
  strncpy(degStr, nmeaCoord, degDigits);
  float degrees = (float)atoi(degStr);

  float minutes = atof(nmeaCoord + degDigits);
  float result = degrees + minutes / 60.0f;

  if (hemisphere == 'S' || hemisphere == 'W') result = -result;

  return result;
}

/*
 * Parse a single NMEA field from a comma-separated string.
 * Returns pointer to the start of field N (0-indexed).
 * Modifies the buffer by replacing commas with nulls.
 */
char* nmeaGetField(char* sentence, int fieldIndex) {
  int currentField = 0;
  char* ptr = sentence;

  while (currentField < fieldIndex && *ptr != '\0') {
    if (*ptr == ',') currentField++;
    ptr++;
  }

  if (currentField != fieldIndex) return NULL;
  return ptr;
}

/*
 * Count fields in NMEA sentence (number of commas + 1).
 */
int nmeaCountFields(const char* sentence) {
  int count = 1;
  const char* p = sentence;
  while (*p != '\0' && *p != '*') {
    if (*p == ',') count++;
    p++;
  }
  return count;
}

/*
 * Parse $GPRMC sentence (Recommended Minimum sentence C).
 * Format: $GPRMC,hhmmss.ss,A,llll.lll,N,yyyyy.yyy,E,speed,course,ddmmyy,magvar,vardir*cs
 *
 * Fields:
 *   0: $GPRMC
 *   1: UTC time (hhmmss.ss)
 *   2: Status (A=valid, V=void)
 *   3: Latitude (ddmm.mmmm)
 *   4: N/S
 *   5: Longitude (dddmm.mmmm)
 *   6: E/W
 *   7: Speed over ground (knots)
 *   8: Course over ground (degrees)
 *   9: Date (ddmmyy)
 *  10: Magnetic variation
 *  11: Variation direction
 */
void parseGPRMC(char* sentence) {
  // Make a working copy since we'll modify it
  char work[128];
  strncpy(work, sentence, sizeof(work) - 1);
  work[sizeof(work) - 1] = '\0';

  // Split by commas into an array of pointers
  char* fields[15];
  int fieldCount = 0;
  char* token = work;
  fields[fieldCount++] = token;

  while (*token != '\0' && *token != '*' && fieldCount < 15) {
    if (*token == ',') {
      *token = '\0';
      fields[fieldCount++] = token + 1;
    }
    token++;
  }
  // Null-terminate at checksum marker
  char* star = strchr(fields[fieldCount - 1], '*');
  if (star) *star = '\0';

  if (fieldCount < 10) return;  // malformed

  // Field 2: status
  if (fields[2][0] == 'A') {
    gpsData.valid = true;
    gpsData.lastFixTime = millis();
  } else {
    gpsData.valid = false;
    return;  // no point parsing invalid fix
  }

  // Field 1: time (hhmmss.ss)
  if (strlen(fields[1]) >= 6) {
    gpsData.hour   = (fields[1][0] - '0') * 10 + (fields[1][1] - '0');
    gpsData.minute = (fields[1][2] - '0') * 10 + (fields[1][3] - '0');
    gpsData.second = (fields[1][4] - '0') * 10 + (fields[1][5] - '0');
  }

  // Fields 3-4: latitude
  gpsData.latitude = nmeaToDecimalDegrees(fields[3], fields[4][0]);

  // Fields 5-6: longitude
  gpsData.longitude = nmeaToDecimalDegrees(fields[5], fields[6][0]);

  // Field 7: speed (knots → km/h)
  if (strlen(fields[7]) > 0) {
    gpsData.speed = atof(fields[7]) * 1.852f;
  }

  // Field 9: date (ddmmyy)
  if (strlen(fields[9]) >= 6) {
    gpsData.day   = (fields[9][0] - '0') * 10 + (fields[9][1] - '0');
    gpsData.month = (fields[9][2] - '0') * 10 + (fields[9][3] - '0');
    gpsData.year  = 2000 + (fields[9][4] - '0') * 10 + (fields[9][5] - '0');
  }
}

/*
 * Parse $GPGGA sentence (Global Positioning System Fix Data).
 * We extract satellites and altitude from this.
 * Format: $GPGGA,hhmmss.ss,llll.lll,N,yyyyy.yyy,E,quality,sats,hdop,alt,M,geoid,M,...*cs
 *
 * Fields:
 *   7: Number of satellites
 *   9: Altitude above MSL
 */
void parseGPGGA(char* sentence) {
  char work[128];
  strncpy(work, sentence, sizeof(work) - 1);
  work[sizeof(work) - 1] = '\0';

  char* fields[16];
  int fieldCount = 0;
  char* token = work;
  fields[fieldCount++] = token;

  while (*token != '\0' && *token != '*' && fieldCount < 16) {
    if (*token == ',') {
      *token = '\0';
      fields[fieldCount++] = token + 1;
    }
    token++;
  }
  char* star = strchr(fields[fieldCount - 1], '*');
  if (star) *star = '\0';

  if (fieldCount < 10) return;

  // Field 7: satellites in use
  if (strlen(fields[7]) > 0) {
    gpsData.satellites = (uint8_t)atoi(fields[7]);
  }

  // Field 9: altitude
  if (strlen(fields[9]) > 0) {
    gpsData.altitude = atof(fields[9]);
  }
}

/*
 * Process a complete NMEA sentence.
 * Dispatches to the appropriate parser based on sentence type.
 */
void gpsProcessSentence(char* sentence) {
  if (sentence[0] != '$') return;

  // Check sentence type (comparing first 6 characters)
  // Support both $GPxxx (GPS) and $GNxxx (multi-GNSS) prefixes
  bool isRMC = (strncmp(sentence + 3, "RMC", 3) == 0);
  bool isGGA = (strncmp(sentence + 3, "GGA", 3) == 0);

  if (isRMC) {
    parseGPRMC(sentence);
  } else if (isGGA) {
    parseGPGGA(sentence);
  }
}

/*
 * Read all available bytes from GPS UART and assemble NMEA sentences.
 * Call this frequently from loop().
 */
void gpsUpdate() {
  while (gpsSerial.available()) {
    char c = gpsSerial.read();

    if (c == '$') {
      // Start of new sentence — reset buffer
      gpsNmeaIdx = 0;
      gpsNmeaBuf[gpsNmeaIdx++] = c;
    } else if (c == '\n' || c == '\r') {
      // End of sentence
      if (gpsNmeaIdx > 5) {
        gpsNmeaBuf[gpsNmeaIdx] = '\0';
        gpsProcessSentence(gpsNmeaBuf);
      }
      gpsNmeaIdx = 0;
    } else {
      if (gpsNmeaIdx < (int)(sizeof(gpsNmeaBuf) - 1)) {
        gpsNmeaBuf[gpsNmeaIdx++] = c;
      } else {
        // Buffer overflow — discard
        gpsNmeaIdx = 0;
      }
    }
  }
}

/*
 * Derive a Unix timestamp from GPS date/time fields.
 * Simple implementation — does not account for leap seconds.
 * Returns 0 if GPS time is not valid.
 */
uint32_t gpsGetUnixTime() {
  if (!gpsData.valid) return 0;
  if (gpsData.year < 2024 || gpsData.year > 2099) return 0;

  // Days per month (non-leap)
  static const uint16_t daysInMonth[] = {0, 31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};

  uint32_t days = 0;

  // Years since 1970
  for (uint16_t y = 1970; y < gpsData.year; y++) {
    bool leap = (y % 4 == 0 && (y % 100 != 0 || y % 400 == 0));
    days += leap ? 366 : 365;
  }

  // Months in current year
  bool leapYear = (gpsData.year % 4 == 0 &&
                   (gpsData.year % 100 != 0 || gpsData.year % 400 == 0));
  for (uint8_t m = 1; m < gpsData.month && m <= 12; m++) {
    days += daysInMonth[m];
    if (m == 2 && leapYear) days += 1;
  }

  // Days in current month
  days += gpsData.day - 1;

  uint32_t unixTime = days * 86400UL;
  unixTime += (uint32_t)gpsData.hour * 3600UL;
  unixTime += (uint32_t)gpsData.minute * 60UL;
  unixTime += (uint32_t)gpsData.second;

  return unixTime;
}
// ─────────────────────────────────────────────────────────────────
//  PHASE 1E — SIM800L GPRS DRIVER
// ─────────────────────────────────────────────────────────────────

/*
 * Send an AT command to SIM800L and wait for a response.
 * Returns true if expectedReply is found in the response within timeout.
 * The full response is stored in simResponseBuf.
 */
bool simSendAT(const char* cmd, const char* expectedReply, uint32_t timeout_ms) {
  // Flush any pending data
  while (simSerial.available()) simSerial.read();

  // Send command
  simSerial.println(cmd);

  // Wait for response
  uint32_t start = millis();
  int idx = 0;
  memset(simResponseBuf, 0, sizeof(simResponseBuf));

  while ((millis() - start) < timeout_ms) {
    while (simSerial.available()) {
      char c = simSerial.read();
      if (idx < (int)(sizeof(simResponseBuf) - 1)) {
        simResponseBuf[idx++] = c;
        simResponseBuf[idx] = '\0';
      }
      // Check if we've received the expected reply
      if (strstr(simResponseBuf, expectedReply) != NULL) {
        delay(10);  // let any trailing chars arrive
        while (simSerial.available()) {
          if (idx < (int)(sizeof(simResponseBuf) - 1)) {
            simResponseBuf[idx++] = simSerial.read();
            simResponseBuf[idx] = '\0';
          } else {
            simSerial.read();  // discard overflow
          }
        }
        return true;
      }
    }
    delay(10);
  }

  Serial.printf("[SIM] AT cmd '%s' — timeout. Got: '%s'\n", cmd, simResponseBuf);
  return false;
}

/*
 * Send AT command, wait for "OK" with default timeout.
 */
bool simSendATOK(const char* cmd) {
  return simSendAT(cmd, "OK", SIM_AT_TIMEOUT_MS);
}

/*
 * Hardware reset SIM800L via RST pin.
 * Active LOW: pull low for 200ms then release.
 */
void simHardwareReset() {
  Serial.println("[SIM] Hardware reset...");
  digitalWrite(SIM_RST, LOW);
  delay(200);
  digitalWrite(SIM_RST, HIGH);
  delay(3000);  // SIM800L takes ~3s to boot after reset
  Serial.println("[SIM] Reset complete, waiting for module...");
}

/*
 * Initialise SIM800L module.
 * Checks for AT response, SIM card, and network registration.
 * Returns true if module is ready for GPRS operations.
 */
bool simInit() {
  Serial.println("[SIM] Initialising SIM800L...");

  // Configure reset pin
  pinMode(SIM_RST, OUTPUT);
  digitalWrite(SIM_RST, HIGH);

  // Start UART
  simSerial.begin(SIM_BAUD, SERIAL_8N1, SIM_RX, SIM_TX);
  delay(1000);

  // Try basic AT — up to 3 attempts with reset between failures
  bool atOk = false;
  for (int attempt = 0; attempt < 3 && !atOk; attempt++) {
    if (attempt > 0) {
      simHardwareReset();
    }
    // Send AT a few times to auto-baud
    for (int i = 0; i < 3; i++) {
      if (simSendAT("AT", "OK", 2000)) {
        atOk = true;
        break;
      }
      delay(500);
    }
  }

  if (!atOk) {
    Serial.println("[SIM] Module not responding to AT commands.");
    return false;
  }
  Serial.println("[SIM] AT response OK.");

  // Disable echo
  simSendATOK("ATE0");

  // Check SIM card
  if (!simSendAT("AT+CPIN?", "READY", SIM_AT_TIMEOUT_MS)) {
    Serial.println("[SIM] SIM card not ready. Check insertion.");
    return false;
  }
  Serial.println("[SIM] SIM card ready.");

  // Wait for network registration (up to 60s)
  bool registered = false;
  uint32_t regStart = millis();
  while ((millis() - regStart) < SIM_GPRS_TIMEOUT_MS) {
    simSendAT("AT+CREG?", "OK", 3000);
    // Check for ,1 (home) or ,5 (roaming)
    if (strstr(simResponseBuf, ",1") != NULL || strstr(simResponseBuf, ",5") != NULL) {
      registered = true;
      break;
    }
    Serial.println("[SIM] Waiting for network registration...");
    delay(3000);
    esp_task_wdt_reset();
  }

  if (!registered) {
    Serial.println("[SIM] Network registration failed.");
    return false;
  }
  Serial.println("[SIM] Registered on network.");

  // Check signal quality
  if (simSendAT("AT+CSQ", "OK", 3000)) {
    Serial.printf("[SIM] Signal: %s\n", simResponseBuf);
  }

  return true;
}

/*
 * Establish GPRS connection.
 * Returns true if IP address obtained successfully.
 */
bool simConnectGPRS() {
  Serial.println("[SIM] Connecting GPRS...");

  // Close any existing connections
  simSendAT("AT+CIPSHUT", "SHUT OK", 10000);
  delay(1000);

  // Set connection type to GPRS
  if (!simSendATOK("AT+SAPBR=3,1,\"Contype\",\"GPRS\"")) {
    Serial.println("[SIM] Failed to set connection type.");
    return false;
  }

  // Set APN
  char apnCmd[128];
  snprintf(apnCmd, sizeof(apnCmd), "AT+SAPBR=3,1,\"APN\",\"%s\"", GPRS_APN);
  if (!simSendATOK(apnCmd)) {
    Serial.println("[SIM] Failed to set APN.");
    return false;
  }

  // Set APN username if provided
  if (strlen(GPRS_USER) > 0) {
    char userCmd[128];
    snprintf(userCmd, sizeof(userCmd), "AT+SAPBR=3,1,\"USER\",\"%s\"", GPRS_USER);
    simSendATOK(userCmd);
  }

  // Set APN password if provided
  if (strlen(GPRS_PASS) > 0) {
    char passCmd[128];
    snprintf(passCmd, sizeof(passCmd), "AT+SAPBR=3,1,\"PWD\",\"%s\"", GPRS_PASS);
    simSendATOK(passCmd);
  }

  // Open bearer
  if (!simSendAT("AT+SAPBR=1,1", "OK", 30000)) {
    Serial.println("[SIM] Failed to open bearer.");
    // Try once more after closing
    simSendAT("AT+SAPBR=0,1", "OK", 5000);
    delay(1000);
    if (!simSendAT("AT+SAPBR=1,1", "OK", 30000)) {
      Serial.println("[SIM] Bearer open failed on retry.");
      return false;
    }
  }

  // Query bearer — should return IP
  if (simSendAT("AT+SAPBR=2,1", "OK", 5000)) {
    Serial.printf("[SIM] Bearer info: %s\n", simResponseBuf);
    // Check we got a real IP (not 0.0.0.0)
    if (strstr(simResponseBuf, "0.0.0.0") != NULL) {
      Serial.println("[SIM] Got 0.0.0.0 — GPRS not connected.");
      return false;
    }
  }

  simGprsConnected = true;
  Serial.println("[SIM] GPRS connected.");
  return true;
}

/*
 * Disconnect GPRS and close bearer.
 */
void simDisconnectGPRS() {
  simSendAT("AT+HTTPTERM", "OK", 3000);
  simSendAT("AT+SAPBR=0,1", "OK", 5000);
  simSendAT("AT+CIPSHUT", "SHUT OK", 5000);
  simGprsConnected = false;
  Serial.println("[SIM] GPRS disconnected.");
}

/*
 * Perform an HTTP POST via SIM800L.
 * payload: JSON string to send
 * Returns HTTP status code (200 = success), or -1 on failure.
 */
int simHttpPost(const char* url, const char* payload) {
  int payloadLen = strlen(payload);

  // Init HTTP service
  if (!simSendATOK("AT+HTTPINIT")) {
    Serial.println("[SIM] HTTPINIT failed.");
    simSendAT("AT+HTTPTERM", "OK", 2000);  // cleanup
    if (!simSendATOK("AT+HTTPINIT")) {
      return -1;
    }
  }

  // Set bearer profile
  if (!simSendATOK("AT+HTTPPARA=\"CID\",1")) {
    simSendAT("AT+HTTPTERM", "OK", 2000);
    return -1;
  }

  // Set URL
  char urlCmd[256];
  snprintf(urlCmd, sizeof(urlCmd), "AT+HTTPPARA=\"URL\",\"%s\"", url);
  if (!simSendATOK(urlCmd)) {
    simSendAT("AT+HTTPTERM", "OK", 2000);
    return -1;
  }

  // Set content type
  if (!simSendATOK("AT+HTTPPARA=\"CONTENT\",\"application/json\"")) {
    simSendAT("AT+HTTPTERM", "OK", 2000);
    return -1;
  }

  // Prepare to send data — AT+HTTPDATA=<size>,<maxLatency>
  char dataCmd[32];
  snprintf(dataCmd, sizeof(dataCmd), "AT+HTTPDATA=%d,10000", payloadLen);
  if (!simSendAT(dataCmd, "DOWNLOAD", 5000)) {
    Serial.println("[SIM] HTTPDATA not ready for download.");
    simSendAT("AT+HTTPTERM", "OK", 2000);
    return -1;
  }

  // Send the actual payload bytes
  simSerial.write(payload, payloadLen);
  delay(100);

  // Wait for OK after data upload
  if (!simSendAT("", "OK", 10000)) {
    Serial.println("[SIM] Data upload not acknowledged.");
    simSendAT("AT+HTTPTERM", "OK", 2000);
    return -1;
  }

  // Execute POST
  // Response: +HTTPACTION: <method>,<statuscode>,<datalen>
  if (!simSendAT("AT+HTTPACTION=1", "+HTTPACTION:", SIM_HTTP_TIMEOUT_MS)) {
    Serial.println("[SIM] HTTPACTION timeout.");
    simSendAT("AT+HTTPTERM", "OK", 2000);
    return -1;
  }

  // Parse status code from response
  // Format: +HTTPACTION: 1,200,<len>
  int httpStatus = -1;
  char* actionPtr = strstr(simResponseBuf, "+HTTPACTION:");
  if (actionPtr != NULL) {
    int method = 0;
    int status = 0;
    int dataLen = 0;
    if (sscanf(actionPtr, "+HTTPACTION: %d,%d,%d", &method, &status, &dataLen) >= 2) {
      httpStatus = status;
    }
  }

  Serial.printf("[SIM] HTTP POST status: %d\n", httpStatus);

  // Optionally read response body (for debugging)
  if (httpStatus == 200) {
    simSendAT("AT+HTTPREAD", "OK", 5000);
    Serial.printf("[SIM] Response: %s\n", simResponseBuf);
  }

  // Terminate HTTP
  simSendAT("AT+HTTPTERM", "OK", 3000);

  return httpStatus;
}

/*
 * Full SIM800L transmit cycle:
 *   1. Ensure GPRS connected
 *   2. POST payload
 *   3. Handle success/failure
 * Returns true if POST succeeded (HTTP 200 or 201).
 */
bool simTransmit(const char* payload) {
  if (!simAvailable) {
    Serial.println("[SIM] Module not available.");
    return false;
  }

  // Ensure GPRS is connected
  if (!simGprsConnected) {
    if (!simConnectGPRS()) {
      Serial.println("[SIM] Cannot establish GPRS.");
      return false;
    }
  }

  // Attempt POST with retries
  for (int attempt = 0; attempt < SIM_MAX_RETRIES; attempt++) {
    if (attempt > 0) {
      Serial.printf("[SIM] Retry %d/%d...\n", attempt + 1, SIM_MAX_RETRIES);
      delay(5000);
      esp_task_wdt_reset();
    }

    int status = simHttpPost(RELAY_SERVER_URL, payload);

    if (status == 200 || status == 201) {
      Serial.println("[SIM] POST successful.");
      transmitSuccessCount++;
      lastSuccessChannel = COMM_SIM_GPRS;
      return true;
    }

    // On 4xx errors, don't retry (config issue, not transient)
    if (status >= 400 && status < 500) {
      Serial.printf("[SIM] HTTP %d — client error, not retrying.\n", status);
      transmitFailCount++;
      return false;
    }

    // On total failure or 5xx, try reconnecting GPRS
    if (status == -1 || status >= 500) {
      Serial.println("[SIM] Connection lost — re-establishing GPRS...");
      simDisconnectGPRS();
      delay(2000);
      if (!simConnectGPRS()) {
        Serial.println("[SIM] GPRS reconnect failed.");
      }
    }
  }

  transmitFailCount++;
  Serial.println("[SIM] All POST attempts failed.");
  return false;
}

// ─────────────────────────────────────────────────────────────────
//  PHASE 1F — WiFi + FIREBASE (FALLBACK PATH)
// ─────────────────────────────────────────────────────────────────

/*
 * Connect to WiFi network.
 * Returns true if connected within timeout.
 */
bool wifiConnect() {
  if (WiFi.status() == WL_CONNECTED) {
    wifiConnected = true;
    return true;
  }

  Serial.printf("[WIFI] Connecting to '%s'...\n", WIFI_SSID);
  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASS);

  uint32_t start = millis();
  while (WiFi.status() != WL_CONNECTED && (millis() - start) < WIFI_CONNECT_TIMEOUT) {
    delay(250);
    Serial.print(".");
  }

  if (WiFi.status() == WL_CONNECTED) {
    wifiConnected = true;
    Serial.printf("\n[WIFI] Connected. IP: %s\n", WiFi.localIP().toString().c_str());
    return true;
  }

  wifiConnected = false;
  Serial.println("\n[WIFI] Connection failed.");
  WiFi.disconnect(true);
  return false;
}

/*
 * POST JSON payload to Firebase Realtime Database via HTTPS.
 * Uses WiFiClientSecure for TLS.
 *
 * Firebase REST API:
 *   PUT https://<host>/varuna/live/<deviceId>.json?auth=<secret>
 *
 * Returns true on success (HTTP 200).
 */
bool firebasePost(const char* payload) {
  if (WiFi.status() != WL_CONNECTED) {
    if (!wifiConnect()) return false;
  }

  WiFiClientSecure client;
  client.setInsecure();  // Skip cert verification (acceptable for IoT data push)
  client.setTimeout(FIREBASE_TIMEOUT_MS / 1000);

  Serial.println("[FIREBASE] Connecting to host...");
  if (!client.connect(FIREBASE_HOST, 443)) {
    Serial.println("[FIREBASE] TLS connection failed.");
    return false;
  }

  // Build the PUT request
  // Path: /varuna/live/<DEVICE_ID>.json?auth=<SECRET>
  int payloadLen = strlen(payload);

  char pathBuf[256];
  snprintf(pathBuf, sizeof(pathBuf), "%s%s.json?auth=%s",
           FIREBASE_PATH, DEVICE_ID, FIREBASE_SECRET);

  // Send HTTP PUT request
  client.print("PUT ");
  client.print(pathBuf);
  client.println(" HTTP/1.1");
  client.print("Host: ");
  client.println(FIREBASE_HOST);
  client.println("Content-Type: application/json");
  client.print("Content-Length: ");
  client.println(payloadLen);
  client.println("Connection: close");
  client.println();
  client.print(payload);

  // Read response
  uint32_t start = millis();
  while (client.connected() && !client.available() &&
         (millis() - start) < FIREBASE_TIMEOUT_MS) {
    delay(10);
  }

  int httpStatus = 0;
  if (client.available()) {
    String statusLine = client.readStringUntil('\n');
    // Parse "HTTP/1.1 200 OK"
    int spaceIdx = statusLine.indexOf(' ');
    if (spaceIdx > 0) {
      httpStatus = statusLine.substring(spaceIdx + 1).toInt();
    }
  }

  // Consume remaining response
  while (client.available()) {
    client.read();
  }
  client.stop();

  Serial.printf("[FIREBASE] HTTP status: %d\n", httpStatus);

  if (httpStatus == 200) {
    transmitSuccessCount++;
    lastSuccessChannel = COMM_WIFI_FIREBASE;
    return true;
  }

  transmitFailCount++;
  return false;
}

// ─────────────────────────────────────────────────────────────────
//  PAYLOAD CONSTRUCTION
// ─────────────────────────────────────────────────────────────────

/*
 * Helper: string representations for enum values.
 */
const char* modeStr(OperatingMode m) {
  switch(m) {
    case MODE_0_SLACK:     return "SLACK";
    case MODE_1_TAUT:      return "TAUT";
    case MODE_2_FLOOD:     return "FLOOD";
    case MODE_3_SUBMERGED: return "SUBMERGED";
    default:               return "UNKNOWN";
  }
}
const char* levelStr(ResponseLevel l) {
  switch(l) {
    case LEVEL_NORMAL:   return "NORMAL";
    case LEVEL_WATCH:    return "WATCH";
    case LEVEL_WARNING:  return "WARNING";
    case LEVEL_FLOOD:    return "FLOOD";
    case LEVEL_CRITICAL: return "CRITICAL";
    default:             return "UNKNOWN";
  }
}
const char* zoneStr(HeightZone z) {
  switch(z) {
    case ZONE_NORMAL:  return "NORMAL";
    case ZONE_ALERT:   return "ALERT";
    case ZONE_WARNING: return "WARNING";
    case ZONE_DANGER:  return "DANGER";
    default:           return "UNKNOWN";
  }
}
const char* commStr(CommChannel c) {
  switch(c) {
    case COMM_SIM_GPRS:      return "SIM_GPRS";
    case COMM_WIFI_FIREBASE: return "WIFI_FIREBASE";
    default:                 return "NONE";
  }
}

/*
 * Build the JSON payload for server transmission.
 * Writes into payloadBuf. Returns the payload string pointer.
 */
const char* buildPayload() {
  uint32_t ts = 0;

  // Try GPS time first, fall back to millis-based estimate
  uint32_t gpsTime = gpsGetUnixTime();
  if (gpsTime > 0) {
    ts = gpsTime;
    // Also update bootUnixTime for EEPROM and CSV timestamps
    bootUnixTime = gpsTime - (millis() / 1000);
  } else {
    ts = millis() / 1000 + bootUnixTime;
  }

  snprintf(payloadBuf, PAYLOAD_BUF_SIZE,
    "{"
    "\"device_id\":\"%s\","
    "\"station\":\"%s\","
    "\"timestamp\":%lu,"
    "\"water_height_cm\":%.2f,"
    "\"response_level\":\"%s\","
    "\"response_level_num\":%d,"
    "\"zone\":\"%s\","
    "\"rate_per_15min\":%.2f,"
    "\"sustained_rise\":%s,"
    "\"operating_mode\":\"%s\","
    "\"tilt_angle_deg\":%.2f,"
    "\"lateral_accel\":%.3f,"
    "\"pressure_hpa\":%.2f,"
    "\"temperature_c\":%.2f,"
    "\"depth_below_surface_cm\":%.2f,"
    "\"battery_percent\":%d,"
    "\"health_score\":%d,"
    "\"olp_length_cm\":%.1f,"
    "\"peak_height_cm\":%.2f,"
    "\"gps_lat\":%.6f,"
    "\"gps_lon\":%.6f,"
    "\"gps_valid\":%s,"
    "\"gps_satellites\":%d,"
    "\"readings_since_boot\":%lu,"
    "\"uptime_s\":%lu,"
    "\"tx_success\":%lu,"
    "\"tx_fail\":%lu,"
    "\"last_channel\":\"%s\""
    "}",
    DEVICE_ID,
    STATION_NAME,
    ts,
    waterHeightCm,
    levelStr(currentResponseLevel),
    (int)currentResponseLevel,
    zoneStr(currentZone),
    ratePer15Min,
    sustainedRise ? "true" : "false",
    modeStr(currentMode),
    tiltAngleDeg,
    lateralAccel,
    currentPressurePa / 100.0f,
    currentTempC,
    depthBelowSurfaceCm,
    (int)batteryPercent,
    healthScore,
    olpLengthCm,
    peakHeightCm,
    gpsData.latitude,
    gpsData.longitude,
    gpsData.valid ? "true" : "false",
    (int)gpsData.satellites,
    readingsSinceBoot,
    millis() / 1000,
    transmitSuccessCount,
    transmitFailCount,
    commStr(lastSuccessChannel)
  );

  return payloadBuf;
}

// ─────────────────────────────────────────────────────────────────
//  STORE-AND-FORWARD
// ─────────────────────────────────────────────────────────────────

/*
 * Store a failed payload for later retry.
 */
void storePayloadForRetry(const char* payload) {
  // Find an empty slot
  for (int i = 0; i < STORE_FORWARD_MAX; i++) {
    if (!storedPayloads[i].occupied) {
      strncpy(storedPayloads[i].data, payload, PAYLOAD_BUF_SIZE - 1);
      storedPayloads[i].data[PAYLOAD_BUF_SIZE - 1] = '\0';
      storedPayloads[i].timestamp = millis() / 1000;
      storedPayloads[i].occupied = true;
      storedPayloadCount++;
      Serial.printf("[STORE] Payload stored in slot %d (total stored: %d)\n",
                    i, storedPayloadCount);
      return;
    }
  }

  // All slots full — overwrite oldest
  uint32_t oldest = 0xFFFFFFFF;
  int oldestIdx = 0;
  for (int i = 0; i < STORE_FORWARD_MAX; i++) {
    if (storedPayloads[i].timestamp < oldest) {
      oldest = storedPayloads[i].timestamp;
      oldestIdx = i;
    }
  }
  strncpy(storedPayloads[oldestIdx].data, payload, PAYLOAD_BUF_SIZE - 1);
  storedPayloads[oldestIdx].data[PAYLOAD_BUF_SIZE - 1] = '\0';
  storedPayloads[oldestIdx].timestamp = millis() / 1000;
  Serial.printf("[STORE] Buffer full — overwrote oldest slot %d\n", oldestIdx);
}

/*
 * Attempt to transmit any stored payloads.
 * Called after a successful fresh transmission to drain the buffer.
 */
void drainStoredPayloads() {
  if (storedPayloadCount == 0) return;

  Serial.printf("[STORE] Attempting to drain %d stored payloads...\n", storedPayloadCount);

  for (int i = 0; i < STORE_FORWARD_MAX; i++) {
    if (!storedPayloads[i].occupied) continue;

    esp_task_wdt_reset();

    bool sent = false;
    // Try SIM first
    if (simAvailable) {
      sent = simTransmit(storedPayloads[i].data);
    }
    // Try WiFi/Firebase as fallback
    if (!sent) {
      sent = firebasePost(storedPayloads[i].data);
    }

    if (sent) {
      storedPayloads[i].occupied = false;
      storedPayloadCount--;
      Serial.printf("[STORE] Slot %d transmitted successfully. Remaining: %d\n",
                    i, storedPayloadCount);
    } else {
      // Stop trying — if one fails, rest probably will too
      Serial.println("[STORE] Drain halted — transmission failed.");
      return;
    }
  }
  Serial.println("[STORE] All stored payloads drained.");
}

/*
 * Master transmit function.
 * Tries SIM800L first, then WiFi+Firebase.
 * On total failure, stores the payload for retry.
 */
void transmitData() {
  const char* payload = buildPayload();
  lastTransmitAttempt = millis();

  Serial.println("[TX] === Transmit cycle start ===");
  Serial.printf("[TX] Payload length: %d bytes\n", (int)strlen(payload));

  bool success = false;

  // ── Path 1: SIM800L GPRS ─────────────────────────────────────
  if (simAvailable) {
    Serial.println("[TX] Trying SIM800L GPRS...");
    success = simTransmit(payload);
    if (success) {
      Serial.println("[TX] SIM800L POST succeeded.");
    }
  }

  // ── Path 2: WiFi + Firebase (fallback) ───────────────────────
  if (!success) {
    Serial.println("[TX] Trying WiFi + Firebase...");
    success = firebasePost(payload);
    if (success) {
      Serial.println("[TX] Firebase POST succeeded.");
    }
  }

  // ── Store on total failure ───────────────────────────────────
  if (!success) {
    Serial.println("[TX] All channels failed. Storing for retry.");
    storePayloadForRetry(payload);
  } else {
    // Successful — try to drain any stored payloads
    drainStoredPayloads();
  }

  Serial.printf("[TX] === Transmit cycle end === success=%d total_ok=%lu total_fail=%lu stored=%d\n",
                (int)success, transmitSuccessCount, transmitFailCount, storedPayloadCount);
}

// ─────────────────────────────────────────────────────────────────
//  SIM800L INBOUND SMS (DIAGNOSTIC ONLY)
//  Handles: STATUS, PING, GETCONFIG, CAL
//  Does NOT handle any alerting. This is purely for field engineers.
// ─────────────────────────────────────────────────────────────────

/*
 * Check for incoming SMS messages.
 * Only processes a small set of diagnostic commands.
 * Call periodically (e.g., every 30 seconds).
 */
void simCheckInboundSMS() {
  if (!simAvailable) return;

  // Set SMS to text mode
  simSendATOK("AT+CMGF=1");

  // Read first unread SMS
  if (!simSendAT("AT+CMGL=\"REC UNREAD\"", "OK", 5000)) {
    return;  // no unread messages or command failed
  }

  // Check if there's actually an SMS in the response
  if (strstr(simResponseBuf, "+CMGL:") == NULL) {
    return;  // no messages
  }

  // Extract the SMS body — it appears after the +CMGL header line
  // Format: +CMGL: <index>,"REC UNREAD","<number>","","<date>"<CRLF><message body><CRLF>
  char* msgBody = strstr(simResponseBuf, "\r\n");
  if (msgBody == NULL) return;
  msgBody += 2;  // skip first CRLF (past the header)

  // Find the actual message line (skip +CMGL header)
  char* headerEnd = strstr(msgBody, "\r\n");
  if (headerEnd != NULL && headerEnd != msgBody) {
    // The first line after +CMGL is still part of the header
    // The message body comes after
    // Actually, let's re-parse more carefully
  }

  // Simpler approach: look for known commands anywhere in the buffer
  char cmdUpper[256];
  strncpy(cmdUpper, simResponseBuf, sizeof(cmdUpper) - 1);
  cmdUpper[sizeof(cmdUpper) - 1] = '\0';
  // Convert to uppercase
  for (int i = 0; cmdUpper[i] != '\0'; i++) {
    if (cmdUpper[i] >= 'a' && cmdUpper[i] <= 'z') {
      cmdUpper[i] -= 32;
    }
  }

  // Extract sender phone number for reply
  char senderNum[20] = {0};
  char* numStart = strstr(simResponseBuf, "\",\"");
  if (numStart != NULL) {
    numStart += 3;  // skip ","
    char* numEnd = strchr(numStart, '"');
    if (numEnd != NULL) {
      int numLen = (int)(numEnd - numStart);
      if (numLen > 0 && numLen < 20) {
        strncpy(senderNum, numStart, numLen);
        senderNum[numLen] = '\0';
      }
    }
  }

  // Process recognised commands
  char replyBuf[160];  // SMS max single segment
  bool sendReply = false;

  if (strstr(cmdUpper, "STATUS") != NULL) {
    snprintf(replyBuf, sizeof(replyBuf),
      "VARUNA %s|H:%.0fcm|%s|%s|Rate:%.1f|Batt:%d%%|HP:%d|Up:%lus",
      DEVICE_ID, waterHeightCm, levelStr(currentResponseLevel),
      modeStr(currentMode), ratePer15Min, batteryPercent,
      healthScore, millis() / 1000);
    sendReply = true;
  }
  else if (strstr(cmdUpper, "PING") != NULL) {
    snprintf(replyBuf, sizeof(replyBuf), "PONG:%s:%lu", DEVICE_ID, millis() / 1000);
    sendReply = true;
  }
  else if (strstr(cmdUpper, "GETCONFIG") != NULL) {
    snprintf(replyBuf, sizeof(replyBuf),
      "OLP:%.0fcm|A:%d W:%d D:%d|ALGO:%s|GPS:%s|SIM:%s",
      olpLengthCm, alertLevelCm, warnLevelCm, dangerLevelCm,
      algorithmEnabled ? "ON" : "OFF",
      gpsData.valid ? "FIX" : "NOFIX",
      simGprsConnected ? "GPRS" : "NO");
    sendReply = true;
  }

  // Send reply SMS if we have a command and a sender number
  if (sendReply && strlen(senderNum) > 3) {
    Serial.printf("[SMS] Replying to %s: %s\n", senderNum, replyBuf);
    char smsCmd[64];
    snprintf(smsCmd, sizeof(smsCmd), "AT+CMGS=\"%s\"", senderNum);
    if (simSendAT(smsCmd, ">", 5000)) {
      simSerial.print(replyBuf);
      simSerial.write(0x1A);  // Ctrl+Z to send
      simSendAT("", "OK", 30000);  // wait for send confirmation
    }
  }

  // Delete all read messages to keep SIM memory clean
  simSendATOK("AT+CMGD=1,4");
}

// ─────────────────────────────────────────────────────────────────
//  PHASE 2 — CORE MEASUREMENT ENGINE
// ─────────────────────────────────────────────────────────────────

void updateComplementaryFilter(float dt_ms) {
  float dt_s = dt_ms / 1000.0f;

  float accelAngleX = atan2f(ay_ms2, sqrtf(ax_ms2*ax_ms2 + az_ms2*az_ms2)) * 180.0f / M_PI;
  float accelAngleY = atan2f(-ax_ms2, sqrtf(ay_ms2*ay_ms2 + az_ms2*az_ms2)) * 180.0f / M_PI;

  filtTiltX = ALPHA * (filtTiltX + gx_dps * dt_s) + (1.0f - ALPHA) * accelAngleX;
  filtTiltY = ALPHA * (filtTiltY + gy_dps * dt_s) + (1.0f - ALPHA) * accelAngleY;

  float corrX = filtTiltX - refTiltX;
  float corrY = filtTiltY - refTiltY;

  tiltAngleDeg = sqrtf(corrX*corrX + corrY*corrY);

  lateralAccel = sqrtf(ax_ms2*ax_ms2 + ay_ms2*ay_ms2);

  float accelMag = sqrtf(ax_ms2*ax_ms2 + ay_ms2*ay_ms2 + az_ms2*az_ms2);
  accelVarBuf[accelVarIdx % ACCEL_VAR_BUF] = accelMag;
  accelVarIdx++;
  if (accelVarCount < ACCEL_VAR_BUF) accelVarCount++;

  float gErr = fabsf(accelMag - GRAVITY);
  if (gErr > 1.0f) {
    gravityErrCount++;
    if (gravityErrCount >= 20) {
      Serial.printf("[FAULT] ACCEL_CALIBRATION_DRIFT — |g|=%.3f m/s²\n", accelMag);
    }
  } else {
    gravityErrCount = 0;
  }
}

OperatingMode determineOperatingMode() {
  float gaugePa = currentPressurePa - baselinePressurePa;

  if (bmpAvailable && gaugePa > SUBMERSION_THRESH_PA) {
    return MODE_3_SUBMERGED;
  }

  static bool tetherTaut = false;
  if (!tetherTaut) {
    if (lateralAccel > TETHER_TAUT_THRESH && tiltAngleDeg > TILT_TAUT_MIN_DEG) {
      tetherTaut = true;
    }
  } else {
    if (lateralAccel < TETHER_SLACK_THRESH) {
      tetherTaut = false;
    }
  }

  if (!tetherTaut) {
    return MODE_0_SLACK;
  }

  float H_cm = olpLengthCm * cosf(tiltAngleDeg * M_PI / 180.0f);
  H_cm = max(0.0f, H_cm);

  if (tiltAngleDeg < FLOOD_NEAR_ANGLE && H_cm > olpLengthCm * FLOOD_NEAR_FRACTION) {
    return MODE_2_FLOOD;
  }

  return MODE_1_TAUT;
}

void computeWaterHeight() {
  float gaugePa = currentPressurePa - baselinePressurePa;

  switch (currentMode) {
    case MODE_0_SLACK:
      waterHeightCm = 0;
      depthBelowSurfaceCm = 0;
      break;

    case MODE_1_TAUT:
    case MODE_2_FLOOD:
      waterHeightCm = olpLengthCm * cosf(tiltAngleDeg * M_PI / 180.0f);
      waterHeightCm = max(0.0f, waterHeightCm);
      depthBelowSurfaceCm = 0;
      break;

    case MODE_3_SUBMERGED:
      depthBelowSurfaceCm = (gaugePa / (WATER_DENSITY * GRAVITY)) * 100.0f;
      waterHeightCm = olpLengthCm + depthBelowSurfaceCm;
      break;
  }

  if (waterHeightCm > peakHeightCm) {
    peakHeightCm   = waterHeightCm;
    peakHeightTime = millis() / 1000;
  }
}

void runMeasurementEngine() {
  uint32_t now = millis();
  float dt_ms = (lastMpuRead == 0) ? 10.0f : (float)(now - lastMpuRead);
  lastMpuRead = now;

  if (mpuHealthy) {
    mpuRead();
    updateComplementaryFilter(dt_ms);

    if (accelVarIdx % 50 == 0) mpuCheckFrozen();
  }

  if (bmpAvailable && (now - lastBmpRead >= 5000)) {
    bmpRead();
    bmpCheckFrozen();
    lastBmpRead = now;
  }

  if (currentMode == MODE_0_SLACK && mpuHealthy) {
    mpuAutoCalibrateModeZero();
  }

  currentMode = determineOperatingMode();
  computeWaterHeight();
}

// ─────────────────────────────────────────────────────────────────
//  PHASE 3 — FLOOD DETECTION STATE MACHINE
// ─────────────────────────────────────────────────────────────────

HeightZone classifyZone(float h_cm) {
  if (h_cm >= dangerLevelCm)  return ZONE_DANGER;
  if (h_cm >= warnLevelCm)    return ZONE_WARNING;
  if (h_cm >= alertLevelCm)   return ZONE_ALERT;
  return ZONE_NORMAL;
}

int classifyRateIndex(float rate) {
  if (rate <= RATE_SLOW_MAX) return 0;
  if (rate <= RATE_MOD_MAX)  return 1;
  return 2;
}

void calculateRateOfChange(float h_cm) {
  uint32_t now = millis() / 1000;
  if (prevHeightTime == 0) {
    prevHeightCm   = h_cm;
    prevHeightTime = now;
    ratePer15Min   = 0;
    return;
  }
  uint32_t elapsed = now - prevHeightTime;
  if (elapsed < 60) return;

  float change = h_cm - prevHeightCm;
  ratePer15Min = change * (900.0f / (float)elapsed);
  if (ratePer15Min >  RATE_CLAMP) ratePer15Min =  RATE_CLAMP;
  if (ratePer15Min < -RATE_CLAMP) ratePer15Min = -RATE_CLAMP;

  prevHeightCm   = h_cm;
  prevHeightTime = now;
}

void updateSustainedBuffer(float h_cm) {
  uint32_t now = millis() / 1000;
  sustainedBuf[sustainedBufHead]     = h_cm;
  sustainedTimeBuf[sustainedBufHead] = now;
  sustainedBufHead = (sustainedBufHead + 1) % SUSTAINED_BUF_SIZE;
  if (sustainedBufCount < SUSTAINED_BUF_SIZE) sustainedBufCount++;

  if (sustainedBufCount < SUSTAINED_BUF_SIZE) {
    sustainedRise = false;
    return;
  }

  float ordered[SUSTAINED_BUF_SIZE];
  for (int i = 0; i < SUSTAINED_BUF_SIZE; i++) {
    int idx = ((sustainedBufHead - SUSTAINED_BUF_SIZE) + i + SUSTAINED_BUF_SIZE) % SUSTAINED_BUF_SIZE;
    ordered[i] = sustainedBuf[idx];
  }

  bool netRising = (ordered[SUSTAINED_BUF_SIZE - 1] > ordered[0] + 0.5f);

  int riseCount = 0;
  for (int i = 0; i < SUSTAINED_BUF_SIZE - 1; i++) {
    if (ordered[i + 1] > ordered[i] + 0.5f) riseCount++;
  }

  sustainedRise = netRising && (riseCount >= 2);
}

void flushSustainedBufferIfIntervalChanged(uint32_t newInterval, uint32_t oldInterval) {
  if (oldInterval == 0) return;
  if (newInterval == 0) return;
  bool bigChange = (oldInterval >= 4 * newInterval) || (newInterval >= 4 * oldInterval);
  if (bigChange) {
    Serial.printf("[SUSTAINED] Interval changed %lums → %lums — flushing buffer.\n",
                  oldInterval, newInterval);
    for (int i = 0; i < SUSTAINED_BUF_SIZE; i++) {
      sustainedBuf[i]     = prevHeightCm;
      sustainedTimeBuf[i] = millis() / 1000;
    }
    sustainedBufCount = SUSTAINED_BUF_SIZE;
    sustainedRise     = false;
  }
}

bool applyOverrides(HeightZone zone, float rate) {
  bool eff = sustainedRise;

  if (readingsSinceBoot <= 4 && zone >= ZONE_WARNING) {
    eff = true;
    Serial.println("[OVERRIDE] Cold start into flood — forcing sustained=true.");
  }

  if (fabsf(rate) > RATE_EXTREME && zone >= ZONE_ALERT) {
    eff = true;
    Serial.printf("[OVERRIDE] Extreme rate %.1f cm/15min — forcing sustained=true.\n", rate);
  }

  if (fabsf(rate) > RATE_CATASTROPHIC) {
    eff = true;
    Serial.printf("[OVERRIDE] CATASTROPHIC rate %.1f cm/15min — forcing sustained=true.\n", rate);
  }

  return eff;
}

ResponseLevel lookupDecisionMatrix(HeightZone zone, float rate, bool sustained) {
  int zoneIdx = (int)zone;
  int rateIdx = classifyRateIndex(fabsf(rate));
  int sustIdx = sustained ? 1 : 0;
  return (ResponseLevel)DECISION_MATRIX[zoneIdx][rateIdx][sustIdx];
}

ResponseLevel evaluateStepDown(ResponseLevel current, ResponseLevel matrixLevel,
                               float h_cm, float rate) {
  uint32_t now_s = millis() / 1000;
  uint32_t timeAtLevel = now_s - stateEntryTime;

  if (matrixLevel > current) {
    stepDownConsecutive = 0;
    return matrixLevel;
  }

  if (matrixLevel == current) {
    stepDownConsecutive = 0;
    return current;
  }

  bool conditionMet = false;
  switch (current) {
    case LEVEL_CRITICAL:
      conditionMet = (h_cm < dangerLevelCm) && (rate <= RATE_SLOW_MAX);
      break;
    case LEVEL_FLOOD:
      conditionMet = (h_cm < warnLevelCm);
      break;
    case LEVEL_WARNING:
      conditionMet = (h_cm < alertLevelCm);
      break;
    case LEVEL_WATCH:
      conditionMet = (h_cm < alertLevelCm) && (rate <= RATE_SLOW_MAX) && !sustainedRise;
      break;
    case LEVEL_NORMAL:
      return LEVEL_NORMAL;
  }

  if (!conditionMet) {
    stepDownConsecutive = 0;
    return current;
  }

  stepDownConsecutive++;

  int readingsNeeded = (current == LEVEL_WATCH) ? STEPDOWN_TO_NORMAL : STEPDOWN_READINGS;
  uint32_t timeNeeded = 0;
  switch (current) {
    case LEVEL_CRITICAL: timeNeeded = MIN_TIME_CRITICAL_S; break;
    case LEVEL_FLOOD:    timeNeeded = MIN_TIME_FLOOD_S;    break;
    case LEVEL_WARNING:  timeNeeded = MIN_TIME_WARNING_S;  break;
    case LEVEL_WATCH:    timeNeeded = MIN_TIME_WATCH_S;    break;
    default: break;
  }

  if (stepDownConsecutive >= readingsNeeded && timeAtLevel >= timeNeeded) {
    stepDownConsecutive = 0;
    stateEntryTime      = now_s;
    ResponseLevel newLevel = (ResponseLevel)((int)current - 1);
    Serial.printf("[STATE] Level stepped DOWN: %d → %d\n", (int)current, (int)newLevel);
    return newLevel;
  }

  return current;
}

void evaluateFloodStatus() {
  readingsSinceBoot++;

  float h = (currentMode == MODE_0_SLACK) ? 0 : waterHeightCm;
  currentZone = classifyZone(h);

  calculateRateOfChange(h);

  updateSustainedBuffer(h);

  bool effectiveSustained = applyOverrides(currentZone, ratePer15Min);

  ResponseLevel matrixLevel = lookupDecisionMatrix(currentZone, ratePer15Min, effectiveSustained);

  if (readingsSinceBoot == 1) {
    if      (currentZone >= ZONE_DANGER)  currentResponseLevel = LEVEL_CRITICAL;
    else if (currentZone >= ZONE_WARNING) currentResponseLevel = LEVEL_WARNING;
    else                                  currentResponseLevel = LEVEL_NORMAL;
    stateEntryTime = millis() / 1000;
    Serial.printf("[COLDSTART] First reading → level=%d zone=%d\n",
                  (int)currentResponseLevel, (int)currentZone);
    return;
  }

  ResponseLevel prevLevel = currentResponseLevel;
  currentResponseLevel    = evaluateStepDown(currentResponseLevel, matrixLevel, h, ratePer15Min);

  if (currentResponseLevel != prevLevel) {
    const char* levelNames[] = {"NORMAL","WATCH","WARNING","FLOOD","CRITICAL"};
    Serial.printf("[STATE] LEVEL CHANGE: %s → %s  |  H=%.1fcm  rate=%.1fcm/15m  sustained=%d\n",
                  levelNames[prevLevel], levelNames[currentResponseLevel],
                  h, ratePer15Min, (int)effectiveSustained);

    forceSaveEeprom();

    // On any level change, trigger an immediate transmit
    // so the server can dispatch notifications without waiting
    // for the next regular transmit interval
    Serial.println("[STATE] Level change detected — triggering immediate transmit.");
    transmitData();
  }
}

// ─────────────────────────────────────────────────────────────────
//  ADAPTIVE SAMPLING
// ─────────────────────────────────────────────────────────────────

void updateAdaptiveIntervals() {
  uint32_t oldSample = sampleInterval;

  int lIdx = (int)currentResponseLevel;
  int bIdx = (currentResponseLevel >= LEVEL_WARNING) ? 0 : batteryState;

  sampleInterval   = (uint32_t)SAMPLE_INTERVALS  [lIdx][bIdx] * 1000UL;
  transmitInterval = (uint32_t)TRANSMIT_INTERVALS [lIdx][bIdx] * 1000UL;

  if (sampleInterval != oldSample) {
    Serial.printf("[ADAPTIVE] Sample=%lus  Transmit=%lus  (level=%d batt=%d)\n",
                  sampleInterval/1000, transmitInterval/1000,
                  (int)currentResponseLevel, bIdx);
    flushSustainedBufferIfIntervalChanged(sampleInterval, oldSample);
  }
}

// ─────────────────────────────────────────────────────────────────
//  HEALTH SCORE
// ─────────────────────────────────────────────────────────────────

void updateHealthScore() {
  healthScore = 100;
  if (!mpuHealthy)   healthScore -= 30;
  if (!bmpAvailable) healthScore -= 20;
  if (batteryState == 1) healthScore -= 10;
  if (batteryState == 2) healthScore -= 25;
  if (mpuFrozenCount > 0) healthScore -= 10;
  if (!simAvailable) healthScore -= 5;
  if (!gpsData.valid) healthScore -= 5;
  if (storedPayloadCount > 0) healthScore -= 5;
  healthScore = max(0, healthScore);
}

// ─────────────────────────────────────────────────────────────────
//  EEPROM PERSISTENCE
// ─────────────────────────────────────────────────────────────────

void forceSaveEeprom() {
  uint32_t nowSec = millis() / 1000 + bootUnixTime;
  EEPROM.write(EE_MAGIC,      EEPROM_MAGIC_BYTE);
  eepromWriteUint32(EE_SAVE_TIME,  nowSec);
  EEPROM.write(EE_RESP_LEVEL,      (uint8_t)currentResponseLevel);
  EEPROM.write(EE_SUSTAINED,       (uint8_t)sustainedRise);
  eepromWriteFloat(EE_OLP_CM,      olpLengthCm);
  eepromWriteInt16(EE_ALERT_CM,    (int16_t)alertLevelCm);
  eepromWriteInt16(EE_WARN_CM,     (int16_t)warnLevelCm);
  eepromWriteInt16(EE_DANGER_CM,   (int16_t)dangerLevelCm);
  eepromWriteFloat(EE_PEAK_CM,     peakHeightCm);
  eepromWriteUint32(EE_PEAK_TIME,  peakHeightTime);
  eepromWriteFloat(EE_PREV_H,      prevHeightCm);
  eepromWriteFloat(EE_BASELINE_P,  baselinePressurePa);
  EEPROM.commit();
  lastEepromSave = millis();
  Serial.println("[EEPROM] State saved.");
}

void attemptWarmBoot() {
  if (EEPROM.read(EE_MAGIC) != EEPROM_MAGIC_BYTE) {
    Serial.println("[BOOT] Cold boot — EEPROM magic mismatch.");
    return;
  }
  uint32_t saveTime = eepromReadUint32(EE_SAVE_TIME);
  uint32_t nowSec   = millis() / 1000 + bootUnixTime;

  // If bootUnixTime is 0 (no RTC/GPS yet), we cannot reliably determine age.
  // Use a heuristic: if saveTime looks like a real Unix timestamp (> 1700000000,
  // which is Nov 2023) and nowSec is small (no GPS yet), defer warm boot decision
  // until GPS fix provides bootUnixTime. For now, try to restore anyway if the
  // saved data looks valid — the state machine will self-correct quickly.
  if (bootUnixTime == 0 && saveTime > 1700000000UL) {
    Serial.println("[BOOT] No RTC/GPS time yet. Restoring saved state optimistically.");
    // Restore without age check — state machine has hysteresis to self-correct
  } else {
    uint32_t age = (nowSec >= saveTime) ? (nowSec - saveTime) : 0xFFFFFFFF;
    if (age > EEPROM_WARM_MAX_S) {
      Serial.printf("[BOOT] Saved state too old (%lus). Cold boot.\n", age);
      return;
    }
    Serial.printf("[BOOT] Warm boot — restoring state (saved %lus ago).\n", age);
  }

  currentResponseLevel = (ResponseLevel)EEPROM.read(EE_RESP_LEVEL);
  if ((int)currentResponseLevel > (int)LEVEL_CRITICAL) {
    currentResponseLevel = LEVEL_NORMAL;  // sanitize
  }
  sustainedRise        = (bool)EEPROM.read(EE_SUSTAINED);
  olpLengthCm          = eepromReadFloat(EE_OLP_CM);
  if (olpLengthCm < 10.0f || olpLengthCm > 2000.0f) olpLengthCm = 200.0f;  // sanitize
  alertLevelCm         = eepromReadInt16(EE_ALERT_CM);
  if (alertLevelCm <= 0 || alertLevelCm > 5000) alertLevelCm = DEFAULT_ALERT_CM;
  warnLevelCm          = eepromReadInt16(EE_WARN_CM);
  if (warnLevelCm <= alertLevelCm || warnLevelCm > 5000) warnLevelCm = DEFAULT_WARN_CM;
  dangerLevelCm        = eepromReadInt16(EE_DANGER_CM);
  if (dangerLevelCm <= warnLevelCm || dangerLevelCm > 5000) dangerLevelCm = DEFAULT_DANGER_CM;
  peakHeightCm         = eepromReadFloat(EE_PEAK_CM);
  if (peakHeightCm < 0 || peakHeightCm > 10000.0f) peakHeightCm = 0;
  peakHeightTime       = eepromReadUint32(EE_PEAK_TIME);
  prevHeightCm         = eepromReadFloat(EE_PREV_H);
  if (prevHeightCm < 0 || prevHeightCm > 10000.0f) prevHeightCm = 0;
  baselinePressurePa   = eepromReadFloat(EE_BASELINE_P);
  if (baselinePressurePa < 30000.0f || baselinePressurePa > 120000.0f) {
    baselinePressurePa = 101325.0f;
  }
  stateEntryTime       = 0;

  Serial.printf("[BOOT] Restored: level=%d  olp=%.1fcm  alert=%d warn=%d danger=%d\n",
                (int)currentResponseLevel, olpLengthCm,
                alertLevelCm, warnLevelCm, dangerLevelCm);
}

// ─────────────────────────────────────────────────────────────────
//  CSV OUTPUT
// ─────────────────────────────────────────────────────────────────

void printCSV() {
  uint32_t ts = millis() / 1000 + bootUnixTime;
  Serial.printf("CSV,%lu,%.2f,%.2f,%.2f,%.2f,%.3f,%s,%s,%s,%d,%d,%d,%.2f,%.2f,%d,%d,%.1f,%.6f,%.6f,%d,%d,%d\n",
    ts,
    waterHeightCm,
    ratePer15Min,
    tiltAngleDeg,
    lateralAccel,
    currentPressurePa / 100.0f,
    modeStr(currentMode),
    levelStr(currentResponseLevel),
    zoneStr(currentZone),
    (int)sustainedRise,
    batteryPercent,
    healthScore,
    olpLengthCm,
    depthBelowSurfaceCm,
    (int)mpuHealthy,
    (int)bmpAvailable,
    currentTempC,
    gpsData.latitude,
    gpsData.longitude,
    (int)gpsData.valid,
    (int)simAvailable,
    storedPayloadCount
  );
}
// ─────────────────────────────────────────────────────────────────
//  SERIAL COMMAND PROCESSOR
// ─────────────────────────────────────────────────────────────────

void printStatus() {
  const char* modeNames[] = {"SLACK","TAUT","FLOOD","SUBMERGED"};
  Serial.println("=== VARUNA STATUS ===");
  Serial.printf("  Device ID     : %s\n",        DEVICE_ID);
  Serial.printf("  Station       : %s\n",        STATION_NAME);
  Serial.printf("  Water Height  : %.1f cm\n",   waterHeightCm);
  Serial.printf("  Mode          : %s\n",        modeNames[(int)currentMode]);
  Serial.printf("  Response Level: %s\n",        levelStr(currentResponseLevel));
  Serial.printf("  Zone          : %s\n",        zoneStr(currentZone));
  Serial.printf("  Tilt θ        : %.2f °\n",    tiltAngleDeg);
  Serial.printf("  Lat. Accel    : %.3f m/s²\n", lateralAccel);
  Serial.printf("  Rate/15min    : %.2f cm\n",    ratePer15Min);
  Serial.printf("  Sustained Rise: %s\n",        sustainedRise ? "YES" : "NO");
  Serial.printf("  Pressure      : %.2f hPa\n",  currentPressurePa / 100.0f);
  Serial.printf("  Baseline P    : %.2f hPa\n",  baselinePressurePa / 100.0f);
  Serial.printf("  Submersion    : %.1f cm\n",    depthBelowSurfaceCm);
  Serial.printf("  Battery       : %d%%\n",       batteryPercent);
  Serial.printf("  Health Score  : %d/100\n",     healthScore);
  Serial.printf("  Peak Height   : %.1f cm\n",    peakHeightCm);
  Serial.printf("  OLP Length    : %.1f cm\n",    olpLengthCm);
  Serial.printf("  Algorithm     : %s\n",        algorithmEnabled ? "ENABLED" : "DISABLED");
  Serial.printf("  Uptime        : %lu s\n",     millis() / 1000);
  Serial.printf("  GPS           : %s  (%.6f, %.6f)  Sats=%d\n",
                gpsData.valid ? "FIX" : "NO FIX",
                gpsData.latitude, gpsData.longitude, gpsData.satellites);
  Serial.printf("  SIM800L       : %s  GPRS=%s\n",
                simAvailable ? "OK" : "N/A",
                simGprsConnected ? "CONNECTED" : "NO");
  Serial.printf("  WiFi          : %s\n",
                (WiFi.status() == WL_CONNECTED) ? WiFi.localIP().toString().c_str() : "N/A");
  Serial.printf("  TX OK/Fail    : %lu / %lu\n", transmitSuccessCount, transmitFailCount);
  Serial.printf("  Stored payloads: %d\n",       storedPayloadCount);
  Serial.printf("  Last TX ch    : %s\n",        commStr(lastSuccessChannel));
  Serial.println("====================");
}

void printSensorTest() {
  Serial.println("=== SENSORTEST ===");
  Serial.printf("  ax=%.3f  ay=%.3f  az=%.3f m/s²\n", ax_ms2, ay_ms2, az_ms2);
  Serial.printf("  gx=%.3f  gy=%.3f  gz=%.3f °/s\n",  gx_dps, gy_dps, gz_dps);
  Serial.printf("  filtTiltX=%.2f  filtTiltY=%.2f °\n", filtTiltX, filtTiltY);
  Serial.printf("  tiltAngle=%.2f°  lateralA=%.3f m/s²\n", tiltAngleDeg, lateralAccel);
  Serial.printf("  P=%.2f hPa  T=%.2f°C\n",
                currentPressurePa / 100.0f, currentTempC);
  Serial.printf("  MPU healthy=%d  BMP available=%d\n", (int)mpuHealthy, (int)bmpAvailable);
  Serial.printf("  GPS: valid=%d  lat=%.6f  lon=%.6f  sats=%d\n",
                (int)gpsData.valid, gpsData.latitude, gpsData.longitude, gpsData.satellites);
  if (gpsData.valid) {
    Serial.printf("  GPS time: %04d-%02d-%02d %02d:%02d:%02d UTC\n",
                  gpsData.year, gpsData.month, gpsData.day,
                  gpsData.hour, gpsData.minute, gpsData.second);
  }
  Serial.println("==================");
}

void processCommand(String cmd) {
  cmd.trim();
  cmd.toUpperCase();

  if (cmd == "STATUS")          { printStatus(); }
  else if (cmd == "SENSORTEST") { mpuRead(); bmpRead(); gpsUpdate(); printSensorTest(); }
  else if (cmd == "PING")       { Serial.printf("PONG:%lu\n", millis() / 1000); }
  else if (cmd == "BATT")       { updateBattery(); Serial.printf("BATT:%d%%\n", batteryPercent); }
  else if (cmd == "SAVEEEPROM") { forceSaveEeprom(); }
  else if (cmd == "GETTHRESH") {
    Serial.printf("ALERT=%d  WARN=%d  DANGER=%d (cm)\n", alertLevelCm, warnLevelCm, dangerLevelCm);
  }
  else if (cmd == "GETCONFIG") {
    Serial.printf("OLP=%.1fcm  ALGO=%s  SAMPLE=%lus  TX=%lus\n",
                  olpLengthCm, algorithmEnabled ? "ON" : "OFF",
                  sampleInterval/1000, transmitInterval/1000);
    Serial.printf("SIM=%s  GPRS=%s  WiFi=%s  GPS=%s\n",
                  simAvailable ? "OK" : "N/A",
                  simGprsConnected ? "YES" : "NO",
                  (WiFi.status() == WL_CONNECTED) ? "YES" : "NO",
                  gpsData.valid ? "FIX" : "NOFIX");
  }
  else if (cmd == "FLOODSTATUS") {
    Serial.printf("LEVEL=%s ZONE=%s RATE=%.1f SUSTAINED=%d STEPDOWN=%d READINGS=%lu\n",
                  levelStr(currentResponseLevel), zoneStr(currentZone),
                  ratePer15Min, (int)sustainedRise, stepDownConsecutive, readingsSinceBoot);
  }
  else if (cmd == "CAL") {
    if (!algorithmEnabled) { mpuCalibrate(); }
    else { Serial.println("ERR: Disable algorithm before calibrating."); }
  }
  else if (cmd == "TXNOW") {
    Serial.println("[CMD] Manual transmit triggered.");
    transmitData();
  }
  else if (cmd == "SIMTEST") {
    Serial.println("[CMD] Testing SIM800L...");
    if (simSendAT("AT", "OK", 3000)) {
      Serial.println("  AT: OK");
      simSendAT("AT+CSQ", "OK", 3000);
      Serial.printf("  Signal: %s\n", simResponseBuf);
      simSendAT("AT+CREG?", "OK", 3000);
      Serial.printf("  Reg: %s\n", simResponseBuf);
    } else {
      Serial.println("  AT: FAILED — module not responding.");
    }
  }
  else if (cmd == "GPSTEST") {
    Serial.println("[CMD] GPS status:");
    Serial.printf("  Valid: %s\n", gpsData.valid ? "YES" : "NO");
    Serial.printf("  Lat: %.6f  Lon: %.6f\n", gpsData.latitude, gpsData.longitude);
    Serial.printf("  Alt: %.1fm  Speed: %.1f km/h\n", gpsData.altitude, gpsData.speed);
    Serial.printf("  Sats: %d\n", gpsData.satellites);
    if (gpsData.valid) {
      Serial.printf("  Time: %04d-%02d-%02d %02d:%02d:%02d UTC\n",
                    gpsData.year, gpsData.month, gpsData.day,
                    gpsData.hour, gpsData.minute, gpsData.second);
      Serial.printf("  Unix: %lu\n", gpsGetUnixTime());
    }
    Serial.printf("  Last fix: %lu ms ago\n", millis() - gpsData.lastFixTime);
  }
  else if (cmd == "WIFITEST") {
    Serial.println("[CMD] Testing WiFi...");
    bool ok = wifiConnect();
    Serial.printf("  WiFi: %s\n", ok ? "CONNECTED" : "FAILED");
    if (ok) {
      Serial.printf("  IP: %s\n", WiFi.localIP().toString().c_str());
      Serial.printf("  RSSI: %d dBm\n", WiFi.RSSI());
    }
  }
  else if (cmd.startsWith("SETALERT:")) {
    int v = cmd.substring(9).toInt();
    if (v > 0 && v < warnLevelCm) { alertLevelCm = v; forceSaveEeprom();
      Serial.printf("ALERT_LEVEL_SET=%d\n", alertLevelCm); }
    else Serial.println("ERR: Invalid alert level.");
  }
  else if (cmd.startsWith("SETWARN:")) {
    int v = cmd.substring(8).toInt();
    if (v > alertLevelCm && v < dangerLevelCm) { warnLevelCm = v; forceSaveEeprom();
      Serial.printf("WARNING_LEVEL_SET=%d\n", warnLevelCm); }
    else Serial.println("ERR: Invalid warn level.");
  }
  else if (cmd.startsWith("SETDANGER:")) {
    int v = cmd.substring(10).toInt();
    if (v > warnLevelCm) { dangerLevelCm = v; forceSaveEeprom();
      Serial.printf("DANGER_LEVEL_SET=%d\n", dangerLevelCm); }
    else Serial.println("ERR: Invalid danger level.");
  }
  else if (cmd.startsWith("OLP:")) {
    float v = cmd.substring(4).toFloat();
    if (v > 10 && v < 2000) { olpLengthCm = v; forceSaveEeprom();
      Serial.printf("OLP_SET=%.1f cm\n", olpLengthCm); }
    else Serial.println("ERR: OLP out of range (10–2000 cm).");
  }
  else if (cmd == "ALGOON") {
    algorithmEnabled = true;
    digitalWrite(ALGO_LED, LOW);
    lastSampleTime = millis();
    updateAdaptiveIntervals();
    Serial.println("ALGORITHM:ENABLED");
  }
  else if (cmd == "ALGOOFF") {
    algorithmEnabled = false;
    digitalWrite(ALGO_LED, HIGH);
    Serial.println("ALGORITHM:DISABLED");
  }
  else if (cmd == "DUMP") {
    printCSV();
    Serial.printf("PEAK:%.1fcm  READINGS:%lu  TX_OK:%lu  TX_FAIL:%lu  STORED:%d\n",
                  peakHeightCm, readingsSinceBoot,
                  transmitSuccessCount, transmitFailCount, storedPayloadCount);
  }
  else {
    Serial.printf("ERR: Unknown command [%s]\n", cmd.c_str());
    Serial.println("Commands: STATUS|SENSORTEST|PING|BATT|CAL|ALGOON|ALGOOFF");
    Serial.println("          TXNOW|SIMTEST|GPSTEST|WIFITEST|DUMP|FLOODSTATUS");
    Serial.println("          SETALERT:<cm>|SETWARN:<cm>|SETDANGER:<cm>|OLP:<cm>");
    Serial.println("          GETCONFIG|GETTHRESH|SAVEEEPROM");
  }
}

// ─────────────────────────────────────────────────────────────────
//  ALGORITHM TOGGLE BUTTON  (GPIO 12)
// ─────────────────────────────────────────────────────────────────

void checkAlgorithmButton() {
  bool pressed = (digitalRead(ALGO_BTN) == LOW);
  if (pressed && (lastBtnState == HIGH) && (millis() - lastBtnPress > 250)) {
    lastBtnPress = millis();
    algorithmEnabled = !algorithmEnabled;
    digitalWrite(ALGO_LED, algorithmEnabled ? LOW : HIGH);
    Serial.printf("[BTN] Algorithm toggled: %s\n", algorithmEnabled ? "ENABLED" : "DISABLED");
    if (algorithmEnabled) {
      lastSampleTime = millis();
      updateAdaptiveIntervals();
    }
  }
  lastBtnState = pressed;
}

// ─────────────────────────────────────────────────────────────────
//  SETUP
// ─────────────────────────────────────────────────────────────────

void setup() {
  Serial.begin(115200);
  delay(500);
  Serial.println("\n\n===================================");
  Serial.println("  VARUNA — Flood Monitoring System  ");
  Serial.println("  ESP32-S3 Firmware v2.0 + COMMS    ");
  Serial.println("===================================\n");

  // ── 1. GPIO init ──────────────────────────────────────────────
  pinMode(STATUS_LED, OUTPUT);
  pinMode(ALGO_LED,   OUTPUT);
  pinMode(ALGO_BTN,   INPUT_PULLUP);
  digitalWrite(STATUS_LED, LOW);
  digitalWrite(ALGO_LED,   HIGH);  // LED ON = algorithm disabled

  // ── 2. Watchdog — extended timeout for init ───────────────────
  esp_task_wdt_deinit();  // remove default WDT if present
  esp_task_wdt_init(WDT_TIMEOUT_INIT_S, true);
  esp_task_wdt_add(NULL);

  // ── 3. I2C buses ─────────────────────────────────────────────
  I2C0.begin(I2C0_SDA, I2C0_SCL, 100000);
  I2C1.begin(I2C1_SDA, I2C1_SCL, 100000);
  Serial.println("[INIT] I2C buses started.");

  // ── 4. ADC ───────────────────────────────────────────────────
  analogReadResolution(12);
  updateBattery();
  Serial.printf("[INIT] Battery: %d%%\n", batteryPercent);

  // ── 5. EEPROM + warm boot ─────────────────────────────────────
  EEPROM.begin(EEPROM_SIZE);
  attemptWarmBoot();

  // ── 6. GPS init ──────────────────────────────────────────────
  gpsInit();
  Serial.println("[INIT] GPS UART started. Waiting for fix in background.");

  esp_task_wdt_reset();

  // ── 7. MPU6050 ───────────────────────────────────────────────
  mpuHealthy = mpuInit();
  if (mpuHealthy) {
    mpuCalibrate();
    Serial.println("[INIT] MPU6050 ready.");
  } else {
    Serial.println("[INIT] MPU6050 unavailable — tilt measurement disabled.");
  }

  esp_task_wdt_reset();

  // ── 8. BMP280 ─────────────────────────────────────────────────
  bmpAvailable = bmpInit();
  if (bmpAvailable) {
    bmpEstablishBaseline(10);
    Serial.println("[INIT] BMP280 ready.");
  } else {
    Serial.println("[INIT] BMP280 unavailable — pressure measurement disabled.");
  }

  esp_task_wdt_reset();

  // ── 9. SIM800L ────────────────────────────────────────────────
  Serial.println("[INIT] Initialising SIM800L...");
  simAvailable = simInit();
  if (simAvailable) {
    Serial.println("[INIT] SIM800L ready.");
    // Set SMS text mode for inbound diagnostic commands
    simSendATOK("AT+CMGF=1");
    // Delete old messages
    simSendATOK("AT+CMGD=1,4");
  } else {
    Serial.println("[INIT] SIM800L unavailable — GPRS disabled.");
  }

  esp_task_wdt_reset();

  // ── 10. Store-and-forward init ────────────────────────────────
  for (int i = 0; i < STORE_FORWARD_MAX; i++) {
    storedPayloads[i].occupied = false;
    storedPayloads[i].timestamp = 0;
    storedPayloads[i].data[0] = '\0';
  }
  storedPayloadCount = 0;

  // ── 11. Sensor source summary ─────────────────────────────────
  if (!mpuHealthy && !bmpAvailable) {
    Serial.println("[FAULT] SENSOR_NONE — no measurement possible. Health critical.");
    healthScore = 0;
  } else if (!mpuHealthy) {
    Serial.println("[WARN] MPU unavailable — only MODE 3 (submerged) detectable.");
  } else if (!bmpAvailable) {
    Serial.println("[WARN] BMP unavailable — MODE 3 undetectable. Modes 0-2 OK.");
  }

  // ── 12. Timing init ──────────────────────────────────────────
  lastSampleTime   = millis();
  lastTransmitTime = millis();
  lastBmpRead      = millis();
  lastBattRead     = millis();
  lastEepromSave   = millis();
  lastGpsRead      = millis();
  stateEntryTime   = millis() / 1000;

  // ── 13. Adaptive intervals ───────────────────────────────────
  updateAdaptiveIntervals();

  // ── 14. Watchdog — reduce to operational timeout ─────────────
  esp_task_wdt_deinit();
  esp_task_wdt_init(WDT_TIMEOUT_OP_S, true);
  esp_task_wdt_add(NULL);

  // ── 15. Ready ─────────────────────────────────────────────────
  digitalWrite(STATUS_LED, HIGH);
  Serial.println("\n[INIT] Setup complete. Entering operational loop.");
  Serial.println("[INIT] Algorithm is DISABLED by default (bench mode).");
  Serial.println("[INIT] Commands: STATUS | SENSORTEST | ALGOON | ALGOOFF | PING | CAL");
  Serial.println("[INIT] Comms: TXNOW | SIMTEST | GPSTEST | WIFITEST");
  Serial.println("[INIT] Thresholds: SETALERT:<cm> | SETWARN:<cm> | SETDANGER:<cm>");
  Serial.println("[INIT] Config: OLP:<cm> | GETCONFIG | GETTHRESH | SAVEEEPROM | DUMP\n");
}

// ─────────────────────────────────────────────────────────────────
//  LOOP
// ─────────────────────────────────────────────────────────────────

void loop() {
  uint32_t now = millis();

  // ── 1. Watchdog kick ─────────────────────────────────────────
  esp_task_wdt_reset();

  // ── 2. Algorithm toggle button ────────────────────────────────
  checkAlgorithmButton();

  // ── 3. Serial command processing ─────────────────────────────
  if (Serial.available()) {
    String cmd = Serial.readStringUntil('\n');
    processCommand(cmd);
  }

  // ── 4. GPS update (continuous — NMEA parsing) ─────────────────
  gpsUpdate();

  // Update bootUnixTime from GPS when first fix arrives
  if (gpsData.valid && bootUnixTime == 0) {
    uint32_t gpsTime = gpsGetUnixTime();
    if (gpsTime > 1700000000UL) {  // sanity: after Nov 2023
      bootUnixTime = gpsTime - (millis() / 1000);
      Serial.printf("[GPS] First fix — bootUnixTime set to %lu\n", bootUnixTime);
    }
  }

  // ── 5. ALWAYS: run measurement engine (Phase 2) ───────────────
  runMeasurementEngine();

  // ── 6. CSV output at 1 Hz (always, regardless of algo state) ──
  static uint32_t lastCsvTime = 0;
  if (now - lastCsvTime >= 1000) {
    lastCsvTime = now;
    printCSV();
  }

  // ── 7. Battery check every 60 s ──────────────────────────────
  if (now - lastBattRead >= 60000UL) {
    lastBattRead = now;
    updateBattery();
    updateHealthScore();
  }

  // ── 8. Check inbound SMS every 30 s (diagnostic channel) ─────
  static uint32_t lastSmsCheck = 0;
  if (simAvailable && (now - lastSmsCheck >= 30000UL)) {
    lastSmsCheck = now;
    simCheckInboundSMS();
  }

  // ── 9. ALGORITHM-GATED: flood state machine (Phase 3) ─────────
  if (algorithmEnabled) {
    if (now - lastSampleTime >= sampleInterval) {
      lastSampleTime = now;

      evaluateFloodStatus();
      updateAdaptiveIntervals();

      Serial.printf("[SAMPLE] H=%.1fcm  Mode=%s  Level=%s  Rate=%.1f  Sustained=%d  Health=%d\n",
                    waterHeightCm, modeStr(currentMode),
                    levelStr(currentResponseLevel), ratePer15Min,
                    (int)sustainedRise, healthScore);
    }

    // ── 10. Scheduled data transmission ─────────────────────────
    if (now - lastTransmitTime >= transmitInterval) {
      lastTransmitTime = now;
      transmitData();
    }

    // ── 11. Periodic EEPROM save — every 30 minutes ─────────────
    if (now - lastEepromSave >= 1800000UL) {
      forceSaveEeprom();
    }
  }

  // ── 12. Minimal delay to prevent tight spinning ──────────────
  delay(5);
}
