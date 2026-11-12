// Tingkat (level) nilai terhadap rentang aman, dipakai untuk mewarnai gauge dan grafik.
// Rentang aman = [okMin, okMax]. Selebar setengah rentang di luar batas dianggap "agak", sisanya "terlalu".
// Warna selalu dipasangkan dengan ikon dan tulisan, supaya tetap terbaca oleh yang sulit membedakan warna.
export const ZONA = {
  sangat_rendah: { warna: '#3b82f6', teks: '#93c5fd', label: 'Terlalu rendah', icon: 'turun', tone: 'bad' },
  rendah: { warna: '#22d3ee', teks: '#67e8f9', label: 'Agak rendah', icon: 'turun', tone: 'warn' },
  aman: { warna: '#4ade80', teks: '#86efac', label: 'Dalam batas', icon: 'ok', tone: 'ok' },
  tinggi: { warna: '#fbbf24', teks: '#fcd34d', label: 'Agak tinggi', icon: 'naik', tone: 'warn' },
  sangat_tinggi: { warna: '#ef4444', teks: '#fca5a5', label: 'Terlalu tinggi', icon: 'bahaya', tone: 'bad' }
};

export const URUTAN_ZONA = ['sangat_rendah', 'rendah', 'aman', 'tinggi', 'sangat_tinggi'];

export function margin(okMin, okMax) {
  return (okMax - okMin) / 2;
}

export function kodeZona(x, okMin, okMax) {
  const m = margin(okMin, okMax);
  if (x < okMin - m) return 'sangat_rendah';
  if (x < okMin) return 'rendah';
  if (x <= okMax) return 'aman';
  if (x <= okMax + m) return 'tinggi';
  return 'sangat_tinggi';
}

export function zonaNilai(x, okMin, okMax) {
  return ZONA[kodeZona(x, okMin, okMax)];
}

// ===== Gradasi warna halus mengikuti level =====
// Titik warna: warna murni di tengah tiap zona, di antaranya dicampur halus (tanpa tepi keras).
export function titikWarna(okMin, okMax) {
  const m = margin(okMin, okMax);
  const lebar = okMax - okMin;
  return [
    { x: okMin - 1.5 * m, c: ZONA.sangat_rendah.warna },
    { x: okMin - 0.5 * m, c: ZONA.rendah.warna },
    { x: okMin + 0.25 * lebar, c: ZONA.aman.warna },
    { x: okMax - 0.25 * lebar, c: ZONA.aman.warna },
    { x: okMax + 0.5 * m, c: ZONA.tinggi.warna },
    { x: okMax + 1.5 * m, c: ZONA.sangat_tinggi.warna }
  ];
}

const rgb = (h) => [1, 3, 5].map((i) => parseInt(h.slice(i, i + 2), 16));

// Warna pada nilai x, dicampur linear di antara dua titik warna terdekat
export function warnaDi(x, titik) {
  if (x <= titik[0].x) return titik[0].c;
  const akhir = titik[titik.length - 1];
  if (x >= akhir.x) return akhir.c;
  for (let i = 0; i < titik.length - 1; i++) {
    const a = titik[i], b = titik[i + 1];
    if (x >= a.x && x <= b.x) {
      const t = (x - a.x) / (b.x - a.x || 1);
      const ca = rgb(a.c), cb = rgb(b.c);
      return '#' + ca.map((v, k) => Math.round(v + (cb[k] - v) * t).toString(16).padStart(2, '0')).join('');
    }
  }
  return akhir.c;
}
