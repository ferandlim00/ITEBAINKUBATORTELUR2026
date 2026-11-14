<script>
  // Set ikon garis membulat dengan isi tipis (duotone) agar serasi dengan tema. Warna mengikuti teks sekitarnya (currentColor).
  let { nama, class: kelas = '', ukuran = '' } = $props();
  const duo = 'currentColor';
</script>

<svg class="ikon {kelas}" style={ukuran ? `width:${ukuran};height:${ukuran}` : undefined} viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="1.9" stroke-linecap="round" stroke-linejoin="round" aria-hidden="true">
  {#if nama === 'ok'}
    <circle cx="12" cy="12" r="9" fill={duo} fill-opacity="0.14" /><path d="M8.2 12.4l2.5 2.5 5.1-5.3" />
  {:else if nama === 'bahaya'}
    <path d="M10.3 4.3L3.1 17a2 2 0 0 0 1.7 3h14.4a2 2 0 0 0 1.7-3L13.7 4.3a2 2 0 0 0-3.4 0z" fill={duo} fill-opacity="0.14" /><path d="M12 9.5v3.8" /><path d="M12 16.8h.01" />
  {:else if nama === 'turun'}
    <circle cx="12" cy="12" r="9" fill={duo} fill-opacity="0.14" /><path d="M12 8v8" /><path d="M8.6 12.6L12 16l3.4-3.4" />
  {:else if nama === 'naik'}
    <circle cx="12" cy="12" r="9" fill={duo} fill-opacity="0.14" /><path d="M12 16V8" /><path d="M8.6 11.4L12 8l3.4 3.4" />
  {:else if nama === 'jam'}
    <circle cx="12" cy="12" r="9" fill={duo} fill-opacity="0.14" /><path d="M12 7.5V12l3 1.8" />
  {:else if nama === 'jeda'}
    <circle cx="12" cy="12" r="9" fill={duo} fill-opacity="0.14" /><path d="M10 9v6" /><path d="M14 9v6" />
  {:else if nama === 'putar'}
    <path d="M20 12a8 8 0 1 1-2.6-5.9" /><path d="M20 4.5v4.2h-4.2" />
  {:else if nama === 'info'}
    <circle cx="12" cy="12" r="9" fill={duo} fill-opacity="0.14" /><path d="M12 11v5" /><path d="M12 7.9h.01" />
  {:else if nama === 'tanya'}
    <circle cx="12" cy="12" r="9" fill={duo} fill-opacity="0.14" /><path d="M9.6 9.4a2.5 2.5 0 1 1 3.6 2.2c-.8.5-1.2 1-1.2 1.9" /><path d="M12 16.9h.01" />
  {:else if nama === 'titik'}
    <circle cx="12" cy="12" r="4.2" fill={duo} stroke="none" />
  {:else if nama === 'otomatis'}
    <path d="M11 4l1.9 5 5 1.9-5 1.9-1.9 5-1.9-5-5-1.9 5-1.9z" fill={duo} fill-opacity="0.14" /><path d="M19 15.2v3.2" /><path d="M17.4 16.8h3.2" />
  {:else if nama === 'pasir'}
    <path d="M7 3.5h10" /><path d="M7 20.5h10" /><path d="M8 3.5v3a4 4 0 0 0 1.5 3.1L12 12l2.5-2.4A4 4 0 0 0 16 6.5v-3" fill={duo} fill-opacity="0.14" /><path d="M8 20.5v-3a4 4 0 0 1 1.5-3.1L12 12l2.5 2.4A4 4 0 0 1 16 17.5v3" />
  {:else if nama === 'bel'}
    <path d="M6.2 9.5a5.8 5.8 0 0 1 11.6 0c0 5.2 2 6.5 2 6.5H4.2s2-1.3 2-6.5z" fill={duo} fill-opacity="0.14" /><path d="M10.2 19.4a2 2 0 0 0 3.6 0" />
  {:else if nama === 'home'}
    <path d="M5.5 9.8V19a1.5 1.5 0 0 0 1.5 1.5h10a1.5 1.5 0 0 0 1.5-1.5V9.8L12 4.6z" fill={duo} fill-opacity="0.14" stroke="none" /><path d="M3.5 11L12 3.8 20.5 11" /><path d="M5.5 9.8V19a1.5 1.5 0 0 0 1.5 1.5h10a1.5 1.5 0 0 0 1.5-1.5V9.8" /><path d="M10 20.5v-5h4v5" />
  {:else if nama === 'kontrol'}
    <rect x="2.8" y="7.2" width="18.4" height="9.6" rx="4.8" fill={duo} fill-opacity="0.14" /><circle cx="16.4" cy="12" r="2.4" fill={duo} />
  {:else if nama === 'grafik'}
    <path d="M3.5 3.8v16.7h17" /><path d="M7 15.5l3.8-4.4 3 2.6 5-6" /><circle cx="18.8" cy="7.7" r="1.2" fill={duo} stroke="none" />
  {:else if nama === 'atur'}
    <circle cx="12" cy="12" r="6.6" fill={duo} fill-opacity="0.14" /><circle cx="12" cy="12" r="2.6" />
    {#each [0, 45, 90, 135, 180, 225, 270, 315] as a}<path d="M12 3v2" transform="rotate({a} 12 12)" />{/each}
  {:else if nama === 'riwayat'}
    <path d="M3.6 12a8.4 8.4 0 1 0 2.5-6" /><path d="M3.8 4.5v3.9h3.9" /><path d="M12 7.8V12l3 1.8" />
  {:else if nama === 'keluar'}
    <path d="M9.5 4H6a2 2 0 0 0-2 2v12a2 2 0 0 0 2 2h3.5" /><path d="M15.5 8l4 4-4 4" /><path d="M19.5 12h-10" />
  {:else if nama === 'kiri'}
    <path d="M14.5 6l-6 6 6 6" />
  {:else if nama === 'kanan'}
    <path d="M9.5 6l6 6-6 6" />
  {:else if nama === 'menu'}
    <path d="M4 7h16" /><path d="M4 12h16" /><path d="M4 17h10" />
  {:else if nama === 'mata'}
    <path d="M2.5 12S6 5.5 12 5.5 21.5 12 21.5 12 18 18.5 12 18.5 2.5 12 2.5 12z" /><circle cx="12" cy="12" r="2.8" fill={duo} fill-opacity="0.14" />
  {:else if nama === 'mata-coret'}
    <path d="M9.9 5.8A9.7 9.7 0 0 1 12 5.5c6 0 9.5 6.5 9.5 6.5a16 16 0 0 1-2.6 3.3" /><path d="M6.1 7.5C3.6 9.2 2.5 12 2.5 12S6 18.5 12 18.5c1.5 0 2.8-.4 4-1" /><path d="M4 4l16 16" />
  {:else if nama === 'suhu'}
    <path d="M10 14.4V5.2a2 2 0 1 1 4 0v9.2a4 4 0 1 1-4 0z" fill={duo} fill-opacity="0.14" /><path d="M12 9v7" /><circle cx="12" cy="16.6" r="1.1" fill={duo} stroke="none" />
  {:else if nama === 'lembap'}
    <path d="M12 3.5S5.5 10.2 5.5 14.6a6.5 6.5 0 0 0 13 0C18.5 10.2 12 3.5 12 3.5z" fill={duo} fill-opacity="0.14" /><path d="M9 15.2a3 3 0 0 0 3 3" />
  {:else if nama === 'lampu'}
    <path d="M12 3.5a6 6 0 0 0-3.6 10.8c.7.6 1.1 1.4 1.1 2.2v.9h5v-.9c0-.8.4-1.6 1.1-2.2A6 6 0 0 0 12 3.5z" fill={duo} fill-opacity="0.14" /><path d="M9.8 19.2h4.4" /><path d="M10.6 21.2h2.8" />
  {:else if nama === 'angin'}
    <path d="M3 8.5h10.5a2.5 2.5 0 1 0-2.5-2.5" /><path d="M3 12.5h14.5a2.5 2.5 0 1 1-2.5 2.5" /><path d="M3 16.5h6.5" />
  {:else if nama === 'wifi'}
    <path d="M3 9.4a13.5 13.5 0 0 1 18 0" /><path d="M6 12.9a9 9 0 0 1 12 0" /><path d="M9 16.3a4.5 4.5 0 0 1 6 0" /><circle cx="12" cy="19.4" r="1.2" fill={duo} stroke="none" />
  {:else if nama === 'user'}
    <circle cx="12" cy="8.2" r="3.8" fill={duo} fill-opacity="0.14" /><path d="M4.5 20c.6-3.7 3.8-5.6 7.5-5.6s6.9 1.9 7.5 5.6" />
  {:else if nama === 'kunci'}
    <rect x="4.5" y="10.5" width="15" height="10" rx="2.6" fill={duo} fill-opacity="0.14" /><path d="M8 10.5V8a4 4 0 0 1 8 0v2.5" />
  {/if}
</svg>

<style>
  .ikon { display: inline-block; width: 1.2em; height: 1.2em; vertical-align: -0.22em; flex: none; }
</style>
