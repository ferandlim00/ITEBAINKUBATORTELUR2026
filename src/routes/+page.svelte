<script>
  import { getTelemetry, getThresholds, sendCommand, mockScenario } from '$lib/api.js';
  import { ageSeconds, ageText, sinyalText } from '$lib/format.js';
  import StatusBadge from '$lib/components/StatusBadge.svelte';
  import MetricCard from '$lib/components/MetricCard.svelte';
  import TurningCard from '$lib/components/TurningCard.svelte';
  import { statusPutar } from '$lib/turning.js';
  import IncubationCard from '$lib/components/IncubationCard.svelte';

  // Ambang diambil dari backend (ditetapkan pemilik data perangkat). Nilai awal ini hanya cadangan
  let SUHU_MIN = $state(36.5);
  let SUHU_MAX = $state(38.5);
  let BATAS_KEDALUWARSA_DETIK = $state(60);

  let data = $state(null);
  let error = $state('');
  let now = $state(Date.now());
  let skenario = $state('normal');
  let kirimSibuk = $state(false);
  let kirimHasil = $state(null);

  $effect(() => {
    getThresholds().then((t) => {
      SUHU_MIN = t.temp_min; SUHU_MAX = t.temp_max; BATAS_KEDALUWARSA_DETIK = t.stale_seconds;
    }).catch(() => {});
  });

  async function muat() {
    try {
      data = await getTelemetry();
      error = '';
    } catch (e) {
      error = e.message;
    }
  }

  // Ambil data tiap 5 detik, dan jam berdetak tiap 1 detik
  $effect(() => {
    muat();
    const ambil = setInterval(muat, 5000);
    const detak = setInterval(() => (now = Date.now()), 1000);
    return () => { clearInterval(ambil); clearInterval(detak); };
  });

  function gantiSkenario() {
    mockScenario.value = skenario;
    muat();
  }

  async function kirimInkubasi(action) {
    kirimSibuk = true; kirimHasil = null;
    try {
      await sendCommand({ target: 'incubation', action });
      kirimHasil = { ok: true, teks: action === 'start' ? 'Perintah mulai terkirim ke perangkat.' : 'Perintah berhenti terkirim ke perangkat.' };
      await muat();
    } catch (e) {
      kirimHasil = { ok: false, teks: `Gagal mengirim: ${e.message}` };
    } finally {
      kirimSibuk = false;
    }
  }

  let umur = $derived(data ? ageSeconds(data.updated_at, now) : 0);
  let kedaluwarsa = $derived(!!data && (data.connection === 'offline' || umur > BATAS_KEDALUWARSA_DETIK));
  let suhuDalamBatas = $derived(!!data && data.temperature >= SUHU_MIN && data.temperature <= SUHU_MAX);
  let wifi = $derived(data?.wifi ?? { connected: false, ssid: '', rssi: null });
  let inc = $derived(data?.incubation ?? { status: 'idle', started_at: null, finish_at: null, stage: null });
  let turning = $derived(data?.turning ?? { status: 'idle', last_turned_at: null, next_turning_at: null });
  let motor = $derived(data?.motor ?? { running: false, encoder_ok: true });

  // Daftar peringatan diturunkan dari data (sementara, sampai backend punya endpoint /alerts)
  let peringatan = $derived.by(() => {
    if (!data) return [];
    const p = [];
    if (data.temperature > SUHU_MAX) p.push(`Suhu ${data.temperature}°C di atas batas ${SUHU_MAX}°C`);
    if (data.temperature < SUHU_MIN) p.push(`Suhu ${data.temperature}°C di bawah batas ${SUHU_MIN}°C`);
    if (turning.status === 'fault') p.push('Motor pembalik bermasalah');
    return p;
  });
  let aktuatorBermasalah = $derived(!!data && turning.status === 'fault');
</script>

<h1 class="text-2xl font-semibold">Dashboard</h1>

