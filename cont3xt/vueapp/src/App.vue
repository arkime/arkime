<!--
Copyright Yahoo Inc.
SPDX-License-Identifier: Apache-2.0
-->
<template>
  <v-app style="height: 100vh;">
    <div
      v-if="compatibleBrowser"
      class="d-flex flex-column h-100">
      <app-banner />
      <cont3xt-navbar />
      <div class="d-flex overflow-y-auto flex-grow-1">
        <router-view class="flex-grow-1 w-100" />
        <keyboard-shortcuts
          @shift-hold-change="shiftHoldChange"
          shortcuts-class="cont3xt-shortcuts"
          shortcuts-btn-transition="cont3xt-shortcuts-slide"
          shortcuts-help-transition="cont3xt-shortcuts-slide-long">
          <template #content>
            <span class="cont3xt-shortcuts-content">
              <template
                v-for="(shortcut, index) in shortcuts"
                :key="shortcut.keys">
                <br v-if="index > 0">
                <code>{{ shortcut.keys }}</code> - {{ shortcut.text }}
              </template>
            </span>
          </template>
        </keyboard-shortcuts>
      </div>
    </div>
    <div v-else>
      <cont3xt-upgrade-browser />
    </div>
  </v-app>
</template>

<script>
import { mapGetters } from 'vuex';

import Cont3xtNavbar from '@/utils/Navbar.vue';
import UserService from '@/components/services/UserService';
import LinkService from '@/components/services/LinkService';
import OverviewService from '@/components/services/OverviewService';
import Cont3xtService from '@/components/services/Cont3xtService';
import Cont3xtUpgradeBrowser from '@/components/pages/UpgradeBrowser.vue';
import KeyboardShortcuts from '@common/KeyboardShortcuts.vue';
import AppBanner from '@common/AppBanner.vue';
import { applyServerTheme } from '@common/themes/persistTheme.js';

