<script>
  // tone: ok | warn | bad | mute. Selalu ada ikon + teks, bukan warna saja (DES 02)
  let { tone = 'mute', icon = '', label = '' } = $props();
  const warna = {
    ok: 'bg-emerald-100 text-emerald-900 border-emerald-700',
    warn: 'bg-amber-100 text-amber-900 border-amber-700',
    bad: 'bg-red-100 text-red-900 border-red-700',
    mute: 'bg-slate-200 text-slate-800 border-slate-500'
  };
</script>

<span class="inline-flex items-center gap-1.5 rounded-full border px-2.5 py-0.5 text-sm font-medium {warna[tone]}">
  <span aria-hidden="true">{icon}</span>{label}
</span>
