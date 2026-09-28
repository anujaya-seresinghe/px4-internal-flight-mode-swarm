import { defineConfig } from 'vite';
import react from '@vitejs/plugin-react';

export default defineConfig({
  plugins: [react()],
  server: {
    host: process.env.HOST || '127.0.0.1',
    port: 3000,
    strictPort: true,
    watch: process.env.CHOKIDAR_USEPOLLING ? { usePolling: true } : undefined,
  },
  build: {
    chunkSizeWarningLimit: 2000,
  },
});
