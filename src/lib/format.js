// Fungsi kecil untuk mengubah waktu jadi teks yang mudah dibaca
export function ageSeconds(iso, now) {
  return Math.max(0, Math.round((now - new Date(iso).getTime()) / 1000));
}

export function ageText(s) {
  if (s < 60) return `${s} detik lalu`;
  if (s < 3600) return `${Math.floor(s / 60)} menit lalu`;
  return `${Math.floor(s / 3600)} jam lalu`;
}

export function nextText(iso, now) {
  if (!iso) return '—';
  const s = Math.round((new Date(iso).getTime() - now) / 1000);
  if (s <= 0) return 'sekarang';
  if (s < 3600) return `${Math.ceil(s / 60)} menit lagi`;
  return `${Math.floor(s / 3600)} jam lagi`;
}

export function tanggalText(iso) {
  return new Date(iso).toLocaleString('id-ID', { day: 'numeric', month: 'short', hour: '2-digit', minute: '2-digit' });
}

export function durasiText(s) {
  if (s <= 0) return 'selesai';
  const jamTotal = Math.floor(s / 3600);
  const hari = Math.floor(jamTotal / 24);
  const jam = jamTotal % 24;
  const menit = Math.floor((s % 3600) / 60);
  if (hari > 0) return `${hari} hari ${jam} jam`;
  if (jamTotal > 0) return `${jamTotal} jam ${menit} menit`;
  return `${menit} menit`;
}

export function hariKe(startIso, now) {
  return Math.floor((now - new Date(startIso).getTime()) / 86400000) + 1;
}

export function sinyalText(rssi) {
  if (rssi == null) return 'tidak diketahui';
  if (rssi >= -60) return 'kuat';
  if (rssi >= -75) return 'sedang';
  return 'lemah';
}
