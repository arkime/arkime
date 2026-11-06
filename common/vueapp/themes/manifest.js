/*
Copyright Yahoo Inc.
SPDX-License-Identifier: Apache-2.0
*/

/**
 * Theme manifest -- single source of truth for all 10 baked-in themes
 * across viewer, cont3xt (and later parliament + wise when their
 * bootstrap rip lands).
 *
 * Each theme entry has the shape Vuetify 3's createVuetify({ theme })
 * expects:
 *   { id, name, dark: boolean, colors: { <vuetify-key>: '#hex' } }
 *
 * Color-key naming conventions (kebab-case so Vuetify emits
 * --v-theme-{key}: r,g,b verbatim):
 *
 *  Vuetify standards (all v-components inherit these):
 *    primary, secondary, success, warning, error, info,
 *    background, surface,
 *    on-background, on-surface, on-primary, on-secondary,
 *    on-success, on-warning, on-error, on-info
 *
 *  Legacy 4-family aliases (one-to-one alias so existing markup
 *  using rgb(var(--v-theme-tertiary)) and rgb(var(--v-theme-quaternary))
 *  paints with the same value as success/info respectively):
 *    tertiary{,-light,-lighter,-lightest,-dark,-darker}    -> alias of success*
 *    quaternary{,-light,-lighter,-lightest,-dark,-darker}  -> alias of info*
 *    primary{-light,-lighter,-lightest,-dark,-darker}      -- shade variants
 *    secondary{-light,-lighter,-lightest,-dark,-darker}    -- shade variants
 *
 *  Neutral ramp (replaces --color-gray* and --color-foreground*):
 *    foreground, foreground-accent
 *    neutral, neutral-{dark,darker,light,lighter,lightest}
 *    outline   -- input/card border (was --color-gray)
 *    button-fg -- text-on-button color (was --color-button)
 *
 *  Semantic surfaces (generalized from cont3xt bespoke tokens):
 *    surface-card, surface-card-hover, surface-card-border
 *    surface-well, surface-well-border
 *    surface-panel, surface-sidebar
 *    input-bg, input-bg-disabled, input-border
 *    progress-track
 *
 *  Visualization (kept as distinct tokens; not Vuetify-themed chrome):
 *    water, land, land-dark, land-light, src, dst
 *
 * Values are ported verbatim from the legacy
 * viewer/vueapp/src/themes/*.css files where present. Semantic
 * surfaces and warning/error tokens that the legacy CSS didn't define
 * are filled with sensible defaults derived from the theme's neutral
 * ramp and per-theme accent.
 *
 * See viewer/vueapp/src/themes/*.css (now deprecated) for historical
 * provenance of the values. See colorShades.js for the algorithmic
 * lighten/darken used by the customTheme.js legacy-format migration.
 */

/* ---- shared default warning/error colors ----------------------------- */
/* Most legacy themes never specified warning/error; we apply a sane
   default pair per light/dark mode. Individual theme entries can
   override. */
const LIGHT_WARNING = '#D97706';
const LIGHT_ERROR = '#DC2626';
const DARK_WARNING = '#FBBF24';
const DARK_ERROR = '#F87171';

