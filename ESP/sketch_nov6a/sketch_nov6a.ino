/**
 * ============================================================================
 * SMART HATCHERY MONITORING - FIRMWARE ESP32 DOIT DEVKIT V1 (30-PIN)
 * (Revisi Final: Tanpa Motor Stepper, 1 Servo MG90S, & 1 Push Button Start)
 * ============================================================================
 * Mapping Pin Hardware (Sesuai Dokumen Spesifikasi Final):
 *   - GPIO 18 : Signal Motor Servo MG90S (1 Unit) -> Catu daya +5V TB-1512
 *   - GPIO 19 : Positif (+) Buzzer Aktif SFM-27
 *   - GPIO 21 : Jalur SDA I2C-1 (SHT30 [0x44], OLED-1 SSD1306 [0x3C], RTC DS3231 [0x68])
 *   - GPIO 22 : Jalur SCL I2C-1 (SHT30 [0x44], OLED-1 SSD1306 [0x3C], RTC DS3231 [0x68])
 *   - GPIO 25 : PWM / DIM Trigger RobotDyn AC Dimmer (Bohlam 60W)
 *   - GPIO 26 : Push Button Start - 1 Unit (INPUT_PULLUP, aktif LOW)
 *   - GPIO 32 : Jalur SDA I2C-2 Khusus OLED-2 SSD1306 [0x3C] (Cara 1 - Tanpa Solder)
 *   - GPIO 33 : Jalur SCL I2C-2 Khusus OLED-2 SSD1306 [0x3C] (Cara 1 - Tanpa Solder)
 *   - GPIO 34 : Z-C (Zero-Cross Detector) RobotDyn AC Dimmer (Input Interrupt)
 *
 * Library yang WAJIB Diinstal di Arduino IDE (Library Manager):
 *   1. ArduinoJson            (oleh Benoit Blanchon, versi 7.x)
 *   2. Adafruit SHT31 Library (oleh Adafruit - kompatibel penuh dengan SHT30/SHT31)
 *   3. Adafruit SSD1306       (oleh Adafruit) + Adafruit GFX Library
 *   4. RTClib                 (oleh Adafruit - untuk RTC DS3231SN)
 *   5. ESP32Servo             (oleh Kevin Harrington / John K. Bennett)
 * ============================================================================
 */

#include <Arduino.h>
#include <esp_arduino_version.h>
#include <soc/gpio_struct.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <HTTPClient.h>
#include <Preferences.h>
#include <Wire.h>
#include <time.h>
#include <ArduinoJson.h>
#include <Adafruit_SHT31.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <RTClib.h>
#include <ESP32Servo.h>

// ============================================================================
// 1. DEFINISI PIN GPIO ESP32 DOIT (30-PIN) - SESUAI MAPPING REVISI FINAL
// ============================================================================
#define PIN_SERVO_MG90S     18  // Signal Motor Servo MG90S (1 Unit)
#define PIN_BUZZER_SFM27    19  // Positif (+) Buzzer Aktif SFM-27
#define PIN_I2C_SDA         21  // SDA Bus I2C-1 (SHT30, OLED-1, RTC)
#define PIN_I2C_SCL         22  // SCL Bus I2C-1 (SHT30, OLED-1, RTC)
#define PIN_DIMMER_DIM      25  // Output Trigger Gate Triac RobotDyn AC Dimmer
#define PIN_BTN_START       26  // Push Button Start (INPUT_PULLUP)
#define PIN_I2C2_SDA        32  // SDA Bus I2C-2 Khusus OLED-2 (Cara 1 - Tanpa Solder)
#define PIN_I2C2_SCL        33  // SCL Bus I2C-2 Khusus OLED-2 (Cara 1 - Tanpa Solder)
#define PIN_DIMMER_ZC       34  // Input Zero-Cross Detector RobotDyn AC Dimmer

// Alamat I2C Perangkat (Agar Tidak Bentrok)
#define I2C_ADDR_SHT30      0x44
#define I2C_ADDR_OLED       0x3C
#define I2C_ADDR_RTC        0x68

// Dimensi Layar OLED 0.96" SSD1306
#define OLED_WIDTH          128
#define OLED_HEIGHT         64
#define OLED_RESET          -1

// ============================================================================
// 2. KONFIGURASI JARINGAN, BACKEND GO FIBER, & IDENTITAS ALAT
// ============================================================================
const char* DEVICE_ID             = "inkubator-01";
const char* DEFAULT_WIFI_SSID     = "Hoho";
const char* DEFAULT_WIFI_PASS     = "0987654321";

// URL Backend Utama (Custom Domain HTTPS) & Fallback Otomatis (Tailscale VPS "nandoz": 100.83.29.119)
const char* BACKEND_TELEMETRY_URL = "https://itebaincubatortelur.cc.cd/api/device/telemetry";
const char* BACKEND_FALLBACK_URL  = "http://100.83.29.119:3000/api/device/telemetry";
const char* DEVICE_API_KEY        = "secret-device-token-esp32";

const char* NTP_SERVER_1          = "pool.ntp.org";
const char* NTP_SERVER_2          = "id.pool.ntp.org";

// ============================================================================
// 3. BATAS KEAMANAN & KONSTANTA SISTEM (PRD V2.0 & KONTRAK API V2.1)
// ============================================================================
const float TEMP_SAFE_MIN             = 36.5f;   // Batas aman bawah (°C)
const float TEMP_SAFE_MAX             = 38.5f;   // Batas aman atas (°C)
const float TEMP_DEFAULT_TARGET       = 37.5f;   // Target suhu optimal default (°C)
const float HUMIDITY_WARN_MIN         = 45.0f;   // Batas pengaman kelembapan bawah (%RH)
const float HUMIDITY_WARN_MAX         = 75.0f;   // Batas pengaman kelembapan atas (%RH)

const int   LAMP_MANUAL_MAX_PCT       = 80;      // Batas aman bohlam 60W saat manual (80%)
const int   LAMP_AUTO_MAX_PCT         = 100;     // Batas bohlam saat Fuzzy Auto (100%)
const int   VENT_ANGLE_MIN            = 0;       // Servo tertutup penuh (0°)
const int   VENT_ANGLE_MAX            = 90;      // Servo terbuka penuh (90°)

const int   MOTOR_INTERVAL_MIN        = 30;      // Interval jadwal putar min (menit)
const int   MOTOR_INTERVAL_MAX        = 480;     // Interval jadwal putar max (menit)
const int   MOTOR_DURATION_MIN        = 5;       // Durasi putar min (detik)
const int   MOTOR_DURATION_MAX        = 60;      // Durasi putar max (detik)
const int   MOTOR_SPEED_MIN           = 10;      // Kecepatan min (%)
const int   MOTOR_SPEED_MAX           = 70;      // Kecepatan max (%)

const int   INCUBATION_TOTAL_DAYS     = 21;      // Siklus penuh 21 hari
const int   INCUBATION_LOCKDOWN_START = 19;      // Hari 19-21 masuk fase Lockdown

const unsigned long TELEMETRY_INTERVAL_MS   = 5000UL;  // Kirim telemetri tiap 5 detik
const unsigned long SENSOR_READ_INTERVAL_MS = 2000UL;  // Baca sensor & hitung Fuzzy tiap 2 detik
const unsigned long OLED_UPDATE_INTERVAL_MS = 1000UL;  // Refresh layar OLED tiap 1 detik
const unsigned long WIFI_CONNECT_TIMEOUT_MS = 30000UL; // Timeout uji Wi-Fi baru 30 detik

// ============================================================================
// 4. OBJEK PERANGKAT KERAS
// ============================================================================
Adafruit_SHT31    sht30 = Adafruit_SHT31();
Adafruit_SSD1306  oled(OLED_WIDTH, OLED_HEIGHT, &Wire, OLED_RESET);   // OLED-1 di Bus I2C-1 (GPIO 21/22)
Adafruit_SSD1306  oled2(OLED_WIDTH, OLED_HEIGHT, &Wire1, OLED_RESET); // OLED-2 di Bus I2C-2 (GPIO 32/33)
RTC_DS3231        rtc;
Servo             mg90sServo;
Preferences       prefs;

