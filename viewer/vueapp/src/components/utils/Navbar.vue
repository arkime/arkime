<!--
Copyright Yahoo Inc.
SPDX-License-Identifier: Apache-2.0
-->
<template>
  <span
    :class="{
      'hide-tool-bars': !showToolBars,
      'show-sticky-sessions-btn': stickySessionsBtn
    }">
    <nav class="navbar navbar-expand fixed-top pe-2 arkime-navbar">

      <router-link
        class="me-2 navbar-brand"
        :to="{ path: helpLink.href, query: helpLink.query, name: 'Help', hash: helpLink.hash }">
        <img
          alt="hoot"
          :src="userLogo"
          class="arkime-logo"
          v-if="!shiftKeyHold">
        <div
          v-else
          class="arkime-logo mt-1 ms-3 text-shortcut"><strong>H</strong></div>
        <v-tooltip activator="parent">{{ $t('navigation.tooltipHelpTip') }}</v-tooltip>
      </router-link>

      <ul class="navbar-nav d-flex flex-row arkime-nav-list">
        <template
          v-for="item of menuOrder"
          :key="item">
          <li
            v-if="user && menu[item] && menu[item].hasPermission && menu[item].hasRole"
            class="nav-item cursor-pointer">
            <router-link
              :to="{ path: menu[item].link, query: menu[item].query, name: menu[item].name }"
              class="nav-link"
              active-class="router-link-active"
              :class="{'router-link-active': $route.path === `/${menu[item].link}`}">
              {{ menu[item].title }}
            </router-link>
          </li>
        </template>
      </ul>

      <div class="ms-auto d-flex flex-row align-items-center">
        <small>
          <Version :timezone="timezone" />
        </small>
        <LanguageSwitcher additional-classes="ms-2" />

        <router-link
          class="ms-2"
          :to="{ path: helpLink.href, query: helpLink.query, name: 'Help' }">
          <span class="fa fa-lg fa-fw fa-question-circle help-link text-theme-button text-theme-gray-hover" />
          <v-tooltip activator="parent">{{ $t('navigation.helpTip') }}</v-tooltip>
        </router-link>

        <e-s-health class="ms-2" />

        <span
          v-if="isAToolBarPage"
          class="toggle-chevrons text-theme-button text-theme-gray-hover ms-2"
          @click="toggleToolBars">
          <span :class="showToolBars ? 'fa fa-chevron-circle-up fa-fw fa-lg' : 'fa fa-chevron-circle-down fa-fw fa-lg'" />
          <v-tooltip activator="parent">{{ $t('navigation.toggleTopStuffTip') }}</v-tooltip>
        </span>

        <Logout
          size="sm"
          :base-path="path"
          class="ms-2 me-2" />
      </div>
    </nav>

    <div class="navbarOffset" />
  </span>
</template>

<script>
import qs from 'qs';
import { mapMutations } from 'vuex';

import ESHealth from './ESHealth.vue';
import Logout from '@common/Logout.vue';
import Version from '@common/Version.vue';
import LanguageSwitcher from '@common/LanguageSwitcher.vue';