/* ---- arkime-light ---------------------------------------------------- */
const arkimeLight = {
  id: 'arkime-light',
  name: 'Arkime Light',
  dark: false,
  colors: {
    background: '#FFFFFF',
    surface: '#FFFFFF',
    foreground: '#303030',
    'foreground-accent': '#004C83',
    'on-background': '#303030',
    'on-surface': '#303030',
    'on-primary': '#FFFFFF',
    'on-secondary': '#FFFFFF',
    'on-success': '#FFFFFF',
    'on-info': '#FFFFFF',
    'on-warning': '#FFFFFF',
    'on-error': '#FFFFFF',
    'button-fg': '#FFFFFF',
    outline: '#CCCCCC',
    white: '#FFFFFF',
    black: '#333333',

    // neutral ramp (mapped from --color-gray family)
    neutral: '#CCCCCC',
    'neutral-dark': '#777777',
    'neutral-darker': '#555555',
    'neutral-light': '#EEEEEE',
    'neutral-lighter': '#F6F6F6',
    'neutral-lightest': '#FAFAFA',

    // primary family
    primary: '#303030',
    'primary-dark': '#212121',
    'primary-darker': '#111111',
    'primary-light': '#3F3F3F',
    'primary-lighter': '#4F4F4F',
    'primary-lightest': '#EAEAEA',

    // secondary family
    secondary: '#004C83',
    'secondary-dark': '#003A64',
    'secondary-darker': '#002846',
    'secondary-light': '#005EA2',
    'secondary-lighter': '#0070C0',
    'secondary-lightest': '#A4C2D6',

    // success (= tertiary alias)
    success: '#66B689',
    'success-dark': '#52AC79',
    'success-darker': '#48976A',
    'success-light': '#7BC099',
    'success-lighter': '#8FCAA9',
    'success-lightest': '#E6F3EB',
    tertiary: '#66B689',
    'tertiary-dark': '#52AC79',
    'tertiary-darker': '#48976A',
    'tertiary-light': '#7BC099',
    'tertiary-lighter': '#8FCAA9',
    'tertiary-lightest': '#E6F3EB',

    // info (= quaternary alias)
    info: '#2F7D86',
    'info-dark': '#27686F',
    'info-darker': '#1F5359',
    'info-light': '#37929D',
    'info-lighter': '#3FA7B3',
    'info-lightest': '#E9F3F3',
    quaternary: '#2F7D86',
    'quaternary-dark': '#27686F',
    'quaternary-darker': '#1F5359',
    'quaternary-light': '#37929D',
    'quaternary-lighter': '#3FA7B3',
    'quaternary-lightest': '#E9F3F3',

    warning: LIGHT_WARNING,
    error: LIGHT_ERROR,

    // semantic surfaces (sensible defaults for a light theme)
    'surface-card': '#FAFAFA',
    'surface-card-hover': '#EEEEEE',
    'surface-card-border': '#E0E0E0',
    'surface-well': '#F6F6F6',
    'surface-well-border': '#EEEEEE',
    'surface-panel': '#FFFFFF',
    'surface-sidebar': '#F6F6F6',
    'input-bg': '#FFFFFF',
    'input-bg-disabled': '#EEEEEE',
    'input-border': '#CCCCCC',
    'progress-track': '#EEEEEE',

    // visualization
    water: '#BCD0E0',
    land: '#66B689',
    'land-dark': '#48976A',
    'land-light': '#8FCAA9',
    src: '#C43D75',
    dst: '#3D7B7E'
  }
};

/* ---- arkime-dark ---------------------------------------------------- */
const arkimeDark = {
  id: 'arkime-dark',
  name: 'Arkime Dark',
  dark: true,
  colors: {
    background: '#303030',
    surface: '#3A3A3A',
    foreground: '#FFFFFF',
    'foreground-accent': '#D1E9DC',
    'on-background': '#FFFFFF',
    'on-surface': '#FFFFFF',
    'on-primary': '#333333',
    'on-secondary': '#333333',
    'on-success': '#333333',
    'on-info': '#333333',
    'on-warning': '#333333',
    'on-error': '#333333',
    'button-fg': '#333333',
    white: '#333333',
    black: '#FFFFFF',
    outline: '#CCCCCC',

    neutral: '#CCCCCC',
    'neutral-dark': '#F6F6F6',
    'neutral-darker': '#EEEEEE',
    'neutral-light': '#777777',
    'neutral-lighter': '#555555',
    'neutral-lightest': '#444444',

    primary: '#ADADAD',
    'primary-dark': '#9E9E9E',
    'primary-darker': '#8E8E8E',
    'primary-light': '#BCBCBC',
    'primary-lighter': '#CCCCCC',
    'primary-lightest': '#303030',

    secondary: '#80A9C7',
    'secondary-dark': '#6B9BBE',
    'secondary-darker': '#568CB4',
    'secondary-light': '#95B7D0',
    'secondary-lighter': '#AAC6DA',
    'secondary-lightest': '#003B66',

    success: '#66B689',
    'success-dark': '#52AC79',
    'success-darker': '#48976A',
    'success-light': '#7BC099',
    'success-lighter': '#8FCAA9',
    'success-lightest': '#237543',
    tertiary: '#66B689',
    'tertiary-dark': '#52AC79',
    'tertiary-darker': '#48976A',
    'tertiary-light': '#7BC099',
    'tertiary-lighter': '#8FCAA9',
    'tertiary-lightest': '#237543',

    info: '#7EC3CC',
    'info-dark': '#68B9C3',
    'info-darker': '#52AFBB',
    'info-light': '#94CDD5',
    'info-lighter': '#AAD7DD',
    'info-lightest': '#1A464B',
    quaternary: '#7EC3CC',
    'quaternary-dark': '#68B9C3',
    'quaternary-darker': '#52AFBB',
    'quaternary-light': '#94CDD5',
    'quaternary-lighter': '#AAD7DD',
    'quaternary-lightest': '#1A464B',

    warning: DARK_WARNING,
    error: DARK_ERROR,

    'surface-card': '#3A3A3A',
    'surface-card-hover': '#444444',
    'surface-card-border': '#4A4A4A',
    'surface-well': '#3A3A3A',
    'surface-well-border': '#444444',
    'surface-panel': '#3A3A3A',
    'surface-sidebar': '#282828',
    'input-bg': '#222222',
    'input-bg-disabled': '#1A1A1A',
    'input-border': '#555555',
    'progress-track': '#444444',

    water: '#7EC3CC',
    land: '#66B689',
    'land-dark': '#48976A',
    'land-light': '#8FCAA9',
    src: '#FF70B8',
    dst: '#B09AFD'
  }
};

