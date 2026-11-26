<script>
  import Ikon from '$lib/components/Ikon.svelte';
  import StatusBadge from './StatusBadge.svelte';
  import StatusPerintah from './StatusPerintah.svelte';
  import { siapAnimasi } from '$lib/transisi.svelte.js';
  import { durasiText, hariKe, tanggalText } from '$lib/format.js';

  // inc: { status: idle|running|finished, current_day, total_days, started_at, finish_at, stage }
  let { inc, now, terkunci = false, sibuk = false, hasil = null, onkirim } = $props();

  const LOCKDOWN_HARI = 3; // hari 19-21 (kontrak v2.1)
  const tahap = { turning: 'Masa pembalikan telur', lockdown: 'Lockdown, persiapan menetas' };

  let konfirmasi = $state(false);
  let total = $derived(inc.total_days ?? 21);
  let sisa = $derived(inc.finish_at ? Math.round((new Date(inc.finish_at).getTime() - now) / 1000) : 0);
  let hari = $derived(inc.current_day ?? (inc.started_at ? Math.min(hariKe(inc.started_at, now), total) : 0));
  let persen = $derived(inc.status === 'finished' ? 100 : Math.min(100, Math.round((hari / total) * 100)));
  let batasLockdown = $derived(((total - LOCKDOWN_HARI) / total) * 100);
  // Bagian isi yang masih di masa pembalikan berwarna hijau, bagian yang sudah masuk lockdown berwarna jingga
  // Gradasi tetap di seluruh batang: hijau-tosca di masa pembalikan, mengalir ke kuning-jingga di masa lockdown
  let gradienIsi = $derived(`linear-gradient(to right, #2dd4bf 0%, #4ade80 ${batasLockdown * 0.6}%, #a3e635 ${batasLockdown * 0.88}%, #fbbf24 ${batasLockdown}%, #f59e0b 100%)`);
  let siap = $state(false);
  $effect(() => {
    if (!siapAnimasi()) return;
    const id = setTimeout(() => (siap = true), 300);
    return () => clearTimeout(id);
  });
  let tampilPersen = $derived(siap ? persen : 0);

  function berhenti() {
    konfirmasi = false;
    onkirim('stop');
  }
</script>

<section class="kartu p-4">
  <div class="flex flex-wrap items-center justify-between gap-2">
    <h2 class="text-lg font-semibold">Proses inkubasi</h2>
    {#if inc.status === 'running'}
      <StatusBadge tone="ok" icon="↻" label="Berjalan" />
    {:else if inc.status === 'finished'}
      <StatusBadge tone="ok" icon="✓" label="Selesai" />
    {:else}
      <StatusBadge tone="mute" icon="⏸" label="Belum dimulai" />
    {/if}
  </div>

  {#if inc.status === 'finished'}
    <p role="status" class="mt-3 rounded-xl border border-good/60 bg-good/10 p-3 font-medium text-good">
      <Ikon nama="ok" /> Inkubasi selesai. Telur sudah memasuki waktu menetas.
    </p>
  {/if}

  {#if inc.status !== 'idle'}
    <!-- Batang kemajuan: bagian hijau = sudah dilalui, latar kuning = masa lockdown -->
    <div class="mt-4">
      <div class="flex items-baseline justify-between">
        <span class="text-lg font-semibold">Hari ke {hari} dari {total}</span>
        <span class="text-base text-ink2">{persen}%</span>
      </div>
      <div class="relative mt-1 h-6 overflow-hidden rounded-full" role="progressbar" aria-label="Kemajuan inkubasi"
        aria-valuemin="0" aria-valuemax={total} aria-valuenow={hari}
        style="background: linear-gradient(to right, rgb(255 255 255 / 0.08) {batasLockdown - 5}%, rgb(251 191 36 / 0.2) {batasLockdown + 5}%)">
        <div class="isi-bar absolute inset-0" style="background: {gradienIsi}; clip-path: inset(0 {100 - tampilPersen}% 0 0 round 999px); filter: drop-shadow(0 0 8px rgb(52 211 153 / 0.35))"></div>
      </div>
      <div class="mt-1 flex text-sm text-ink2">
        <span style="width: {batasLockdown}%">Pembalikan (hari 1–{total - LOCKDOWN_HARI})</span>
        <span class="flex-1 text-right">Lockdown ({total - LOCKDOWN_HARI + 1}–{total})</span>
      </div>
    </div>
  {/if}

  {#if inc.status === 'running'}
    <dl class="mt-4 grid grid-cols-1 gap-y-2 text-sm sm:grid-cols-3">
      <div><dt class="text-mute">Hari mulai</dt><dd class="font-medium">{tanggalText(inc.started_at)}</dd></div>
      <div><dt class="text-mute">Sisa waktu</dt><dd class="font-medium">{durasiText(sisa)}</dd></div>
      <div><dt class="text-mute">Tahapan</dt><dd class="font-medium">{tahap[inc.stage] ?? 'Sedang diproses'}</dd></div>
    </dl>
  {:else if inc.status === 'idle'}
    <p class="mt-3 text-sm text-mute">Inkubasi belum dimulai. Tekan Mulai setelah telur dimasukkan.</p>
  {/if}

  <div class="mt-4">
    {#if inc.status === 'running' && konfirmasi}
      <div class="rounded-xl border border-bad/60 bg-bad/10 p-3 text-sm text-bad">
        <p class="font-medium"><Ikon nama="bahaya" /> Yakin menghentikan inkubasi?</p>
        <p class="mt-1">Pemanasan dan pembalikan telur akan berhenti. Telur bisa rusak jika terlalu lama tanpa pemanas.</p>
        <div class="mt-3 grid grid-cols-2 gap-2 sm:flex">
          <button type="button" onclick={berhenti} class="tombol tombol-bahaya">Ya, hentikan</button>
          <button type="button" onclick={() => (konfirmasi = false)} class="tombol tombol-sekunder">Batal</button>
        </div>
      </div>
    {:else if inc.status === 'running'}
      <button type="button" onclick={() => (konfirmasi = true)} disabled={terkunci || sibuk} class="tombol tombol-garis-bahaya w-full sm:w-auto">
        {sibuk ? 'Mengirim…' : 'Berhenti'}
      </button>
    {:else}
      <button type="button" onclick={() => onkirim('start')} disabled={terkunci || sibuk} class="tombol w-full sm:w-auto">
        {sibuk ? 'Mengirim…' : inc.status === 'finished' ? 'Mulai inkubasi baru' : 'Mulai'}
      </button>
    {/if}
    {#if terkunci}<p class="mt-2 text-sm text-mute">Tidak tersedia: perangkat tidak terhubung atau data kedaluwarsa.</p>{/if}
    {#if hasil}
      {#if hasil.ok && hasil.id}<div class="mt-2"><StatusPerintah id={hasil.id} /></div>
      {:else}<p role="status" class="mt-2 text-sm {hasil.ok ? 'text-good' : 'text-bad'}"><Ikon nama={hasil.ok ? 'ok' : 'bahaya'} /> {hasil.teks}</p>{/if}
    {/if}
  </div>
</section>
