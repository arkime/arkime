<template>
  <div>
    <moloch-navbar></moloch-navbar>
    <router-view />
    <transition name="shortcuts-slide">
      <moloch-keyboard-shortcuts
        v-if="displayKeyboardShortcutsHelp">
      </moloch-keyboard-shortcuts>
    </transition>
    <moloch-footer></moloch-footer>
  </div>
</template>

<script>
import MolochNavbar from './components/utils/Navbar';
import MolochFooter from './components/utils/Footer';
import MolochKeyboardShortcuts from './components/utils/KeyboardShortcuts';

let holdingShiftKey = false;

export default {
  name: 'App',
  components: {
    MolochNavbar,
    MolochFooter,
    MolochKeyboardShortcuts
  },
  computed: {
    displayKeyboardShortcutsHelp: {
      get: function () {
        return this.$store.state.displayKeyboardShortcutsHelp;
      },
      set: function (newValue) {
        this.$store.commit('setDisplayKeyboardShortcutsHelp', newValue);
      }
    }
  },
  mounted: function () {
    const inputs = ['input', 'select', 'textarea'];

    window.addEventListener('keyup', (event) => {
      const activeElement = document.activeElement;

      if (event.keyCode === 27) { // esc
        activeElement.blur(); // remove focus from all inputs
        this.$store.commit('setFocusSearch', false);
        this.$store.commit('setFocusTimeRange', false);
        return;
      }

      // quit if the user is in an input or not holding the shift key
      if (!holdingShiftKey || (activeElement && inputs.indexOf(activeElement.tagName.toLowerCase()) !== -1)) {
        return;
      }

      switch (event.keyCode) {
        case 81: // q
          // focus on search expression input
          this.$store.commit('setFocusSearch', true);
          break;
        case 84: // t
          // focus on time range selector
          this.$store.commit('setFocusTimeRange', true);
          break;
        case 83: // s
          // open sessions page if not on sessions page
          if (this.$route.name !== 'Sessions') {
            this.routeTo('/sessions');
          }
          break;
        case 86: // v
          // open spiview page if not on spiview page
          if (this.$route.name !== 'Spiview') {
            this.routeTo('/spiview');
          }
          break;
        case 71: // g
          // open spigraph page if not on spigraph page
          if (this.$route.name !== 'Spigraph') {
            this.routeTo('/spigraph');
          }
          break;
        case 67: // c
          // open connections page if not on connections page
          if (this.$route.name !== 'Connections') {
            this.routeTo('/connections');
          }
          break;
        case 72: // h
          // open help page if not on help page
          if (this.$route.name !== 'Help') {
            this.routeTo('/help');
          }
          break;
        case 85: // u
          // open hunt page if not on hunt page
          if (this.$route.name !== 'Hunt') {
            this.routeTo('/hunt');
          }
          break;
        case 191: // /
          // display the keyboard shortcut dialog
          this.$store.commit('setDisplayKeyboardShortcutsHelp', true);
          break;
        case 16: // shift
          // keyup on shift key, user is no longer holding shift
          holdingShiftKey = false;
          break;
      }
    });

    window.addEventListener('keydown', (event) => {
      if (event.keyCode === 16) { // shift
        holdingShiftKey = true;
      }
    });
  },
  methods: {
    routeTo: function (url) {
      this.$router.push({
        path: url,
        query: {
          ...this.$route.query,
          expression: this.$store.state.expression
        },
        hash: this.$route.hash
      });
    }
  }
};
</script>

<style>
body {
  color: var(--color-foreground);
  background-color: var(--color-background);
  font-family: "Helvetica Neue", Helvetica, Arial, sans-serif !important;

  --px-none   : 0;          /* 0px */
  --px-xs     : 0.125rem;   /* 2px */
  --px-sm     : .25rem;     /* 4px */
  --px-md     : .5rem;      /* 8px */
  --px-lg     : .75rem;     /* 12px */
  --px-xlg    : 1rem;       /* 16px */
  --px-xxlg   : 1.5rem;     /* 24px */
  --px-xxxlg  : 2rem;       /* 32px */
  --px-xxxxlg : 3rem;       /* 48px */
  --px-xxxxxlg: 3.5rem;     /* 56px */
}

/* cursors */
.cursor-help, .help-cursor { cursor: help; }
.cursor-text, .text-cursor { cursor: text; }
.cursor-pointer, .pointer-cursor { cursor: pointer; }
.cursor-crosshair, .crosshair-cursor { cursor: crosshair; }

/* text */
.text-theme-accent    { color: var(--color-foreground-accent); }
.text-theme-primary   { color: var(--color-primary); }
.text-theme-secondary { color: var(--color-secondary); }
.text-theme-tertiary  { color: var(--color-tertiary); }
.text-theme-quaternary{ color: var(--color-quaternary); }
.text-muted-more      { color: var(--color-gray); }

/* font sizes */
.medium { font-size: 95%; }
.large  { font-size: 1.8rem; }
.xlarge { font-size: 2rem; }

.bold   { font-weight: bold; }

/* wrapping */
.no-wrap { white-space: nowrap; }

/* displaying */
.display-inline { display: inline; }

/* overflow */
.no-overflow    { overflow: hidden; }
.no-overflow-x  { overflow-x: hidden; }
.no-overflow-y  { overflow-y: hidden; }

/* anchor tag no decoration */
a.no-decoration { text-decoration: none; }

/* extra small buttons */
.btn-xs, .btn-group-xs > .btn {
  padding: 1px 5px;
  font-size: 12px;
  line-height: 1.5;
  border-radius: 3px;
}

