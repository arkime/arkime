// NOTE: vueapp/build/** & vueapp/config/** currently unused - we may want to add back in check-versions.js?

import { fileURLToPath } from 'node:url';

/// <reference types="vitest" />
import path from 'path';
import { defineConfig } from 'vite';
import vue from '@vitejs/plugin-vue';
import { git } from '../common/git'; // NOTE: modified copy of global-common git.js
import Vuetify from 'vite-plugin-vuetify';

// https://vitejs.dev/config/
export default defineConfig({
  root: fileURLToPath(new URL('../../', import.meta.url)), // routing back to top-level allows us to use files from the other directories (eg. top-level common)
  define: {
    BUILD_VERSION: JSON.stringify(git('describe --tags')),
    BUILD_DATE: JSON.stringify(git('log -1 --format=%aI'))
  },
  plugins: [
    vue({}),
    Vuetify({
      treeShake: true,
      styles: {
        configFile: 'cont3xt/vueapp/src/vuetify-settings.scss'
      }
    })
  ],
  resolve: {
    alias: {
      '@common': fileURLToPath(new URL('../common/vueapp', import.meta.url)),
      '@real_common': fileURLToPath(new URL('../../common/vueapp', import.meta.url)),
      '@': fileURLToPath(new URL('./src', import.meta.url)),
      vue: fileURLToPath(new URL('../node_modules/vue', import.meta.url))
    }
  },
  build: {
    sourcemap: true, // do we want sourcemap for production builds?
    outDir: './cont3xt/vueapp/dist',
    manifest: true,
    rollupOptions: {
      input: path.resolve(__dirname, 'src/main.js'),
    },
  },
  logLevel: 'warn'
  // ---- attempted WIP vitest setup (very broken) ----
  // vitest config
  // test: {
  //   include: ['cont3xt/vueapp/tests/**'],
  //   setupFiles: ['cont3xt/vueapp/vitest-setup.js'],
  //   globals: true, // not sure if globals is necessary...? (https://testing-library.com/docs/vue-testing-library/setup)
  //   environment: 'jsdom'
  // }
});
