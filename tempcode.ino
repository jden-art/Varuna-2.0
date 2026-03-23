#include <Wire.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include <math.h>

// ─── WiFi Configuration ───
#define WIFI_SSID "YOUR_SSID"
#define WIFI_PASSWORD "YOUR_PASSWORD"

// ─── Firebase Configuration ───
#define FIREBASE_API_KEY "YOUR_API_KEY"
#define FIREBASE_AUTH_DOMAIN "YOUR_PROJECT.firebaseapp.com"
#define FIREBASE_DATABASE_URL "https://YOUR_PROJECT-default-rtdb.firebaseio.com"
#define FIREBASE_PROJECT_ID "YOUR_PROJECT_ID"
#define FIREBASE_STORAGE_BUCKET "YOUR_PROJECT.appspot.com"
#define FIREBASE_MESSAGING_SENDER_ID "YOUR_SENDER_ID"
#define FIREBASE_APP_ID "YOUR_APP_ID"

#define FIREBASE_STATION_PATH "/stations/station_01"

// ─── Pin Definitions ───
#define SDA_BUS0 8
#define SCL_BUS0 9
#define SDA_BUS1 4
#define SCL_BUS1 5

#define UART1_RX 7
#define UART1_TX 6

#define BATTERY_ADC 2
#define ALGO_BUTTON 12
#define ALGO_LED 13
#define STATUS_LED 3

// ─── I2C Addresses ───
#define MPU6050_ADDR 0x68
#define BMP280_ADDR 0x76
#define DS1307_ADDR 0x68

// ─── Physical Constants ───
#define GRAVITY 9.80665f
#define WATER_DENSITY 1000.0f
#define PA_PER_METER (WATER_DENSITY * GRAVITY)

// ─── OLP Configuration ───
#define OLP_LENGTH 2.0f

// ─── Mode Detection Thresholds ───
#define LATERAL_ACCEL_TAUT_ENTER 0.15f
#define LATERAL_ACCEL_TAUT_EXIT 0.10f
#define TILT_TAUT_ENTER_DEG 3.0f
#define TILT_TAUT_EXIT_DEG 2.0f
#define FLOOD_THETA_DEG 10.0f
#define FLOOD_H_RATIO 0.95f
#define SUBMERSION_PRESSURE_PA 500.0f

// ─── Filter Constants ───
#define COMPLEMENTARY_ALPHA 0.97f
#define ACCEL_LPF_ALPHA 0.05f
#define PRESSURE_LPF_ALPHA 0.02f
#define LATERAL_LPF_ALPHA 0.1f

// ─── Timing ───
#define SENSOR_READ_INTERVAL_MS 20
#define MODE_EVAL_INTERVAL_MS 100
#define REPORT_INTERVAL_MS 5000
#define PRESSURE_BASELINE_INTERVAL_MS 600000
#define GPS_READ_INTERVAL_MS 2000
#define WIFI_RECONNECT_INTERVAL_MS 30000
#define FIREBASE_RETRY_INTERVAL_MS 15000

// ─── BMP280 Calibration ───
struct BMP280Calib {
    uint16_t dig_T1;
    int16_t dig_T2, dig_T3;
    uint16_t dig_P1;
    int16_t dig_P2, dig_P3, dig_P4, dig_P5, dig_P6, dig_P7, dig_P8, dig_P9;
    int32_t t_fine;
};

// ─── Operating Modes ───
enum FloodMode : uint8_t {
    MODE_SLACK = 0,
    MODE_TAUT = 1,
    MODE_FLOOD = 2,
    MODE_SUBMERGED = 3
};

struct IMUData {
    float ax, ay, az;
    float gx, gy, gz;
    float ax_filt, ay_filt, az_filt;
    float lateral_accel;
    float lateral_accel_filt;
    float pitch_accel;
    float roll_accel;
    float pitch_fused;
    float roll_fused;
    float theta;
};

struct PressureData {
    float pressure_raw;
    float pressure_filtered;
    float temperature;
    float baseline_atmospheric;
    float gauge_pressure;
    float depth;
    bool baseline_valid;
    uint32_t last_baseline_update;
    float baseline_accumulator;
    int baseline_sample_count;
};

