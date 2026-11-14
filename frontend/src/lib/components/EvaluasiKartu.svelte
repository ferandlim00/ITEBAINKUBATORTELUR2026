<script>
  import Ikon from '$lib/components/Ikon.svelte';
  import StatusBadge from './StatusBadge.svelte';
  import { tanggalText } from '$lib/format.js';
  import { siapAnimasi } from '$lib/transisi.svelte.js';
  import { warnaDi } from '$lib/level.js';
  const titikSkor = [{ x: 0, c: '#ef4444' }, { x: 45, c: '#fbbf24' }, { x: 72, c: '#a3e635' }, { x: 100, c: '#4ade80' }];

  // Hasil evaluasi stabilitas (lihat src/lib/stabilitas.js)
  let { judul, unit, hasil, target, min, max, desimal = 2, kejadian = [] } = $props();
  const durasi = (ms) => (ms < 60000 ? 'kurang dari 1 menit' : ms < 3600000 ? `${Math.round(ms / 60000)} menit` : `${Math.floor(ms / 3600000)} jam ${Math.round((ms % 3600000) / 60000)} menit`);
  const terbaru = $derived([...kejadian].reverse().slice(0, 5));
  const f = (x) => Number(x).toFixed(desimal);
  let siap = $state(false);
  $effect(() => {
    if (!siapAnimasi()) return;
    const id = setTimeout(() => (siap = true), 300);
    return () => clearTimeout(id);
  });
</script>

<section class="kartu p-4">
  <div class="flex flex-wrap items-center justify-between gap-2">
    <h3 class="text-lg font-semibold">{judul}</h3>
    {#if hasil}<StatusBadge tone={hasil.label.tone} icon={hasil.label.icon} label={hasil.label.teks} />{/if}
  </div>

  {#if !hasil}
    <p class="mt-2 text-sm text-mute">Data belum cukup atau batas belum tersedia untuk dievaluasi.</p>
  {:else}
    <p class="mt-2"><span class="text-4xl font-bold">{hasil.skor}</span><span class="text-mute"> / 100 skor stabilitas</span></p>
    <div class="relative mt-2 h-3 rounded-full" style="background: linear-gradient(to right, #ef4444 0%, #fbbf24 45%, #a3e635 72%, #4ade80 100%)" role="progressbar"
      aria-label="Skor stabilitas" aria-valuemin="0" aria-valuemax="100" aria-valuenow={hasil.skor}>
      <span class="absolute top-1/2 h-[1.3rem] w-[1.3rem] -translate-x-1/2 -translate-y-1/2 rounded-full bg-ink transition-[left] duration-1000 ease-out" style="left: {siap ? hasil.skor : 0}%; box-shadow: 0 0 0 4px {warnaDi(siap ? hasil.skor : 0, titikSkor)}, 0 0 10px {warnaDi(siap ? hasil.skor : 0, titikSkor)}"></span>
    </div>
    <p class="mt-2 text-sm">{Math.round(hasil.dalam)}% waktu berada dalam batas {min}–{max} {unit}.</p>

    <dl class="mt-3 grid grid-cols-2 gap-y-2 text-sm">
      <dt class="text-mute">Rata-rata</dt><dd class="text-right font-medium">{f(hasil.rata)} {unit}</dd>
      <dt class="text-mute">Target</dt><dd class="text-right font-medium">{target} {unit}</dd>
      <dt class="text-mute">Simpangan baku</dt><dd class="text-right font-medium">{f(hasil.simpangan)} {unit}</dd>
      <dt class="text-mute">Jarak rata-rata ke target (RMSE)</dt><dd class="text-right font-medium">{f(hasil.rmse)} {unit}</dd>
      <dt class="text-mute">Penyimpangan terbesar</dt><dd class="text-right font-medium">{f(hasil.devMaks)} {unit}</dd>
      <dt class="text-mute">Jumlah data</dt><dd class="text-right font-medium">{hasil.n} titik</dd>
    </dl>

    <div class="mt-4 border-t border-line pt-3">
      <h4 class="font-semibold">Catatan di luar batas</h4>
      {#if kejadian.length === 0}
        <p class="mt-1 text-sm text-good"><Ikon nama="ok" /> Tidak ada kejadian di luar batas pada rentang ini.</p>
      {:else}
        <p class="mt-1 text-sm text-ink2">{kejadian.length} kejadian tercatat{kejadian.length > 5 ? ', 5 terbaru:' : ':'}</p>
        <ul class="mt-1 space-y-1 text-sm">
          {#each terbaru as k}
            <li class="rounded-lg bg-warn/10 px-3 py-2">
              <Ikon nama={k.arah === 'tinggi' ? 'naik' : 'turun'} /> {tanggalText(k.mulai)}, {durasi(k.durasiMs)}, {k.arah === 'tinggi' ? 'tertinggi' : 'terendah'} {f(k.puncak)} {unit}
            </li>
          {/each}
        </ul>
      {/if}
    </div>

    <details class="mt-3 text-sm">
      <summary class="cursor-pointer font-medium underline">Cara menghitung skor</summary>
      <ol class="mt-2 list-decimal space-y-1 pl-5 text-ink2">
        <li><strong>Waktu dalam batas</strong> = berapa persen data yang berada di antara batas bawah dan batas atas.</li>
        <li><strong>Simpangan baku</strong> = seberapa besar angka "bergoyang" di sekitar rata-ratanya. Makin kecil makin stabil.</li>
        <li><strong>RMSE</strong> = rata-rata jarak angka ke target (jarak yang besar dihitung lebih berat).</li>
        <li><strong>Skor</strong> = 70% waktu dalam batas + 30% kedekatan ke target. Kedekatan ke target bernilai 100 kalau RMSE nol, dan 0 kalau RMSE sebesar setengah lebar batas.</li>
        <li>Penilaian: 90 ke atas <em>Sangat stabil</em>, 75–89 <em>Stabil</em>, 60–74 <em>Cukup stabil</em>, di bawah 60 <em>Perlu perhatian</em>.</li>
      </ol>
    </details>
  {/if}
</section>
