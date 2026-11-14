<script>
  import { ZONA, URUTAN_ZONA, titikWarna, zonaNilai } from '$lib/level.js';
  import Ikon from './Ikon.svelte';
  import { siapAnimasi } from '$lib/transisi.svelte.js';

  // Grafik garis ringan (tanpa pustaka). Warna garis mengikuti level: biru = rendah, hijau = aman, kuning/merah = tinggi.
  // Tanpa rentang aman (mis. lampu) dipakai skala satu warna ('panas'). Arahkan kursor/jari atau pakai tombol panah.
  let {
    points = [], // [{ t: ISO, v: angka }]
    label, unit = '', color = '#f08040', skala = '',
    target = null, bandMin = null, bandMax = null,
    desimal = 1, spanMs = 6 * 3600 * 1000
  } = $props();

  // Kurva halus yang tidak "melompat" melewati data (interpolasi kubik monoton)
  function kurva(xs, ys) {
    const n = xs.length;
    if (n < 2) return '';
    const dx = [], m = [];
    for (let i = 0; i < n - 1; i++) { dx[i] = xs[i + 1] - xs[i] || 1e-6; m[i] = (ys[i + 1] - ys[i]) / dx[i]; }
    const t = [m[0]];
    for (let i = 1; i < n - 1; i++) t[i] = m[i - 1] * m[i] <= 0 ? 0 : (m[i - 1] + m[i]) / 2;
    t[n - 1] = m[n - 2];
    for (let i = 0; i < n - 1; i++) {
      if (m[i] === 0) { t[i] = 0; t[i + 1] = 0; continue; }
      const a = t[i] / m[i], b = t[i + 1] / m[i], q = a * a + b * b;
      if (q > 9) { const k = 3 / Math.sqrt(q); t[i] = k * a * m[i]; t[i + 1] = k * b * m[i]; }
    }
    let d = `M${xs[0].toFixed(1)},${ys[0].toFixed(1)}`;
    for (let i = 0; i < n - 1; i++) {
      d += ` C${(xs[i] + dx[i] / 3).toFixed(1)},${(ys[i] + (t[i] * dx[i]) / 3).toFixed(1)} ${(xs[i + 1] - dx[i] / 3).toFixed(1)},${(ys[i + 1] - (t[i + 1] * dx[i]) / 3).toFixed(1)} ${xs[i + 1].toFixed(1)},${ys[i + 1].toFixed(1)}`;
    }
    return d;
  }

  const H = 250, MT = 14, MB = 34;
  const gid = 'g' + Math.random().toString(36).slice(2, 9);
  let w = $state(0);
  const kecil = $derived(w < 460); // layar sempit: margin, huruf, dan garis dibuat lebih ringkas
  const ML = $derived(kecil ? 40 : 52);
  const MR = $derived(kecil ? 8 : 14);
  const desTick = desimal > 1 ? 1 : desimal;
  let idx = $state(null);
  let jalan = $state(false); // garis mulai tergambar setelah tabir login terbuka
  $effect(() => {
    if (!siapAnimasi()) return;
    const id = requestAnimationFrame(() => (jalan = true));
    return () => cancelAnimationFrame(id);
  });

  const punyaBatas = $derived(bandMin != null && bandMax != null);
  const ada = $derived(points.length > 1 && w > 0);
  const tms = $derived(points.map((p) => new Date(p.t).getTime()));
  const iw = $derived(Math.max(10, w - ML - MR));
  const ih = H - MT - MB;
  const dom = $derived.by(() => {
    if (points.length === 0) return { lo: 0, hi: 1 };
    const vs = points.map((p) => p.v);
    let lo = Math.min(...vs), hi = Math.max(...vs);
    for (const x of [target, bandMin, bandMax]) if (x != null) { lo = Math.min(lo, x); hi = Math.max(hi, x); }
    const pad = (hi - lo) * 0.12 || 1;
    return { lo: lo - pad, hi: hi + pad };
  });

  const X = (t) => ML + ((t - tms[0]) / (tms[tms.length - 1] - tms[0])) * iw;
  const Y = (v) => MT + ((dom.hi - v) / (dom.hi - dom.lo)) * ih;
  const garis = $derived(ada ? kurva(tms.map(X), points.map((p) => Y(p.v))) : '');
  const area = $derived(ada ? `${garis} L${X(tms[tms.length - 1]).toFixed(1)},${MT + ih} L${X(tms[0]).toFixed(1)},${MT + ih} Z` : '');
  const yTicks = $derived((kecil ? [0, 1, 2, 3] : [0, 1, 2, 3, 4]).map((i) => dom.lo + ((dom.hi - dom.lo) * i) / (kecil ? 3 : 4)));
  const xTicks = $derived(ada ? (kecil ? [0, 1, 2, 3] : [0, 1, 2, 3, 4]).map((i) => Math.round(((points.length - 1) * i) / (kecil ? 3 : 4))) : []);

  // Warna garis berubah menurut ketinggian nilainya (gradasi vertikal)
  const stops = $derived.by(() => {
    if (!ada) return [];
    const off = (v) => Math.min(1, Math.max(0, (Y(v) - MT) / ih));
    if (punyaBatas) {
      // Gradasi mengalir halus dari biru (rendah) lewat hijau (aman) ke kuning dan merah (tinggi)
      return titikWarna(bandMin, bandMax)
        .sort((a, b) => b.x - a.x)
        .map((t) => ({ o: off(t.x), c: t.c }));
    }
    if (skala === 'panas') return [{ o: 0, c: '#c2410c' }, { o: 0.5, c: '#f59e0b' }, { o: 1, c: '#fcd34d' }];
    return [{ o: 0, c: color }, { o: 1, c: color }];
  });

  function fmtX(ms) {
    const d = new Date(ms);
    return spanMs <= 36 * 3600 * 1000
      ? d.toLocaleTimeString('id-ID', { hour: '2-digit', minute: '2-digit' })
      : d.toLocaleDateString('id-ID', { day: 'numeric', month: 'short' });
  }
  const fmtTooltip = (ms) => new Date(ms).toLocaleString('id-ID', { day: 'numeric', month: 'short', hour: '2-digit', minute: '2-digit' });

  function terdekat(clientX, el) {
    const x = clientX - el.getBoundingClientRect().left;
    const t = tms[0] + ((x - ML) / iw) * (tms[tms.length - 1] - tms[0]);
    let best = 0;
    for (let i = 1; i < tms.length; i++) if (Math.abs(tms[i] - t) < Math.abs(tms[best] - t)) best = i;
    idx = best;
  }
  function tombol(e) {
    if (!ada) return;
    if (e.key === 'ArrowLeft') { idx = Math.max(0, (idx ?? points.length) - 1); e.preventDefault(); }
    if (e.key === 'ArrowRight') { idx = Math.min(points.length - 1, (idx ?? -1) + 1); e.preventDefault(); }
    if (e.key === 'Escape') idx = null;
  }

  const vs = $derived(points.map((p) => p.v));
  const ringkas = $derived(points.length ? { min: Math.min(...vs), max: Math.max(...vs), akhir: vs[vs.length - 1] } : null);
  const f = (x) => Number(x).toFixed(desimal);
  const zHover = $derived(idx != null && punyaBatas ? zonaNilai(points[idx].v, bandMin, bandMax) : null);
