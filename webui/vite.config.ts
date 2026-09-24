import { defineConfig, type Plugin } from 'vite';
import vue from '@vitejs/plugin-vue';
import { resolve } from 'node:path';

const env = (name: string, fallback: string): string =>
  process.env[name] || fallback;

const ksuPreviewMock = (): Plugin => ({
  name: 'znn-ksu-preview-mock',
  apply: 'serve',
  transformIndexHtml: {
    order: 'pre',
    handler: (html) => ({
      html,
      tags: [
        {
          tag: 'script',
          attrs: { src: '/dev/ksu-mock.js' },
          injectTo: 'head',
        },
      ],
    }),
  },
});

export default defineConfig(() => ({
  base: './',
  plugins: [
    vue({
      template: {
        compilerOptions: {
          isCustomElement: (tag) => tag.startsWith('m3e-'),
        },
      },
    }),
    ksuPreviewMock(),
  ],
  define: {
    __ZNN_MODULE_ID__: JSON.stringify(env('ZNN_MODULE_ID', 'zygisknextsu')),
    __ZNN_MODULE_NAME__: JSON.stringify(env('ZNN_MODULE_NAME', 'Zygisk Next Next')),
    __ZNN_VER_NAME__: JSON.stringify(env('ZNN_VER_NAME', 'v0.0.0')),
    __ZNN_COMMIT_HASH__: JSON.stringify(env('ZNN_COMMIT_HASH', 'unknown')),
  },
  server: {
    open: true,
  },
  build: {
    outDir: resolve(__dirname, '../module/webroot'),
    emptyOutDir: true,
    target: 'es2020',
    sourcemap: false,
  },
}));
