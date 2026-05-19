<!--
Copyright Yahoo Inc.
SPDX-License-Identifier: Apache-2.0
-->
<template>
  <div v-if="compatibleBrowser">
    <arkime-navbar />
    <router-view v-if="user" />
    <div class="float-right small app-info-error">
      <arkime-toast
        class="me-1"
        type="danger"
        :duration="1000000"
        :done="messageDone"
        :message="appInfoMissing" />
    </div>
    <keyboard-shortcuts
      shortcuts-class="arkime-shortcuts"
      @shift-hold-change="shiftHoldChange">
      <template #content>
        <code>'Q'</code> - set focus to query bar
        <br>
        <code>'T'</code> - set focus to time range selector
        <br>
        <code>'S'</code> - jump to the Sessions page
        <br>
        <code>'A'</code> - jump to the Arkime page
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
    <arkime-footer :store="$store" />
    <arkime-welcome-message
      v-if="user && (!user.welcomeMsgNum || user.welcomeMsgNum < 1)" />
  </div>
  <div v-else>
    <arkime-upgrade-browser />
  </div>
</template>

<script>
import ConfigService from './components/utils/ConfigService.js';
import ArkimeToast from './components/utils/Toast.vue';
import ArkimeNavbar from './components/utils/Navbar.vue';
import ArkimeFooter from '@common/Footer.vue';
import ArkimeWelcomeMessage from './components/utils/WelcomeMessage.vue';
import ArkimeUpgradeBrowser from './components/utils/UpgradeBrowser.vue';
import KeyboardShortcuts from '@common/KeyboardShortcuts.vue';
import UserService from './components/users/UserService.js';
import { isLegacyCustomTheme, migrateLegacyCustomTheme } from '@common/themes/customTheme.js';
import { registerVuetifyTheme } from '@common/themes/registerVuetifyTheme.js';

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
      this.applySavedTheme(response.user.settings);
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

      if (e.key === 'Escape') { // esc
        activeElement.blur(); // remove focus from all inputs
        return;
      }

      // quit if the user is in an input or not holding the shift key
      if (!this.shiftKeyHold || (activeElement && inputs.indexOf(activeElement.tagName.toLowerCase()) !== -1)) {
        return;
      }

      // Only convert to lowercase if the key is a single character (letter)
      const key = e.key.length === 1 ? e.key.toLowerCase() : e.key;
      switch (key) {
      case 'q': // q
        // focus on search expression input
        this.$store.commit('setFocusSearch', true);
        break;
      case 't': // t
        // focus on time range selector
        this.$store.commit('setFocusTimeRange', true);
        break;
      case 's': // s
        // open sessions page if not on sessions page
        if (this.$route.name !== 'Sessions') {
          this.routeTo('/sessions');
        }
        break;
      case 'a': // a
        // open arkime page if not on arkime page
        if (this.$route.name !== 'Arkime') {
          this.routeTo('/arkime');
        }
        break;
      case 'v': // v
        // open spiview page if not on spiview page
        if (this.$route.name !== 'Spiview') {
          this.routeTo('/spiview');
        }
        break;
      case 'g': // g
        // open spigraph page if not on spigraph page
        if (this.$route.name !== 'Spigraph') {
          this.routeTo('/spigraph');
        }
        break;
      case 'c': // c
        // open connections page if not on connections page
        if (this.$route.name !== 'Connections') {
          this.routeTo('/connections');
        }
        break;
      case 'h': // h
        // open help page if not on help page
        if (this.$route.name !== 'Help') {
          this.routeTo('/help');
        }
        break;
      case 'u': // u
        // open hunt page if not on hunt page
        if (this.$route.name !== 'Hunt') {
          this.routeTo('/hunt');
        }
        break;
      case 'Enter': // enter
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
    /* Activate the user's saved theme via Vuetify. Handles three shapes:
       1. Legacy 'custom1:hex,hex,...' positional string -> migrate to
          the new {customTheme object, theme:'custom1'} shape, persist
          back, then activate.
       2. Legacy '{name}-theme' suffix strings (e.g. 'arkime-light-theme')
          -> strip suffix to get the new bare id.
       3. Bare new id ('arkime-light', 'custom1', etc.) -> activate. */
    applySavedTheme (settings) {
      const setting = settings && settings.theme;

      const registerCustom = (themeObj) => {
        registerVuetifyTheme(this.$vuetify, 'custom1', {
          dark: !!themeObj.dark,
          colors: { ...themeObj.colors }
        });
      };

      // Case 1: legacy 'custom1:...' positional format -> one-shot migration.
      if (isLegacyCustomTheme(setting)) {
        const migrated = migrateLegacyCustomTheme(setting);
        if (migrated) {
          settings.customTheme = migrated;
          settings.theme = 'custom1';
          // Persist the new shape; legacy string is dropped.
          UserService.saveSettings(settings).catch(() => { /* idempotent on retry */ });
          registerCustom(migrated);
          this.$vuetify.theme.change('custom1');
          return;
        }
      }

      // Register a stored custom theme (regardless of which theme is active)
      // so the user can switch to it via the picker.
      if (settings && settings.customTheme && settings.customTheme.colors) {
        registerCustom(settings.customTheme);
      }

      // Case 2/3: normalize id and activate.
      if (!setting || setting === 'default-theme') {
        this.setTheme();
        return;
      }
      const id = String(setting).replace(/-theme$/, '');
      const themesRecord = this.$vuetify.theme.themes.value || this.$vuetify.theme.themes;
      if (themesRecord && themesRecord[id]) {
        this.$vuetify.theme.change(id);
      } else {
        this.setTheme();
      }
    },
    // OS-color-scheme fallback when the user has no saved theme.
    setTheme () {
      if (window.matchMedia) {
        const darkMode = window.matchMedia('(prefers-color-scheme: dark)').matches;
        this.$vuetify.theme.change(darkMode ? 'arkime-dark' : 'arkime-light');
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
  color: rgb(var(--v-theme-foreground));
  background-color: rgb(var(--v-theme-background));
}

/* text */
.text-theme-accent    { color: rgb(var(--v-theme-foreground-accent)); }
.text-theme-primary   { color: rgb(var(--v-theme-primary)); }
.text-theme-secondary { color: rgb(var(--v-theme-secondary)); }
.text-theme-tertiary  { color: rgb(var(--v-theme-tertiary)); }
.text-theme-quaternary{ color: rgb(var(--v-theme-quaternary)); }
.text-muted-more      { color: rgb(var(--v-theme-neutral)); }
.text-theme-white     { color: rgb(var(--v-theme-white)); }
.text-theme-button    { color: rgb(var(--v-theme-button-fg)); }

.text-theme-gray-hover:hover {
  color: rgb(var(--v-theme-neutral));
}

/* displaying */
.fixed-header {
  z-index: 5;
  position: fixed;
  left: 0;
  right: 0;
  background-color: rgb(var(--v-theme-quaternary-lightest));

  -webkit-box-shadow: 0 0 16px -2px black;
     -moz-box-shadow: 0 0 16px -2px black;
          box-shadow: 0 0 16px -2px black;
}

/* themed buttons */
.btn-clear-input {
  color: rgb(var(--v-theme-foreground)) !important;
  background-color: rgb(var(--v-theme-background)) !important;
  border-color: rgb(var(--v-theme-neutral)) !important;
}

/* see top level common.css info area for usage */
.info-area { color: rgb(var(--v-theme-neutral-dark)); }
.info-area > div { background-color: rgb(var(--v-theme-neutral-light)); }

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
  top: 160px;
  z-index: 9;
  position: fixed;
  border-radius: 0 4px 4px 0;
  border: 1px solid rgb(var(--v-theme-neutral));
  border-left: none;
  background: rgb(var(--v-theme-background));
  -webkit-box-shadow: 0 0 16px -2px black;
     -moz-box-shadow: 0 0 16px -2px black;
          box-shadow: 0 0 16px -2px black;
}

/* make the shortcut letter the same size/position as the icon */
.query-shortcut {
  color: rgb(var(--v-theme-tertiary-lighter));
  font-size: 14px;
  width: 20px;
}
.time-shortcut {
  color: rgb(var(--v-theme-tertiary-lighter));
  font-size: 14px;
  width: 20px;
}
/* enter icon for search/refresh button to be displayed on shift hold */
.refresh-btn { width: 66px; }
.enter-icon > .fa-long-arrow-left {
  position: relative;
  top: 2px;
}
.enter-icon > .enter-arm {
  display: inline-block;
  height: 9px;
  width: 3px;
  background-color: rgb(var(--v-theme-white));
  position: relative;
  top: -2px;
  right: 3px;
}

/* custom venn-diagram composed icon: two overlapping circle outlines */
.arkime-venn {
  position: relative;
  display: inline-block;
  vertical-align: middle;
  width: 1.6em;
  height: 1em;
}
.arkime-venn .v-icon {
  position: absolute;
  top: -2px;
  font-size: 1.2em !important;
}
.arkime-venn .v-icon:first-child { left: 0; }
.arkime-venn .v-icon:last-child  { right: 0; }

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
  color: rgb(var(--v-theme-primary));
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

/* application information error */
.app-info-error {
  right: 10px;
  bottom: 15px;
  z-index: 999;
  position: fixed;
}

</style>