// ============================================================================
// 5. DRIVER ROBOTDYN AC DIMMER (ZERO-CROSS GPIO 34 + TICK 100us GPIO 25)
// ============================================================================
// Listrik AC 50Hz memiliki setengah gelombang = 10.000 us (100 tick @ 100 us).
// Menggunakan timer periodik tetap 100 us + tulis register langsung (IRAM-safe)
// sehingga stabil di ESP32 Core 2.x maupun 3.x tanpa risiko crash ISR.
hw_timer_t*  dimmerTimer       = NULL;
volatile int acDimmerIntensity = 0;   // 0 - 100 (%)
volatile int zcTickCount       = 0;   // Penghitung tick sejak Zero-Cross terakhir
volatile int dimTargetTick     = 92;  // Tick penyulutan Triac (8 = terang, 92 = redup)
volatile bool dimmerZCPaused   = false;

void IRAM_ATTR onDimmerTickISR() {
  if (dimmerZCPaused) return;

  int pct = acDimmerIntensity;
  if (pct <= 2 || pct >= 98) return;

  int tick = zcTickCount;
  if (tick < 105) {
    tick++;
    zcTickCount = tick;
  }

  int target = dimTargetTick;
  if (tick == target) {
    GPIO.out_w1ts = (1UL << PIN_DIMMER_DIM); // Trigger gate Triac HIGH
  } else if (tick == target + 2) {
    GPIO.out_w1tc = (1UL << PIN_DIMMER_DIM); // Lepas trigger setelah pulsa 200 us
  }
}

void IRAM_ATTR onZeroCrossISR() {
  if (dimmerZCPaused) return;

  zcTickCount = 0;
  int pct = acDimmerIntensity;
  if (pct >= 98) {
    GPIO.out_w1ts = (1UL << PIN_DIMMER_DIM); // Nyala penuh
  } else {
    GPIO.out_w1tc = (1UL << PIN_DIMMER_DIM); // Mati saat awal setengah siklus
  }
}

void initRobotDynDimmer() {
  pinMode(PIN_DIMMER_DIM, OUTPUT);
  GPIO.out_w1tc = (1UL << PIN_DIMMER_DIM);
  pinMode(PIN_DIMMER_ZC, INPUT);

#if ESP_ARDUINO_VERSION >= ESP_ARDUINO_VERSION_VAL(3, 0, 0)
  dimmerTimer = timerBegin(1000000); // 1 MHz (1 tick = 1 us)
  timerAttachInterrupt(dimmerTimer, &onDimmerTickISR);
  timerAlarm(dimmerTimer, 100, true, 0); // Auto-reload tiap 100 us
#else
  dimmerTimer = timerBegin(0, 80, true); // 80 MHz / 80 = 1 MHz
  timerAttachInterrupt(dimmerTimer, &onDimmerTickISR, true);
  timerAlarmWrite(dimmerTimer, 100, true);
  timerAlarmEnable(dimmerTimer);
#endif

  attachInterrupt(digitalPinToInterrupt(PIN_DIMMER_ZC), onZeroCrossISR, RISING);
}

// ============================================================================
// 6. STATE OPERASIONAL INKUBATOR
// ============================================================================
struct IncubatorState {
  // Status Perangkat I2C
  bool  sht30Available    = false;
  bool  oledAvailable     = false;
  bool  oled2Available    = false;
  bool  rtcAvailable      = false;

  // Sensor & Target
  float temperature       = 37.5f;
  float prevTemperature   = 37.5f;
  float targetTemperature = TEMP_DEFAULT_TARGET;
  float humidity          = 58.0f;
  int   sensorFailStreak  = 0;
  bool  sensorError       = false;

  // Siklus Inkubasi 21 Hari
  String incubationStatus = "idle"; // "idle" | "running" | "finished"
  int    currentDay       = 0;      // 1..21 (0 jika idle)
  time_t startedAtEpoch   = 0;
  time_t finishAtEpoch    = 0;
  String stage            = "";     // "turning" | "lockdown" | ""

  // Lampu Pemanas (RobotDyn AC Dimmer - Bohlam 60W)
  String lampMode         = "auto"; // "auto" | "manual"
  int    lampIntensity    = 0;      // 0..100 (auto), 0..80 (manual)
  bool   lampFuzzyActive  = true;

  // Ventilasi (Motor Servo MG90S GPIO 18)
  String ventMode         = "auto"; // "auto" | "manual"
  int    ventAngle        = 0;      // 0..90 derajat
  bool   ventFuzzyActive  = true;

  // Status Jadwal Pembalikan (Kompatibilitas Kontrak API v2.1)
  bool          motorRunning          = false;
  bool          encoderOk             = true;
  String        turningStatus         = "idle"; // "idle" | "turning" | "lockdown" | "fault"
  time_t        lastTurnedEpoch       = 0;
  time_t        nextTurningEpoch      = 0;
  unsigned long motorTurnStartMs      = 0;

  // Parameter Konfigurasi Jadwal Putar
  int motorIntervalMinutes = 120;
  int motorDurationSeconds = 20;
  int motorSpeedPercent    = 50;

  // Kredensial Wi-Fi Aktif
  String wifiSsid          = DEFAULT_WIFI_SSID;
  String wifiPass          = DEFAULT_WIFI_PASS;

  // Alert Komunikasi (setelah gagal ganti Wi-Fi & rollback)
  bool   commAlertPending  = false;
  String commAlertMessage  = "";

  // Status Koneksi ke Server Web (untuk ditampilkan di OLED)
  bool   backendChecked    = false;
  bool   backendConnected  = false;

  // Pelacakan Perintah Terakhir (last_command)
  long   lastCmdId         = 0;
  String lastCmdTarget     = "";
  String lastCmdAction     = "";
  String lastCmdStatus     = ""; // "executed" | "failed"
  time_t lastCmdUpdatedAt  = 0;
} state;

unsigned long lastSensorMillis    = 0;
unsigned long lastTelemetryMillis = 0;
unsigned long lastOledMillis      = 0;

// Variabel Debounce Push Button Start (GPIO 26)
bool          lastBtnReading      = HIGH;
bool          stableBtnState      = HIGH;
unsigned long lastBtnDebounceMs   = 0;
unsigned long btnPressStartMs     = 0;
bool          btnLongPressHandled = false;

// Variabel Pola Buzzer Non-Blocking (GPIO 19)
unsigned long lastBuzzerToggleMs  = 0;
bool          buzzerPinState      = false;

// ============================================================================
// 7. FUNGSI WAKTU (KOMBINASI NTP + RTC DS3231SN 0x68)
// ============================================================================
time_t getCurrentEpoch() {
  time_t now;
  time(&now);
  // 1. Jika NTP sudah tersinkronisasi (epoch valid > tahun 2024)
  if (now > 1700000000L) {
    if (state.rtcAvailable) {
      DateTime rtcNow = rtc.now();
      if (labs((long)now - (long)rtcNow.unixtime()) > 60L) {
        rtc.adjust(DateTime((uint32_t)now));
      }
    }
    return now;
  }

  // 2. Jika offline / NTP belum dapat, ambil waktu akurat dari RTC DS3231SN (0x68)
  if (state.rtcAvailable) {
    DateTime rtcNow = rtc.now();
    if (rtcNow.unixtime() > 1700000000UL) {
      return (time_t)rtcNow.unixtime();
    }
  }

  // 3. Fallback terakhir jika RTC belum disetel dan Wi-Fi offline
  return 1761460000L + (millis() / 1000UL);
}

String formatIso8601Utc(time_t epoch) {
  if (epoch <= 0) return "";
  struct tm timeinfo;
  gmtime_r(&epoch, &timeinfo);
  char buf[25];
  strftime(buf, sizeof(buf), "%Y-%m-%dT%H:%M:%SZ", &timeinfo);
  return String(buf);
}

