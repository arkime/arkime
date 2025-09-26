const {
    defineConfig,
    globalIgnores,
} = require("eslint/config");

const globals = require("globals");
const vue = require("eslint-plugin-vue");
const js = require("@eslint/js");
const vueParser = require('vue-eslint-parser');
const babelParser = require('@babel/eslint-parser');

const {
    FlatCompat,
} = require("@eslint/eslintrc");

const compat = new FlatCompat({
    baseDirectory: __dirname,
    recommendedConfig: js.configs.recommended,
    allConfig: js.configs.all
});

// Shared JavaScript syntax rules for both JS and Vue files
const commonJavaScriptRules = {
    "generator-star-spacing": "off",
    "no-debugger": process.env.NODE_ENV === "production" ? "error" : "off",
    "semi": ["error", "always"],
    "handle-callback-err": ["error", "never"],
    "prefer-promise-reject-errors": 0,
    "standard/no-callback-literal": "off",

    "no-labels": ["error", {
        "allowLoop": true,
    }],

    "no-new-func": "off",

    "indent": ["error", 2, {
        "SwitchCase": 0,
    }],

    "no-useless-return": "off",
    "n/no-callback-literal": "off",
    "n/handle-callback-err": "off",
    "no-case-declarations": "off",
    "no-empty": "off",
    "default-case-last": "off",

    "no-shadow": ["error", {
        "builtinGlobals": true,
        "hoist": "all",
        "allow": ["err", "req", "res", "stop", "self"],
    }],

    "quotes": ["error", "single", {
        "avoidEscape": true,
        "allowTemplateLiterals": true,
    }],

    "no-multiple-empty-lines": ["error", {
        "max": 1,
        "maxEOF": 0,
        "maxBOF": 0,
    }],
};

// Vue-specific rule overrides (applied after extending recommended configs)
const vueRuleOverrides = {
    "vue/multi-word-component-names": "off",
    "vue/valid-v-slot": ["error", {
        allowModifiers: true,
    }],
};

module.exports = defineConfig([
    // Configuration for JavaScript files
    {
        files: ["**/*.js"],
        languageOptions: {
            parser: babelParser,
            parserOptions: {
                requireConfigFile: false,
            },
            globals: {
                ...globals.browser,
                ...globals.jquery,
            },
        },
        rules: {
            ...commonJavaScriptRules,
        },
    },
    // Configuration for Vue files using predefined configs
    ...compat.extends("plugin:vue/strongly-recommended"),
    {
        files: ["**/*.vue"],
        languageOptions: {
            parser: vueParser,
            parserOptions: {
                requireConfigFile: false,
                parser: babelParser,
            },
            globals: {
                ...globals.browser,
                ...globals.jquery,
            },
        },
        rules: {
            ...commonJavaScriptRules,
            ...vueRuleOverrides,
        },
    },
    globalIgnores([
    "parliament/vueapp/dist/",
    "parliament/node_modules",
    "wiseService/vueapp/dist/",
    "wiseService/node_modules",
    "wiseService/sprintf.js",
    "viewer/vueapp/dist/",
    "viewer/arkimeparser.js",
    "viewer/node_modules",
    "viewer/public",
    "common/version.js",
    "cont3xt/vueapp/dist/",
    "cont3xt/node_modules",
])]);
