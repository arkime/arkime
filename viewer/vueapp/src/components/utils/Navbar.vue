<!--
Copyright Yahoo Inc.
SPDX-License-Identifier: Apache-2.0
-->
<template>
  <span :class="{
    'hide-tool-bars': !showToolBars,
    'show-sticky-sessions-btn': stickySessionsBtn
  }">
    <b-navbar
      fixed="top"
      toggleable="md"
      type="dark"
      :container="false">

      <b-navbar-toggle
        target="nav_collapse">
      </b-navbar-toggle>

      <b-navbar-brand>
        <router-link
          class="me-2"
          :to="{ path: helpLink.href, query: helpLink.query, name: 'Help', hash: helpLink.hash }">
          <div id="helpTooltipContainer">
            <img
              alt="hoot"
              :src="userLogo"
              id="tooltipHelp"
              class="arkime-logo"
              v-if="!shiftKeyHold"
            />
            <div v-else class="arkime-logo mt-1 text-white"><strong>H</strong></div>
            <BTooltip target="tooltipHelp">HOOT! Can I help you? Click me to see the help page</BTooltip>
          </div>
        </router-link>
      </b-navbar-brand>

      <b-collapse is-nav
        id="nav_collapse">

        <b-navbar-nav class="ms-4">
          <template v-for="item of menuOrder">
            <template v-if="user && menu[item] && menu[item].hasPermission && menu[item].hasRole">
              <b-nav-item
                :key="menu[item].link"
                class="cursor-pointer"
                :to="{ path: menu[item].link, query: menu[item].query, name: menu[item].name }"
                :class="{'router-link-active': $route.path === `/${menu[item].link}`}">
                <span v-if="menu[item].hotkey">
                  <p v-for="(text, index) in menu[item].hotkey"
                    :key="text"
                    :class="{'holding-shift':shiftKeyHold && index === menu[item].hotkey.length-1,'shortcut-letter': index === menu[item].hotkey.length-1}">{{ text }}</p>
                </span>
                <p v-else>
                  {{ menu[item].title }}
                </p>
              </b-nav-item>
            </template>
          </template>
        </b-navbar-nav>

        <b-navbar-nav
          class="ms-auto">
          <small>
            <Version :timezone="timezone" />
          </small>
          <router-link
            id="help"
            :to="{ path: helpLink.href, query: helpLink.query, name: 'Help' }">
            <span class="fa fa-lg fa-fw fa-question-circle help-link text-theme-button text-theme-gray-hover">
            </span>
            <BTooltip target="help">HELP!</BTooltip>
          </router-link>
          <e-s-health></e-s-health>
        </b-navbar-nav>
        <span v-if="isAToolBarPage"
           id="toggleTopStuff"
          class="toggle-chevrons text-theme-button text-theme-gray-hover"
          @click="toggleToolBars">
          <span :class="showToolBars ? 'fa fa-chevron-circle-up fa-fw fa-lg' : 'fa fa-chevron-circle-down fa-fw fa-lg'">
          </span>
          <BTooltip target="toggleTopStuff">
            Toggle toolbars and visualization
          </BTooltip>
        </span>

      </b-collapse>
      <Logout size="sm" :base-path="path" class="ms-2 me-2" />
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

export default {
  name: 'ArkimeNavbar',
  components: {
    Logout,
    Version,
    ESHealth
  },
  data: function () {
    return {
      path: this.$constants.PATH,
      menuOrder: [
        'sessions', 'spiview', 'spigraph', 'connections', 'hunt',
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
        sessions: { title: 'Sessions', link: 'sessions', hotkey: ['Sessions'], name: 'Sessions' },
        spiview: { title: 'SPI View', link: 'spiview', hotkey: ['SPI ', 'View'], name: 'Spiview' },
        spigraph: { title: 'SPI Graph', link: 'spigraph', hotkey: ['SPI ', 'Graph'], name: 'Spigraph' },
        connections: { title: 'Connections', link: 'connections', hotkey: ['Connections'], name: 'Connections' },
        files: { title: 'Files', link: 'files', permission: 'hideFiles', reverse: true, name: 'Files' },
        stats: { title: 'Stats', link: 'stats', permission: 'hideStats', reverse: true, name: 'Stats' },
        upload: { title: 'Upload', link: 'upload', permission: 'canUpload', name: 'Upload' },
        roles: { title: 'Roles', link: 'roles', permission: 'canAssignRoles', name: 'Roles' },
        hunt: { title: 'Hunt', link: 'hunt', permission: 'packetSearch', hotkey: ['H', 'unt'], name: 'Hunt' }
      };

      if (!this.$constants.DEMO_MODE) {
        menu.history = { title: 'History', link: 'history', name: 'ArkimeHistory' };
        menu.settings = { title: 'Settings', link: 'settings', name: 'Settings' };
        menu.users = { title: 'Users', link: 'users', role: 'usersAdmin', name: 'Users' };
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
      return ['settings', 'upload', 'help'].every(item => item !== this.activePage);
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

<style>
/* add an H tooltip by the owl but move it down a bit so
   that the links in the navbar are not covered up */
#helpTooltipContainer > div.tooltip {
  top: 12px !important;
}
/* move the arrow up to line up with the owl (since the
   tooltip was moved down) */
#helpTooltipContainer > div.tooltip > div.arrow {
  top: -2px !important;
}
/* make the tooltip smaller */
#helpTooltipContainer > div.tooltip > div.tooltip-inner {
  padding: 0 0.2rem !important;
  color: var(--color-tertiary-lighter) !important;
}
</style>

<style scoped>
nav.navbar {
  z-index: 7;
  max-height: 36px;
  min-height: 36px;
  padding-right: 0.5rem;
}
.navbarOffset {
  padding-top: 36px;
}
a.nav-link {
  max-height: 38px;
  margin-bottom: 2px;
}
.arkime-logo {
  top: 0;
  left: 20px;
  height: 36px;
  position: absolute;
}
/* icon logos (logo in circle) are wider */
.arkime-logo[src*="Icon"] {
  left: 12px;
}
ul.navbar-nav {
  margin-left: 20px;
}
.toggle-chevrons {
  align-items: center;
  cursor: pointer;
  display: flex;
  justify-content: center;
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
/* need this so that styled first letters don't expand the text */
p.shortcut-letter::first-letter {
  color: var(--color-button, #FFF);
}
li.nav-item.router-link-active > a.nav-link p.shortcut-letter::first-letter {
  color: var(--color-button, #FFF);
}
/* style the shortcut letter */
p.shortcut-letter.holding-shift::first-letter {
  color: var(--color-black) !important;
}

/* move the top nav content to the left to accommodate the sticky sessions
 button when the user has the toolbars collapsed and session(s) open */
span.show-sticky-sessions-btn.hide-tool-bars > nav > div.navbar-collapse.collapse {
  margin-right: 32px;
}
</style>
