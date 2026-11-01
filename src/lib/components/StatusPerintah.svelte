<script>
  import { live } from '$lib/live.svelte.js';

  // Status perintah dari objek last_command: dispatched -> executed / failed
  let { id } = $props();
  let last = $derived(live.data?.last_command);
  let s = $derived(last && last.id === id ? last.status : 'dispatched');
</script>

{#if s === 'executed'}
  <p role="status" class="text-sm text-emerald-900">✓ Dijalankan oleh alat.</p>
{:else if s === 'failed'}
  <p role="alert" class="text-sm text-red-900">⚠ Alat gagal menjalankan perintah. Coba lagi atau periksa alat.</p>
{:else}
  <p role="status" class="text-sm text-slate-700">⏳ Terkirim ke alat, menunggu konfirmasi…</p>
{/if}
