import { fileURLToPath } from 'node:url';

/// <reference types="vitest" />
import { defineConfig } from 'vite';
import vue from '@vitejs/plugin-vue';
import { git } from '../../common2/badcopy/git';

// https://vitejs.dev/config/
export default defineConfig({
  root: fileURLToPath(new URL('../../', import.meta.url)),
  define: {
    BUILD_VERSION: JSON.stringify(git('describe --tags')),
    BUILD_DATE: JSON.stringify(git('log -1 --format=%aI'))
  },
  plugins: [
    vue({
      template: {
        compilerOptions: {
          compatConfig: {
            MODE: 2
          }
        }
      }
    })
  ],
  resolve: {
    alias: {
      '@common': fileURLToPath(new URL('../../common2/vueapp', import.meta.url)),
      '@': fileURLToPath(new URL('./src', import.meta.url)),
      vue: '@vue/compat'
    }
  },
  build: {
    sourcemap: true, // TODO: do we want sourcemap?
    outDir: './cont3xt/vueapp/dist',
    manifest: true,
    rollupOptions: {
      input: './src/mainn.js'
    }
  },
  // vitest config
  test: {
    // exclude: ['**/tests/**']
    include: ['cont3xt/vueapp/temp_tests/**'],
    setupFiles: ['cont3xt/vueapp/vitest-setup.js'],
    globals: true, // TODO: toby ??? - this said to enable, but not sure if that applies here (https://testing-library.com/docs/vue-testing-library/setup)
    environment: 'jsdom'
  }
});
