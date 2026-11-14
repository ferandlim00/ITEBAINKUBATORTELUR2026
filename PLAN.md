# Rencana Kerja

## Tujuan
Memantau serta mengontrol Incubator Telur berbasis IOT menggunakan Svelte,GoFiber,dan MYSQL dan terhubung dengan ESP32 DevKit.

## Pengguna
Operator

## Fitur
1. Dashboard - Kondisi Suhu, Mulai, Berhenti ,Timer, Status Lampu, Status Ventilasi, Status Motor DC, Status Turn, Status Koneksi Wifi, dan Status Kondisi atau Pembaruan pada incubator
2. START/FINISH - Mengontrol status hari mulai, tahapan yang diproses serta ,mendapatkan notif penyelesaian inkubator
3. Suhu - Mengatur Target Suhu yang diinginkan operator
4. Ventilasi - Mengontrol Ventilasi secara manual atau otomatis
5. Lampu - Mengontrol Lampu secara manual atau otomatis
6. Motor DC - Mengatur nilai pengaturan sesuai batas konfirgurasi.
7. Koneksi - Mengkonfirgurasi Wifi dan password yang dibutuhkan untuk disambungkan.

## Aturan
Pada Dashboard, Wajib dicantumkan seluruh kondisi dari ventilasi,Lampu, Motor DC, Turn, Koneksi , dan Suhu 
Pastikan ada Notifikasi jika sudah masuk ke dalam ranah suhu di luar ketentuan
PAstikan Ventilasi, Lampu dan Motor DC berjalan dengan lancar
Pastikan Logika Fuzzy dapat terlaksana dengan baik

## Batasan
Kelembapan semata hanya untuk menjadi pengaman dan tidak ada target.
Ada Pembatas pada Motor DC
