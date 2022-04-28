<template>
  <div id="app">
    <div v-if="compatibleBrowser">
      <cont3xt-navbar />
      <router-view class="margin-for-nav-and-progress" />
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
          <code>'H'</code> - jump to the Help page
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
    Cont3xtService.getIntegrations();
    LinkService.getLinkGroups();
    UserService.getUser();
    UserService.getRoles();
    UserService.getIntegrationViews();

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
      if (!this.getShiftKeyHold || (activeElement && inputs.indexOf(activeElement.tagName.toLowerCase()) !== -1)) {
        return;
      }

      switch (e.keyCode) {
      case 81: // q
        // focus on search expression input
        this.$store.commit('SET_FOCUS_SEARCH', true);
        break;
      case 84: // t
        // focus on start time input
        this.$store.commit('SET_FOCUS_START_DATE', true);
        break;
      case 70: // f
        // focus on time range selector
        this.$store.commit('SET_FOCUS_LINK_SEARCH', true);
        break;
      case 69: // e
        // toggle cache
        this.$store.commit('SET_TOGGLE_CACHE', true);
        break;
      case 82: // r
        // download report
        this.$store.commit('SET_DOWNLOAD_REPORT', true);
        break;
      case 76: // l
        // copy share link to clipboard
        this.$store.commit('SET_COPY_SHARE_LINK', true);
        break;
      case 67: // c
        // open cont3xt page if not on cont3xt page
        if (this.$route.name !== 'Cont3xt') {
          this.routeTo('/');
        }
        break;
      case 65: // a
        // open stats page if not on stats page
        if (this.$route.name !== 'Settings') {
          this.routeTo('/settings');
        }
        break;
      case 83: // s
        // open settings page if not on settings page
        if (this.$route.name !== 'Settings') {
          this.routeTo('/settings');
        }
        break;
      case 72: // h
        // open help page if not on help page
        if (this.$route.name !== 'Help') {
          this.routeTo('/help');
        }
        break;
      case 13: // enter
        // trigger search/refresh
        this.$store.commit('SET_ISSUE_SEARCH', true);
        break;
      }
    });
  },
  methods: {
    shiftHoldChange (val) {
      this.$store.commit('SET_SHIFT_HOLD', val);
    }
  }
};
</script>

<style>
.cont3xt-shortcuts {
  right: 0;
  top: 140px;
  z-index: 9;
  position: fixed;
  border-left: none;
  color: var(--info);
  border: var(--color-gray);
  background: var(--color-light);
  border-radius: 4px 0 0 4px;
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

/* make the shortcut letter the same size/position as the icon */
.query-shortcut {
  width: 21px;
  font-size: 16px;
  color: var(--warning);
}
</style>
