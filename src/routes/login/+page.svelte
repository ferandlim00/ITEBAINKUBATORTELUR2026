<script>
  import { masuk } from '$lib/auth.svelte.js';
  import { goto } from '$app/navigation';

  let username = $state('');
  let password = $state('');
  let error = $state('');
  let sibuk = $state(false);

  async function kirim(e) {
    e.preventDefault();
    if (!username.trim() || !password) { error = 'Isi nama pengguna dan kata sandi.'; return; }
    sibuk = true; error = '';
    try {
      await masuk(username.trim(), password);
      goto('/');
    } catch (err) {
      error = err.message;
      password = '';
    } finally {
      sibuk = false;
    }
  }
</script>

<main class="mx-auto mt-16 max-w-sm px-4">
  <h1 class="text-2xl font-semibold">Smart Hatchery Monitoring</h1>
  <p class="mt-1 text-slate-600">Masuk untuk memantau dan mengendalikan inkubator.</p>

  <form onsubmit={kirim} class="mt-6 space-y-4 rounded-lg border border-slate-300 bg-white p-5">
    <div>
      <label for="username" class="block text-sm font-medium">Nama pengguna</label>
      <input id="username" type="text" autocomplete="username" bind:value={username}
        class="mt-1 w-full rounded border border-slate-400 px-3 py-2" />
    </div>
    <div>
      <label for="password" class="block text-sm font-medium">Kata sandi</label>
      <input id="password" type="password" autocomplete="current-password" bind:value={password}
        class="mt-1 w-full rounded border border-slate-400 px-3 py-2" />
    </div>
    {#if error}<p role="alert" class="text-sm text-red-900">⚠ {error}</p>{/if}
    <button type="submit" disabled={sibuk}
      class="w-full rounded bg-slate-900 px-4 py-2 text-white disabled:cursor-not-allowed disabled:opacity-40">
      {sibuk ? 'Memeriksa…' : 'Masuk'}
    </button>
  </form>
</main>
