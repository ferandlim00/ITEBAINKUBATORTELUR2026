// SATU-SATUNYA tempat yang boleh fetch data. Komponen/halaman memanggil fungsi di sini.
// Saat backend siap: set VITE_USE_MOCK=false di .env, lalu sesuaikan path/bentuk JSON kalau perlu.
import {
  telemetryNormal, telemetryStale, telemetryAlert, telemetryDone, alerts, auditLogs, users
} from './mock/telemetry.js';

const USE_MOCK = import.meta.env.VITE_USE_MOCK !== 'false';
const BASE = '/api';

const tunggu = (ms = 300) => new Promise((r) => setTimeout(r, ms));

// Dipanggil kalau backend membalas 401 (sesi habis) di tengah pemakaian
let onUnauthorized = null;
export function setUnauthorizedHandler(fn) { onUnauthorized = fn; }

async function request(path, options = {}) {
  const res = await fetch(BASE + path, {
    headers: { 'Content-Type': 'application/json' },
    credentials: 'include',
    ...options
  });
  if (!res.ok) {
    if (res.status === 401 && !path.startsWith('/auth/login')) onUnauthorized?.();
    const body = await res.json().catch(() => ({}));
    throw new Error(body.message ?? `Permintaan gagal (${res.status})`);
  }
  return res.json();
}

// Ganti skenario saat mengembangkan tampilan: 'normal' | 'stale' | 'alert' | 'finished'
export const mockScenario = { value: 'normal' };

// Perintah dari halaman Kontrol/Dashboard disimpan di sini, supaya data dummy ikut berubah (hanya mode mock)
const mockOverride = { lamp: null, ventilation: null, incubation: null, turn: false };
let skenarioTerakhir = 'normal';

// ---------- Autentikasi ----------
// DATA DUMMY. Kata sandi di bawah hanya untuk mode mock dan hilang saat backend dipakai.
const mockPassword = { operator1: 'operator12345' };

function simpanSesiMock(user) {
  try { user ? sessionStorage.setItem('mock_user', JSON.stringify(user)) : sessionStorage.removeItem('mock_user'); } catch { /* abaikan */ }
}

export async function login(username, password) {
  if (USE_MOCK) {
    await tunggu();
    const user = users.find((u) => u.username === username);
    if (!user || mockPassword[username] !== password) throw new Error('Nama pengguna atau kata sandi salah');
    simpanSesiMock(user);
    return { user };
  }
  return request('/auth/login', { method: 'POST', body: JSON.stringify({ username, password }) });
}

export async function logout() {
  if (USE_MOCK) { simpanSesiMock(null); return {}; }
  return request('/auth/logout', { method: 'POST' });
}

// Mengembalikan user yang sedang login, atau null
export async function getMe() {
  if (USE_MOCK) {
    try { return JSON.parse(sessionStorage.getItem('mock_user')) ?? null; } catch { return null; }
  }
  try { return (await request('/auth/me')).user; } catch { return null; }
}

// ---------- Data alat ----------
export async function getTelemetry() {
  if (USE_MOCK) {
    await tunggu();
    if (mockScenario.value !== skenarioTerakhir) {
      skenarioTerakhir = mockScenario.value;
      mockOverride.lamp = mockOverride.ventilation = mockOverride.incubation = null; mockOverride.turn = false;
    }
    const dasar = { normal: telemetryNormal, stale: telemetryStale, alert: telemetryAlert, finished: telemetryDone }[mockScenario.value];
    // Stempel waktu dibuat ulang tiap kali, supaya data "normal" tidak menua sendiri
    const umurDetik = mockScenario.value === 'stale' ? 60 * 12 : 3;
    const hasil = { ...dasar, updated_at: new Date(Date.now() - umurDetik * 1000).toISOString() };
    if (mockOverride.lamp) hasil.lamp = { ...hasil.lamp, ...mockOverride.lamp };
    if (mockOverride.ventilation) hasil.ventilation = { ...hasil.ventilation, ...mockOverride.ventilation };
    if (mockOverride.incubation) hasil.incubation = { ...hasil.incubation, ...mockOverride.incubation };
    if (mockOverride.turn) {
      hasil.turning = { ...hasil.turning, status: 'turning' };
      hasil.motor = { ...hasil.motor, running: true };
    }
    hasil.target_temperature = mockSettings.target_temperature;
    hasil.wifi = { ...hasil.wifi, ssid: mockSettings.wifi.ssid };
    return hasil;
  }
  return request('/telemetry/latest');
}