struct SystemState {
    FloodMode current_mode;
    FloodMode previous_mode;
    float water_level_h;
    float flood_ratio;
    float tether_length;
    float battery_voltage;
    bool tether_was_taut;
    uint32_t mode_entry_time;
    uint32_t last_sensor_read;
    uint32_t last_mode_eval;
    uint32_t last_report;
    uint32_t last_gps_read;
    uint32_t last_wifi_attempt;
    bool algo_enabled;
    bool mpu_ok;
    bool bmp_ok;
    bool gps_ok;
    bool wifi_connected;
    float latitude;
    float longitude;
    uint8_t hour, minute, second;
    uint8_t day, month;
    uint16_t year;
    uint32_t boot_time;
    uint32_t successful_uploads;
    uint32_t failed_uploads;
};

// ─── Globals ───
TwoWire I2C_BUS0 = TwoWire(0);
TwoWire I2C_BUS1 = TwoWire(1);

IMUData imu;
PressureData press;
BMP280Calib bmp_cal;
SystemState sys;

char gps_buffer[128];
uint8_t gps_buf_idx = 0;

// ─── Forward Declarations ───
void initMPU6050();
void initBMP280();
void readMPU6050();
void readBMP280();
void updateSensorFusion(float dt);
void evaluateMode();
float computeWaterLevel();
void connectWiFi();
void sendToFirebase();
void readGPS();
void parseGNRMC(char* sentence);
void readRTC();
void updatePressureBaseline();
float readBattery();
void i2cWriteByte(TwoWire &bus, uint8_t addr, uint8_t reg, uint8_t val);
uint8_t i2cReadByte(TwoWire &bus, uint8_t addr, uint8_t reg);
void i2cReadBytes(TwoWire &bus, uint8_t addr, uint8_t reg, uint8_t *buf, uint8_t len);

// ═══════════════════════════════════════════════════════
// SETUP
// ═══════════════════════════════════════════════════════
void setup() {
    Serial.begin(115200);

    pinMode(ALGO_BUTTON, INPUT_PULLUP);
    pinMode(ALGO_LED, OUTPUT);
    pinMode(STATUS_LED, OUTPUT);
    pinMode(BATTERY_ADC, INPUT);

    digitalWrite(STATUS_LED, HIGH);
    digitalWrite(ALGO_LED, LOW);

    I2C_BUS0.begin(SDA_BUS0, SCL_BUS0, 400000);
    I2C_BUS1.begin(SDA_BUS1, SCL_BUS1, 400000);

    Serial1.begin(9600, SERIAL_8N1, UART1_RX, UART1_TX);

    memset(&imu, 0, sizeof(imu));
    memset(&press, 0, sizeof(press));
    memset(&sys, 0, sizeof(sys));

    sys.tether_length = OLP_LENGTH;
    sys.current_mode = MODE_SLACK;
    sys.previous_mode = MODE_SLACK;
    sys.algo_enabled = true;
    sys.tether_was_taut = false;
    sys.boot_time = millis();

    initMPU6050();
    initBMP280();

    delay(500);

    for (int i = 0; i < 100; i++) {
        readBMP280();
        delay(20);
    }
    press.baseline_atmospheric = press.pressure_filtered;
    press.baseline_valid = true;
    press.last_baseline_update = millis();

    for (int i = 0; i < 200; i++) {
        readMPU6050();
        float dt = SENSOR_READ_INTERVAL_MS / 1000.0f;
        updateSensorFusion(dt);
        delay(SENSOR_READ_INTERVAL_MS);
    }

    connectWiFi();

    sys.last_sensor_read = millis();
    sys.last_mode_eval = millis();
    sys.last_report = millis();
    sys.last_gps_read = millis();

    digitalWrite(STATUS_LED, LOW);
}

