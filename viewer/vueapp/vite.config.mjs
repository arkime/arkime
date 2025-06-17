// NOTE: vueapp/build/** & vueapp/config/** currently unused - we may want to add back in check-versions.js?

import { fileURLToPath } from 'node:url';
import inject from '@rollup/plugin-inject';

import path from 'path';
import { defineConfig } from 'vite';
import vue from '@vitejs/plugin-vue';
import Components from 'unplugin-vue-components/vite'
import { BootstrapVueNextResolver } from 'bootstrap-vue-next'

import { git } from '../common/git'; // NOTE: modified copy of global-common git.js

// https://vitejs.dev/config/
export default defineConfig({
  server: {
    port: 5173,
    strictPort: true, // fail if port is already in use
  },
  root: fileURLToPath(new URL('../../', import.meta.url)), // routing back to top-level allows us to use files from the other directories (eg. top-level common)
  define: {
    BUILD_VERSION: JSON.stringify(git('describe --tags')),
    BUILD_DATE: JSON.stringify(git('log -1 --format=%aI'))
  },
  plugins: [
    vue({}),
    inject({ // jquery must be first
      $: 'jquery',
      jQuery: 'jquery'
    }),
    Components({
      resolvers: [BootstrapVueNextResolver()],
    })
  ],
  resolve: {
    alias: {
      '@common': fileURLToPath(new URL('../common/vueapp', import.meta.url)),
      '@real_common': fileURLToPath(new URL('../../common/vueapp', import.meta.url)),
      '@': fileURLToPath(new URL('./src', import.meta.url)),
      'public': fileURLToPath(new URL('../public', import.meta.url)),
      vue: fileURLToPath(new URL('../node_modules/vue/dist/vue.esm-bundler.js', import.meta.url))
    }
  },
  build: {
    sourcemap: true, // do we want sourcemap for production builds?
    outDir: './viewer/vueapp/dist',
    manifest: true,
    rollupOptions: {
      input: path.resolve(__dirname, 'src/main.js')
    }
  },
  logLevel: 'warn',
  compilerOptions: {
    whitespace: 'preserve'
  }
});
