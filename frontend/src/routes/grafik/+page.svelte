<script>
  import Ikon from '$lib/components/Ikon.svelte';
  import { getHistory } from '$lib/api.js';
  import { live, ambangLembap } from '$lib/live.svelte.js';
  import { hitungStabilitas, cariKejadian } from '$lib/stabilitas.js';
  import LineChart from '$lib/components/LineChart.svelte';
  import EvaluasiKartu from '$lib/components/EvaluasiKartu.svelte';

  const rentang = [
    { kode: '1j', teks: '1 jam', jam: 1 },
    { kode: '6j', teks: '6 jam', jam: 6 },
    { kode: '24j', teks: '24 jam', jam: 24 },
    { kode: '7h', teks: '7 hari', jam: 168 }
  ];

  let pilih = $state('6j');
  let titik = $state(null);
  let error = $state('');
  let memuat = $state(false);

  let jam = $derived(rentang.find((r) => r.kode === pilih).jam);
  let data = $derived(live.data);
  let batas = $derived(live.thresholds);
  let batasLembap = $derived(ambangLembap(data, batas));

  async function muat() {
    memuat = true;
    try {
      const to = new Date();
      const from = new Date(to.getTime() - jam * 3600 * 1000);
      titik = await getHistory({ from: from.toISOString(), to: to.toISOString(), maxPoints: 144 });
      error = '';
    } catch (e) {
      error = e.message;
    } finally {
      memuat = false;
    }
  }

  // Muat ulang saat rentang berubah, lalu tiap 30 detik
  $effect(() => {
    muat();
    const id = setInterval(muat, 30000);
    return () => clearInterval(id);
  });

  let sSuhu = $derived(titik && data ? hitungStabilitas(titik.map((p) => p.temperature), { target: data.target_temperature, min: batas.temp_min, max: batas.temp_max }) : null);
  let sLembap = $derived(titik && data ? hitungStabilitas(titik.map((p) => p.humidity), { target: data.target_humidity, min: batasLembap.min, max: batasLembap.max }) : null);
  let kSuhu = $derived(titik && data ? cariKejadian(titik.map((p) => ({ t: p.t, v: p.temperature })), { target: data.target_temperature, min: batas.temp_min, max: batas.temp_max }) : []);
  let kLembap = $derived(titik && data ? cariKejadian(titik.map((p) => ({ t: p.t, v: p.humidity })), { target: data.target_humidity, min: batasLembap.min, max: batasLembap.max }) : []);
  const span = $derived(jam * 3600 * 1000);
</script>

<h1 class="text-xl font-bold sm:text-2xl">Grafik dan evaluasi</h1>
<p class="mt-1 text-ink2">Rekap suhu, kelembapan, dan lampu dari waktu ke waktu, beserta penilaian seberapa stabil inkubatornya.</p>

<div class="mt-4 grid grid-cols-4 gap-2 sm:flex sm:flex-wrap sm:items-center" role="group" aria-label="Pilih rentang waktu">
  {#each rentang as r}
    <button type="button" onclick={() => (pilih = r.kode)} aria-pressed={pilih === r.kode}
      class="tombol tombol-kecil w-full px-2 sm:w-auto sm:px-4 {pilih === r.kode ? '' : 'tombol-sekunder'}">{r.teks}</button>
  {/each}
  {#if memuat}<span class="col-span-4 text-sm text-mute sm:col-span-1">Memuat…</span>{/if}
</div>

{#if error}
  <p class="mt-4 rounded-xl border border-bad/60 bg-bad/10 p-3 text-bad"><Ikon nama="bahaya" /> Gagal memuat riwayat: {error}.</p>
{/if}

{#if !titik}
  {#if !error}<p class="mt-4 text-mute">Memuat grafik…</p>{/if}
{:else if data}
  <section class="kartu mt-4 p-4">
    <h2 class="text-lg font-semibold">Suhu (°C)</h2>
    <div class="mt-2">
      <LineChart points={titik.map((p) => ({ t: p.t, v: p.temperature }))} label="Suhu" unit="°C" target={data.target_temperature} bandMin={batas.temp_min} bandMax={batas.temp_max} desimal={2} spanMs={span} />
    </div>
  </section>

  <section class="kartu mt-4 p-4">
    <h2 class="text-lg font-semibold">Kelembapan (%)</h2>
    <div class="mt-2">
      <LineChart points={titik.map((p) => ({ t: p.t, v: p.humidity }))} label="Kelembapan" unit="%" target={data.target_humidity ?? null} bandMin={batasLembap.min ?? null} bandMax={batasLembap.max ?? null} desimal={1} spanMs={span} />
    </div>
  </section>

  <section class="kartu mt-4 p-4">
    <h2 class="text-lg font-semibold">Intensitas lampu (%)</h2>
    <div class="mt-2">
      <LineChart points={titik.map((p) => ({ t: p.t, v: p.lamp_intensity }))} label="Lampu" unit="%" skala="panas" desimal={0} spanMs={span} />
    </div>
  </section>

  <h2 class="mt-8 text-xl font-bold">Evaluasi stabilitas</h2>
  <p class="mt-1 text-ink2">Dihitung dari data pada rentang waktu yang dipilih di atas. Skor 0–100: makin tinggi makin stabil.</p>
  <div class="mt-3 grid gap-4 md:grid-cols-2">
    <EvaluasiKartu judul="Suhu" unit="°C" kejadian={kSuhu} hasil={sSuhu} target={data.target_temperature} min={batas.temp_min} max={batas.temp_max} />
    <EvaluasiKartu judul="Kelembapan" unit="%" kejadian={kLembap} hasil={sLembap} target={data.target_humidity} min={batasLembap.min} max={batasLembap.max} desimal={1} />
  </div>
{/if}