/* ---- purp ----------------------------------------------------------- */
const purp = {
  id: 'purp',
  name: 'Purp-Purp',
  dark: false,
  colors: {
    background: '#FFFFFF',
    surface: '#FFFFFF',
    foreground: '#303030',
    'foreground-accent': '#76207d',
    'on-background': '#303030',
    'on-surface': '#303030',
    'on-primary': '#FFFFFF',
    'on-secondary': '#FFFFFF',
    'on-success': '#FFFFFF',
    'on-info': '#000000',
    'on-warning': '#000000',
    'on-error': '#FFFFFF',
    'button-fg': '#FFFFFF',
    white: '#FFFFFF',
    black: '#000000',
    outline: '#CCCCCC',

    neutral: '#CCCCCC',
    'neutral-dark': '#777777',
    'neutral-darker': '#555555',
    'neutral-light': '#EEEEEE',
    'neutral-lighter': '#F6F6F6',
    'neutral-lightest': '#FAFAFA',

    primary: '#830B9C',
    'primary-dark': '#530763',
    'primary-darker': '#360540',
    'primary-light': '#9D0DBA',
    'primary-lighter': '#B52BD5',
    'primary-lightest': '#ECE4FF',

    secondary: '#1F1FA5',
    'secondary-dark': '#1A1A87',
    'secondary-darker': '#141469',
    'secondary-light': '#2323BE',
    'secondary-lighter': '#2A2ADC',
    'secondary-lightest': '#EDFCFF',

    success: '#079B72',
    'success-dark': '#077D5C',
    'success-darker': '#07694A',
    'success-light': '#07B98C',
    'success-lighter': '#0AD29A',
    'success-lightest': '#CCEEE4',
    tertiary: '#079B72',
    'tertiary-dark': '#077D5C',
    'tertiary-darker': '#07694A',
    'tertiary-light': '#07B98C',
    'tertiary-lighter': '#0AD29A',
    'tertiary-lightest': '#CCEEE4',

    info: '#ECB30A',
    'info-dark': '#CD9A09',
    'info-darker': '#B98609',
    'info-light': '#FFC30A',
    'info-lighter': '#FFE480',
    'info-lightest': '#FFF7E5',
    quaternary: '#ECB30A',
    'quaternary-dark': '#CD9A09',
    'quaternary-darker': '#B98609',
    'quaternary-light': '#FFC30A',
    'quaternary-lighter': '#FFE480',
    'quaternary-lightest': '#FFF7E5',

    warning: LIGHT_WARNING,
    error: LIGHT_ERROR,

    'surface-card': '#FAFAFA',
    'surface-card-hover': '#EEEEEE',
    'surface-card-border': '#E0E0E0',
    'surface-well': '#F6F6F6',
    'surface-well-border': '#EEEEEE',
    'surface-panel': '#FFFFFF',
    'surface-sidebar': '#F6F6F6',
    'input-bg': '#FFFFFF',
    'input-bg-disabled': '#EEEEEE',
    'input-border': '#CCCCCC',
    'progress-track': '#EEEEEE',

    water: '#C3E1E2',
    land: '#079B72',
    'land-dark': '#07694A',
    'land-light': '#0AD29A',
    src: '#C43D75',
    dst: '#3D7B7E'
  }
};

