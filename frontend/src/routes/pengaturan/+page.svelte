<script>
  import Ikon from '$lib/components/Ikon.svelte';
  import {
    getSettings, getLimits, saveTargetTemperature, saveTargetHumidity, saveMotorSettings, saveWifi
  } from '$lib/api.js';
  import { live, isStale, aktifkanNotifikasi, kirimNotifikasiUji, setNotifAktif } from '$lib/live.svelte.js';
  import TargetForm from '$lib/components/TargetForm.svelte';

  let settings = $state(null);
  let limits = $state(null);
  let error = $state('');

  // Wi-Fi
  let ssid = $state('');
  let sandi = $state('');
  let lihatSandi = $state(false);
  let konfirmasiWifi = $state(false);
  let wifiSibuk = $state(false);
  let wifiHasil = $state(null);

  $effect(() => {
    Promise.all([getSettings(), getLimits()]).then(([s, l]) => {
      settings = s; limits = l; ssid = s.wifi.ssid;
    }).catch((e) => (error = e.message));
  });

  let data = $derived(live.data);
  let kedaluwarsa = $derived(isStale());
  let terkunci = $derived(!data || kedaluwarsa);

  async function simpanSuhu(v) { await saveTargetTemperature(v); settings.target_temperature = v; }
  async function simpanLembap(v) { await saveTargetHumidity(v); settings.target_humidity = v; }
  async function simpanMotor(v) { await saveMotorSettings({ interval_minutes: v }); settings.motor.interval_minutes = v; }

  function periksaWifi() {
    wifiHasil = null;
    const nama = ssid.trim();
    if (nama.length < 1 || nama.length > 32) { wifiHasil = { ok: false, teks: 'Nama Wi-Fi (SSID) harus 1–32 karakter.' }; return; }
    if (sandi.length < 8 || sandi.length > 63) { wifiHasil = { ok: false, teks: 'Kata sandi Wi-Fi harus 8–63 karakter.' }; return; }
    konfirmasiWifi = true;
  }

  async function kirimWifi() {
    konfirmasiWifi = false;
    wifiSibuk = true; wifiHasil = null;
    const nama = ssid.trim();
    try {
      await saveWifi({ ssid: nama, password: sandi });
      settings.wifi.ssid = nama;
      wifiHasil = { ok: true, teks: 'Terkirim ke perangkat. Perangkat mencoba tersambung ke jaringan baru maksimal 30 detik. Jika gagal, otomatis kembali ke Wi-Fi lama dan muncul peringatan komunikasi.' };
    } catch (e) {
      wifiHasil = { ok: false, teks: `Gagal mengirim: ${e.message}` };
    } finally {
      wifiSibuk = false;
      sandi = ''; // kata sandi tidak disimpan di layar
      lihatSandi = false;
    }
  }
</script>

<h1 class="text-xl font-bold sm:text-2xl">Pengaturan</h1>

