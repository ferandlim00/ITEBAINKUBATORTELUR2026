// Status login untuk seluruh aplikasi. Penegakan akses yang sebenarnya ada di backend;
// ini hanya untuk mengatur apa yang ditampilkan.
import { login, logout, getMe, setUnauthorizedHandler } from './api.js';

export const auth = $state({ user: null, siap: false });
let sudahInit = false;

export async function initAuth() {
  if (sudahInit) return;
  sudahInit = true;
  // Sesi habis di tengah pemakaian: kosongkan user, layout otomatis mengarahkan ke login
  setUnauthorizedHandler(() => { auth.user = null; });
  try { auth.user = await getMe(); } catch { auth.user = null; }
  auth.siap = true;
}

export async function masuk(username, password) {
  const r = await login(username, password);
  auth.user = r.user;
}

export async function keluar() {
  try { await logout(); } catch { /* tetap keluar di sisi tampilan */ }
  auth.user = null;
}
