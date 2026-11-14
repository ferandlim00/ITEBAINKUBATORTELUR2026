// Evaluasi stabilitas inkubator dari data riwayat (suhu atau kelembapan).
// Rumus dibuat sederhana dan bisa dijelaskan, dan bisa disesuaikan oleh tim.
//
//   n            = jumlah titik data (diasumsikan berjarak waktu hampir sama)
//   rata-rata    = jumlah(x) / n
//   simpangan    = akar( jumlah((x - rata-rata)^2) / n )          seberapa "bergoyang"
//   RMSE         = akar( jumlah((x - target)^2) / n )             rata-rata jarak ke target
//   deviasi maks = nilai terbesar dari |x - target|
//   dalam batas  = (jumlah titik yang berada di antara batas bawah dan atas) / n x 100%
//   skor         = 0,7 x dalam batas + 0,3 x 100 x max(0, 1 - RMSE / setengah lebar batas)
export function hitungStabilitas(nilai, { target, min, max }) {
  const n = nilai.length;
  if (n < 2 || target == null || min == null || max == null) return null;

  const rata = nilai.reduce((a, x) => a + x, 0) / n;
  const simpangan = Math.sqrt(nilai.reduce((a, x) => a + (x - rata) ** 2, 0) / n);
  const rmse = Math.sqrt(nilai.reduce((a, x) => a + (x - target) ** 2, 0) / n);
  const devMaks = Math.max(...nilai.map((x) => Math.abs(x - target)));
  const dalam = (nilai.filter((x) => x >= min && x <= max).length / n) * 100;
  const setengah = (max - min) / 2;
  const skorRmse = Math.max(0, 1 - rmse / setengah) * 100;
  const skor = Math.round(0.7 * dalam + 0.3 * skorRmse);

  let label = { teks: 'Perlu perhatian', icon: '⚠', tone: 'bad' };
  if (skor >= 90) label = { teks: 'Sangat stabil', icon: '✓', tone: 'ok' };
  else if (skor >= 75) label = { teks: 'Stabil', icon: '✓', tone: 'ok' };
  else if (skor >= 60) label = { teks: 'Cukup stabil', icon: '!', tone: 'warn' };

  return { n, rata, simpangan, rmse, devMaks, dalam, skor, label };
}

// Mencatat tiap periode saat nilai berada di luar batas (kejadian ketidakstabilan), supaya ketahuan walau tidak sedang dipantau.
// titik: [{ t: ISO, v: angka }]. Durasi dihitung dari jarak antar titik data.
export function cariKejadian(titik, { min, max, target }) {
  if (titik.length < 2 || min == null || max == null) return [];
  const jeda = (new Date(titik[titik.length - 1].t) - new Date(titik[0].t)) / (titik.length - 1);
  const daftar = [];
  let aktif = null;
  for (const p of titik) {
    const luar = p.v < min || p.v > max;
    if (luar) {
      if (!aktif) aktif = { mulai: p.t, akhir: p.t, puncak: p.v, arah: p.v > max ? 'tinggi' : 'rendah' };
      else {
        aktif.akhir = p.t;
        if (Math.abs(p.v - target) > Math.abs(aktif.puncak - target)) aktif.puncak = p.v;
      }
    } else if (aktif) {
      daftar.push(aktif);
      aktif = null;
    }
  }
  if (aktif) daftar.push(aktif);
  return daftar.map((k) => ({ ...k, durasiMs: new Date(k.akhir) - new Date(k.mulai) + jeda }));
}