/* themed buttons */
.btn.btn-theme-primary {
  color           : #FFFFFF;
  background-color: var(--color-primary);
  border-color    : var(--color-primary-dark);
}
.btn.btn-theme-primary:hover {
  background-color: var(--color-primary-dark);
  border-color    : var(--color-primary-darker);
}
.btn.btn-theme-primary.active {
  background-color: var(--color-primary-darker);
  border-color    : var(--color-primary-darker);
}

.btn.btn-theme-secondary {
  color           : #FFFFFF;
  background-color: var(--color-secondary);
  border-color    : var(--color-secondary-dark);
}
.btn.btn-theme-secondary:hover {
  background-color: var(--color-secondary-dark);
  border-color    : var(--color-secondary-darker);
}
.btn.btn-theme-secondary.active {
  background-color: var(--color-secondary-darker);
  border-color    : var(--color-secondary-darker);
}

.btn.btn-theme-tertiary {
  color           : #FFFFFF;
  background-color: var(--color-tertiary);
  border-color    : var(--color-tertiary-dark);
}
.btn.btn-theme-tertiary:hover {
  background-color: var(--color-tertiary-dark);
  border-color    : var(--color-tertiary-darker);
}
.btn.btn-theme-tertiary.active {
  background-color: var(--color-tertiary-darker);
  border-color    : var(--color-tertiary-darker);
}

.btn.btn-theme-quaternary {
  color           : #FFFFFF;
  background-color: var(--color-quaternary);
  border-color    : var(--color-quaternary-dark);
}
.btn.btn-theme-quaternary:hover {
  background-color: var(--color-quaternary-dark);
  border-color    : var(--color-quaternary-darker);
}
.btn.btn-theme-quaternary.active {
  background-color: var(--color-quaternary-darker);
  border-color    : var(--color-quaternary-darker);
}

/* themed radio/checkbox buttons */
label.btn-radio, button.btn-checkbox {
  background-image: none;
  background-color: var(--color-background, white) !important;
  border-color    : var(--color-primary) !important;
  color           : var(--color-primary);
}
label.btn-radio.active:hover:not(:disabled),
button.btn-checkbox.active:hover:not(:disabled) {
  background-color: var(--color-primary-darker) !important;
}
label.btn-radio:hover:not(:disabled),
button.btn-checkbox:hover:not(:disabled) {
  color           : var(--color-primary);
  background-color: var(--color-primary-lightest) !important;
}
label.btn-radio.active:not(:disabled),
button.btn-checkbox.active:not(:disabled) {
  border-color    : var(--color-primary) !important;
  background-color: var(--color-primary) !important;
}
label.btn-radio:disabled, button.btn-checkbox:disabled {
  background-color: var(--color-background, white);
  color: var(--color-gray);
  border-color: var(--color-gray) !important;
  cursor: not-allowed;
}

/* themed labels */
.label.label-theme-primary {
  background-color: var(--color-primary);
}
.label.label-theme-secondary {
  background-color: var(--color-secondary);
}
.label.label-theme-tertiary {
  background-color: var(--color-tertiary);
}
.label.label-theme-quaternary {
  background-color: var(--color-quaternary);
}

/* small horizontal rule */
.hr-small {
  margin-top   : var(--px-sm);
  margin-bottom:var(--px-xs);
}

/* flex block to display text centered
 * (horizontally and vertically) on the page
 * note: takes up half of the page
 */
.vertical-horizontal-center {
  min-height      : 40vh;
  display         : flex;
  align-items     : center;
  justify-content : center;
  text-align      : center;
  flex-direction  : column;
}

.vertical-center {
  min-height      : 40vh;
  display         : flex;
  align-items     : center;
  justify-content : center;
  flex-direction  : column;
}

/* displays large text for important information
 * note: must contain an inner div with the text
 * example:
 * <div class="info-area">
 *   <div>Some important text!</div>
 * </div>
 */
.info-area {
  font-size : var(--px-xxxlg);
  color     : var(--color-gray-dark);
}

.info-area > div {
  padding         : var(--px-xxxlg);
  border-radius   : var(--px-sm);
  background-color: var(--color-gray-light);
}

.info-area span.fa {
  display         : flex;
  justify-content : center;
}

/* small alert areas */
.alert.alert-sm {
  padding       : 4px 35px 3px 8px;
  margin-top    : var(--px-none);
  margin-bottom : var(--px-none);
  font-size     : .85rem;
}
.alert.alert-sm button.close {
  padding: 0 .5rem;
}

/* sub navbars */
.sub-navbar {
  position: fixed;
  top: 36px;
  left: 0;
  right: 0;
  padding: var(--px-lg) var(--px-md) var(--px-sm) 13px;
  background-color: var(--color-secondary-lightest);
  -webkit-box-shadow: 0 0 16px -2px black;
     -moz-box-shadow: 0 0 16px -2px black;
          box-shadow: 0 0 16px -2px black;
}
.sub-navbar .sub-navbar-title {
  font-size: 19px;
  font-weight: bold;
}
.sub-navbar .sub-navbar-title .fa-stack {
  margin-top: -14px;
}
.sub-navbar > .toast-container {
  margin-top: -6px;
}

/* description list styles */
dl.dl-horizontal dt {
  float: left;
  width: 190px;
  overflow: hidden;
  clear: left;
  text-align: right;
  text-overflow: ellipsis;
  white-space: nowrap;
  margin-bottom: 0;
}
dl.dl-horizontal dd {
  margin-left: 205px;
  margin-bottom: 0;
}

/* keyboard shortcuts help animation */
.shortcuts-slide-enter-active, .shortcuts-slide-leave-active {
  transition: all .5s ease;
}
.shortcuts-slide-enter, .shortcuts-slide-leave-to {
  transform: translateX(-465px);
}
</style>
