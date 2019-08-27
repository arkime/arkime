<template>

  <b-navbar
    fixed="top"
    toggleable="md"
    type="dark">

    <b-navbar-toggle
      target="nav_collapse">
    </b-navbar-toggle>

    <b-navbar-brand>
      <router-link
        :to="{ path: helpLink.href, query: helpLink.query, params: { nav: true } }">
        <div id="helpTooltipContainer">
          <img src="../../assets/logo.png"
            class="moloch-logo"
            alt="hoot"
            v-b-tooltip.hover
            title="HOOT! Can I help you? Click me to see the help page"
            id="tooltipHelp"
          />
        </div>
      </router-link>
      <b-tooltip :show="shiftKeyHold"
        triggers=""
        target="tooltipHelp"
        placement="leftbottom"
        container="helpTooltipContainer">
        <strong class="help-shortcut">H</strong>
      </b-tooltip>
    </b-navbar-brand>

    <b-collapse is-nav
      id="nav_collapse">

      <b-navbar-nav>
        <template v-for="item of menuOrder">
          <template v-if="user && menu[item] && menu[item].hasPermission">
            <b-nav-item
              :key="menu[item].link"
              class="cursor-pointer">
              <router-link
                :to="{ path: menu[item].link, query: menu[item].query, params: { nav: true } }"
                :class="{'router-link-active': $route.path === `/${menu[item].link}`}">
                <span v-if="menu[item].hotkey">
                  <p v-for="(text, index) in menu[item].hotkey"
                    :key="text"
                    :class="{'holding-shift':shiftKeyHold && index === menu[item].hotkey.length-1,'shortcut-letter': index === menu[item].hotkey.length-1}">{{ text }}</p>
                </span>
                <p v-else>
                  {{ menu[item].title }}
                </p>
              </router-link>
            </b-nav-item>
          </template>
        </template>
      </b-navbar-nav>

      <b-navbar-nav
        class="ml-auto">
        <small class="navbar-text mr-2 text-right">
          v{{ molochVersion }}
        </small>
        <e-s-health></e-s-health>
      </b-navbar-nav>

    </b-collapse>
  </b-navbar>

</template>

<script>
import qs from 'qs';

import ESHealth from './ESHealth';

export default {
  name: 'MolochNavbar',
  components: { ESHealth },
  data: function () {
    return {
      molochVersion: this.$constants.MOLOCH_VERSION,
      menuOrder: [
        'sessions', 'spiview', 'spigraph', 'connections', 'hunt',
        'files', 'stats', 'history', 'upload', 'settings', 'users'
      ]
    };
  },
  computed: {
    menu: function () {
      let menu = {
        sessions: { title: 'Sessions', link: 'sessions', hotkey: ['Sessions'] },
        spiview: { title: 'SPI View', link: 'spiview', hotkey: ['SPI ', 'View'] },
        spigraph: { title: 'SPI Graph', link: 'spigraph', hotkey: ['SPI ', 'Graph'] },
        connections: { title: 'Connections', link: 'connections', hotkey: ['Connections'] },
        files: { title: 'Files', link: 'files', permission: 'hideFiles', reverse: true },
        stats: { title: 'Stats', link: 'stats', permission: 'hideStats', reverse: true },
        upload: { title: 'Upload', link: 'upload', permission: 'canUpload' }
      };

      if (!this.$constants.MOLOCH_MULTIVIEWER) {
        menu.hunt = { title: 'Hunt', link: 'hunt', permission: 'packetSearch', hotkey: ['H', 'unt'] };
      }

      if (!this.$constants.MOLOCH_DEMO_MODE) {
        menu.history = { title: 'History', link: 'history' };
        menu.settings = { title: 'Settings', link: 'settings' };
        menu.users = { title: 'Users', link: 'users', permission: 'createEnabled' };
      }

      // preserve url query parameters
      for (let m in menu) {
        let item = menu[m];
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
            (this.user.hasOwnProperty(item.permission) && this.user[item.permission] && !item.reverse) ||
            (!this.user.hasOwnProperty(item.permission) || (!this.user[item.permission] && item.reverse));
        }
      }

      return menu;
    },
    helpLink: function () {
      let helpLink = {
        href: `help?${qs.stringify(this.$route.query)}`,
        query: {
          ...this.$route.query,
          expression: this.$store.state.expression
        }
      };
      if (this.activePage) {
        helpLink.href += `#${this.activePage}`;
      }
      return helpLink;
    },
    activePage: function () {
      let activeLink;
      for (let page in this.menu) {
        let link = this.menu[page].link;
        if (link === this.$route.path.split('/')[1]) {
          activeLink = link;
          break;
        }
      }
      return activeLink;
    },
    user: function () {
      return this.$store.state.user;
    },
    shiftKeyHold: function () {
      return this.$store.state.shiftKeyHold;
    }
  },
  methods: {
    isActive: function (link) {
      return link === this.$route.path.split('/')[1];
    }
  }
};
</script>

<style>
/* add an H tooltip by the owl but move it down a bit so
   that the links in the navbar are not covered up */
#helpTooltipContainer > div.tooltip {
  top: 16px !important;
}
/* move the arrow up to line up with the owl (since the
   tooltip was moved down) */
#helpTooltipContainer > div.tooltip > div.arrow {
  top: 2px !important;
}
/* make the tooltip smaller */
#helpTooltipContainer > div.tooltip > div.tooltip-inner {
  padding: 0 0.2rem !important;
}
</style>

<style scoped>
nav.navbar {
  z-index: 6;
  max-height: 36px;
  min-height: 36px;
}
.moloch-logo {
  position: absolute;
  height: 41px;
  top: 2px;
}
ul.navbar-nav {
  margin-left: 20px;
}

a.nav-link > a {
  text-decoration: none;
  color: rgba(255, 255, 255, 0.5);
}
a.nav-link:hover > a {
  color: rgba(255, 255, 255, 0.75);
}
a.nav-link > a.router-link-active {
  color: #fff;
}

/* apply theme colors to navbar */
.navbar-dark {
  background-color: var(--color-primary-dark);
  border-color: var(--color-primary-darker);
}

/* shortcut letter styles */
p { /* ::first-letter only works on blocks */
  margin-bottom: -16px;
  display: inline-block;
}
/* need this so that styled first letters don't expand the text */
p.shortcut-letter::first-letter {
  color: rgba(255, 255, 255, 0.5);
}
a.nav-link > a.router-link-active p.shortcut-letter::first-letter {
  color: #FFFFFF !important;
}
/* make sure hover still works */
.nav-link:hover p.shortcut-letter::first-letter {
  color: rgba(255, 255, 255, 0.75) !important;
}
/* style the sortcut letter */
p.shortcut-letter.holding-shift::first-letter {
  color: var(--color-tertiary-lighter) !important;
}
/* color the help shortcut letter in the tooltip */
.help-shortcut {
  color: var(--color-tertiary-lighter);
}
</style>
