<script>
  import '../app.css';
  import { page } from '$app/state';
  import { goto } from '$app/navigation';
  import { auth, initAuth, keluar } from '$lib/auth.svelte.js';

  let { children } = $props();

  // Satu peran (Operator): semua menu terbuka untuk yang sudah login
  const menu = [
    { href: '/', teks: 'Dashboard' },
    { href: '/kontrol', teks: 'Kontrol' },
    { href: '/pengaturan', teks: 'Pengaturan' },
    { href: '/peringatan', teks: 'Peringatan' },
    { href: '/riwayat', teks: 'Riwayat' }
  ];

  let path = $derived(page.url.pathname);
  let diLogin = $derived(path === '/login');

  $effect(() => { initAuth(); });
  $effect(() => {
    if (!auth.siap) return;
    if (!auth.user && !diLogin) goto('/login');
    if (auth.user && diLogin) goto('/');
  });

  async function klikKeluar() {
    await keluar();
    goto('/login');
  }
</script>

<div class="min-h-screen bg-slate-100 text-slate-900">
  {#if !auth.siap}
    <p class="p-6 text-slate-600">Memuat…</p>
  {:else if diLogin}
    {@render children()}
  {:else if auth.user}
    <header class="border-b border-slate-300 bg-white">
      <div class="mx-auto flex max-w-5xl flex-wrap items-center justify-between gap-2 px-4 py-3">
        <span class="text-lg font-semibold">Smart Hatchery</span>
        <nav class="flex flex-wrap gap-4 text-sm">
          {#each menu as m}
            <a href={m.href} aria-current={path === m.href ? 'page' : undefined}
               class={path === m.href ? 'font-medium underline underline-offset-4' : 'text-slate-700 hover:underline'}>
              {m.teks}
            </a>
          {/each}
        </nav>
        <div class="flex items-center gap-3 text-sm">
          <span class="text-slate-600">{auth.user.username}</span>
          <button type="button" onclick={klikKeluar} class="rounded border border-slate-400 px-3 py-1 hover:bg-slate-100">Keluar</button>
        </div>
      </div>
    </header>
    <div class="mx-auto max-w-5xl px-4 py-6">
      {@render children()}
    </div>
  {/if}
</div>
