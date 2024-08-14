// NOTE: vueapp/build/ currently unused - we may want to add back in check-versions.js?

import { fileURLToPath } from 'node:url';

/// <reference types="vitest" />
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
      styles: {
        configFile: 'cont3xt/vueapp/src/settings.scss'
      }
    })
  ],
  resolve: {
    alias: {
      '@common': fileURLToPath(new URL('../common/vueapp', import.meta.url)),
      '@real_common': fileURLToPath(new URL('../../common', import.meta.url)),
      '@': fileURLToPath(new URL('./src', import.meta.url)),
      vue: fileURLToPath(new URL('../node_modules/vue', import.meta.url))
    }
  },
  build: {
    sourcemap: true, // TODO: do we want sourcemap?
    outDir: './cont3xt/vueapp/dist',
    manifest: true,
    rollupOptions: {
      input: './src/main.js'
    }
  }
  // vitest config
  // test: {
  //   // exclude: ['**/tests/**']
  //   include: ['cont3xt/vueapp/temp_tests/**'],
  //   // include: ['cont3xt/vueapp/tests/**'],
  //   setupFiles: ['cont3xt/vueapp/vitest-setup.js'],
  //   globals: true, // TODO: toby ??? - this said to enable, but not sure if that applies here (https://testing-library.com/docs/vue-testing-library/setup)
  //   environment: 'jsdom'
  // }
});
