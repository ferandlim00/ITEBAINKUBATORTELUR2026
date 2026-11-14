import { sveltekit } from '@sveltejs/kit/vite';
import tailwindcss from '@tailwindcss/vite';
import { defineConfig } from 'vite';

export default defineConfig({
  plugins: [tailwindcss(), sveltekit()],
  server: {
    // Nanti saat backend Go Fiber jalan di :3000, request /api diteruskan ke sana
    proxy: { '/api': 'http://127.0.0.1:3000' }
  }
});
