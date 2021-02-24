// https://eslint.org/docs/user-guide/configuring

module.exports = {
  root: true,
  parserOptions: {
    parser: '@babel/eslint-parser',
    requireConfigFile: false,
    babelOptions: {
      plugins: [
        "@babel/plugin-proposal-class-properties"
      ]
    }
  },
  env: {
    browser: true,
    jquery: true
  },
  extends: [
    // https://github.com/vuejs/eslint-plugin-vue#priority-a-essential-error-prevention
    // consider switching to `plugin:vue/strongly-recommended` or `plugin:vue/recommended` for stricter rules.
    'plugin:vue/essential',
    // https://github.com/standard/standard/blob/master/docs/RULES-en.md
    'standard'
  ],
  // required to lint *.vue files
  plugins: [
    'vue'
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
    'node/no-callback-literal': 'off',
    'node/handle-callback-err': 'off',
    'no-case-declarations': 'off',
    'no-empty': 'off',
    'default-case-last': 'off'
  }
}
