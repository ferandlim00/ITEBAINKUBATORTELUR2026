<script>
  import Ikon from '$lib/components/Ikon.svelte';
  import { untrack } from 'svelte';

  // Kartu pengaturan satu angka (target suhu, target kelembapan, interval pembalikan)
  let { judul, label, unit, step = 1, bulat = false, saatIni, batas, terkunci = false, bantuan = '', onsimpan } = $props();

  let nilai = $state(untrack(() => saatIni));
  let sibuk = $state(false);
  let hasil = $state(null);

  async function simpan() {
    hasil = null;
    const v = Number(nilai);
    if (nilai === '' || nilai === null || !Number.isFinite(v)) { hasil = { ok: false, teks: `Isi ${label.toLowerCase()} dengan angka.` }; return; }
    if (bulat && !Number.isInteger(v)) { hasil = { ok: false, teks: `${label} harus berupa bilangan bulat.` }; return; }
    if (v < batas.min || v > batas.max) { hasil = { ok: false, teks: `${label} harus antara ${batas.min} dan ${batas.max} ${unit}.` }; return; }
    sibuk = true;
    try {
      await onsimpan(v);
      hasil = { ok: true, teks: `${judul} ${v} ${unit} terkirim ke perangkat.` };
    } catch (e) {
      hasil = { ok: false, teks: `Gagal mengirim: ${e.message}` };
    } finally {
      sibuk = false;
    }
  }
</script>

<section class="kartu p-4">
  <h2 class="text-lg font-semibold">{judul}</h2>
  <p class="mt-1 text-sm text-mute">Saat ini: <strong>{saatIni} {unit}</strong>{bantuan ? `. ${bantuan}` : ''}</p>
  <fieldset class="mt-3" disabled={terkunci}>
    <label for={'f-' + judul} class="block text-sm font-medium">{label} ({unit})</label>
    <input id={'f-' + judul} type="number" {step} bind:value={nilai} class="input mt-1" />
    <p class="mt-1 text-sm text-mute">Batas yang diizinkan: {batas.min}–{batas.max} {unit}</p>
    <button type="button" onclick={simpan} disabled={terkunci || sibuk} class="tombol mt-4">
      {sibuk ? 'Mengirim…' : 'Simpan'}
    </button>
  </fieldset>
  {#if hasil}<p role="status" class="mt-2 text-sm {hasil.ok ? 'text-good' : 'text-bad'}"><Ikon nama={hasil.ok ? 'ok' : 'bahaya'} /> {hasil.teks}</p>{/if}
</section>
