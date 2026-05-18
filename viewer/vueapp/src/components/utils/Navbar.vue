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
    <nav class="arkime-navbar d-flex align-center pe-2">

      <router-link
        :to="{ path: helpLink.href, query: helpLink.query, name: 'Help', hash: helpLink.hash }"
        class="arkime-navbar-brand me-2">
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

      <div class="arkime-nav-list d-flex align-center">
        <template
          v-for="item of menuOrder"
          :key="item">
          <v-btn
            v-if="user && menu[item] && menu[item].hasPermission && menu[item].hasRole"
            :to="{ path: menu[item].link, query: menu[item].query, name: menu[item].name }"
            :active="menu[item].isActive"
            :variant="menu[item].isActive ? 'flat' : 'text'"
            :style="menu[item].isActive ? activePillStyle : null"
            size="small"
            class="arkime-nav-btn">
            {{ menu[item].title }}
          </v-btn>
        </template>
      </div>

      <v-spacer />

      <div class="arkime-navbar-actions d-flex align-center">
        <Version :timezone="timezone" />
        <LanguageSwitcher additional-classes="ms-2" />

        <v-btn
          :to="{ path: helpLink.href, query: helpLink.query, name: 'Help' }"
          variant="text"
          icon
          size="small"
          density="comfortable"
          class="ms-2">
          <span class="fa fa-fw fa-question-circle help-link text-theme-button text-theme-gray-hover" />
          <v-tooltip activator="parent">{{ $t('navigation.helpTip') }}</v-tooltip>
        </v-btn>

        <e-s-health class="ms-2" />

        <v-btn
          v-if="isAToolBarPage"
          variant="text"
          icon
          size="small"
          density="comfortable"
          class="ms-2 toggle-chevrons text-theme-button text-theme-gray-hover"
          @click="toggleToolBars">
          <span :class="showToolBars ? 'fa fa-chevron-circle-up fa-fw' : 'fa fa-chevron-circle-down fa-fw'" />
          <v-tooltip activator="parent">{{ $t('navigation.toggleTopStuffTip') }}</v-tooltip>
        </v-btn>

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
      ],
      // active-pill colors -- use Arkime CSS vars so the pill flips
      // between themes (white-on-dark in light theme, dark-on-light in
      // dark theme) without us picking specific colors per theme.
      activePillStyle: {
        backgroundColor: 'rgb(var(--v-theme-button-fg))',
        color: 'rgb(var(--v-theme-foreground))'
      }
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

        item.isActive = this.$route.path === `/${item.link}`;
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
/* navbar shell -- replaces what Bootstrap's .navbar + .fixed-top used
   to give us, plus the global nav.navbar rules from common.css /
   overrides.css that no longer match without the .navbar class. */
.arkime-navbar {
  position: fixed;
  top: 0;
  left: 0;
  right: 0;
  height: 36px;
  z-index: 7;
  background-color: rgb(var(--v-theme-primary-dark));
  color: rgb(var(--v-theme-button-fg));
  padding-left: 0.5rem;
}
.navbarOffset {
  padding-top: 36px;
}

/* brand (logo) -- router-link, intentionally narrow; the 40px-tall
   logo is absolutely positioned relative to .arkime-navbar so it
   overhangs the 36px bar. .arkime-nav-list margin-left clears it. */
.arkime-navbar-brand {
  display: inline-block;
  height: 36px;
  flex-shrink: 0;
  text-decoration: none;
}
.arkime-logo {
  position: absolute;
  height: 40px;
  top: 2px;
}
/* Wide Logo_* assets overhang the 36px bar by design. Square Icon_*
   assets are 1:1, so the 40px treatment makes them oversized inside
   the navbar -- shrink them to fit the bar height. */
.arkime-logo[src*="Icon"] {
  height: 30px;
  top: 3px;
  left: 8px;
}
.arkime-logo[src*="Logo"] { left: 20px; }

/* push the nav list off the logo */
.arkime-nav-list {
  margin-left: 3rem;
}

/* nav buttons -- typography only. Pill bg + text color handled by
   :variant + :style on the v-btn (theme-adaptive via CSS vars). */
.arkime-navbar :deep(.arkime-nav-btn) {
  text-transform: uppercase;
  font-family: ui-monospace, "SF Mono", Menlo, Consolas, monospace;
  font-size: 0.85rem;
  font-weight: 500;
  letter-spacing: 0.4px;
  margin: 0;
  height: 26px !important;
  padding: 0 12px !important;
  border-radius: 5px;
}

/* shortcut letter (shown in place of logo when shift is held) */
.text-shortcut {
  color: rgb(var(--v-theme-tertiary-lighter)) !important;
}

/* right-side action group: fill the navbar height and force every
   child (text, FA-icon spans, v-btns, Logout, LanguageSwitcher) to
   center its content vertically. Without this each item aligns by
   its own baseline / box height and they drift visually. */
.arkime-navbar-actions {
  height: 100%;
}
.arkime-navbar-actions > * {
  display: flex;
  align-items: center;
  height: 28px;
}

/* uniform icon size + tight line-height so the glyph fills its own
   box exactly (no leading shifting it off-center). :deep() reaches
   FA spans in ESHealth.vue and Logout.vue (different scope). */
.arkime-navbar-actions :deep(.fa) {
  font-size: 22px;
  line-height: 1;
}

/* ESHealth's info icon lives in a bare span chain (no v-btn wrapper),
   so its glyph aligns on the text baseline instead of geometrically
   centered like the v-btn icons. Nudge it up to match the others. */
.arkime-navbar-actions :deep(.fa-info-circle) {
  transform: translateY(-2px);
}

/* Version link: rainbow gradient text via background-clip, bold, no
   underline. -webkit-text-fill-color: transparent overrides any
   existing color !important from Vuetify utility classes. */
.arkime-navbar-actions :deep(.navbar-text) {
  text-decoration: none;
  font-family: ui-monospace, "SF Mono", Menlo, Consolas, monospace;
  font-weight: 700;
  font-size: 0.95rem;
  white-space: nowrap;
  background: linear-gradient(
    90deg,
    #FF8A95 0%,
    #FFB36B 17%,
    #FFE066 33%,
    #8AE890 50%,
    #7BCEFF 67%,
    #B69DFF 83%,
    #FF9DD8 100%
  );
  background-clip: text;
  -webkit-background-clip: text;
  -webkit-text-fill-color: rgb(var(--v-theme-button-fg));
  transition: -webkit-text-fill-color 0.4s ease;
}
.arkime-navbar-actions :deep(.navbar-text:hover) {
  -webkit-text-fill-color: transparent;
}

/* Language picker: smaller / more compact than the Bootstrap btn-sm
   defaults to better fit the navbar. */
.arkime-navbar-actions :deep(.btn-language) {
  padding: 2px 6px;
  font-size: 0.8rem;
  min-height: 0;
  height: 22px;
  line-height: 1;
}

/* misc utilities still used by the icon row on the right */
.toggle-chevrons {
  cursor: pointer;
}
.help-link {
  margin-left: 0;
}
</style>