// ═══════════════════════════════════════════════════════
// MAIN LOOP
// ═══════════════════════════════════════════════════════
void loop() {
    uint32_t now = millis();

    static bool last_button = HIGH;
    bool btn = digitalRead(ALGO_BUTTON);
    if (last_button == HIGH && btn == LOW) {
        sys.algo_enabled = !sys.algo_enabled;
        digitalWrite(ALGO_LED, sys.algo_enabled ? HIGH : LOW);
    }
    last_button = btn;

    if (now - sys.last_sensor_read >= SENSOR_READ_INTERVAL_MS) {
        float dt = (now - sys.last_sensor_read) / 1000.0f;
        sys.last_sensor_read = now;
        readMPU6050();
        readBMP280();
        updateSensorFusion(dt);
    }

    if (now - sys.last_mode_eval >= MODE_EVAL_INTERVAL_MS) {
        sys.last_mode_eval = now;
        updatePressureBaseline();
        if (sys.algo_enabled) {
            evaluateMode();
            sys.water_level_h = computeWaterLevel();
            sys.flood_ratio = (sys.water_level_h > 0) ?
                              sys.water_level_h / sys.tether_length : 0.0f;
        }
    }

    if (now - sys.last_gps_read >= GPS_READ_INTERVAL_MS) {
        sys.last_gps_read = now;
        readGPS();
    }

    if (!sys.wifi_connected && (now - sys.last_wifi_attempt >= WIFI_RECONNECT_INTERVAL_MS)) {
        connectWiFi();
    }

    if (now - sys.last_report >= REPORT_INTERVAL_MS) {
        sys.last_report = now;
        sys.battery_voltage = readBattery();
        readRTC();
        if (sys.wifi_connected) {
            sendToFirebase();
        }
    }
}

// ═══════════════════════════════════════════════════════
// WiFi
// ═══════════════════════════════════════════════════════
void connectWiFi() {
    sys.last_wifi_attempt = millis();
    sys.wifi_connected = false;

    WiFi.disconnect(true);
    delay(100);
    WiFi.mode(WIFI_STA);
    WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

    uint32_t start = millis();
    while (WiFi.status() != WL_CONNECTED && (millis() - start < 15000)) {
        delay(250);
        digitalWrite(STATUS_LED, !digitalRead(STATUS_LED));
    }

    if (WiFi.status() == WL_CONNECTED) {
        sys.wifi_connected = true;
        digitalWrite(STATUS_LED, LOW);
    } else {
        digitalWrite(STATUS_LED, HIGH);
    }
}