export default {
  name: 'ArkimeNavbar',
  components: {
    Logout,
    Version,
    ESHealth,
    LanguageSwitcher
  },
  data: function () {
    return {
      path: this.$constants.PATH,
      menuOrder: [
        'arkime', 'sessions', 'spiview', 'spigraph', 'connections', 'hunt',
        'files', 'stats', 'history', 'upload', 'settings', 'users', 'roles'
      ]
    };
  },
  computed: {
    userLogo: function () {
      if (this.user && this.user.settings.logo) {
        return this.user.settings.logo;
      }
      return 'assets/Arkime_Logo_Mark_White.png';
    },
    menu: function () {
      const menu = {
        arkime: { title: this.$t('navigation.arkime'), link: 'arkime', hotkey: ['Arkime'], name: 'Arkime' },
        sessions: { title: this.$t('navigation.sessions'), link: 'sessions', hotkey: ['Sessions'], name: 'Sessions' },
        spiview: { title: this.$t('navigation.spiview'), link: 'spiview', hotkey: ['SPI ', 'View'], name: 'Spiview' },
        spigraph: { title: this.$t('navigation.spigraph'), link: 'spigraph', hotkey: ['SPI ', 'Graph'], name: 'Spigraph' },
        connections: { title: this.$t('navigation.connections'), link: 'connections', hotkey: ['Connections'], name: 'Connections' },
        files: { title: this.$t('navigation.files'), link: 'files', permission: 'hideFiles', reverse: true, name: 'Files' },
        stats: { title: this.$t('navigation.stats'), link: 'stats', permission: 'hideStats', reverse: true, name: 'Stats' },
        upload: { title: this.$t('navigation.upload'), link: 'upload', permission: 'canUpload', name: 'Upload' },
        roles: { title: this.$t('navigation.roles'), link: 'roles', permission: 'canAssignRoles', name: 'Roles' },
        hunt: { title: this.$t('navigation.hunt'), link: 'hunt', permission: 'packetSearch', hotkey: ['H', 'unt'], name: 'Hunt' }
      };

      if (!this.$constants.DEMO_MODE) {
        menu.history = { title: this.$t('navigation.history'), link: 'history', name: 'ArkimeHistory' };
        menu.settings = { title: this.$t('navigation.settings'), link: 'settings', name: 'Settings' };
        menu.users = { title: this.$t('navigation.users'), link: 'users', role: 'usersAdmin', name: 'Users' };
      }

      // preserve url query parameters
      for (const m in menu) {
        const item = menu[m];

        item.href = `${item.link}?${qs.stringify(this.$route.query)}`;
        // make sure the stored expression is part of the query
        item.query = {
          ...this.$route.query,
          expression: this.$store.state.expression
        };

        // update the start/stop time if they have not been updated
        if ((this.$store.state.time.startTime !== this.$route.query.startTime ||
          this.$store.state.time.stopTime !== this.$route.query.stopTime) &&
          this.$store.state.timeRange === '0') {
          item.query.startTime = this.$store.state.time.startTime;
          item.query.stopTime = this.$store.state.time.stopTime;
          item.query.date = undefined;
        }

        // determine if user can view menu item
        // this can't be done with the has-permission directive because
        // a sibling of this component might update the user (Users.vue)
        if (this.user) {
          item.hasPermission = !item.permission ||
            (this.user[item.permission] !== undefined && this.user[item.permission] && !item.reverse) ||
            (this.user[item.permission] === undefined || (!this.user[item.permission] && item.reverse));
          item.hasRole = !item.role || this.user.roles?.includes(item.role);
        }
      }

      return menu;
    },
    helpLink: function () {
      const helpLink = {
        href: 'help',
        query: {
          ...this.$route.query,
          expression: this.$store.state.expression
        }
      };
      if (this.activePage) {
        helpLink.hash = `#${this.activePage}`;
      }
      return helpLink;
    },
    activePage: function () {
      let activeLink;
      const chosenPath = this.$route.path.split('/')[1];
      for (const page in this.menu) {
        const link = this.menu[page].link;
        if (link === chosenPath) {
          activeLink = chosenPath;
          break;
        }
      }
      // Help page is not in menu options
      if (chosenPath === 'help') {
        activeLink = chosenPath;
      }
      return activeLink;
    },
    isAToolBarPage: function () {
      if (!this.activePage) { return false; }
      return ['settings', 'upload', 'help', 'users'].every(item => item !== this.activePage);
    },
    user: function () {
      return this.$store.state.user;
    },
    showToolBars: function () {
      return this.$store.state.showToolBars;
    },
    shiftKeyHold: function () {
      return this.$store.state.shiftKeyHold;
    },
    stickySessionsBtn: function () {
      return this.$store.state.stickySessionsBtn;
    },
    timezone: function () {
      return this.$store.state?.user?.settings?.timezone || 'gmt';
    }
  },
  methods: {
    isActive: function (link) {
      return link === this.$route.path.split('/')[1];
    },
    ...mapMutations([
      'toggleToolBars'
    ])
  }
};
</script>

<style scoped>
nav.navbar {
  z-index: 7;
}
.navbarOffset {
  padding-top: 36px;
}
/* icon logos (logo in circle) are wider */
.arkime-logo[src*="Icon"] {
  left: 8px;
}
.arkime-logo[src*="Logo"] {
  left: 20px;
}
.toggle-chevrons {
  align-items: center;
  cursor: pointer;
  display: flex;
  justify-content: center;
  margin-top: 1px;
}
.help-link {
  margin-left: 0;
}