{#if error}
  <p class="mt-4 rounded-xl border border-bad/60 bg-bad/10 p-3 text-bad"><Ikon nama="bahaya" /> Gagal memuat data: {error}. Muat ulang halaman.</p>
{:else if !data || !settings || !limits}
  <p class="mt-4 text-mute">Memuat pengaturan…</p>
{:else}
  {#if terkunci}
    <p class="mt-4 rounded-xl border border-warn/60 bg-warn/10 p-3 text-warn">
      <Ikon nama="jam" /> Pengaturan alat dinonaktifkan karena data kedaluwarsa atau perangkat terputus. Perubahan tidak dikirim ke perangkat yang tidak merespons.
    </p>
  {/if}

  <div class="mt-4 grid gap-4 md:grid-cols-2">
    <TargetForm judul="Target suhu" label="Target suhu" unit="°C" step={0.1}
      saatIni={settings.target_temperature} batas={limits.target_temperature} {terkunci} onsimpan={simpanSuhu} />
    {#if settings.target_humidity != null && limits.target_humidity}
      <TargetForm judul="Target kelembapan" label="Target kelembapan" unit="%" step={1}
        saatIni={settings.target_humidity} batas={limits.target_humidity} {terkunci} onsimpan={simpanLembap} />
    {/if}
    <TargetForm judul="Interval pembalikan telur" label="Interval pembalikan" unit="menit" step={1} bulat
      saatIni={settings.motor.interval_minutes} batas={limits.motor.interval_minutes} {terkunci}
      bantuan="Jarak waktu antar pembalikan" onsimpan={simpanMotor} />
  </div>

  <!-- WI-FI -->
  <section class="kartu mt-4 p-4 md:max-w-xl">
    <h2 class="text-lg font-semibold">Wi-Fi perangkat</h2>
    <p class="mt-1 text-sm text-mute">
      Jaringan saat ini: {data.wifi?.connected ? data.wifi.ssid : 'terputus'}. Kata sandi tidak pernah ditampilkan kembali setelah dikirim.
    </p>
    <fieldset class="mt-3 space-y-3" disabled={terkunci || wifiSibuk}>
      <div>
        <label for="ssid" class="block text-sm font-medium">Nama Wi-Fi (SSID)</label>
        <input id="ssid" bind:value={ssid} autocomplete="off" class="input mt-1" />
      </div>
      <div>
        <label for="sandi" class="block text-sm font-medium">Kata sandi Wi-Fi</label>
        <div class="mt-1 flex gap-2">
          <input id="sandi" type={lihatSandi ? 'text' : 'password'} bind:value={sandi} autocomplete="new-password" placeholder="Isi untuk mengganti" class="input" />
          <button type="button" onclick={() => (lihatSandi = !lihatSandi)} class="tombol tombol-sekunder tombol-kecil whitespace-nowrap">
            {lihatSandi ? 'Sembunyikan' : 'Tampilkan'}
          </button>
        </div>
      </div>

      {#if konfirmasiWifi}
        <div class="rounded-xl border border-bad/60 bg-bad/10 p-3 text-sm text-bad">
          <p class="font-medium"><Ikon nama="bahaya" /> Kirim pengaturan Wi-Fi ke perangkat?</p>
          <p class="mt-1">Perangkat akan mencoba tersambung ke <strong>{ssid.trim()}</strong> maksimal 30 detik, dan data bisa terputus sebentar. Jika gagal, perangkat otomatis kembali ke Wi-Fi lama dan muncul peringatan komunikasi.</p>
          <div class="mt-3 flex gap-2">
            <button type="button" onclick={kirimWifi} class="tombol tombol-bahaya">Ya, kirim</button>
            <button type="button" onclick={() => (konfirmasiWifi = false)} class="tombol tombol-sekunder">Batal</button>
          </div>
        </div>
      {:else}
        <button type="button" onclick={periksaWifi} disabled={terkunci || wifiSibuk} class="tombol">
          {wifiSibuk ? 'Mengirim…' : 'Kirim ke perangkat'}
        </button>
      {/if}
    </fieldset>
    {#if wifiHasil}<p role="status" class="mt-2 text-sm {wifiHasil.ok ? 'text-good' : 'text-bad'}"><Ikon nama={wifiHasil.ok ? 'ok' : 'bahaya'} /> {wifiHasil.teks}</p>{/if}
  </section>

  <!-- NOTIFIKASI (saklar nyala/mati) -->
  <section class="kartu mt-4 p-4 md:max-w-xl">
    <h2 class="text-lg font-semibold">Notifikasi peringatan</h2>
    <p class="mt-1 text-sm text-mute">Notifikasi browser muncul saat ada peringatan baru dan saat inkubasi selesai, selama halaman ini terbuka atau diminimalkan.</p>

    <div class="mt-4 flex items-center gap-4">
      <button type="button" role="switch" aria-checked={live.notifAktif} aria-label="Notifikasi peringatan" class="saklar"
        onclick={() => setNotifAktif(!live.notifAktif)}></button>
      <span class="text-lg font-semibold">{live.notifAktif ? 'Aktif' : 'Nonaktif'}</span>
    </div>

    {#if !live.notifAktif}
      <p class="mt-3 text-sm text-ink2">Notifikasi dimatikan. Peringatan tetap tampil di halaman Dashboard dan Peringatan.</p>
    {:else if live.izin === 'granted'}
      <p class="mt-3 text-sm text-good"><Ikon nama="ok" /> Notifikasi akan muncul di browser ini.</p>
      <button type="button" onclick={kirimNotifikasiUji} class="tombol tombol-sekunder mt-2">Kirim notifikasi uji</button>
    {:else if live.izin === 'denied'}
      <p class="mt-3 text-sm text-bad"><Ikon nama="bahaya" /> Notifikasi diblokir browser. Klik ikon gembok di sebelah alamat web, lalu izinkan Notifikasi.</p>
    {:else if live.izin === 'unsupported'}
      <p class="mt-3 text-sm text-ink2">Browser ini tidak mendukung notifikasi.</p>
    {:else}
      <p class="mt-3 text-sm text-ink2">Browser belum memberi izin.</p>
      <button type="button" onclick={aktifkanNotifikasi} class="tombol mt-2">Izinkan di browser</button>
    {/if}
    <p class="mt-3 text-sm text-mute">Pilihan ini tersimpan di browser ini saja. Peringatan penting juga dikirim lewat Telegram oleh server (opsional).</p>
  </section>
{/if}
