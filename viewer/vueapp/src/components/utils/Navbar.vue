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
    <b-navbar
      fixed="top"
      class="pe-2"
      :container="false">

      <b-navbar-brand>
        <router-link
          class="me-2"
          :to="{ path: helpLink.href, query: helpLink.query, name: 'Help', hash: helpLink.hash }">
          <img
            alt="hoot"
            :src="userLogo"
            id="tooltipHelp"
            class="arkime-logo"
            v-if="!shiftKeyHold">
          <div
            v-else
            class="arkime-logo mt-1 ms-3 text-shortcut"><strong>H</strong></div>
          <BTooltip target="tooltipHelp">{{ $t('navigation.tooltipHelpTip') }}</BTooltip>
        </router-link>
      </b-navbar-brand>

      <b-navbar-nav class="ms-4">
        <template
          v-for="item of menuOrder"
          :key="item">
          <template v-if="user && menu[item] && menu[item].hasPermission && menu[item].hasRole">
            <!-- TODO i18n redo hotkey highlighting -->
            <b-nav-item
              :key="menu[item].link"
              class="cursor-pointer"
              :to="{ path: menu[item].link, query: menu[item].query, name: menu[item].name }"
              :class="{'router-link-active': $route.path === `/${menu[item].link}`}">
              {{ menu[item].title }}
            </b-nav-item>
          </template>
        </template>
      </b-navbar-nav>

      <b-navbar-nav
        class="ms-auto">
        <small>
          <Version :timezone="timezone" />
        </small>
        <LanguageSwitcher additional-classes="ms-2" />
        <router-link
          id="help"
          :to="{ path: helpLink.href, query: helpLink.query, name: 'Help' }">
          <span class="fa fa-lg fa-fw fa-question-circle help-link text-theme-button text-theme-gray-hover" />
          <BTooltip target="help"><span v-i18n-btip="'navigation.'" /></BTooltip>
        </router-link>
        <e-s-health />
      </b-navbar-nav>

      <span
        v-if="isAToolBarPage"
        id="toggleTopStuff"
        class="toggle-chevrons text-theme-button text-theme-gray-hover"
        @click="toggleToolBars">
        <span :class="showToolBars ? 'fa fa-chevron-circle-up fa-fw fa-lg' : 'fa fa-chevron-circle-down fa-fw fa-lg'" />
        <BTooltip target="toggleTopStuff"><span v-i18n-btip="'navigation.'" /></BTooltip>
      </span>

      <Logout
        size="sm"
        :base-path="path"
        class="ms-2 me-2" />
    </b-navbar>
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
        'sessions', 'spiview', 'spigraph', 'connections', 'summary', 'hunt',
        'files', 'stats', 'history', 'upload', 'settings', 'users', 'roles'
      ]
    };
  },
  computed: {
    userLogo: function () {
      if (this.user && this.user.settings.logo && this.user.settings.logo) {
        return this.user.settings.logo;
      }
      return 'assets/Arkime_Logo_Mark_White.png';
    },
    menu: function () {
      const menu = {
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
  margin-left: 10px;
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
</style>
