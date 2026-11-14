<script>
  import Ikon from '$lib/components/Ikon.svelte';
  import { sendCommand, getLimits } from '$lib/api.js';
  import { live, isStale, refresh } from '$lib/live.svelte.js';
  import StatusBadge from '$lib/components/StatusBadge.svelte';
  import StatusPerintah from '$lib/components/StatusPerintah.svelte';
  import { statusPutar } from '$lib/turning.js';

  // Batas diambil dari backend (GET /settings/limits). Nilai cadangan ini sama dengan kontrak v2.1
  let limits = $state(null);
  let lampuMaks = $derived(limits?.lamp_manual_intensity?.max ?? 80);
  let ventMaks = $derived(limits?.ventilation_angle?.max ?? 90);

  // Pilihan yang sedang diatur di layar (draft). Belum dikirim sampai tombol Terapkan ditekan
  let lampuMode = $state('auto');
  let lampuNilai = $state(50);
  let ventMode = $state('auto');
  let ventNilai = $state(30);
  let draftSiap = false;

  let sibuk = $state({});
  let hasil = $state({});

  $effect(() => { getLimits().then((l) => (limits = l)).catch(() => {}); });

  $effect(() => {
    const d = live.data;
    if (d && !draftSiap) {
      draftSiap = true; // isi draft sekali dari kondisi alat saat ini
      lampuMode = d.lamp.mode; lampuNilai = Math.min(d.lamp.intensity, 80);
      ventMode = d.ventilation.mode; ventNilai = d.ventilation.angle;
    }
  });

  let data = $derived(live.data);
  let kedaluwarsa = $derived(isStale());
  let terkunci = $derived(!data || kedaluwarsa);

  async function kirim(kunci, perintah) {
    sibuk[kunci] = true;
    hasil[kunci] = null;
    try {
      const r = await sendCommand(perintah);
      hasil[kunci] = { ok: true, id: r.id };
      await refresh();
    } catch (e) {
      hasil[kunci] = { ok: false, teks: `Gagal mengirim: ${e.message}` };
    } finally {
      sibuk[kunci] = false;
    }
  }

  function terapkanLampu() {
    const p = { target: 'lamp', mode: lampuMode };
    if (lampuMode === 'manual') p.intensity = Math.min(Math.max(Number(lampuNilai), 0), lampuMaks);
    kirim('lamp', p);
  }

  function terapkanVentilasi() {
    const p = { target: 'ventilation', mode: ventMode };
    if (ventMode === 'manual') p.angle = Math.min(Math.max(Number(ventNilai), 0), ventMaks);
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

{#if !data}
  <p class="mt-4 text-mute">{#if live.error}<Ikon nama="bahaya" /> Gagal memuat data: {live.error}{:else}Memuat data…{/if}</p>
{:else}
  {#if terkunci}
    <p class="mt-4 rounded border border-warn/60 bg-warn/10 p-3 text-warn">
      <Ikon nama="jam" /> Kontrol dinonaktifkan karena data kedaluwarsa atau perangkat terputus. Perintah tidak dikirim ke perangkat yang tidak merespons.
    </p>
  {/if}

  <div class="mt-4 grid gap-4 md:grid-cols-2">
    <!-- LAMPU -->
    <section class="kartu p-4">
      <h2 class="text-lg font-semibold">Lampu pemanas</h2>
      <p class="mt-1 text-sm text-mute">
        Saat ini: {data.lamp.mode === 'auto' ? (data.lamp.fuzzy_active ? 'otomatis (fuzzy)' : 'otomatis') : 'manual'}, intensitas {data.lamp.intensity}%
      </p>

      <fieldset class="mt-3" disabled={terkunci}>
        <legend class="text-sm font-medium">Mode</legend>
        <div class="mt-1 flex gap-4">
          <label class="flex items-center gap-2"><input type="radio" value="auto" bind:group={lampuMode} /> Otomatis</label>
          <label class="flex items-center gap-2"><input type="radio" value="manual" bind:group={lampuMode} /> Manual</label>
        </div>

        {#if lampuMode === 'manual'}
          <label class="mt-3 block text-sm font-medium" for="lampu-nilai">Intensitas: {lampuNilai}% (maks. {lampuMaks}%)</label>
          <input id="lampu-nilai" type="range" min="0" max={lampuMaks} bind:value={lampuNilai} class="w-full" />
          <p class="mt-1 text-sm text-warn"><Ikon nama="bahaya" /> Mode manual tidak menjaga suhu otomatis. Pantau suhu di Dashboard.</p>
        {:else}
          <p class="mt-3 text-sm text-mute">Intensitas diatur otomatis oleh logika fuzzy di perangkat.</p>
        {/if}

        <button type="button" onclick={terapkanLampu} disabled={terkunci || sibuk.lamp}
          class="mt-4 tombol">
          {sibuk.lamp ? 'Mengirim…' : 'Terapkan'}
        </button>
      </fieldset>
      {#if hasil.lamp}
        {#if hasil.lamp.ok}<div class="mt-2"><StatusPerintah id={hasil.lamp.id} /></div>
        {:else}<p role="alert" class="mt-2 text-sm text-bad"><Ikon nama="bahaya" /> {hasil.lamp.teks}</p>{/if}
      {/if}
    </section>

    <!-- VENTILASI -->
    <section class="kartu p-4">
      <h2 class="text-lg font-semibold">Ventilasi</h2>
      <p class="mt-1 text-sm text-mute">
        Saat ini: {data.ventilation.mode === 'auto' ? (data.ventilation.fuzzy_active ? 'otomatis (fuzzy)' : 'otomatis') : 'manual'}, bukaan {data.ventilation.angle}°
      </p>

      <fieldset class="mt-3" disabled={terkunci}>
        <legend class="text-sm font-medium">Mode</legend>
        <div class="mt-1 flex gap-4">
          <label class="flex items-center gap-2"><input type="radio" value="auto" bind:group={ventMode} /> Otomatis</label>
          <label class="flex items-center gap-2"><input type="radio" value="manual" bind:group={ventMode} /> Manual</label>
        </div>

        {#if ventMode === 'manual'}
          <label class="mt-3 block text-sm font-medium" for="vent-nilai">Bukaan: {ventNilai}° (0° tutup, {ventMaks}° buka penuh)</label>
          <input id="vent-nilai" type="range" min="0" max={ventMaks} bind:value={ventNilai} class="w-full" />
        {:else}
          <p class="mt-3 text-sm text-mute">Bukaan diatur otomatis oleh logika fuzzy di perangkat.</p>
        {/if}

        <button type="button" onclick={terapkanVentilasi} disabled={terkunci || sibuk.ventilation}
          class="mt-4 tombol">
          {sibuk.ventilation ? 'Mengirim…' : 'Terapkan'}
        </button>
      </fieldset>
      {#if hasil.ventilation}
        {#if hasil.ventilation.ok}<div class="mt-2"><StatusPerintah id={hasil.ventilation.id} /></div>
        {:else}<p role="alert" class="mt-2 text-sm text-bad"><Ikon nama="bahaya" /> {hasil.ventilation.teks}</p>{/if}
      {/if}
    </section>
  </div>

  <!-- PEMBALIKAN TELUR -->
  <h2 class="mt-6 text-lg font-semibold">Pembalikan telur</h2>
  <section class="mt-2 kartu p-4 md:max-w-md">
    <div class="flex items-center justify-between gap-2">
      <p class="text-sm text-mute">Semua telur dibalik bersamaan.</p>
      <StatusBadge tone={sp.tone} icon={sp.icon} label={sp.label} />
    </div>
    <button type="button" disabled={!!alasanBalik || sibuk.turning}
      onclick={() => kirim('turning', { target: 'turning', action: 'turn_now' })}
      class="mt-3 w-full tombol">
      {sibuk.turning ? 'Mengirim…' : 'Balik sekarang'}
    </button>
    {#if alasanBalik}<p class="mt-2 text-sm text-mute">Tidak tersedia: {alasanBalik}.</p>{/if}
    {#if hasil.turning}
      {#if hasil.turning.ok}<div class="mt-2"><StatusPerintah id={hasil.turning.id} /></div>
      {:else}<p role="alert" class="mt-2 text-sm text-bad"><Ikon nama="bahaya" /> {hasil.turning.teks}</p>{/if}
    {/if}
  </section>
{/if}
