<script>
  // Kartu angka tunggal: suhu, kelembapan, lampu, ventilasi
  let { title, value, unit = '', note = '', old = false } = $props();
</script>

<div class="rounded-lg border border-slate-300 bg-white p-4 {old ? 'opacity-60' : ''}">
  <p class="text-sm text-slate-600">{title}</p>
  <p class="mt-1 break-words text-3xl font-semibold text-slate-900">{value}<span class="ml-1 text-lg font-normal text-slate-600">{unit}</span></p>
  {#if note}<p class="mt-1 text-sm text-slate-600">{note}</p>{/if}
</div>