export default {
  name: 'App',
  components: {
    Cont3xtNavbar,
    KeyboardShortcuts,
    Cont3xtUpgradeBrowser,
    AppBanner
  },
  data: function () {
    return {
      compatibleBrowser: true
    };
  },
  computed: {
    ...mapGetters(['getShiftKeyHold']),
    // keys are physical, so they stay literal -- only the descriptions translate
    shortcuts () {
      return [
        { keys: "'Q'", text: this.$t('cont3xt.shortcuts.focusQuery') },
        { keys: "'T'", text: this.$t('cont3xt.shortcuts.focusStartTime') },
        { keys: "'F'", text: this.$t('cont3xt.shortcuts.focusLinkFilter') },
        { keys: "'V'", text: this.$t('cont3xt.shortcuts.focusViewFilter') },
        { keys: "'O'", text: this.$t('cont3xt.shortcuts.focusOverviewFilter') },
        { keys: "'G'", text: this.$t('cont3xt.shortcuts.focusTag') },
        { keys: "'E'", text: this.$t('cont3xt.shortcuts.toggleCache') },
        { keys: "'R'", text: this.$t('cont3xt.shortcuts.report') },
        { keys: "'L'", text: this.$t('cont3xt.shortcuts.shareLink') },
        { keys: "'S'", text: this.$t('cont3xt.shortcuts.jumpSettings') },
        { keys: "'C'", text: this.$t('cont3xt.shortcuts.jumpCont3xt') },
        { keys: "'A'", text: this.$t('cont3xt.shortcuts.jumpStats') },
        { keys: "'Y'", text: this.$t('cont3xt.shortcuts.jumpHistory') },
        { keys: "'H'", text: this.$t('cont3xt.shortcuts.jumpHelp') },
        { keys: "'<'", text: this.$t('cont3xt.shortcuts.toggleIntegrationPanel') },
        { keys: "'>'", text: this.$t('cont3xt.shortcuts.toggleLinkGroupPanel') },
        { keys: "'shift -'", text: this.$t('cont3xt.shortcuts.collapseRoots') },
        { keys: "'shift +'", text: this.$t('cont3xt.shortcuts.expandRoots') },
        { keys: "'h'", text: this.$t('cont3xt.shortcuts.treeLeft') },
        { keys: "'j'", text: this.$t('cont3xt.shortcuts.treeDown') },
        { keys: "'k'", text: this.$t('cont3xt.shortcuts.treeUp') },
        { keys: "'l'", text: this.$t('cont3xt.shortcuts.treeRight') },
        { keys: "'shift + enter'", text: this.$t('cont3xt.shortcuts.issueSearch') },
        { keys: "'esc'", text: this.$t('cont3xt.shortcuts.escape') },
        { keys: "'?'", text: this.$t('cont3xt.shortcuts.showDialog') }
      ];
    }
  },
  mounted () {
    this.compatibleBrowser = (typeof Object.__defineSetter__ === 'function') &&
      !!String.prototype.includes;

    if (!this.compatibleBrowser) {
      console.log('Incompatible browser, please upgrade!');
    }

    // NOTE: don't need to do anything with the data (the store does it)
    Promise.allSettled([
      Cont3xtService.getIntegrations(),
      UserService.getIntegrationViews()
    ]).then(() => {
      // raise flag to process on-load query parameters like 'submit' once the necessary data is loaded
      this.$store.commit('SET_IMMEDIATE_SUBMISSION_READY', true);
    });
    LinkService.getLinkGroups();
    OverviewService.getOverviews();
    UserService.getUser().then((user) => { this.hydrateThemeFromUser(user); });
    UserService.getRoles();
    UserService.getUserSettings().then((response) => {
      this.$store.commit('SET_SELECTED_OVERVIEW_ID_MAP', response.selectedOverviews ?? {});
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

      // quit if the user is in an input
      if (activeElement && inputs.indexOf(activeElement.tagName.toLowerCase()) !== -1) {
        return;
      }

      // non-shift shortcuts
      if (!this.getShiftKeyHold) {
        switch (e.code) {
        case 'KeyJ':
          // navigate down the indicator result tree
          this.$store.commit('SET_RESULT_TREE_NAVIGATION_DIRECTION', 'down');
          break;
        case 'KeyK':
          // navigate up the indicator result tree
          this.$store.commit('SET_RESULT_TREE_NAVIGATION_DIRECTION', 'up');
          break;
        case 'KeyH':
          // navigate left in the indicator result tree
          this.$store.commit('SET_RESULT_TREE_NAVIGATION_DIRECTION', 'left');
          break;
        case 'KeyL':
          // navigate right in the indicator result tree
          this.$store.commit('SET_RESULT_TREE_NAVIGATION_DIRECTION', 'right');
          break;
        }
        return;
      }

      // shifted shortcuts
      switch (e.code) {
      case 'KeyQ':
        // focus on search expression input
        this.$store.commit('SET_FOCUS_SEARCH', true);
        break;
      case 'KeyT':
        // focus on start time input
        this.$store.commit('SET_FOCUS_START_DATE', true);
        break;
      case 'KeyF':
        // focus on time range selector
        this.$store.commit('SET_FOCUS_LINK_SEARCH', true);
        break;
      case 'KeyV':
        // focus on view dropdown selector
        this.$store.commit('SET_FOCUS_VIEW_SEARCH', true);
        break;
      case 'KeyO':
        // focus on overview dropdown selector
        this.$store.commit('SET_FOCUS_OVERVIEW_SEARCH', true);
        break;
      case 'KeyG':
        // focus on tag input
        this.$store.commit('SET_FOCUS_TAG_INPUT', true);
        break;
      case 'KeyE':
        // toggle cache
        this.$store.commit('SET_TOGGLE_CACHE', true);
        break;
      case 'KeyR':
        // download report
        this.$store.commit('SET_DOWNLOAD_REPORT', true);
        break;
      case 'KeyL':
        // copy share link to clipboard
        this.$store.commit('SET_COPY_SHARE_LINK', true);
        break;
      case 'KeyC':
        // open cont3xt page if not on cont3xt page
        if (this.$route.name !== 'Cont3xt') {
          this.routeTo('/');
        }
        break;
      case 'KeyA':
        // open stats page if not on stats page
        if (this.$route.name !== 'Stats') {
          this.routeTo('/stats');
        }
        break;
      case 'KeyY':
        // open history page if not on history page
        if (this.$route.name !== 'History') {
          this.routeTo('/history');
        }
        break;
      case 'KeyS':
        // open settings page if not on settings page
        if (this.$route.name !== 'Settings') {
          this.routeTo('/settings');
        }
        break;
      case 'KeyH':
        // open help page if not on help page
        if (this.$route.name !== 'Help') {
          this.routeTo('/help');
        }
        break;
      case 'Comma': // (seen as `<`, since shift is required)
        // toggle integration panel
        this.$store.commit('SET_TOGGLE_INTEGRATION_PANEL', true);
        break;
      case 'Period': // (seen as `>`, since shift is required)
        // toggle link groups panel
        this.$store.commit('TOGGLE_LINK_GROUPS_PANEL');
        break;
      case 'Enter':
        // trigger search/refresh
        this.$store.commit('SET_ISSUE_SEARCH', true);
        break;
      case 'Minus':
        // collapse all indicator result tree nodes
        this.$store.commit('SET_COLLAPSE_OR_EXPAND_INDICATOR_ROOTS', { setRootsOpen: false });
        break;
      case 'Equal': // (seen as `+`, since shift is required)
        // expand all indicator result tree nodes
        this.$store.commit('SET_COLLAPSE_OR_EXPAND_INDICATOR_ROOTS', { setRootsOpen: true });
        break;
      }
    });
  },
  methods: {
    shiftHoldChange (val) {
      this.$store.commit('SET_SHIFT_HOLD', val);
    },
    routeTo (url) {
      this.$router.push({
        path: url,
        hash: this.$route.hash,
        query: { ...this.$route.query }
      });
    },
    hydrateThemeFromUser (user) {
      applyServerTheme(user?.settings, (themeId, customTheme) => {
        this.$store.commit('HYDRATE_THEME_FROM_SERVER', { themeId, customTheme });
      });
    }
  }
};
</script>

