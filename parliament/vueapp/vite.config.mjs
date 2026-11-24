// NOTE: vueapp/build/** & vueapp/config/** currently unused - we may want to add back in check-versions.js?

import { fileURLToPath } from 'node:url';

import path from 'path';
import { defineConfig } from 'vite';
import vue from '@vitejs/plugin-vue';
import Vuetify from 'vite-plugin-vuetify';

import { git } from '../../common/git.mjs';
import thirdPartyLicenses from '../../common/vite-plugin-third-party-licenses.mjs';

// https://vitejs.dev/config/
export default defineConfig({
  server: {
    port: 5175,
    strictPort: true // fail if port is already in use
  },
  root: fileURLToPath(new URL('../../', import.meta.url)), // routing back to top-level allows us to use files from the other directories (eg. top-level common)
  define: {
    BUILD_VERSION: JSON.stringify(git('describe --tags')),
    BUILD_DATE: JSON.stringify(git('log -1 --format=%aI'))
  },
  plugins: [
    vue({}),
    Vuetify({ treeShake: true }),
    thirdPartyLicenses({ file: fileURLToPath(new URL('./third-party-licenses.json', import.meta.url)) })
  ],
  resolve: {
    alias: {
      '@common': fileURLToPath(new URL('../../common/vueapp', import.meta.url)),
      '@': fileURLToPath(new URL('./src', import.meta.url)),
      'public': fileURLToPath(new URL('../public', import.meta.url)),
      vue: fileURLToPath(new URL('../../node_modules/vue/dist/vue.esm-bundler.js', import.meta.url))
    }
  },
  build: {
    modulePreload: false, // preload helper ignores <base href>, 404s under webBasePath
    outDir: './parliament/vueapp/dist',
    manifest: true,
    rolldownOptions: {
      input: path.resolve(import.meta.dirname, 'src/main.js')
    }
  },
  logLevel: 'warn',
  compilerOptions: {
    whitespace: 'preserve'
  }
});
