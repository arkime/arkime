<!--
Copyright Yahoo Inc.
SPDX-License-Identifier: Apache-2.0
-->
<template>
  <span>
    <nav class="arkime-navbar d-flex align-center pe-2">
      <router-link
        to="help"
        class="arkime-navbar-brand"
        exact>
        <img
          :src="logo"
          alt="hoot"
          class="arkime-logo">
        <v-tooltip activator="parent">
          {{ $t('navigation.tooltipHelpTip') }}
        </v-tooltip>
      </router-link>

      <div class="arkime-nav-list d-flex align-center">
        <template
          v-for="item of navItems"
          :key="item.to">
          <v-btn
            v-if="!item.requiresUser || getUser"
            :to="item.to"
            :variant="$route.path === item.to ? 'flat' : 'text'"
            :style="$route.path === item.to ? activePillStyle : null"
            size="small"
            class="arkime-nav-btn"
            exact>
            <span>{{ item.parts.before }}</span><span
              v-if="item.parts.key"
              :class="{'nav-shortcut-active': getShiftKeyHold}">{{ item.parts.key }}</span><span>{{ item.parts.after }}</span>
          </v-btn>
        </template>
      </div>

      <v-spacer />

      <div class="arkime-navbar-actions d-flex align-center">
        <!-- health check inline message -->
        <span
          v-if="healthError"
          class="me-2 text-medium-emphasis">
          {{ healthError || 'Network Error' }} - try
          <a
            tabindex="-1"
            @click="reload"
            class="cursor-pointer">
            reloading the page
          </a>
        </span>

        <!-- version (rainbow gradient via shared Version.vue) -->
        <Version :timezone="timezone" />

        <!-- language switcher -->
        <LanguageSwitcher additional-classes="ms-2" />

        <!-- help button -->
        <v-btn
          to="/help"
          variant="text"
          icon
          size="small"
          density="comfortable"
          class="arkime-help-btn ms-2">
          <v-icon icon="mdi-help-circle" />
          <v-tooltip activator="parent">
            {{ $t('navigation.helpTip') }}
          </v-tooltip>
        </v-btn>

        <!-- admin menu (users/roles/banner) -->
        <AdminMenu
          v-if="adminItems.length"
          :items="adminItems"
          :active-pill-style="activePillStyle"
          additional-classes="ms-2" />

        <Logout
          :base-path="path"
          class="ms-2"
          size="small" />
      </div>
    </nav>

    <div class="navbarOffset" />

    <!-- progress bar -->
    <div class="cont3xt-progress-container">
      <v-progress-linear
        height="8px"
        min="0"
        :max="getLoading.total || 1"
        :striped="getLoading.total != getLoading.received + getLoading.failed"
        :class="{'cursor-help': getLoading.total}"
        :buffer-value="getLoading.failed"
        buffer-color="error"
        :model-value="getLoading.received"
        color="success" />
      <v-tooltip
        activator="parent"
        v-if="getLoading.total">
        {{ `${this.getLoading.received}/${this.getLoading.total} fetched successfully${(this.getLoading.failed > 0) ? `, ${this.getLoading.failed}/${this.getLoading.total} failed` : ''}` }}
      </v-tooltip>
    </div>
  </span>
</template>

<script>
import axios from 'axios';
import { mapGetters, useStore } from 'vuex';

import Logout from '@common/Logout.vue';
import Version from '@common/Version.vue';
import AdminMenu from '@common/AdminMenu.vue';
import LanguageSwitcher from '@common/LanguageSwitcher.vue';
import { useTheme } from 'vuetify';
import { watchEffect } from 'vue';
import { useGetters } from '@/vue3-helpers';
import { registerVuetifyTheme } from '@common/themes/registerVuetifyTheme.js';

// Underline the shortcut letter wherever it lands in the translated
// label. The shortcut is a physical key, so a translation that doesn't
// contain that letter simply gets no hint -- the key still works.
function shortcutParts (title, key) {
  const i = title.toLowerCase().indexOf(key.toLowerCase());
  if (i < 0) { return { before: title, key: '', after: '' }; }
  return { before: title.slice(0, i), key: title[i], after: title.slice(i + 1) };
}

