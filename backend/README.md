# Smart Hatchery Monitoring — Backend Go Fiber

Backend REST API berbasis **Go Fiber v3** + **MySQL** untuk memantau dan mengendalikan inkubator telur IoT berbasis ESP32 DOIT DevKit V1, selaras penuh dengan:
- `PRD_Inkubator_Telur_IoT_Revisi_v2.docx` (v2.0)
- `Kontrak_API_Smart_Hatchery_v2_Final.docx` (v2.1)
- Frontend SvelteKit (`frontend/src/lib/api.js`, termasuk `/api/telemetry/history`)
- Firmware ESP32 (`ESP/sketch_nov6a/sketch_nov6a.ino`)

---

## Fitur Utama

1. **Keamanan & Autentikasi Operator**
   - Password di-hash dengan **Argon2id** (`golang.org/x/crypto/argon2`).
   - Cookie sesi `HttpOnly`, `SameSite=Strict`, `Secure` (konfigurasi via `COOKIE_SECURE`).
   - Rate-limiting login maksimal **5 percobaan gagal per menit per IP** (balas HTTP `429`).
   - Kredensial kata sandi Wi-Fi dienkripsi dengan **AES-256-GCM** (nonce unik tiap enkripsi) dan bersifat *write-only* (tidak pernah dikembalikan ke UI maupun dicatat di audit log).
2. **Validasi Ketat & Pengaman Aktuator**
   - Menolak perintah jika alat `offline` atau data kedaluwarsa (`> 15 detik`).
   - Lampu manual dikunci `0–80%`, ventilasi `0–90°`, target suhu `36.5–38.5°C`, motor `interval_minutes` `30–480`.
   - `turn_now` ditolak jika inkubasi `idle`, memasuki fase `lockdown` (Hari 19–21), motor sedang berputar, atau motor mengalami gangguan.
3. **Sinkronisasi ESP32 & Grafik Historis**
   - Menerima telemetri tiap 5 detik di `POST /api/device/telemetry` (`X-Device-Key`).
   - Mengembalikan antrean `pending_command` (`dispatched` $\rightarrow$ `executed`/`failed`) dan `settings` terbaru ke ESP32.
   - Menyediakan endpoint `GET /api/telemetry/history` dengan *downsampling/bucketing* otomatis sesuai parameter `max_points`.
   - Memantau status kedaluwarsa (`stale_seconds: 15`) di background dan memicu notifikasi **Telegram Bot** opsional untuk peringatan berlevel `high`.

---

## Cara Menjalankan di Lokal

### 1. Jalankan MySQL
Pastikan layanan MySQL berjalan di komputer Anda (misal lewat **XAMPP**, **Laragon**, atau service MySQL lokal pada port `3306`).
*(Backend otomatis membuat database `smart_hatchery`, seluruh tabel, serta akun awal `operator1` saat pertama kali dijalankan. File SQL manual juga tersedia di `internal/database/schema.sql`).*

### 2. Siapkan File `.env`
Salin `.env.example` menjadi `.env`:
```powershell
cd C:\ITIOT\backend
copy .env.example .env
```
Sesuaikan `DB_USER`, `DB_PASSWORD`, dan `DB_NAME` dengan MySQL lokal Anda.

### 3. Jalankan Backend Go Fiber
```powershell
cd C:\ITIOT\backend
go run ./cmd/server
```
Server akan berjalan di **`http://localhost:3000`**.

### 4. Sambungkan Frontend SvelteKit
Di folder `C:\ITIOT\frontend`, salin `.env.example` menjadi `.env` dan ubah menjadi:
```env
VITE_USE_MOCK=false
```
Lalu jalankan `npm run dev` di `C:\ITIOT\frontend` dan buka `http://localhost:5173`.
Login menggunakan akun default:
- **Username:** `operator1`
- **Password:** `operator12345`

---

## Menambah Akun Operator / Reset Kata Sandi (CLI)

Gunakan CLI `cmd/create-user` (otomatis melakukan hash Argon2id dan mencatat `audit_logs`):

- **Jika menjalankan via Docker / VPS:**
  ```bash
  docker exec -it smart-hatchery-backend ./create-user -username operator2 -password PasswordBaru123
  ```
- **Jika menjalankan langsung lewat Go di lokal:**
  ```powershell
  cd C:\ITIOT\backend
  go run ./cmd/create-user -username operator2 -password PasswordBaru123
  ```
- Tambahkan flag `-update` jika ingin memperbarui kata sandi akun yang sudah terdaftar:
  ```bash
  docker exec -it smart-hatchery-backend ./create-user -username operator1 -password PasswordBaru123 -update
  ```
