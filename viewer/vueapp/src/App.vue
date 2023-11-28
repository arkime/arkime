<!--
Copyright Yahoo Inc.
SPDX-License-Identifier: Apache-2.0
-->
<template>
  <div v-if="compatibleBrowser">
    <arkime-navbar></arkime-navbar>
    <router-view v-if="user" />
    <div class="pull-right small app-info-error">
      <arkime-toast
        class="mr-1"
        type="danger"
        :duration="1000000"
        :done="messageDone"
        :message="appInfoMissing">
      </arkime-toast>
    </div>
    <keyboard-shortcuts
      shortcuts-class="arkime-shortcuts"
      @shift-hold-change="shiftHoldChange">
      <template v-slot:content>
        <code>'Q'</code> - set focus to query bar
        <br>
        <code>'T'</code> - set focus to time range selector
        <br>
        <code>'S'</code> - jump to the Sessions page
        <br>
        <code>'V'</code> - jump to the SPI View page
        <br>
        <code>'G'</code> - jump to the SPI Graph page
        <br>
        <code>'C'</code> - jump to the Connections page
        <br>
        <code>'H'</code> - jump to the Arkime Help page
        <br>
        <code>'U'</code> - jump to the Arkime Hunt page
        <br>
        <code>'shift + enter'</code> - issue search/refresh
        <br>
        <code>'esc'</code> - remove focus from any input and close this dialog
        <br>
        <code>'?'</code> - shows you this dialog, but I guess you already knew that
      </template>
    </keyboard-shortcuts>
    <arkime-footer></arkime-footer>
    <arkime-welcome-message
      v-if="user && (!user.welcomeMsgNum || user.welcomeMsgNum < 1)">
    </arkime-welcome-message>
  </div>
  <div v-else>
    <arkime-upgrade-browser>
    </arkime-upgrade-browser>
  </div>
</template>

<script>
import ConfigService from './components/utils/ConfigService';
import ArkimeToast from './components/utils/Toast';
import ArkimeNavbar from './components/utils/Navbar';
import ArkimeFooter from './components/utils/Footer';
import ArkimeWelcomeMessage from './components/utils/WelcomeMessage';
import ArkimeUpgradeBrowser from './components/utils/UpgradeBrowser';
import KeyboardShortcuts from '../../../common/vueapp/KeyboardShortcuts';