// ============================================================================
// 8. PERSISTENSI NVS (PREFERENCES)
// ============================================================================
void loadSettingsFromNVS() {
  prefs.begin("hatchery", true);
  state.wifiSsid             = prefs.getString("wifi_ssid", DEFAULT_WIFI_SSID);
  state.wifiPass             = prefs.getString("wifi_pass", DEFAULT_WIFI_PASS);
  state.targetTemperature    = prefs.getFloat("target_temp", TEMP_DEFAULT_TARGET);
  state.motorIntervalMinutes = prefs.getInt("m_interval", 120);
  state.motorDurationSeconds = prefs.getInt("m_duration", 20);
  state.motorSpeedPercent    = prefs.getInt("m_speed", 50);

  state.incubationStatus     = prefs.getString("inc_status", "idle");
  state.startedAtEpoch       = prefs.getLong64("inc_start", 0);
  state.finishAtEpoch        = prefs.getLong64("inc_finish", 0);
  state.lastTurnedEpoch      = prefs.getLong64("last_turn", 0);
  state.lampMode             = prefs.getString("lamp_mode", "auto");
  state.lampIntensity        = prefs.getInt("lamp_int", 0);
  state.ventMode             = prefs.getString("vent_mode", "auto");
  state.ventAngle            = prefs.getInt("vent_ang", 0);
  prefs.end();

  state.lampFuzzyActive = (state.lampMode == "auto");
  state.ventFuzzyActive = (state.ventMode == "auto");
}

void saveSettingsToNVS() {
  // Jeda interrupt dimmer sejenak saat NVS menulis ke Flash
  dimmerZCPaused = true;
  GPIO.out_w1tc = (1UL << PIN_DIMMER_DIM);

  prefs.begin("hatchery", false);
  prefs.putString("wifi_ssid", state.wifiSsid);
  prefs.putString("wifi_pass", state.wifiPass);
  prefs.putFloat("target_temp", state.targetTemperature);
  prefs.putInt("m_interval", state.motorIntervalMinutes);
  prefs.putInt("m_duration", state.motorDurationSeconds);
  prefs.putInt("m_speed", state.motorSpeedPercent);

  prefs.putString("inc_status", state.incubationStatus);
  prefs.putLong64("inc_start", state.startedAtEpoch);
  prefs.putLong64("inc_finish", state.finishAtEpoch);
  prefs.putLong64("last_turn", state.lastTurnedEpoch);
  prefs.putString("lamp_mode", state.lampMode);
  prefs.putInt("lamp_int", state.lampIntensity);
  prefs.putString("vent_mode", state.ventMode);
  prefs.putInt("vent_ang", state.ventAngle);
  prefs.end();

  dimmerZCPaused = false;
}

// ============================================================================
// 9. AKTUATOR: ROBOTDYN AC DIMMER (GPIO 25/34), SERVO MG90S (GPIO 18), BUZZER
// ============================================================================
void applyLampOutput(int intensityPercent) {
  int clamped = constrain(intensityPercent, 0, 100);
  if (state.lampMode == "manual") {
    clamped = constrain(clamped, 0, LAMP_MANUAL_MAX_PCT); // Maks 80% saat manual
  }
  state.lampIntensity = clamped;

  if (clamped <= 2) {
    acDimmerIntensity = 0;
    dimTargetTick     = 92;
    GPIO.out_w1tc     = (1UL << PIN_DIMMER_DIM);
  } else if (clamped >= 98) {
    acDimmerIntensity = 100;
    dimTargetTick     = 8;
    GPIO.out_w1ts     = (1UL << PIN_DIMMER_DIM);
  } else {
    dimTargetTick     = map(clamped, 3, 97, 92, 8);
    acDimmerIntensity = clamped;
  }
}

void applyVentilationOutput(int angleDeg) {
  int clamped = constrain(angleDeg, VENT_ANGLE_MIN, VENT_ANGLE_MAX);
  state.ventAngle = clamped;
  mg90sServo.write(clamped);
}

void startTurningCycle() {
  state.motorRunning     = true;
  state.turningStatus    = "turning";
  state.motorTurnStartMs = millis();
  Serial.printf("[TURNING] Siklus balik telur aktif selama %d detik\n", state.motorDurationSeconds);
}

void stopTurningCycle(bool completedNormally) {
  state.motorRunning = false;
  if (completedNormally) {
    state.lastTurnedEpoch = getCurrentEpoch();
    if (state.incubationStatus == "running" && state.stage == "turning") {
      state.nextTurningEpoch = state.lastTurnedEpoch + (state.motorIntervalMinutes * 60L);
      state.turningStatus    = "idle";
    } else if (state.stage == "lockdown") {
      state.nextTurningEpoch = 0;
      state.turningStatus    = "lockdown";
    } else {
      state.nextTurningEpoch = 0;
      state.turningStatus    = "idle";
    }
    saveSettingsToNVS();
  }
}

void updateBuzzerAlarm() {
  bool isTempAlarm = (state.temperature > TEMP_SAFE_MAX || state.temperature < TEMP_SAFE_MIN);
  bool isCritical  = isTempAlarm || state.sensorError;

  if (!isCritical) {
    if (buzzerPinState) {
      digitalWrite(PIN_BUZZER_SFM27, LOW);
      buzzerPinState = false;
    }
    return;
  }

  // Pola Beep-Beep intermiten (300ms ON, 700ms OFF)
  unsigned long nowMs = millis();
  unsigned long interval = buzzerPinState ? 300UL : 700UL;
  if (nowMs - lastBuzzerToggleMs >= interval) {
    lastBuzzerToggleMs = nowMs;
    buzzerPinState     = !buzzerPinState;
    digitalWrite(PIN_BUZZER_SFM27, buzzerPinState ? HIGH : LOW);
  }
}

// ============================================================================
// 10. SISTEM INFERENSI LOGIKA FUZZY MAMDANI (2 INPUT -> 2 OUTPUT, 9 ATURAN)
// ============================================================================
// Tahapan Metode Fuzzy Mamdani:
//   1. Fuzzifikasi   : Mengubah nilai sensor (Error Suhu & Perubahan Suhu) ke derajat
//                      keanggotaan [0..1] menggunakan kurva Bahu dan Segitiga.
//   2. Inferensi MIN : Mengevaluasi 9 aturan IF-THEN (3x3) dengan operator AND (MIN)
//                      untuk memotong (clipping) tinggi kurva himpunan output.
//   3. Agregasi MAX  : Menggabungkan seluruh kurva output yang terpotong dengan MAX.
//   4. Defuzzifikasi : Menghitung titik berat kurva gabungan (Centroid / Center of Area).

enum FuzzyErrorSet {
  ERR_DINGIN = 0,
  ERR_NORMAL = 1,
  ERR_PANAS  = 2
};

enum FuzzyDeltaSet {
  DERR_TURUN  = 0,
  DERR_STABIL = 1,
  DERR_NAIK   = 2
};

enum FuzzyLampSet {
  LAMP_MATI          = 0,
  LAMP_REDUP         = 1,
  LAMP_SEDANG        = 2,
  LAMP_TERANG        = 3,
  LAMP_SANGAT_TERANG = 4
};

enum FuzzyVentSet {
  VENT_TUTUP       = 0,
  VENT_BUKA_KECIL  = 1,
  VENT_BUKA_SEDANG = 2,
  VENT_BUKA_LEBAR  = 3,
  VENT_BUKA_PENUH  = 4
};

struct MamdaniRuleOutput {
  FuzzyLampSet lampSet;
  FuzzyVentSet ventSet;
};

float fuzzyTriangle(float x, float a, float b, float c) {
  if (x <= a || x >= c) return 0.0f;
  if (fabs(x - b) < 0.0001f) return 1.0f;
  if (x < b) return (x - a) / (b - a);
  return (c - x) / (c - b);
}

float fuzzyLeftShoulder(float x, float a, float b) {
  if (x <= a) return 1.0f;
  if (x >= b) return 0.0f;
  return (b - x) / (b - a);
}

float fuzzyRightShoulder(float x, float a, float b) {
  if (x <= a) return 0.0f;
  if (x >= b) return 1.0f;
  return (x - a) / (b - a);
}

// Kurva Keanggotaan Output 1: Intensitas Lampu Pemanas (0% - 100%)
float muOutputLamp(int setIdx, float x) {
  switch (setIdx) {
    case LAMP_MATI:          return fuzzyLeftShoulder(x, 0.0f, 10.0f);
    case LAMP_REDUP:         return fuzzyTriangle(x, 5.0f, 25.0f, 45.0f);
    case LAMP_SEDANG:        return fuzzyTriangle(x, 30.0f, 50.0f, 70.0f);
    case LAMP_TERANG:        return fuzzyTriangle(x, 55.0f, 75.0f, 95.0f);
    case LAMP_SANGAT_TERANG: return fuzzyRightShoulder(x, 90.0f, 100.0f);
    default:                 return 0.0f;
  }
}

