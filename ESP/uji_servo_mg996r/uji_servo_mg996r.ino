/**
 * ============================================================================
 * SKETCH KHUSUS UJI MOTOR SERVO MG996R - ESP32 DOIT DEVKIT V1
 * (Tanpa Library Tambahan - Compile Cepat & Kompatibel ESP32 Core 2.x / 3.x)
 * ============================================================================
 * Kabel Servo MG996R:
 *   - Oranye / Kuning (Signal) -> GPIO 23 ESP32
 *   - Merah (VCC)              -> +5V Power Supply Eksternal (Min. 2A - 3A)
 *   - Cokelat / Hitam (GND)    -> GND Power Supply DAN WAJIB nyambung ke GND ESP32!
 *
 * Cara Pakai:
 *   1. Upload sketch ini ke ESP32.
 *   2. Buka Serial Monitor (Baudrate 115200).
 *   3. Otomatis bergerak: 0 -> 45 -> 90 -> 135 -> 180 -> kembali ke 0 derajat.
 *   4. Ketik angka 0 s/d 180 di Serial Monitor lalu Enter untuk tahan di sudut tertentu
 *      (ketik -1 lalu Enter untuk kembali ke mode gerak otomatis).
 * ============================================================================
 */

#include <Arduino.h>
#include <esp_arduino_version.h>

#define PIN_SERVO           23    // Pin Signal Servo MG996R (bisa diganti jika tes pin lain)
#define SERVO_PULSE_MIN_US  1000  // 0 derajat   (1000 us)
#define SERVO_PULSE_MAX_US  2000  // 180 derajat (2000 us)
#define SERVO_LEDC_CHANNEL  2
#define SERVO_LEDC_BITS     14    // Resolusi 14-bit (16.384 step per 20.000 us)

int manualAngle = -1; // -1 = mode ayun otomatis, 0..180 = tahan sudut manual

void setServoAngle(int angleDeg) {
  int clamped = constrain(angleDeg, 0, 180);
  int pulseUs = map(clamped, 0, 180, SERVO_PULSE_MIN_US, SERVO_PULSE_MAX_US);
  uint32_t maxDuty = (1UL << SERVO_LEDC_BITS) - 1UL;
  uint32_t duty    = (uint32_t)(((uint64_t)pulseUs * maxDuty + 10000ULL) / 20000ULL);

#if ESP_ARDUINO_VERSION >= ESP_ARDUINO_VERSION_VAL(3, 0, 0)
  ledcWrite(PIN_SERVO, duty);
#else
  ledcWrite(SERVO_LEDC_CHANNEL, duty);
#endif

  Serial.printf("[SERVO] Sudut: %3d deg | Pulsa: %4d us | Duty: %lu\n",
                clamped, pulseUs, (unsigned long)duty);
}

void setup() {
  Serial.begin(115200);
  delay(400);
  Serial.println("\n=== UJI FISIK SERVO MG996R (GPIO 23) ===");

#if ESP_ARDUINO_VERSION >= ESP_ARDUINO_VERSION_VAL(3, 0, 0)
  bool ok = ledcAttachChannel(PIN_SERVO, 50, SERVO_LEDC_BITS, SERVO_LEDC_CHANNEL);
#else
  double f = ledcSetup(SERVO_LEDC_CHANNEL, 50, SERVO_LEDC_BITS);
  ledcAttachPin(PIN_SERVO, SERVO_LEDC_CHANNEL);
  bool ok = (f > 0.0);
#endif

  Serial.printf("Inisialisasi PWM 50Hz GPIO %d: %s\n", PIN_SERVO, ok ? "BERHASIL" : "GAGAL");
  Serial.println("Ketik angka 0-180 di Serial Monitor untuk uji sudut manual, atau -1 untuk mode otomatis.");
}

void checkSerialInput() {
  if (Serial.available() > 0) {
    String input = Serial.readStringUntil('\n');
    input.trim();
    if (input.length() > 0) {
      int val = input.toInt();
      if (val < 0) {
        manualAngle = -1;
        Serial.println("-> Kembali ke mode ayun otomatis.");
      } else {
        manualAngle = constrain(val, 0, 180);
        Serial.printf("-> Mode manual dikunci ke %d derajat.\n", manualAngle);
        setServoAngle(manualAngle);
      }
    }
  }
}

void loop() {
  checkSerialInput();

  if (manualAngle >= 0) {
    delay(50);
    return;
  }

  // Tahap 1: Lompat per 45 derajat agar terlihat tegas
  const int testSteps[] = { 0, 45, 90, 135, 180, 90, 0 };
  for (size_t i = 0; i < sizeof(testSteps) / sizeof(testSteps[0]); i++) {
    checkSerialInput();
    if (manualAngle >= 0) return;
    setServoAngle(testSteps[i]);
    delay(1000);
  }

  // Tahap 2: Sapuan halus (Sweep) 0 -> 90 -> 0 derajat
  Serial.println("[SWEEP] Sapuan halus 0 -> 90 -> 0 derajat...");
  for (int a = 0; a <= 90; a += 3) {
    checkSerialInput();
    if (manualAngle >= 0) return;
    setServoAngle(a);
    delay(30);
  }
  delay(400);
  for (int a = 90; a >= 0; a -= 3) {
    checkSerialInput();
    if (manualAngle >= 0) return;
    setServoAngle(a);
    delay(30);
  }
  delay(800);
}