/* ---- blue ----------------------------------------------------------- */
const blue = {
  id: 'blue',
  name: 'Blue',
  dark: false,
  colors: {
    background: '#FFFFFF',
    surface: '#FFFFFF',
    foreground: '#303030',
    'foreground-accent': '#9A4E93',
    'on-background': '#303030',
    'on-surface': '#303030',
    'on-primary': '#FFFFFF',
    'on-secondary': '#FFFFFF',
    'on-success': '#FFFFFF',
    'on-info': '#000000',
    'on-warning': '#000000',
    'on-error': '#FFFFFF',
    'button-fg': '#FFFFFF',
    white: '#FFFFFF',
    black: '#000000',
    outline: '#CCCCCC',

    neutral: '#CCCCCC',
    'neutral-dark': '#777777',
    'neutral-darker': '#555555',
    'neutral-light': '#EEEEEE',
    'neutral-lighter': '#F6F6F6',
    'neutral-lightest': '#FAFAFA',

    primary: '#214B78',
    'primary-dark': '#163254',
    'primary-darker': '#000000',
    'primary-light': '#205B87',
    'primary-lighter': '#2C79B4',
    'primary-lightest': '#DCD9F3',

    secondary: '#3D7B7E',
    'secondary-dark': '#306264',
    'secondary-darker': '#1B4F50',
    'secondary-light': '#4d9599',
    'secondary-lighter': '#63BED3',
    'secondary-lightest': '#DBECE7',

    success: '#42B7C5',
    'success-dark': '#33919b',
    'success-darker': '#29757d',
    'success-light': '#7ACCD7',
    'success-lighter': '#87E6F3',
    'success-lightest': '#D3EEF1',
    tertiary: '#42B7C5',
    'tertiary-dark': '#33919b',
    'tertiary-darker': '#29757d',
    'tertiary-light': '#7ACCD7',
    'tertiary-lighter': '#87E6F3',
    'tertiary-lightest': '#D3EEF1',

    info: '#ECB30A',
    'info-dark': '#CD9A09',
    'info-darker': '#B98609',
    'info-light': '#FFC30A',
    'info-lighter': '#FFE480',
    'info-lightest': '#FFF7E5',
    quaternary: '#ECB30A',
    'quaternary-dark': '#CD9A09',
    'quaternary-darker': '#B98609',
    'quaternary-light': '#FFC30A',
    'quaternary-lighter': '#FFE480',
    'quaternary-lightest': '#FFF7E5',

    warning: LIGHT_WARNING,
    error: LIGHT_ERROR,

    'surface-card': '#FAFAFA',
    'surface-card-hover': '#EEEEEE',
    'surface-card-border': '#E0E0E0',
    'surface-well': '#F6F6F6',
    'surface-well-border': '#EEEEEE',
    'surface-panel': '#FFFFFF',
    'surface-sidebar': '#F6F6F6',
    'input-bg': '#FFFFFF',
    'input-bg-disabled': '#EEEEEE',
    'input-border': '#CCCCCC',
    'progress-track': '#EEEEEE',

    water: '#E1EBEB',
    land: '#42B7C5',
    'land-dark': '#29757d',
    'land-light': '#87E6F3',
    src: '#C43D75',
    dst: '#3D7B7E'
  }
};

/* ---- green ---------------------------------------------------------- */
const green = {
  id: 'green',
  name: 'Green',
  dark: false,
  colors: {
    background: '#FFFFFF',
    surface: '#FFFFFF',
    foreground: '#303030',
    'foreground-accent': '#38738d',
    'on-background': '#303030',
    'on-surface': '#303030',
    'on-primary': '#FFFFFF',
    'on-secondary': '#FFFFFF',
    'on-success': '#000000',
    'on-info': '#000000',
    'on-warning': '#000000',
    'on-error': '#FFFFFF',
    'button-fg': '#FFFFFF',
    white: '#FFFFFF',
    black: '#000000',
    outline: '#CCCCCC',

    neutral: '#CCCCCC',
    'neutral-dark': '#777777',
    'neutral-darker': '#555555',
    'neutral-light': '#EEEEEE',
    'neutral-lighter': '#F6F6F6',
    'neutral-lightest': '#FAFAFA',

    primary: '#2A7847',
    'primary-dark': '#2A6E3d',
    'primary-darker': '#235A32',
    'primary-light': '#308C55',
    'primary-lighter': '#37A05f',
    'primary-lightest': '#C1DBCA',

    secondary: '#3D7B7E',
    'secondary-dark': '#306264',
    'secondary-darker': '#1B4F50',
    'secondary-light': '#4d9599',
    'secondary-lighter': '#63BED3',
    'secondary-lightest': '#DBECE7',

    success: '#91C563',
    'success-dark': '#7EAA57',
    'success-darker': '#759B54',
    'success-light': '#A0D96E',
    'success-lighter': '#AAE37C',
    'success-lightest': '#E4F0d2',
    tertiary: '#91C563',
    'tertiary-dark': '#7EAA57',
    'tertiary-darker': '#759B54',
    'tertiary-light': '#A0D96E',
    'tertiary-lighter': '#AAE37C',
    'tertiary-lightest': '#E4F0d2',

    info: '#BECF14',
    'info-dark': '#ADBC12',
    'info-darker': '#9CAA10',
    'info-light': '#CDDE15',
    'info-lighter': '#DEF015',
    'info-lightest': '#FDFFE2',
    quaternary: '#BECF14',
    'quaternary-dark': '#ADBC12',
    'quaternary-darker': '#9CAA10',
    'quaternary-light': '#CDDE15',
    'quaternary-lighter': '#DEF015',
    'quaternary-lightest': '#FDFFE2',

    warning: LIGHT_WARNING,
    error: LIGHT_ERROR,

    'surface-card': '#FAFAFA',
    'surface-card-hover': '#EEEEEE',
    'surface-card-border': '#E0E0E0',
    'surface-well': '#F6F6F6',
    'surface-well-border': '#EEEEEE',
    'surface-panel': '#FFFFFF',
    'surface-sidebar': '#F6F6F6',
    'input-bg': '#FFFFFF',
    'input-bg-disabled': '#EEEEEE',
    'input-border': '#CCCCCC',
    'progress-track': '#EEEEEE',

    water: '#E8F0F3',
    land: '#91C563',
    'land-dark': '#759B54',
    'land-light': '#AAE37C',
    src: '#C43D75',
    dst: '#3D7B7E'
  }
};

