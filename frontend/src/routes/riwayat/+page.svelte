<script>
  import Ikon from '$lib/components/Ikon.svelte';
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
  const PER_HALAMAN = 15;

  let logs = $state(null);
  let error = $state('');
  let memuat = $state(false);
  let filterJenis = $state('semua');
  let filterJam = $state(24 * 7);
  let halaman = $state(1);

  // Filter dan halaman diproses di server (?from=&event=&page=&limit=)
  async function muat() {
    memuat = true; error = '';
    try {
      logs = await getAuditLogs({
        from: filterJam === 0 ? undefined : new Date(Date.now() - filterJam * 3600 * 1000).toISOString(),
        event: filterJenis === 'semua' ? undefined : filterJenis,
        page: halaman,
        limit: PER_HALAMAN
      });
    } catch (e) {
      error = e.message;
    } finally {
      memuat = false;
    }
  }

  $effect(() => { muat(); });
</script>

<h1 class="text-2xl font-semibold">Riwayat aktivitas</h1>

<div class="mt-4 grid grid-cols-1 gap-3 text-sm sm:flex sm:flex-wrap sm:items-center sm:gap-4">
  <label class="flex flex-col gap-1 sm:flex-row sm:items-center sm:gap-2">Rentang waktu:
    <select bind:value={filterJam} onchange={() => (halaman = 1)} class="input-kecil w-full sm:w-auto">
      {#each rentang as r}<option value={r.jam}>{r.teks}</option>{/each}
    </select>
  </label>
  <label class="flex flex-col gap-1 sm:flex-row sm:items-center sm:gap-2">Jenis kejadian:
    <select bind:value={filterJenis} onchange={() => (halaman = 1)} class="input-kecil w-full sm:w-auto">
      <option value="semua">Semua</option>
      {#each Object.entries(jenis) as [kode, teks]}<option value={kode}>{teks}</option>{/each}
    </select>
  </label>
  {#if memuat}<span class="text-mute sm:ml-auto">Memuat…</span>{/if}
</div>

{#if error}
  <p class="mt-4 rounded border border-bad/60 bg-bad/10 p-3 text-bad"><Ikon nama="bahaya" /> Gagal memuat riwayat: {error}.</p>
{:else if !logs}
  <p class="mt-4 text-mute">Memuat riwayat…</p>
{:else if logs.length === 0}
  <p class="mt-6 kartu p-4 text-mute">
    {halaman > 1 ? 'Tidak ada data lagi di halaman ini.' : 'Tidak ada kejadian untuk filter ini. Coba perluas rentang waktu.'}
  </p>
{:else}
  <!-- Tampilan kartu ringkas untuk ponsel (mudah dibaca tanpa geser ke samping) -->
  <ul class="mt-4 space-y-2.5 md:hidden">
    {#each logs as l (l.id)}
      <li class="kartu p-3.5 text-sm">
        <div class="flex items-center justify-between gap-2">
          <span class="font-semibold text-ink">
            {#if l.event === 'access_denied'}<Ikon nama="bahaya" class="text-warn" /> {/if}{jenis[l.event] ?? l.event}
          </span>
          <span class="rounded-md bg-white/5 px-2 py-0.5 text-xs text-ink2">{l.user}</span>
        </div>
        <p class="mt-1.5 text-ink2">{l.detail}</p>
        <p class="mt-1.5 text-xs text-mute">{tanggalText(l.timestamp)}</p>
      </li>
    {/each}
  </ul>

  <!-- Tampilan tabel untuk layar lebar -->
  <div class="mt-4 hidden overflow-x-auto kartu md:block">
    <table class="w-full min-w-[32rem] text-left text-sm">
      <thead class="border-b border-line bg-white/5">
        <tr><th class="p-3">Waktu</th><th class="p-3">Pengguna</th><th class="p-3">Kejadian</th><th class="p-3">Keterangan</th></tr>
      </thead>
      <tbody>
        {#each logs as l (l.id)}
          <tr class="border-b border-line last:border-0">
            <td class="whitespace-nowrap p-3">{tanggalText(l.timestamp)}</td>
            <td class="p-3">{l.user}</td>
            <td class="p-3">{#if l.event === 'access_denied'}<Ikon nama="bahaya" class="text-warn" /> {/if}{jenis[l.event] ?? l.event}</td>
            <td class="p-3">{l.detail}</td>
          </tr>
        {/each}
      </tbody>
    </table>
  </div>
{/if}

<div class="mt-4 grid grid-cols-[1fr_auto_1fr] items-center gap-2 text-sm sm:flex sm:gap-3">
  <button type="button" onclick={() => (halaman -= 1)} disabled={halaman === 1 || memuat}
    class="tombol tombol-sekunder tombol-kecil w-full sm:w-auto"><Ikon nama="kiri" /> Sebelumnya</button>
  <span class="px-2 text-center text-ink2">Hal. {halaman}</span>
  <button type="button" onclick={() => (halaman += 1)} disabled={!logs || logs.length < PER_HALAMAN || memuat}
    class="tombol tombol-sekunder tombol-kecil w-full sm:w-auto">Berikutnya <Ikon nama="kanan" /></button>
</div>
