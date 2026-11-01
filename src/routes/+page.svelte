<script>
  import { sendCommand, mockScenario } from '$lib/api.js';
  import { live, isStale, umurDetik, refresh } from '$lib/live.svelte.js';
  import { ageText, sinyalText, durasiText } from '$lib/format.js';
  import { statusPutar } from '$lib/turning.js';
  import StatusBadge from '$lib/components/StatusBadge.svelte';
  import MetricCard from '$lib/components/MetricCard.svelte';
  import TurningCard from '$lib/components/TurningCard.svelte';
  import IncubationCard from '$lib/components/IncubationCard.svelte';

  const kondisi = {
    normal: { label: 'Normal', icon: '✓', tone: 'ok' },
    warning: { label: 'Perlu perhatian', icon: '!', tone: 'warn' },
    fault: { label: 'Gangguan', icon: '⚠', tone: 'bad' }
  };

  let skenario = $state('normal');
  let kirimSibuk = $state(false);
  let kirimHasil = $state(null);

  let data = $derived(live.data);
  let batas = $derived(live.thresholds);
  let kedaluwarsa = $derived(isStale());
  let suhuDalamBatas = $derived(!!data && data.temperature >= batas.temp_min && data.temperature <= batas.temp_max);
  let wifi = $derived(data?.wifi ?? { connected: false, ssid: '', rssi: null });
  let inc = $derived(data?.incubation ?? { status: 'idle', started_at: null, finish_at: null, stage: null });
  let turning = $derived(data?.turning ?? { status: 'idle', last_turned_at: null, next_turning_at: null });
  let motor = $derived(data?.motor ?? { running: false, encoder_ok: true });
  let dc = $derived(data?.device_condition ? (kondisi[data.device_condition.system_health] ?? kondisi.normal) : null);

  // Peringatan aktif berasal dari backend (GET /alerts)
  let peringatan = $derived(live.alerts.filter((a) => a.status === 'active').map((a) => a.message));
  let aktuatorBermasalah = $derived(!!data && turning.status === 'fault');

  function gantiSkenario() {
    mockScenario.value = skenario;
    refresh();
  }

  async function kirimInkubasi(action) {
    kirimSibuk = true; kirimHasil = null;
    try {
      const r = await sendCommand({ target: 'incubation', action });
      kirimHasil = { ok: true, id: r.id };
      await refresh();
    } catch (e) {
      kirimHasil = { ok: false, teks: `Gagal mengirim: ${e.message}` };
    } finally {
      kirimSibuk = false;
    }
  }

  function modeNote(x) {
    if (x.mode !== 'auto') return 'Manual';
    return x.fuzzy_active ? '⚙ Otomatis (Fuzzy)' : 'Otomatis';
  }
</script>

<h1 class="text-2xl font-semibold">Dashboard</h1>

{#if live.error}
  <p class="mt-4 rounded border border-red-700 bg-red-100 p-3 text-red-900">
    ⚠ Gagal memuat data terbaru: {live.error}. Tampilan di bawah bisa sudah lama.
  </p>
{/if}

{#if !data}
  {#if !live.error}<p class="mt-4 text-slate-600">Memuat data…</p>{/if}
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

    <span class="ml-auto text-sm text-slate-600">Pembaruan terakhir: {ageText(umurDetik())}</span>
  </div>

  {#if peringatan.length > 0}
    <ul class="mt-3 space-y-1 rounded-lg border border-red-700 bg-red-50 p-3 text-sm text-red-900">
      {#each peringatan as teks}<li>⚠ {teks}</li>{/each}
    </ul>
  {/if}

  {#if live.izin === 'default'}
    <p class="mt-3 text-sm text-slate-600">🔔 Notifikasi peringatan belum aktif. <a href="/pengaturan" class="underline">Aktifkan di Pengaturan</a>.</p>
  {/if}

  <div class="mt-4">
    <IncubationCard {inc} now={live.now} terkunci={kedaluwarsa} sibuk={kirimSibuk} hasil={kirimHasil} onkirim={kirimInkubasi} />
  </div>

  {#if dc}
    <section class="mt-4 rounded-lg border border-slate-300 bg-white p-4 {kedaluwarsa ? 'opacity-60' : ''}">
      <div class="flex flex-wrap items-center justify-between gap-2">
        <h2 class="text-lg font-semibold">Kondisi alat</h2>
        <StatusBadge tone={dc.tone} icon={dc.icon} label={dc.label} />
      </div>
      <p class="mt-2 text-sm">{data.device_condition.summary}</p>
      {#if data.device_condition.uptime_seconds > 0}
        <p class="mt-1 text-sm text-slate-600">Menyala terus selama {durasiText(data.device_condition.uptime_seconds)}</p>
      {/if}
    </section>
  {/if}

  <h2 class="mt-6 text-lg font-semibold">Kondisi inkubator</h2>
  <div class="mt-2 grid grid-cols-2 gap-3 md:grid-cols-3">
    <MetricCard title="Suhu" value={data.temperature} unit="°C"
      note={kedaluwarsa ? 'Data lama' : data.target_temperature != null ? `Target ${data.target_temperature}°C` : ''} old={kedaluwarsa} />
    <MetricCard title="Kelembapan (pengaman)" value={data.humidity} unit="%" note={kedaluwarsa ? 'Data lama' : 'Indikator pengaman, tanpa target'} old={kedaluwarsa} />
    <MetricCard title="Lampu" value={data.lamp.intensity} unit="%" note={modeNote(data.lamp)} old={kedaluwarsa} />
    <MetricCard title="Ventilasi" value={data.ventilation.angle} unit="°" note={modeNote(data.ventilation)} old={kedaluwarsa} />
    <MetricCard title="Status putar" value={statusPutar[turning.status]?.label ?? '—'} note={motor.running ? 'Motor berputar' : 'Motor berhenti'} old={kedaluwarsa} />
    <MetricCard title="Wi-Fi" value={wifi.connected ? wifi.ssid : 'Terputus'} note={wifi.connected ? `Sinyal ${sinyalText(wifi.rssi)}` : 'Perangkat tidak terhubung'} old={kedaluwarsa} />
  </div>

  <h2 class="mt-6 text-lg font-semibold">Pembalikan telur</h2>
  <div class="mt-2 md:max-w-md">
    <TurningCard {turning} {motor} now={live.now} old={kedaluwarsa} />
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