// ═══════════════════════════════════════════════════════
// FIREBASE RTDB — REST API via HTTPS PUT
// ═══════════════════════════════════════════════════════
void sendToFirebase() {
    if (WiFi.status() != WL_CONNECTED) {
        sys.wifi_connected = false;
        return;
    }

    const char* mode_str;
    switch (sys.current_mode) {
        case MODE_SLACK:     mode_str = "SLACK"; break;
        case MODE_TAUT:      mode_str = "TAUT"; break;
        case MODE_FLOOD:     mode_str = "FLOOD"; break;
        case MODE_SUBMERGED: mode_str = "SUBMERGED"; break;
        default:             mode_str = "UNKNOWN"; break;
    }

    float theta_deg = imu.theta * (180.0f / M_PI);
    uint32_t uptime_sec = (millis() - sys.boot_time) / 1000;

    char timestamp[32];
    snprintf(timestamp, sizeof(timestamp), "%04d-%02d-%02dT%02d:%02d:%02d",
             sys.year, sys.month, sys.day, sys.hour, sys.minute, sys.second);

    char json[1024];
    snprintf(json, sizeof(json),
        "{"
            "\"mode\":%d,"
            "\"mode_str\":\"%s\","
            "\"water_level\":%.3f,"
            "\"flood_ratio\":%.3f,"
            "\"theta_deg\":%.2f,"
            "\"lateral_accel\":%.3f,"
            "\"pressure_pa\":%.1f,"
            "\"gauge_pa\":%.1f,"
            "\"depth_m\":%.3f,"
            "\"temperature\":%.1f,"
            "\"battery_v\":%.2f,"
            "\"latitude\":%.6f,"
            "\"longitude\":%.6f,"
            "\"timestamp\":\"%s\","
            "\"tether_length\":%.2f,"
            "\"uptime_s\":%lu,"
            "\"sensors\":{"
                "\"mpu\":%s,"
                "\"bmp\":%s,"
                "\"gps\":%s"
            "},"
            "\"uploads_ok\":%lu,"
            "\"uploads_fail\":%lu"
        "}",
        sys.current_mode, mode_str,
        sys.water_level_h,
        sys.flood_ratio,
        theta_deg,
        imu.lateral_accel,
        press.pressure_filtered,
        press.gauge_pressure,
        press.depth,
        press.temperature,
        sys.battery_voltage,
        sys.latitude, sys.longitude,
        timestamp,
        sys.tether_length,
        uptime_sec,
        sys.mpu_ok ? "true" : "false",
        sys.bmp_ok ? "true" : "false",
        sys.gps_ok ? "true" : "false",
        sys.successful_uploads,
        sys.failed_uploads);

    // ── PUT to /stations/station_01/latest.json ──
    char url_latest[256];
    snprintf(url_latest, sizeof(url_latest),
             "%s%s/latest.json?auth=%s",
             FIREBASE_DATABASE_URL, FIREBASE_STATION_PATH, FIREBASE_API_KEY);

    HTTPClient http;
    http.begin(url_latest);
    http.addHeader("Content-Type", "application/json");
    int code = http.PUT(json);
    http.end();

    bool latest_ok = (code == 200);

    // ── POST to /stations/station_01/history.json ──
    char url_history[256];
    snprintf(url_history, sizeof(url_history),
             "%s%s/history.json?auth=%s",
             FIREBASE_DATABASE_URL, FIREBASE_STATION_PATH, FIREBASE_API_KEY);

    http.begin(url_history);
    http.addHeader("Content-Type", "application/json");
    code = http.POST(json);
    http.end();

    bool history_ok = (code == 200);

    // ── POST alert node on mode transition ──
    if (sys.current_mode != sys.previous_mode &&
        (sys.current_mode == MODE_FLOOD || sys.current_mode == MODE_SUBMERGED)) {

        char alert_json[512];
        snprintf(alert_json, sizeof(alert_json),
            "{"
                "\"mode\":%d,"
                "\"mode_str\":\"%s\","
                "\"water_level\":%.3f,"
                "\"flood_ratio\":%.3f,"
                "\"timestamp\":\"%s\","
                "\"latitude\":%.6f,"
                "\"longitude\":%.6f,"
                "\"acknowledged\":false"
            "}",
            sys.current_mode, mode_str,
            sys.water_level_h,
            sys.flood_ratio,
            timestamp,
            sys.latitude, sys.longitude);

        char url_alert[256];
        snprintf(url_alert, sizeof(url_alert),
                 "%s%s/alerts.json?auth=%s",
                 FIREBASE_DATABASE_URL, FIREBASE_STATION_PATH, FIREBASE_API_KEY);

        http.begin(url_alert);
        http.addHeader("Content-Type", "application/json");
        http.POST(alert_json);
        http.end();

        sys.previous_mode = sys.current_mode;
    }

    if (latest_ok && history_ok) {
        sys.successful_uploads++;
        digitalWrite(STATUS_LED, LOW);
    } else {
        sys.failed_uploads++;
        digitalWrite(STATUS_LED, HIGH);
    }
}

// ═══════════════════════════════════════════════════════
// MPU6050
// ═══════════════════════════════════════════════════════
void initMPU6050() {
    i2cWriteByte(I2C_BUS0, MPU6050_ADDR, 0x6B, 0x00);
    delay(50);
    i2cWriteByte(I2C_BUS0, MPU6050_ADDR, 0x6B, 0x01);
    i2cWriteByte(I2C_BUS0, MPU6050_ADDR, 0x1A, 0x03);
    i2cWriteByte(I2C_BUS0, MPU6050_ADDR, 0x1B, 0x00);
    i2cWriteByte(I2C_BUS0, MPU6050_ADDR, 0x1C, 0x00);
    i2cWriteByte(I2C_BUS0, MPU6050_ADDR, 0x19, 0x04);
    delay(100);

    uint8_t who = i2cReadByte(I2C_BUS0, MPU6050_ADDR, 0x75);
    sys.mpu_ok = (who == 0x68 || who == 0x98);
}

