<!--
Copyright Yahoo Inc.
SPDX-License-Identifier: Apache-2.0
-->
<template>
  <div class="settings-page">
    <!-- sub navbar -->
    <div class="sub-navbar">
      <span class="sub-navbar-title">
        <v-icon
          icon="mdi-cog"
          class="me-1" />
        <span>WISE Settings</span>
      </span>
    </div> <!-- /sub navbar -->

    <v-row no-gutters>
      <!-- navigation -->
      <v-col
        cols="12"
        xl="1"
        lg="2"
        md="2"
        sm="3"
        role="tablist"
        aria-orientation="vertical">
        <v-tabs
          :model-value="visibleTab"
          direction="vertical"
          density="compact"
          color="primary"
          selected-class="font-weight-bold"
          @update:model-value="openView($event)">
          <v-tab value="themes">
            <v-icon
              icon="mdi-brush"
              class="me-1" />
            Themes
          </v-tab>
          <v-tab
            v-if="isAdmin"
            value="banner">
            <v-icon
              icon="mdi-bullhorn"
              class="me-1" />
            {{ $t('settings.banner.title') }}
          </v-tab>
        </v-tabs>
      </v-col> <!-- /navigation -->

      <!-- content -->
      <v-col
        cols="12"
        xl="11"
        lg="10"
        md="10"
        sm="9"
        class="settings-content-pane">
        <div v-if="visibleTab === 'themes'">
          <h1 class="mb-3">
            Themes
          </h1>
          <p class="text-medium-emphasis mb-4">
            Choose a theme or build your own. Themes are saved to your
            account and apply across all Arkime apps.
          </p>
          <ThemePicker
            :model-value="getTheme"
            :themes="themes"
            :custom-theme="getCustomTheme"
            @update:model-value="onThemeChange"
            @update:custom-theme="onCustomThemeChange" />
        </div>
        <div v-if="visibleTab === 'banner' && isAdmin">
          <banner-settings />
        </div>
      </v-col>
    </v-row>
  </div>
</template>

<script>
import { mapGetters } from 'vuex';

import ThemePicker from '@common/ThemePicker.vue';
import BannerSettings from '@common/BannerSettings.vue';
import WiseService from './wise.service';
import { THEMES } from '@common/themes/manifest.js';
import { registerVuetifyTheme } from '@common/themes/registerVuetifyTheme.js';

export default {
  name: 'WiseSettings',
  components: { ThemePicker, BannerSettings },
  data: function () {
    return {
      visibleTab: 'themes',
      themes: THEMES,
      isAdmin: false
    };
  },
  computed: {
    ...mapGetters(['getTheme', 'getCustomTheme'])
  },
  mounted: function () {
    const tab = window.location.hash.replace(/^#/, '');
    if (tab === 'themes' || tab === 'banner') this.visibleTab = tab;
    WiseService.getCurrentUser().then((user) => {
      this.isAdmin = !!(user && (user.roles || []).includes('wiseAdmin'));
    }).catch(() => { /* anonymous / unauthenticated */ });
  },
  methods: {
    openView (tabName) {
      if (this.visibleTab === tabName) return;
      this.visibleTab = tabName;
      this.$router.push({ hash: `#${tabName}` });
    },
    onThemeChange (newThemeId) {
      this.$store.commit('SET_THEME', newThemeId);
    },
    onCustomThemeChange (newCustomTheme) {
      if (!newCustomTheme || typeof newCustomTheme.colors !== 'object' || !newCustomTheme.colors) return;
      const safe = {
        dark: !!newCustomTheme.dark,
        colors: { ...newCustomTheme.colors }
      };
      registerVuetifyTheme(this.$vuetify, 'custom1', safe);
      this.$store.commit('SET_CUSTOM_THEME', safe);
      if (this.getTheme !== 'custom1') {
        this.$store.commit('SET_THEME', 'custom1');
      }
    }
  }
};
</script>

<style scoped>
/* Tighten the vertical tab strip to match viewer's settings nav. */
.v-tab {
  min-height: 28px !important;
  height: 28px !important;
  padding: 0 12px !important;
  font-size: 0.85rem !important;
  justify-content: flex-start !important;
}
</style>
