import adapter from '@sveltejs/adapter-static';

export default {
  kit: {
    // SPA mode: semua rute dilayani index.html, data diambil dari API Go Fiber
    adapter: adapter({ fallback: 'index.html' })
  }
};