/* ---- cotton-candy --------------------------------------------------- */
const cottonCandy = {
  id: 'cotton-candy',
  name: 'Cotton Candy',
  dark: false,
  colors: {
    background: '#FFFFFF',
    surface: '#FFFFFF',
    foreground: '#303030',
    'foreground-accent': '#9A4E93',
    'on-background': '#303030',
    'on-surface': '#303030',
    'on-primary': '#FFFFFF',
    'on-secondary': '#FFFFFF',
    'on-success': '#FFFFFF',
    'on-info': '#000000',
    'on-warning': '#000000',
    'on-error': '#FFFFFF',
    'button-fg': '#FFFFFF',
    white: '#FFFFFF',
    black: '#000000',
    outline: '#CCCCCC',

    neutral: '#CCCCCC',
    'neutral-dark': '#777777',
    'neutral-darker': '#555555',
    'neutral-light': '#EEEEEE',
    'neutral-lighter': '#F6F6F6',
    'neutral-lightest': '#FAFAFA',

    primary: '#C43D75',
    'primary-dark': '#B0346D',
    'primary-darker': '#9B335A',
    'primary-light': '#E8488B',
    'primary-lighter': '#FF62A2',
    'primary-lightest': '#FFC2CF',

    secondary: '#3CAED2',
    'secondary-dark': '#389BBE',
    'secondary-darker': '#2B86A0',
    'secondary-light': '#3EC1E6',
    'secondary-lighter': '#67DDFF',
    'secondary-lightest': '#D7F1FF',

    success: '#079B72',
    'success-dark': '#077D5C',
    'success-darker': '#07694A',
    'success-light': '#07B98C',
    'success-lighter': '#08E6AD',
    'success-lightest': '#D5F3E7',
    tertiary: '#079B72',
    'tertiary-dark': '#077D5C',
    'tertiary-darker': '#07694A',
    'tertiary-light': '#07B98C',
    'tertiary-lighter': '#08E6AD',
    'tertiary-lightest': '#D5F3E7',

    info: '#F39C12',
    'info-dark': '#D78A10',
    'info-darker': '#AF770B',
    'info-light': '#FFB032',
    'info-lighter': '#FFCD70',
    'info-lightest': '#FFF8DD',
    quaternary: '#F39C12',
    'quaternary-dark': '#D78A10',
    'quaternary-darker': '#AF770B',
    'quaternary-light': '#FFB032',
    'quaternary-lighter': '#FFCD70',
    'quaternary-lightest': '#FFF8DD',

    warning: LIGHT_WARNING,
    error: LIGHT_ERROR,

    'surface-card': '#FAFAFA',
    'surface-card-hover': '#EEEEEE',
    'surface-card-border': '#E0E0E0',
    'surface-well': '#F6F6F6',
    'surface-well-border': '#EEEEEE',
    'surface-panel': '#FFFFFF',
    'surface-sidebar': '#F6F6F6',
    'input-bg': '#FFFFFF',
    'input-bg-disabled': '#EEEEEE',
    'input-border': '#CCCCCC',
    'progress-track': '#EEEEEE',

    water: '#E2DAE5',
    land: '#079B72',
    'land-dark': '#07694A',
    'land-light': '#08E6AD',
    src: '#C43D75',
    dst: '#3D7B7E'
  }
};