</script>

<!-- svelte-ignore a11y_no_noninteractive_tabindex, a11y_no_noninteractive_element_interactions -->
<div bind:clientWidth={w} class="relative w-full" role="group" aria-label="Grafik {label}" tabindex="0" onkeydown={tombol} onblur={() => (idx = null)}>
  {#if ada}
    <svg width={w} height={H} role="img"
      aria-label="Grafik {label}. Terendah {f(ringkas.min)} {unit}, tertinggi {f(ringkas.max)} {unit}, terakhir {f(ringkas.akhir)} {unit}. Gunakan tombol panah kiri dan kanan untuk melihat nilai.">
      <defs>
        <linearGradient id={gid} gradientUnits="userSpaceOnUse" x1="0" y1={MT} x2="0" y2={MT + ih}>
          {#each stops as s}<stop offset={s.o} stop-color={s.c} />{/each}
        </linearGradient>
      </defs>
      {#if punyaBatas}
        <rect x={ML} y={Y(bandMax)} width={iw} height={Math.max(0, Y(bandMin) - Y(bandMax))} fill="#4ade80" opacity="0.1" />
      {/if}
      {#each yTicks as v}
        <line x1={ML} x2={ML + iw} y1={Y(v)} y2={Y(v)} stroke="rgba(255,255,255,0.09)" stroke-width="1" />
        <text x={ML - 8} y={Y(v) + 5} text-anchor="end" font-size={kecil ? 12 : 14} fill="#bcab9c">{Number(v).toFixed(desTick)}</text>
      {/each}
      {#each xTicks as i}
        <text x={X(tms[i])} y={H - 10} text-anchor={i === 0 ? 'start' : i === points.length - 1 ? 'end' : 'middle'} font-size={kecil ? 12 : 14} fill="#bcab9c">{fmtX(tms[i])}</text>
      {/each}
      {#if target != null}
        <line x1={ML} x2={ML + iw} y1={Y(target)} y2={Y(target)} stroke="#e3d6ca" stroke-width="1.5" stroke-dasharray="5 5" />
      {/if}
      <path d={area} fill="url(#{gid})" opacity="0.1" />
      {#key spanMs}<path d={garis} pathLength="1" class="garis-gambar {jalan ? 'jalan' : ''}" fill="none" stroke="url(#{gid})" stroke-width={kecil ? 1.6 : 2.1} stroke-linejoin="round" stroke-linecap="round" />{/key}
      {#if idx != null}
        <line x1={X(tms[idx])} x2={X(tms[idx])} y1={MT} y2={MT + ih} stroke="#e3d6ca" stroke-width="1.5" />
        <circle cx={X(tms[idx])} cy={Y(points[idx].v)} r="7" fill="#f8f1ea" stroke={zHover ? zHover.warna : color} stroke-width="3.5" style="filter: drop-shadow(0 0 4px {zHover ? zHover.warna : color})" />
      {/if}
      <!-- svelte-ignore a11y_no_static_element_interactions -->
      <rect x={ML} y={MT} width={iw} height={ih} fill="transparent" style="touch-action: pan-y"
        onpointermove={(e) => terdekat(e.clientX, e.currentTarget.ownerSVGElement)}
        onpointerdown={(e) => terdekat(e.clientX, e.currentTarget.ownerSVGElement)}
        onpointerleave={() => (idx = null)} />
    </svg>
    {#if idx != null}
      <div class="pointer-events-none absolute top-1 z-10 rounded-xl border border-line bg-panel px-3 py-2 text-sm shadow-lg"
        style="left: {Math.min(Math.max(X(tms[idx]) - 75, 0), Math.max(0, w - 160))}px; width: 160px">
        <p class="text-mute">{fmtTooltip(tms[idx])}</p>
        <p class="text-lg font-bold" style="color: {zHover ? zHover.teks : '#f8f1ea'}">{f(points[idx].v)} {unit}</p>
        {#if zHover}<p class="font-medium" style="color: {zHover.teks}">{zHover.icon} {zHover.label}</p>{/if}
      </div>
    {/if}
  {:else}
    <p class="flex h-[250px] items-center justify-center text-mute">Belum ada data untuk ditampilkan.</p>
  {/if}
</div>
{#if ringkas}
  <dl class="mt-3 grid grid-cols-2 gap-2 text-sm sm:grid-cols-4">
    <div class="rounded-xl bg-white/5 px-3 py-2"><dt class="text-xs text-mute">Terendah</dt><dd class="font-semibold">{f(ringkas.min)} {unit}</dd></div>
    <div class="rounded-xl bg-white/5 px-3 py-2"><dt class="text-xs text-mute">Tertinggi</dt><dd class="font-semibold">{f(ringkas.max)} {unit}</dd></div>
    <div class="rounded-xl bg-white/5 px-3 py-2"><dt class="text-xs text-mute">Terakhir</dt><dd class="font-semibold">{f(ringkas.akhir)} {unit}</dd></div>
    {#if target != null}
      <div class="rounded-xl bg-white/5 px-3 py-2"><dt class="text-xs text-mute">Target (garis putus-putus)</dt><dd class="font-semibold">{target} {unit}</dd></div>
    {/if}
  </dl>
{/if}
{#if punyaBatas}
  <!-- Legenda warna: satu jalur gradasi dengan lima keterangan berikon di bawahnya -->
  <div class="mt-4" aria-label="Arti warna garis">
    <div class="h-2 rounded-full" style="background: linear-gradient(to right, {ZONA.sangat_rendah.warna}, {ZONA.rendah.warna} 25%, {ZONA.aman.warna} 50%, {ZONA.tinggi.warna} 75%, {ZONA.sangat_tinggi.warna})"></div>
    <ul class="mt-2 grid grid-cols-5 gap-1 text-center text-xs leading-tight text-mute">
      {#each URUTAN_ZONA as k}
        <li class="flex flex-col items-center gap-1"><span style="color: {ZONA[k].warna}"><Ikon nama={ZONA[k].icon} /></span>{ZONA[k].label}</li>
      {/each}
    </ul>
  </div>
{/if}
