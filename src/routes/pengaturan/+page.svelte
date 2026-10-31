<script>
  import {
    getTelemetry, getThresholds, getSettings, getLimits,
    saveTargetTemperature, saveMotorSettings, saveWifi
  } from '$lib/api.js';
  import { ageSeconds } from '$lib/format.js';

  let data = $state(null); // telemetri, untuk tahu perangkat terhubung atau tidak
  let settings = $state(null);
  let limits = $state(null);
  let error = $state('');
  let now = $state(Date.now());
  let staleDetik = $state(60);

  // Isian di layar (draft). Baru dikirim saat tombol ditekan
  let target = $state(37.5);
  let motor = $state({ interval_minutes: 120, duration_seconds: 20, speed_percent: 50 });
  let ssid = $state('');
  let sandi = $state('');
  let lihatSandi = $state(false);
  let konfirmasiWifi = $state(false);

  let sibuk = $state({});
  let hasil = $state({});

  const kolomMotor = [
    { key: 'interval_minutes', label: 'Interval pembalikan', unit: 'menit' },
    { key: 'duration_seconds', label: 'Durasi putar', unit: 'detik' },
    { key: 'speed_percent', label: 'Kecepatan motor', unit: '%' }
  ];

  $effect(() => {
    Promise.all([getSettings(), getLimits(), getThresholds()]).then(([s, l, t]) => {
      settings = s; limits = l; staleDetik = t.stale_seconds;
      target = s.target_temperature;
      motor = { ...s.motor };
      ssid = s.wifi.ssid;
    }).catch((e) => (error = e.message));
  });

  async function muat() {
    try { data = await getTelemetry(); } catch (e) { error = e.message; }
  }
  $effect(() => {
    muat();
    const ambil = setInterval(muat, 5000);
    const detak = setInterval(() => (now = Date.now()), 1000);
    return () => { clearInterval(ambil); clearInterval(detak); };
  });

  let kedaluwarsa = $derived(!!data && (data.connection === 'offline' || ageSeconds(data.updated_at, now) > staleDetik));
  let terkunci = $derived(!data || kedaluwarsa);

  async function jalankan(kunci, fn, pesanOk) {
    sibuk[kunci] = true; hasil[kunci] = null;
    try {
      await fn();
      hasil[kunci] = { ok: true, teks: pesanOk };
      return true;
    } catch (e) {
      hasil[kunci] = { ok: false, teks: `Gagal mengirim: ${e.message}` };
      return false;
    } finally {
      sibuk[kunci] = false;
    }
  }

  function salah(kunci, teks) { hasil[kunci] = { ok: false, teks }; }

  async function simpanTarget() {
    hasil.target = null;
    const v = Number(target);
    const b = limits.target_temperature;
    if (target === '' || !Number.isFinite(v)) return salah('target', 'Isi target suhu dengan angka.');
    if (v < b.min || v > b.max) return salah('target', `Target suhu harus antara ${b.min} dan ${b.max} °C.`);
    if (await jalankan('target', () => saveTargetTemperature(v), `Target suhu ${v}°C terkirim ke perangkat.`)) {
      settings.target_temperature = v;
    }
  }

  async function simpanMotor() {
    hasil.motor = null;
    const baru = {};
    for (const k of kolomMotor) {
      const v = Number(motor[k.key]);
      const b = limits.motor[k.key];
      if (motor[k.key] === '' || !Number.isInteger(v)) return salah('motor', `${k.label}: isi dengan bilangan bulat.`);
      if (v < b.min || v > b.max) return salah('motor', `${k.label} harus antara ${b.min} dan ${b.max} ${k.unit}.`);
      baru[k.key] = v;
    }
    if (await jalankan('motor', () => saveMotorSettings(baru), 'Pengaturan motor terkirim ke perangkat.')) {
      settings.motor = baru;
    }
  }

  function periksaWifi() {
    hasil.wifi = null;
    const nama = ssid.trim();
    if (nama.length < 1 || nama.length > 32) return salah('wifi', 'Nama Wi-Fi (SSID) harus 1–32 karakter.');
    if (sandi.length < 8 || sandi.length > 63) return salah('wifi', 'Kata sandi Wi-Fi harus 8–63 karakter.');
    konfirmasiWifi = true;
  }

  async function kirimWifi() {
    konfirmasiWifi = false;
    const nama = ssid.trim();
    const ok = await jalankan('wifi', () => saveWifi({ ssid: nama, password: sandi }),
      'Pengaturan Wi-Fi terkirim. Perangkat akan mencoba tersambung ke jaringan baru, dan sementara bisa terputus.');
    sandi = ''; // kata sandi tidak disimpan di layar
    lihatSandi = false;
    if (ok) settings.wifi.ssid = nama;
  }
</script>

<h1 class="text-2xl font-semibold">Pengaturan</h1>