export async function getAlerts() {
  if (USE_MOCK) { await tunggu(); return alerts; }
  return request('/alerts');
}

export async function getAuditLogs() {
  if (USE_MOCK) { await tunggu(); return auditLogs; }
  return request('/audit-logs');
}

// Ambang peringatan dibaca saja (ditetapkan pemilik data perangkat, bukan lewat UI)
export async function getThresholds() {
  if (USE_MOCK) { await tunggu(150); return { temp_min: 36.5, temp_max: 38.5, stale_seconds: 60 }; }
  return request('/settings/thresholds');
}

// Kirim perintah ke perangkat. Bentuk perintah (usulan, belum disepakati dengan Dodos):
//   { target: 'incubation', action: 'start' | 'stop' }
//   { target: 'lamp', mode: 'auto' | 'manual', intensity?: 0-100 }
//   { target: 'ventilation', mode: 'auto' | 'manual', angle?: 0-90 }
//   { target: 'turning', action: 'turn_now' }   (balik semua telur sekarang)
export async function sendCommand(command) {
  if (USE_MOCK) {
    await tunggu(500);
    if (command.target === 'lamp') {
      mockOverride.lamp = { mode: command.mode, ...(command.intensity != null && { intensity: command.intensity }) };
    }
    if (command.target === 'ventilation') {
      mockOverride.ventilation = { mode: command.mode, ...(command.angle != null && { angle: command.angle }) };
    }
    if (command.target === 'turning') {
      mockOverride.turn = true; // motor "berputar" 8 detik, lalu kembali menunggu jadwal
      setTimeout(() => { mockOverride.turn = false; }, 8000);
    }
    if (command.target === 'incubation') {
      const sekarang = Date.now();
      mockOverride.incubation = command.action === 'start'
        ? { status: 'running', stage: 'turning', started_at: new Date(sekarang).toISOString(), finish_at: new Date(sekarang + 21 * 86400000).toISOString() }
        : { status: 'idle', stage: null, started_at: null, finish_at: null };
    }
    return { id: Date.now(), status: 'sent', ...command };
  }
  return request('/commands', { method: 'POST', body: JSON.stringify(command) });
}

// ---------- Pengaturan ----------
// Nilai di bawah hanya untuk mode mock. Batas SEMENTARA: nilai asli menunggu keputusan tim hardware.
const mockSettings = {
  target_temperature: 37.5,
  motor: { interval_minutes: 120, duration_seconds: 20, speed_percent: 50 },
  wifi: { ssid: 'IncubatorNet' } // kata sandi Wi-Fi tidak pernah dikembalikan
};
const mockLimits = {
  target_temperature: { min: 36.5, max: 38.5 },
  motor: {
    interval_minutes: { min: 30, max: 480 },
    duration_seconds: { min: 5, max: 60 },
    speed_percent: { min: 10, max: 70 }
  }
};

export async function getSettings() {
  if (USE_MOCK) { await tunggu(200); return JSON.parse(JSON.stringify(mockSettings)); }
  return request('/settings/device');
}

// Batas yang diizinkan untuk tiap pengaturan, ditetapkan di backend
export async function getLimits() {
  if (USE_MOCK) { await tunggu(200); return mockLimits; }
  return request('/settings/limits');
}

export async function saveTargetTemperature(value) {
  if (USE_MOCK) { await tunggu(500); mockSettings.target_temperature = value; return { status: 'sent' }; }
  return request('/settings/target-temperature', { method: 'PUT', body: JSON.stringify({ value }) });
}

// { interval_minutes, duration_seconds, speed_percent }
export async function saveMotorSettings(data) {
  if (USE_MOCK) { await tunggu(500); Object.assign(mockSettings.motor, data); return { status: 'sent' }; }
  return request('/settings/motor', { method: 'PUT', body: JSON.stringify(data) });
}

// { ssid, password }. Kata sandi hanya dikirim, tidak pernah disimpan atau ditampilkan di frontend
export async function saveWifi({ ssid, password }) {
  if (USE_MOCK) { await tunggu(800); mockSettings.wifi.ssid = ssid; return { status: 'sent' }; }
  return request('/settings/wifi', { method: 'PUT', body: JSON.stringify({ ssid, password }) });
}
