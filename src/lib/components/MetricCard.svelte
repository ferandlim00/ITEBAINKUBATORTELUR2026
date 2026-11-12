<script>
  import { siapAnimasi } from '$lib/transisi.svelte.js';
  import Ikon from './Ikon.svelte';

  // Kartu informasi dengan tiga tingkat yang jelas:
  //   1) JUDUL    : ikon + nama (mis. "Wi-Fi")
  //   2) NILAI    : jawaban utama di dalam panel (angka, atau status berikon)
  //   3) RINCIAN  : baris berlabel kecil (mis. "Kekuatan sinyal  Kuat")
  // persen (0-100) menambah batang level bergradasi yang terisi dari kosong saat kartu muncul.
  let { title, ikon = '', value = '', unit = '', status = null, persen = null, skala = '', baris = [], old = false } = $props();

  const gradien = {
    panas: 'linear-gradient(to right, #fcd34d, #f59e0b, #ea580c, #dc2626)',
    angin: 'linear-gradient(to right, #a5f3fc, #38bdf8, #3b82f6, #6366f1)',
    '': 'linear-gradient(to right, #86efac, #22c55e)'
  };
  const warnaTone = { ok: 'text-good', warn: 'text-warn', bad: 'text-bad', mute: 'text-ink2' };
  const p = $derived(Math.min(100, Math.max(0, persen ?? 0)));

  let siap = $state(false);
  $effect(() => {
    if (!siapAnimasi()) return;
    const id = setTimeout(() => (siap = true), 250);
    return () => clearTimeout(id);
  });
  const tampil = $derived(siap ? p : 0);
</script>

<div class="kartu flex flex-col p-3.5 sm:p-4 {old ? 'opacity-60' : ''}">
  <div class="flex items-center gap-2.5">
    {#if ikon}<span class="grid h-9 w-9 flex-none place-items-center rounded-xl bg-ember/15 text-ember2"><Ikon nama={ikon} ukuran="1.3rem" /></span>{/if}
    <h3 class="text-base font-semibold leading-tight text-ink">{title}</h3>
  </div>

  <div class="mt-3 rounded-xl bg-white/5 px-3 py-3">
    {#if status}
      <p class="flex items-center gap-2 text-lg font-bold leading-tight {warnaTone[status.tone] ?? 'text-ink'}"><Ikon nama={status.icon} ukuran="1.4rem" />{status.label}</p>
    {:else}
      <p class="text-3xl font-bold leading-none tracking-tight text-ink">{value}{#if unit}<span class="ml-1 text-base font-medium text-mute">{unit}</span>{/if}</p>
    {/if}
    {#if persen != null}
      <div class="mt-3 h-2.5 overflow-hidden rounded-full bg-white/10" role="progressbar" aria-label={title}
        aria-valuemin="0" aria-valuemax="100" aria-valuenow={Math.round(p)}>
        <div class="isi-bar h-full w-full" style="background-image: {gradien[skala] ?? gradien['']}; clip-path: inset(0 {100 - tampil}% 0 0 round 999px)"></div>
      </div>
    {/if}
  </div>

  {#if baris.length}
    <dl class="mt-3 space-y-1.5 text-sm">
      {#each baris as b}
        <div class="flex items-baseline justify-between gap-3">
          <dt class="text-mute">{b.label}</dt>
          <dd class="text-right font-semibold text-ink">{b.nilai}</dd>
        </div>
      {/each}
    </dl>
  {/if}
</div>