{#if error}
  <p class="mt-4 rounded border border-red-700 bg-red-100 p-3 text-red-900">⚠ Gagal memuat data: {error}. Muat ulang halaman.</p>
{:else if !data || !settings || !limits}
  <p class="mt-4 text-slate-600">Memuat pengaturan…</p>
{:else}
  {#if terkunci}
    <p class="mt-4 rounded border border-amber-700 bg-amber-100 p-3 text-amber-900">
      ⏱ Pengaturan dinonaktifkan karena data kedaluwarsa atau perangkat terputus. Perubahan tidak dikirim ke perangkat yang tidak merespons.
    </p>
  {/if}

  <div class="mt-4 grid gap-4 md:grid-cols-2">
    <!-- TARGET SUHU -->
    <section class="rounded-lg border border-slate-300 bg-white p-4">
      <h2 class="text-lg font-semibold">Target suhu</h2>
      <p class="mt-1 text-sm text-slate-600">Saat ini: {settings.target_temperature}°C. Kelembapan hanya indikator pengaman, tidak punya target.</p>
      <fieldset class="mt-3" disabled={terkunci}>
        <label for="target" class="block text-sm font-medium">Target suhu (°C)</label>
        <input id="target" type="number" step="0.1" bind:value={target} class="mt-1 w-full rounded border border-slate-400 px-3 py-2" />
        <p class="mt-1 text-sm text-slate-600">Batas yang diizinkan: {limits.target_temperature.min}–{limits.target_temperature.max} °C</p>
        <button type="button" onclick={simpanTarget} disabled={terkunci || sibuk.target}
          class="mt-4 rounded bg-slate-900 px-4 py-2 text-white disabled:cursor-not-allowed disabled:opacity-40">
          {sibuk.target ? 'Mengirim…' : 'Simpan target'}
        </button>
      </fieldset>
      {#if hasil.target}<p role="status" class="mt-2 text-sm {hasil.target.ok ? 'text-emerald-900' : 'text-red-900'}">{hasil.target.ok ? '✓' : '⚠'} {hasil.target.teks}</p>{/if}
    </section>

    <!-- MOTOR DC -->
    <section class="rounded-lg border border-slate-300 bg-white p-4">
      <h2 class="text-lg font-semibold">Motor DC (pembalikan telur)</h2>
      <p class="mt-1 text-sm text-slate-600">Nilai hanya bisa diubah dalam batas yang diizinkan.</p>
      <fieldset class="mt-3 space-y-3" disabled={terkunci}>
        {#each kolomMotor as k}
          <div>
            <label for={'m-' + k.key} class="block text-sm font-medium">{k.label} ({k.unit})</label>
            <input id={'m-' + k.key} type="number" step="1" bind:value={motor[k.key]} class="mt-1 w-full rounded border border-slate-400 px-3 py-2" />
            <p class="mt-1 text-sm text-slate-600">Saat ini {settings.motor[k.key]} · batas {limits.motor[k.key].min}–{limits.motor[k.key].max} {k.unit}</p>
          </div>
        {/each}
        <button type="button" onclick={simpanMotor} disabled={terkunci || sibuk.motor}
          class="rounded bg-slate-900 px-4 py-2 text-white disabled:cursor-not-allowed disabled:opacity-40">
          {sibuk.motor ? 'Mengirim…' : 'Simpan pengaturan motor'}
        </button>
      </fieldset>
      {#if hasil.motor}<p role="status" class="mt-2 text-sm {hasil.motor.ok ? 'text-emerald-900' : 'text-red-900'}">{hasil.motor.ok ? '✓' : '⚠'} {hasil.motor.teks}</p>{/if}
    </section>
  </div>

  <!-- WI-FI -->
  <section class="mt-4 rounded-lg border border-slate-300 bg-white p-4 md:max-w-xl">
    <h2 class="text-lg font-semibold">Wi-Fi perangkat</h2>
    <p class="mt-1 text-sm text-slate-600">
      Jaringan saat ini: {data.wifi?.connected ? data.wifi.ssid : 'terputus'}. Kata sandi tidak pernah ditampilkan kembali setelah dikirim.
    </p>
    <fieldset class="mt-3 space-y-3" disabled={terkunci || sibuk.wifi}>
      <div>
        <label for="ssid" class="block text-sm font-medium">Nama Wi-Fi (SSID)</label>
        <input id="ssid" bind:value={ssid} autocomplete="off" class="mt-1 w-full rounded border border-slate-400 px-3 py-2" />
      </div>
      <div>
        <label for="sandi" class="block text-sm font-medium">Kata sandi Wi-Fi</label>
        <div class="mt-1 flex gap-2">
          <input id="sandi" type={lihatSandi ? 'text' : 'password'} bind:value={sandi} autocomplete="new-password" placeholder="Isi untuk mengganti"
            class="w-full rounded border border-slate-400 px-3 py-2" />
          <button type="button" onclick={() => (lihatSandi = !lihatSandi)} class="whitespace-nowrap rounded border border-slate-400 px-3 text-sm">
            {lihatSandi ? 'Sembunyikan' : 'Tampilkan'}
          </button>
        </div>
      </div>

      {#if konfirmasiWifi}
        <div class="rounded border border-red-700 bg-red-50 p-3 text-sm text-red-900">
          <p class="font-medium">⚠ Kirim pengaturan Wi-Fi ke perangkat?</p>
          <p class="mt-1">Jika nama atau kata sandi salah, perangkat akan terputus dan tidak bisa dipantau atau dikendalikan dari aplikasi sampai diperbaiki langsung di alat. Jaringan baru: <strong>{ssid.trim()}</strong>.</p>
          <div class="mt-3 flex gap-2">
            <button type="button" onclick={kirimWifi} class="rounded bg-red-800 px-4 py-2 text-white">Ya, kirim</button>
            <button type="button" onclick={() => (konfirmasiWifi = false)} class="rounded border border-slate-400 bg-white px-4 py-2 text-slate-900">Batal</button>
          </div>
        </div>
      {:else}
        <button type="button" onclick={periksaWifi} disabled={terkunci || sibuk.wifi}
          class="rounded bg-slate-900 px-4 py-2 text-white disabled:cursor-not-allowed disabled:opacity-40">
          {sibuk.wifi ? 'Mengirim…' : 'Kirim ke perangkat'}
        </button>
      {/if}
    </fieldset>
    {#if hasil.wifi}<p role="status" class="mt-2 text-sm {hasil.wifi.ok ? 'text-emerald-900' : 'text-red-900'}">{hasil.wifi.ok ? '✓' : '⚠'} {hasil.wifi.teks}</p>{/if}
  </section>
{/if}
