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

  const KUNCI = 'menu_kecil';
  function bacaKecil() {
    try { return localStorage.getItem(KUNCI) === '1'; } catch { return false; }
  }

  let kecil = $state(bacaKecil()); // menu samping diperkecil (layar lebar)
  let buka = $state(false); // menu samping terbuka (layar HP)

  let path = $derived(page.url.pathname);
  let diLogin = $derived(path === '/login');
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
  // Menu di HP menutup sendiri setelah pindah halaman
  $effect(() => { path; buka = false; });

  async function klikKeluar() {
    await keluar();
    goto('/login');
  }
</script>

<svelte:window onkeydown={(e) => { if (e.key === 'Escape') buka = false; }} />



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
      {#if buka}
        <button type="button" class="fixed inset-0 z-30 bg-black/40 md:hidden" aria-label="Tutup menu" onclick={() => (buka = false)}></button>
      {/if}

      <!-- MENU SAMPING: di layar lebar bisa diperkecil jadi ikon, di HP berupa laci yang digeser -->
      <aside id="menu-samping" aria-label="Menu samping"
        style="opacity: {sembunyi ? 0 : 1}; transition: transform 0.3s ease, width 0.35s cubic-bezier(0.4, 0, 0.2, 1), opacity 0.9s ease 0.1s"
        class="fixed inset-y-0 left-0 z-40 flex w-64 flex-col menu-samping border-r border-line bg-night/95 shadow-2xl transition-[transform,width] duration-300 md:sticky md:top-0 md:h-screen md:translate-x-0 md:shadow-none {buka ? 'translate-x-0' : '-translate-x-full'} {kecil ? 'md:w-[4.25rem]' : 'md:w-60'}">
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
        <!-- Bilah atas khusus HP -->
        <div class="sticky top-0 z-20 flex items-center gap-3 border-b border-line bg-night/95 px-4 py-3 md:hidden">
          <button type="button" class="tombol tombol-sekunder tombol-kecil" aria-label="Buka menu" aria-expanded={buka} aria-controls="menu-samping" onclick={() => (buka = true)}><Ikon nama="menu" class="mr-1" /> Menu</button>
          <span class="flex items-center gap-2 font-bold text-ink"><img src="/logo.png" alt="" class="h-7 w-6 object-contain" />Smart Hatchery</span>
          {#if jumlahAktif > 0}<span class="ml-auto rounded-full bg-red-700 px-2 text-sm font-bold text-white" aria-label="{jumlahAktif} peringatan aktif"><Ikon nama="bahaya" /> {jumlahAktif}</span>{/if}
        </div>

        {#key path}
          <div use:animasiMuncul class="halaman-masuk mx-auto max-w-5xl px-3 py-5 sm:px-4 sm:py-6 {sembunyi ? 'jeda' : ''}">
            {@render children()}
          </div>
        {/key}
      </div>
    </div>
  {/if}

  <!-- Tabir transisi masuk: kartu login oranye membesar ke seluruh layar, lalu bergeser keluar ke kiri -->
  {#if trans.tampil}
    <div class="tabir {trans.keluar ? 'tabir-keluar' : ''}" style={tabirGaya} aria-hidden="true">
      <img src="/logo-gelap.png" alt="" class="denyut h-24 w-20 object-contain" />
    </div>
  {/if}
</div>
