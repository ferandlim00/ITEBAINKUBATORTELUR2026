<script>
  import Ikon from '$lib/components/Ikon.svelte';
  import '../app.css';
  import { page } from '$app/state';
  import { goto } from '$app/navigation';
  import { auth, initAuth, keluar } from '$lib/auth.svelte.js';
  import { live, startLive, stopLive } from '$lib/live.svelte.js';
  import { trans } from '$lib/transisi.svelte.js';

  let { children } = $props();

  // Setiap kartu yang muncul di halaman diberi jeda bertahap supaya tampil bergiliran (halus saat pindah menu)
  function animasiMuncul(node) {
    let urut = 0;
    let reset;
    const proses = () => {
      node.querySelectorAll('.kartu:not([data-m])').forEach((el) => {
        el.dataset.m = '1';
        el.style.setProperty('--tunda', `${Math.min(urut, 12) * 70}ms`);
        el.classList.add('muncul');
        urut++;
      });
      clearTimeout(reset);
      reset = setTimeout(() => (urut = 0), 200);
    };
    const pengamat = new MutationObserver(proses);
    pengamat.observe(node, { childList: true, subtree: true });
    proses();
    return { destroy() { pengamat.disconnect(); clearTimeout(reset); } };
  }

  // Satu peran (Operator): semua menu terbuka untuk yang sudah login
  const menu = [
    { href: '/', teks: 'Dashboard', ikon: 'home' },
    { href: '/kontrol', teks: 'Kontrol', ikon: 'kontrol' },
    { href: '/grafik', teks: 'Grafik', ikon: 'grafik' },
    { href: '/pengaturan', teks: 'Pengaturan', ikon: 'atur' },
    { href: '/peringatan', teks: 'Peringatan', ikon: 'bel' },
    { href: '/riwayat', teks: 'Riwayat', ikon: 'riwayat' }
  ];

  // Navigasi bawah khusus ponsel: 5 menu langsung tanpa perlu tombol "Lainnya" (Grafik disembunyikan di mode ponsel)
  const menuBawah = [
    { href: '/', teks: 'Beranda', ikon: 'home' },
    { href: '/kontrol', teks: 'Kontrol', ikon: 'kontrol' },
    { href: '/peringatan', teks: 'Peringatan', ikon: 'bel' },
    { href: '/pengaturan', teks: 'Atur', ikon: 'atur' },
    { href: '/riwayat', teks: 'Riwayat', ikon: 'riwayat' }
  ];

  const KUNCI = 'menu_kecil';
  function bacaKecil() {
    try { return localStorage.getItem(KUNCI) === '1'; } catch { return false; }
  }

  let kecil = $state(bacaKecil()); // menu samping diperkecil (layar lebar)

  let path = $derived(page.url.pathname);
  let diLogin = $derived(path === '/login');
  let judulHalaman = $derived(menu.find((m) => m.href === path)?.teks ?? 'Smart Hatchery');
  // Dashboard disembunyikan di balik tabir login, lalu muncul halus saat tabir bergeser keluar
  let sembunyi = $derived(trans.tampil && !trans.keluar);
  let tabirGaya = $derived.by(() => {
    const r = trans.rect;
    if (!trans.penuh && r) return `left:${r.left}px;top:${r.top}px;width:${r.width}px;height:${r.height}px;border-radius:30px`;
    return 'left:0;top:0;width:100vw;height:100dvh;border-radius:0';
  });
  let jumlahAktif = $derived(live.alerts.filter((a) => a.status === 'active').length);

  function ubahKecil() {
    kecil = !kecil;
    try { localStorage.setItem(KUNCI, kecil ? '1' : '0'); } catch { /* abaikan */ }
  }

  $effect(() => { initAuth(); });
  $effect(() => {
    if (!auth.siap) return;
    if (!auth.user && !diLogin) goto('/login');
    if (auth.user && diLogin && !trans.sedang) goto('/'); // saat transisi masuk, halaman login yang memindahkan
  });
  // Pemantauan data alat hanya jalan saat sudah login
  $effect(() => {
    if (auth.user) startLive(); else stopLive();
  });

  async function klikKeluar() {
    await keluar();
    goto('/login');
  }
</script>