let interval;
const minTimeToWait = 10000;
let timeToWait = minTimeToWait;

export default {
  name: 'Cont3xtNavbar',
  components: {
    Logout,
    Version,
    AdminMenu,
    LanguageSwitcher
  },
  setup () {
    const theme = useTheme();
    const store = useStore();
    const { getTheme, getCustomTheme, getDarkThemeEnabled } = useGetters(store);

    // Re-runs when the theme id or custom palette arrives from the server
    // (HYDRATE_THEME_FROM_SERVER). Register 'custom1' before switching to
    // it so theme.change('custom1') resolves even when the palette only
    // shows up after the async /api/user fetch.
    watchEffect(() => {
      if (getCustomTheme.value && getCustomTheme.value.colors) {
        registerVuetifyTheme({ theme }, 'custom1', getCustomTheme.value);
      }
      theme.change(getTheme.value || 'arkime-light');
      // legacy body.dark hook for any non-Vuetify selectors that still read it
      document.body.classList = getDarkThemeEnabled.value ? ['dark'] : [];
    });
  },
  data: function () {
    return {
      healthError: '',
      path: this.$constants.WEB_PATH,
      logo: 'assets/Arkime_Icon_White.png',
      // active-pill colors -- use button-fg + foreground so the pill
      // flips between themes (white-on-dark in light theme, dark-on-light
      // in dark theme) without us picking specific colors per theme.
      activePillStyle: {
        backgroundColor: 'rgb(var(--v-theme-button-fg))',
        color: 'rgb(var(--v-theme-foreground))'
      }
    };
  },
  computed: {
    ...mapGetters(['getLoading', 'getUser', 'getShiftKeyHold', 'getTheme']),
    timezone () {
      return this.getUser?.settings?.timezone || 'local';
    },
    navItems () {
      // keys match the shifted shortcuts wired up in App.vue
      return [
        { to: '/', title: this.$t('navigation.cont3xt'), key: 'C' },
        { to: '/stats', title: this.$t('navigation.stats'), key: 'A' },
        { to: '/settings', title: this.$t('navigation.settings'), key: 'S' },
        { to: '/history', title: this.$t('navigation.history'), key: 'Y', requiresUser: true }
      ].map(item => ({ ...item, parts: shortcutParts(item.title, item.key) }));
    },
    adminItems () {
      return [
        { title: this.$t('navigation.users'), link: '/users', name: 'Users', show: !!this.getUser?.roles?.includes('usersAdmin') },
        { title: this.$t('navigation.roles'), link: '/roles', name: 'Roles', show: this.getUser?.assignableRoles?.length > 0 },
        { title: this.$t('navigation.banner'), link: '/banner', name: 'Banner', show: !!this.getUser?.roles?.includes('cont3xtAdmin') }
      ].filter(item => item.show).map(item => ({
        ...item, isActive: this.$route.path === item.link
      }));
    }
  },
  mounted: function () {
    this.getHealth();
  },
  methods: {
    /* page functions ------------------------------------------------------ */
    reload () {
      window.location.reload();
    },
    /* helper functions ---------------------------------------------------- */
    getHealth () {
      if (interval) { clearInterval(interval); }

      interval = setInterval(() => {
        axios.get('api/health').then((response) => {
          this.healthError = '';
          if (timeToWait !== minTimeToWait) {
            timeToWait = minTimeToWait;
            this.getHealth();
          }
        }).catch((error) => {
          this.healthError = error.text || error;
          timeToWait = Math.min(timeToWait * 2, 300000); // max 5 minutes between retries
          this.getHealth();
        });
      }, timeToWait);
    }
  },
  beforeUnmount: function () {
    if (interval) { clearInterval(interval); }
  }
};
</script>

<style scoped>
/* navbar shell comes from common/vueapp/arkime-navbar.css; below are
   cont3xt-specific helpers only. */

/* shortcut-letter underline shown when shift is held (so users see
   which key jumps to which page) */
.nav-shortcut-active {
  text-decoration: underline;
  text-decoration-color: rgb(var(--v-theme-warning));
}

/* progress bar sits flush under the navbar */
.cont3xt-progress-container .v-progress-linear {
  border-radius: 0;
}
</style>
