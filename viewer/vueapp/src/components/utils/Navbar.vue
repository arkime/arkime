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
        <b-nav-item
          v-for="item of menu"
          :key="item.link"
          class="cursor-pointer"
          v-has-permission="item.permission">
          <router-link :to="{ path: item.link, query: item.query, params: { nav: true } }"
            :class="{'router-link-active': $route.path === `/${item.link}`}">
            {{ item.title }}
          </router-link>
        </b-nav-item>
      </b-navbar-nav>

      <b-navbar-nav
        class="ml-auto">
        <small class="navbar-text mr-2">
          v{{ molochVersion }}
        </small>
        <e-s-health></e-s-health>
      </b-navbar-nav>

    </b-collapse>
  </b-navbar>

</template>

<script>
import ESHealth from './ESHealth';

export default {
  name: 'MolochNavbar',
  components: { ESHealth },
  data: function () {
    return {
      molochVersion: this.$constants.MOLOCH_VERSION
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

      if (!this.$constants.MOLOCH_DEMO_MODE) {
        menu.history = { title: 'History', link: 'history' };
        menu.settings = { title: 'Settings', link: 'settings' };
        menu.users = { title: 'Users', link: 'users', permission: 'createEnabled' };
      }

      // preserve url query parameters
      for (let m in menu) {
        let item = menu[m];
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
