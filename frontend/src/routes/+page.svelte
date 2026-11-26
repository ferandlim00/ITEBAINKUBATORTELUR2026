<script>
  import Ikon from '$lib/components/Ikon.svelte';
  import { sendCommand, getHistory, mockScenario } from '$lib/api.js';
  import { live, isStale, umurDetik, refresh, ambangLembap } from '$lib/live.svelte.js';
  import { ageText, ageSeconds, sinyalText, durasiText } from '$lib/format.js';
  import { statusPutar } from '$lib/turning.js';
  import StatusBadge from '$lib/components/StatusBadge.svelte';
  import MetricCard from '$lib/components/MetricCard.svelte';
  import Gauge from '$lib/components/Gauge.svelte';
  import LineChart from '$lib/components/LineChart.svelte';
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
  let riwayat = $state([]);

  let data = $derived(live.data);
  let batas = $derived(live.thresholds);
  let batasLembap = $derived(ambangLembap(data, batas));
  let kedaluwarsa = $derived(isStale());
  let suhuDalamBatas = $derived(!!data && data.temperature >= batas.temp_min && data.temperature <= batas.temp_max);
  let punyaBatasLembap = $derived(batasLembap.min != null && batasLembap.max != null);
  let lembapDalamBatas = $derived(!!data && punyaBatasLembap && data.humidity >= batasLembap.min && data.humidity <= batasLembap.max);
  let wifi = $derived(data?.wifi ?? { connected: false, ssid: '', rssi: null });
  let inc = $derived(data?.incubation ?? { status: 'idle', started_at: null, finish_at: null, stage: null });
  let turning = $derived(data?.turning ?? { status: 'idle', last_turned_at: null, next_turning_at: null });
  let motor = $derived(data?.motor ?? { running: false, encoder_ok: true });
  let dc = $derived(data?.device_condition ? (kondisi[data.device_condition.system_health] ?? kondisi.normal) : null);

  // Peringatan aktif berasal dari backend (GET /alerts)
  let peringatan = $derived(live.alerts.filter((a) => a.status === 'active').map((a) => a.message));
  let aktuatorBermasalah = $derived(!!data && turning.status === 'fault');

  // Grafik suhu 6 jam terakhir, diperbarui tiap menit
  async function muatRiwayat() {
    try {
      const to = new Date();
      const from = new Date(to.getTime() - 6 * 3600 * 1000);
      riwayat = await getHistory({ from: from.toISOString(), to: to.toISOString(), maxPoints: 90 });
    } catch { /* grafik kosong, bagian lain tetap jalan */ }
  }
  $effect(() => {
    muatRiwayat();
    const id = setInterval(muatRiwayat, 60000);
    return () => clearInterval(id);
  });

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

  function modeNote(x, autoLabel = 'Otomatis') {
    if (x.mode !== 'auto') return 'Manual';
    return x.fuzzy_active ? 'Otomatis (Fuzzy)' : autoLabel;
  }
</script>

<h1 class="text-xl font-bold sm:text-2xl">Dashboard</h1>

