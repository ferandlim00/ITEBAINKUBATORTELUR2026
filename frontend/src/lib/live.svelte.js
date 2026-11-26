// Satu-satunya "pemantau" data alat untuk seluruh aplikasi: dipanggil sekali dari layout,
// semua halaman membaca hasilnya dari sini (tidak ada polling dobel).
import { getTelemetry, getThresholds, getAlerts, getNotificationSettings } from './api.js';
import { ageSeconds } from './format.js';

const KUNCI_NOTIF = 'notif_aktif';
const KUNCI_NOTIF_JENIS = 'notif_jenis';
const DEFAULT_NOTIF_JENIS = {
  over_temperature: true,
  low_temperature: true,
  sensor_error: true,
  humidity_warning: true,
  communication: true,
  motor_fault: true,
  general: true
};

function bacaPref() {
  try { return localStorage.getItem(KUNCI_NOTIF) !== '0'; } catch { return true; }
}

function bacaJenisPref() {
  try {
    const raw = localStorage.getItem(KUNCI_NOTIF_JENIS);
    if (!raw) return { ...DEFAULT_NOTIF_JENIS };
    return { ...DEFAULT_NOTIF_JENIS, ...JSON.parse(raw) };
  } catch {
    return { ...DEFAULT_NOTIF_JENIS };
  }
}

export const live = $state({
  data: null,
  error: '',
  now: Date.now(), // jam yang sudah dikoreksi dengan jam server (kalau server_time tersedia)
  offset: 0,
  thresholds: {
    temp_min: 36.5,
    temp_max: 38.5,
    hum_min: 45,
    hum_max: 70,
    lockdown_hum_min: 60,
    lockdown_hum_max: 78,
    stale_seconds: 15
  },
  alerts: [],
  izin: 'unsupported', // izin notifikasi browser: default | granted | denied | unsupported
  notifAktif: bacaPref(), // pilihan operator: notifikasi dinyalakan atau dimatikan (disimpan di browser ini)
  notifJenis: bacaJenisPref() // saklar per jenis peringatan, disinkronkan dari server
});

let timers = [];
let berjalan = false;
let alertTerlihat = null; // id peringatan aktif yang sudah pernah dilihat
let incSebelumnya = null;
let stageSebelumnya = null;

// Menghitung batas aman kelembapan aktif sesuai fase siklus (Setter 45-70% vs Lockdown 60-78%)
export function ambangLembap(data, thresholds) {
  if (!thresholds) return { min: null, max: null };
  if (data?.incubation?.stage === 'lockdown') {
    return {
      min: thresholds.lockdown_hum_min ?? 60,
      max: thresholds.lockdown_hum_max ?? 78
    };
  }
  return {
    min: thresholds.hum_min ?? null,
    max: thresholds.hum_max ?? null
  };
}

function bacaIzin() {
  live.izin = typeof Notification === 'undefined' ? 'unsupported' : Notification.permission;
}

function beritahu(judul, isi, tag, jenis = 'general') {
  if (!live.notifAktif) return;
  if (jenis && live.notifJenis?.[jenis] === false) return;
  if (typeof Notification === 'undefined' || Notification.permission !== 'granted') return;
  try { new Notification(judul, { body: isi, tag }); } catch { /* abaikan */ }
}

export function setNotifJenis(cfg) {
  if (!cfg) return;
  const merged = { ...DEFAULT_NOTIF_JENIS };
  for (const k of Object.keys(DEFAULT_NOTIF_JENIS)) {
    if (typeof cfg[k] === 'boolean') merged[k] = cfg[k];
  }
  live.notifJenis = merged;
  try { localStorage.setItem(KUNCI_NOTIF_JENIS, JSON.stringify(merged)); } catch { /* abaikan */ }
}