{#snippet tip(teks)}
  <span class="pointer-events-none absolute left-full z-50 ml-3 hidden whitespace-nowrap rounded-lg border border-line bg-panel px-3 py-1.5 text-sm font-medium text-ink opacity-0 shadow-lg transition-opacity group-hover:opacity-100 group-focus-visible:opacity-100 md:block" aria-hidden="true">{teks}</span>
{/snippet}

<div class="min-h-screen text-ink">
  {#if !auth.siap}
    <p class="p-6 text-mute">Memuat…</p>
  {:else if diLogin}
    {@render children()}
  {:else if auth.user}
    <div class="md:flex">
      <!-- MENU SAMPING: khusus layar lebar (tablet/desktop), di ponsel pindah ke navigasi bawah -->
      <aside id="menu-samping" aria-label="Menu samping"
        style="opacity: {sembunyi ? 0 : 1}; transition: width 0.35s cubic-bezier(0.4, 0, 0.2, 1), opacity 0.9s ease 0.1s"
        class="hidden md:sticky md:top-0 md:z-40 md:flex md:h-screen md:flex-col menu-samping border-r border-line bg-night/95 {kecil ? 'md:w-[4.25rem]' : 'md:w-60'}">
        <a href="/" class="flex items-center gap-3 px-4 py-5 text-lg font-bold tracking-tight text-ink no-underline">
          <img src="/logo.png" alt="" class="h-10 w-9 flex-none object-contain" />
          <span class="rail-label {kecil ? 'rail-kecil' : ''}">Smart Hatchery</span>
        </a>

        <nav class="flex-1 px-2" aria-label="Menu utama">
          <ul class="space-y-1">
            {#each menu as m}
              <li>
                <a href={m.href} aria-current={path === m.href ? 'page' : undefined}
                  class="group relative flex items-center gap-3 rounded-xl px-3 py-3 font-medium no-underline transition-colors {path === m.href ? 'nav-aktif' : 'text-ink2 hover:bg-white/5 hover:text-ink'}">
                  <span class="relative flex h-6 w-6 flex-none items-center justify-center">
                    <Ikon nama={m.ikon} ukuran="1.5rem" />
                    {#if m.href === '/peringatan' && jumlahAktif > 0}
                      <span class="absolute -right-2.5 -top-2.5 rounded-full bg-red-700 px-1.5 text-xs font-bold text-white" aria-label="{jumlahAktif} peringatan aktif">{jumlahAktif}</span>
                    {/if}
                  </span>
                  <span class="rail-label {kecil ? 'rail-kecil' : ''}">{m.teks}</span>
                  {#if kecil}{@render tip(m.teks)}{/if}
                </a>
              </li>
            {/each}
          </ul>
        </nav>

        <div class="space-y-1 border-t border-line p-2">
          <p class="rail-blok px-3 py-1 text-sm text-ink2 {kecil ? 'rail-kecil' : ''}">Masuk sebagai <strong>{auth.user.username}</strong></p>
          <button type="button" onclick={klikKeluar}
            class="group relative flex w-full items-center gap-3 rounded-xl px-3 py-3 font-medium text-ink transition-colors hover:bg-bad/10 hover:text-bad">
            <span class="flex h-6 w-6 flex-none items-center justify-center"><Ikon nama="keluar" ukuran="1.5rem" /></span>
            <span class="rail-label {kecil ? 'rail-kecil' : ''}">Keluar</span>
            {#if kecil}{@render tip('Keluar')}{/if}
          </button>
          <button type="button" onclick={ubahKecil} aria-expanded={!kecil} aria-controls="menu-samping"
            class="group relative hidden w-full items-center gap-3 rounded-xl px-3 py-3 font-medium text-ink2 transition-colors hover:bg-white/5 md:flex">
            <span class="flex h-6 w-6 flex-none items-center justify-center transition-transform duration-300 {kecil ? 'rotate-180' : ''}"><Ikon nama="kiri" ukuran="1.5rem" /></span>
            <span class="rail-label {kecil ? 'rail-kecil' : ''}">Kecilkan menu</span>
            {#if kecil}{@render tip('Perbesar menu')}{/if}
          </button>
        </div>
      </aside>

      <div class="min-w-0 flex-1" style="opacity: {sembunyi ? 0 : 1}; transition: opacity 0.9s ease 0.25s">
        <!-- Bilah atas ringkas khusus ponsel (aman dari status bar / notch APK) -->
        <header style="padding-top: max(0.65rem, env(safe-area-inset-top));"
          class="sticky top-0 z-20 flex items-center justify-between gap-3 border-b border-line bg-night/90 px-4 pb-2.5 backdrop-blur-md md:hidden">
          <a href="/" class="flex items-center gap-2.5 text-ink no-underline">
            <img src="/logo.png" alt="" class="h-7 w-6 flex-none object-contain" />
            <div class="leading-tight">
              <span class="block text-sm font-bold tracking-tight">Smart Hatchery</span>
              <span class="block text-xs text-mute">{judulHalaman}</span>
            </div>
          </a>
          <div class="flex items-center gap-2">
            {#if jumlahAktif > 0}
              <a href="/peringatan" class="inline-flex items-center gap-1 rounded-full border border-bad/60 bg-bad/15 px-2.5 py-1 text-xs font-bold text-bad no-underline" aria-label="{jumlahAktif} peringatan aktif">
                <Ikon nama="bahaya" ukuran="0.95rem" /> {jumlahAktif}
              </a>
            {/if}
            <span class="inline-flex items-center gap-1.5 rounded-full border border-line bg-panel px-2.5 py-1 text-xs font-medium text-ink2">
              <span class="h-2 w-2 rounded-full {live.data && !live.error ? 'bg-good' : 'bg-warn'}"></span>
              {live.data && !live.error ? 'Aktif' : 'Offline'}
            </span>
            <button type="button" onclick={klikKeluar} aria-label="Keluar dari akun" title="Keluar"
              class="grid h-9 w-9 place-items-center rounded-full border border-line bg-panel text-ink2 transition-colors active:bg-bad/15 active:text-bad">
              <Ikon nama="keluar" ukuran="1.15rem" />
            </button>
          </div>
        </header>

        {#key path}
          <div use:animasiMuncul class="halaman-masuk mx-auto max-w-5xl px-3.5 pt-4 pb-24 sm:px-4 sm:py-6 md:pb-8 {sembunyi ? 'jeda' : ''}">
            {@render children()}
          </div>
        {/key}
      </div>
    </div>

    <!-- NAVBAR BAWAH KHUSUS PONSEL / APK (5 menu rapi langsung tanpa "Lainnya") -->
    <nav aria-label="Navigasi bawah ponsel"
      style="opacity: {sembunyi ? 0 : 1}; transition: opacity 0.6s ease 0.2s"
      class="nav-bawah fixed inset-x-0 bottom-0 z-30 border-t border-line md:hidden">
      <ul class="grid grid-cols-5 items-stretch px-1 pt-1.5">
        {#each menuBawah as m}
          <li>
            <a href={m.href} aria-current={path === m.href ? 'page' : undefined}
              class="nav-bawah-item flex min-h-[3.35rem] flex-col items-center justify-center gap-0.5 rounded-xl px-1 py-1 text-[11px] font-semibold no-underline {path === m.href ? 'text-ink' : 'text-mute'}">
              <span class="nav-bawah-pil relative flex h-8 w-12 items-center justify-center rounded-full">
                <Ikon nama={m.ikon} ukuran="1.35rem" />
                {#if m.href === '/peringatan' && jumlahAktif > 0}
                  <span class="absolute right-1 top-0.5 flex h-4 min-w-[1rem] items-center justify-center rounded-full bg-red-600 px-1 text-[10px] font-bold leading-none text-white" aria-label="{jumlahAktif} peringatan aktif">{jumlahAktif}</span>
                {/if}
              </span>
              <span class="truncate max-w-full">{m.teks}</span>
            </a>
          </li>
        {/each}
      </ul>
    </nav>
  {/if}

  <!-- Tabir transisi masuk: kartu login oranye membesar ke seluruh layar, lalu bergeser keluar ke kiri -->
  {#if trans.tampil}
    <div class="tabir {trans.keluar ? 'tabir-keluar' : ''}" style={tabirGaya} aria-hidden="true">
      <img src="/logo-gelap.png" alt="" class="denyut h-24 w-20 object-contain" />
    </div>
  {/if}
</div>
