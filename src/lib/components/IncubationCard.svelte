<script>
  import StatusBadge from './StatusBadge.svelte';
  import { durasiText, hariKe, tanggalText } from '$lib/format.js';
  import StatusPerintah from './StatusPerintah.svelte';

  // inc: { status: idle|running|finished, started_at, finish_at, stage }
  let { inc, now, terkunci = false, sibuk = false, hasil = null, onkirim } = $props();

  const tahap = { turning: 'Masa pembalikan telur', lockdown: 'Lockdown, persiapan menetas' };

  let konfirmasi = $state(false);
  let sisa = $derived(inc.finish_at ? Math.round((new Date(inc.finish_at).getTime() - now) / 1000) : 0);
  let total = $derived(inc.total_days ?? 21);
  let hari = $derived(inc.current_day ?? (inc.started_at ? Math.min(hariKe(inc.started_at, now), total) : 0));

  function berhenti() {
    konfirmasi = false;
    onkirim('stop');
  }
</script>

<section class="rounded-lg border border-slate-300 bg-white p-4">
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
    <p role="status" class="mt-3 rounded border border-emerald-700 bg-emerald-50 p-3 font-medium text-emerald-900">
      ✓ Inkubasi selesai. Telur sudah memasuki waktu menetas.
    </p>
  {/if}

  {#if inc.status === 'running'}
    <dl class="mt-3 grid grid-cols-2 gap-y-2 text-sm md:grid-cols-4">
      <div><dt class="text-slate-600">Hari mulai</dt><dd class="font-medium">{tanggalText(inc.started_at)}</dd></div>
      <div><dt class="text-slate-600">Hari ke</dt><dd class="font-medium">{hari} dari {total}</dd></div>
      <div><dt class="text-slate-600">Sisa waktu</dt><dd class="font-medium">{durasiText(sisa)}</dd></div>
      <div><dt class="text-slate-600">Tahapan</dt><dd class="font-medium">{tahap[inc.stage] ?? 'Sedang diproses'}</dd></div>
    </dl>
  {:else if inc.status === 'idle'}
    <p class="mt-3 text-sm text-slate-600">Inkubasi belum dimulai. Tekan Mulai setelah telur dimasukkan.</p>
  {/if}

  <div class="mt-4">
    {#if inc.status === 'running' && konfirmasi}
      <div class="rounded border border-red-700 bg-red-50 p-3 text-sm text-red-900">
        <p class="font-medium">⚠ Yakin menghentikan inkubasi?</p>
        <p class="mt-1">Pemanasan dan pembalikan telur akan berhenti. Telur bisa rusak jika terlalu lama tanpa pemanas.</p>
        <div class="mt-3 flex gap-2">
          <button type="button" onclick={berhenti} class="rounded bg-red-800 px-4 py-2 text-white">Ya, hentikan</button>
          <button type="button" onclick={() => (konfirmasi = false)} class="rounded border border-slate-400 bg-white px-4 py-2 text-slate-900">Batal</button>
        </div>
      </div>
    {:else if inc.status === 'running'}
      <button type="button" onclick={() => (konfirmasi = true)} disabled={terkunci || sibuk}
        class="rounded border border-red-800 px-4 py-2 text-red-900 disabled:cursor-not-allowed disabled:opacity-40">
        {sibuk ? 'Mengirim…' : 'Berhenti'}
      </button>
    {:else}
      <button type="button" onclick={() => onkirim('start')} disabled={terkunci || sibuk}
        class="rounded bg-slate-900 px-4 py-2 text-white disabled:cursor-not-allowed disabled:opacity-40">
        {sibuk ? 'Mengirim…' : inc.status === 'finished' ? 'Mulai inkubasi baru' : 'Mulai'}
      </button>
    {/if}
    {#if terkunci}<p class="mt-2 text-sm text-slate-600">Tidak tersedia: perangkat tidak terhubung atau data kedaluwarsa.</p>{/if}
    {#if hasil}
      {#if hasil.ok && hasil.id}<div class="mt-2"><StatusPerintah id={hasil.id} /></div>
      {:else}<p role="status" class="mt-2 text-sm {hasil.ok ? 'text-emerald-900' : 'text-red-900'}">{hasil.ok ? '✓' : '⚠'} {hasil.teks}</p>{/if}
    {/if}
  </div>
</section>