{#if live.error}
  <p class="mt-4 rounded-xl border border-bad/60 bg-bad/10 p-3 text-bad">
    <Ikon nama="bahaya" /> Gagal memuat data terbaru: {live.error}. Tampilan di bawah bisa sudah lama.
  </p>
{/if}

{#if !data}
  {#if !live.error}<p class="mt-4 text-mute">Memuat data…</p>{/if}
{:else}
  <!-- Pertanyaan utama (DES 01): data baru? suhu dan kelembapan dalam batas? peringatan? aktuator? Wi-Fi? -->
  <div class="kartu mt-3 p-3.5 sm:mt-4 sm:p-4">
    <div class="strip-gulir flex flex-nowrap items-center gap-2 overflow-x-auto pb-1 md:flex-wrap md:gap-2.5 md:overflow-visible md:pb-0">
      {#if kedaluwarsa}
        <span class="shrink-0"><StatusBadge tone="warn" icon="⏱" label="Data kedaluwarsa" /></span>
        <span class="shrink-0"><StatusBadge tone="mute" icon="?" label="Suhu belum pasti" /></span>
      {:else}
        <span class="shrink-0"><StatusBadge tone="ok" icon="✓" label="Data terbaru" /></span>
        <span class="shrink-0">
          {#if suhuDalamBatas}
            <StatusBadge tone="ok" icon="✓" label="Suhu dalam batas" />
          {:else}
            <StatusBadge tone="bad" icon="⚠" label="Suhu di luar batas" />
          {/if}
        </span>
        {#if punyaBatasLembap}
          <span class="shrink-0">
            {#if lembapDalamBatas}
              <StatusBadge tone="ok" icon="✓" label="Kelembapan dalam batas" />
            {:else}
              <StatusBadge tone="bad" icon="⚠" label="Kelembapan di luar batas" />
            {/if}
          </span>
        {/if}
      {/if}

      <span class="shrink-0">
        {#if peringatan.length > 0}
          <StatusBadge tone="bad" icon="⚠" label="{peringatan.length} peringatan aktif" />
        {:else}
          <StatusBadge tone="ok" icon="✓" label="Tidak ada peringatan" />
        {/if}
      </span>

      <span class="shrink-0">
        {#if aktuatorBermasalah}
          <StatusBadge tone="bad" icon="⚠" label="Ada aktuator bermasalah" />
        {:else}
          <StatusBadge tone="ok" icon="✓" label="Aktuator sesuai" />
        {/if}
      </span>

      <span class="shrink-0">
        {#if wifi.connected}
          <StatusBadge tone="ok" icon="✓" label="Wi-Fi terhubung" />
        {:else}
          <StatusBadge tone="bad" icon="⚠" label="Wi-Fi terputus" />
        {/if}
      </span>

      <span class="hidden text-sm text-mute md:ml-auto md:inline">Pembaruan terakhir: {ageText(umurDetik())}</span>
    </div>
    <p class="mt-2 border-t border-line/60 pt-2 text-xs text-mute md:hidden">Pembaruan terakhir: {ageText(umurDetik())}</p>
  </div>

  {#if peringatan.length > 0}
    <ul class="mt-3 space-y-1 rounded-xl border border-bad/60 bg-bad/10 p-3 text-sm text-bad">
      {#each peringatan as teks}<li><Ikon nama="bahaya" /> {teks}</li>{/each}
    </ul>
  {/if}

  {#if live.izin === 'default' && live.notifAktif}
    <p class="mt-3 text-sm text-mute"><Ikon nama="bel" /> Notifikasi peringatan belum diizinkan browser. <a href="/pengaturan" class="underline">Atur di Pengaturan</a>.</p>
  {/if}

  <div class="mt-4">
    <IncubationCard {inc} now={live.now} terkunci={kedaluwarsa} sibuk={kirimSibuk} hasil={kirimHasil} onkirim={kirimInkubasi} />
  </div>

  {#if dc}
    <section class="kartu mt-4 p-4 {kedaluwarsa ? 'opacity-60' : ''}">
      <div class="flex flex-wrap items-center justify-between gap-2">
        <h2 class="text-lg font-semibold">Kondisi alat</h2>
        <StatusBadge tone={dc.tone} icon={dc.icon} label={dc.label} />
      </div>
      <p class="mt-2 text-sm">{data.device_condition.summary}</p>
      {#if data.device_condition.uptime_seconds > 0}
        <p class="mt-1 text-sm text-mute">Menyala terus selama {durasiText(data.device_condition.uptime_seconds)}</p>
      {/if}
    </section>
  {/if}

  <h2 class="mt-6 text-lg font-semibold">Kondisi inkubator</h2>
  <div class="mt-2 grid grid-cols-2 gap-3 md:grid-cols-3">
    <Gauge title="Suhu" ikon="suhu" value={data.temperature} unit="°C" min={30} max={45} target={data.target_temperature}
      okMin={batas.temp_min} okMax={batas.temp_max} desimal={1} old={kedaluwarsa} />
    <Gauge title="Kelembapan" ikon="lembap" value={data.humidity} unit="%" min={0} max={100} target={data.target_humidity ?? null}
      okMin={punyaBatasLembap ? batasLembap.min : null} okMax={punyaBatasLembap ? batasLembap.max : null} desimal={0} old={kedaluwarsa} />
    <MetricCard title="Lampu" ikon="lampu" value={data.lamp.intensity} unit="%" persen={data.lamp.intensity} skala="panas" old={kedaluwarsa}
      baris={[{ label: 'Mode', nilai: modeNote(data.lamp) }, { label: 'Skala', nilai: '0% mati, 100% terang' }]} />
    <MetricCard title="Ventilasi" ikon="angin" value={data.ventilation.angle} unit="°" persen={(data.ventilation.angle / 90) * 100} skala="angin" old={kedaluwarsa}
      baris={[{ label: 'Mode', nilai: modeNote(data.ventilation, 'Otomatis (4 Mode)') }, { label: 'Skala', nilai: '0° tutup, 90° buka' }]} />
    <MetricCard title="Status putar" ikon="putar" status={{ label: statusPutar[turning.status]?.label ?? '—', icon: statusPutar[turning.status]?.icon ?? 'info', tone: statusPutar[turning.status]?.tone ?? 'mute' }} old={kedaluwarsa}
      baris={[{ label: 'Motor pembalik', nilai: motor.running ? 'Berputar' : 'Berhenti' }, { label: 'Terakhir membalik', nilai: turning.last_turned_at ? ageText(ageSeconds(turning.last_turned_at, live.now)) : '—' }]} />
    <MetricCard title="Wi-Fi" ikon="wifi" status={wifi.connected ? { label: 'Terhubung', icon: 'wifi', tone: 'ok' } : { label: 'Terputus', icon: 'wifi', tone: 'bad' }} old={kedaluwarsa}
      baris={[{ label: 'Nama jaringan', nilai: wifi.connected ? wifi.ssid : '—' }, { label: 'Kekuatan sinyal', nilai: wifi.connected ? sinyalText(wifi.rssi).replace(/^./, (c) => c.toUpperCase()) : '—' }]} />
  </div>

  <section class="kartu mt-4 p-4">
    <div class="flex flex-wrap items-center justify-between gap-2">
      <h2 class="text-lg font-semibold">Grafik suhu (6 jam terakhir)</h2>
      <a href="/grafik" class="text-sm font-medium underline">Lihat semua grafik dan evaluasi stabilitas <Ikon nama="kanan" /></a>
    </div>
    <div class="mt-3">
      <LineChart points={riwayat.map((p) => ({ t: p.t, v: p.temperature }))} label="Suhu" unit="°C"
        target={data.target_temperature} bandMin={batas.temp_min} bandMax={batas.temp_max} desimal={2} spanMs={6 * 3600 * 1000} />
    </div>
  </section>

  <h2 class="mt-6 text-lg font-semibold">Pembalikan telur</h2>
  <div class="mt-2 md:max-w-md">
    <TurningCard {turning} {motor} now={live.now} old={kedaluwarsa} />
  </div>
{/if}

{#if import.meta.env.DEV && import.meta.env.VITE_USE_MOCK !== 'false'}
  <div class="mt-8 rounded-xl border border-dashed border-line p-3 text-sm text-ink2">
    Mode pengembangan, skenario data dummy:
    <select bind:value={skenario} onchange={gantiSkenario} class="input-kecil ml-2">
      <option value="normal">Normal</option>
      <option value="stale">Data kedaluwarsa</option>
      <option value="alert">Peringatan</option>
      <option value="finished">Inkubasi selesai</option>
    </select>
  </div>
{/if}
