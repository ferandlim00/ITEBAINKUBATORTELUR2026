/**
 * ============================================================================
 * SMART HATCHERY MONITORING - FIRMWARE ESP32 DOIT DEVKIT V1 (30-PIN)
 * (Revisi Final: Tanpa Motor Stepper, 1 Servo MG996R, & 1 Push Button Start)
 * ============================================================================
 * Mapping Pin Hardware (Sesuai Dokumen Spesifikasi Final & Dual OLED):
 *   - GPIO 16 : Jalur SDA2 I2C-2 Khusus OLED-2 SSD1306 [0x3C] (Pin RX2)
 *   - GPIO 17 : Jalur SCL2 I2C-2 Khusus OLED-2 SSD1306 [0x3C] (Pin TX2)
 *   - GPIO 18 : Signal Motor Servo MG996R (1 Unit) -> Catu daya +5V TB-1512
 *   - GPIO 19 : Positif (+) Buzzer Aktif SFM-27
 *   - GPIO 21 : Jalur SDA1 I2C-1 (SHT30 [0x44], OLED-1 SSD1306 [0x3C], RTC DS3231 [0x68])
 *   - GPIO 22 : Jalur SCL1 I2C-1 (SHT30 [0x44], OLED-1 SSD1306 [0x3C], RTC DS3231 [0x68])
 *   - GPIO 25 : PWM / DIM Trigger RobotDyn AC Dimmer (Bohlam 60W)
 *   - GPIO 26 : Push Button Start Utama (INPUT_PULLUP, aktif LOW) + dukungan GPIO 4 (Simulasi)
 *   - GPIO 34 : Z-C (Zero-Cross Detector) RobotDyn AC Dimmer (Input Interrupt)
 *
 * ============================================================================
 * DAFTAR LENGKAP LIBRARY YANG DIBUTUHKAN DI ARDUINO IDE:
 * ============================================================================
 * A. Library Eksternal (WAJIB Diinstal via Tools > Manage Libraries...):
 *   1. ArduinoJson            (oleh Benoit Blanchon, versi 7.x)       -> Header: <ArduinoJson.h>
 *   2. Adafruit SHT31 Library (oleh Adafruit, versi 2.2.x+)           -> Header: <Adafruit_SHT31.h>
 *   3. Adafruit GFX Library   (oleh Adafruit, versi 1.11.x+)          -> Header: <Adafruit_GFX.h>
 *   4. Adafruit SSD1306       (oleh Adafruit, versi 2.5.x+)           -> Header: <Adafruit_SSD1306.h>
 *   5. Adafruit BusIO         (oleh Adafruit, dependensi wajib I2C)   -> Dipakai oleh SHT31, SSD1306, GFX, RTClib
 *   6. RTClib                 (oleh Adafruit, versi 2.1.x+)           -> Header: <RTClib.h>
 *   7. ESP32Servo             (oleh Kevin Harrington / John K. Bennett)-> Header: <ESP32Servo.h>
 *
 * B. Library Bawaan Core ESP32 (Otomatis Ada Saat Install Board "esp32 by Espressif Systems"):
 *   - <Arduino.h>, <Wire.h>, <WiFi.h>, <WiFiClientSecure.h>, <HTTPClient.h>,
 *     <WebServer.h>, <DNSServer.h>, <Preferences.h>, <time.h>,
 *     <esp_arduino_version.h>, <soc/gpio_struct.h>
 * ============================================================================
 */

#include <Arduino.h>
#include <esp_arduino_version.h>
#include <soc/gpio_struct.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <HTTPClient.h>
#include <WebServer.h>
#include <DNSServer.h>
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
#define PIN_I2C2_SDA        16  // RX2 - SDA Bus I2C-2 Khusus OLED-2 (0x3C)
#define PIN_I2C2_SCL        17  // TX2 - SCL Bus I2C-2 Khusus OLED-2 (0x3C)
#define PIN_SERVO_MG996R     18  // Signal Motor Servo MG996R (1 Unit)
#define PIN_BUZZER_SFM27    19  // Positif (+) Buzzer Aktif SFM-27
#define PIN_I2C_SDA         21  // SDA Bus I2C-1 (SHT30, OLED-1, RTC)
#define PIN_I2C_SCL         22  // SCL Bus I2C-1 (SHT30, OLED-1, RTC)
#define PIN_DIMMER_DIM      25  // Output Trigger Gate Triac RobotDyn AC Dimmer
#define PIN_BTN_START       26  // Push Button Start Hardware (INPUT_PULLUP)
#define PIN_BTN_START_ALT   4   // Push Button Start Alternatif/Simulasi (INPUT_PULLUP)
#define PIN_DIMMER_ZC       34  // Input Zero-Cross Detector RobotDyn AC Dimmer

// Lebar pulsa Servo MG996R (standar 50 Hz: 500 us = 0 deg, 2500 us = 180 deg)
// Menggunakan Hardware LEDC 14-bit langsung (Channel 2 / Timer 1) agar bebas bug double-attach ESP32Servo di Core 3.x.
// Catatan daya: MG996R menarik arus hingga ~2.5 A saat stall (4.8-6.6 V), WAJIB pakai
// catu +5V TB-1512 minimal 3 A + kapasitor 470-1000 uF + satukan GND adaptor servo ke GND ESP32.
#define SERVO_PULSE_MIN_US   500
#define SERVO_PULSE_MAX_US   2500
#define SERVO_LEDC_CHANNEL   2
#define SERVO_LEDC_BITS      14
#define BUZZER_LEDC_CHANNEL  4
#define BUZZER_LEDC_BITS     10

// Alamat I2C Perangkat (Agar Tidak Bentrok)
#define I2C_ADDR_SHT30      0x44
#define I2C_ADDR_OLED       0x3C
#define I2C_ADDR_RTC        0x68

// Dimensi Layar OLED 0.96" SSD1306
#define OLED_WIDTH          128
#define OLED_HEIGHT         64
#define OLED_RESET          -1

// ============================================================================
// 2. KONFIGURASI JARINGAN, HOTSPOT PORTAL, BACKEND GO FIBER, & IDENTITAS ALAT
// ============================================================================
const char* DEVICE_ID              = "inkubator-01";
const char* DEFAULT_WIFI_SSID      = "";
const char* DEFAULT_WIFI_PASS      = "";

// Konfigurasi Hotspot Captive Portal untuk setup Wi-Fi dari HP tanpa ubah kode
const char* AP_HOTSPOT_SSID        = "SmartHatchery-Setup";
const char* AP_HOTSPOT_PASS        = "12345678";
const IPAddress AP_LOCAL_IP(192, 168, 4, 1);
const IPAddress AP_GATEWAY_IP(192, 168, 4, 1);
const IPAddress AP_SUBNET_MASK(255, 255, 255, 0);

// Daftar URL Backend: Domain Web HTTPS (Utama), Jembatan Wi-Fi Laptop, & IP VPS Tailscale
const char* const BACKEND_DOMAIN_URL     = "https://app.itebaincubatortelur.cc.cd/api/device/telemetry";
const char* const BACKEND_FALLBACK_URL   = "http://192.168.100.4:3000/api/device/telemetry";
const char* const BACKEND_TELEMETRY_URL  = "http://100.102.108.69:3000/api/device/telemetry";
const char* const DEVICE_API_KEY         = "secret-device-token-esp32";

// Default utama saat ESP32 pertama kali dipakai (dapat diganti lewat Hotspot Portal)
const char* const DEFAULT_BACKEND_URL    = BACKEND_DOMAIN_URL;
const char* const DEFAULT_DEVICE_API_KEY = DEVICE_API_KEY;

const char* NTP_SERVER_1           = "pool.ntp.org";
const char* NTP_SERVER_2           = "id.pool.ntp.org";

// ============================================================================
// 3. BATAS KEAMANAN & KONSTANTA SISTEM (PRD V2.0 & KONTRAK API V2.1)
// ============================================================================
const float TEMP_SAFE_MIN             = 36.5f;   // Batas aman bawah (°C)
const float TEMP_SAFE_MAX             = 38.5f;   // Batas aman atas (°C)
const float TEMP_DEFAULT_TARGET       = 37.5f;   // Target suhu optimal default (°C)
const float HUMIDITY_WARN_MIN         = 45.0f;   // Batas pengaman kelembapan bawah fase Setter (%RH)
const float HUMIDITY_WARN_MAX         = 70.0f;   // Batas pengaman kelembapan atas fase Setter (%RH)

const int   LAMP_MANUAL_MAX_PCT       = 80;      // Batas aman bohlam 60W saat manual (80%)
const int   LAMP_AUTO_MAX_PCT         = 100;     // Batas bohlam saat Fuzzy Auto (100%)
const int   VENT_ANGLE_MIN            = 0;       // Servo tertutup penuh (0°)
const int   VENT_ANGLE_MAX            = 90;      // Servo terbuka penuh (90°)
const int   VENT_MODE_ANGLES[4]       = { 0, 15, 40, 90 }; // 0=Tutup, 1=Oksigen Minimal, 2=Sirkulasi/Lockdown, 3=Buang Panas
const float HUMIDITY_WARN_CLEAR_HYST  = 66.0f;   // Batas histeresis keluar dari mode buang lembap (%RH)
const float HUMIDITY_LOCKDOWN_MIN     = 60.0f;   // Batas pengaman kelembapan bawah fase Lockdown (%RH)
const float HUMIDITY_LOCKDOWN_MAX     = 78.0f;   // Batas kelembapan berlebih saat Lockdown (Hari 19-21)

// Parameter Filter Sensor, Adaptasi Dimmer Smooth, & Anti-Goyang Servo
const float TEMP_EMA_ALPHA            = 0.25f;   // Filter EMA suhu SHT30 (menghilangkan jitter 0.1 C)
const float HUM_EMA_ALPHA             = 0.20f;   // Filter EMA kelembapan SHT30
const int   TEMP_TREND_SAMPLES        = 5;       // Rentang tren laju suhu = 5 x 2 detik = 10 detik
const float LAMP_ADAPTIVE_KI          = 0.40f;   // Gain koreksi adaptif ringan agar dimmer menemukan titik anteng
const float LAMP_BIAS_MAX             = 35.0f;   // Batas koreksi adaptif ruangan (-35% s.d. +35%)
const float LAMP_SLEW_FINE_PCT        = 1.5f;    // Maks pergeseran dimmer per 2s saat dekat target (|e| <= 0.45 C)
const float LAMP_SLEW_COARSE_PCT      = 4.0f;    // Maks pergeseran dimmer per 2s saat pemanasan/pendinginan jauh
const unsigned long VENT_MODE_DWELL_MS = 15000UL; // Jeda minimal pindah mode normal servo (15 detik)

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
const unsigned long OLED_ANIM_INTERVAL_MS   = 250UL;   // Interval frame animasi OLED (4 fps)
const unsigned long WIFI_CONNECT_TIMEOUT_MS = 30000UL; // Timeout uji Wi-Fi baru 30 detik
const unsigned long BOOT_WIFI_TIMEOUT_MS    = 15000UL; // Timeout koneksi Wi-Fi saat boot
const unsigned long PORTAL_CONNECT_WAIT_MS  = 20000UL; // Timeout uji Wi-Fi baru di portal
const unsigned long PORTAL_AP_SHUTDOWN_MS   = 5000UL;  // Jeda matikan Hotspot setelah sukses
const unsigned long WIFI_ANIM_CONNECTED_MS  = 4500UL;  // Durasi animasi OLED berhasil konek
const unsigned long WIFI_ANIM_FAILED_MS     = 3000UL;  // Durasi animasi OLED gagal konek
const unsigned long WIFI_RECONNECT_FAIL_MS  = 30000UL; // Buka hotspot otomatis bila putus > 30 detik

// ============================================================================
// 4. OBJEK PERANGKAT KERAS
// ============================================================================
TwoWire           I2C_Dua = TwoWire(1);
Adafruit_SHT31    sht30   = Adafruit_SHT31();
Adafruit_SSD1306  oled(OLED_WIDTH, OLED_HEIGHT, &Wire, OLED_RESET);     // OLED-1 di Bus I2C-1 (GPIO 21/22)
Adafruit_SSD1306  oled2(OLED_WIDTH, OLED_HEIGHT, &I2C_Dua, OLED_RESET); // OLED-2 di Bus I2C-2 (GPIO 16/17)
RTC_DS3231        rtc;
Servo             mg996rServo;
Preferences       prefs;
WebServer         portalServer(80);
DNSServer         portalDns;

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
  float temperature         = 37.5f;
  float prevTemperature     = 37.5f;
  float filteredTemperature = 37.5f;
  float filteredHumidity    = 58.0f;
  float tempHistory[TEMP_TREND_SAMPLES] = { 37.5f, 37.5f, 37.5f, 37.5f, 37.5f };
  uint8_t tempHistCount     = 0;
  uint8_t tempHistIdx       = 0;
  bool  sensorFilterSeeded  = false;
  float targetTemperature   = TEMP_DEFAULT_TARGET;
  float humidity            = 58.0f;
  int   sensorFailStreak    = 0;
  bool  sensorError         = false;

  // Siklus Inkubasi 21 Hari
  String incubationStatus = "idle"; // "idle" | "running" | "finished"
  int    currentDay       = 0;      // 1..21 (0 jika idle)
  time_t startedAtEpoch   = 0;
  time_t finishAtEpoch    = 0;
  String stage            = "";     // "turning" | "lockdown" | ""

  // Lampu Pemanas (RobotDyn AC Dimmer - Bohlam 60W)
  String lampMode            = "auto"; // "auto" | "manual"
  int    lampIntensity       = 0;      // 0..100 (auto), 0..80 (manual)
  float  lampFloatPct        = 0.0f;   // Akumulator presisi float untuk pergeseran smooth
  float  lampIntegralBias    = 0.0f;   // Trim adaptif menyesuaikan suhu ruangan
  bool   lampSeedOnFirstAuto = true;
  bool   lampFuzzyActive     = true;

  // Ventilasi (Motor Servo MG996R GPIO 18 - 4 Mode Otomatis Bertahap / Manual)
  String ventMode                = "auto"; // "auto" | "manual"
  int    ventAngle               = 15;     // 0..90 derajat
  int    ventDiscreteMode        = 1;      // 0=Tutup(0°), 1=Oksigen(15°), 2=Sirkulasi(40°), 3=Darurat(90°)
  bool   ventHighHumidityLatched = false;
  bool   ventForceEvalImmediate  = true;
  bool   ventModeInitialized     = false;
  unsigned long lastVentModeChangeMs = 0;
  bool   ventFuzzyActive         = false;

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

  // Kredensial Wi-Fi & Konfigurasi Server Aktif (tersimpan di NVS)
  String wifiSsid          = DEFAULT_WIFI_SSID;
  String wifiPass          = DEFAULT_WIFI_PASS;
  String backendUrl        = DEFAULT_BACKEND_URL;
  String deviceApiKey      = DEFAULT_DEVICE_API_KEY;

  // Alert Komunikasi (setelah gagal ganti Wi-Fi & rollback)
  bool   commAlertPending  = false;
  String commAlertMessage  = "";

  // Pengaturan Buzzer Lagu Rock & Mute + Pengaturan Tampilan OLED (tersimpan di NVS)
  bool    buzzerMuted      = false; // true = alarm dibisukan (MUTE)
  uint8_t buzzerStyle      = 2;     // 0 = We Will Rock You, 1 = Smoke on the Water, 2 = Linkin Park - Numb (Default)
  uint8_t oledBrightness   = 2;     // 0 = Hemat (Redup), 1 = Sedang, 2 = Cerah Penuh
  bool    oledSwapScreens  = false; // true = tukar posisi tampilan OLED 1 <-> OLED 2

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

