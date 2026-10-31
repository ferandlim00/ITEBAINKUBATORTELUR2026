# Inkubator Web (Frontend)

SvelteKit (mode SPA) + Tailwind. Backend: Go Fiber (Dodos).

## Menjalankan
    npm install
    npm run dev      # buka http://localhost:5173

## Data dummy vs backend
- `.env` → `VITE_USE_MOCK=true` memakai data dari `src/lib/mock/telemetry.js`
- Saat backend siap: `VITE_USE_MOCK=false`. Request `/api/*` diteruskan ke `localhost:3000` (lihat `vite.config.js`).
- Semua fetch hanya lewat `src/lib/api.js`.

## Struktur
    src/lib/api.js           satu-satunya lapisan akses data
    src/lib/mock/            data dummy (normal, kedaluwarsa, peringatan)
    src/routes/              halaman