// Kurva Keanggotaan Output 2: Sudut Bukaan Servo Ventilasi (0° - 90°)
float muOutputVent(int setIdx, float x) {
  switch (setIdx) {
    case VENT_TUTUP:       return fuzzyLeftShoulder(x, 0.0f, 10.0f);
    case VENT_BUKA_KECIL:  return fuzzyTriangle(x, 5.0f, 25.0f, 45.0f);
    case VENT_BUKA_SEDANG: return fuzzyTriangle(x, 25.0f, 45.0f, 65.0f);
    case VENT_BUKA_LEBAR:  return fuzzyTriangle(x, 45.0f, 65.0f, 85.0f);
    case VENT_BUKA_PENUH:  return fuzzyRightShoulder(x, 80.0f, 90.0f);
    default:               return 0.0f;
  }
}

// Defuzzifikasi Mamdani Centroid (Center of Area) untuk Lampu (Domain 0..100%)
float defuzzifyCentroidLamp(const float clipLevel[5]) {
  float num = 0.0f;
  float den = 0.0f;
  for (int x = 0; x <= 100; x++) {
    float xf = (float)x;
    float muAgg = 0.0f;
    for (int k = 0; k < 5; k++) {
      if (clipLevel[k] > 0.0f) {
        // Implikasi MIN (potong kurva) lalu Agregasi MAX (gabung kurva)
        float clipped = fminf(clipLevel[k], muOutputLamp(k, xf));
        muAgg = fmaxf(muAgg, clipped);
      }
    }
    num += xf * muAgg;
    den += muAgg;
  }
  return (den > 0.0001f) ? (num / den) : 50.0f;
}

// Defuzzifikasi Mamdani Centroid (Center of Area) untuk Servo (Domain 0..90°)
float defuzzifyCentroidVent(const float clipLevel[5]) {
  float num = 0.0f;
  float den = 0.0f;
  for (int x = 0; x <= 90; x++) {
    float xf = (float)x;
    float muAgg = 0.0f;
    for (int k = 0; k < 5; k++) {
      if (clipLevel[k] > 0.0f) {
        // Implikasi MIN (potong kurva) lalu Agregasi MAX (gabung kurva)
        float clipped = fminf(clipLevel[k], muOutputVent(k, xf));
        muAgg = fmaxf(muAgg, clipped);
      }
    }
    num += xf * muAgg;
    den += muAgg;
  }
  return (den > 0.0001f) ? (num / den) : 25.0f;
}

void evaluateFuzzyControl() {
  float e  = state.temperature - state.targetTemperature;
  float de = state.temperature - state.prevTemperature;

  // 1. FUZZIFIKASI INPUT 1: Error Suhu (e) -> 3 Himpunan (Dingin, Normal, Panas)
  float muError[3];
  muError[ERR_DINGIN] = fuzzyLeftShoulder(e, -1.0f, 0.0f);
  muError[ERR_NORMAL] = fuzzyTriangle(e, -0.5f, 0.0f, 0.5f);
  muError[ERR_PANAS]  = fuzzyRightShoulder(e, 0.0f, 1.0f);

  // 2. FUZZIFIKASI INPUT 2: Perubahan Suhu (de) -> 3 Himpunan (Turun, Stabil, Naik)
  float muDelta[3];
  muDelta[DERR_TURUN]  = fuzzyLeftShoulder(de, -0.20f, 0.0f);
  muDelta[DERR_STABIL] = fuzzyTriangle(de, -0.10f, 0.0f, 0.10f);
  muDelta[DERR_NAIK]   = fuzzyRightShoulder(de, 0.0f, 0.20f);

  // 3. BASIS ATURAN MAMDANI (3x3 = 9 Aturan IF-THEN Linguistik)
  const MamdaniRuleOutput rules[3][3] = {
    // Baris 1: Saat Suhu DINGIN (de: Turun, Stabil, Naik)
    {
      { LAMP_SANGAT_TERANG, VENT_TUTUP },
      { LAMP_SANGAT_TERANG, VENT_TUTUP },
      { LAMP_TERANG,        VENT_BUKA_KECIL }
    },
    // Baris 2: Saat Suhu NORMAL / IDEAL (de: Turun, Stabil, Naik)
    {
      { LAMP_TERANG,        VENT_BUKA_KECIL },
      { LAMP_SEDANG,        VENT_BUKA_KECIL },
      { LAMP_REDUP,         VENT_BUKA_SEDANG }
    },
    // Baris 3: Saat Suhu PANAS (de: Turun, Stabil, Naik)
    {
      { LAMP_REDUP,         VENT_BUKA_SEDANG },
      { LAMP_MATI,          VENT_BUKA_LEBAR },
      { LAMP_MATI,          VENT_BUKA_PENUH }
    }
  };

  // Evaluasi Inferensi MIN (AND) & Agregasi MAX pada Himpunan Output
  float clipLamp[5] = { 0.0f, 0.0f, 0.0f, 0.0f, 0.0f };
  float clipVent[5] = { 0.0f, 0.0f, 0.0f, 0.0f, 0.0f };

  for (int i = 0; i < 3; i++) {
    for (int j = 0; j < 3; j++) {
      float alpha = fminf(muError[i], muDelta[j]); // Operator AND = MIN
      if (alpha > 0.0f) {
        FuzzyLampSet outL = rules[i][j].lampSet;
        FuzzyVentSet outV = rules[i][j].ventSet;
        clipLamp[outL] = fmaxf(clipLamp[outL], alpha); // Agregasi = MAX
        clipVent[outV] = fmaxf(clipVent[outV], alpha); // Agregasi = MAX
      }
    }
  }

  // 4. DEFUZZIFIKASI CENTROID (Center of Area / Titik Berat)
  float crispLamp = defuzzifyCentroidLamp(clipLamp);
  float crispVent = defuzzifyCentroidVent(clipVent);

  // Pengaman Ekstrem Suhu & Kelembapan Pasif (Sesuai PLAN.md):
  if (state.temperature >= TEMP_SAFE_MAX) {
    crispLamp = 0.0f;
    crispVent = 90.0f;
  } else if (state.temperature <= TEMP_SAFE_MIN) {
    crispLamp = 100.0f;
    crispVent = 0.0f;
  }
  if (state.humidity > HUMIDITY_WARN_MAX && e > -0.3f && crispVent < 35.0f) {
    crispVent = 35.0f;
  }

  crispLamp = constrain(crispLamp, 0.0f, (float)LAMP_AUTO_MAX_PCT);
  crispVent = constrain(crispVent, (float)VENT_ANGLE_MIN, (float)VENT_ANGLE_MAX);

  // 5. Terapkan ke aktuator yang berstatus "auto"
  if (state.lampMode == "auto") {
    state.lampFuzzyActive = true;
    applyLampOutput((int)round(crispLamp));
  } else {
    state.lampFuzzyActive = false;
  }

  if (state.ventMode == "auto") {
    state.ventFuzzyActive = true;
    applyVentilationOutput((int)round(crispVent));
  } else {
    state.ventFuzzyActive = false;
  }
}

// ============================================================================
// 11. INPUT TOMBOL FISIK (PUSH BUTTON START GPIO 26)
// ============================================================================
void startIncubationLocalOrRemote() {
  time_t now             = getCurrentEpoch();
  state.incubationStatus = "running";
  state.currentDay       = 1;
  state.startedAtEpoch   = now;
  state.finishAtEpoch    = now + (INCUBATION_TOTAL_DAYS * 86400L);
  state.stage            = "turning";
  state.lastTurnedEpoch  = now;
  state.nextTurningEpoch = now + (state.motorIntervalMinutes * 60L);
  state.encoderOk        = true;
  state.turningStatus    = "idle";
  saveSettingsToNVS();
  Serial.println("[INKUBASI] Siklus 21 hari DIMULAI.");
}

void stopIncubationLocalOrRemote() {
  if (state.motorRunning) {
    stopTurningCycle(false);
  }
  state.incubationStatus = "idle";
  state.currentDay       = 0;
  state.startedAtEpoch   = 0;
  state.finishAtEpoch    = 0;
  state.stage            = "";
  state.nextTurningEpoch = 0;
  state.turningStatus    = "idle";
  saveSettingsToNVS();
  Serial.println("[INKUBASI] Siklus inkubasi DIHENTIKAN (Idle).");
}

