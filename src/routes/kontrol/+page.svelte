<script>
  import { getTelemetry, sendCommand } from '$lib/api.js';
  import { ageSeconds } from '$lib/format.js';
  import StatusBadge from '$lib/components/StatusBadge.svelte';
  import { statusPutar } from '$lib/turning.js';

  // Batas SEMENTARA. Nilai asli menunggu keputusan tim hardware
  const LAMPU_MAKS_MANUAL = 80; // % intensitas lampu tertinggi yang boleh diatur manual
  const VENTILASI_MAKS = 90; // derajat, 0 = tutup
  const BATAS_KEDALUWARSA_DETIK = 60;

  let data = $state(null);
  let error = $state('');
  let now = $state(Date.now());

  // Pilihan yang sedang diatur di layar (draft). Belum dikirim sampai tombol Terapkan ditekan
  let lampuMode = $state('auto');
  let lampuNilai = $state(50);
  let ventMode = $state('auto');
  let ventNilai = $state(30);
  let draftSiap = false;

  let sibuk = $state({}); // { lamp: true, ventilation: true, turning: true }
  let hasil = $state({}); // { lamp: { ok: true, teks: '...' } }

  async function muat() {
    try {
      data = await getTelemetry();
      error = '';
      if (!draftSiap) {
        // Isi draft sekali dari kondisi alat saat ini
        lampuMode = data.lamp.mode; lampuNilai = Math.min(data.lamp.intensity, LAMPU_MAKS_MANUAL);
        ventMode = data.ventilation.mode; ventNilai = data.ventilation.angle;
        draftSiap = true;
      }
    } catch (e) {
      error = e.message;
    }
  }

  $effect(() => {
    muat();
    const ambil = setInterval(muat, 5000);
    const detak = setInterval(() => (now = Date.now()), 1000);
    return () => { clearInterval(ambil); clearInterval(detak); };
  });

  let kedaluwarsa = $derived(!!data && (data.connection === 'offline' || ageSeconds(data.updated_at, now) > BATAS_KEDALUWARSA_DETIK));
  let terkunci = $derived(!data || kedaluwarsa);

  async function kirim(kunci, perintah) {
    sibuk[kunci] = true;
    hasil[kunci] = null;
    try {
      await sendCommand(perintah);
      hasil[kunci] = { ok: true, teks: 'Perintah terkirim ke perangkat.' };
      await muat();
    } catch (e) {
      hasil[kunci] = { ok: false, teks: `Gagal mengirim: ${e.message}` };
    } finally {
      sibuk[kunci] = false;
    }
  }

  function terapkanLampu() {
    const p = { target: 'lamp', mode: lampuMode };
    if (lampuMode === 'manual') p.intensity = Math.min(Math.max(Number(lampuNilai), 0), LAMPU_MAKS_MANUAL);
    kirim('lamp', p);
  }

  function terapkanVentilasi() {
    const p = { target: 'ventilation', mode: ventMode };
    if (ventMode === 'manual') p.angle = Math.min(Math.max(Number(ventNilai), 0), VENTILASI_MAKS);
    kirim('ventilation', p);
  }

  let inc = $derived(data?.incubation ?? { status: 'idle', stage: null });
  let turning = $derived(data?.turning ?? { status: 'idle' });
  let motor = $derived(data?.motor ?? { running: false, encoder_ok: true });
  let sp = $derived(statusPutar[turning.status] ?? statusPutar.idle);

  // Alasan tombol "Balik sekarang" tidak bisa dipakai (kosong = boleh)
  let alasanBalik = $derived.by(() => {
    if (terkunci) return 'Perangkat tidak terhubung';
    if (inc.status !== 'running') return 'Inkubasi belum berjalan';
    if (inc.stage === 'lockdown' || turning.status === 'lockdown') return 'Sedang lockdown, pembalikan dihentikan';
    if (turning.status === 'fault') return 'Motor sedang gangguan';
    if (turning.status === 'turning' || motor.running) return 'Motor sedang berputar';
    return '';
  });
</script>

<h1 class="text-2xl font-semibold">Kontrol</h1>

