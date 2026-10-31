<script>
  import { getAlerts } from '$lib/api.js';
  import { tanggalText } from '$lib/format.js';
  import StatusBadge from '$lib/components/StatusBadge.svelte';

  const jenis = {
    over_temperature: 'Suhu terlalu tinggi',
    low_temperature: 'Suhu terlalu rendah',
    motor_fault: 'Motor gangguan',
    sensor_error: 'Sensor error',
    communication: 'Komunikasi terputus'
  };
  const tingkat = {
    high: { label: 'Tinggi', icon: '⚠', tone: 'bad' },
    medium: { label: 'Sedang', icon: '!', tone: 'warn' },
    low: { label: 'Rendah', icon: 'i', tone: 'mute' }
  };

  let daftar = $state(null);
  let error = $state('');
  let filterStatus = $state('semua');

  $effect(() => {
    getAlerts().then((d) => (daftar = d)).catch((e) => (error = e.message));
  });

  let tampil = $derived(
    (daftar ?? [])
      .filter((a) => filterStatus === 'semua' || a.status === filterStatus)
      .sort((a, b) => new Date(b.timestamp) - new Date(a.timestamp))
  );
  let jumlahAktif = $derived((daftar ?? []).filter((a) => a.status === 'active').length);
</script>

<h1 class="text-2xl font-semibold">Peringatan</h1>

{#if error}
  <p class="mt-4 rounded border border-red-700 bg-red-100 p-3 text-red-900">⚠ Gagal memuat peringatan: {error}.</p>
{:else if !daftar}
  <p class="mt-4 text-slate-600">Memuat peringatan…</p>
{:else}
  <div class="mt-4 flex flex-wrap items-center gap-3">
    {#if jumlahAktif > 0}
      <StatusBadge tone="bad" icon="⚠" label="{jumlahAktif} peringatan aktif" />
    {:else}
      <StatusBadge tone="ok" icon="✓" label="Tidak ada peringatan aktif" />
    {/if}
    <label class="ml-auto text-sm">Tampilkan:
      <select bind:value={filterStatus} class="ml-2 rounded border border-slate-400 bg-white px-2 py-1">
        <option value="semua">Semua</option>
        <option value="active">Aktif</option>
        <option value="resolved">Selesai</option>
      </select>
    </label>
  </div>

  {#if tampil.length === 0}
    <p class="mt-6 rounded-lg border border-slate-300 bg-white p-4 text-slate-600">Tidak ada peringatan untuk filter ini.</p>
  {:else}
    <ul class="mt-4 space-y-3">
      {#each tampil as a (a.id)}
        {@const t = tingkat[a.severity] ?? tingkat.low}
        <li class="rounded-lg border border-slate-300 bg-white p-4 {a.status === 'resolved' ? 'opacity-70' : ''}">
          <div class="flex flex-wrap items-center gap-2">
            <StatusBadge tone={t.tone} icon={t.icon} label="Tingkat {t.label}" />
            {#if a.status === 'active'}
              <StatusBadge tone="bad" icon="●" label="Aktif" />
            {:else}
              <StatusBadge tone="ok" icon="✓" label="Selesai" />
            {/if}
            <span class="ml-auto text-sm text-slate-600">{tanggalText(a.timestamp)}</span>
          </div>
          <p class="mt-2 font-medium">{jenis[a.type] ?? a.type}</p>
          <p class="text-sm text-slate-700">{a.message}</p>
        </li>
      {/each}
    </ul>
  {/if}
{/if}
