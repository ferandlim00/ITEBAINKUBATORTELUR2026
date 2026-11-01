// Data dummy. Bentuknya sengaja dibuat mirip respons API yang nanti diberikan backend.
// Kalau Dodos ubah bentuk JSON, cukup sesuaikan file ini dan src/lib/api.js.

const now = Date.now();
const ago = (detik) => new Date(now - detik * 1000).toISOString();

// Satu proses inkubasi dan satu pembalik telur: semua telur dibalik bersamaan
export const telemetryNormal = {
  device_id: 'inkubator-01',
  connection: 'online', // online | offline
  updated_at: ago(3),
  temperature: 37.5, // °C
  target_temperature: 37.5, // °C, diatur operator
  humidity: 58, // %RH, hanya indikator pengaman
  // Proses inkubasi: status idle | running | finished. stage: turning | lockdown
  incubation: {
    status: 'running',
    current_day: 18,
    total_days: 21,
    started_at: ago(86400 * 17),
    finish_at: ago(-86400 * 4),
    stage: 'turning'
  },
  device_condition: {
    system_health: 'normal', // normal | warning | fault
    uptime_seconds: 1490400,
    summary: 'Hari ke-18 (Fase Turning). Suhu dan aktuator bekerja normal.'
  },
  lamp: { mode: 'auto', intensity: 62, fuzzy_active: true }, // mode: auto | manual, intensity 0-100 (%)
  ventilation: { mode: 'auto', angle: 35, fuzzy_active: true }, // angle 0 (tutup) - 90 (buka), derajat
  motor: { running: false, encoder_ok: true },
  // status: idle (menunggu jadwal) | turning | lockdown | fault
  turning: { status: 'idle', last_turned_at: ago(3600 * 2), next_turning_at: ago(-3600) },
  wifi: { connected: true, ssid: 'IncubatorNet', rssi: -58 } // rssi dalam dBm
};

// Data kedaluwarsa: sudah lama tidak diperbarui (FUN 01, DES 02)
export const telemetryStale = {
  ...telemetryNormal,
  connection: 'offline',
  updated_at: ago(60 * 12)
};

// Suhu melewati ambang + motor pembalik macet
export const telemetryAlert = {
  ...telemetryNormal,
  temperature: 39.4,
  device_condition: { system_health: 'fault', uptime_seconds: 1490400, summary: 'Suhu di atas batas dan motor pembalik bermasalah.' },
  motor: { running: false, encoder_ok: false },
  turning: { ...telemetryNormal.turning, status: 'fault' }
};

// Inkubasi sudah selesai (untuk menguji penanda selesai)
export const telemetryDone = {
  ...telemetryNormal,
  incubation: { status: 'finished', current_day: 21, total_days: 21, started_at: ago(86400 * 21 + 3600), finish_at: ago(3600), stage: 'lockdown' },
  device_condition: { system_health: 'normal', uptime_seconds: 1900000, summary: 'Siklus 21 hari selesai. Alat siap untuk inkubasi berikutnya.' },
  turning: { status: 'lockdown', last_turned_at: ago(86400 * 3), next_turning_at: null }
};

export const alerts = [
  { id: 1, type: 'over_temperature', severity: 'high', message: 'Suhu 39.4°C melewati batas atas', timestamp: ago(120), status: 'active' },
  { id: 2, type: 'motor_fault', severity: 'high', message: 'Motor pembalik menyala tetapi encoder tidak mendeteksi pulsa', timestamp: ago(300), status: 'active' },
  { id: 3, type: 'communication', severity: 'medium', message: 'Perangkat tidak mengirim data selama 5 menit', timestamp: ago(3600), status: 'resolved' },
  { id: 4, type: 'sensor_error', severity: 'medium', message: 'Pembacaan sensor SHT31 gagal 3 kali berturut-turut', timestamp: ago(3600 * 20), status: 'resolved' },
  { id: 5, type: 'low_temperature', severity: 'low', message: 'Suhu 36.2°C di bawah batas bawah', timestamp: ago(3600 * 26), status: 'resolved' }
];

export const auditLogs = [
  { id: 1, user: 'operator2', event: 'login', detail: 'Login berhasil', timestamp: ago(900) },
  { id: 2, user: 'operator1', event: 'command', detail: 'Lampu diubah ke mode manual', timestamp: ago(600) },
  { id: 3, user: 'operator1', event: 'access_denied', detail: 'Akses ditolak: sesi tidak valid', timestamp: ago(200) },
  { id: 4, user: 'operator1', event: 'command', detail: 'Pembalikan manual', timestamp: ago(3600 * 3) },
  { id: 5, user: 'operator2', event: 'config_change', detail: 'Target suhu diubah ke 37.5°C', timestamp: ago(3600 * 8) },
  { id: 6, user: 'operator1', event: 'login', detail: 'Login berhasil', timestamp: ago(3600 * 9) },
  { id: 7, user: 'operator1', event: 'command', detail: 'Ventilasi diubah ke mode otomatis', timestamp: ago(3600 * 30) },
  { id: 8, user: 'operator2', event: 'config_change', detail: 'SSID Wi-Fi diubah', timestamp: ago(3600 * 24 * 4) }
];

// Tambahan data contoh supaya paginasi Riwayat bisa dicoba
const contoh = [
  ['command', 'Lampu diubah ke mode otomatis'], ['login', 'Login berhasil'], ['command', 'Ventilasi diubah ke 45°'],
  ['config_change', 'Parameter motor diubah'], ['command', 'Pembalikan manual']
];
for (let i = 0; i < 40; i++) {
  const [event, detail] = contoh[i % 5];
  auditLogs.push({ id: 100 + i, user: i % 3 ? 'operator1' : 'operator2', event, detail, timestamp: ago(3600 * (10 + i * 3)) });
}

export const users = [{ id: 1, username: 'operator1', role: 'operator' }];