/* ---- dark-2 (green-on-black) --------------------------------------- */
const dark2 = {
  id: 'dark-2',
  name: 'Dark 2',
  dark: true,
  colors: {
    background: '#282828',
    surface: '#333333',
    foreground: '#33FF00',
    'foreground-accent': '#FFCC00',
    'on-background': '#33FF00',
    'on-surface': '#33FF00',
    'on-primary': '#EEEEEE',
    'on-secondary': '#000000',
    'on-success': '#000000',
    'on-info': '#FFFFFF',
    'on-warning': '#000000',
    'on-error': '#FFFFFF',
    'button-fg': '#EEEEEE',
    white: '#333333',
    black: '#EEEEEE',
    outline: '#AAAAAA',

    neutral: '#AAAAAA',
    'neutral-dark': '#CCCCCC',
    'neutral-darker': '#DDDDDD',
    'neutral-light': '#555555',
    'neutral-lighter': '#444444',
    'neutral-lightest': '#333333',

    primary: '#555555',
    'primary-dark': '#222222',
    'primary-darker': '#000000',
    'primary-light': '#444444',
    'primary-lighter': '#777777',
    'primary-lightest': '#000000',

    secondary: '#00BB20',
    'secondary-dark': '#009D1D',
    'secondary-darker': '#007F12',
    'secondary-light': '#00CF20',
    'secondary-lighter': '#00E326',
    'secondary-lightest': '#1e1e1e',

    success: '#22C55E',
    'success-dark': '#16A34A',
    'success-darker': '#15803D',
    'success-light': '#4ADE80',
    'success-lighter': '#86EFAC',
    'success-lightest': '#222222',
    tertiary: '#22C55E',
    'tertiary-dark': '#16A34A',
    'tertiary-darker': '#15803D',
    'tertiary-light': '#4ADE80',
    'tertiary-lighter': '#86EFAC',
    'tertiary-lightest': '#222222',

    info: '#400078',
    'info-dark': '#2f0058',
    'info-darker': '#22003f',
    'info-light': '#4a008b',
    'info-lighter': '#5a01a8',
    'info-lightest': '#000000',
    quaternary: '#400078',
    'quaternary-dark': '#2f0058',
    'quaternary-darker': '#22003f',
    'quaternary-light': '#4a008b',
    'quaternary-lighter': '#5a01a8',
    'quaternary-lightest': '#000000',

    warning: DARK_WARNING,
    error: DARK_ERROR,

    'surface-card': '#333333',
    'surface-card-hover': '#3F3F3F',
    'surface-card-border': '#444444',
    'surface-well': '#1e1e1e',
    'surface-well-border': '#333333',
    'surface-panel': '#333333',
    'surface-sidebar': '#1e1e1e',
    'input-bg': '#111111',
    'input-bg-disabled': '#0A0A0A',
    'input-border': '#555555',
    'progress-track': '#444444',

    water: '#333333',
    land: '#00A517',
    'land-dark': '#006911',
    'land-light': '#00E116',
    src: '#ff0073',
    dst: '#00ffb3'
  }
};

