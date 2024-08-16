// NOTE: in theming, it is important to specify both light/dark variants (or use `all`)
//       unless it is a pre-existing color. Otherwise, vuetify components
//       will have janky background colors over the unspecified variant
//       since they use the background color to calculate parts of their color.
/** @type{Object<string, { dark: string, light: string }>} */
const theming = {
  info: { all: '#AB4CFF' },
  background: { // existing color `background`
    dark: '#212121'
  },
  dark: {
    light: '#343a40',
    dark: '#131313'
  },
  light: { // used for variant="light" badges
    light: '#f3f3f3',
    dark: '#131313'
  },
  'secondary-gray': { all: '#6C757D' },
  muted: { // used for icons on pre-search cont3xt page
    light: '#6c757d',
    dark: '#6c757d'
  },
  well: {
    light: '#d6d8d9',
    dark: '#333'
  },
  'well-border': {
    light: '#c6c8ca',
    dark: '#444'
  },
  'progress-bar': {
    light: '#e9ecef',
    dark: '#404040'
  },
  'side-stub': {
    light: '#e9ecef',
    dark: '#555'
  },
  'integration-panel': {
    dark: '#333', // dark well color
    light: '#FFFFFF'
  },
  'cont3xt-card': {
    light: '#E9ECEF',
    dark: '#303030'
  },
  'cont3xt-card-hover': {
    light: '#d9dbde',
    dark: '#3d3d3d'
  },
  'cont3xt-card-border': {
    light: '#FFF',
    dark: '#232323'
  },
  'cont3xt-table-border': {
    light: '#dee2e6',
    dark: '#CCC'
  },
  'integration-btn': { all: '#343a40' },
  textarea: {
    light: '#FFFFFF',
    dark: '#000000'
  },
  'textarea-disabled': {
    light: '#EEEEEE',
    dark: '#111111'
  },
  'textarea-border': {
    light: '#ced4da',
    dark: '#EEEEEE'
  }
};

/**
  * @param {'light' | 'dark'} variant
  */
export function createCont3xtTheme (variant) {
  return {
    dark: variant === 'dark',
    colors:
      Object.fromEntries(
        Object.entries(theming)
          .map(([colorName, colors]) => [colorName, colors[variant] ?? colors.all])
          .filter(([_, color]) => color != null)
      )
  };
}

// /* old bootstrap theming colors for reference --------------- */
// $theme-colors: (
//   "danger": #FF0080,
//   "warning": #E39300,
//   "success": #00AF68,
//   "primary": #00AAC5,
//   "info": #AB4CFF
// );
//
// body {
//   --color-light: #F3F3F3;
//   --color-gray-light: #BBBBBB;
//   --color-gray: #777777;
//   --color-gray-dark: #555555;
//   --color-dark: #131313;
//   --color-background: #FFFFFF;
// }
// body.dark {
//   --color-dark: #F3F3F3;
//   --color-gray-dark: #BBBBBB;
//   --color-gray: #777777;
//   --color-gray-light: #555555;
//   --color-light: #131313;
//   --color-background: #222;
// }