void pollPhysicalSwitches() {
  // Push Button Start (GPIO 26 - INPUT_PULLUP, Aktif LOW)
  bool reading = digitalRead(PIN_BTN_START);
  unsigned long nowMs = millis();

  if (reading != lastBtnReading) {
    lastBtnDebounceMs = nowMs;
  }
  lastBtnReading = reading;

  if (nowMs - lastBtnDebounceMs > 50UL) {
    if (reading != stableBtnState) {
      stableBtnState = reading;
      if (stableBtnState == LOW) {
        btnPressStartMs     = nowMs;
        btnLongPressHandled = false;
      } else {
        // Tekan singkat (< 2 detik) saat idle/finished -> Mulai inkubasi
        if (!btnLongPressHandled && (nowMs - btnPressStartMs < 2000UL)) {
          if (state.incubationStatus != "running") {
            startIncubationLocalOrRemote();
            digitalWrite(PIN_BUZZER_SFM27, HIGH);
            delay(100);
            digitalWrite(PIN_BUZZER_SFM27, LOW);
          }
        }
      }
    } else if (stableBtnState == LOW && !btnLongPressHandled) {
      // Tekan tahan (>= 3 detik) saat running -> Hentikan inkubasi
      if (nowMs - btnPressStartMs >= 3000UL) {
        btnLongPressHandled = true;
        if (state.incubationStatus == "running") {
          stopIncubationLocalOrRemote();
          digitalWrite(PIN_BUZZER_SFM27, HIGH);
          delay(250);
          digitalWrite(PIN_BUZZER_SFM27, LOW);
        }
      }
    }
  }
}

// ============================================================================
// 12. LAYAR DUAL OLED 0.96" SSD1306 (CARA 1: BUS I2C-1 & BUS I2C-2)
// ============================================================================
void formatStartDateWib(time_t epochUtc, char* outBuf, size_t outSize) {
  if (epochUtc <= 0) {
    snprintf(outBuf, outSize, "Belum Mulai");
    return;
  }
  time_t wibEpoch = epochUtc + (7 * 3600L); // WIB (UTC+7)
  struct tm timeinfo;
  gmtime_r(&wibEpoch, &timeinfo);
  snprintf(outBuf, outSize, "%02d/%02d/%04d",
           timeinfo.tm_mday, timeinfo.tm_mon + 1, timeinfo.tm_year + 1900);
}

const char* getSesiLabel() {
  if (state.incubationStatus == "running") {
    if (state.stage == "lockdown") return "Tidak Putar";
    if (state.motorRunning)        return "Sedang Putar";
    return "Putar Telur";
  }
  if (state.incubationStatus == "finished") return "Siap Menetas";
  return "Belum Jalan";
}

const char* getKoneksiLabel() {
  if (WiFi.status() != WL_CONNECTED) return "Putus WiFi";
  if (!state.backendChecked)         return "Mengecek..";
  if (state.backendConnected)        return "Berhasil";
  return "Gagal Web";
}

void renderSingleCombinedOled(Adafruit_SSD1306& disp, const char* startDateStr) {
  disp.clearDisplay();
  disp.setTextSize(1);
  disp.setTextColor(SSD1306_WHITE);

  disp.setCursor(0, 0);
  disp.printf("Mulai  : %s", startDateStr);

  disp.setCursor(0, 11);
  if (state.incubationStatus == "running") {
    disp.printf("Progres: Hari %d/21", state.currentDay);
  } else if (state.incubationStatus == "finished") {
    disp.print("Progres: Selesai 21H");
  } else {
    disp.print("Progres: 0/21 Hari");
  }

  disp.setCursor(0, 22);
  disp.printf("Sesi   : %s", getSesiLabel());

  disp.setCursor(0, 33);
  if (state.sensorError) {
    disp.print("Suhu   : Cek Sensor!");
  } else {
    disp.printf("Suhu   : %.1f C", state.temperature);
  }

  disp.setCursor(0, 44);
  if (state.sensorError) {
    disp.print("Lembap : Cek Sensor!");
  } else {
    disp.printf("Lembap : %.0f %%", state.humidity);
  }

  disp.setCursor(0, 55);
  disp.printf("Koneksi: %s", getKoneksiLabel());

  disp.display();
}

void updateOledDisplay() {
  if (!state.oledAvailable && !state.oled2Available) return;

  char startDateStr[16];
  if (state.incubationStatus == "running" || state.incubationStatus == "finished") {
    formatStartDateWib(state.startedAtEpoch, startDateStr, sizeof(startDateStr));
  } else {
    snprintf(startDateStr, sizeof(startDateStr), "Belum Mulai");
  }

  // Jika kedua OLED terpasang (OLED-1 di GPIO 21/22 & OLED-2 di GPIO 32/33):
  // - OLED-1 menampilkan Suhu, Kelembapan, dan Status Koneksi
  // - OLED-2 menampilkan Kapan Mulai, Progres Hari, dan Sesi Inkubasi
  if (state.oledAvailable && state.oled2Available) {
    // --- LAYAR OLED 1 (GPIO 21 SDA, GPIO 22 SCL): SUHU, KELEMBAPAN & KONEKSI ---
    oled.clearDisplay();
    oled.setTextSize(1);
    oled.setTextColor(SSD1306_WHITE);

    oled.setCursor(10, 0);
    oled.print("SUHU & KELEMBAPAN");
    oled.drawLine(0, 11, 127, 11, SSD1306_WHITE);

    oled.setCursor(0, 18);
    if (state.sensorError) {
      oled.print("Suhu   : Cek Sensor!");
    } else {
      oled.printf("Suhu   : %.1f C", state.temperature);
    }

    oled.setCursor(0, 34);
    if (state.sensorError) {
      oled.print("Lembap : Cek Sensor!");
    } else {
      oled.printf("Lembap : %.0f %%", state.humidity);
    }

    oled.setCursor(0, 50);
    oled.printf("Koneksi: %s", getKoneksiLabel());
    oled.display();

    // --- LAYAR OLED 2 (GPIO 32 SDA, GPIO 33 SCL): MULAI, PROGRES & SESI ---
    oled2.clearDisplay();
    oled2.setTextSize(1);
    oled2.setTextColor(SSD1306_WHITE);

    oled2.setCursor(16, 0);
    oled2.print("STATUS INKUBASI");
    oled2.drawLine(0, 11, 127, 11, SSD1306_WHITE);

    oled2.setCursor(0, 18);
    oled2.printf("Mulai  : %s", startDateStr);

    oled2.setCursor(0, 34);
    if (state.incubationStatus == "running") {
      oled2.printf("Progres: Hari %d/21", state.currentDay);
    } else if (state.incubationStatus == "finished") {
      oled2.print("Progres: Selesai 21H");
    } else {
      oled2.print("Progres: 0/21 Hari");
    }

    oled2.setCursor(0, 50);
    oled2.printf("Sesi   : %s", getSesiLabel());
    oled2.display();
    return;
  }

  // Fallback otomatis jika hanya 1 OLED yang sedang dicolok: tampilkan semua 6 baris
  if (state.oledAvailable) {
    renderSingleCombinedOled(oled, startDateStr);
  } else if (state.oled2Available) {
    renderSingleCombinedOled(oled2, startDateStr);
  }
}