<style>
/* don't allow the entire page to scroll with the navbar */
body {
  overflow-y: hidden;
}

#app {
  height: 100vh;
}

.cont3xt-shortcuts {
  right: 0;
  top: 140px;
  z-index: 9;
  position: fixed;
  color: rgb(var(--v-theme-info));
  border: rgb(var(--v-theme-outline));
  background: rgb(var(--v-theme-neutral-lighter));
  border-radius: 4px 0 0 4px;
  border-right: none;
  -webkit-box-shadow: 0 0 16px -2px black;
     -moz-box-shadow: 0 0 16px -2px black;
          box-shadow: 0 0 16px -2px black;
}

/* keyboard shortcuts help animation */
.cont3xt-shortcuts-slide-enter-active,
.cont3xt-shortcuts-slide-leave-active {
  transition: all 0.5s ease;
}
.cont3xt-shortcuts-slide-long-enter-active,
.cont3xt-shortcuts-slide-long-leave-active {
  transition: all 1s ease;
}
.cont3xt-shortcuts-slide-enter, .cont3xt-shortcuts-slide-leave,
.cont3xt-shortcuts-slide-long-enter, .cont3xt-shortcuts-slide-long-leave {
  transform: translateX(465px);
}

/* make the shortcut letter the same size/position as the original icon/text it replaces */
.query-shortcut {
  width: 20.563px;
  font-size: 16px;
  color: rgb(var(--v-theme-warning));
}
.lg-query-shortcut {
  width: 18px;
  color: rgb(var(--v-theme-warning));
}
.start-time-shortcut {
  width: 28.359px;
  color: rgb(var(--v-theme-warning));
}
.tag-shortcut {
  color: rgb(var(--v-theme-warning));
  padding-inline: 2px;
}
.side-panel-stub {
  background-color: #ececec;
  color: black;
}
body.dark .side-panel-stub {
  color: #EEE;
  background-color: #555;
}
.cont3xt-shortcuts-content {
  color: rgb(var(--v-theme-info));
}
.cont3xt-shortcuts-content code {
  color: rgb(var(--v-theme-error));
}
</style>
