// TODO: toby, do i need URL?
import { fileURLToPath, URL } from 'node:url';

import { defineConfig } from 'vite';
import vue from '@vitejs/plugin-vue';
import commonjs from '@rollup/plugin-commonjs';

// https://vitejs.dev/config/
export default defineConfig({
  plugins: [
    vue({
      template: {
        compilerOptions: {
          compatConfig: {
            MODE: 2
          }
        }
      }
    }),
    commonjs()
  ],
  resolve: {
    alias: {
      '@common': fileURLToPath(new URL('../../common2/vueapp', import.meta.url)),
      '@': fileURLToPath(new URL('./src', import.meta.url)),
      vue: '@vue/compat'
    }
  },
  // optimizeDeps: {
  //   // include: [fileURLToPath(new URL('../', import.meta.url)), '../src/']
  //   include: ['*.cjs']
  // },
  build: {
    sourcemap: true
    // commonjsOptions: {
    //   transformMixedEsModules: true,
    //   include: ['*.cjs']
    // }
  }
  // commonjsOptions: {
  //   esmExternals: true
  // }
  // TODO: toby-rm ? add this back? not sure how we want to architect exactly...?
  // , build: {
  //   manifest: true,
  //   rollupOptions: {
  //     input: './src/mainn.ts'
  //   }
  // }
});
