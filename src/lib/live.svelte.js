// Satu-satunya "pemantau" data alat untuk seluruh aplikasi: dipanggil sekali dari layout,
// semua halaman membaca hasilnya dari sini (tidak ada polling dobel).
import { getTelemetry, getThresholds, getAlerts } from './api.js';
import { ageSeconds } from './format.js';

export const live = $state({
  data: null,
  error: '',
  now: Date.now(), // jam yang sudah dikoreksi dengan jam server (kalau server_time tersedia)
  offset: 0,
  thresholds: { temp_min: 36.5, temp_max: 38.5, stale_seconds: 15 },
  alerts: [],
  izin: 'unsupported' // izin notifikasi browser: default | granted | denied | unsupported
});

let timers = [];
let berjalan = false;
let alertTerlihat = null; // id peringatan aktif yang sudah pernah dilihat
let incSebelumnya = null;

function bacaIzin() {
  live.izin = typeof Notification === 'undefined' ? 'unsupported' : Notification.permission;
}

function beritahu(judul, isi, tag) {
  if (typeof Notification === 'undefined' || Notification.permission !== 'granted') return;
  try { new Notification(judul, { body: isi, tag }); } catch { /* abaikan */ }
}

async function muatTelemetri() {
  try {
    const d = await getTelemetry();
    live.offset = d.server_time ? new Date(d.server_time).getTime() - Date.now() : 0;
    live.now = Date.now() + live.offset;
    live.data = d;
    live.error = '';
    const st = d.incubation?.status ?? null;
    if (incSebelumnya === 'running' && st === 'finished') {
      beritahu('Inkubasi selesai', 'Siklus inkubasi sudah selesai. Telur memasuki waktu menetas.', 'selesai');
    }
    incSebelumnya = st;
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
        beritahu(x.severity === 'high' ? '⚠ Peringatan penting' : 'Peringatan inkubator', x.message, 'alert-' + x.id);
      }
    }
  } catch { /* gagal sesaat: dicoba lagi 10 detik berikutnya */ }
}

export function startLive() {
  if (berjalan) return;
  berjalan = true;
  bacaIzin();
  getThresholds().then((t) => (live.thresholds = t)).catch(() => {});
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
}

export async function refresh() {
  await Promise.all([muatTelemetri(), muatAlerts()]);
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
  beritahu('Notifikasi aktif', 'Ini contoh notifikasi dari Smart Hatchery.', 'uji');
}
