<script>
  import Ikon from '$lib/components/Ikon.svelte';
  import { tick } from 'svelte';
  import { goto } from '$app/navigation';
  import { masuk } from '$lib/auth.svelte.js';
  import { trans } from '$lib/transisi.svelte.js';

  let username = $state('');
  let password = $state('');
  let lihat = $state(false);
  let error = $state('');
  let sibuk = $state(false);
  let menutup = $state(false); // panel oranye menyapu ke kiri menutupi kartu
  let goyang = $state(false);
  let kotak;

  const cepat = typeof matchMedia !== 'undefined' && matchMedia('(prefers-reduced-motion: reduce)').matches;
  const tunggu = (ms) => new Promise((r) => setTimeout(r, cepat ? 0 : ms));
  const bingkai = () => new Promise((r) => requestAnimationFrame(() => requestAnimationFrame(r)));

  async function kirim(e) {
    e.preventDefault();
    if (sibuk) return;
    if (!username.trim() || !password) { error = 'Isi nama pengguna dan kata sandi.'; return; }
    sibuk = true; error = '';
    trans.sedang = true; // cegah layout memindahkan halaman sebelum animasi selesai
    try {
      await masuk(username.trim(), password);
    } catch (err) {
      error = err.message;
      password = '';
      trans.sedang = false;
      sibuk = false;
      goyang = true;
      setTimeout(() => (goyang = false), 500);
      return;
    }

    // 1) Panel oranye menyapu ke kiri menutupi kartu, tulisan sambutan keluar ke kiri
    menutup = true;
    await tunggu(1000);

    // 2) Kartu (sudah oranye penuh) membesar mengisi seluruh layar
    const r = kotak.getBoundingClientRect();
    trans.rect = { left: r.left, top: r.top, width: r.width, height: r.height };
    trans.penuh = false;
    trans.keluar = false;
    trans.tampil = true;
    await tick();
    await bingkai();
    trans.penuh = true;
    await tunggu(950);

    // 3) Pindah ke dashboard di balik tabir, lalu tabir bergeser keluar ke kiri
    await goto('/');
    await tunggu(700);
    trans.keluar = true;
    await tunggu(1000);
    trans.tampil = false; trans.keluar = false; trans.penuh = false; trans.rect = null; trans.sedang = false;
  }
</script>

<main class="grid min-h-screen place-items-center px-4 py-8">
  <div bind:this={kotak} class="login-kotak halaman-masuk">
    <form id="form-masuk" onsubmit={kirim} novalidate class="login-form {goyang ? 'goyang' : ''}">
      <h1 class="text-3xl font-bold tracking-tight">Masuk</h1>
      <p class="mt-1 text-mute">Gunakan akun operator untuk memantau inkubator.</p>

      <div class="mt-6">
        <label for="username" class="block text-sm font-semibold">Nama pengguna</label>
        <div class="relative mt-1">
          <Ikon nama="user" ukuran="1.25rem" class="pointer-events-none absolute left-3.5 top-1/2 -translate-y-1/2 text-mute" />
          <input id="username" type="text" autocomplete="username" bind:value={username} class="input" style="padding-left: 2.8rem" />
        </div>
      </div>

      <div class="mt-4">
        <label for="password" class="block text-sm font-semibold">Kata sandi</label>
        <div class="relative mt-1">
          <Ikon nama="kunci" ukuran="1.25rem" class="pointer-events-none absolute left-3.5 top-1/2 -translate-y-1/2 text-mute" />
          <input id="password" type={lihat ? 'text' : 'password'} autocomplete="current-password" bind:value={password} class="input" style="padding-left: 2.8rem; padding-right: 5.5rem" />
          <button type="button" onclick={() => (lihat = !lihat)} class="absolute right-2 top-1/2 -translate-y-1/2 rounded-lg px-2 py-1 text-sm font-semibold text-ember2 hover:bg-white/5">
            <Ikon nama={lihat ? 'mata-coret' : 'mata'} class="mr-1" />{lihat ? 'Sembunyikan' : 'Tampilkan'}
          </button>
        </div>
      </div>

      {#if error}<p role="alert" class="mt-3 text-sm font-medium text-bad"><Ikon nama="bahaya" /> {error}</p>{/if}
    </form>

    <!-- Panel sambutan: tombol Masuk ada di sini dan mengirim formulir di sebelah kiri -->
    <div class="sambutan-bg {menutup ? 'penuh' : ''}" aria-hidden="true"></div>
    <div class="sambutan-isi {menutup ? 'keluar' : ''}">
      <img src="/logo-gelap.png" alt="Logo Smart Hatchery" class="h-16 w-14 object-contain" />
      <h2 class="text-xl font-bold leading-snug tracking-tight md:text-2xl">Selamat datang di Smart Hatchery Monitoring</h2>
      <p class="hidden text-sm font-medium opacity-80 md:block">Pantau suhu, kelembapan, dan pembalikan telur inkubator dari satu tempat.</p>
      <button type="submit" form="form-masuk" class="tombol-kaca" disabled={sibuk}>{sibuk ? 'Memeriksa…' : 'Masuk'}</button>
    </div>
  </div>
</main>
