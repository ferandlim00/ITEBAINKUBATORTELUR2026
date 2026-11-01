# Smart Hatchery Monitoring (Frontend)

Dashboard web untuk memantau dan mengendalikan inkubator telur berbasis IoT.
Proyek akhir mata kuliah Proyek Rekayasa Teknik Komputer.

- **Frontend:** SvelteKit (mode SPA) + Tailwind CSS (repo ini)
- **Backend:** Go Fiber + MySQL (dikerjakan terpisah)
- **Perangkat:** ESP32 DevKit V1
- **Acuan:** PRD Revisi v2.0 dan Kontrak API v2.1 Final

Saat ini frontend berjalan dengan **data dummy**. Penyambungan ke backend asli dijelaskan di bagian "Mode data".

## Menjalankan di komputer

Prasyarat: [Node.js](https://nodejs.org) versi LTS.

```
npm install
npm run dev
```

Buka http://localhost:5173.

Akun untuk mode data dummy (tidak berlaku di backend asli):

| Nama pengguna | Kata sandi |
|---|---|
| `operator1` | `operator12345` |

## Mode data: dummy atau backend

Salin `.env.example` menjadi `.env`, lalu atur satu baris ini:

```
VITE_USE_MOCK=true
```

| Nilai | Artinya |
|---|---|
| `true` | Memakai data dummy dari `src/lib/mock/` dan `src/lib/api.js`. Tidak butuh backend. |
| `false` | Mengambil data dari backend. Request `/api/*` diteruskan ke `http://localhost:3000` (lihat `vite.config.js`). |

**Penting:** jika `VITE_USE_MOCK` tidak diisi sama sekali, aplikasi **tetap memakai data dummy**. Untuk memakai backend asli, nilainya harus ditulis `false` secara eksplisit. Nilai ini tertanam saat `npm run dev` atau `npm run build` dijalankan, jadi ubah dulu sebelum build produksi.

Saat mode dummy aktif, di bagian bawah Dashboard ada pemilih skenario untuk menguji tampilan: **Normal**, **Data kedaluwarsa**, **Peringatan**, dan **Inkubasi selesai**. Pemilih ini hanya muncul saat pengembangan dengan data dummy.

## Halaman

| Alamat | Isi |
|---|---|
| `/login` | Masuk sebagai Operator (satu-satunya peran) |
| `/` | Dashboard: status utama, proses inkubasi (mulai/berhenti, hari ke-N dari 21, sisa waktu, tahapan), kondisi alat, suhu, kelembapan, lampu, ventilasi, status putar, Wi-Fi, pembalik telur |
| `/kontrol` | Lampu dan ventilasi (otomatis/manual), tombol "Balik sekarang" |
| `/pengaturan` | Target suhu, parameter motor DC, Wi-Fi perangkat, notifikasi browser |
| `/peringatan` | Daftar peringatan, bisa disaring berdasarkan status |
| `/riwayat` | Audit log dengan filter waktu dan jenis kejadian, berpaginasi (diproses di server) |

## Struktur kode

```
src/
  lib/
    api.js               satu-satunya tempat yang boleh fetch data (dummy atau backend)
    live.svelte.js       pemantau data bersama: telemetri tiap 5 detik, peringatan tiap 10 detik
    auth.svelte.js       status login
    format.js            pembantu format waktu dan durasi
    turning.js           label status pembalikan telur
    components/          kartu dan lencana yang dipakai ulang
    mock/telemetry.js    data dummy (bentuknya meniru respons API)
  routes/                halaman (lihat tabel di atas)
```

## Aturan yang dipegang di kode

- **Semua akses data lewat `src/lib/api.js`.** Halaman dan komponen tidak boleh memanggil `fetch` sendiri. Kalau bentuk data berubah, cukup sesuaikan file ini dan data dummy.
- **Data kedaluwarsa menonaktifkan kontrol.** Kalau `connection = offline` atau `updated_at` lebih lama dari `stale_seconds` (15 detik dari `/settings/thresholds`), semua tombol kontrol dan form pengaturan dikunci dengan penjelasan di layar.
- **Status selalu ikon + teks**, bukan warna saja (DES 02).
- **Kata sandi Wi-Fi hanya bisa ditulis.** Kolomnya dikosongkan setelah dikirim, tidak pernah ditampilkan kembali.
- **Batas nilai berasal dari backend** (`/settings/limits`). Frontend hanya ikut membatasi input demi kenyamanan.
- **Keamanan sebenarnya ada di backend.** Menyembunyikan menu atau tombol di frontend hanya mengatur tampilan.
- **Status perintah** dibaca dari `last_command` pada telemetri: `dispatched` (terkirim), `executed` (dijalankan alat), atau `failed`.

## Endpoint yang dipanggil frontend

Rinciannya ada di dokumen Kontrak API v2.1 Final. Ringkasnya:

| Method | Alamat |
|---|---|
| POST | `/api/auth/login`, `/api/auth/logout` |
| GET | `/api/auth/me` |
| GET | `/api/telemetry/latest` |
| GET | `/api/alerts` |
| POST | `/api/commands` |
| GET | `/api/audit-logs?from=&to=&event=&page=&limit=` |
| GET | `/api/settings/device`, `/api/settings/limits`, `/api/settings/thresholds` |
| PUT | `/api/settings/target-temperature`, `/api/settings/motor`, `/api/settings/wifi` |

Jika backend membalas 401 saat aplikasi dipakai, pengguna otomatis diarahkan ke halaman login.

## Build untuk produksi

```
npm run build
```

Hasilnya berupa situs statis di folder `build/` (SPA dengan `index.html` sebagai fallback). Server atau reverse proxy yang menyajikannya harus:

1. mengarahkan semua alamat halaman (misalnya `/kontrol`) ke `index.html`, dan
2. meneruskan `/api/*` ke backend Go Fiber.

## Alur Git

Setiap selesai satu perubahan:

```
git add .
git commit -m "tulis singkat apa yang berubah"
git push
```

Jangan pernah meng-commit file `.env`, kata sandi, atau kunci rahasia. `.env` sudah diabaikan lewat `.gitignore`.

## Catatan dan hal yang masih terbuka

- **`server_time` pada telemetri (usulan).** Frontend sudah memakainya jika ada di balasan `/api/telemetry/latest`, supaya umur data dihitung dengan jam server dan tidak bergantung pada jam komputer pengguna. Jika field ini belum ada, frontend memakai jam komputer pengguna.
- **Notifikasi browser** hanya muncul saat halaman terbuka atau diminimalkan. Notifikasi saat tab ditutup membutuhkan Web Push penuh dengan endpoint tambahan dari backend, yang belum ada di kontrak.
- **Peringatan penting ke Telegram** ditangani sepenuhnya oleh backend.
- Computer vision, ID telur, dan 3 zona independen **sudah tidak dipakai** (ditiadakan dari PRD v2).