.navbar-text {
  color: var(--color-button, #FFF);
}

a.nav-link > a {
  text-decoration: none;
  color: var(--color-button, #FFF);
}
/* shortcut letter styles */
p { /* ::first-letter only works on blocks */
  margin-bottom: -16px;
  display: inline-block;
}
/* style the shortcut letter */
p.shortcut-letter.holding-shift::first-letter {
  color: var(--color-tertiary-lighter) !important;
}
.text-shortcut {
  color: var(--color-tertiary-lighter) !important;
}

/* move the top nav content to the left to accommodate the sticky sessions
 button when the user has the toolbars collapsed and session(s) open */
span.show-sticky-sessions-btn.hide-tool-bars > nav > div.navbar-collapse.collapse {
  margin-right: 32px;
}

/* v7: navbar nav links uppercase + monospace, with thin dividers
   between items so the tabs read as distinct lozenges instead of one
   long string of letters. */
.arkime-navbar .navbar-nav .nav-link {
  text-transform: uppercase;
  font-family: ui-monospace, "SF Mono", Menlo, Consolas, monospace;
  letter-spacing: 0.5px;
  padding-left: 14px;
  padding-right: 14px;
  position: relative;
  transition: color 0.18s ease;
}
.arkime-navbar .navbar-nav .nav-item {
  position: relative;
}
.arkime-navbar .navbar-nav .nav-item + .nav-item::before {
  content: "";
  position: absolute;
  left: 0;
  top: 8px;
  bottom: 8px;
  width: 1px;
  background-color: rgba(255, 255, 255, 0.18);
  pointer-events: none;
  transition: top 0.18s ease, bottom 0.18s ease, width 0.18s ease,
              background-color 0.18s ease;
}

/* underline that grows from center on hover; full-width when active */
.arkime-navbar .navbar-nav .nav-link::after {
  content: "";
  position: absolute;
  left: 50%;
  right: 50%;
  bottom: 4px;
  height: 2px;
  background-color: var(--color-tertiary-lighter, #FFF);
  transition: left 0.18s ease, right 0.18s ease, opacity 0.18s ease;
  opacity: 0;
}
.arkime-navbar .navbar-nav .nav-link:hover::after {
  left: 14px;
  right: 14px;
  opacity: 0.6;
}

/* override global nav.navbar li hover/active backgrounds (which just
   tint the bg with the primary color); we want clean text-driven
   feedback instead. */
.arkime-navbar .navbar-nav .nav-item:hover,
.arkime-navbar .navbar-nav .nav-item:focus,
.arkime-navbar .navbar-nav .nav-item:has(.router-link-active) {
  background-color: transparent !important;
  border-bottom: none !important;
}

/* hover: brighten the text */
.arkime-navbar .navbar-nav .nav-link:hover {
  color: var(--color-tertiary-lighter, #FFF) !important;
}

/* active: brighter text + full underline + the two dividers framing
   the active tab become full-height accents (so the tab reads as
   "bracketed" by the dividers, not just colored in). The active
   underline lives on the .nav-item parent (not on .nav-link) so it
   spans full width and sits flush with the bottom edge of the
   navbar, matching the full-height dividers' bottom. */
.arkime-navbar .navbar-nav .nav-link.router-link-active {
  color: var(--color-tertiary-lighter, #FFF) !important;
  font-weight: 600;
}
.arkime-navbar .navbar-nav .nav-item:has(.router-link-active)::after {
  content: "";
  position: absolute;
  left: 0;
  right: 0;
  bottom: 0;
  height: 2px;
  background-color: var(--color-tertiary-lighter, #FFF);
  pointer-events: none;
}
.arkime-navbar .navbar-nav .nav-item:has(.router-link-active)::before,
.arkime-navbar .navbar-nav .nav-item:has(.router-link-active) + .nav-item::before {
  top: 0;
  bottom: 0;
  width: 2px;
  background-color: var(--color-tertiary-lighter, #FFF);
}

/* push the nav list off the logo */
.arkime-nav-list {
  margin-left: 3rem;
  position: relative;
}

/* left + right outer edge dividers (between-item dividers handle the
   inner ones; these complete the frame) */
.arkime-nav-list::before,
.arkime-nav-list::after {
  content: "";
  position: absolute;
  top: 8px;
  bottom: 8px;
  width: 1px;
  background-color: rgba(255, 255, 255, 0.18);
  pointer-events: none;
  transition: top 0.18s ease, bottom 0.18s ease, width 0.18s ease,
              background-color 0.18s ease;
}
.arkime-nav-list::before { left: 0; }
.arkime-nav-list::after { right: 0; }

/* if the first/last tab is active, light up the matching outer
   edge divider so the active "bracket" stays consistent */
.arkime-nav-list:has(.nav-item:first-child .router-link-active)::before,
.arkime-nav-list:has(.nav-item:last-child .router-link-active)::after {
  top: 0;
  bottom: 0;
  width: 2px;
  background-color: var(--color-tertiary-lighter, #FFF);
}
</style>
