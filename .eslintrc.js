// https://eslint.org/docs/user-guide/configuring

module.exports = {
  root: true,
  parserOptions: {
    requireConfigFile: false,
    parser: '@babel/eslint-parser'
  },
  env: {
    browser: true,
    jquery: true,
    jest: true
  },
  extends: [
    // https://github.com/vuejs/eslint-plugin-vue#priority-a-essential-error-prevention
    // consider switching to `plugin:vue/strongly-recommended` or `plugin:vue/recommended` for stricter rules.
    // 'plugin:vue/essential',
    // https://github.com/standard/standard/blob/master/docs/RULES-en.md
    'standard',
    'plugin:jest/recommended'
  ],
  // required to lint *.vue files
  plugins: [
    'vue',
    'jest'
  ],
  // add your custom rules here
  rules: {
    // allow async-await
    'generator-star-spacing': 'off',
    // allow debugger during development
    'no-debugger': process.env.NODE_ENV === 'production' ? 'error' : 'off',
    'semi': ['error', 'always'],
    'handle-callback-err': ['error', 'never'],
    'prefer-promise-reject-errors': 0,
    'standard/no-callback-literal': 'off',
    'no-labels': ['error', { 'allowLoop': true }],
    'no-new-func': 'off',
    'indent': ['error', 2, {'SwitchCase': 0}],
    'no-useless-return': 'off',
    'n/no-callback-literal': 'off',
    'n/handle-callback-err': 'off',
    'no-case-declarations': 'off',
    'no-empty': 'off',
    'default-case-last': 'off',
    'no-shadow': ['error', { 'builtinGlobals': true, 'hoist': 'all', 'allow': ['err', 'req', 'res', 'stop', 'self'] }],
    'jest/no-conditional-expect': 'off',
    'jest/expect-expect': [ 'error', { 'assertFunctionNames': ['expect', 'getAllByText', 'getByText', 'getByPlaceholderText', 'getByTitle'] }],
    'quotes': ['error', 'single', { 'avoidEscape': true, 'allowTemplateLiterals': true }],
  },
  // used to split up Vue 2 vs Vue 3 linting
  overrides: [
    // Vue 2 linting -----------------------
    {
      files: ['parliament/**/*.vue', 'wiseService/**/*.vue', 'common/**/*.vue'],
      extends: [
        // consider switching to `plugin:vue/vue3-strongly-recommended` or `plugin:vue/vue3-recommended` for stricter rules.
        'plugin:vue/essential'
      ],
      rules: {
        'vue/multi-word-component-names': 'off'
      }
    },
    // Vue 3 linting -----------------------
    {
      files: ['viewer/**/*.vue', 'cont3xt/**/*.vue'],
      extends: [
        // consider switching to `plugin:vue/vue3-strongly-recommended` or `plugin:vue/vue3-recommended` for stricter rules.
        'plugin:vue/vue3-essential'
      ],
      rules: {
        'vue/multi-word-component-names': 'off',
        // allow modifiers for v-slots like `#item.buttons`, since this style is used by Vuetify's slots (eg. datatables)
        'vue/valid-v-slot': ['error', { allowModifiers: true }]
        // I believe there is a way to fix the 'defineEmits/defineProps/defineModel' is not imported error, since they are compiler builtins
        // but I was not able to get it to work (possibly having to do with 'vue/setup-compiler-macros', depending on the version?)
      }
    }
  ]
};