void readMPU6050() {
    if (!sys.mpu_ok) return;

    uint8_t buf[14];
    i2cReadBytes(I2C_BUS0, MPU6050_ADDR, 0x3B, buf, 14);

    int16_t raw_ax = (buf[0] << 8) | buf[1];
    int16_t raw_ay = (buf[2] << 8) | buf[3];
    int16_t raw_az = (buf[4] << 8) | buf[5];
    int16_t raw_gx = (buf[8] << 8) | buf[9];
    int16_t raw_gy = (buf[10] << 8) | buf[11];
    int16_t raw_gz = (buf[12] << 8) | buf[13];

    imu.ax = raw_ax / 16384.0f * GRAVITY;
    imu.ay = raw_ay / 16384.0f * GRAVITY;
    imu.az = raw_az / 16384.0f * GRAVITY;

    imu.gx = raw_gx / 131.0f;
    imu.gy = raw_gy / 131.0f;
    imu.gz = raw_gz / 131.0f;

    imu.ax_filt += ACCEL_LPF_ALPHA * (imu.ax - imu.ax_filt);
    imu.ay_filt += ACCEL_LPF_ALPHA * (imu.ay - imu.ay_filt);
    imu.az_filt += ACCEL_LPF_ALPHA * (imu.az - imu.az_filt);

    float lat = sqrtf(imu.ax_filt * imu.ax_filt + imu.ay_filt * imu.ay_filt);
    imu.lateral_accel_filt += LATERAL_LPF_ALPHA * (lat - imu.lateral_accel_filt);
    imu.lateral_accel = imu.lateral_accel_filt;
}

// ═══════════════════════════════════════════════════════
// SENSOR FUSION
// ═══════════════════════════════════════════════════════
void updateSensorFusion(float dt) {
    if (!sys.mpu_ok) return;
    if (dt <= 0.0f || dt > 0.5f) dt = 0.02f;

    imu.pitch_accel = atan2f(imu.ax_filt,
                             sqrtf(imu.ay_filt * imu.ay_filt + imu.az_filt * imu.az_filt));
    imu.roll_accel = atan2f(imu.ay_filt,
                            sqrtf(imu.ax_filt * imu.ax_filt + imu.az_filt * imu.az_filt));

    float pitch_gyro = imu.pitch_fused + imu.gy * dt * (M_PI / 180.0f);
    float roll_gyro = imu.roll_fused + imu.gx * dt * (M_PI / 180.0f);

    imu.pitch_fused = COMPLEMENTARY_ALPHA * pitch_gyro +
                      (1.0f - COMPLEMENTARY_ALPHA) * imu.pitch_accel;
    imu.roll_fused = COMPLEMENTARY_ALPHA * roll_gyro +
                     (1.0f - COMPLEMENTARY_ALPHA) * imu.roll_accel;

    imu.theta = sqrtf(imu.pitch_fused * imu.pitch_fused +
                      imu.roll_fused * imu.roll_fused);
}

// ═══════════════════════════════════════════════════════
// BMP280
// ═══════════════════════════════════════════════════════
void initBMP280() {
    uint8_t chip_id = i2cReadByte(I2C_BUS1, BMP280_ADDR, 0xD0);
    sys.bmp_ok = (chip_id == 0x58);
    if (!sys.bmp_ok) return;

    i2cWriteByte(I2C_BUS1, BMP280_ADDR, 0xE0, 0xB6);
    delay(100);

    uint8_t cal[26];
    i2cReadBytes(I2C_BUS1, BMP280_ADDR, 0x88, cal, 26);

    bmp_cal.dig_T1 = cal[0] | (cal[1] << 8);
    bmp_cal.dig_T2 = cal[2] | (cal[3] << 8);
    bmp_cal.dig_T3 = cal[4] | (cal[5] << 8);
    bmp_cal.dig_P1 = cal[6] | (cal[7] << 8);
    bmp_cal.dig_P2 = cal[8] | (cal[9] << 8);
    bmp_cal.dig_P3 = cal[10] | (cal[11] << 8);
    bmp_cal.dig_P4 = cal[12] | (cal[13] << 8);
    bmp_cal.dig_P5 = cal[14] | (cal[15] << 8);
    bmp_cal.dig_P6 = cal[16] | (cal[17] << 8);
    bmp_cal.dig_P7 = cal[18] | (cal[19] << 8);
    bmp_cal.dig_P8 = cal[20] | (cal[21] << 8);
    bmp_cal.dig_P9 = cal[22] | (cal[23] << 8);

    i2cWriteByte(I2C_BUS1, BMP280_ADDR, 0xF5, 0x00);
    i2cWriteByte(I2C_BUS1, BMP280_ADDR, 0xF4, 0xFF);
    delay(50);
}

