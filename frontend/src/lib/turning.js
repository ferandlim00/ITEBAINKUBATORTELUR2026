// Status pembalikan telur (satu proses untuk semua telur). Selalu ikon + teks, bukan warna saja
export const statusPutar = {
  idle: { label: 'Menunggu jadwal', icon: 'jeda', tone: 'mute' },
  turning: { label: 'Sedang membalik', icon: 'putar', tone: 'ok' },
  lockdown: { label: 'Lockdown', icon: 'jeda', tone: 'mute' },
  fault: { label: 'Gangguan', icon: 'bahaya', tone: 'bad' }
};