// Pertukaran data thread-safe antara Core 1 (loop kendali suhu) & Core 0 (HTTP/TLS non-blocking)
struct NetExchange {
  SemaphoreHandle_t mutex          = NULL;
  volatile bool     pauseRequested = false;
  volatile bool     inFlight       = false;
  bool              jobPending     = false;
  String            payload;
  String            urls[4];
  int               urlCount       = 0;
  String            apiKey;
  bool              resultReady    = false;
  bool              resultConnected = false;
  String            resultBody;
  int               resultHttpCode = -1;
} netBox;
TaskHandle_t netTaskHandle = NULL;
const unsigned long NET_FAILURE_BACKOFF_MS = 15000UL;

enum WifiUiPhase {
  WIFI_UI_NORMAL = 0,
  WIFI_UI_PORTAL,
  WIFI_UI_CONNECTING,
  WIFI_UI_CONNECTED_ANIM,
  WIFI_UI_FAILED_ANIM
};

bool          portalActive            = false;
bool          portalRoutesRegistered  = false;
bool          portalConnecting        = false;
bool          portalShutdownPending   = false;
unsigned long portalShutdownAtMs      = 0;
unsigned long wifiConnectStartMs      = 0;
unsigned long wifiDisconnectedSinceMs = 0;
WifiUiPhase   wifiUiPhase             = WIFI_UI_NORMAL;
unsigned long wifiAnimStartMs         = 0;
uint8_t       wifiAnimFrame           = 0;
String        pendingWifiSsid         = "";
String        pendingWifiPass         = "";
String        pendingBackendUrl       = "";
String        pendingApiKey           = "";
String        portalStatusText        = "idle";
String        portalStatusMsg         = "";
String        connectedSsidDisplay    = "";
String        connectedIpDisplay      = "";

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
  state.backendUrl           = prefs.getString("backend_url", DEFAULT_BACKEND_URL);
  state.deviceApiKey         = prefs.getString("api_key", DEFAULT_DEVICE_API_KEY);
  if (state.backendUrl.isEmpty() ||
      state.backendUrl == "https://itebaincubatortelur.cc.cd/api/device/telemetry" ||
      state.backendUrl == "https://aapanel.itebaincubatortelur.cc.cd/api/device/telemetry") {
    state.backendUrl = DEFAULT_BACKEND_URL;
  }
  if (state.deviceApiKey.isEmpty()) state.deviceApiKey = DEFAULT_DEVICE_API_KEY;
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
  state.ventAngle            = prefs.getInt("vent_ang", VENT_MODE_ANGLES[1]);
  state.buzzerMuted          = prefs.getBool("bz_mute", false);
  state.buzzerStyle          = (uint8_t)constrain((int)prefs.getUChar("bz_style", 2), 0, 2);
  state.oledBrightness       = (uint8_t)constrain((int)prefs.getUChar("oled_br", 2), 0, 2);
  state.oledSwapScreens      = prefs.getBool("oled_swap", false);
  prefs.end();

  state.lampFloatPct        = (float)state.lampIntensity;
  state.lampSeedOnFirstAuto = true;
  state.lampFuzzyActive     = (state.lampMode == "auto");
  state.ventFuzzyActive     = false; // Ventilasi otomatis memakai 4 mode bertahap
  if (state.ventAngle <= 5)       state.ventDiscreteMode = 0;
  else if (state.ventAngle <= 25) state.ventDiscreteMode = 1;
  else if (state.ventAngle <= 65) state.ventDiscreteMode = 2;
  else                            state.ventDiscreteMode = 3;
}

void saveSettingsToNVS() {
  // Jeda interrupt dimmer sejenak saat NVS menulis ke Flash
  dimmerZCPaused = true;
  GPIO.out_w1tc = (1UL << PIN_DIMMER_DIM);

  prefs.begin("hatchery", false);
  prefs.putString("wifi_ssid", state.wifiSsid);
  prefs.putString("wifi_pass", state.wifiPass);
  prefs.putString("backend_url", state.backendUrl);
  prefs.putString("api_key", state.deviceApiKey);
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
  prefs.putBool("bz_mute", state.buzzerMuted);
  prefs.putUChar("bz_style", state.buzzerStyle);
  prefs.putUChar("oled_br", state.oledBrightness);
  prefs.putBool("oled_swap", state.oledSwapScreens);
  prefs.end();

  dimmerZCPaused = false;
}

// ============================================================================
// 9. AKTUATOR: SERVO MG996R LEDC 14-BIT (GPIO 18), BUZZER LAGU ROCK + MUTE (GPIO 19)
// ============================================================================
bool          servoHwReady        = false;
int           lastServoHwAngle    = -1;
bool          servoTestActive     = false;
unsigned long servoTestStartMs    = 0;
unsigned long lastTurnSweepStepMs = 0;
int           turnSweepAngle      = 0;
int           turnSweepDir        = 1;

bool initServoHardware() {
#if ESP_ARDUINO_VERSION >= ESP_ARDUINO_VERSION_VAL(3, 0, 0)
  ledcDetach(PIN_SERVO_MG996R);
  servoHwReady = ledcAttachChannel(PIN_SERVO_MG996R, 50, SERVO_LEDC_BITS, SERVO_LEDC_CHANNEL);
#else
  double f = ledcSetup(SERVO_LEDC_CHANNEL, 50, SERVO_LEDC_BITS);
  ledcAttachPin(PIN_SERVO_MG996R, SERVO_LEDC_CHANNEL);
  servoHwReady = (f > 0.0);
#endif
  Serial.printf("[SERVO] Hardware LEDC 14-bit (50Hz, Ch %d) pada GPIO %d -> %s\n",
                SERVO_LEDC_CHANNEL, PIN_SERVO_MG996R, servoHwReady ? "OK" : "GAGAL");
  return servoHwReady;
}

void writeServoRawAngle(int angleDeg) {
  if (!servoHwReady) {
    initServoHardware();
  }
  int clamped = constrain(angleDeg, 0, 180);
  int pulseUs = map(clamped, 0, 180, SERVO_PULSE_MIN_US, SERVO_PULSE_MAX_US);
  uint32_t maxDuty = (1UL << SERVO_LEDC_BITS) - 1UL; // 16383
  uint32_t duty    = (uint32_t)(((uint64_t)pulseUs * maxDuty + 10000ULL) / 20000ULL);

#if ESP_ARDUINO_VERSION >= ESP_ARDUINO_VERSION_VAL(3, 0, 0)
  ledcWrite(PIN_SERVO_MG996R, duty);
#else
  ledcWrite(SERVO_LEDC_CHANNEL, duty);
#endif
  lastServoHwAngle = clamped;
}

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
  bool changed = (state.ventAngle != clamped);
  state.ventAngle = clamped;
  // Jangan timpa gerakan fisik servo bila sedang siklus balik telur atau tes servo
  if (!state.motorRunning && !servoTestActive) {
    writeServoRawAngle(clamped);
  }
  if (changed) Serial.printf("[SERVO] Ventilasi -> %d deg (mode=%s)\n", clamped, state.ventMode.c_str());
}

void startServoHardwareTest() {
  servoTestActive  = true;
  servoTestStartMs = millis();
  writeServoRawAngle(0);
  Serial.println("[SERVO] Tes gerak MG996R dimulai (0 -> 45 -> 90 -> 0 -> kembali)");
}

void startTurningCycle() {
  state.motorRunning     = true;
  state.turningStatus    = "turning";
  state.motorTurnStartMs = millis();
  lastTurnSweepStepMs    = millis();
  turnSweepAngle         = constrain(state.ventAngle, 0, 90);
  turnSweepDir           = (turnSweepAngle < 45) ? 1 : -1;
  Serial.printf("[TURNING] Siklus balik telur + gerak servo MG996R aktif selama %d detik\n",
                state.motorDurationSeconds);
}

