<script>
  import StatusBadge from './StatusBadge.svelte';
  import { statusPutar } from '$lib/turning.js';
  import { nextText, ageText, ageSeconds } from '$lib/format.js';

  let { turning, motor, now, old = false } = $props();
  let s = $derived(statusPutar[turning.status] ?? statusPutar.idle);
</script>

<section class="kartu p-4 {old ? 'opacity-60' : ''}">
  <div class="flex items-center justify-between gap-2">
    <h3 class="text-lg font-semibold">Pembalik telur</h3>
    <StatusBadge tone={s.tone} icon={s.icon} label={s.label} />
  </div>
  <p class="mt-1 text-sm text-mute">Semua telur dibalik bersamaan.</p>

  <dl class="mt-3 grid grid-cols-2 gap-y-2 text-sm">
    <dt class="text-mute">Motor</dt>
    <dd class="text-right font-medium">{motor.running ? 'Berputar' : 'Berhenti'}</dd>

    <dt class="text-mute">Encoder</dt>
    <dd class="text-right font-medium">{motor.encoder_ok ? 'Normal' : 'Tidak ada pulsa'}</dd>

    <dt class="text-mute">Terakhir membalik</dt>
    <dd class="text-right font-medium">{turning.last_turned_at ? ageText(ageSeconds(turning.last_turned_at, now)) : '—'}</dd>

    <dt class="text-mute">Pembalikan berikutnya</dt>
    <dd class="text-right font-medium">{turning.status === 'lockdown' ? 'Berhenti (lockdown)' : nextText(turning.next_turning_at, now)}</dd>
  </dl>
</section>
