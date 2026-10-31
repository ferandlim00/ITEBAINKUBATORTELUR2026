<script>
  import { getAuditLogs } from '$lib/api.js';
  import { tanggalText } from '$lib/format.js';

  const jenis = {
    login: 'Login',
    command: 'Perintah ke perangkat',
    access_denied: 'Akses ditolak',
    config_change: 'Perubahan konfigurasi'
  };
  // Rentang waktu dalam jam. 0 = semua
  const rentang = [
    { jam: 1, teks: '1 jam terakhir' },
    { jam: 24, teks: '24 jam terakhir' },
    { jam: 24 * 7, teks: '7 hari terakhir' },
    { jam: 0, teks: 'Semua waktu' }
  ];

  let logs = $state(null);
  let error = $state('');
  let filterJenis = $state('semua');
  let filterJam = $state(24 * 7);

  $effect(() => {
    getAuditLogs().then((d) => (logs = d)).catch((e) => (error = e.message));
  });

  let tampil = $derived.by(() => {
    const batas = filterJam === 0 ? 0 : Date.now() - filterJam * 3600 * 1000;
    return (logs ?? [])
      .filter((l) => filterJenis === 'semua' || l.event === filterJenis)
      .filter((l) => new Date(l.timestamp).getTime() >= batas)
      .sort((a, b) => new Date(b.timestamp) - new Date(a.timestamp));
  });
</script>

<h1 class="text-2xl font-semibold">Riwayat aktivitas</h1>

{#if error}
  <p class="mt-4 rounded border border-red-700 bg-red-100 p-3 text-red-900">⚠ Gagal memuat riwayat: {error}.</p>
{:else if !logs}
  <p class="mt-4 text-slate-600">Memuat riwayat…</p>
{:else}
  <div class="mt-4 flex flex-wrap gap-4 text-sm">
    <label>Rentang waktu:
      <select bind:value={filterJam} class="ml-2 rounded border border-slate-400 bg-white px-2 py-1">
        {#each rentang as r}<option value={r.jam}>{r.teks}</option>{/each}
      </select>
    </label>
    <label>Jenis kejadian:
      <select bind:value={filterJenis} class="ml-2 rounded border border-slate-400 bg-white px-2 py-1">
        <option value="semua">Semua</option>
        {#each Object.entries(jenis) as [kode, teks]}<option value={kode}>{teks}</option>{/each}
      </select>
    </label>
    <span class="ml-auto text-slate-600">{tampil.length} kejadian</span>
  </div>

  {#if tampil.length === 0}
    <p class="mt-6 rounded-lg border border-slate-300 bg-white p-4 text-slate-600">Tidak ada kejadian untuk filter ini. Coba perluas rentang waktu.</p>
  {:else}
    <div class="mt-4 overflow-x-auto rounded-lg border border-slate-300 bg-white">
      <table class="w-full min-w-[32rem] text-left text-sm">
        <thead class="border-b border-slate-300 bg-slate-50">
          <tr><th class="p-3">Waktu</th><th class="p-3">Pengguna</th><th class="p-3">Kejadian</th><th class="p-3">Keterangan</th></tr>
        </thead>
        <tbody>
          {#each tampil as l (l.id)}
            <tr class="border-b border-slate-200 last:border-0">
              <td class="whitespace-nowrap p-3">{tanggalText(l.timestamp)}</td>
              <td class="p-3">{l.user}</td>
              <td class="p-3">{l.event === 'access_denied' ? '⚠ ' : ''}{jenis[l.event] ?? l.event}</td>
              <td class="p-3">{l.detail}</td>
            </tr>
          {/each}
        </tbody>
      </table>
    </div>
  {/if}
{/if}