{#if error}
  <p class="mt-4 rounded border border-red-700 bg-red-100 p-3 text-red-900">⚠ Gagal memuat data: {error}. Coba lagi dalam beberapa detik.</p>
{:else if !data}
  <p class="mt-4 text-slate-600">Memuat data…</p>
{:else}
  {#if terkunci}
    <p class="mt-4 rounded border border-amber-700 bg-amber-100 p-3 text-amber-900">
      ⏱ Kontrol dinonaktifkan karena data kedaluwarsa atau perangkat terputus. Perintah tidak dikirim ke perangkat yang tidak merespons.
    </p>
  {/if}

  <div class="mt-4 grid gap-4 md:grid-cols-2">
    <!-- LAMPU -->
    <section class="rounded-lg border border-slate-300 bg-white p-4">
      <h2 class="text-lg font-semibold">Lampu pemanas</h2>
      <p class="mt-1 text-sm text-slate-600">Saat ini: {data.lamp.mode === 'auto' ? 'otomatis' : 'manual'}, intensitas {data.lamp.intensity}%</p>

      <fieldset class="mt-3" disabled={terkunci}>
        <legend class="text-sm font-medium">Mode</legend>
        <div class="mt-1 flex gap-4">
          <label class="flex items-center gap-2"><input type="radio" value="auto" bind:group={lampuMode} /> Otomatis</label>
          <label class="flex items-center gap-2"><input type="radio" value="manual" bind:group={lampuMode} /> Manual</label>
        </div>

        {#if lampuMode === 'manual'}
          <label class="mt-3 block text-sm font-medium" for="lampu-nilai">Intensitas: {lampuNilai}% (maks. {LAMPU_MAKS_MANUAL}%)</label>
          <input id="lampu-nilai" type="range" min="0" max={LAMPU_MAKS_MANUAL} bind:value={lampuNilai} class="w-full" />
          <p class="mt-1 text-sm text-amber-900">⚠ Mode manual tidak menjaga suhu otomatis. Pantau suhu di Dashboard.</p>
        {:else}
          <p class="mt-3 text-sm text-slate-600">Intensitas diatur otomatis oleh alat berdasarkan suhu.</p>
        {/if}

        <button type="button" onclick={terapkanLampu} disabled={terkunci || sibuk.lamp}
          class="mt-4 rounded bg-slate-900 px-4 py-2 text-white disabled:cursor-not-allowed disabled:opacity-40">
          {sibuk.lamp ? 'Mengirim…' : 'Terapkan'}
        </button>
      </fieldset>
      {#if hasil.lamp}<p class="mt-2 text-sm {hasil.lamp.ok ? 'text-emerald-900' : 'text-red-900'}">{hasil.lamp.ok ? '✓' : '⚠'} {hasil.lamp.teks}</p>{/if}
    </section>

    <!-- VENTILASI -->
    <section class="rounded-lg border border-slate-300 bg-white p-4">
      <h2 class="text-lg font-semibold">Ventilasi</h2>
      <p class="mt-1 text-sm text-slate-600">Saat ini: {data.ventilation.mode === 'auto' ? 'otomatis' : 'manual'}, bukaan {data.ventilation.angle}°</p>

      <fieldset class="mt-3" disabled={terkunci}>
        <legend class="text-sm font-medium">Mode</legend>
        <div class="mt-1 flex gap-4">
          <label class="flex items-center gap-2"><input type="radio" value="auto" bind:group={ventMode} /> Otomatis</label>
          <label class="flex items-center gap-2"><input type="radio" value="manual" bind:group={ventMode} /> Manual</label>
        </div>

        {#if ventMode === 'manual'}
          <label class="mt-3 block text-sm font-medium" for="vent-nilai">Bukaan: {ventNilai}° (0° tutup, {VENTILASI_MAKS}° buka penuh)</label>
          <input id="vent-nilai" type="range" min="0" max={VENTILASI_MAKS} bind:value={ventNilai} class="w-full" />
        {:else}
          <p class="mt-3 text-sm text-slate-600">Bukaan diatur otomatis oleh logika fuzzy di perangkat.</p>
        {/if}

        <button type="button" onclick={terapkanVentilasi} disabled={terkunci || sibuk.ventilation}
          class="mt-4 rounded bg-slate-900 px-4 py-2 text-white disabled:cursor-not-allowed disabled:opacity-40">
          {sibuk.ventilation ? 'Mengirim…' : 'Terapkan'}
        </button>
      </fieldset>
      {#if hasil.ventilation}<p class="mt-2 text-sm {hasil.ventilation.ok ? 'text-emerald-900' : 'text-red-900'}">{hasil.ventilation.ok ? '✓' : '⚠'} {hasil.ventilation.teks}</p>{/if}
    </section>
  </div>

  <!-- PEMBALIKAN TELUR -->
  <h2 class="mt-6 text-lg font-semibold">Pembalikan telur</h2>
  <section class="mt-2 rounded-lg border border-slate-300 bg-white p-4 md:max-w-md">
    <div class="flex items-center justify-between gap-2">
      <p class="text-sm text-slate-600">Semua telur dibalik bersamaan.</p>
      <StatusBadge tone={sp.tone} icon={sp.icon} label={sp.label} />
    </div>
    <button type="button" disabled={!!alasanBalik || sibuk.turning}
      onclick={() => kirim('turning', { target: 'turning', action: 'turn_now' })}
      class="mt-3 w-full rounded bg-slate-900 px-4 py-2 text-white disabled:cursor-not-allowed disabled:opacity-40">
      {sibuk.turning ? 'Mengirim…' : 'Balik sekarang'}
    </button>
    {#if alasanBalik}<p class="mt-2 text-sm text-slate-600">Tidak tersedia: {alasanBalik}.</p>{/if}
    {#if hasil.turning}
      <p role="status" class="mt-2 text-sm {hasil.turning.ok ? 'text-emerald-900' : 'text-red-900'}">{hasil.turning.ok ? '✓' : '⚠'} {hasil.turning.teks}</p>
    {/if}
  </section>
{/if}
