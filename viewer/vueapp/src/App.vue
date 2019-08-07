<template>
  <div v-if="compatibleBrowser">
    <moloch-navbar></moloch-navbar>
    <router-view v-if="user" />
    <transition name="shortcuts-slide">
      <moloch-keyboard-shortcuts
        v-if="displayKeyboardShortcutsHelp">
      </moloch-keyboard-shortcuts>
      <div v-else-if="shiftKeyHold && !displayKeyboardShortcutsHelp"
        class="shortcut-help">
        <span class="fa fa-question fa-fw">
        </span>
      </div>
    </transition>
    <moloch-footer></moloch-footer>
    <moloch-welcome-message
      v-if="user && (!user.welcomeMsgNum || user.welcomeMsgNum < 1)">
    </moloch-welcome-message>
  </div>
  <div v-else>
    <moloch-upgrade-browser>
    </moloch-upgrade-browser>
  </div>
</template>

<script>
import UserService from './components/users/UserService';
import MolochNavbar from './components/utils/Navbar';
import MolochFooter from './components/utils/Footer';
import MolochWelcomeMessage from './components/utils/WelcomeMessage';
import MolochUpgradeBrowser from './components/utils/UpgradeBrowser';
import MolochKeyboardShortcuts from './components/utils/KeyboardShortcuts';