// ============================================================================
// 13. SIKLUS INKUBASI 21 HARI & PEMBACAAN SENSOR SHT30 (0x44)
// ============================================================================
void updateIncubationCycle() {
  if (state.motorRunning) {
    if (millis() - state.motorTurnStartMs >= (unsigned long)state.motorDurationSeconds * 1000UL) {
      stopTurningCycle(true);
    }
  }

  if (state.incubationStatus != "running") {
    if (state.incubationStatus == "idle") {
      state.currentDay       = 0;
      state.stage            = "";
      state.nextTurningEpoch = 0;
      state.turningStatus    = "idle";
    } else if (state.incubationStatus == "finished") {
      state.currentDay       = INCUBATION_TOTAL_DAYS;
      state.stage            = "lockdown";
      state.nextTurningEpoch = 0;
      state.turningStatus    = "lockdown";
    }
    return;
  }

  time_t now = getCurrentEpoch();
  if (now >= state.finishAtEpoch && state.finishAtEpoch > 0) {
    state.incubationStatus = "finished";
    state.currentDay       = INCUBATION_TOTAL_DAYS;
    state.stage            = "lockdown";
    state.nextTurningEpoch = 0;
    if (state.motorRunning) stopTurningCycle(false);
    state.turningStatus = "lockdown";
    saveSettingsToNVS();
    return;
  }

  long elapsedSeconds = max(0L, (long)(now - state.startedAtEpoch));
  int calculatedDay   = (int)(elapsedSeconds / 86400L) + 1;
  state.currentDay    = constrain(calculatedDay, 1, INCUBATION_TOTAL_DAYS);

  if (state.currentDay >= INCUBATION_LOCKDOWN_START) {
    state.stage            = "lockdown";
    state.nextTurningEpoch = 0;
    if (state.motorRunning) stopTurningCycle(false);
    state.turningStatus = "lockdown";
  } else {
    state.stage = "turning";
    if (!state.motorRunning) {
      state.turningStatus = "idle";
      if (state.nextTurningEpoch <= 0) {
        time_t baseTime = (state.lastTurnedEpoch > 0) ? state.lastTurnedEpoch : now;
        state.nextTurningEpoch = baseTime + (state.motorIntervalMinutes * 60L);
      }
      if (now >= state.nextTurningEpoch) {
        startTurningCycle();
      }
    }
  }
}

void readSensorData() {
  state.prevTemperature = state.temperature;

  if (state.sht30Available) {
    float t = sht30.readTemperature();
    float h = sht30.readHumidity();

    if (!isnan(t) && !isnan(h)) {
      state.temperature      = roundf(t * 10.0f) / 10.0f;
      state.humidity         = roundf(h);
      state.sensorFailStreak = 0;
      state.sensorError      = false;
    } else {
      state.sensorFailStreak++;
      if (state.sensorFailStreak >= 3) {
        state.sensorError = true;
      }
    }
  } else {
    if (sht30.begin(I2C_ADDR_SHT30)) {
      state.sht30Available   = true;
      state.sensorFailStreak = 0;
      state.sensorError      = false;
    } else {
      state.sensorFailStreak++;
      if (state.sensorFailStreak >= 3) {
        state.sensorError = true;
      }
    }
  }
}

// ============================================================================
// 14. GANTI WI-FI ANTI-PUTUS (TIMEOUT 30 DETIK -> AUTO ROLLBACK)
// ============================================================================
bool applyNewWifiWithRollback(const String& newSsid, const String& newPassword) {
  if (newSsid.length() < 1 || newSsid.length() > 32 ||
      newPassword.length() < 8 || newPassword.length() > 63) {
    return false;
  }

  String oldSsid = state.wifiSsid;
  String oldPass = state.wifiPass;

  WiFi.disconnect(true);
  delay(200);
  WiFi.mode(WIFI_STA);
  WiFi.begin(newSsid.c_str(), newPassword.c_str());

  unsigned long startAttempt = millis();
  while (WiFi.status() != WL_CONNECTED && (millis() - startAttempt < WIFI_CONNECT_TIMEOUT_MS)) {
    delay(250);
  }

  if (WiFi.status() == WL_CONNECTED) {
    state.wifiSsid         = newSsid;
    state.wifiPass         = newPassword;
    state.commAlertPending = false;
    state.commAlertMessage = "";
    saveSettingsToNVS();
    return true;
  }

  // Rollback ke Wi-Fi lama di NVS
  WiFi.disconnect(true);
  delay(200);
  WiFi.begin(oldSsid.c_str(), oldPass.c_str());

  unsigned long rollbackStart = millis();
  while (WiFi.status() != WL_CONNECTED && (millis() - rollbackStart < WIFI_CONNECT_TIMEOUT_MS)) {
    delay(250);
  }

  state.wifiSsid         = oldSsid;
  state.wifiPass         = oldPass;
  state.commAlertPending = true;
  state.commAlertMessage = "Gagal menyambung ke Wi-Fi '" + newSsid + "' dalam 30 detik. Perangkat rollback ke '" + oldSsid + "'.";
  return false;
}

// ============================================================================
// 15. RINGKASAN KONDISI OPERASIONAL ALAT (device_condition)
// ============================================================================
void buildDeviceCondition(String& outHealth, String& outSummary) {
  bool tempOut = (state.temperature > TEMP_SAFE_MAX || state.temperature < TEMP_SAFE_MIN);
  bool fault   = tempOut || !state.encoderOk || state.sensorError;

  if (fault) {
    outHealth = "fault";
    if (state.temperature > TEMP_SAFE_MAX) {
      outSummary = "Peringatan: Suhu melewati batas atas (" + String(TEMP_SAFE_MAX, 1) + "°C).";
    } else if (state.temperature < TEMP_SAFE_MIN) {
      outSummary = "Peringatan: Suhu di bawah batas bawah (" + String(TEMP_SAFE_MIN, 1) + "°C).";
    } else {
      outSummary = "Gangguan sensor: Pembacaan sensor SHT30/SHT31 gagal 3 kali berturut-turut.";
    }
    return;
  }

  if (state.commAlertPending) {
    outHealth  = "warning";
    outSummary = state.commAlertMessage;
    return;
  }

  outHealth = "normal";
  if (state.incubationStatus == "running") {
    String stageLabel = (state.stage == "lockdown") ? "Fase Lockdown" : "Fase Turning";
    outSummary = "Hari ke-" + String(state.currentDay) + " (" + stageLabel + "). Suhu dan aktuator bekerja normal.";
  } else if (state.incubationStatus == "finished") {
    outSummary = "Siklus 21 hari selesai. Alat siap untuk inkubasi berikutnya.";
  } else {
    outSummary = "Inkubator siap (Idle). Suhu dan aktuator bekerja normal.";
  }
}

// ============================================================================
// 16. PEMROSESAN PERINTAH & PENGATURAN DARI BACKEND GO FIBER
// ============================================================================
void recordCommandResult(long cmdId, const String& target, const String& action, bool success) {
  state.lastCmdId        = cmdId;
  state.lastCmdTarget    = target;
  state.lastCmdAction    = action;
  state.lastCmdStatus    = success ? "executed" : "failed";
  state.lastCmdUpdatedAt = getCurrentEpoch();
}

void processIncomingCommand(JsonVariantConst cmdObj) {
  if (cmdObj.isNull()) return;

  long   cmdId  = cmdObj["id"] | 0L;
  String target = cmdObj["target"] | "";
  if (target.isEmpty() || (cmdId > 0 && cmdId == state.lastCmdId)) return;

  if (target == "incubation") {
    String action = cmdObj["action"] | "";
    if (action == "start") {
      if (state.incubationStatus == "running") {
        recordCommandResult(cmdId, "incubation", "start", false);
      } else {
        startIncubationLocalOrRemote();
        recordCommandResult(cmdId, "incubation", "start", true);
      }
    } else if (action == "stop") {
      stopIncubationLocalOrRemote();
      recordCommandResult(cmdId, "incubation", "stop", true);
    } else {
      recordCommandResult(cmdId, "incubation", action, false);
    }
  }
  else if (target == "lamp") {
    String mode = cmdObj["mode"] | "";
    if (mode == "auto") {
      state.lampMode        = "auto";
      state.lampFuzzyActive = true;
      evaluateFuzzyControl();
      saveSettingsToNVS();
      recordCommandResult(cmdId, "lamp", "auto", true);
    } else if (mode == "manual") {
      int intensity = cmdObj["intensity"] | -1;
      if (intensity < 0 || intensity > LAMP_MANUAL_MAX_PCT) {
        recordCommandResult(cmdId, "lamp", "manual", false);
      } else {
        state.lampMode        = "manual";
        state.lampFuzzyActive = false;
        applyLampOutput(intensity);
        saveSettingsToNVS();
        recordCommandResult(cmdId, "lamp", "manual", true);
      }
    } else {
      recordCommandResult(cmdId, "lamp", mode, false);
    }
  }
  else if (target == "ventilation") {
    String mode = cmdObj["mode"] | "";
    if (mode == "auto") {
      state.ventMode        = "auto";
      state.ventFuzzyActive = true;
      evaluateFuzzyControl();
      saveSettingsToNVS();
      recordCommandResult(cmdId, "ventilation", "auto", true);
    } else if (mode == "manual") {
      int angle = cmdObj["angle"] | -1;
      if (angle < VENT_ANGLE_MIN || angle > VENT_ANGLE_MAX) {
        recordCommandResult(cmdId, "ventilation", "manual", false);
      } else {
        state.ventMode        = "manual";
        state.ventFuzzyActive = false;
        applyVentilationOutput(angle);
        saveSettingsToNVS();
        recordCommandResult(cmdId, "ventilation", "manual", true);
      }
    } else {
      recordCommandResult(cmdId, "ventilation", mode, false);
    }
  }
  else if (target == "turning") {
    String action = cmdObj["action"] | "";
    if (action == "turn_now") {
      bool canTurn = (state.incubationStatus == "running") &&
                     (state.stage == "turning") &&
                     state.encoderOk &&
                     !state.motorRunning;
      if (!canTurn) {
        recordCommandResult(cmdId, "turning", "turn_now", false);
      } else {
        startTurningCycle();
        recordCommandResult(cmdId, "turning", "turn_now", true);
      }
    } else {
      recordCommandResult(cmdId, "turning", action, false);
    }
  }
}