void stopTurningCycle(bool completedNormally) {
  state.motorRunning = false;
  writeServoRawAngle(state.ventAngle); // Kembalikan ke sudut bukaan ventilasi aktif
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

void updateServoMotions() {
  unsigned long nowMs = millis();

  // 1. Mode Tes Gerak Servo dari Menu OLED (durasi 3.2 detik: 0 -> 45 -> 90 -> 0 -> ventAngle)
  if (servoTestActive) {
    unsigned long el = nowMs - servoTestStartMs;
    if (el < 800UL) {
      if (lastServoHwAngle != 0) writeServoRawAngle(0);
    } else if (el < 1600UL) {
      if (lastServoHwAngle != 45) writeServoRawAngle(45);
    } else if (el < 2400UL) {
      if (lastServoHwAngle != 90) writeServoRawAngle(90);
    } else if (el < 3200UL) {
      if (lastServoHwAngle != 0) writeServoRawAngle(0);
    } else {
      servoTestActive = false;
      writeServoRawAngle(state.ventAngle);
      Serial.println("[SERVO] Tes gerak MG996R selesai.");
    }
    return;
  }

  // 2. Gerak Ayun Halus Servo MG996R Saat Siklus Pembalikan Telur ("Balik sekarang" / Jadwal)
  if (state.motorRunning) {
    unsigned long stepIntervalMs = (unsigned long)map(constrain(state.motorSpeedPercent, 10, 70), 10, 70, 55, 18);
    if (nowMs - lastTurnSweepStepMs >= stepIntervalMs) {
      lastTurnSweepStepMs = nowMs;
      turnSweepAngle += turnSweepDir * 3;
      if (turnSweepAngle >= 90) {
        turnSweepAngle = 90;
        turnSweepDir   = -1;
      } else if (turnSweepAngle <= 0) {
        turnSweepAngle = 0;
        turnSweepDir   = 1;
      }
      writeServoRawAngle(turnSweepAngle);
    }
  }
}

// --- ENGINE BUZZER LAGU ROCK NON-BLOCKING + FITUR MUTE ---
struct RockStep {
  uint16_t freqHz; // Frekuensi PWM (dipakai pada mode Rock Nada; mode Rock Beat memakai DC HIGH murni)
  uint16_t onMs;   // Durasi bunyi (ms)
  uint16_t offMs;  // Jeda antar ketukan (ms)
};

// Lagu Rock 1 (Default): Queen - "We Will Rock You" (DUM - DUM - TAK!) + Rock Riff Syncopation
// Paling keras, tegas, dan langsung dikenali pada Buzzer Aktif SFM-27 maupun Buzzer Pasif.
const RockStep ROCK_SONG_BEAT[] = {
  // Bar 1: DUM - DUM - TAK!!
  { 196, 150,  90 }, { 196, 150,  90 }, { 392, 340, 360 },
  // Bar 2: DUM - DUM - TAK!!
  { 196, 150,  90 }, { 196, 150,  90 }, { 392, 340, 240 },
  // Bar 3: Guitar Riff Cepat (da-da-DAA! da-da-DA-DAA!)
  { 330, 115,  65 }, { 392, 115,  65 }, { 440, 230, 110 },
  { 330, 115,  65 }, { 392, 115,  65 }, { 466, 120,  55 }, { 440, 300, 520 }
};
const uint8_t ROCK_SONG_BEAT_LEN = sizeof(ROCK_SONG_BEAT) / sizeof(ROCK_SONG_BEAT[0]);

// Lagu Rock 2: Deep Purple - "Smoke on the Water" Riff (0 - 3 - 5 | 0 - 3 - 6 - 5 | 0 - 3 - 5 - 3 - 0)
const RockStep ROCK_SONG_TONE[] = {
  { 330, 220,  80 }, { 392, 220,  80 }, { 440, 360, 180 },
  { 330, 220,  80 }, { 392, 220,  80 }, { 466, 170,  60 }, { 440, 360, 180 },
  { 330, 220,  80 }, { 392, 220,  80 }, { 440, 320,  90 }, { 392, 220,  80 }, { 330, 420, 500 }
};
const uint8_t ROCK_SONG_TONE_LEN = sizeof(ROCK_SONG_TONE) / sizeof(ROCK_SONG_TONE[0]);

// Lagu Rock 3 (Default): Linkin Park - "Numb" (Intro Synth Riff F#m + Chorus "I've become so numb")
const RockStep ROCK_SONG_NUMB[] = {
  // Intro Bar 1-2: C#5 - E5 - C#5 - F#5 - A5 - G#5
  { 554, 240,  40 }, { 659, 240,  40 }, { 554, 240,  40 },
  { 740, 500,  60 }, { 880, 500,  60 }, { 831, 740, 260 },
  // Intro Bar 3-4: C#5 - E5 - C#5 - A5 - G#5 - E5
  { 554, 240,  40 }, { 659, 240,  40 }, { 554, 240,  40 },
  { 880, 500,  60 }, { 831, 500,  60 }, { 659, 760, 280 },
  // Chorus: "I've be-come so numb" (A5 - G#5 - F#5 - E5 - F#5)
  { 880, 240,  40 }, { 831, 240,  40 }, { 740, 260,  40 }, { 659, 260,  40 }, { 740, 620, 200 },
  // Chorus: "I can't feel you there" (F#5 - G#5 - A5 - E5 - C#5)
  { 740, 240,  40 }, { 831, 240,  40 }, { 880, 260,  40 }, { 659, 280,  40 }, { 554, 700, 520 }
};
const uint8_t ROCK_SONG_NUMB_LEN = sizeof(ROCK_SONG_NUMB) / sizeof(ROCK_SONG_NUMB[0]);

// Jingle pendek konfirmasi aksi tombol (non-blocking)
const RockStep CUE_START_RIFF[] = {
  { 330, 95, 45 }, { 392, 95, 45 }, { 440, 95, 45 }, { 587, 280, 80 }
};
const RockStep CUE_STOP_RIFF[] = {
  { 440, 130, 60 }, { 330, 130, 60 }, { 220, 260, 80 }
};
const RockStep CUE_WIFI_RIFF[] = {
  { 392, 85, 50 }, { 523, 160, 60 }
};
const RockStep CUE_CLICK_TICK[] = {
  { 523, 24, 20 }
};

enum BuzzerCueType {
  BZ_CUE_NONE = 0,
  BZ_CUE_CLICK,
  BZ_CUE_START,
  BZ_CUE_STOP,
  BZ_CUE_WIFI,
  BZ_CUE_PREVIEW_ROCK
};

bool          buzzerPwmAttached  = false;
uint8_t       activeBuzzerCue    = BZ_CUE_NONE;
uint8_t       buzzerStepIdx      = 0;
bool          buzzerInOnPhase    = false;
unsigned long buzzerPhaseStartMs = 0;
bool          alarmWasActive     = false;

void buzzerHwOff() {
  if (buzzerPwmAttached) {
#if ESP_ARDUINO_VERSION >= ESP_ARDUINO_VERSION_VAL(3, 0, 0)
    ledcWrite(PIN_BUZZER_SFM27, 0);
    ledcDetach(PIN_BUZZER_SFM27);
#else
    ledcWrite(BUZZER_LEDC_CHANNEL, 0);
    ledcDetachPin(PIN_BUZZER_SFM27);
#endif
    buzzerPwmAttached = false;
    pinMode(PIN_BUZZER_SFM27, OUTPUT);
  }
  digitalWrite(PIN_BUZZER_SFM27, LOW);
  buzzerPinState = false;
}

void buzzerHwOn(uint16_t freqHz, bool useTonePwm) {
  if (useTonePwm && freqHz > 0) {
#if ESP_ARDUINO_VERSION >= ESP_ARDUINO_VERSION_VAL(3, 0, 0)
    if (buzzerPwmAttached) {
      ledcDetach(PIN_BUZZER_SFM27);
    }
    buzzerPwmAttached = ledcAttachChannel(PIN_BUZZER_SFM27, freqHz, BUZZER_LEDC_BITS, BUZZER_LEDC_CHANNEL);
    if (buzzerPwmAttached) {
      ledcWrite(PIN_BUZZER_SFM27, 1UL << (BUZZER_LEDC_BITS - 1)); // 50% duty square wave
    } else {
      pinMode(PIN_BUZZER_SFM27, OUTPUT);
      digitalWrite(PIN_BUZZER_SFM27, HIGH);
    }
#else
    ledcSetup(BUZZER_LEDC_CHANNEL, freqHz, BUZZER_LEDC_BITS);
    if (!buzzerPwmAttached) {
      ledcAttachPin(PIN_BUZZER_SFM27, BUZZER_LEDC_CHANNEL);
      buzzerPwmAttached = true;
    }
    ledcWrite(BUZZER_LEDC_CHANNEL, 1UL << (BUZZER_LEDC_BITS - 1));
#endif
  } else {
    if (buzzerPwmAttached) buzzerHwOff();
    digitalWrite(PIN_BUZZER_SFM27, HIGH);
  }
  buzzerPinState = true;
}

void triggerBuzzerCue(uint8_t cue) {
  if (state.buzzerMuted && cue != BZ_CUE_PREVIEW_ROCK) {
    return;
  }
  activeBuzzerCue    = cue;
  buzzerStepIdx      = 0;
  buzzerInOnPhase    = false;
  buzzerPhaseStartMs = 0;
  buzzerHwOff();
}

void setBuzzerMuted(bool muted) {
  state.buzzerMuted = muted;
  if (muted) {
    activeBuzzerCue = BZ_CUE_NONE;
    buzzerHwOff();
  }
  saveSettingsToNVS();
  Serial.printf("[BUZZER] Status Mute -> %s\n", muted ? "AKTIF (SENYAP)" : "BUNYI (ROCK)");
}

bool isAlarmConditionActive() {
  bool isTempAlarm = (state.temperature > TEMP_SAFE_MAX || state.temperature < TEMP_SAFE_MIN);
  return isTempAlarm || state.sensorError;
}

void updateBuzzerAlarm() {
  unsigned long nowMs = millis();
  const RockStep* activeSong = ROCK_SONG_BEAT;
  uint8_t activeSongLen = ROCK_SONG_BEAT_LEN;
  bool useTone = false;
  if (state.buzzerStyle == 2) {
    activeSong    = ROCK_SONG_NUMB;
    activeSongLen = ROCK_SONG_NUMB_LEN;
    useTone       = true;
  } else if (state.buzzerStyle == 1) {
    activeSong    = ROCK_SONG_TONE;
    activeSongLen = ROCK_SONG_TONE_LEN;
    useTone       = true;
  }

  // 1. Prioritas Cue / Preview Lagu Rock (berjalan sekali sampai selesai)
  if (activeBuzzerCue != BZ_CUE_NONE) {
    const RockStep* seq = nullptr;
    uint8_t seqLen = 0;
    bool cueTone = useTone;
    switch (activeBuzzerCue) {
      case BZ_CUE_CLICK:        seq = CUE_CLICK_TICK; seqLen = 1; cueTone = false; break;
      case BZ_CUE_START:        seq = CUE_START_RIFF; seqLen = 4; break;
      case BZ_CUE_STOP:         seq = CUE_STOP_RIFF;  seqLen = 3; break;
      case BZ_CUE_WIFI:         seq = CUE_WIFI_RIFF;  seqLen = 2; break;
      case BZ_CUE_PREVIEW_ROCK: seq = activeSong;     seqLen = activeSongLen; break;
      default: break;
    }

    if (seq == nullptr || buzzerStepIdx >= seqLen) {
      activeBuzzerCue = BZ_CUE_NONE;
      buzzerHwOff();
      return;
    }

    if (buzzerPhaseStartMs == 0) {
      buzzerInOnPhase    = true;
      buzzerPhaseStartMs = nowMs;
      buzzerHwOn(seq[buzzerStepIdx].freqHz, cueTone);
      return;
    }

    unsigned long dur = buzzerInOnPhase ? seq[buzzerStepIdx].onMs : seq[buzzerStepIdx].offMs;
    if (nowMs - buzzerPhaseStartMs >= dur) {
      buzzerPhaseStartMs = nowMs;
      if (buzzerInOnPhase) {
        buzzerInOnPhase = false;
        buzzerHwOff();
      } else {
        buzzerStepIdx++;
        if (buzzerStepIdx >= seqLen) {
          activeBuzzerCue = BZ_CUE_NONE;
          buzzerHwOff();
        } else {
          buzzerInOnPhase = true;
          buzzerHwOn(seq[buzzerStepIdx].freqHz, cueTone);
        }
      }
    }
    return;
  }

  // 2. Evaluasi Alarm Suhu / Sensor dengan Irama Lagu Rock + Dukungan MUTE
  bool isCritical = isAlarmConditionActive();
  if (!isCritical || state.buzzerMuted) {
    if (buzzerPinState || buzzerPwmAttached || alarmWasActive) {
      buzzerHwOff();
      alarmWasActive     = false;
      buzzerStepIdx      = 0;
      buzzerInOnPhase    = false;
      buzzerPhaseStartMs = 0;
    }
    return;
  }

  if (!alarmWasActive) {
    alarmWasActive     = true;
    buzzerStepIdx      = 0;
    buzzerInOnPhase    = true;
    buzzerPhaseStartMs = nowMs;
    buzzerHwOn(activeSong[0].freqHz, useTone);
    return;
  }

  unsigned long waitMs = buzzerInOnPhase ? activeSong[buzzerStepIdx].onMs : activeSong[buzzerStepIdx].offMs;
  if (nowMs - buzzerPhaseStartMs >= waitMs) {
    buzzerPhaseStartMs = nowMs;
    if (buzzerInOnPhase) {
      buzzerInOnPhase = false;
      buzzerHwOff();
    } else {
      buzzerStepIdx   = (buzzerStepIdx + 1) % activeSongLen;
      buzzerInOnPhase = true;
      buzzerHwOn(activeSong[buzzerStepIdx].freqHz, useTone);
    }
  }
}

// ============================================================================
// 10. KENDALI TERPADU: FUZZY MAMDANI SMOOTH (DIMMER) & 4-MODE SERVO VENTILASI
// ============================================================================
// A. Lampu Pemanas (AC Dimmer): Fuzzy Mamdani (2 Input -> 1 Output) + Filter EMA
//    + Trim Adaptif Ruangan + Pergeseran Halus (Slew-Rate Limiter +/-1.5%..4%).
// B. Ventilasi (Servo MG996R): 4 Mode Posisi Tetap + Histeresis + Jeda 15 detik
//    (0° Tutup, 15° Oksigen Minimal, 40° Sirkulasi/Lockdown/Lembap, 90° Buang Panas).

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

enum VentDiscreteMode {
  VENT_MODE_TUTUP     = 0, // 0°  : Tutup saat pemanasan awal (Setter saja)
  VENT_MODE_OKSIGEN   = 1, // 15° : Bukaan minimal suplai O2/CO2 harian (hemat panas & lembap)
  VENT_MODE_SIRKULASI = 2, // 40° : Lockdown (Hari 19-21), hangat ringan, atau kelembapan > 70%
  VENT_MODE_DARURAT   = 3  // 90° : Buang panas berlebih / darurat
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

// Kurva Keanggotaan Output Lampu Pemanas (0% - 100%)
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

// Defuzzifikasi Mamdani Centroid (Center of Area) untuk Lampu (Domain 0..100%)
float defuzzifyCentroidLamp(const float clipLevel[5]) {
  float num = 0.0f;
  float den = 0.0f;
  for (int x = 0; x <= 100; x++) {
    float xf = (float)x;
    float muAgg = 0.0f;
    for (int k = 0; k < 5; k++) {
      if (clipLevel[k] > 0.0f) {
        float clipped = fminf(clipLevel[k], muOutputLamp(k, xf));
        muAgg = fmaxf(muAgg, clipped);
      }
    }
    num += xf * muAgg;
    den += muAgg;
  }
  return (den > 0.0001f) ? (num / den) : 50.0f;
}

bool isIncubationLockdownPhase() {
  return (state.stage == "lockdown") ||
         (state.incubationStatus == "running" && state.currentDay >= INCUBATION_LOCKDOWN_START);
}

void getActiveHumidityBounds(float& outMin, float& outMax) {
  if (isIncubationLockdownPhase()) {
    outMin = HUMIDITY_LOCKDOWN_MIN;
    outMax = HUMIDITY_LOCKDOWN_MAX;
  } else {
    outMin = HUMIDITY_WARN_MIN;
    outMax = HUMIDITY_WARN_MAX;
  }
}

void evaluateVentilationModes(float e, bool isLockdown) {
  if (state.ventMode != "auto") {
    state.ventFuzzyActive = false;
    return;
  }
  state.ventFuzzyActive = false; // Otomatis memakai 4 mode bertahap agar servo tidak goyang

  float T = state.filteredTemperature;
  float H = state.filteredHumidity;

  // Histeresis kelembapan tinggi (Setter > 70% masuk, < 66% keluar; Lockdown > 78% masuk, < 74% keluar)
  float humHighOn  = isLockdown ? HUMIDITY_LOCKDOWN_MAX : HUMIDITY_WARN_MAX;
  float humHighOff = humHighOn - 4.0f;
  if (H > humHighOn) {
    state.ventHighHumidityLatched = true;
  } else if (H < humHighOff) {
    state.ventHighHumidityLatched = false;
  }

  int currentMode = constrain(state.ventDiscreteMode, 0, 3);
  int desiredMode = currentMode;

  bool thermalEmergency = (T >= TEMP_SAFE_MAX) || (state.temperature >= TEMP_SAFE_MAX) || (e >= 0.70f);
  bool coldEmergency    = (T <= TEMP_SAFE_MIN) || (state.temperature <= TEMP_SAFE_MIN);

  if (thermalEmergency) {
    desiredMode = VENT_MODE_DARURAT;
  } else if (coldEmergency) {
    // Saat Lockdown, jangan pernah tutup 0° agar embrio yang mulai bernapas paru-paru tetap mendapat O2
    desiredMode = isLockdown ? VENT_MODE_OKSIGEN : VENT_MODE_TUTUP;
  } else if (currentMode == VENT_MODE_DARURAT) {
    desiredMode = (e <= 0.45f && T <= TEMP_SAFE_MAX - 0.25f) ? VENT_MODE_SIRKULASI : VENT_MODE_DARURAT;
  } else if (isLockdown) {
    // Fase Lockdown (Hari 19-21): bukaan dasar naik ke Mode 2 (40°) untuk kebutuhan O2 puncak
    if (e < -0.70f) {
      desiredMode = VENT_MODE_OKSIGEN;
    } else if (currentMode <= VENT_MODE_OKSIGEN && e < -0.40f) {
      desiredMode = VENT_MODE_OKSIGEN;
    } else {
      desiredMode = VENT_MODE_SIRKULASI;
    }
  } else {
    // Fase Setter (Hari 1-18) atau Idle: fokus menjaga O2 minimal (15°) agar dimmer anteng
    bool needSirkulasi = (e >= 0.30f) || (state.ventHighHumidityLatched && e >= -0.30f);
    bool keepSirkulasi = (e >  0.12f) || (state.ventHighHumidityLatched && e >= -0.35f);
    bool needTutup     = (e <= -0.60f);
    bool keepTutup     = (e <  -0.25f);

    if (needSirkulasi || (currentMode == VENT_MODE_SIRKULASI && keepSirkulasi)) {
      desiredMode = VENT_MODE_SIRKULASI;
    } else if (needTutup || (currentMode == VENT_MODE_TUTUP && keepTutup)) {
      desiredMode = VENT_MODE_TUTUP;
    } else {
      desiredMode = VENT_MODE_OKSIGEN;
    }
  }

  unsigned long nowMs = millis();
  bool bypassDwell = !state.ventModeInitialized ||
                     state.ventForceEvalImmediate ||
                     thermalEmergency ||
                     coldEmergency;

  if (desiredMode != currentMode) {
    if (bypassDwell || (nowMs - state.lastVentModeChangeMs >= VENT_MODE_DWELL_MS)) {
      state.ventDiscreteMode     = desiredMode;
      state.lastVentModeChangeMs = nowMs;
      state.ventModeInitialized  = true;
      applyVentilationOutput(VENT_MODE_ANGLES[desiredMode]);
      Serial.printf("[VENT] Pindah ke Mode %d (%d deg) | e=%.2fC H=%.0f%% Stage=%s\n",
                    desiredMode, VENT_MODE_ANGLES[desiredMode], e, H,
                    isLockdown ? "LOCKDOWN" : "SETTER");
    }
  } else if (!state.ventModeInitialized || state.ventForceEvalImmediate ||
             state.ventAngle != VENT_MODE_ANGLES[currentMode]) {
    state.lastVentModeChangeMs = nowMs;
    state.ventModeInitialized  = true;
    applyVentilationOutput(VENT_MODE_ANGLES[currentMode]);
  }
  state.ventForceEvalImmediate = false;
}

void evaluateFuzzyControl() {
  float e  = state.filteredTemperature - state.targetTemperature;
  float de = 0.0f;
  if (state.tempHistCount > 1) {
    uint8_t oldestIdx = (state.tempHistCount < TEMP_TREND_SAMPLES)
                        ? 0
                        : (state.tempHistIdx % TEMP_TREND_SAMPLES);
    de = state.filteredTemperature - state.tempHistory[oldestIdx];
  }

  // 1. FUZZIFIKASI INPUT 1: Error Suhu (e) -> pita normal rapat +/-0.35 C
  float muError[3];
  muError[ERR_DINGIN] = fuzzyLeftShoulder(e, -0.80f, -0.05f);
  muError[ERR_NORMAL] = fuzzyTriangle(e, -0.35f, 0.0f, 0.35f);
  muError[ERR_PANAS]  = fuzzyRightShoulder(e, 0.05f, 0.80f);

  // 2. FUZZIFIKASI INPUT 2: Perubahan Suhu 10 detik terakhir (de) -> mencegah kaget jitter 2s
  float muDelta[3];
  muDelta[DERR_TURUN]  = fuzzyLeftShoulder(de, -0.18f, -0.02f);
  muDelta[DERR_STABIL] = fuzzyTriangle(de, -0.10f, 0.0f, 0.10f);
  muDelta[DERR_NAIK]   = fuzzyRightShoulder(de, 0.02f, 0.18f);

  // 3. BASIS ATURAN MAMDANI LAMP (3x3 = 9 Aturan IF-THEN Linguistik)
  const FuzzyLampSet lampRules[3][3] = {
    // Suhu DINGIN (de: Turun, Stabil, Naik)
    { LAMP_SANGAT_TERANG, LAMP_SANGAT_TERANG, LAMP_TERANG },
    // Suhu NORMAL / IDEAL (de: Turun, Stabil, Naik)
    { LAMP_TERANG,        LAMP_SEDANG,        LAMP_REDUP },
    // Suhu PANAS (de: Turun, Stabil, Naik)
    { LAMP_REDUP,         LAMP_MATI,          LAMP_MATI }
  };

  float clipLamp[5] = { 0.0f, 0.0f, 0.0f, 0.0f, 0.0f };
  for (int i = 0; i < 3; i++) {
    for (int j = 0; j < 3; j++) {
      float alpha = fminf(muError[i], muDelta[j]);
      if (alpha > 0.0f) {
        FuzzyLampSet outL = lampRules[i][j];
        clipLamp[outL] = fmaxf(clipLamp[outL], alpha);
      }
    }
  }

  // 4. DEFUZZIFIKASI CENTROID UNTUK LAMPU + TRIM ADAPTIF + SLEW-RATE HALUS
  float crispLamp = defuzzifyCentroidLamp(clipLamp);

  if (state.lampMode == "auto") {
    state.lampFuzzyActive = true;

    if (state.filteredTemperature >= TEMP_SAFE_MAX || state.temperature >= TEMP_SAFE_MAX) {
      state.lampFloatPct        = 0.0f;
      state.lampSeedOnFirstAuto = false;
      applyLampOutput(0);
    } else if (state.filteredTemperature <= TEMP_SAFE_MIN || state.temperature <= TEMP_SAFE_MIN) {
      state.lampFloatPct        = (float)LAMP_AUTO_MAX_PCT;
      state.lampSeedOnFirstAuto = false;
      applyLampOutput(LAMP_AUTO_MAX_PCT);
    } else {
      if (state.lampSeedOnFirstAuto) {
        state.lampFloatPct        = crispLamp;
        state.lampSeedOnFirstAuto = false;
      }

      // Koreksi adaptif ringan agar titik seimbang dimmer menyesuaikan kondisi ruangan (anti-windup)
      if (!state.sensorError && fabs(e) <= 1.20f && fabs(e) >= 0.03f) {
        float prevTarget = crispLamp + state.lampIntegralBias;
        bool windupUp    = (prevTarget >= (float)LAMP_AUTO_MAX_PCT) && (e < 0.0f);
        bool windupDown  = (prevTarget <= 0.0f) && (e > 0.0f);
        if (!windupUp && !windupDown) {
          state.lampIntegralBias = constrain(
            state.lampIntegralBias + (-e * LAMP_ADAPTIVE_KI),
            -LAMP_BIAS_MAX,
            LAMP_BIAS_MAX
          );
        }
      }

      float targetLamp   = constrain(crispLamp + state.lampIntegralBias, 0.0f, (float)LAMP_AUTO_MAX_PCT);
      float maxStep      = (fabs(e) <= 0.45f) ? LAMP_SLEW_FINE_PCT : LAMP_SLEW_COARSE_PCT;
      float deltaLamp    = constrain(targetLamp - state.lampFloatPct, -maxStep, maxStep);
      state.lampFloatPct = constrain(state.lampFloatPct + deltaLamp, 0.0f, (float)LAMP_AUTO_MAX_PCT);
      applyLampOutput((int)roundf(state.lampFloatPct));
    }
  } else {
    state.lampFuzzyActive     = false;
    state.lampFloatPct        = (float)state.lampIntensity;
    state.lampSeedOnFirstAuto = false;
  }

  // 5. EVALUASI 4 MODE BERTAHAP SERVO VENTILASI
  bool isLockdown = isIncubationLockdownPhase();
  evaluateVentilationModes(e, isLockdown);
}

// ============================================================================
// 11. TOMBOL INTERAKTIF, VERIFIKASI 2 LANGKAH (MULAI/BERHENTI), & MENU PENGATURAN OLED
// ============================================================================
void startWifiPortal();
void stopWifiPortal();
void updateOledDisplay();

enum LocalUiMode {
  LOCAL_UI_HOME = 0,          // Tampilan pemantauan normal
  LOCAL_UI_MENU,              // Menu Pengaturan OLED & Kontrol Alat
  LOCAL_UI_CONFIRM_START,     // Verifikasi Langkah ke-2: Konfirmasi Mulai Inkubasi
  LOCAL_UI_CONFIRM_STOP,      // Verifikasi Langkah ke-2: Konfirmasi Hentikan Inkubasi
  LOCAL_UI_TOAST              // Notifikasi visual aksi berhasil/batal (1.8 detik)
};

const unsigned long BTN_HOLD_CONFIRM_MS = 850UL;   // Tahan >= 0.85 detik = OK / Pilih
const unsigned long LOCAL_UI_TIMEOUT_MS = 15000UL; // Otomatis tutup menu bila diam 15 detik
const unsigned long LOCAL_TOAST_MS      = 1800UL;  // Durasi banner notifikasi OLED
const uint8_t       OLED_MENU_COUNT     = 8;

LocalUiMode   localUiMode       = LOCAL_UI_HOME;
uint8_t       oledMenuCursor    = 0;
uint8_t       confirmChoiceIdx  = 0; // 0 = [ BATAL ] (default aman), 1 = [ YA, LANJUT ]
unsigned long localUiLastActMs  = 0;
unsigned long localToastStartMs = 0;
String        toastTitle        = "";
String        toastSubtitle     = "";

void applyOledContrastSettings() {
  uint8_t contrast = 0xFF;
  if (state.oledBrightness == 0)      contrast = 0x12; // Hemat / Redup
  else if (state.oledBrightness == 1) contrast = 0x7F; // Sedang
  else                                contrast = 0xFF; // Cerah Penuh

  if (state.oledAvailable) {
    oled.ssd1306_command(SSD1306_SETCONTRAST);
    oled.ssd1306_command(contrast);
  }
  if (state.oled2Available) {
    oled2.ssd1306_command(SSD1306_SETCONTRAST);
    oled2.ssd1306_command(contrast);
  }
}

void showOledToast(const String& title, const String& subtitle) {
  toastTitle        = title;
  toastSubtitle     = subtitle;
  localUiMode       = LOCAL_UI_TOAST;
  localToastStartMs = millis();
  localUiLastActMs  = millis();
}

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
  state.ventForceEvalImmediate = true;
  saveSettingsToNVS();
  triggerBuzzerCue(BZ_CUE_START);
  showOledToast("SIKLUS DIMULAI", "DAY 01/21 Aktif!");
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
  state.ventForceEvalImmediate = true;
  saveSettingsToNVS();
  triggerBuzzerCue(BZ_CUE_STOP);
  showOledToast("SIKLUS BERHENTI", "Status: SIAGA");
  Serial.println("[INKUBASI] Siklus inkubasi DIHENTIKAN (Idle).");
}

void openConfirmDialog(bool isStartAction) {
  confirmChoiceIdx = 0; // Selalu mulai di [ BATAL ] agar aman (wajib geser ke YA lalu tahan OK)
  localUiMode      = isStartAction ? LOCAL_UI_CONFIRM_START : LOCAL_UI_CONFIRM_STOP;
  localUiLastActMs = millis();
}

void executeMenuSelection(uint8_t itemIdx) {
  localUiLastActMs = millis();
  switch (itemIdx) {
    case 0: // Langkah 1 -> Buka Verifikasi Langkah ke-2 (Mulai atau Hentikan)
      if (state.incubationStatus == "running") {
        openConfirmDialog(false);
      } else {
        openConfirmDialog(true);
      }
      break;

    case 1: // Mode & Mute Buzzer (LP NUMB -> ROCK BEAT -> SMOKE WATER -> MUTE)
      if (state.buzzerMuted) {
        state.buzzerStyle = 2; // Linkin Park - Numb
        setBuzzerMuted(false);
        triggerBuzzerCue(BZ_CUE_CLICK);
      } else if (state.buzzerStyle == 2) {
        state.buzzerStyle = 0; // Queen - We Will Rock You
        saveSettingsToNVS();
        triggerBuzzerCue(BZ_CUE_CLICK);
      } else if (state.buzzerStyle == 0) {
        state.buzzerStyle = 1; // Deep Purple - Smoke on the Water
        saveSettingsToNVS();
        triggerBuzzerCue(BZ_CUE_CLICK);
      } else {
        setBuzzerMuted(true);
      }
      break;

    case 2: // Tes Lagu Rock (Putar sampel riff lagu rock aktif)
      triggerBuzzerCue(BZ_CUE_PREVIEW_ROCK);
      if (state.buzzerStyle == 2)      showOledToast("LINKIN PARK!", "Numb - Playing..");
      else if (state.buzzerStyle == 1) showOledToast("DEEP PURPLE!", "Smoke On Water");
      else                             showOledToast("QUEEN ROCK!", "We Will Rock U");
      break;

    case 3: // Tes Gerak Fisik Servo MG996R (0 -> 45 -> 90 -> kembali)
      startServoHardwareTest();
      showOledToast("TES SERVO", "Ayun 0-45-90 deg");
      break;

    case 4: // Pengaturan Kecerahan Layar OLED
      state.oledBrightness = (state.oledBrightness + 1) % 3;
      applyOledContrastSettings();
      saveSettingsToNVS();
      break;

    case 5: // Tukar Posisi Layar OLED 1 <-> OLED 2
      state.oledSwapScreens = !state.oledSwapScreens;
      saveSettingsToNVS();
      showOledToast("POSISI LAYAR", state.oledSwapScreens ? "Layar Ditukar" : "Layar Normal");
      break;

    case 6: // Nyalakan / Matikan Hotspot Captive Portal Setup Wi-Fi
      if (portalActive) {
        stopWifiPortal();
        showOledToast("HOTSPOT OFF", "Mode WiFi Normal");
      } else {
        startWifiPortal();
        localUiMode = LOCAL_UI_HOME;
      }
      break;

    default: // Tutup Menu / Kembali
      localUiMode = LOCAL_UI_HOME;
      break;
  }
}

void handleShortButtonClick() {
  localUiLastActMs = millis();
  triggerBuzzerCue(BZ_CUE_CLICK);

  if (localUiMode == LOCAL_UI_TOAST) {
    localUiMode = LOCAL_UI_HOME;
    return;
  }

  if (localUiMode == LOCAL_UI_HOME) {
    // Jika alarm sedang bunyi bising, arahkan kursor langsung ke menu Buzzer/Mute agar cepat dibisukan!
    oledMenuCursor = (isAlarmConditionActive() && !state.buzzerMuted) ? 1 : 0;
    localUiMode    = LOCAL_UI_MENU;
    return;
  }

  if (localUiMode == LOCAL_UI_MENU) {
    oledMenuCursor = (oledMenuCursor + 1) % OLED_MENU_COUNT;
    return;
  }

  if (localUiMode == LOCAL_UI_CONFIRM_START || localUiMode == LOCAL_UI_CONFIRM_STOP) {
    confirmChoiceIdx = (confirmChoiceIdx == 0) ? 1 : 0;
    return;
  }
}

void handleLongButtonHold() {
  localUiLastActMs = millis();

  if (localUiMode == LOCAL_UI_HOME) {
    // Pintasan tahan tombol dari layar utama -> Masuk ke Verifikasi 2 Langkah Mulai / Berhenti
    triggerBuzzerCue(BZ_CUE_CLICK);
    openConfirmDialog(state.incubationStatus != "running");
    return;
  }

  if (localUiMode == LOCAL_UI_MENU) {
    triggerBuzzerCue(BZ_CUE_CLICK);
    executeMenuSelection(oledMenuCursor);
    return;
  }

  if (localUiMode == LOCAL_UI_CONFIRM_START) {
    if (confirmChoiceIdx == 1) {
      startIncubationLocalOrRemote();
    } else {
      triggerBuzzerCue(BZ_CUE_CLICK);
      showOledToast("DIBATALKAN", "Belum Dimulai");
    }
    return;
  }

  if (localUiMode == LOCAL_UI_CONFIRM_STOP) {
    if (confirmChoiceIdx == 1) {
      stopIncubationLocalOrRemote();
    } else {
      triggerBuzzerCue(BZ_CUE_CLICK);
      showOledToast("DIBATALKAN", "Tetap Berjalan");
    }
    return;
  }
}

void pollPhysicalSwitches() {
  // Mendukung Push Button di GPIO 26 (Hardware Utama) maupun GPIO 4 (Simulasi/Alternatif)
  bool pressed = (digitalRead(PIN_BTN_START) == LOW) || (digitalRead(PIN_BTN_START_ALT) == LOW);
  bool reading = pressed ? LOW : HIGH;
  unsigned long nowMs = millis();

  // Auto-timeout kembali ke layar utama bila menu/konfirmasi tidak disentuh selama 15 detik
  if (localUiMode == LOCAL_UI_TOAST && (nowMs - localToastStartMs >= LOCAL_TOAST_MS)) {
    localUiMode = LOCAL_UI_HOME;
  } else if (localUiMode != LOCAL_UI_HOME && localUiMode != LOCAL_UI_TOAST &&
             stableBtnState == HIGH && (nowMs - localUiLastActMs >= LOCAL_UI_TIMEOUT_MS)) {
    localUiMode = LOCAL_UI_HOME;
  }

  if (reading != lastBtnReading) {
    lastBtnDebounceMs = nowMs;
  }
  lastBtnReading = reading;

  if (nowMs - lastBtnDebounceMs > 40UL) {
    if (reading != stableBtnState) {
      stableBtnState = reading;
      if (stableBtnState == LOW) {
        btnPressStartMs     = nowMs;
        btnLongPressHandled = false;
        localUiLastActMs    = nowMs;
      } else {
        // Tombol dilepas sebelum mencapai batas tahan -> Klik Singkat (Geser / Buka Menu)
        if (!btnLongPressHandled && (nowMs - btnPressStartMs < BTN_HOLD_CONFIRM_MS)) {
          handleShortButtonClick();
        }
      }
    } else if (stableBtnState == LOW && !btnLongPressHandled) {
      // Tombol ditahan >= 0.85 detik -> Eksekusi / Pilih (OK)
      if (nowMs - btnPressStartMs >= BTN_HOLD_CONFIRM_MS) {
        btnLongPressHandled = true; // Kunci sampai tombol dilepas agar tidak menembus step ke-2 otomatis!
        handleLongButtonHold();
      }
    }
  }
}

// ============================================================================
// 12. LAYAR DUAL OLED 0.96" SSD1306 (BITMAP IKON TERMO, TELUR & WIFI LENGKUNG)
//     - OLED 1 (GPIO 21/22): Ikon Termo + "Inkubator" + Jam WIB | Suhu + Telur + Lembab%
//     - OLED 2 (GPIO 16/17): Ikon WiFi + dBm/OFFLINE + SETTER/LOCKDOWN | DAY XX/21 | Mulai
// ============================================================================
const unsigned char ICON_THERMO[] PROGMEM = {
  0x18, 0x24, 0x24, 0x2C, 0x24, 0x2C, 0x24, 0x42, 0x5A, 0x5A, 0x42, 0x3C
};

const unsigned char ICON_TELUR[] PROGMEM = {
  0x03, 0xC0, 0x07, 0xE0, 0x0F, 0xF0, 0x1C, 0xF8, 0x19, 0xF8,
  0x3B, 0xFC, 0x37, 0xFC, 0x3F, 0xFC, 0x7F, 0xFE, 0x7F, 0xFE,
  0x7F, 0xFE, 0x7F, 0xFE, 0x7F, 0xFE, 0x7F, 0xFE, 0x3F, 0xFC,
  0x3F, 0xFC, 0x1F, 0xF8, 0x0F, 0xF0, 0x07, 0xE0, 0x00, 0x00
};

// Bitmap Ikon Wi-Fi Lengkungan (12x10 px)
const unsigned char ICON_WIFI_3[] PROGMEM = {
  0x3F, 0xC0, 0x60, 0x60, 0x80, 0x10, 0x1F, 0x80, 0x30, 0xC0,
  0x0F, 0x00, 0x19, 0x80, 0x00, 0x00, 0x06, 0x00, 0x06, 0x00
};
const unsigned char ICON_WIFI_2[] PROGMEM = {
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x1F, 0x80, 0x30, 0xC0,
  0x0F, 0x00, 0x19, 0x80, 0x00, 0x00, 0x06, 0x00, 0x06, 0x00
};
const unsigned char ICON_WIFI_1[] PROGMEM = {
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x0F, 0x00, 0x19, 0x80, 0x00, 0x00, 0x06, 0x00, 0x06, 0x00
};
const unsigned char ICON_WIFI_0[] PROGMEM = {
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x10, 0x80,
  0x09, 0x00, 0x06, 0x00, 0x09, 0x00, 0x10, 0x80, 0x00, 0x00
};

int hitungLevelWiFi(int rssi) {
  if (WiFi.status() != WL_CONNECTED || rssi == 0 || rssi <= -95) return 0;
  if (rssi >= -60) return 3;
  if (rssi >= -76) return 2;
  return 1;
}

void formatClockWib(time_t epochUtc, char* outBuf, size_t outSize) {
  time_t wibEpoch = epochUtc + (7 * 3600L); // WIB (UTC+7)
  struct tm timeinfo;
  gmtime_r(&wibEpoch, &timeinfo);
  snprintf(outBuf, outSize, "%02d:%02d", timeinfo.tm_hour, timeinfo.tm_min);
}

void formatStartDateTimeWib(time_t epochUtc, char* outBuf, size_t outSize) {
  if (epochUtc <= 0) {
    snprintf(outBuf, outSize, "Mulai: Belum Dimulai");
    return;
  }
  time_t wibEpoch = epochUtc + (7 * 3600L); // WIB (UTC+7)
  struct tm timeinfo;
  gmtime_r(&wibEpoch, &timeinfo);
  snprintf(outBuf, outSize, "Mulai:%02d/%02d/%02d %02d:%02d",
           timeinfo.tm_mday,
           timeinfo.tm_mon + 1,
           (timeinfo.tm_year + 1900) % 100,
           timeinfo.tm_hour,
           timeinfo.tm_min);
}

void tampilOLED1(Adafruit_SSD1306& disp) {
  disp.clearDisplay();
  disp.setTextColor(SSD1306_WHITE);

  // --- BARIS ATAS: IKON TERMO + JUDUL "Inkubator" + BADGE MUTE + JAM WIB ---
  disp.drawBitmap(2, 2, ICON_THERMO, 8, 12, SSD1306_WHITE);
  disp.setTextSize(1);
  disp.setCursor(14, 4);
  if (state.buzzerMuted) {
    disp.print("Inkubator[M]");
  } else {
    disp.print("Inkubator");
  }

  char clockBuf[8];
  formatClockWib(getCurrentEpoch(), clockBuf, sizeof(clockBuf));
  disp.setCursor(95, 4);
  disp.print(clockBuf);

  // --- GARIS PEMISAH HORIZONTAL ---
  disp.drawFastHLine(0, 15, 128, SSD1306_WHITE);

  // KIRI: SUHU (Area x = 4..51)
  disp.setTextSize(2);
  disp.setCursor(4, 24);
  if (state.sensorError) {
    disp.print("ERR");
  } else {
    disp.print(state.temperature, 1);
  }
  disp.drawFastHLine(4, 44, 48, SSD1306_WHITE);
  disp.setTextSize(1);
  disp.setCursor(16, 49);
  disp.print("Suhu");

  // TENGAH: IKON TELUR (x = 56..71)
  disp.drawBitmap(56, 29, ICON_TELUR, 16, 20, SSD1306_WHITE);

  // KANAN: KELEMBAPAN (Area x = 76..123)
  disp.setTextSize(2);
  disp.setCursor(85, 24);
  if (state.sensorError) {
    disp.print("ERR");
  } else {
    disp.print((int)round(state.humidity));
    disp.setTextSize(1);
    disp.print("%");
  }
  disp.drawFastHLine(76, 44, 48, SSD1306_WHITE);
  disp.setTextSize(1);
  disp.setCursor(82, 49);
  disp.print("Lembab");

  disp.display();
}

void tampilOLED2(Adafruit_SSD1306& disp) {
  disp.clearDisplay();
  disp.setTextColor(SSD1306_WHITE);
  disp.setTextSize(1);

  // --- BARIS ATAS: IKON WIFI LENGKUNGAN + RSSI & LABEL FASE ---
  int rssi  = (WiFi.status() == WL_CONNECTED) ? (int)WiFi.RSSI() : 0;
  int level = hitungLevelWiFi(rssi);

  const unsigned char* ikonWifi = ICON_WIFI_0;
  if (level == 3)      ikonWifi = ICON_WIFI_3;
  else if (level == 2) ikonWifi = ICON_WIFI_2;
  else if (level == 1) ikonWifi = ICON_WIFI_1;

  disp.drawBitmap(2, 2, ikonWifi, 12, 10, SSD1306_WHITE);

  disp.setCursor(17, 3);
  if (level == 0) {
    disp.print("OFFLINE");
  } else {
    disp.print(rssi);
    disp.print("dBm");
  }

  disp.setCursor(76, 3);
  if (state.incubationStatus == "idle") {
    disp.print(" SIAGA  ");
  } else if (state.incubationStatus == "finished") {
    disp.print("SELESAI ");
  } else if (state.stage == "lockdown" || state.currentDay >= INCUBATION_LOCKDOWN_START) {
    disp.print("LOCKDOWN");
  } else {
    disp.print(" SETTER ");
  }

  // --- GARIS PEMISAH HORIZONTAL ---
  disp.drawFastHLine(0, 15, 128, SSD1306_WHITE);

  // Teks Utama Hari Inkubasi (Rata Tengah)
  disp.setTextSize(2);
  char dayBuf[16];
  int displayDay = (state.incubationStatus == "finished") ? INCUBATION_TOTAL_DAYS : state.currentDay;
  snprintf(dayBuf, sizeof(dayBuf), "DAY %02d/21", displayDay);
  disp.setCursor(10, 23);
  disp.print(dayBuf);

  // Garis Pemisah Halus antara DAY dan Waktu Start
  disp.drawFastHLine(10, 43, 108, SSD1306_WHITE);

  // Informasi Waktu Start (Tanggal/Bulan/Tahun Jam:Menit WIB)
  disp.setTextSize(1);
  char startBuf[24];
  if (state.incubationStatus == "running" || state.incubationStatus == "finished") {
    formatStartDateTimeWib(state.startedAtEpoch, startBuf, sizeof(startBuf));
  } else {
    snprintf(startBuf, sizeof(startBuf), "Mulai: Belum Dimulai");
  }
  disp.setCursor(4, 49);
  disp.print(startBuf);

  disp.display();
}

String truncateText(const String& s, size_t maxLen) {
  if (s.length() <= maxLen) return s;
  if (maxLen <= 2) return s.substring(0, maxLen);
  return s.substring(0, maxLen - 2) + "..";
}

void tampilOLEDWifiPortal(Adafruit_SSD1306& disp) {
  disp.clearDisplay();
  disp.setTextColor(SSD1306_WHITE);
  disp.setTextSize(1);

  const unsigned char* ikon = ((wifiAnimFrame % 2) == 0) ? ICON_WIFI_3 : ICON_WIFI_1;
  disp.drawBitmap(2, 2, ikon, 12, 10, SSD1306_WHITE);
  disp.setCursor(18, 4);
  disp.print("SETUP WIFI");
  disp.drawFastHLine(0, 15, 128, SSD1306_WHITE);

  disp.setCursor(4, 20);
  disp.print("Hotspot:");
  disp.setCursor(4, 30);
  disp.print(truncateText(AP_HOTSPOT_SSID, 20));
  disp.setCursor(4, 42);
  disp.print("Pass: ");
  disp.print(AP_HOTSPOT_PASS);
  disp.setCursor(4, 53);
  disp.print("Web : 192.168.4.1");

  disp.display();
}

void tampilOLEDWifiConnecting(Adafruit_SSD1306& disp) {
  disp.clearDisplay();
  disp.setTextColor(SSD1306_WHITE);
  disp.setTextSize(1);

  uint8_t step = wifiAnimFrame % 3;
  const unsigned char* ikon = (step == 0) ? ICON_WIFI_1 : (step == 1 ? ICON_WIFI_2 : ICON_WIFI_3);
  disp.drawBitmap(2, 2, ikon, 12, 10, SSD1306_WHITE);
  disp.setCursor(18, 4);
  disp.print("MENGHUBUNGKAN");
  disp.drawFastHLine(0, 15, 128, SSD1306_WHITE);

  // Gelombang sinyal besar animasi di tengah atas
  int cx = 64;
  int cy = 32;
  disp.fillCircle(cx, cy, 2, SSD1306_WHITE);
  if (step >= 0) disp.drawCircle(cx, cy, 5, SSD1306_WHITE);
  if (step >= 1) disp.drawCircle(cx, cy, 9, SSD1306_WHITE);
  if (step >= 2) disp.drawCircle(cx, cy, 13, SSD1306_WHITE);
  disp.fillRect(0, cy + 3, 128, 12, SSD1306_BLACK); // Potong separuh bawah agar berbentuk busur Wi-Fi

  String targetSsid = pendingWifiSsid.length() > 0 ? pendingWifiSsid : state.wifiSsid;
  disp.setCursor(4, 37);
  disp.print("WiFi: ");
  disp.print(truncateText(targetSsid, 14));

  disp.setCursor(4, 47);
  disp.print("Menyambung");
  for (uint8_t i = 0; i <= step; i++) disp.print(".");

  // Progress bar bergerak di bagian bawah
  disp.drawRect(4, 57, 120, 5, SSD1306_WHITE);
  int barPos = 6 + ((wifiAnimFrame * 14) % 92);
  disp.fillRect(barPos, 58, 24, 3, SSD1306_WHITE);

  disp.display();
}

void tampilOLEDWifiConnected(Adafruit_SSD1306& disp) {
  disp.clearDisplay();
  disp.setTextColor(SSD1306_WHITE);
  disp.setTextSize(1);

  unsigned long elapsed = millis() - wifiAnimStartMs;

  if (elapsed < 700UL) {
    // Fase 1: Lingkaran konfirmasi membesar & sinyal Wi-Fi naik
    int r = (int)map((long)elapsed, 0L, 700L, 3L, 12L);
    disp.drawCircle(64, 24, r, SSD1306_WHITE);
    const unsigned char* ikon = (elapsed < 230UL) ? ICON_WIFI_1 : (elapsed < 460UL ? ICON_WIFI_2 : ICON_WIFI_3);
    disp.drawBitmap(58, 42, ikon, 12, 10, SSD1306_WHITE);
    disp.display();
    return;
  }

  if (elapsed < 1300UL) {
    // Fase 2: Tanda centang tergambar bertahap di dalam lingkaran
    disp.drawCircle(64, 22, 12, SSD1306_WHITE);
    disp.drawLine(58, 22, 62, 26, SSD1306_WHITE);
    disp.drawLine(58, 23, 62, 27, SSD1306_WHITE);
    if (elapsed >= 950UL) {
      disp.drawLine(62, 26, 70, 17, SSD1306_WHITE);
      disp.drawLine(62, 27, 70, 18, SSD1306_WHITE);
    }
    disp.setCursor(32, 42);
    disp.print("TERHUBUNG!");
    disp.display();
    return;
  }

  // Fase 3: Informasi lengkap Wi-Fi yang terhubung + IP lokal + bar durasi
  disp.drawBitmap(2, 2, ICON_WIFI_3, 12, 10, SSD1306_WHITE);
  disp.setCursor(18, 4);
  disp.print("WIFI TERHUBUNG!");
  disp.drawFastHLine(0, 15, 128, SSD1306_WHITE);

  disp.drawCircle(16, 30, 9, SSD1306_WHITE);
  disp.drawLine(11, 30, 14, 33, SSD1306_WHITE);
  disp.drawLine(14, 33, 21, 26, SSD1306_WHITE);
  disp.drawLine(14, 34, 21, 27, SSD1306_WHITE);

  disp.setCursor(30, 21);
  disp.print("Terhubung ke:");
  disp.setCursor(30, 31);
  disp.print(truncateText(connectedSsidDisplay, 16));

  disp.setCursor(4, 46);
  disp.print("IP: ");
  disp.print(connectedIpDisplay);

  long rem = (long)WIFI_ANIM_CONNECTED_MS - (long)elapsed;
  if (rem < 0) rem = 0;
  int barW = (int)map(rem, 0L, (long)(WIFI_ANIM_CONNECTED_MS - 1300UL), 0L, 120L);
  barW = constrain(barW, 0, 120);
  disp.drawFastHLine(4, 59, barW, SSD1306_WHITE);

  disp.display();
}

void tampilOLEDWifiFailed(Adafruit_SSD1306& disp) {
  disp.clearDisplay();
  disp.setTextColor(SSD1306_WHITE);
  disp.setTextSize(1);

  disp.drawBitmap(2, 2, ICON_WIFI_0, 12, 10, SSD1306_WHITE);
  disp.setCursor(18, 4);
  disp.print("GAGAL KONEK!");
  disp.drawFastHLine(0, 15, 128, SSD1306_WHITE);

  disp.drawCircle(16, 32, 9, SSD1306_WHITE);
  disp.drawLine(12, 28, 20, 36, SSD1306_WHITE);
  disp.drawLine(20, 28, 12, 36, SSD1306_WHITE);

  String targetSsid = pendingWifiSsid.length() > 0 ? pendingWifiSsid : state.wifiSsid;
  disp.setCursor(30, 24);
  disp.print(truncateText(targetSsid, 16));
  disp.setCursor(30, 35);
  disp.print("Gagal tersambung");
  disp.setCursor(4, 50);
  disp.print("Cek sandi / 2.4GHz");

  disp.display();
}

// Menggambar bar progres saat tombol sedang ditahan menuju konfirmasi OK (0.85 detik)
void drawButtonHoldProgress(Adafruit_SSD1306& disp) {
  if (stableBtnState == LOW && !btnLongPressHandled) {
    unsigned long heldMs = millis() - btnPressStartMs;
    if (heldMs > 60UL) {
      int w = (int)map((long)min(heldMs, BTN_HOLD_CONFIRM_MS), 0L, (long)BTN_HOLD_CONFIRM_MS, 0L, 124L);
      disp.drawRect(0, 56, 128, 8, SSD1306_WHITE);
      disp.fillRect(2, 58, w, 4, SSD1306_WHITE);
      return;
    }
  }
  disp.setTextSize(1);
  disp.setCursor(2, 56);
  disp.print("Klik:Geser  Tahan:OK");
}

String getOledMenuLabel(uint8_t idx) {
  switch (idx) {
    case 0:
      return (state.incubationStatus == "running") ? "Hentikan Siklus" : "Mulai Inkubasi";
    case 1:
      if (state.buzzerMuted)        return "Buzzer: MUTE";
      if (state.buzzerStyle == 2)   return "Buzzer: LP - NUMB";
      if (state.buzzerStyle == 1)   return "Buzzer: SMOKE WTR";
      return "Buzzer: ROCK BEAT";
    case 2:
      return "Tes Lagu Rock";
    case 3:
      return servoTestActive ? "Servo: Bergerak.." : "Tes Gerak Servo";
    case 4:
      if (state.oledBrightness == 0) return "OLED: Hemat Redup";
      if (state.oledBrightness == 1) return "OLED: Cerah Sedang";
      return "OLED: Cerah Penuh";
    case 5:
      return state.oledSwapScreens ? "Layar: Ditukar" : "Layar: Normal";
    case 6:
      return portalActive ? "Hotspot: AKTIF" : "Hotspot: Mati";
    default:
      return "<< Kembali";
  }
}

void tampilOLEDMenu(Adafruit_SSD1306& disp) {
  disp.clearDisplay();
  disp.setTextColor(SSD1306_WHITE);
  disp.setTextSize(1);

  disp.setCursor(2, 2);
  disp.printf("PENGATURAN OLED %d/%d", oledMenuCursor + 1, OLED_MENU_COUNT);
  disp.drawFastHLine(0, 12, 128, SSD1306_WHITE);

  // Tampilkan 3 item sekaligus agar baris lega dan mudah dibaca
  int startIdx = (int)oledMenuCursor - 1;
  if (startIdx < 0) startIdx = 0;
  if (startIdx > (int)OLED_MENU_COUNT - 3) startIdx = (int)OLED_MENU_COUNT - 3;

  for (int row = 0; row < 3; row++) {
    int itemIdx = startIdx + row;
    int y = 15 + (row * 13);
    bool selected = (itemIdx == (int)oledMenuCursor);
    if (selected) {
      disp.fillRect(0, y - 1, 128, 11, SSD1306_WHITE);
      disp.setTextColor(SSD1306_BLACK);
      disp.setCursor(2, y);
      disp.print("> ");
    } else {
      disp.setTextColor(SSD1306_WHITE);
      disp.setCursor(2, y);
      disp.print("  ");
    }
    disp.print(truncateText(getOledMenuLabel((uint8_t)itemIdx), 18));
  }

  disp.setTextColor(SSD1306_WHITE);
  disp.drawFastHLine(0, 53, 128, SSD1306_WHITE);
  drawButtonHoldProgress(disp);
  disp.display();
}

void tampilOLEDConfirm(Adafruit_SSD1306& disp, bool isStartAction) {
  disp.clearDisplay();
  disp.setTextColor(SSD1306_WHITE);
  disp.setTextSize(1);

  disp.setCursor(2, 2);
  disp.print(isStartAction ? "VERIFIKASI MULAI 2/2" : "VERIFIKASI STOP! 2/2");
  disp.drawFastHLine(0, 12, 128, SSD1306_WHITE);

  disp.setCursor(2, 16);
  if (isStartAction) {
    disp.print("Mulai siklus 21 hari?");
  } else {
    disp.printf("Hentikan H-%02d/21 skrg?", state.currentDay);
  }

  // Pilihan 0: BATAL | Pilihan 1: YA, KONFIRMASI
  const char* opt0 = "[ BATAL / KEMBALI ]";
  const char* opt1 = isStartAction ? "[ YA, MULAI SKRG! ]" : "[ YA, HENTIKAN! ]";

  for (int i = 0; i < 2; i++) {
    int y = 28 + (i * 12);
    bool sel = ((int)confirmChoiceIdx == i);
    if (sel) {
      disp.fillRect(2, y - 1, 124, 11, SSD1306_WHITE);
      disp.setTextColor(SSD1306_BLACK);
    } else {
      disp.setTextColor(SSD1306_WHITE);
    }
    disp.setCursor(6, y);
    disp.print(sel ? "> " : "  ");
    disp.print(i == 0 ? opt0 : opt1);
  }

  disp.setTextColor(SSD1306_WHITE);
  drawButtonHoldProgress(disp);
  disp.display();
}

void tampilOLEDToast(Adafruit_SSD1306& disp) {
  disp.clearDisplay();
  disp.setTextColor(SSD1306_WHITE);
  disp.drawRoundRect(2, 4, 124, 56, 4, SSD1306_WHITE);
  disp.setTextSize(1);
  disp.setCursor(12, 14);
  disp.print(truncateText(toastTitle, 18));
  disp.drawFastHLine(8, 26, 112, SSD1306_WHITE);
  disp.setCursor(10, 35);
  disp.print(truncateText(toastSubtitle, 19));
  if (servoTestActive) {
    disp.setCursor(10, 47);
    disp.printf("Sudut Servo: %d deg", lastServoHwAngle);
  }
  disp.display();
}

void updateOledDisplay() {
  if (!state.oledAvailable && !state.oled2Available) return;

  if (wifiUiPhase != WIFI_UI_NORMAL) {
    wifiAnimFrame++;
  }

  auto renderActiveWifiScreen = [&](Adafruit_SSD1306& disp) {
    if (wifiUiPhase == WIFI_UI_PORTAL) {
      tampilOLEDWifiPortal(disp);
    } else if (wifiUiPhase == WIFI_UI_CONNECTING) {
      tampilOLEDWifiConnecting(disp);
    } else if (wifiUiPhase == WIFI_UI_CONNECTED_ANIM) {
      tampilOLEDWifiConnected(disp);
    } else if (wifiUiPhase == WIFI_UI_FAILED_ANIM) {
      tampilOLEDWifiFailed(disp);
    } else {
      tampilOLED2(disp);
    }
  };

  auto renderInteractiveOrStatus = [&](Adafruit_SSD1306& disp) {
    if (localUiMode == LOCAL_UI_MENU) {
      tampilOLEDMenu(disp);
    } else if (localUiMode == LOCAL_UI_CONFIRM_START) {
      tampilOLEDConfirm(disp, true);
    } else if (localUiMode == LOCAL_UI_CONFIRM_STOP) {
      tampilOLEDConfirm(disp, false);
    } else if (localUiMode == LOCAL_UI_TOAST) {
      tampilOLEDToast(disp);
    } else if (wifiUiPhase != WIFI_UI_NORMAL) {
      renderActiveWifiScreen(disp);
    } else {
      tampilOLED2(disp);
    }
  };

  // Jika kedua OLED terpasang (mendukung opsi Tukar Layar 1 <-> 2):
  if (state.oledAvailable && state.oled2Available) {
    Adafruit_SSD1306& sensorDisp = state.oledSwapScreens ? oled2 : oled;
    Adafruit_SSD1306& statusDisp = state.oledSwapScreens ? oled : oled2;
    tampilOLED1(sensorDisp);
    renderInteractiveOrStatus(statusDisp);
    return;
  }

  // Jika hanya 1 OLED terpasang: prioritaskan Menu / Verifikasi 2 Langkah saat sedang dibuka
  Adafruit_SSD1306& activeDisp = state.oledAvailable ? oled : oled2;
  if (localUiMode != LOCAL_UI_HOME) {
    renderInteractiveOrStatus(activeDisp);
    return;
  }
  if (wifiUiPhase == WIFI_UI_CONNECTING ||
      wifiUiPhase == WIFI_UI_CONNECTED_ANIM ||
      wifiUiPhase == WIFI_UI_FAILED_ANIM) {
    renderActiveWifiScreen(activeDisp);
    return;
  }

  if (((millis() / 5000UL) % 2UL) == 0UL) {
    tampilOLED1(activeDisp);
  } else if (wifiUiPhase == WIFI_UI_PORTAL) {
    tampilOLEDWifiPortal(activeDisp);
  } else {
    tampilOLED2(activeDisp);
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
    if (state.stage != "lockdown") state.ventForceEvalImmediate = true;
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
    if (state.stage != "lockdown") state.ventForceEvalImmediate = true;
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
      if (!state.sensorFilterSeeded) {
        state.filteredTemperature = t;
        state.filteredHumidity    = h;
        for (int i = 0; i < TEMP_TREND_SAMPLES; i++) {
          state.tempHistory[i] = t;
        }
        state.tempHistCount      = TEMP_TREND_SAMPLES;
        state.tempHistIdx        = 0;
        state.sensorFilterSeeded = true;
      } else {
        state.filteredTemperature += TEMP_EMA_ALPHA * (t - state.filteredTemperature);
        state.filteredHumidity    += HUM_EMA_ALPHA  * (h - state.filteredHumidity);
        state.tempHistory[state.tempHistIdx] = state.filteredTemperature;
        state.tempHistIdx = (state.tempHistIdx + 1) % TEMP_TREND_SAMPLES;
        if (state.tempHistCount < TEMP_TREND_SAMPLES) state.tempHistCount++;
      }

      state.temperature      = roundf(state.filteredTemperature * 10.0f) / 10.0f;
      state.humidity         = roundf(state.filteredHumidity);
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
      state.sht30Available     = true;
      state.sensorFilterSeeded = false;
      float t = sht30.readTemperature();
      float h = sht30.readHumidity();
      if (!isnan(t) && !isnan(h)) {
        state.filteredTemperature = t;
        state.filteredHumidity    = h;
        for (int i = 0; i < TEMP_TREND_SAMPLES; i++) state.tempHistory[i] = t;
        state.tempHistCount      = TEMP_TREND_SAMPLES;
        state.tempHistIdx        = 0;
        state.sensorFilterSeeded = true;
        state.temperature        = roundf(t * 10.0f) / 10.0f;
        state.humidity           = roundf(h);
        state.sensorFailStreak   = 0;
        state.sensorError        = false;
      } else {
        state.sensorFailStreak++;
        if (state.sensorFailStreak >= 3) state.sensorError = true;
      }
    } else {
      state.sensorFailStreak++;
      if (state.sensorFailStreak >= 3) {
        state.sensorError = true;
      }
    }
  }
}