export default {
  name: 'App',
  components: {
    MolochNavbar,
    MolochFooter,
    MolochWelcomeMessage,
    MolochUpgradeBrowser,
    MolochKeyboardShortcuts
  },
  data: function () {
    return {
      compatibleBrowser: true
    };
  },
  computed: {
    shiftKeyHold: {
      get: function () {
        return this.$store.state.shiftKeyHold;
      },
      set: function (newValue) {
        this.$store.commit('setShiftKeyHold', newValue);
      }
    },
    displayKeyboardShortcutsHelp: {
      get: function () {
        return this.$store.state.displayKeyboardShortcutsHelp;
      },
      set: function (newValue) {
        this.$store.commit('setDisplayKeyboardShortcutsHelp', newValue);
      }
    },
    user: {
      get: function () {
        return this.$store.state.user;
      },
      set: function (newValue) {
        this.$store.commit('setUser', newValue);
      }
    }
  },
  mounted: function () {
    this.compatibleBrowser = (typeof Object['__defineSetter__'] === 'function') &&
      !!String.prototype.includes;

    if (!this.compatibleBrowser) {
      console.log('Incompatible browser, please upgrade!');
      return;
    }

    // get the current user for the entire app
    // the rest of the app should compute the user with $store.state.user
    UserService.getCurrent()
      .then((response) => {
        this.user = response;
      }, (error) => {
        this.user = { settings: { timezone: 'local' } };
      });

    const inputs = ['input', 'select', 'textarea'];

    // watch for keyup/down events for the entire app
    // the rest of the app should compute necessary values with:
    // $store.state.shiftKeyHold, focusSearch, and focusTimeRange
    window.addEventListener('keyup', (event) => {
      const activeElement = document.activeElement;

      if (event.keyCode === 27) { // esc
        activeElement.blur(); // remove focus from all inputs
        this.$store.commit('setFocusSearch', false);
        this.$store.commit('setFocusTimeRange', false);
        return;
      } else if (event.keyCode === 16) { // shift
        this.shiftKeyHold = false;
        return;
      }

      // quit if the user is in an input or not holding the shift key
      if (!this.shiftKeyHold || (activeElement && inputs.indexOf(activeElement.tagName.toLowerCase()) !== -1)) {
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
        case 13: // enter
          // trigger search/refresh
          this.$store.commit('setIssueSearch', true);
          break;
        case 191: // /
          // toggle display of the the keyboard shortcut dialog
          this.$store.commit('setDisplayKeyboardShortcutsHelp', !this.displayKeyboardShortcutsHelp);
          break;
      }
    });

    window.addEventListener('keydown', (event) => {
      const activeElement = document.activeElement;
      // quit if the user is in an input or not holding the shift key
      if (activeElement && inputs.indexOf(activeElement.tagName.toLowerCase()) !== -1) {
        return;
      }
      if (event.keyCode === 16) { // shift
        this.shiftKeyHold = true;
      } else if (event.keyCode === 27) { // esc
        this.shiftKeyHold = false;
      }
    });

    // if the user clicks something, remove shift hold
    window.addEventListener('mousedown', (event) => {
      this.shiftKeyHold = false;
    });

    // if the user focus is not in the web page, remove shift hold
    window.addEventListener('blur', (event) => {
      this.shiftKeyHold = false;
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
/* styles for bottom footer */
html {
  position: relative;
  min-height: 100%;
}
#app {
  padding-bottom: 25px;
}

/* global font, colors, and vars */
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
.btn-clear-input {
  color: var(--color-foreground, #555) !important;
  background-color: var(--color-background, #EEE) !important;
  border-color: var(--color-gray) !important;
}

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
label.btn-radio,
button.btn-checkbox,
div.btn-checkbox > label {
  cursor: pointer;
  background-image: none;
  background-color: var(--color-background, white) !important;
  border-color    : var(--color-primary) !important;
  color           : var(--color-primary);
}
label.btn-radio.active:hover:not(:disabled),
button.btn-checkbox.active:hover:not(:disabled),
div.btn-checkbox > label.active:hover:not(:disabled) {
  background-color: var(--color-primary-darker) !important;
}
label.btn-radio:hover:not(:disabled),
button.btn-checkbox:hover:not(:disabled),
div.btn-checkbox > label:hover:not(:disabled) {
  color           : var(--color-primary);
  background-color: var(--color-primary-lightest) !important;
}
label.btn-radio.active:not(:disabled),
button.btn-checkbox.active:not(:disabled),
div.btn-checkbox > label.active:not(:disabled) {
  border-color    : var(--color-primary) !important;
  background-color: var(--color-primary) !important;
}
label.btn-radio:disabled,
button.btn-checkbox:disabled,
div.btn-checkbox > label:disabled {
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

.horizontal-center {
  display         : flex;
  align-items     : center;
  justify-content : center;
  text-align      : center;
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
dl.dl-horizontal.dl-horizontal-wide dt {
  width: 400px;
}
dl.dl-horizontal.dl-horizontal-wide dd {
  margin-left: 410px;
  margin-bottom: 0;
}

.shortcut-help {
  position: fixed;
  top: 155px;
  z-index: 1;
  color: var(--color-tertiary-lighter);
  background: var(--color-background, white);
  border: 1px solid var(--color-gray);
  border-left: none;
  border-radius: 0 4px 4px 0 ;
  -webkit-box-shadow: 0 0 16px -2px black;
     -moz-box-shadow: 0 0 16px -2px black;
          box-shadow: 0 0 16px -2px black;
}

/* keyboard shortcuts help animation */
.shortcuts-slide-enter-active, .shortcuts-slide-leave-active {
  transition: all .5s ease;
}
.shortcuts-slide-enter, .shortcuts-slide-leave-to {
  transform: translateX(-465px);
}

/* make the shortcut letter the same size/position as the icon */
.query-shortcut {
  color: var(--color-tertiary-lighter);
  font-size: 14px;
  width: 20px;
}
.time-shortcut {
  color: var(--color-tertiary-lighter);
  font-size: 14px;
  width: 20px;
}
/* make sure the width of the input prepend doesn't change */
.input-group-prepend-fw, .input-group-text-fw {
  width: 36px;
}

/* enter icon for search/refresh button to be displayed on shift hold */
.search-btn { width: 62px; }
.refresh-btn { width: 66px; }
.enter-icon > .fa-long-arrow-left {
  position: relative;
  top: 2px;
}
.enter-icon > .enter-arm {
  display: inline-block;
  height: 9px;
  width: 3px;
  background-color: #FFFFFF;
  position: relative;
  top: -2px;
  right: 6px;
}

/* custom font awesome icons */
.fa.fa-venn {
  position: relative;
  display: inline-block;
  vertical-align: middle;
  width: 1.28571429em;
  text-align: center;
}
.fa.fa-venn > span.fa-circle-o {
  position: absolute;
  top: -7px;
}
.fa.fa-venn> span.fa-circle-o:first-child {
  left: 5px;
}
.fa.fa-venn> span.fa-circle-o:last-child {
  right: 6px;
}

/* info page (404 & upgrade) */
.moloch-info {
  margin-top: 20px;
}

.moloch-info .center-area > img {
  z-index: 99;
}

.moloch-info .center-area {
  display: flex;
  align-items: center;
  justify-content: center;
  text-align: center;
  flex-direction: column;
  min-height: 75vh;
}

.moloch-info .well {
  margin-top: -6px;
  min-width: 25%;
  box-shadow: 4px 4px 10px 0 rgba(0,0,0,0.5);
}

.moloch-info .well > h1 {
  margin-top: 0;
  color: var(--color-primary);
}

/* apply theme foreground to tables */
table.table {
  color: var(--color-foreground, #555);
}
</style>
