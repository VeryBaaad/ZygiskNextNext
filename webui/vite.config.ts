import { defineConfig } from 'vite';
import { resolve } from 'node:path';

const env = (name: string, fallback: string): string =>
  process.env[name] || fallback;

export default defineConfig(() => ({
  base: './',
  define: {
    __ZNN_MODULE_ID__: JSON.stringify(env('ZNN_MODULE_ID', 'zygisknextsu')),
    __ZNN_MODULE_NAME__: JSON.stringify(env('ZNN_MODULE_NAME', 'Zygisk Next Next')),
    __ZNN_VER_NAME__: JSON.stringify(env('ZNN_VER_NAME', 'v1-0.1.0')),
    __ZNN_COMMIT_HASH__: JSON.stringify(env('ZNN_COMMIT_HASH', 'unknown')),
  },
  build: {
    outDir: resolve(__dirname, '../module/webroot'),
    emptyOutDir: true,
    target: 'es2020',
    sourcemap: false,
  },
}));
