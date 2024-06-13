// TODO: toby, do i need URL?
import { fileURLToPath, URL } from 'node:url';

import { defineConfig } from 'vite';
import vue from '@vitejs/plugin-vue';
import { git } from '../../common2/badcopy/git';

// TODO: toby, test git stuff

// https://vitejs.dev/config/
export default defineConfig({
  root: fileURLToPath(new URL('../../', import.meta.url)),
  // assetsInclude: ['**/*.gif'],
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
  // optimizeDeps: {
  //   // include: [fileURLToPath(new URL('../', import.meta.url)), '../src/']
  //   include: ['*.cjs']
  // },
  build: {
    outDir: './cont3xt/vueapp/dist'
    // sourcemap: true
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
