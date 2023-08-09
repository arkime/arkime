<template>
  <div id="app">
    <div v-if="compatibleBrowser" class="d-flex flex-column h-100">
      <cont3xt-navbar />
      <router-view />
      <keyboard-shortcuts
        @shift-hold-change="shiftHoldChange"
        shortcuts-class="cont3xt-shortcuts"
        shortcuts-btn-transition="cont3xt-shortcuts-slide"
        shortcuts-help-transition="cont3xt-shortcuts-slide-long">
        <template v-slot:content>
          <code>'Q'</code> - set focus to query bar
          <br>
          <code>'T'</code> - set focus to the start time field
          <br>
          <code>'F'</code> - set focus to the link group search filter
          <br>
          <code>'V'</code> - set focus to the view dropdown search filter
          <br>
          <code>'G'</code> - set focus to the tag input
          <br>
          <code>'E'</code> - toggle cache On/Off
          <br>
          <code>'R'</code> - generate a report of the current results
          <br>
          <code>'L'</code> - copy the share link to the clipboard
          <br>
          <code>'S'</code> - jump to the Settings page
          <br>
          <code>'C'</code> - jump to the Cont3xt search page
          <br>
          <code>'A'</code> - jump to the Stats page
          <br>
          <code>'Y'</code> - jump to the History page
          <br>
          <code>'H'</code> - jump to the Help page
          <br>
          <code>'>'</code> - toggle the link group panel
          <br>
          <code>'-'</code> - collapse all top-level indicator result tree nodes
          <br>
          <code>'+'</code> - expand all top-level indicator result tree nodes
          <br>
          <code>'h'</code> - collapse active indicator result tree node, or navigate left
          <br>
          <code>'j'</code> - navigate down in indicator result tree
          <br>
          <code>'k'</code> - navigate up in indicator result tree
          <br>
          <code>'l'</code> - expand active indicator result tree node, or navigate right
          <br>
          <code>'shift + enter'</code> - issue search/refresh
          <br>
          <code>'esc'</code> - remove focus from any input and close this dialog
          <br>
          <code>'?'</code> - shows you this dialog, but I guess you already knew that
        </template>
      </keyboard-shortcuts>
    </div>
    <div v-else>
      <cont3xt-upgrade-browser />
    </div>
  </div>
</template>

<script>
import { mapGetters } from 'vuex';

import Cont3xtNavbar from '@/utils/Navbar';
import UserService from '@/components/services/UserService';
import LinkService from '@/components/services/LinkService';
import OverviewService from '@/components/services/OverviewService';
import Cont3xtService from '@/components/services/Cont3xtService';
import Cont3xtUpgradeBrowser from '@/components/pages/UpgradeBrowser';
import KeyboardShortcuts from '../../../common/vueapp/KeyboardShortcuts';

export default {
  name: 'App',
  components: {
    Cont3xtNavbar,
    KeyboardShortcuts,
    Cont3xtUpgradeBrowser
  },
  data: function () {
    return {
      compatibleBrowser: true
    };
  },
  computed: {
    ...mapGetters(['getShiftKeyHold'])
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
    UserService.getUser();
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
  color: var(--info);
  border: var(--color-gray);
  background: var(--color-light);
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
  color: var(--warning);
}
.lg-query-shortcut {
  width: 18px;
  color: var(--warning);
}
.start-time-shortcut {
  width: 28.359px;
  color: var(--warning);
}
.tag-shortcut {
  color: var(--warning);
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
</style>