/* ---- dark-3 (solarized-leaning) ------------------------------------ */
const dark3 = {
  id: 'dark-3',
  name: 'Dark 3',
  dark: true,
  colors: {
    background: '#002833',
    surface: '#073642',
    foreground: '#ADC1C3',
    'foreground-accent': '#A6A8E2',
    'on-background': '#ADC1C3',
    'on-surface': '#ADC1C3',
    'on-primary': '#002833',
    'on-secondary': '#002833',
    'on-success': '#FFFFFF',
    'on-info': '#FFFFFF',
    'on-warning': '#000000',
    'on-error': '#FFFFFF',
    'button-fg': '#002833',
    white: '#333333',
    black: '#EEEEEE',
    outline: '#AAAAAA',

    neutral: '#AAAAAA',
    'neutral-dark': '#CCCCCC',
    'neutral-darker': '#DDDDDD',
    'neutral-light': '#555555',
    'neutral-lighter': '#444444',
    'neutral-lightest': '#333333',

    primary: '#666666',
    'primary-dark': '#7D7D7D',
    'primary-darker': '#999999',
    'primary-light': '#4F4F4F',
    'primary-lighter': '#424242',
    'primary-lightest': '#383838',

    secondary: '#2AA198',
    'secondary-dark': '#23837B',
    'secondary-darker': '#1B655F',
    'secondary-light': '#32BFB4',
    'secondary-lighter': '#38DDD1',
    'secondary-lightest': '#124B47',

    success: '#268BD2',
    'success-dark': '#1F76B4',
    'success-darker': '#1B6396',
    'success-light': '#2BA2F0',
    'success-lighter': '#4AB6FF',
    'success-lightest': '#154369',
    tertiary: '#268BD2',
    'tertiary-dark': '#1F76B4',
    'tertiary-darker': '#1B6396',
    'tertiary-light': '#2BA2F0',
    'tertiary-lighter': '#4AB6FF',
    'tertiary-lightest': '#154369',

    info: '#D33682',
    'info-dark': '#B42C72',
    'info-darker': '#96285C',
    'info-light': '#E73697',
    'info-lighter': '#F14195',
    'info-lightest': '#460C3A',
    quaternary: '#D33682',
    'quaternary-dark': '#B42C72',
    'quaternary-darker': '#96285C',
    'quaternary-light': '#E73697',
    'quaternary-lighter': '#F14195',
    'quaternary-lightest': '#460C3A',

    warning: DARK_WARNING,
    error: DARK_ERROR,

    'surface-card': '#073642',
    'surface-card-hover': '#0E3F4D',
    'surface-card-border': '#0F4858',
    'surface-well': '#002833',
    'surface-well-border': '#073642',
    'surface-panel': '#073642',
    'surface-sidebar': '#002833',
    'input-bg': '#222222',
    'input-bg-disabled': '#1A1A1A',
    'input-border': '#555555',
    'progress-track': '#444444',

    water: '#5F7C82',
    land: '#179B96',
    'land-dark': '#124B47',
    'land-light': '#32BFB4',
    src: '#F34545',
    dst: '#6C95E6'
  }
};

/* ---- v7-modern-light (new) ----------------------------------------- */
/* Warm-neutral background, saturated teal accent. Distinct from
   arkime-light's cool navy. */
const v7ModernLight = {
  id: 'v7-modern-light',
  name: 'V7 Modern Light',
  dark: false,
  colors: {
    background: '#FAFAF7',
    surface: '#FFFFFF',
    foreground: '#1F2326',
    'foreground-accent': '#00838F',
    'on-background': '#1F2326',
    'on-surface': '#1F2326',
    'on-primary': '#FFFFFF',
    'on-secondary': '#FFFFFF',
    'on-success': '#FFFFFF',
    'on-info': '#FFFFFF',
    'on-warning': '#FFFFFF',
    'on-error': '#FFFFFF',
    'button-fg': '#FFFFFF',
    white: '#FFFFFF',
    black: '#1F2326',
    outline: '#D4D4CE',

    neutral: '#D4D4CE',
    'neutral-dark': '#6E6E66',
    'neutral-darker': '#4A4A44',
    'neutral-light': '#E8E8E1',
    'neutral-lighter': '#F0F0E9',
    'neutral-lightest': '#F7F7F2',

    primary: '#00838F',
    'primary-dark': '#006E78',
    'primary-darker': '#005961',
    'primary-light': '#0AA6B3',
    'primary-lighter': '#26B7C3',
    'primary-lightest': '#D9F0F2',

    secondary: '#5D6970',
    'secondary-dark': '#4A555C',
    'secondary-darker': '#3A444A',
    'secondary-light': '#7A858B',
    'secondary-lighter': '#97A1A6',
    'secondary-lightest': '#E2E5E7',

    success: '#2E8B57',
    'success-dark': '#246B43',
    'success-darker': '#1B5232',
    'success-light': '#3FA76B',
    'success-lighter': '#56BF82',
    'success-lightest': '#DBEFE3',
    tertiary: '#2E8B57',
    'tertiary-dark': '#246B43',
    'tertiary-darker': '#1B5232',
    'tertiary-light': '#3FA76B',
    'tertiary-lighter': '#56BF82',
    'tertiary-lightest': '#DBEFE3',

    info: '#8B91D9',
    'info-dark': '#6F76C7',
    'info-darker': '#555CB0',
    'info-light': '#A2A7E0',
    'info-lighter': '#B9BDE8',
    'info-lightest': '#EAEBF7',
    quaternary: '#8B91D9',
    'quaternary-dark': '#6F76C7',
    'quaternary-darker': '#555CB0',
    'quaternary-light': '#A2A7E0',
    'quaternary-lighter': '#B9BDE8',
    'quaternary-lightest': '#EAEBF7',

    warning: '#D97706',
    error: '#C2410C',

    'surface-card': '#FFFFFF',
    'surface-card-hover': '#F0F0E9',
    'surface-card-border': '#E8E8E1',
    'surface-well': '#F0F0E9',
    'surface-well-border': '#E8E8E1',
    'surface-panel': '#FFFFFF',
    'surface-sidebar': '#F7F7F2',
    'input-bg': '#FFFFFF',
    'input-bg-disabled': '#E8E8E1',
    'input-border': '#D4D4CE',
    'progress-track': '#E8E8E1',

    water: '#BFD8DC',
    land: '#2E8B57',
    'land-dark': '#1B5232',
    'land-light': '#56BF82',
    src: '#C2410C',
    dst: '#6A6FCC'
  }
};

