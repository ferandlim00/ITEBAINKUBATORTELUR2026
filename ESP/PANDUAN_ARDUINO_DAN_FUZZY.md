# Panduan Persiapan Arduino IDE & Desain Logika Fuzzy (ESP32 DOIT 30-Pin)

Dokumen ini berisi seluruh bahan persiapan untuk melakukan konfigurasi, kompilasi, dan *upload* firmware [sketch_nov6a.ino](file:///c:/ITIOT/ESP/sketch_nov6a/sketch_nov6a.ino) pada **ESP32 DOIT DevKit V1 (30-Pin)** sesuai revisi mapping pin final.

---

## 1. Persiapan Board di Arduino IDE

1. Buka **Arduino IDE** $\rightarrow$ masuk ke menu **File > Preferences**.
2. Pada kolom **Additional Boards Manager URLs**, masukkan URL berikut:
   ```text
   https://espressif.github.io/arduino-esp32/package_esp32_index.json
   ```
3. Buka menu **Tools > Board > Boards Manager**, cari **`esp32`** (oleh *Espressif Systems*), lalu klik **Install** (kode di `sketch_nov6a.ino` sudah mendukung penuh versi **2.0.x** maupun **3.x**).
4. Pilih Board & Pengaturan Port:
   * **Board:** `DOIT ESP32 DEVKIT V1` (atau `ESP32 Dev Module`)
   * **Upload Speed:** `115200` atau `921600`
   * **Flash Frequency:** `80MHz`
   * **Port:** Pilih `COMx` yang muncul saat ESP32 dicolokkan (pastikan driver **CP2102** atau **CH340** sudah terinstal di Windows).

---

## 2. Daftar Lengkap Library yang Wajib Diinstal

### A. Library Eksternal (WAJIB Diinstal via **Tools > Manage Libraries...** di Arduino IDE)

Cari dan instal **7 library** berikut di *Library Manager* (pastikan klik **Install All** saat muncul konfirmasi dependensi):

| No | Nama Persis di Library Manager | File Header (`#include`) | Pembuat (Author) | Kegunaan pada Alat |
|---|---|---|---|---|
| 1 | **ArduinoJson** *(versi 7.x)* | `<ArduinoJson.h>` | *Benoit Blanchon* | Serialisasi JSON telemetri & parsing perintah Backend Go Fiber |
| 2 | **Adafruit SHT31 Library** | `<Adafruit_SHT31.h>` | *Adafruit* | Membaca sensor suhu & kelembapan **SHT30-D / SHT31-D (`0x44`)** |
| 3 | **Adafruit GFX Library** | `<Adafruit_GFX.h>` | *Adafruit* | Engine grafis inti untuk menggambar teks, garis, dan ikon bitmap (`PROGMEM`) |
| 4 | **Adafruit SSD1306** | `<Adafruit_SSD1306.h>` | *Adafruit* | Driver utama untuk **Dual OLED 0.96" 128x64 (`0x3C`)** |
| 5 | **Adafruit BusIO** | *(Dependensi I2C/SPI)* | *Adafruit* | **Wajib ada!** Dipakai di balik layar oleh `Adafruit_SHT31`, `Adafruit_SSD1306`, `Adafruit_GFX`, dan `RTClib` |
| 6 | **RTClib** | `<RTClib.h>` | *Adafruit* | Membaca & menyinkronkan waktu ke **RTC DS3231SN (`0x68`)** |
| 7 | **ESP32Servo** | `<ESP32Servo.h>` | *Kevin Harrington, John K. Bennett* | Menggerakkan **Motor Servo MG996R (`GPIO 18`)** |

### B. Library Bawaan Core ESP32 (Otomatis Terinstal Bersama Board `esp32 by Espressif Systems`)

Library berikut **tidak perlu dicari di Library Manager** karena sudah langsung tersedia di dalam paket board ESP32:
- `<Arduino.h>`, `<Wire.h>` (mendukung `Wire` & `TwoWire I2C_Dua = TwoWire(1)`)
- `<WiFi.h>`, `<WiFiClientSecure.h>` (mendukung HTTPS Domain), `<HTTPClient.h>`
- `<Preferences.h>` (penyimpanan memori NVS anti-reset), `<time.h>` (sinkronisasi jam NTP)
- `<esp_arduino_version.h>`, `<soc/gpio_struct.h>` (akses register GPIO cepat untuk ISR AC Dimmer)

---

## 3. Ringkasan Mapping Pin & Cara Kerja Perangkat Keras di Kode

| Pin ESP32 | Perangkat Keras | Konfigurasi di Kode | Cara Kerja di Firmware |
|---|---|---|---|
| **GPIO 16 (RX2)** | Jalur SDA2 I2C-2 (OLED-2) | `I2C_Dua.begin(16, 17)` | Jalur data khusus **OLED-2 (`0x3C`)** (Status Wi-Fi dBm, `SETTER`/`LOCKDOWN`, `DAY XX/21`, Waktu Mulai). |
| **GPIO 17 (TX2)** | Jalur SCL2 I2C-2 (OLED-2) | `I2C_Dua.begin(16, 17)` | Jalur clock khusus **OLED-2 (`0x3C`)**. |
| **GPIO 18** | Signal Servo MG996R (1 Unit) | `mg996rServo.attach(18, 500, 2500)` | Mengatur sudut bukaan `0°` (tutup) s/d `90°` (buka penuh) secara otomatis (Fuzzy) atau manual. Catu daya `+5V` minimal **3 A** + kapasitor **470–1000 µF** (arus stall ~2.5 A). |
| **GPIO 19** | Buzzer Aktif SFM-27 (+) | `OUTPUT` | Berbunyi *beep-beep* intermiten (300ms ON / 700ms OFF) saat suhu di luar batas aman (`< 36.5°C` atau `> 38.5°C`) atau sensor error. |
| **GPIO 21** | Jalur SDA1 I2C-1 Bersama | `Wire.begin(21, 22)` | Terhubung paralel ke **SHT30 (`0x44`)**, **OLED-1 (`0x3C`)** (Suhu, Ikon Telur, Kelembapan, Jam), dan **RTC DS3231 (`0x68`)**. |
| **GPIO 22** | Jalur SCL1 I2C-1 Bersama | `Wire.begin(21, 22)` | Jalur clock I2C-1 untuk ketiga modul di atas. |
| **GPIO 25** | PWM / DIM RobotDyn AC Dimmer | `OUTPUT` (Pulsa Gate Triac) | Mengirimkan pulsa pemicu $200\ \mu\text{s}$ lewat timer tick $100\ \mu\text{s}$ setelah jeda sudut fasa (*phase-cut*) tercapai. |
| **GPIO 26 / GPIO 4** | Push Button Start (1 Unit) | `INPUT_PULLUP` (Aktif `LOW`) | Mendukung **`GPIO 26`** (wiring utama) maupun **`GPIO 4`** (simulasi).<br>**Tekan singkat (< 2 detik):** Mulai siklus inkubasi 21 hari (`DAY 01/21`).<br>**Tekan tahan ($\ge$ 3 detik):** Hentikan/reset siklus ke `idle`. |
| **GPIO 34** | Z-C RobotDyn AC Dimmer | `INPUT` + `attachInterrupt(RISING)` | Mendeteksi titik *Zero-Crossing* gelombang AC 50Hz setiap $10\ \text{ms}$ ($10.000\ \mu\text{s}$). |

---

## 4. Desain Lengkap Sistem Inferensi Logika Fuzzy Mamdani (2-Input, 2-Output, 9 Aturan)

Agar logika kendali lebih intuitif, mudah dipahami, dan mudah dijelaskan, firmware menggunakan **Metode Fuzzy Mamdani** dengan **2 variabel input**, **2 variabel output**, dan **9 aturan (`3 × 3`)**.

Keunggulan utama **Fuzzy Mamdani** adalah baik **Input** maupun **Output** sama-sama direpresentasikan menggunakan **bahasa manusia (himpunan linguistik)** melalui **4 tahapan standar**:
1. **Fuzzifikasi (*Fuzzification*):** Mengubah nilai sensor tegas (*crisp*) menjadi derajat keanggotaan ($\mu \in [0, 1]$).
2. **Inferensi / Implikasi (`MIN`):** Mengevaluasi aturan `IF ... AND ... THEN ...` dengan operator `MIN` ($\alpha = \min(\mu_{\text{Error}}, \mu_{\Delta\text{Error}})$) untuk memotong (*clipping*) tinggi kurva himpunan output.
3. **Agregasi (`MAX`):** Menggabungkan seluruh kurva output yang terpotong menggunakan operator `MAX` menjadi satu bidang kurva gabungan.
4. **Defuzzifikasi *Centroid* (*Center of Area* / Titik Berat):** Menghitung titik pusat massa dari bidang kurva gabungan untuk menghasilkan nilai persentase lampu (`0–100%`) dan sudut servo (`0°–90°`).

---

### 4.1 Variabel Input (Fuzzifikasi)

1. **Input 1 — Error Suhu ($e = T_{\text{aktual}} - T_{\text{target}}$)** dalam satuan $^\circ\text{C}$ (3 Himpunan):
   * **Dingin (`ERR_DINGIN`):** Kurva Bahu Kiri (*Left Shoulder*) pada $[-1.0^\circ\text{C},\ 0.0^\circ\text{C}]$ (bernilai $1$ jika $e \le -1.0^\circ\text{C}$).
   * **Normal / Ideal (`ERR_NORMAL`):** Kurva Segitiga pada $[-0.5^\circ\text{C},\ 0.0^\circ\text{C},\ +0.5^\circ\text{C}]$ (bernilai puncak $1$ saat suhu tepat di target $e = 0.0^\circ\text{C}$).
   * **Panas (`ERR_PANAS`):** Kurva Bahu Kanan (*Right Shoulder*) pada $[0.0^\circ\text{C},\ +1.0^\circ\text{C}]$ (bernilai $1$ jika $e \ge +1.0^\circ\text{C}$).

2. **Input 2 — Perubahan Suhu ($\Delta e = T_{\text{aktual}} - T_{\text{sebelumnya}}$)** tiap 2 detik (3 Himpunan):
   * **Turun (`DERR_TURUN`):** Kurva Bahu Kiri pada $[-0.20^\circ\text{C},\ 0.0^\circ\text{C}]$.
   * **Stabil (`DERR_STABIL`):** Kurva Segitiga pada $[-0.10^\circ\text{C},\ 0.0^\circ\text{C},\ +0.10^\circ\text{C}]$.
   * **Naik (`DERR_NAIK`):** Kurva Bahu Kanan pada $[0.0^\circ\text{C},\ +0.20^\circ\text{C}]$.

---

### 4.2 Variabel Output (Himpunan Linguistik Mamdani)

1. **Output 1 — Intensitas Bohlam Pemanas 60W (`0% – 100%`, 5 Himpunan):**
   * **Mati (`LAMP_MATI`):** Kurva Bahu Kiri $[0\%,\ 10\%]$ *(titik berat $\approx 3\%$)*.
   * **Redup (`LAMP_REDUP`):** Kurva Segitiga $[5\%,\ 25\%,\ 45\%]$ *(titik berat $= 25\%$)*.
   * **Sedang (`LAMP_SEDANG`):** Kurva Segitiga $[30\%,\ 50\%,\ 70\%]$ *(titik berat $= 50\%$)*.
   * **Terang (`LAMP_TERANG`):** Kurva Segitiga $[55\%,\ 75\%,\ 95\%]$ *(titik berat $= 75\%$)*.
   * **Sangat Terang (`LAMP_SANGAT_TERANG`):** Kurva Bahu Kanan $[90\%,\ 100\%]$ *(titik berat $\approx 97\%$)*.

2. **Output 2 — Sudut Bukaan Servo Ventilasi MG996R (`0° – 90°`, 5 Himpunan):**
   * **Tutup (`VENT_TUTUP`):** Kurva Bahu Kiri $[0^\circ,\ 10^\circ]$ *(titik berat $\approx 3^\circ$)*.
   * **Buka Kecil (`VENT_BUKA_KECIL`):** Kurva Segitiga $[5^\circ,\ 25^\circ,\ 45^\circ]$ *(titik berat $= 25^\circ$)*.
   * **Buka Sedang (`VENT_BUKA_SEDANG`):** Kurva Segitiga $[25^\circ,\ 45^\circ,\ 65^\circ]$ *(titik berat $= 45^\circ$)*.
   * **Buka Lebar (`VENT_BUKA_LEBAR`):** Kurva Segitiga $[45^\circ,\ 65^\circ,\ 85^\circ]$ *(titik berat $= 65^\circ$)*.
   * **Buka Penuh (`VENT_BUKA_PENUH`):** Kurva Bahu Kanan $[80^\circ,\ 90^\circ]$ *(titik berat $\approx 87^\circ$)*.

---

### 4.3 Basis Aturan Fuzzy Mamdani (*Rule Base Matrix* $3 \times 3 = 9$ Aturan)

Operator logika `AND` menggunakan fungsi **MIN**: $\alpha_{i,j} = \min(\mu_{\text{Error}}[i],\ \mu_{\Delta\text{Error}}[j])$.

#### Daftar 9 Aturan Linguistik (`IF-THEN`)

| No | Aturan Kondisi (`IF`) | Keputusan Output (`THEN`) |
|---|---|---|
| **R1** | **IF** Suhu *Dingin* **AND** Perubahan *Turun* | **THEN** Lampu *Sangat Terang*, Ventilasi *Tutup* |
| **R2** | **IF** Suhu *Dingin* **AND** Perubahan *Stabil* | **THEN** Lampu *Sangat Terang*, Ventilasi *Tutup* |
| **R3** | **IF** Suhu *Dingin* **AND** Perubahan *Naik* | **THEN** Lampu *Terang*, Ventilasi *Buka Kecil* |
| **R4** | **IF** Suhu *Normal* **AND** Perubahan *Turun* | **THEN** Lampu *Terang*, Ventilasi *Buka Kecil* |
| **R5** | **IF** Suhu *Normal* **AND** Perubahan *Stabil* | **THEN** Lampu *Sedang*, Ventilasi *Buka Kecil* |
| **R6** | **IF** Suhu *Normal* **AND** Perubahan *Naik* | **THEN** Lampu *Redup*, Ventilasi *Buka Sedang* |
| **R7** | **IF** Suhu *Panas* **AND** Perubahan *Turun* | **THEN** Lampu *Redup*, Ventilasi *Buka Sedang* |
| **R8** | **IF** Suhu *Panas* **AND** Perubahan *Stabil* | **THEN** Lampu *Mati*, Ventilasi *Buka Lebar* |
| **R9** | **IF** Suhu *Panas* **AND** Perubahan *Naik* | **THEN** Lampu *Mati*, Ventilasi *Buka Penuh* |

#### Ringkasan Matriks $3 \times 3$ (Lampu / Ventilasi)

| Error Suhu ($e$) \ Perubahan ($\Delta e$) | **Suhu Turun** | **Suhu Stabil** | **Suhu Naik** |
|---|---|---|---|
| **Dingin** | *Sangat Terang* / *Tutup* | *Sangat Terang* / *Tutup* | *Terang* / *Buka Kecil* |
| **Normal (Ideal)** | *Terang* / *Buka Kecil* | *Sedang* / *Buka Kecil* | *Redup* / *Buka Sedang* |
| **Panas** | *Redup* / *Buka Sedang* | *Mati* / *Buka Lebar* | *Mati* / *Buka Penuh* |

* **Pengaman Suhu Ekstrem & Kelembapan Pasif (Sesuai `PLAN.md`):**
  - Jika suhu mencapai atau melewati batas aman atas ($T \ge 38.5^\circ\text{C}$), sistem memaksa lampu **`0%`** (mati total) dan servo **`90°`** (buka penuh). Sebaliknya jika $T \le 36.5^\circ\text{C}$, lampu dipaksa **`100%`** dan servo **`0°`**.
  - Saat mode diubah ke **Manual** oleh operator, intensitas bohlam dikunci maksimal **80%** sesuai ketetapan keamanan PRD v2.0.
  - Jika kelembapan terlalu jenuh ($\text{Humidity} > 75\%\ \text{RH}$) dan suhu tidak sedang dingin ($e > -0.3^\circ\text{C}$), sudut bukaan servo dipaksa membuka minimal **$35^\circ$** untuk membuang uap air berlebih.

---

### 4.4 Rumus Agregasi (`MAX`) & Defuzzifikasi *Centroid* (*Center of Area*)

Setelah setiap aturan menghasilkan derajat potong (*clipping level*) $\text{clip}[k]$ melalui operator `MAX`, kurva gabungan $\mu_{\text{agg}}(x)$ pada domain output dihitung dengan:

$$\mu_{\text{agg}}(x) = \max_{k=0..4} \Big( \min\big(\text{clip}[k],\ \mu_{\text{Output}_k}(x)\big) \Big)$$

Selanjutnya, nilai tegas (*crisp output*) dihitung menggunakan metode **Centroid (Titik Berat)** pada fungsi `defuzzifyCentroidLamp()` ($x = 0\dots 100$) dan `defuzzifyCentroidVent()` ($x = 0\dots 90$):

$$\text{Intensity}_{\text{Lampu}} = \frac{\sum_{x=0}^{100} x \cdot \mu_{\text{agg,Lampu}}(x)}{\sum_{x=0}^{100} \mu_{\text{agg,Lampu}}(x)}, \qquad \text{Angle}_{\text{Servo}} = \frac{\sum_{x=0}^{90} x \cdot \mu_{\text{agg,Servo}}(x)}{\sum_{x=0}^{90} \mu_{\text{agg,Servo}}(x)}$$

Nilai $\text{Intensity}_{\text{Lampu}}$ (`0–100%`) kemudian diterjemahkan menjadi tick waktu sulut Triac (`dimTargetTick`) pada gelombang AC 50Hz ($100\ \text{tick} \times 100\ \mu\text{s} = 10.000\ \mu\text{s}$ per setengah siklus):

$$\text{dimTargetTick} = \text{map}(\text{Intensity}_{\text{Lampu}},\ 3,\ 97,\ 92,\ 8)$$

---

## 5. Bagian Kode yang Perlu Anda Sesuaikan Sebelum Upload

Buka [sketch_nov6a.ino](file:///c:/ITIOT/ESP/sketch_nov6a/sketch_nov6a.ino#L65-L72) pada baris **65–72**, lalu sesuaikan konfigurasi berikut dengan Wi-Fi dan IP komputer/VPS tempat Backend Go Fiber berjalan:

```cpp
const char* DEFAULT_WIFI_SSID     = "IncubatorNet"; // Nama Wi-Fi awal
const char* DEFAULT_WIFI_PASS     = "password123";  // Password Wi-Fi awal

// Alamat endpoint Backend Go Fiber untuk menerima telemetri ESP32:
const char* BACKEND_TELEMETRY_URL = "http://192.168.1.100:3000/api/device/telemetry";
const char* DEVICE_API_KEY        = "secret-device-token-esp32";
```