void processIncomingSettings(JsonVariantConst settingsObj) {
  if (settingsObj.isNull()) return;
  bool changed = false;

  if (!settingsObj["target_temperature"].isNull()) {
    float newTarget = settingsObj["target_temperature"].as<float>();
    if (newTarget >= TEMP_SAFE_MIN && newTarget <= TEMP_SAFE_MAX &&
        fabs(newTarget - state.targetTemperature) > 0.01f) {
      state.targetTemperature = newTarget;
      changed = true;
    }
  }

  JsonVariantConst motorObj = settingsObj["motor"];
  if (!motorObj.isNull()) {
    int interval = motorObj["interval_minutes"] | state.motorIntervalMinutes;
    int duration = motorObj["duration_seconds"] | state.motorDurationSeconds;
    int speed    = motorObj["speed_percent"]    | state.motorSpeedPercent;

    if (interval >= MOTOR_INTERVAL_MIN && interval <= MOTOR_INTERVAL_MAX &&
        duration >= MOTOR_DURATION_MIN && duration <= MOTOR_DURATION_MAX &&
        speed    >= MOTOR_SPEED_MIN    && speed    <= MOTOR_SPEED_MAX) {
      if (interval != state.motorIntervalMinutes ||
          duration != state.motorDurationSeconds ||
          speed    != state.motorSpeedPercent) {
        state.motorIntervalMinutes = interval;
        state.motorDurationSeconds = duration;
        state.motorSpeedPercent    = speed;
        if (state.incubationStatus == "running" && state.stage == "turning" && !state.motorRunning) {
          time_t baseTime = (state.lastTurnedEpoch > 0) ? state.lastTurnedEpoch : getCurrentEpoch();
          state.nextTurningEpoch = baseTime + (state.motorIntervalMinutes * 60L);
        }
        changed = true;
      }
    }
  }

  if (changed) {
    saveSettingsToNVS();
    evaluateFuzzyControl();
  }

  JsonVariantConst wifiObj = settingsObj["wifi"];
  if (!wifiObj.isNull()) {
    String newSsid = wifiObj["ssid"]     | "";
    String newPass = wifiObj["password"] | "";
    if (!newSsid.isEmpty() && !newPass.isEmpty() &&
        (newSsid != state.wifiSsid || newPass != state.wifiPass)) {
      applyNewWifiWithRollback(newSsid, newPass);
    }
  }
}

// ============================================================================
// 17. PENGIRIMAN TELEMETRI HTTP REST TIAP 5 DETIK
// ============================================================================
void sendTelemetryAndPollBackend() {
  if (WiFi.status() != WL_CONNECTED) {
    state.backendConnected = false;
    WiFi.reconnect();
    return;
  }

  time_t nowEpoch = getCurrentEpoch();
  String nowIso   = formatIso8601Utc(nowEpoch);

  JsonDocument doc;
  doc["device_id"]          = DEVICE_ID;
  doc["connection"]         = "online";
  doc["updated_at"]         = nowIso;
  doc["temperature"]        = state.temperature;
  doc["target_temperature"] = state.targetTemperature;
  doc["humidity"]           = (int)round(state.humidity);

  JsonObject inc = doc["incubation"].to<JsonObject>();
  inc["status"]     = state.incubationStatus;
  inc["total_days"] = INCUBATION_TOTAL_DAYS;
  if (state.incubationStatus == "idle") {
    inc["current_day"] = nullptr;
    inc["started_at"]  = nullptr;
    inc["finish_at"]   = nullptr;
    inc["stage"]       = nullptr;
  } else {
    inc["current_day"] = state.currentDay;
    inc["started_at"]  = formatIso8601Utc(state.startedAtEpoch);
    inc["finish_at"]   = formatIso8601Utc(state.finishAtEpoch);
    inc["stage"]       = state.stage;
  }

  String sysHealth, sysSummary;
  buildDeviceCondition(sysHealth, sysSummary);
  JsonObject cond = doc["device_condition"].to<JsonObject>();
  cond["system_health"]  = sysHealth;
  cond["uptime_seconds"] = (unsigned long)(millis() / 1000UL);
  cond["summary"]        = sysSummary;

  JsonObject lamp = doc["lamp"].to<JsonObject>();
  lamp["mode"]         = state.lampMode;
  lamp["intensity"]    = state.lampIntensity;
  lamp["fuzzy_active"] = state.lampFuzzyActive;

  JsonObject vent = doc["ventilation"].to<JsonObject>();
  vent["mode"]         = state.ventMode;
  vent["angle"]        = state.ventAngle;
  vent["fuzzy_active"] = state.ventFuzzyActive;

  JsonObject motor = doc["motor"].to<JsonObject>();
  motor["running"]    = state.motorRunning;
  motor["encoder_ok"] = state.encoderOk;

  JsonObject turning = doc["turning"].to<JsonObject>();
  turning["status"] = state.turningStatus;
  if (state.lastTurnedEpoch > 0) {
    turning["last_turned_at"] = formatIso8601Utc(state.lastTurnedEpoch);
  } else {
    turning["last_turned_at"] = nullptr;
  }
  if (state.incubationStatus == "running" && state.stage == "turning" && state.nextTurningEpoch > 0) {
    turning["next_turning_at"] = formatIso8601Utc(state.nextTurningEpoch);
  } else {
    turning["next_turning_at"] = nullptr;
  }

  JsonObject wifi = doc["wifi"].to<JsonObject>();
  wifi["connected"] = true;
  wifi["ssid"]      = WiFi.SSID();
  wifi["rssi"]      = WiFi.RSSI();

  if (state.lastCmdId > 0) {
    JsonObject lastCmd = doc["last_command"].to<JsonObject>();
    lastCmd["id"]         = state.lastCmdId;
    lastCmd["target"]     = state.lastCmdTarget;
    lastCmd["action"]     = state.lastCmdAction;
    lastCmd["status"]     = state.lastCmdStatus;
    lastCmd["updated_at"] = formatIso8601Utc(state.lastCmdUpdatedAt);
  } else {
    doc["last_command"] = nullptr;
  }

  JsonArray alertsArr = doc["active_alerts"].to<JsonArray>();
  if (state.temperature > TEMP_SAFE_MAX) {
    JsonObject a = alertsArr.add<JsonObject>();
    a["type"]      = "over_temperature";
    a["severity"]  = "high";
    a["message"]   = "Suhu " + String(state.temperature, 1) + "°C melewati batas atas (" + String(TEMP_SAFE_MAX, 1) + "°C)";
    a["timestamp"] = nowIso;
    a["status"]    = "active";
  }
  if (state.temperature < TEMP_SAFE_MIN) {
    JsonObject a = alertsArr.add<JsonObject>();
    a["type"]      = "low_temperature";
    a["severity"]  = "low";
    a["message"]   = "Suhu " + String(state.temperature, 1) + "°C di bawah batas bawah (" + String(TEMP_SAFE_MIN, 1) + "°C)";
    a["timestamp"] = nowIso;
    a["status"]    = "active";
  }
  if (state.sensorError) {
    JsonObject a = alertsArr.add<JsonObject>();
    a["type"]      = "sensor_error";
    a["severity"]  = "medium";
    a["message"]   = "Pembacaan sensor SHT30/SHT31 gagal 3 kali berturut-turut";
    a["timestamp"] = nowIso;
    a["status"]    = "active";
  }
  if (state.commAlertPending) {
    JsonObject a = alertsArr.add<JsonObject>();
    a["type"]      = "communication";
    a["severity"]  = "medium";
    a["message"]   = state.commAlertMessage;
    a["timestamp"] = nowIso;
    a["status"]    = "active";
  }

  String payload;
  serializeJson(doc, payload);

  auto postToBackend = [&](const String& url, String& outResponse) -> int {
    HTTPClient http;
    WiFiClientSecure secureClient;
    if (url.startsWith("https://")) {
      secureClient.setInsecure();
      http.begin(secureClient, url);
    } else {
      http.begin(url);
    }
    http.setTimeout(3500);
    http.addHeader("Content-Type", "application/json");
    http.addHeader("X-Device-Key", DEVICE_API_KEY);

    int code = http.POST(payload);
    if (code >= 200 && code < 300) {
      outResponse = http.getString();
    }
    http.end();
    return code;
  };

  String responseBody;
  int httpCode = postToBackend(String(BACKEND_TELEMETRY_URL), responseBody);
  if ((httpCode < 200 || httpCode >= 300) && strlen(BACKEND_FALLBACK_URL) > 0) {
    Serial.printf("[HTTP] URL utama gagal (HTTP %d), mencoba fallback Tailscale (%s)...\n", httpCode, BACKEND_FALLBACK_URL);
    httpCode = postToBackend(String(BACKEND_FALLBACK_URL), responseBody);
  }

  state.backendChecked = true;
  if (httpCode >= 200 && httpCode < 300) {
    state.backendConnected = true;
    if (state.commAlertPending) state.commAlertPending = false;

    if (responseBody.length() > 2) {
      JsonDocument respDoc;
      if (!deserializeJson(respDoc, responseBody)) {
        if (!respDoc["pending_command"].isNull()) {
          processIncomingCommand(respDoc["pending_command"]);
        } else if (!respDoc["command"].isNull()) {
          processIncomingCommand(respDoc["command"]);
        }
        if (!respDoc["settings"].isNull()) {
          processIncomingSettings(respDoc["settings"]);
        }
      }
    }
  } else {
    state.backendConnected = false;
    Serial.printf("[HTTP] Gagal kirim telemetri (HTTP %d)\n", httpCode);
  }
}