void readBMP280() {
    if (!sys.bmp_ok) return;

    uint8_t buf[6];
    i2cReadBytes(I2C_BUS1, BMP280_ADDR, 0xF7, buf, 6);

    int32_t adc_P = ((int32_t)buf[0] << 12) | ((int32_t)buf[1] << 4) | (buf[2] >> 4);
    int32_t adc_T = ((int32_t)buf[3] << 12) | ((int32_t)buf[4] << 4) | (buf[5] >> 4);

    int32_t var1t = ((((adc_T >> 3) - ((int32_t)bmp_cal.dig_T1 << 1))) *
                     ((int32_t)bmp_cal.dig_T2)) >> 11;
    int32_t var2t = (((((adc_T >> 4) - ((int32_t)bmp_cal.dig_T1)) *
                       ((adc_T >> 4) - ((int32_t)bmp_cal.dig_T1))) >> 12) *
                     ((int32_t)bmp_cal.dig_T3)) >> 14;
    bmp_cal.t_fine = var1t + var2t;
    press.temperature = (bmp_cal.t_fine * 5 + 128) >> 8;
    press.temperature /= 100.0f;

    int64_t var1 = ((int64_t)bmp_cal.t_fine) - 128000;
    int64_t var2 = var1 * var1 * (int64_t)bmp_cal.dig_P6;
    var2 = var2 + ((var1 * (int64_t)bmp_cal.dig_P5) << 17);
    var2 = var2 + (((int64_t)bmp_cal.dig_P4) << 35);
    var1 = ((var1 * var1 * (int64_t)bmp_cal.dig_P3) >> 8) +
           ((var1 * (int64_t)bmp_cal.dig_P2) << 12);
    var1 = (((((int64_t)1) << 47) + var1)) * ((int64_t)bmp_cal.dig_P1) >> 33;

    if (var1 == 0) {
        press.pressure_raw = 0;
        return;
    }

    int64_t p = 1048576 - adc_P;
    p = (((p << 31) - var2) * 3125) / var1;
    var1 = (((int64_t)bmp_cal.dig_P9) * (p >> 13) * (p >> 13)) >> 25;
    var2 = (((int64_t)bmp_cal.dig_P8) * p) >> 19;
    p = ((p + var1 + var2) >> 8) + (((int64_t)bmp_cal.dig_P7) << 4);

    press.pressure_raw = (float)p / 256.0f;
    press.pressure_filtered += PRESSURE_LPF_ALPHA *
                               (press.pressure_raw - press.pressure_filtered);

    if (press.baseline_valid) {
        press.gauge_pressure = press.pressure_filtered - press.baseline_atmospheric;
        press.depth = press.gauge_pressure / PA_PER_METER;
        if (press.depth < 0.0f) press.depth = 0.0f;
    }
}

// ═══════════════════════════════════════════════════════
// PRESSURE BASELINE
// ═══════════════════════════════════════════════════════
void updatePressureBaseline() {
    if (!sys.bmp_ok || !press.baseline_valid) return;

    if (sys.current_mode == MODE_SLACK || sys.current_mode == MODE_TAUT) {
        if (press.gauge_pressure < SUBMERSION_PRESSURE_PA * 0.5f) {
            press.baseline_accumulator += press.pressure_filtered;
            press.baseline_sample_count++;

            if (millis() - press.last_baseline_update >= PRESSURE_BASELINE_INTERVAL_MS &&
                press.baseline_sample_count > 50) {
                float new_baseline = press.baseline_accumulator / press.baseline_sample_count;
                float drift = fabsf(new_baseline - press.baseline_atmospheric);
                if (drift < 2000.0f) {
                    press.baseline_atmospheric = new_baseline;
                }
                press.baseline_accumulator = 0;
                press.baseline_sample_count = 0;
                press.last_baseline_update = millis();
            }
        }
    } else {
        press.baseline_accumulator = 0;
        press.baseline_sample_count = 0;
    }
}

