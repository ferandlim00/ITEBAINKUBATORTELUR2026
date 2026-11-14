// Keadaan transisi masuk (login -> dashboard). Dipakai bersama oleh halaman login dan layout,
// karena tabir oranye harus tetap tampil saat pindah halaman.
export const trans = $state({
  sedang: false, // sedang proses masuk: layout tidak boleh langsung memindahkan halaman
  tampil: false, // tabir layar penuh sedang dipasang
  penuh: false, // tabir sudah membesar ke seluruh layar
  keluar: false, // tabir sedang bergeser keluar ke kiri
  rect: null // posisi dan ukuran kartu login saat tabir mulai membesar
});

// Animasi gauge dan grafik menunggu sampai tabir mulai terbuka, supaya terlihat oleh pengguna
export function siapAnimasi() {
  return !(trans.tampil && !trans.keluar);
}