// ============================================================================
// 18. SETUP & MAIN LOOP
// ============================================================================
void setup() {
  Serial.begin(115200);
  delay(300);
  Serial.println("\n=== SMART HATCHERY ESP32 DOIT (30-PIN) STARTUP ===");

  loadSettingsFromNVS();

  // 1. Konfigurasi Pin Tombol Start dan Buzzer
  pinMode(PIN_BTN_START, INPUT_PULLUP);
  pinMode(PIN_BUZZER_SFM27, OUTPUT);
  digitalWrite(PIN_BUZZER_SFM27, LOW);

  // 2. Inisialisasi Bus I2C-1 (GPIO 21 = SDA, GPIO 22 = SCL) & Bus I2C-2 (GPIO 32 = SDA2, GPIO 33 = SCL2)
  Wire.begin(PIN_I2C_SDA, PIN_I2C_SCL);
  Wire1.begin(PIN_I2C2_SDA, PIN_I2C2_SCL);

  // [A] Sensor SHT30-D / SHT31-D (Alamat 0x44 di I2C-1)
  state.sht30Available = sht30.begin(I2C_ADDR_SHT30);
  Serial.printf("[I2C-1] SHT30  (0x44): %s\n", state.sht30Available ? "OK" : "TIDAK TERDETEKSI");

  // [B1] Layar OLED-1 0.96" SSD1306 (Alamat 0x3C di I2C-1: GPIO 21/22)
  state.oledAvailable = oled.begin(SSD1306_SWITCHCAPVCC, I2C_ADDR_OLED);
  Serial.printf("[I2C-1] OLED-1 (0x3C): %s\n", state.oledAvailable ? "OK" : "TIDAK TERDETEKSI");
  if (state.oledAvailable) {
    oled.clearDisplay();
    oled.setTextSize(1);
    oled.setTextColor(SSD1306_WHITE);
    oled.setCursor(10, 24);
    oled.println("SMART HATCHERY IoT");
    oled.setCursor(12, 38);
    oled.println("Layar 1: Siap...");
    oled.display();
  }

  // [B2] Layar OLED-2 0.96" SSD1306 (Alamat 0x3C di I2C-2: GPIO 32/33)
  state.oled2Available = oled2.begin(SSD1306_SWITCHCAPVCC, I2C_ADDR_OLED);
  Serial.printf("[I2C-2] OLED-2 (0x3C): %s\n", state.oled2Available ? "OK" : "TIDAK TERDETEKSI");
  if (state.oled2Available) {
    oled2.clearDisplay();
    oled2.setTextSize(1);
    oled2.setTextColor(SSD1306_WHITE);
    oled2.setCursor(10, 24);
    oled2.println("SMART HATCHERY IoT");
    oled2.setCursor(12, 38);
    oled2.println("Layar 2: Siap...");
    oled2.display();
  }

  // [C] Modul Jam RTC DS3231SN (Alamat 0x68 di I2C-1)
  state.rtcAvailable = rtc.begin(&Wire);
  Serial.printf("[I2C-1] RTC    (0x68): %s\n", state.rtcAvailable ? "OK" : "TIDAK TERDETEKSI");

  // 3. Inisialisasi Servo MG90S (GPIO 18)
  mg90sServo.setPeriodHertz(50);
  mg90sServo.attach(PIN_SERVO_MG90S, 500, 2400);
  applyVentilationOutput(state.ventAngle);

  // 4. Inisialisasi RobotDyn AC Dimmer (Z-C = GPIO 34, DIM = GPIO 25)
  initRobotDynDimmer();
  applyLampOutput(state.lampIntensity);

  // 5. Koneksi Wi-Fi & Sinkronisasi NTP
  WiFi.mode(WIFI_STA);
  WiFi.begin(state.wifiSsid.c_str(), state.wifiPass.c_str());

  unsigned long startWifi = millis();
  while (WiFi.status() != WL_CONNECTED && (millis() - startWifi < 10000UL)) {
    delay(250);
  }
  if (WiFi.status() == WL_CONNECTED) {
    Serial.printf("[WIFI] Terhubung ke %s (IP: %s)\n",
                  state.wifiSsid.c_str(), WiFi.localIP().toString().c_str());
    configTime(0, 0, NTP_SERVER_1, NTP_SERVER_2);
  } else {
    Serial.println("[WIFI] Belum terhubung, berjalan mode lokal + RTC/Fuzzy.");
  }

  pollPhysicalSwitches();
  readSensorData();
  evaluateFuzzyControl();
  updateIncubationCycle();
  updateOledDisplay();
}

void loop() {
  unsigned long nowMs = millis();

  // 1. Cek Push Button Start (GPIO 26) & Buzzer (GPIO 19)
  pollPhysicalSwitches();
  updateBuzzerAlarm();

  // 2. Refresh tampilan OLED 0.96" tiap 1 detik
  if (nowMs - lastOledMillis >= OLED_UPDATE_INTERVAL_MS) {
    lastOledMillis = nowMs;
    updateOledDisplay();
  }

  // 3. Baca Sensor SHT30, Hitung Logika Fuzzy, & Evaluasi Siklus 21 Hari tiap 2 detik
  if (nowMs - lastSensorMillis >= SENSOR_READ_INTERVAL_MS) {
    lastSensorMillis = nowMs;
    readSensorData();
    evaluateFuzzyControl();
    updateIncubationCycle();
  }

  // 4. Kirim Telemetri JSON & Tarik Perintah/Pengaturan dari Backend tiap 5 detik
  if (nowMs - lastTelemetryMillis >= TELEMETRY_INTERVAL_MS) {
    lastTelemetryMillis = nowMs;
    sendTelemetryAndPollBackend();
  }
}