// ═══════════════════════════════════════════════════════
// MODE EVALUATION
// ═══════════════════════════════════════════════════════
void evaluateMode() {
    FloodMode new_mode = sys.current_mode;
    float theta_deg = imu.theta * (180.0f / M_PI);
    float L = sys.tether_length;

    bool pressure_submerged = sys.bmp_ok && press.baseline_valid &&
                              (press.gauge_pressure > SUBMERSION_PRESSURE_PA);

    bool pressure_surfaced = !sys.bmp_ok || !press.baseline_valid ||
                             (press.gauge_pressure < SUBMERSION_PRESSURE_PA * 0.7f);

    bool tether_taut;
    if (sys.tether_was_taut) {
        tether_taut = (imu.lateral_accel > LATERAL_ACCEL_TAUT_EXIT) ||
                      (theta_deg > TILT_TAUT_EXIT_DEG);
    } else {
        tether_taut = (imu.lateral_accel > LATERAL_ACCEL_TAUT_ENTER) &&
                      (theta_deg > TILT_TAUT_ENTER_DEG);
    }
    sys.tether_was_taut = tether_taut;

    float h_trig = L * cosf(imu.theta);

    switch (sys.current_mode) {
        case MODE_SLACK:
            if (pressure_submerged) {
                new_mode = MODE_SUBMERGED;
            } else if (tether_taut) {
                if (theta_deg < FLOOD_THETA_DEG && h_trig > FLOOD_H_RATIO * L) {
                    new_mode = MODE_FLOOD;
                } else {
                    new_mode = MODE_TAUT;
                }
            }
            break;

        case MODE_TAUT:
            if (pressure_submerged) {
                new_mode = MODE_SUBMERGED;
            } else if (theta_deg < FLOOD_THETA_DEG && h_trig > FLOOD_H_RATIO * L) {
                new_mode = MODE_FLOOD;
            } else if (!tether_taut) {
                new_mode = MODE_SLACK;
            }
            break;

        case MODE_FLOOD:
            if (pressure_submerged) {
                new_mode = MODE_SUBMERGED;
            } else if (theta_deg > FLOOD_THETA_DEG * 1.2f ||
                       h_trig < FLOOD_H_RATIO * L * 0.98f) {
                new_mode = MODE_TAUT;
            }
            break;

        case MODE_SUBMERGED:
            if (pressure_surfaced) {
                if (tether_taut) {
                    if (theta_deg < FLOOD_THETA_DEG && h_trig > FLOOD_H_RATIO * L) {
                        new_mode = MODE_FLOOD;
                    } else {
                        new_mode = MODE_TAUT;
                    }
                } else {
                    new_mode = MODE_SLACK;
                }
            }
            break;
    }

    if (new_mode != sys.current_mode) {
        sys.previous_mode = sys.current_mode;
        sys.current_mode = new_mode;
        sys.mode_entry_time = millis();
    }
}

// ═══════════════════════════════════════════════════════
// WATER LEVEL COMPUTATION
// ═══════════════════════════════════════════════════════
float computeWaterLevel() {
    float L = sys.tether_length;

    switch (sys.current_mode) {
        case MODE_SLACK:
            return -1.0f;

        case MODE_TAUT: {
            float h = L * cosf(imu.theta);
            if (h < 0.0f) h = 0.0f;
            if (h > L) h = L;
            return h;
        }

        case MODE_FLOOD:
            if (sys.bmp_ok && press.baseline_valid && press.depth > 0.01f) {
                float h_pressure = L + press.depth;
                float h_trig = L * cosf(imu.theta);
                float w = press.depth / 0.05f;
                if (w > 1.0f) w = 1.0f;
                return h_trig * (1.0f - w) + h_pressure * w;
            }
            return L * cosf(imu.theta);

        case MODE_SUBMERGED:
            if (sys.bmp_ok && press.baseline_valid) {
                return L + press.depth;
            }
            return L;

        default:
            return -1.0f;
    }
}

// ═══════════════════════════════════════════════════════
// GPS
// ═══════════════════════════════════════════════════════
void readGPS() {
    while (Serial1.available()) {
        char c = Serial1.read();
        if (c == '\n' || c == '\r') {
            if (gps_buf_idx > 6) {
                gps_buffer[gps_buf_idx] = '\0';
                if (strncmp(gps_buffer, "$GNRMC", 6) == 0 ||
                    strncmp(gps_buffer, "$GPRMC", 6) == 0) {
                    parseGNRMC(gps_buffer);
                }
            }
            gps_buf_idx = 0;
        } else if (gps_buf_idx < sizeof(gps_buffer) - 1) {
            gps_buffer[gps_buf_idx++] = c;
        }
    }
}

