<script>
  // tone: ok | warn | bad | mute. Selalu ada ikon + teks, bukan warna saja (DES 02)
  import Ikon from './Ikon.svelte';

  let { tone = 'mute', icon = '', label = '' } = $props();
  // Kode lama berupa karakter teks dipetakan ke ikon garis yang serasi dengan tema
  const peta = { '✓': 'ok', '⚠': 'bahaya', '↓': 'turun', '↑': 'naik', '⏱': 'jam', '⏸': 'jeda', '↻': 'putar', '!': 'info', '?': 'tanya', '●': 'titik', '⚙': 'otomatis' };
  const nama = $derived(peta[icon] ?? icon);
  const warna = {
    ok: 'border-good/50 bg-good/10 text-good',
    warn: 'border-warn/50 bg-warn/10 text-warn',
    bad: 'border-bad/50 bg-bad/10 text-bad',
    mute: 'border-white/20 bg-white/5 text-ink2'
  };
</script>

<span class="inline-flex items-center gap-1.5 rounded-full border px-3 py-1 text-sm font-semibold {warna[tone]}">
  {#if nama}<Ikon {nama} />{/if}{label}
</span>
