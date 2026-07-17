<!--
Copyright Yahoo Inc.
SPDX-License-Identifier: Apache-2.0
-->
<template>
  <span>
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
          <v-icon
            icon="mdi-help-circle"
            class="help-link text-theme-button text-theme-gray-hover" />
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
          <v-icon :icon="showToolBars ? 'mdi-chevron-up-circle' : 'mdi-chevron-down-circle'" />
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
        'arkime', 'sessions', 'spiview', 'spigraph', 'hunt',
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
/* navbar shell + utility classes live in common/vueapp/arkime-navbar.css
   so every app (viewer/cont3xt/parliament/wise) stays in lockstep on
   look. Only viewer-specific helpers below. */
.toggle-chevrons {
  cursor: pointer;
}
.help-link {
  margin-left: 0;
}
</style>