{#if error}
  <p class="mt-4 rounded border border-red-700 bg-red-100 p-3 text-red-900">
    ⚠ Gagal memuat data: {error}. Coba lagi dalam beberapa detik.
  </p>
{:else if !data}
  <p class="mt-4 text-slate-600">Memuat data…</p>
{:else}
  <!-- Pertanyaan utama (DES 01): data baru? suhu dalam batas? peringatan? aktuator, putar, Wi-Fi, inkubasi sesuai? -->
  <div class="mt-4 flex flex-wrap items-center gap-3 rounded-lg border border-slate-300 bg-white p-4">
    {#if kedaluwarsa}
      <StatusBadge tone="warn" icon="⏱" label="Data kedaluwarsa" />
      <StatusBadge tone="mute" icon="?" label="Suhu belum pasti" />
    {:else}
      <StatusBadge tone="ok" icon="✓" label="Data terbaru" />
      {#if suhuDalamBatas}
        <StatusBadge tone="ok" icon="✓" label="Suhu dalam batas" />
      {:else}
        <StatusBadge tone="bad" icon="⚠" label="Suhu di luar batas" />
      {/if}
    {/if}

    {#if peringatan.length > 0}
      <StatusBadge tone="bad" icon="⚠" label="{peringatan.length} peringatan aktif" />
    {:else}
      <StatusBadge tone="ok" icon="✓" label="Tidak ada peringatan" />
    {/if}

    {#if aktuatorBermasalah}
      <StatusBadge tone="bad" icon="⚠" label="Ada aktuator bermasalah" />
    {:else}
      <StatusBadge tone="ok" icon="✓" label="Aktuator sesuai" />
    {/if}

    {#if wifi.connected}
      <StatusBadge tone="ok" icon="✓" label="Wi-Fi terhubung" />
    {:else}
      <StatusBadge tone="bad" icon="⚠" label="Wi-Fi terputus" />
    {/if}

    <span class="ml-auto text-sm text-slate-600">Pembaruan terakhir: {ageText(umur)}</span>
  </div>

  {#if peringatan.length > 0}
    <ul class="mt-3 space-y-1 rounded-lg border border-red-700 bg-red-50 p-3 text-sm text-red-900">
      {#each peringatan as teks}<li>⚠ {teks}</li>{/each}
    </ul>
  {/if}

  <div class="mt-4">
    <IncubationCard {inc} {now} terkunci={kedaluwarsa} sibuk={kirimSibuk} hasil={kirimHasil} onkirim={kirimInkubasi} />
  </div>

  <h2 class="mt-6 text-lg font-semibold">Kondisi inkubator</h2>
  <div class="mt-2 grid grid-cols-2 gap-3 md:grid-cols-3">
    <MetricCard title="Suhu" value={data.temperature} unit="°C"
      note={kedaluwarsa ? 'Data lama' : data.target_temperature != null ? `Target ${data.target_temperature}°C` : ''} old={kedaluwarsa} />
    <MetricCard title="Kelembapan (pengaman)" value={data.humidity} unit="%" note={kedaluwarsa ? 'Data lama' : 'Indikator pengaman, tanpa target'} old={kedaluwarsa} />
    <MetricCard title="Lampu" value={data.lamp.intensity} unit="%" note={data.lamp.mode === 'auto' ? 'Otomatis (fuzzy)' : 'Manual'} old={kedaluwarsa} />
    <MetricCard title="Ventilasi" value={data.ventilation.angle} unit="°" note={data.ventilation.mode === 'auto' ? 'Otomatis (fuzzy)' : 'Manual'} old={kedaluwarsa} />
    <MetricCard title="Status putar" value={statusPutar[turning.status]?.label ?? '—'} note={motor.running ? 'Motor berputar' : 'Motor berhenti'} old={kedaluwarsa} />
    <MetricCard title="Wi-Fi" value={wifi.connected ? wifi.ssid : 'Terputus'} note={wifi.connected ? `Sinyal ${sinyalText(wifi.rssi)}` : 'Perangkat tidak terhubung'} old={kedaluwarsa} />
  </div>

  <h2 class="mt-6 text-lg font-semibold">Pembalikan telur</h2>
  <div class="mt-2 md:max-w-md">
    <TurningCard {turning} {motor} {now} old={kedaluwarsa} />
  </div>
{/if}

{#if import.meta.env.DEV && import.meta.env.VITE_USE_MOCK !== 'false'}
  <div class="mt-8 rounded border border-dashed border-slate-500 p-3 text-sm text-slate-700">
    Mode pengembangan, skenario data dummy:
    <select bind:value={skenario} onchange={gantiSkenario} class="ml-2 rounded border border-slate-400 bg-white px-2 py-1">
      <option value="normal">Normal</option>
      <option value="stale">Data kedaluwarsa</option>
      <option value="alert">Peringatan</option>
      <option value="finished">Inkubasi selesai</option>
    </select>
  </div>
{/if}