export default {
  name: 'App',
  components: {
    ArkimeToast,
    ArkimeNavbar,
    ArkimeFooter,
    KeyboardShortcuts,
    ArkimeWelcomeMessage,
    ArkimeUpgradeBrowser
  },
  data: function () {
    return {
      appInfoMissing: '',
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
    this.compatibleBrowser = (typeof Object.__defineSetter__ === 'function') &&
      !!String.prototype.includes;

    if (!this.compatibleBrowser) {
      console.log('Incompatible browser, please upgrade!');
      return;
    }

    // get the information for the entire app
    // the rest of the app should compute from $store.state
    ConfigService.getAppInfo().then((response) => {
      if (!response.user.settings.theme || response.user.settings.theme === 'default-theme') {
        this.setTheme();
      }
    }).catch((error) => {
      // display appwide error that floats at the bottom right of the screen on top of everything
      this.appInfoMissing = 'Error fetching app info! Arkime will not work as intended.';
      this.user = { settings: { timezone: 'local' } };
      this.setTheme();
    });

    // watch for keyup/down events for the entire app
    // the rest of the app should compute necessary values with:
    // $store.state.shiftKeyHold, focusSearch, and focusTimeRange
    window.addEventListener('keyup', (e) => {
      const activeElement = document.activeElement;
      const inputs = ['input', 'select', 'textarea'];

      if (e.keyCode === 27) { // esc
        activeElement.blur(); // remove focus from all inputs
        return;
      }

      // quit if the user is in an input or not holding the shift key
      if (!this.shiftKeyHold || (activeElement && inputs.indexOf(activeElement.tagName.toLowerCase()) !== -1)) {
        return;
      }

      switch (e.keyCode) {
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
      }
    });
  },
  methods: {
    shiftHoldChange (val) {
      this.shiftKeyHold = val;
    },
    routeTo: function (url) {
      this.$router.push({
        path: url,
        query: {
          ...this.$route.query,
          expression: this.$store.state.expression
        },
        hash: this.$route.hash
      });
    },
    // if the user doesn't have a theme preference, set dark/light theme based on OS color scheme
    setTheme () {
      if (window.matchMedia) {
        const darkMode = window.matchMedia('(prefers-color-scheme: dark)').matches;
        document.body.className = darkMode ? 'arkime-dark-theme' : 'arkime-light-theme';
      }
    },
    /* remove the message when user is done with it or duration ends */
    messageDone: function () {
      this.appInfoMissing = '';
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
}

/* text */
.text-theme-accent    { color: var(--color-foreground-accent); }
.text-theme-primary   { color: var(--color-primary); }
.text-theme-secondary { color: var(--color-secondary); }
.text-theme-tertiary  { color: var(--color-tertiary); }
.text-theme-quaternary{ color: var(--color-quaternary); }
.text-muted-more      { color: var(--color-gray); }
.text-theme-white     { color: var(--color-white); }
.text-theme-button    { color: var(--color-button, #FFF); }

.text-theme-gray-hover:hover {
  color: var(--color-gray);
}

/* displaying */
.fixed-header {
  z-index: 5;
  position: fixed;
  left: 0;
  right: 0;
  background-color: var(--color-quaternary-lightest);

  -webkit-box-shadow: 0 0 16px -2px black;
     -moz-box-shadow: 0 0 16px -2px black;
          box-shadow: 0 0 16px -2px black;
}

/* themed buttons */
a[class*=' btn-theme'],
div[class*=' btn-theme'],
button[class*=' btn-theme'] {
  color: var(--color-button, #FFF) !important;
}

.btn-clear-input {
  color: var(--color-foreground, #555) !important;
  background-color: var(--color-background, #EEE) !important;
  border-color: var(--color-gray) !important;
}

.btn.btn-danger,
.btn.btn-primary,
.btn.btn-theme-primary {
  color           : var(--color-button, #FFF);
  background-color: var(--color-primary);
  border-color    : var(--color-primary-dark);
}
.btn.btn-danger:hover,
.btn.btn-primary:hover,
.btn.btn-theme-primary:hover {
  background-color: var(--color-primary-dark);
  border-color    : var(--color-primary-darker);
}
.btn.btn-danger.active,
.btn.btn-primary.active,
.btn.btn-theme-primary.active {
  background-color: var(--color-primary-darker);
  border-color    : var(--color-primary-darker);
}

.btn.btn-warning,
.btn.btn-theme-secondary {
  color           : var(--color-button, #FFF);
  background-color: var(--color-secondary);
  border-color    : var(--color-secondary-dark);
}
.btn.btn-warning:hover,
.btn.btn-theme-secondary:hover {
  background-color: var(--color-secondary-dark);
  border-color    : var(--color-secondary-darker);
}
.btn.btn-warning.active,
.btn.btn-theme-secondary.active {
  background-color: var(--color-secondary-darker);
  border-color    : var(--color-secondary-darker);
}

.btn.btn-success,
.btn.btn-theme-tertiary {
  color           : var(--color-button, #FFF);
  background-color: var(--color-tertiary);
  border-color    : var(--color-tertiary-dark);
}
.btn.btn-success:hover,
.btn.btn-theme-tertiary:hover {
  background-color: var(--color-tertiary-dark);
  border-color    : var(--color-tertiary-darker);
}
.btn.btn-success:active,
.btn.btn-theme-tertiary.active {
  background-color: var(--color-tertiary-darker);
  border-color    : var(--color-tertiary-darker);
}

.btn.btn-info,
.btn.btn-theme-quaternary {
  color           : var(--color-button, #FFF);
  background-color: var(--color-quaternary);
  border-color    : var(--color-quaternary-dark);
}
.btn.btn-info:hover,
.btn.btn-theme-quaternary:hover {
  background-color: var(--color-quaternary-dark);
  border-color    : var(--color-quaternary-darker);
}
.btn.btn-info.active,
.btn.btn-theme-quaternary.active {
  background-color: var(--color-quaternary-darker);
  border-color    : var(--color-quaternary-darker);
}

/* themed radio/checkbox buttons */
label.btn-radio,
button.btn-checkbox,
div.btn-checkbox > label,
div.btn-group-toggle > label {
  cursor: pointer;
  background-image: none;
  background-color: var(--color-background, white) !important;
  border-color    : var(--color-primary) !important;
  color           : var(--color-primary) !important;
}
label.btn-radio.active:hover:not(:disabled),
button.btn-checkbox.active:hover:not(:disabled),
div.btn-checkbox > label.active:hover:not(:disabled) {
  background-color: var(--color-primary-darker) !important;
  color: var(--color-button, #FFF) !important;
}
label.btn-radio:hover:not(:disabled),
button.btn-checkbox:hover:not(:disabled),
div.btn-checkbox > label:hover:not(:disabled) {
  color           : var(--color-primary);
  background-color: var(--color-primary-lightest) !important;
}
label.btn-radio.active:not(:disabled),
button.btn-checkbox.active:not(:disabled),
div.btn-checkbox > label.active:not(:disabled),
.btn-group-checkboxes label.btn-secondary.active:not(:disabled) {
  border-color    : var(--color-primary) !important;
  background-color: var(--color-primary) !important;
  color: var(--color-button, #FFF) !important;
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

/* see top level common.css info area for usage */
.info-area { color: var(--color-gray-dark); }
.info-area > div { background-color: var(--color-gray-light); }

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

/* theme alert areas */
.alert.alert-info {
  color: var(--color-primary);
  border-color: var(--color-primary-darkest);
  background-color: var(--color-primary-lightest);
}
.alert.alert-danger {
  color: var(--color-secondary);
  border-color: var(--color-secondary-darkest);
  background-color: var(--color-secondary-lightest);
}
.alert.alert-success {
  color: var(--color-tertiary);
  border-color: var(--color-tertiary-darkest);
  background-color: var(--color-tertiary-lightest);
}
.alert.alert-warning {
  color: var(--color-quaternary);
  border-color: var(--color-quaternary-darkest);
  background-color: var(--color-quaternary-lightest);
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

/* keyboard shortcut info styling */
.arkime-shortcuts {
  top: 155px;
  z-index: 9;
  position: fixed;
  border-radius: 0 4px 4px 0;
  border: 1px solid var(--color-gray);
  border-left: none;
  background: var(--color-background, white);
  -webkit-box-shadow: 0 0 16px -2px black;
     -moz-box-shadow: 0 0 16px -2px black;
          box-shadow: 0 0 16px -2px black;
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
  background-color: var(--color-white, #FFFFFF);
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
.arkime-info {
  margin-top: 20px;
}

.arkime-info .center-area > img {
  z-index: 99;
}

.arkime-info .center-area {
  display: flex;
  align-items: center;
  justify-content: center;
  text-align: center;
  flex-direction: column;
  min-height: 75vh;
}

.arkime-info .well {
  margin-top: -6px;
  min-width: 25%;
  box-shadow: 4px 4px 10px 0 rgba(0,0,0,0.5);
}

.arkime-info .well > h1 {
  margin-top: 0;
  color: var(--color-primary);
}

/* apply theme foreground to tables */
table.table {
  color: var(--color-foreground, #555);
}

/* column resize grips */
.grip {
  width: 8px;
  top: -2px;
  right: -4px;
  bottom: -2px;
  cursor: col-resize;
  position: absolute;
  z-index: 9;
}

/* badge remove button */
.badge > button.close {
  line-height: 0.4;
  font-size: 1.2rem;
  margin-left: 0.3rem;
}

/* application information error */
.app-info-error {
  right: 10px;
  bottom: 15px;
  z-index: 999;
  position: fixed;
}
.app-info-error > div.alert {
  font-size: 17px;
}
</style>