// ============================================================================
// 14. GANTI WI-FI ANTI-PUTUS & HOTSPOT CAPTIVE PORTAL (192.168.4.1)
// ============================================================================
void beepWifiConnected() {
  if (isAlarmConditionActive() || state.buzzerMuted) return;
  triggerBuzzerCue(BZ_CUE_WIFI);
}

const char PORTAL_HTML[] PROGMEM =
  "<!doctype html>\n"
  "<html lang='id'>\n"
  "<head>\n"
  "<meta charset='utf-8'>\n"
  "<meta name='viewport' content='width=device-width,initial-scale=1'>\n"
  "<title>Setup Wi-Fi Smart Hatchery</title>\n"
  "<style>\n"
  "  body{margin:0;padding:16px;background:#0b0705;color:#f8f1ea;font-family:system-ui,-apple-system,sans-serif;line-height:1.45}\n"
  "  .card{max-width:440px;margin:0 auto;background:#16100c;border:1px solid #3b2819;border-radius:16px;padding:18px}\n"
  "  h1{margin:0 0 4px;font-size:20px;color:#f0a040}\n"
  "  p.sub{margin:0 0 14px;font-size:13px;color:#bcab9c}\n"
  "  label{display:block;font-size:13px;font-weight:600;margin:12px 0 4px}\n"
  "  input{width:100%;box-sizing:border-box;padding:10px 12px;border-radius:10px;border:1px solid #573b27;background:#0e0907;color:#f8f1ea;font-size:15px}\n"
  "  .btn{width:100%;margin-top:14px;padding:11px;border:0;border-radius:10px;background:#f08040;color:#1a0d06;font-weight:700;font-size:15px;cursor:pointer}\n"
  "  .btn2{width:100%;margin:4px 0 10px;padding:9px;border:1px solid #573b27;border-radius:10px;background:#201610;color:#f8f1ea;font-size:14px;cursor:pointer}\n"
  "  .list{max-height:180px;overflow-y:auto;margin-bottom:8px;border:1px solid #3b2819;border-radius:10px;display:none}\n"
  "  .item{display:flex;justify-content:space-between;padding:9px 12px;border-bottom:1px solid #2b1d12;cursor:pointer;font-size:14px}\n"
  "  .item:last-child{border-bottom:0}\n"
  "  .item:hover{background:#261911}\n"
  "  .rssi{color:#f0a040;font-size:12px}\n"
  "  .row{display:flex;align-items:center;gap:6px;margin-top:6px;font-size:13px;color:#bcab9c}\n"
  "  .row input{width:auto}\n"
  "  details{margin-top:14px;border-top:1px solid #3b2819;padding-top:10px}\n"
  "  summary{cursor:pointer;font-size:13px;color:#f0a040;font-weight:600}\n"
  "  #status{margin-top:14px;padding:10px 12px;border-radius:10px;font-size:14px;display:none}\n"
  "  .info{display:block!important;background:#261a10;border:1px solid #8a5629;color:#fcd34d}\n"
  "  .ok{display:block!important;background:#0f291a;border:1px solid #2e7d4f;color:#86efac}\n"
  "  .err{display:block!important;background:#2b1111;border:1px solid #8c2f2f;color:#fca5a5}\n"
  "  .presets{display:flex;flex-wrap:wrap;gap:6px;margin:6px 0 8px}\n"
  "  .pbtn{flex:1 1 auto;padding:7px 8px;border:1px solid #573b27;border-radius:8px;background:#201610;color:#f8f1ea;font-size:12px;cursor:pointer}\n"
  "  .pbtn:hover{border-color:#f0a040;color:#f0a040}\n"
  "</style>\n"
  "</head>\n"
  "<body>\n"
  "<div class='card'>\n"
  "  <h1>Smart Hatchery Wi-Fi</h1>\n"
  "  <p class='sub'>Pilih jaringan Wi-Fi 2.4 GHz agar alat inkubator dapat mengirim data ke server.</p>\n"
  "  <button type='button' class='btn2' id='scanBtn' onclick='scanWifi()'>Scan Wi-Fi Sekitar</button>\n"
  "  <div id='netList' class='list'></div>\n"
  "  <form id='f' onsubmit='return submitForm(event)'>\n"
  "    <label for='ssid'>Nama Wi-Fi (SSID)</label>\n"
  "    <input id='ssid' name='ssid' maxlength='32' required placeholder='Klik hasil scan atau ketik manual'>\n"
  "    <label for='pass'>Kata Sandi Wi-Fi</label>\n"
  "    <input id='pass' name='pass' type='password' maxlength='63' placeholder='Minimal 8 karakter (kosong jika terbuka)'>\n"
  "    <div class='row'><input type='checkbox' id='show' onchange=\"pass.type=this.checked?'text':'password'\"><label for='show' style='margin:0;font-weight:400'>Tampilkan kata sandi</label></div>\n"
  "    <details>\n"
  "      <summary>Pengaturan Server (Opsional)</summary>\n"
  "      <label for='burl'>URL Server Backend</label>\n"
  "      <div id='presetBox' class='presets'></div>\n"
  "      <input id='burl' name='backend_url' placeholder='https://.../api/device/telemetry'>\n"
  "      <label for='akey'>Device API Key</label>\n"
  "      <input id='akey' name='api_key' placeholder='Token rahasia alat'>\n"
  "    </details>\n"
  "    <button type='submit' class='btn' id='subBtn'>Simpan & Hubungkan</button>\n"
  "  </form>\n"
  "  <div id='status'></div>\n"
  "</div>\n"
  "<script>\n"
  "const st = document.getElementById('status');\n"
  "let pollTimer = null;\n"
  "fetch('/config').then(r=>r.json()).then(c=>{\n"
  "  if(c.ssid) document.getElementById('ssid').value = c.ssid;\n"
  "  if(c.backend_url) document.getElementById('burl').value = c.backend_url;\n"
  "  if(c.api_key) document.getElementById('akey').value = c.api_key;\n"
  "  if(c.presets && c.presets.length){\n"
  "    const pb = document.getElementById('presetBox');\n"
  "    pb.innerHTML = '';\n"
  "    c.presets.forEach(p=>{\n"
  "      const b = document.createElement('button');\n"
  "      b.type = 'button';\n"
  "      b.className = 'pbtn';\n"
  "      b.textContent = p.label;\n"
  "      b.onclick = ()=>{ document.getElementById('burl').value = p.url; };\n"
  "      pb.appendChild(b);\n"
  "    });\n"
  "  }\n"
  "}).catch(()=>{});\n"
  "const setSt = (cls, msg) => { st.className = cls; st.textContent = msg; };\n"
  "const scanWifi = async () => {\n"
  "  const btn = document.getElementById('scanBtn');\n"
  "  const box = document.getElementById('netList');\n"
  "  btn.disabled = true; btn.textContent = 'Memindai Wi-Fi...';\n"
  "  try{\n"
  "    const res = await fetch('/scan');\n"
  "    const nets = await res.json();\n"
  "    box.innerHTML = '';\n"
  "    if(!nets.length){\n"
  "      box.innerHTML = '<div class=\"item\">Tidak ada jaringan 2.4 GHz ditemukan</div>';\n"
  "    } else {\n"
  "      nets.forEach(n=>{\n"
  "        const d = document.createElement('div');\n"
  "        d.className = 'item';\n"
  "        d.innerHTML = '<span>'+n.ssid.replace(/</g,'&lt;')+(n.open?' (Terbuka)':'')+'</span><span class=\"rssi\">'+n.rssi+' dBm</span>';\n"
  "        d.onclick = ()=>{ document.getElementById('ssid').value = n.ssid; document.getElementById('pass').focus(); };\n"
  "        box.appendChild(d);\n"
  "      });\n"
  "    }\n"
  "    box.style.display = 'block';\n"
  "  } catch(e){ setSt('err','Gagal memindai Wi-Fi'); }\n"
  "  btn.disabled = false; btn.textContent = 'Scan Wi-Fi Sekitar';\n"
  "};\n"
  "const submitForm = async (e) => {\n"
  "  e.preventDefault();\n"
  "  const btn = document.getElementById('subBtn');\n"
  "  const body = new URLSearchParams(new FormData(document.getElementById('f')));\n"
  "  btn.disabled = true;\n"
  "  setSt('info','Menghubungkan ke Wi-Fi... Lihat layar OLED ESP32.');\n"
  "  try{\n"
  "    const res = await fetch('/connect',{method:'POST',body});\n"
  "    const d = await res.json();\n"
  "    if(!d.ok){ setSt('err', d.message || 'Input tidak valid'); btn.disabled = false; return false; }\n"
  "    if(pollTimer) clearInterval(pollTimer);\n"
  "    pollTimer = setInterval(async ()=>{\n"
  "      try{\n"
  "        const s = await (await fetch('/status')).json();\n"
  "        if(s.status === 'connected'){\n"
  "          clearInterval(pollTimer);\n"
  "          setSt('ok', s.message + ' Hotspot akan mati otomatis dalam 5 detik.');\n"
  "          btn.disabled = false;\n"
  "        } else if(s.status === 'failed'){\n"
  "          clearInterval(pollTimer);\n"
  "          setSt('err', s.message || 'Gagal terhubung. Silakan coba lagi.');\n"
  "          btn.disabled = false;\n"
  "        }\n"
  "      } catch(_){}\n"
  "    }, 1000);\n"
  "  } catch(err){ setSt('err','Gagal mengirim pengaturan'); btn.disabled = false; }\n"
  "  return false;\n"
  "};\n"
  "</script>\n"
  "</body>\n"
  "</html>\n";