async function muatTelemetri() {
  try {
    const d = await getTelemetry();
    live.offset = d.server_time ? new Date(d.server_time).getTime() - Date.now() : 0;
    live.now = Date.now() + live.offset;
    live.data = d;
    live.error = '';

    const st = d.incubation?.status ?? null;
    const stg = d.incubation?.stage ?? null;
    if (incSebelumnya && incSebelumnya !== 'running' && st === 'running') {
      beritahu('Siklus Inkubasi Dimulai', 'Siklus penetasan 21 hari resmi dimulai (Hari ke-1). Telur akan dibalik otomatis sesuai jadwal.', 'mulai', 'general');
    } else if (st === 'running' && stageSebelumnya && stageSebelumnya !== 'lockdown' && stg === 'lockdown') {
      beritahu('Memasuki Fase Lockdown (Hari 19–21)', 'Pembalikan telur dihentikan, ventilasi oksigen naik ke 40°, dan jaga kelembapan di 60%–78%.', 'lockdown', 'general');
    } else if (incSebelumnya === 'running' && st === 'finished') {
      beritahu('Siklus Inkubasi Selesai', 'Siklus 21 hari telah selesai! Telur memasuki masa menetas.', 'selesai', 'general');
    }
    incSebelumnya = st;
    stageSebelumnya = stg;
  } catch (e) {
    live.error = e.message;
  }
}

async function muatAlerts() {
  try {
    const a = await getAlerts();
    live.alerts = a;
    const aktif = a.filter((x) => x.status === 'active');
    if (alertTerlihat === null) {
      alertTerlihat = new Set(aktif.map((x) => x.id)); // yang sudah ada saat dibuka tidak diberitahukan ulang
      return;
    }
    for (const x of aktif) {
      if (!alertTerlihat.has(x.id)) {
        alertTerlihat.add(x.id);
        const judul = x.severity === 'high' ? 'Peringatan Darurat Inkubator' : 'Peringatan Inkubator';
        beritahu(judul, x.message, 'alert-' + x.id, x.type);
      }
    }
  } catch { /* gagal sesaat: dicoba lagi 10 detik berikutnya */ }
}

export function startLive() {
  if (berjalan) return;
  berjalan = true;
  bacaIzin();
  getThresholds().then((t) => (live.thresholds = t)).catch(() => {});
  getNotificationSettings().then((n) => setNotifJenis(n)).catch(() => {});
  muatTelemetri();
  muatAlerts();
  timers = [
    setInterval(muatTelemetri, 5000),
    setInterval(muatAlerts, 10000),
    setInterval(() => { live.now = Date.now() + live.offset; }, 1000)
  ];
}

export function stopLive() {
  timers.forEach(clearInterval);
  timers = [];
  berjalan = false;
  live.data = null;
  live.alerts = [];
  alertTerlihat = null;
  incSebelumnya = null;
  stageSebelumnya = null;
}

export async function refresh() {
  await Promise.all([
    muatTelemetri(),
    muatAlerts(),
    getNotificationSettings().then((n) => setNotifJenis(n)).catch(() => {})
  ]);
}

export function umurDetik() {
  return live.data ? ageSeconds(live.data.updated_at, live.now) : 0;
}

// Data dianggap kedaluwarsa kalau alat offline atau lebih lama dari stale_seconds
export function isStale() {
  return !!live.data && (live.data.connection === 'offline' || umurDetik() > live.thresholds.stale_seconds);
}

export async function aktifkanNotifikasi() {
  if (typeof Notification === 'undefined') return;
  try { await Notification.requestPermission(); } catch { /* abaikan */ }
  bacaIzin();
}

export function kirimNotifikasiUji() {
  beritahu('Notifikasi Aktif', 'Ini contoh notifikasi pop-up dari Smart Hatchery.', 'uji-' + Date.now(), null);
}

// Saklar notifikasi (FUN 06): menyalakan juga meminta izin browser kalau belum pernah diminta
export async function setNotifAktif(nilai) {
  live.notifAktif = nilai;
  try { localStorage.setItem(KUNCI_NOTIF, nilai ? '1' : '0'); } catch { /* abaikan */ }
  if (nilai && typeof Notification !== 'undefined' && Notification.permission === 'default') {
    try { await Notification.requestPermission(); } catch { /* abaikan */ }
    bacaIzin();
  }
}
