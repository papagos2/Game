import { defineConfig } from 'vite';

// Relative base so the build works inside the Capacitor WebView (file-style origin).
export default defineConfig({
  base: './',
  build: {
    target: 'es2020',
    chunkSizeWarningLimit: 1200,
  },
  test: {
    include: ['tests/**/*.test.ts'],
  },
} as any);
