// Status pembalikan telur (satu proses untuk semua telur). Selalu ikon + teks, bukan warna saja
export const statusPutar = {
  idle: { label: 'Menunggu jadwal', icon: '⏸', tone: 'mute' },
  turning: { label: 'Sedang membalik', icon: '↻', tone: 'ok' },
  lockdown: { label: 'Lockdown', icon: '⏸', tone: 'mute' },
  fault: { label: 'Gangguan', icon: '⚠', tone: 'bad' }
};
