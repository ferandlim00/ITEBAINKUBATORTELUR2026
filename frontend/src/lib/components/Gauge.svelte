<script>
  import { titikWarna, warnaDi, zonaNilai } from '$lib/level.js';
  import { siapAnimasi } from '$lib/transisi.svelte.js';
  import Ikon from './Ikon.svelte';

  // Pengukur busur berwarna sesuai level, ujungnya membulat. Bulatan putih = nilai sekarang (menyapu dari awal),
  // segitiga oranye di luar busur = target yang diatur operator.
  let { title, ikon = '', value, unit = '', min = 0, max = 100, target = null, okMin = null, okMax = null, desimal = 1, old = false } = $props();

  const R = 78;
  const C = 2 * Math.PI * R;
  const ARC = C * 0.75; // busur 270 derajat
  const N = 100;

  const frac = (x) => Math.min(1, Math.max(0, (x - min) / (max - min)));
  const punyaBatas = $derived(okMin != null && okMax != null);
  const z = $derived(punyaBatas ? zonaNilai(value, okMin, okMax) : null);

  let siap = $state(false);
  $effect(() => {
    if (!siapAnimasi()) return; // tunggu tabir login terbuka
    const id = requestAnimationFrame(() => (siap = true));
    return () => cancelAnimationFrame(id);
  });

  // Busur dipotong tipis-tipis, tiap potongan berwarna hasil campuran halus antar zona
  const potongan = $derived.by(() => {
    const titik = punyaBatas ? titikWarna(okMin, okMax) : null;
    return Array.from({ length: N }, (_, i) => (titik ? warnaDi(min + ((i + 0.5) / N) * (max - min), titik) : '#a8957f'));
  });

  // Ujung busur dibuat bulat (oval) dengan lingkaran kecil berwarna sama dengan ujungnya
  const ujung = (derajat) => {
    const a = (derajat * Math.PI) / 180;
    return [100 + R * Math.cos(a), 100 + R * Math.sin(a)];
  };
  const [x0, y0] = ujung(135);
  const [x1, y1] = ujung(405);

  const derajat = $derived(135 + 270 * frac(siap ? value : min));
  const derajatTarget = $derived(135 + 270 * frac(target ?? min));
  const warnaNilai = $derived(old ? '#8d7a6a' : z ? z.warna : '#f0a040');
</script>

<div class="kartu p-4 {old ? 'opacity-70' : ''}">
  <div class="flex items-center gap-2.5">
    {#if ikon}<span class="grid h-9 w-9 flex-none place-items-center rounded-xl bg-ember/15 text-ember2"><Ikon nama={ikon} ukuran="1.3rem" /></span>{/if}
    <h3 class="text-base font-semibold leading-tight text-ink">{title}</h3>
  </div>
  <svg viewBox="0 0 200 180" class="mx-auto mt-1 block w-full max-w-[220px]" role="img"
    aria-label="{title}: {Number(value).toFixed(desimal)} {unit}{z ? `, ${z.label}` : ''}{target != null ? `, target ${target} ${unit}` : ''}">
    <g fill="none" opacity={old ? 0.55 : 1}>
      <g transform="rotate(135 100 100)">
        <circle cx="100" cy="100" r={R} stroke="rgba(255,255,255,0.08)" stroke-width="16" stroke-linecap="round" stroke-dasharray="{ARC} {C}" />
        {#each potongan as warna, i}
          <circle cx="100" cy="100" r={R} stroke={warna} stroke-width="16"
            stroke-dasharray="{ARC / N + 0.7} {C}" stroke-dashoffset={-(ARC / N) * i} />
        {/each}
      </g>
      <circle cx={x0} cy={y0} r="8" fill={potongan[0]} />
      <circle cx={x1} cy={y1} r="8" fill={potongan[N - 1]} />
    </g>

    {#if target != null}
      <!-- Penanda target: segitiga membulat di luar busur, menunjuk ke busur -->
      <g class="penanda" style="transform: rotate({derajatTarget}deg); transform-origin: 100px 100px">
        <path d="M{100 + R + 11} 100 L{100 + R + 22} 94.5 L{100 + R + 22} 105.5 Z" fill="#f0a040" stroke="#f0a040" stroke-width="3" stroke-linejoin="round" />
      </g>
    {/if}

    <!-- Penanda nilai sekarang: bulatan putih bercincin warna level -->
    <g class="penanda" style="transform: rotate({derajat}deg); transform-origin: 100px 100px; filter: drop-shadow(0 0 5px {warnaNilai})">
      <circle cx={100 + R} cy="100" r="10" fill="#f8f1ea" stroke={warnaNilai} stroke-width="4" />
    </g>

    <text x="100" y="104" text-anchor="middle" font-size="40" font-weight="700" fill={z && !old ? z.teks : '#f8f1ea'}>{Number(value).toFixed(desimal)}</text>
    <text x="100" y="130" text-anchor="middle" font-size="18" fill="#bcab9c">{unit}</text>
    <text x="30" y="172" text-anchor="middle" font-size="12" fill="#bcab9c">{min}</text>
    <text x="170" y="172" text-anchor="middle" font-size="12" fill="#bcab9c">{max}</text>
  </svg>

  <p class="mt-1 text-center text-base font-semibold" style="color: {old || !z ? '#bcab9c' : z.teks}">
    {#if old}<Ikon nama="jam" /> Data lama{:else if z}<Ikon nama={z.icon} /> {z.label}{:else}&nbsp;{/if}
  </p>
  <p class="mt-1 flex flex-wrap items-center justify-center gap-x-3 gap-y-0.5 text-xs text-mute">
    <span class="inline-flex items-center gap-1.5"><span class="inline-block h-2.5 w-2.5 rounded-full bg-ink"></span>Sekarang</span>
    {#if target != null}
      <span class="inline-flex items-center gap-1.5"><svg width="10" height="10" viewBox="0 0 10 10" aria-hidden="true"><path d="M1 5L9 1v8z" fill="#f0a040" stroke="#f0a040" stroke-width="1.5" stroke-linejoin="round" /></svg>Target {target}{unit}</span>
    {/if}
  </p>
</div>

<style>
  .penanda { transition: transform 1.1s cubic-bezier(0.22, 1, 0.36, 1); }
</style>