void registerPortalRoutes() {
  if (portalRoutesRegistered) return;
  portalRoutesRegistered = true;

  portalServer.on("/", HTTP_GET, []() {
    portalServer.sendHeader("Cache-Control", "no-store");
    portalServer.send_P(200, "text/html", PORTAL_HTML);
  });

  portalServer.on("/config", HTTP_GET, []() {
    JsonDocument doc;
    doc["ssid"]        = state.wifiSsid;
    doc["backend_url"] = state.backendUrl;
    doc["api_key"]     = state.deviceApiKey;
    JsonArray presets  = doc["presets"].to<JsonArray>();
    JsonObject p1 = presets.add<JsonObject>();
    p1["label"] = "Domain Web (Utama)";
    p1["url"]   = BACKEND_DOMAIN_URL;
    JsonObject p2 = presets.add<JsonObject>();
    p2["label"] = "Jembatan Laptop";
    p2["url"]   = BACKEND_FALLBACK_URL;
    JsonObject p3 = presets.add<JsonObject>();
    p3["label"] = "VPS Tailscale";
    p3["url"]   = BACKEND_TELEMETRY_URL;
    String out;
    serializeJson(doc, out);
    portalServer.send(200, "application/json", out);
  });

  portalServer.on("/scan", HTTP_GET, []() {
    int n = WiFi.scanNetworks();
    struct NetItem {
      String ssid;
      int    rssi;
      bool   open;
    };
    NetItem items[12];
    int count = 0;

    for (int i = 0; i < n; i++) {
      String s = WiFi.SSID(i);
      if (s.isEmpty()) continue;
      int r = WiFi.RSSI(i);
      bool isOpen = (WiFi.encryptionType(i) == WIFI_AUTH_OPEN);

      int existing = -1;
      for (int k = 0; k < count; k++) {
        if (items[k].ssid == s) { existing = k; break; }
      }
      if (existing >= 0) {
        if (r > items[existing].rssi) {
          items[existing].rssi = r;
          items[existing].open = isOpen;
        }
      } else if (count < 12) {
        items[count++] = { s, r, isOpen };
      }
    }
    WiFi.scanDelete();

    // Urutkan dari sinyal terkuat (RSSI terbesar)
    for (int i = 0; i < count - 1; i++) {
      for (int j = i + 1; j < count; j++) {
        if (items[j].rssi > items[i].rssi) {
          NetItem tmp = items[i];
          items[i] = items[j];
          items[j] = tmp;
        }
      }
    }

    JsonDocument doc;
    JsonArray arr = doc.to<JsonArray>();
    for (int i = 0; i < count; i++) {
      JsonObject obj = arr.add<JsonObject>();
      obj["ssid"] = items[i].ssid;
      obj["rssi"] = items[i].rssi;
      obj["open"] = items[i].open;
    }
    String out;
    serializeJson(doc, out);
    portalServer.send(200, "application/json", out);
  });

  portalServer.on("/connect", HTTP_POST, []() {
    String ssid   = portalServer.arg("ssid");
    String pass   = portalServer.arg("pass");
    String bUrl   = portalServer.arg("backend_url");
    String apiKey = portalServer.arg("api_key");
    ssid.trim();
    bUrl.trim();
    apiKey.trim();

    if (ssid.length() < 1 || ssid.length() > 32) {
      portalServer.send(400, "application/json", "{\"ok\":false,\"message\":\"Nama Wi-Fi (SSID) wajib 1-32 karakter.\"}");
      return;
    }
    if (pass.length() > 0 && (pass.length() < 8 || pass.length() > 63)) {
      portalServer.send(400, "application/json", "{\"ok\":false,\"message\":\"Kata sandi Wi-Fi wajib 8-63 karakter (atau kosong jika terbuka).\"}");
      return;
    }
    if (!bUrl.isEmpty() && !bUrl.startsWith("http://") && !bUrl.startsWith("https://")) {
      portalServer.send(400, "application/json", "{\"ok\":false,\"message\":\"URL server harus diawali http:// atau https://.\"}");
      return;
    }

    pendingWifiSsid    = ssid;
    pendingWifiPass    = pass;
    pendingBackendUrl  = bUrl;
    pendingApiKey      = apiKey;
    portalConnecting   = true;
    portalShutdownPending = false;
    wifiConnectStartMs = millis();
    portalStatusText   = "connecting";
    portalStatusMsg    = "Sedang menyambung ke '" + ssid + "'...";
    wifiUiPhase        = WIFI_UI_CONNECTING;
    wifiAnimStartMs    = millis();
    wifiAnimFrame      = 0;

    WiFi.disconnect(false);
    WiFi.begin(pendingWifiSsid.c_str(), pendingWifiPass.c_str());

    portalServer.send(200, "application/json", "{\"ok\":true,\"status\":\"connecting\"}");
  });

  portalServer.on("/status", HTTP_GET, []() {
    JsonDocument doc;
    doc["status"]  = portalStatusText;
    doc["ssid"]    = connectedSsidDisplay;
    doc["ip"]      = connectedIpDisplay;
    doc["message"] = portalStatusMsg;
    String out;
    serializeJson(doc, out);
    portalServer.send(200, "application/json", out);
  });

  portalServer.onNotFound([]() {
    portalServer.sendHeader("Location", String("http://") + AP_LOCAL_IP.toString() + "/", true);
    portalServer.send(302, "text/plain", "");
  });
}