/* ---- v7-modern-dark (new) ------------------------------------------ */
/* High-contrast charcoal with a vibrant amber accent. Distinct from
   the 3 existing dark themes which use blue/green/gray accents. */
const v7ModernDark = {
  id: 'v7-modern-dark',
  name: 'V7 Modern Dark',
  dark: true,
  colors: {
    background: '#0F1115',
    surface: '#1A1D23',
    foreground: '#F5F5F0',
    'foreground-accent': '#22D3EE',
    'on-background': '#F5F5F0',
    'on-surface': '#F5F5F0',
    'on-primary': '#0F1115',
    'on-secondary': '#0F1115',
    'on-success': '#0F1115',
    'on-info': '#0F1115',
    'on-warning': '#0F1115',
    'on-error': '#0F1115',
    'button-fg': '#0F1115',
    white: '#0F1115',
    black: '#F5F5F0',
    outline: '#2E323A',

    neutral: '#A0A8B0',
    'neutral-dark': '#C5CBD0',
    'neutral-darker': '#E0E4E8',
    'neutral-light': '#3A3F47',
    'neutral-lighter': '#2A2E35',
    'neutral-lightest': '#1F232A',

    primary: '#22D3EE',
    'primary-dark': '#0EA5C0',
    'primary-darker': '#0B8395',
    'primary-light': '#4ADCF1',
    'primary-lighter': '#7EE7F5',
    'primary-lightest': '#0E3A42',

    secondary: '#A0A8B0',
    'secondary-dark': '#838B93',
    'secondary-darker': '#666D75',
    'secondary-light': '#B5BCC4',
    'secondary-lighter': '#C9CFD5',
    'secondary-lightest': '#262B33',

    success: '#4ADE80',
    'success-dark': '#34B463',
    'success-darker': '#258949',
    'success-light': '#65E594',
    'success-lighter': '#82EBA8',
    'success-lightest': '#1A3B23',
    tertiary: '#4ADE80',
    'tertiary-dark': '#34B463',
    'tertiary-darker': '#258949',
    'tertiary-light': '#65E594',
    'tertiary-lighter': '#82EBA8',
    'tertiary-lightest': '#1A3B23',

    info: '#8B6CE6',
    'info-dark': '#6F50D0',
    'info-darker': '#583EB2',
    'info-light': '#9F84EC',
    'info-lighter': '#B19DF1',
    'info-lightest': '#231843',
    quaternary: '#8B6CE6',
    'quaternary-dark': '#6F50D0',
    'quaternary-darker': '#583EB2',
    'quaternary-light': '#9F84EC',
    'quaternary-lighter': '#B19DF1',
    'quaternary-lightest': '#231843',

    warning: '#FBBF24',
    error: '#F87171',

    'surface-card': '#1A1D23',
    'surface-card-hover': '#23272E',
    'surface-card-border': '#2E323A',
    'surface-well': '#161A1F',
    'surface-well-border': '#2E323A',
    'surface-panel': '#1A1D23',
    'surface-sidebar': '#0F1115',
    'input-bg': '#161A1F',
    'input-bg-disabled': '#0F1115',
    'input-border': '#2E323A',
    'progress-track': '#2A2E35',

    water: '#1F2A38',
    land: '#4ADE80',
    'land-dark': '#258949',
    'land-light': '#82EBA8',
    src: '#F87171',
    dst: '#FBBF24'
  }
};

/* ---- exported manifest ---------------------------------------------- */

export const THEMES = [
  arkimeLight,
  arkimeDark,
  purp,
  blue,
  green,
  cottonCandy,
  dark2,
  dark3,
  v7ModernLight,
  v7ModernDark
];

export const DEFAULT_THEME_ID = 'arkime-light';

/** Lookup helper. */
export function getTheme (id) {
  return THEMES.find(t => t.id === id);
}
