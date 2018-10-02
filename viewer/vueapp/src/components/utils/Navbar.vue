<template>

  <b-navbar
    fixed="top"
    toggleable="md"
    type="dark">

    <b-navbar-toggle
      target="nav_collapse">
    </b-navbar-toggle>

    <b-navbar-brand>
      <a :href="'help#' + activePage"
        class="cursor-pointer">
        <img src="../../assets/logo.png"
          class="moloch-logo"
          alt="hoot"
        />
        <small class="help-shortcut"
          :class="{'holding-shift': holdingShiftKey}">
          H
        </small>
      </a>
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
                    :class="{'holding-shift':holdingShiftKey && index === menu[item].hotkey.length-1,'shortcut-letter': index === menu[item].hotkey.length-1}">{{ text }}</p>
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
      holdingShiftKey: false,
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
        files: { title: 'Files', link: 'files' },
        stats: { title: 'Stats', link: 'stats' },
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

        // determine if user can view menu item
        // this can't be done with the has-permission directive because
        // a sibling of this component might update the user (Users.vue)
        if (this.user) {
          item.hasPermission = !item.permission ||
            (this.user.hasOwnProperty(item.permission) && this.user[item.permission]);
        }
      }

      return menu;
    },
    activePage: function () {
      for (let page in this.menu) {
        let link = this.menu[page].link;
        if (link === this.$route.path.split('/')[1]) {
          return link;
        }
      }
    },
    user: function () {
      return this.$store.state.user;
    }
  },
  mounted: function () {
    window.addEventListener('keydown', (event) => {
      const activeElement = document.activeElement;
      const inputs = ['input', 'select', 'textarea'];

      // quit if the user is in an input
      if (activeElement && inputs.indexOf(activeElement.tagName.toLowerCase()) !== -1) {
        return;
      }

      if (event.keyCode === 16) { // shift
        this.holdingShiftKey = true;
      }
    });

    window.addEventListener('keyup', (event) => {
      if (event.keyCode === 16) { // shift
        this.holdingShiftKey = false;
      }
    });
  },
  methods: {
    isActive: function (link) {
      return link === this.$route.path.split('/')[1];
    }
  }
};
</script>

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
  margin-top: 16px;
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

/* add an H by the owl */
.help-shortcut {
  visibility: hidden;
  position: absolute;
  top: 0px;
  color: var(--color-tertiary-lighter);
  left: 4px;
  font-size: 18px;
}
.help-shortcut.holding-shift {
  visibility: visible;
}
</style>