void startWifiPortal() {
  if (portalActive) return;

  WiFi.disconnect(false);
  WiFi.mode(WIFI_AP_STA);
  WiFi.softAPConfig(AP_LOCAL_IP, AP_GATEWAY_IP, AP_SUBNET_MASK);
  WiFi.softAP(AP_HOTSPOT_SSID, AP_HOTSPOT_PASS);

  portalDns.setErrorReplyCode(DNSReplyCode::NoError);
  portalDns.start(53, "*", AP_LOCAL_IP);
  registerPortalRoutes();
  portalServer.begin();

  portalActive          = true;
  portalConnecting      = false;
  portalShutdownPending = false;
  portalStatusText      = "idle";
  portalStatusMsg       = "";
  wifiUiPhase           = WIFI_UI_PORTAL;
  wifiAnimStartMs       = millis();

  Serial.printf("[PORTAL] Hotspot aktif: SSID='%s' Pass='%s' IP=%s\n",
                AP_HOTSPOT_SSID, AP_HOTSPOT_PASS, WiFi.softAPIP().toString().c_str());
}

void stopWifiPortal() {
  if (!portalActive) return;

  portalDns.stop();
  portalServer.stop();
  WiFi.softAPdisconnect(true);
  WiFi.mode(WIFI_STA);

  portalActive          = false;
  portalShutdownPending = false;
  Serial.println("[PORTAL] Hotspot setup dimatikan (mode WIFI_STA aktif).");
}

