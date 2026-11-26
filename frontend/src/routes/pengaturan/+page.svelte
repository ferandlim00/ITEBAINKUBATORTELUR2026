<script>
  import Ikon from '$lib/components/Ikon.svelte';
  import {
    getSettings, getLimits, getNotificationSettings,
    saveTargetTemperature, saveTargetHumidity, saveMotorSettings, saveWifi,
    saveNotificationSettings, testTelegramNotification
  } from '$lib/api.js';
  import { live, isStale, aktifkanNotifikasi, kirimNotifikasiUji, setNotifAktif, setNotifJenis } from '$lib/live.svelte.js';
  import TargetForm from '$lib/components/TargetForm.svelte';

  const daftarJenisNotif = [
    { key: 'over_temperature', judul: 'Suhu terlalu tinggi (> 38,5°C)', ket: 'Peringatan darurat saat suhu melewati batas aman atas (jeda anti-spam 5 menit).' },
    { key: 'low_temperature', judul: 'Suhu terlalu dingin (< 36,5°C)', ket: 'Peringatan saat suhu inkubator turun di bawah batas aman bawah (jeda anti-spam 5 menit).' },
    { key: 'sensor_error', judul: 'Sensor SHT30 / SHT31 tidak terbaca', ket: 'Peringatan bila pembacaan sensor suhu & kelembapan gagal 3 kali berturut-turut.' },
    { key: 'humidity_warning', judul: 'Kelembapan di luar rentang aman', ket: 'Setter (Hari 1–18): 45%–70% | Lockdown (Hari 19–21): 60%–78%.' },
    { key: 'communication', judul: 'Alat offline / putus koneksi (> 45 detik)', ket: 'Peringatan saat ESP32 tidak melapor > 45 detik atau gagal ganti Wi-Fi (anti putus sesaat).' },
    { key: 'motor_fault', judul: 'Gangguan motor pembalik', ket: 'Peringatan bila terjadi gangguan pada sistem pembalik telur.' },
    { key: 'general', judul: 'Notifikasi umum, siklus, & kondisi pulih', ket: 'Info siklus dimulai, Lockdown (Hari 19), selesai (Hari 21), & kabar saat kondisi pulih normal.' }
  ];

  let settings = $state(null);
  let limits = $state(null);
  let notifCfg = $state(null);
  let notifSibuk = $state(false);
  let notifStatus = $state(null);
  let tgUjiSibuk = $state(false);
  let tgUjiHasil = $state(null);
  let error = $state('');

  // Wi-Fi
  let ssid = $state('');
  let sandi = $state('');
  let lihatSandi = $state(false);
  let konfirmasiWifi = $state(false);
  let wifiSibuk = $state(false);
  let wifiHasil = $state(null);

  $effect(() => {
    Promise.all([getSettings(), getLimits(), getNotificationSettings()]).then(([s, l, n]) => {
      settings = s; limits = l; ssid = s.wifi.ssid; notifCfg = n; setNotifJenis(n);
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

  async function ubahSaklarNotif(key) {
    if (!notifCfg || notifSibuk) return;
    const nilaiLama = notifCfg[key];
    const nilaiBaru = !nilaiLama;
    notifCfg[key] = nilaiBaru;
    notifSibuk = true;
    notifStatus = null;
    try {
      const updated = await saveNotificationSettings({ [key]: nilaiBaru });
      notifCfg = updated;
      setNotifJenis(updated);
      notifStatus = { ok: true, teks: 'Pengaturan notifikasi tersimpan.' };
    } catch (e) {
      notifCfg[key] = nilaiLama;
      notifStatus = { ok: false, teks: `Gagal menyimpan: ${e.message}` };
    } finally {
      notifSibuk = false;
    }
  }

  async function kirimUjiTelegram() {
    tgUjiSibuk = true;
    tgUjiHasil = null;
    try {
      const r = await testTelegramNotification();
      tgUjiHasil = { ok: true, teks: r.message ?? 'Pesan uji Telegram berhasil dikirim.' };
    } catch (e) {
      tgUjiHasil = { ok: false, teks: e.message };
    } finally {
      tgUjiSibuk = false;
    }
  }
</script>

<h1 class="text-xl font-bold sm:text-2xl">Pengaturan</h1>

{#if error}
  <p class="mt-4 rounded-xl border border-bad/60 bg-bad/10 p-3 text-bad"><Ikon nama="bahaya" /> Gagal memuat data: {error}. Muat ulang halaman.</p>
{:else if !data || !settings || !limits || !notifCfg}
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
        <button type="button" onclick={periksaWifi} disabled={terkunci || wifiSibuk} class="tombol w-full sm:w-auto">
          {wifiSibuk ? 'Mengirim…' : 'Kirim ke perangkat'}
        </button>
      {/if}
    </fieldset>
    {#if wifiHasil}<p role="status" class="mt-2 text-sm {wifiHasil.ok ? 'text-good' : 'text-bad'}"><Ikon nama={wifiHasil.ok ? 'ok' : 'bahaya'} /> {wifiHasil.teks}</p>{/if}
  </section>

  <!-- NOTIFIKASI SISTEM & TELEGRAM (Per Jenis + Master Switch, tetap aktif walau alat offline) -->
  <section class="kartu mt-4 p-4 md:max-w-2xl">
    <div class="flex flex-wrap items-center justify-between gap-2">
      <h2 class="text-lg font-semibold">Notifikasi Telegram & jenis peringatan</h2>
      {#if notifCfg.telegram_configured}
        <span class="rounded-full border border-good/50 bg-good/10 px-3 py-0.5 text-xs font-medium text-good">
          <Ikon nama="ok" /> Bot Telegram terhubung
        </span>
      {:else}
        <span class="rounded-full border border-warn/50 bg-warn/10 px-3 py-0.5 text-xs font-medium text-warn">
          <Ikon nama="jam" /> Token / Chat ID Telegram belum diisi
        </span>
      {/if}
    </div>
    <p class="mt-1 text-sm text-mute">
      Atur nyala/mati tiap jenis notifikasi untuk Telegram (dari server) maupun pop-up browser. Tetap bisa diatur meski ESP32 sedang offline.
    </p>

    <div class="mt-4 flex flex-wrap items-center justify-between gap-3 rounded-xl border border-line bg-white/[0.02] p-3">
      <div>
        <p class="font-semibold">Kirim ke Telegram Bot</p>
        <p class="text-sm text-mute">Saklar utama pengiriman seluruh notifikasi ke bot Telegram server.</p>
      </div>
      <div class="flex items-center gap-3">
        <button type="button" role="switch" aria-checked={notifCfg.telegram_enabled}
          aria-label="Saklar utama notifikasi Telegram Bot" class="saklar"
          disabled={notifSibuk} onclick={() => ubahSaklarNotif('telegram_enabled')}></button>
        <span class="min-w-[4.5rem] text-sm font-semibold">{notifCfg.telegram_enabled ? 'Aktif' : 'Nonaktif'}</span>
      </div>
    </div>

    <div class="mt-3 divide-y divide-line rounded-xl border border-line bg-white/[0.015]">
      {#each daftarJenisNotif as item}
        <div class="flex items-start justify-between gap-3 p-3.5 sm:items-center sm:gap-4">
          <div class="min-w-0">
            <p class="font-medium leading-snug">{item.judul}</p>
            <p class="mt-0.5 text-sm text-mute">{item.ket}</p>
          </div>
          <div class="flex shrink-0 flex-col items-end gap-1 sm:flex-row sm:items-center sm:gap-3">
            <button type="button" role="switch" aria-checked={notifCfg[item.key]}
              aria-label={item.judul} class="saklar"
              disabled={notifSibuk} onclick={() => ubahSaklarNotif(item.key)}></button>
            <span class="text-xs font-semibold sm:min-w-[4.2rem] sm:text-sm">{notifCfg[item.key] ? 'Aktif' : 'Nonaktif'}</span>
          </div>
        </div>
      {/each}
    </div>

    <div class="mt-3 flex flex-wrap items-center gap-3">
      <button type="button" onclick={kirimUjiTelegram}
        disabled={!notifCfg.telegram_configured || tgUjiSibuk}
        class="tombol tombol-sekunder">
        {tgUjiSibuk ? 'Mengirim uji…' : 'Kirim pesan uji Telegram'}
      </button>
      {#if notifStatus}
        <span role="status" class="text-sm {notifStatus.ok ? 'text-good' : 'text-bad'}">
          <Ikon nama={notifStatus.ok ? 'ok' : 'bahaya'} /> {notifStatus.teks}
        </span>
      {/if}
    </div>

    {#if tgUjiHasil}
      <p role="status" class="mt-2 text-sm {tgUjiHasil.ok ? 'text-good' : 'text-bad'}">
        <Ikon nama={tgUjiHasil.ok ? 'ok' : 'bahaya'} /> {tgUjiHasil.teks}
      </p>
    {/if}
    {#if !notifCfg.telegram_configured}
      <p class="mt-2 text-sm text-mute">
        Untuk mengaktifkan bot Telegram, isi <code>TELEGRAM_BOT_TOKEN</code> dan <code>TELEGRAM_CHAT_ID</code> pada file <code>.env</code> server lalu restart backend.
      </p>
    {/if}
  </section>

  <!-- NOTIFIKASI BROWSER (saklar nyala/mati lokal browser) -->
  <section class="kartu mt-4 p-4 md:max-w-2xl">
    <h2 class="text-lg font-semibold">Notifikasi pop-up browser</h2>
    <p class="mt-1 text-sm text-mute">Muncul di layar browser ini mengikuti pilihan jenis peringatan di atas, selama halaman terbuka atau diminimalkan.</p>

    <div class="mt-4 flex items-center gap-4">
      <button type="button" role="switch" aria-checked={live.notifAktif} aria-label="Notifikasi pop-up browser" class="saklar"
        onclick={() => setNotifAktif(!live.notifAktif)}></button>
      <span class="text-lg font-semibold">{live.notifAktif ? 'Aktif' : 'Nonaktif'}</span>
    </div>

    {#if !live.notifAktif}
      <p class="mt-3 text-sm text-ink2">Notifikasi browser dimatikan. Peringatan tetap tampil di halaman Dashboard dan Peringatan.</p>
    {:else if live.izin === 'granted'}
      <p class="mt-3 text-sm text-good"><Ikon nama="ok" /> Notifikasi akan muncul di browser ini.</p>
      <button type="button" onclick={kirimNotifikasiUji} class="tombol tombol-sekunder mt-2">Kirim notifikasi uji browser</button>
    {:else if live.izin === 'denied'}
      <p class="mt-3 text-sm text-bad"><Ikon nama="bahaya" /> Notifikasi diblokir browser. Klik ikon gembok di sebelah alamat web, lalu izinkan Notifikasi.</p>
    {:else if live.izin === 'unsupported'}
      <p class="mt-3 text-sm text-ink2">Browser ini tidak mendukung notifikasi.</p>
    {:else}
      <p class="mt-3 text-sm text-ink2">Browser belum memberi izin.</p>
      <button type="button" onclick={aktifkanNotifikasi} class="tombol mt-2">Izinkan di browser</button>
    {/if}
    <p class="mt-3 text-sm text-mute">Pilihan izin pop-up ini tersimpan di browser ini saja, sedangkan pilihan per jenis di atas tersimpan di database server.</p>
  </section>
{/if}
