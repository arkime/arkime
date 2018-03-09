<template>

  <b-navbar
    fixed="top"
    toggleable="md"
    type="dark">

    <b-navbar-toggle
      target="nav_collapse">
    </b-navbar-toggle>

    <b-navbar-brand>
      <!-- TODO this won't always be the stats page -->
      <a href="help#stats"
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
          :href="item.link"
          :active="item.link === 'stats'"
          v-has-permission="item.permission">
          {{ item.title }}
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

      return menu;
    }
  }
};
</script>

<style scoped>
nav.navbar {
  z-index: 5;
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

/* apply theme colors to navbar */
.navbar-dark {
  background-color: var(--color-primary-dark);
  border-color: var(--color-primary-darker);
}
</style>
