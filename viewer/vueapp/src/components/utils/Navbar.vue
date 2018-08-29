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
          alt="hoot">
      </a>
    </b-navbar-brand>

    <b-collapse is-nav
      id="nav_collapse">

      <b-navbar-nav>
        <template v-for="item of menuOrder">
          <template v-if="menu[item]">
            <b-nav-item
              v-if="menu[item].oldPage"
              :key="menu[item].link"
              class="cursor-pointer"
              :href="menu[item].href"
              :active="isActive(menu[item].link)"
              v-has-permission="menu[item].permission">
              {{ menu[item].title }}
            </b-nav-item>
            <b-nav-item
              v-else
              :key="menu[item].link"
              class="cursor-pointer"
              v-has-permission="menu[item].permission">
              <router-link :to="{ path: menu[item].link, query: menu[item].query, params: { nav: true } }"
                :class="{'router-link-active': $route.path === `/${menu[item].link}`}">
                {{ menu[item].title }}
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
        sessions: { title: 'Sessions', link: 'sessions' },
        spiview: { title: 'SPI View', link: 'spiview' },
        spigraph: { title: 'SPI Graph', link: 'spigraph' },
        connections: { title: 'Connections', link: 'connections' },
        files: { title: 'Files', link: 'files' },
        stats: { title: 'Stats', link: 'stats' },
        upload: { title: 'Upload', link: 'upload', permission: 'canUpload' }
      };

      if (!this.$constants.MOLOCH_MULTIVIEWER) {
        menu.hunt = { title: 'Hunt', link: 'hunt', permission: 'packetSearch' };
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
    }
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
</style>