void handleWifiPortalAndState() {
  netBox.pauseRequested = (portalActive || portalConnecting);
  if (portalActive) {
    portalDns.processNextRequest();
    portalServer.handleClient();
  }

  if (portalConnecting) {
    if (WiFi.status() == WL_CONNECTED) {
      state.wifiSsid = pendingWifiSsid;
      state.wifiPass = pendingWifiPass;
      if (!pendingBackendUrl.isEmpty() &&
          (pendingBackendUrl.startsWith("http://") || pendingBackendUrl.startsWith("https://"))) {
        state.backendUrl = pendingBackendUrl;
      }
      if (!pendingApiKey.isEmpty()) {
        state.deviceApiKey = pendingApiKey;
      }
      state.commAlertPending = false;
      state.commAlertMessage = "";
      saveSettingsToNVS();
      configTime(0, 0, NTP_SERVER_1, NTP_SERVER_2);

      connectedSsidDisplay    = state.wifiSsid;
      connectedIpDisplay      = WiFi.localIP().toString();
      portalConnecting        = false;
      portalStatusText        = "connected";
      portalStatusMsg         = "Berhasil terhubung ke '" + state.wifiSsid + "' (IP: " + connectedIpDisplay + ").";
      wifiUiPhase             = WIFI_UI_CONNECTED_ANIM;
      wifiAnimStartMs         = millis();
      wifiDisconnectedSinceMs = 0;
      portalShutdownPending   = true;
      portalShutdownAtMs      = millis() + PORTAL_AP_SHUTDOWN_MS;

      Serial.printf("[PORTAL] Terhubung ke Wi-Fi '%s' (IP: %s)\n",
                    connectedSsidDisplay.c_str(), connectedIpDisplay.c_str());
      beepWifiConnected();
    } else if (millis() - wifiConnectStartMs >= PORTAL_CONNECT_WAIT_MS) {
      WiFi.disconnect(false);
      portalConnecting = false;
      portalStatusText = "failed";
      portalStatusMsg  = "Gagal tersambung ke '" + pendingWifiSsid + "' dalam 20 detik. Cek kata sandi & pastikan Wi-Fi 2.4 GHz.";
      wifiUiPhase      = WIFI_UI_FAILED_ANIM;
      wifiAnimStartMs  = millis();
      Serial.printf("[PORTAL] Gagal menyambung ke '%s'\n", pendingWifiSsid.c_str());
    }
  }

  if (portalShutdownPending && (long)(millis() - portalShutdownAtMs) >= 0) {
    stopWifiPortal();
  }

  if (wifiUiPhase == WIFI_UI_CONNECTED_ANIM && (millis() - wifiAnimStartMs >= WIFI_ANIM_CONNECTED_MS)) {
    wifiUiPhase = WIFI_UI_NORMAL;
  } else if (wifiUiPhase == WIFI_UI_FAILED_ANIM && (millis() - wifiAnimStartMs >= WIFI_ANIM_FAILED_MS)) {
    wifiUiPhase = portalActive ? WIFI_UI_PORTAL : WIFI_UI_NORMAL;
  }

  // Fallback otomatis buka hotspot bila belum ada Wi-Fi atau putus > 30 detik
  if (!portalActive && !portalConnecting) {
    if (WiFi.status() == WL_CONNECTED) {
      wifiDisconnectedSinceMs = 0;
    } else if (state.wifiSsid.isEmpty()) {
      startWifiPortal();
    } else {
      if (wifiDisconnectedSinceMs == 0) {
        wifiDisconnectedSinceMs = millis();
      } else if (millis() - wifiDisconnectedSinceMs >= WIFI_RECONNECT_FAIL_MS) {
        wifiDisconnectedSinceMs = 0;
        startWifiPortal();
      }
    }
  }
}