void parseGNRMC(char* sentence) {
    char* fields[15];
    int field_count = 0;
    char* p = sentence;

    fields[field_count++] = p;
    while (*p && field_count < 15) {
        if (*p == ',') {
            *p = '\0';
            fields[field_count++] = p + 1;
        }
        p++;
    }

    if (field_count < 10) return;
    if (fields[2][0] != 'A') return;

    sys.gps_ok = true;

    if (strlen(fields[1]) >= 6) {
        sys.hour = (fields[1][0] - '0') * 10 + (fields[1][1] - '0');
        sys.minute = (fields[1][2] - '0') * 10 + (fields[1][3] - '0');
        sys.second = (fields[1][4] - '0') * 10 + (fields[1][5] - '0');
    }

    if (strlen(fields[3]) >= 4) {
        float raw = atof(fields[3]);
        int degrees = (int)(raw / 100);
        float minutes = raw - degrees * 100;
        sys.latitude = degrees + minutes / 60.0f;
        if (fields[4][0] == 'S') sys.latitude = -sys.latitude;
    }

    if (strlen(fields[5]) >= 5) {
        float raw = atof(fields[5]);
        int degrees = (int)(raw / 100);
        float minutes = raw - degrees * 100;
        sys.longitude = degrees + minutes / 60.0f;
        if (fields[6][0] == 'W') sys.longitude = -sys.longitude;
    }

    if (field_count > 9 && strlen(fields[9]) >= 6) {
        sys.day = (fields[9][0] - '0') * 10 + (fields[9][1] - '0');
        sys.month = (fields[9][2] - '0') * 10 + (fields[9][3] - '0');
        sys.year = 2000 + (fields[9][4] - '0') * 10 + (fields[9][5] - '0');
    }
}

// ═══════════════════════════════════════════════════════
// DS1307 RTC
// ═══════════════════════════════════════════════════════
void readRTC() {
    uint8_t buf[7];
    i2cReadBytes(I2C_BUS1, DS1307_ADDR, 0x00, buf, 7);

    sys.second = ((buf[0] >> 4) & 0x07) * 10 + (buf[0] & 0x0F);
    sys.minute = ((buf[1] >> 4) & 0x07) * 10 + (buf[1] & 0x0F);
    sys.hour = ((buf[2] >> 4) & 0x03) * 10 + (buf[2] & 0x0F);
    sys.day = ((buf[4] >> 4) & 0x03) * 10 + (buf[4] & 0x0F);
    sys.month = ((buf[5] >> 4) & 0x01) * 10 + (buf[5] & 0x0F);
    sys.year = 2000 + ((buf[6] >> 4) & 0x0F) * 10 + (buf[6] & 0x0F);
}

// ═══════════════════════════════════════════════════════
// BATTERY
// ═══════════════════════════════════════════════════════
float readBattery() {
    uint32_t sum = 0;
    for (int i = 0; i < 16; i++) {
        sum += analogRead(BATTERY_ADC);
        delayMicroseconds(100);
    }
    float adc_avg = sum / 16.0f;
    return (adc_avg / 4095.0f) * 3.3f * 2.0f;
}

// ═══════════════════════════════════════════════════════
// I2C HELPERS
// ═══════════════════════════════════════════════════════
void i2cWriteByte(TwoWire &bus, uint8_t addr, uint8_t reg, uint8_t val) {
    bus.beginTransmission(addr);
    bus.write(reg);
    bus.write(val);
    bus.endTransmission();
}

uint8_t i2cReadByte(TwoWire &bus, uint8_t addr, uint8_t reg) {
    bus.beginTransmission(addr);
    bus.write(reg);
    bus.endTransmission(false);
    bus.requestFrom(addr, (uint8_t)1);
    return bus.available() ? bus.read() : 0;
}

void i2cReadBytes(TwoWire &bus, uint8_t addr, uint8_t reg, uint8_t *buf, uint8_t len) {
    bus.beginTransmission(addr);
    bus.write(reg);
    bus.endTransmission(false);
    bus.requestFrom(addr, len);
    for (uint8_t i = 0; i < len && bus.available(); i++) {
        buf[i] = bus.read();
    }
}