bool applyNewWifiWithRollback(const String& newSsid, const String& newPassword) {
  if (newSsid.length() < 1 || newSsid.length() > 32 ||
      newPassword.length() < 8 || newPassword.length() > 63) {
    return false;
  }

  netBox.pauseRequested = true;
  unsigned long waitNetStart = millis();
  while (netBox.inFlight && (millis() - waitNetStart < 4500UL)) {
    delay(25);
  }

  String oldSsid = state.wifiSsid;
  String oldPass = state.wifiPass;

  WiFi.disconnect(false);
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

    connectedSsidDisplay = newSsid;
    connectedIpDisplay   = WiFi.localIP().toString();
    wifiUiPhase          = WIFI_UI_CONNECTED_ANIM;
    wifiAnimStartMs      = millis();
    beepWifiConnected();
    netBox.pauseRequested = false;
    return true;
  }

  // Rollback ke Wi-Fi lama di NVS
  WiFi.disconnect(false);
  delay(200);
  WiFi.mode(WIFI_STA);
  WiFi.begin(oldSsid.c_str(), oldPass.c_str());

  unsigned long rollbackStart = millis();
  while (WiFi.status() != WL_CONNECTED && (millis() - rollbackStart < WIFI_CONNECT_TIMEOUT_MS)) {
    delay(250);
  }

  state.wifiSsid         = oldSsid;
  state.wifiPass         = oldPass;
  state.commAlertPending = true;
  state.commAlertMessage = "Gagal menyambung ke Wi-Fi '" + newSsid + "' dalam 30 detik. Perangkat rollback ke '" + oldSsid + "'.";
  netBox.pauseRequested  = false;
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

  float humMinBound, humMaxBound;
  getActiveHumidityBounds(humMinBound, humMaxBound);
  String phaseName = isIncubationLockdownPhase() ? "Fase Lockdown" : "Fase Setter";
  if (!state.sensorError && (state.humidity < humMinBound || state.humidity > humMaxBound)) {
    outHealth = "warning";
    outSummary = (state.humidity < humMinBound)
      ? ("Peringatan: Kelembapan rendah " + String((int)round(state.humidity)) + "% di " + phaseName + " (min " + String((int)humMinBound) + "%).")
      : ("Peringatan: Kelembapan tinggi " + String((int)round(state.humidity)) + "% di " + phaseName + " (maks " + String((int)humMaxBound) + "%).");
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
      state.lampFloatPct    = (float)state.lampIntensity;
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
        state.lampFloatPct    = (float)state.lampIntensity;
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
      state.ventMode               = "auto";
      state.ventFuzzyActive        = false;
      state.ventForceEvalImmediate = true;
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
  else if (target == "buzzer") {
    String action = cmdObj["action"] | "";
    if (action == "mute") {
      setBuzzerMuted(true);
      showOledToast("BUZZER: MUTE", "Dibisukan via Web");
      recordCommandResult(cmdId, "buzzer", "mute", true);
    } else if (action == "unmute") {
      setBuzzerMuted(false);
      triggerBuzzerCue(BZ_CUE_CLICK);
      showOledToast("BUZZER: AKTIF", "Lagu Rock Siap!");
      recordCommandResult(cmdId, "buzzer", "unmute", true);
    } else if (action == "test_rock") {
      triggerBuzzerCue(BZ_CUE_PREVIEW_ROCK);
      showOledToast("TES LAGU ROCK", "Dipicu dari Web");
      recordCommandResult(cmdId, "buzzer", "test_rock", true);
    } else if (action == "style_beat") {
      state.buzzerStyle = 0;
      setBuzzerMuted(false);
      triggerBuzzerCue(BZ_CUE_PREVIEW_ROCK);
      recordCommandResult(cmdId, "buzzer", "style_beat", true);
    } else if (action == "style_tone") {
      state.buzzerStyle = 1;
      setBuzzerMuted(false);
      triggerBuzzerCue(BZ_CUE_PREVIEW_ROCK);
      recordCommandResult(cmdId, "buzzer", "style_tone", true);
    } else if (action == "style_numb") {
      state.buzzerStyle = 2;
      setBuzzerMuted(false);
      triggerBuzzerCue(BZ_CUE_PREVIEW_ROCK);
      showOledToast("LINKIN PARK!", "Numb Dipilih");
      recordCommandResult(cmdId, "buzzer", "style_numb", true);
    } else {
      recordCommandResult(cmdId, "buzzer", action, false);
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
    state.ventForceEvalImmediate = true;
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
  if (portalActive || portalConnecting || state.wifiSsid.isEmpty()) {
    state.backendConnected = false;
    return;
  }

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

  JsonObject bz = doc["buzzer"].to<JsonObject>();
  bz["muted"] = state.buzzerMuted;
  bz["style"] = (state.buzzerStyle == 2) ? "numb" : ((state.buzzerStyle == 1) ? "rock_tone" : "rock_beat");

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
  float humMinBound, humMaxBound;
  getActiveHumidityBounds(humMinBound, humMaxBound);
  String phaseLabel = isIncubationLockdownPhase() ? "Fase Lockdown" : "Fase Setter";
  if (!state.sensorError && (state.humidity < humMinBound || state.humidity > humMaxBound)) {
    JsonObject a = alertsArr.add<JsonObject>();
    a["type"]     = "humidity_warning";
    a["severity"] = "medium";
    a["message"]  = (state.humidity < humMinBound)
      ? ("Kelembapan " + String((int)round(state.humidity)) + "% di bawah batas minimal " + phaseLabel + " (" + String((int)humMinBound) + "%)")
      : ("Kelembapan " + String((int)round(state.humidity)) + "% melewati batas maksimal " + phaseLabel + " (" + String((int)humMaxBound) + "%)");
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

  const char* candidates[4] = {
    state.backendUrl.c_str(),
    BACKEND_DOMAIN_URL,
    BACKEND_FALLBACK_URL,
    BACKEND_TELEMETRY_URL
  };
  String uniqueUrls[4];
  int urlCount = 0;
  for (int i = 0; i < 4; i++) {
    String u = String(candidates[i]);
    u.trim();
    if (u.isEmpty() || (!u.startsWith("http://") && !u.startsWith("https://"))) continue;
    bool dup = false;
    for (int j = 0; j < urlCount; j++) {
      if (uniqueUrls[j] == u) { dup = true; break; }
    }
    if (!dup) uniqueUrls[urlCount++] = u;
  }

  if (netBox.mutex != NULL && urlCount > 0) {
    if (xSemaphoreTake(netBox.mutex, pdMS_TO_TICKS(10)) == pdTRUE) {
      if (!netBox.inFlight && !netBox.jobPending) {
        netBox.payload  = payload;
        netBox.urlCount = urlCount;
        for (int i = 0; i < urlCount; i++) netBox.urls[i] = uniqueUrls[i];
        netBox.apiKey     = state.deviceApiKey;
        netBox.jobPending = true;
      }
      xSemaphoreGive(netBox.mutex);
    }
  }
}

void telemetryNetworkTask(void* pvParameters) {
  (void)pvParameters;
  int consecutiveFailRounds = 0;
  unsigned long nextFullRetryMs = 0;

  for (;;) {
    if (netBox.pauseRequested || netBox.mutex == NULL) {
      vTaskDelay(pdMS_TO_TICKS(80));
      continue;
    }

    String localPayload;
    String localUrls[4];
    int    localUrlCount = 0;
    String localApiKey;
    bool   hasJob = false;

    if (xSemaphoreTake(netBox.mutex, pdMS_TO_TICKS(50)) == pdTRUE) {
      if (netBox.jobPending) {
        localPayload     = netBox.payload;
        localUrlCount    = netBox.urlCount;
        for (int i = 0; i < localUrlCount && i < 4; i++) localUrls[i] = netBox.urls[i];
        localApiKey       = netBox.apiKey;
        netBox.jobPending = false;
        netBox.inFlight   = true;
        hasJob            = true;
      }
      xSemaphoreGive(netBox.mutex);
    }

    if (!hasJob) {
      vTaskDelay(pdMS_TO_TICKS(50));
      continue;
    }

    int triesAllowed = localUrlCount;
    if (consecutiveFailRounds >= 2 && (long)(millis() - nextFullRetryMs) < 0) {
      triesAllowed = 1; // Hindari menumpuk fallback saat internet putus; coba URL utama saja sampai backoff selesai
    }

    String responseBody;
    int httpCode = -1;
    for (int i = 0; i < triesAllowed; i++) {
      if (netBox.pauseRequested || WiFi.status() != WL_CONNECTED) break;

      uint16_t timeoutMs = localUrls[i].startsWith("https://") ? 5000 : (i == 0 ? 3000 : 1800);
      if (i > 0) {
        Serial.printf("[HTTP] URL sebelumnya gagal (HTTP %d), mencoba cadangan #%d (%s)...\n",
                      httpCode, i, localUrls[i].c_str());
      }

      HTTPClient http;
      WiFiClientSecure secureClient;
      if (localUrls[i].startsWith("https://")) {
        secureClient.setInsecure();
        http.begin(secureClient, localUrls[i]);
      } else {
        http.begin(localUrls[i]);
      }
      http.setTimeout(timeoutMs);
      http.addHeader("Content-Type", "application/json");
      http.addHeader("X-Device-Key", localApiKey);

      httpCode = http.POST(localPayload);
      if (httpCode >= 200 && httpCode < 300) {
        responseBody = http.getString();
      }
      http.end();

      if (httpCode >= 200 && httpCode < 300) {
        if (i > 0) {
          Serial.printf("[HTTP] Berhasil lewat URL cadangan (%s)\n", localUrls[i].c_str());
        }
        break;
      }
    }

    bool ok = (httpCode >= 200 && httpCode < 300);
    if (ok) {
      consecutiveFailRounds = 0;
    } else {
      consecutiveFailRounds++;
      if (consecutiveFailRounds >= 2) {
        nextFullRetryMs = millis() + NET_FAILURE_BACKOFF_MS;
      }
    }

    if (xSemaphoreTake(netBox.mutex, portMAX_DELAY) == pdTRUE) {
      netBox.inFlight        = false;
      netBox.resultConnected = ok;
      netBox.resultBody      = responseBody;
      netBox.resultHttpCode  = httpCode;
      netBox.resultReady     = true;
      xSemaphoreGive(netBox.mutex);
    }

    vTaskDelay(pdMS_TO_TICKS(50));
  }
}

void applyPendingNetworkResult() {
  if (netBox.mutex == NULL) return;
  bool hasResult = false;
  bool ok = false;
  String body;
  int httpCode = -1;

  if (xSemaphoreTake(netBox.mutex, pdMS_TO_TICKS(5)) == pdTRUE) {
    if (netBox.resultReady) {
      hasResult          = true;
      ok                 = netBox.resultConnected;
      body               = netBox.resultBody;
      httpCode           = netBox.resultHttpCode;
      netBox.resultReady = false;
      netBox.resultBody  = "";
    }
    xSemaphoreGive(netBox.mutex);
  }
  if (!hasResult) return;

  state.backendChecked = true;
  if (ok) {
    state.backendConnected = true;
    if (state.commAlertPending) state.commAlertPending = false;

    if (body.length() > 2) {
      JsonDocument respDoc;
      if (!deserializeJson(respDoc, body)) {
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

  // 1. Konfigurasi Pin Tombol Start (GPIO 26 & GPIO 4) dan Buzzer
  pinMode(PIN_BTN_START, INPUT_PULLUP);
  pinMode(PIN_BTN_START_ALT, INPUT_PULLUP);
  pinMode(PIN_BUZZER_SFM27, OUTPUT);
  digitalWrite(PIN_BUZZER_SFM27, LOW);

  // 2. Inisialisasi Bus I2C-1 (GPIO 21 = SDA, GPIO 22 = SCL) & Bus I2C-2 (GPIO 16 = SDA2, GPIO 17 = SCL2)
  Wire.begin(PIN_I2C_SDA, PIN_I2C_SCL);
  I2C_Dua.begin(PIN_I2C2_SDA, PIN_I2C2_SCL);

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

  // [B2] Layar OLED-2 0.96" SSD1306 (Alamat 0x3C di I2C-2: GPIO 16/17)
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

  // Terapkan tingkat kecerahan OLED tersimpan
  applyOledContrastSettings();

  // 3. Inisialisasi Servo MG996R (GPIO 18) via Hardware LEDC 14-bit langsung (bebas bug Core 3.x)
  initServoHardware();
  int savedVentAngle = state.ventAngle;
  writeServoRawAngle(0);   delay(350);
  writeServoRawAngle(45);  delay(350);
  writeServoRawAngle(90);  delay(350);
  state.ventAngle = -1; // paksa log & tulis ulang posisi awal
  applyVentilationOutput(savedVentAngle);

  // 4. Inisialisasi RobotDyn AC Dimmer (Z-C = GPIO 34, DIM = GPIO 25)
  initRobotDynDimmer();
  applyLampOutput(state.lampIntensity);

  // 5. Baca sensor, perbarui siklus 21 hari, & hitung kontrol awal sebelum Wi-Fi
  pollPhysicalSwitches();
  readSensorData();
  updateIncubationCycle();
  evaluateFuzzyControl();

  // 6. Koneksi Wi-Fi (atau langsung nyalakan Hotspot Setup jika NVS belum punya Wi-Fi / gagal konek)
  if (!state.wifiSsid.isEmpty()) {
    WiFi.mode(WIFI_STA);
    WiFi.begin(state.wifiSsid.c_str(), state.wifiPass.c_str());

    wifiUiPhase     = WIFI_UI_CONNECTING;
    wifiAnimStartMs = millis();
    updateOledDisplay();

    unsigned long startWifi    = millis();
    unsigned long lastBootAnim = millis();
    while (WiFi.status() != WL_CONNECTED && (millis() - startWifi < BOOT_WIFI_TIMEOUT_MS)) {
      pollPhysicalSwitches();
      updateBuzzerAlarm();
      if (millis() - lastBootAnim >= OLED_ANIM_INTERVAL_MS) {
        lastBootAnim = millis();
        updateOledDisplay();
      }
      delay(50);
    }

    if (WiFi.status() == WL_CONNECTED) {
      connectedSsidDisplay = state.wifiSsid;
      connectedIpDisplay   = WiFi.localIP().toString();
      Serial.printf("[WIFI] Terhubung ke %s (IP: %s)\n",
                    connectedSsidDisplay.c_str(), connectedIpDisplay.c_str());
      configTime(0, 0, NTP_SERVER_1, NTP_SERVER_2);

      wifiUiPhase     = WIFI_UI_CONNECTED_ANIM;
      wifiAnimStartMs = millis();
      beepWifiConnected();
    } else {
      Serial.println("[WIFI] Gagal tersambung ke Wi-Fi tersimpan, membuka Hotspot Setup...");
      WiFi.disconnect(false);
      startWifiPortal();
    }
  } else {
    Serial.println("[WIFI] Belum ada Wi-Fi di NVS, membuka Hotspot Setup...");
    startWifiPortal();
  }

  updateOledDisplay();

  // 7. Jalankan task jaringan HTTP/HTTPS di Core 0 agar loop kendali suhu 2s di Core 1 tidak terblokir
  netBox.mutex = xSemaphoreCreateMutex();
  if (netBox.mutex != NULL) {
    xTaskCreatePinnedToCore(
      telemetryNetworkTask,
      "TelemetryNet",
      10240,
      NULL,
      1,
      &netTaskHandle,
      0
    );
  }
}

void loop() {
  unsigned long nowMs = millis();

  // 1. Layani Hotspot Captive Portal, transisi Wi-Fi, & animasi state
  handleWifiPortalAndState();

  // 2. Cek Push Button (Menu & Verifikasi 2 Langkah), Gerak Servo (Turning/Tes), & Buzzer Lagu Rock
  pollPhysicalSwitches();
  updateServoMotions();
  updateBuzzerAlarm();

  // 3. Terapkan respons perintah/setting dari Backend bila Core 0 sudah selesai mengirim
  applyPendingNetworkResult();

  // 4. Refresh tampilan OLED (60 ms saat tombol ditekan/menu/konfirmasi, 250 ms animasi Wi-Fi, 1s normal)
  bool fastUiActive = (localUiMode != LOCAL_UI_HOME) || (stableBtnState == LOW) || servoTestActive;
  unsigned long oledInterval = fastUiActive
    ? 60UL
    : ((wifiUiPhase != WIFI_UI_NORMAL) ? OLED_ANIM_INTERVAL_MS : OLED_UPDATE_INTERVAL_MS);
  if (nowMs - lastOledMillis >= oledInterval) {
    lastOledMillis = nowMs;
    updateOledDisplay();
  }

  // 5. Baca Sensor SHT30, Perbarui Siklus 21 Hari, & Jalankan Kendali Smooth Dimmer + 4-Mode Servo tiap 2 detik
  if (nowMs - lastSensorMillis >= SENSOR_READ_INTERVAL_MS) {
    lastSensorMillis = nowMs;
    readSensorData();
    updateIncubationCycle();
    evaluateFuzzyControl();
  }

  // 6. Antrekan pengiriman Telemetri JSON & polling Backend ke Core 0 tiap 5 detik
  if (nowMs - lastTelemetryMillis >= TELEMETRY_INTERVAL_MS) {
    lastTelemetryMillis = nowMs;
    sendTelemetryAndPollBackend();
  }
}
