<!--
Copyright Yahoo Inc.
SPDX-License-Identifier: Apache-2.0
-->
<!--
  Explore host: one page, four tabs (Summary / Sessions / SPIView / SPIGraph)
  sharing a single search bar, time range, and visualization. Each tab body is
  a self-contained component that teleports its own subnav into
  #tab-subnav-anchor, its viz into #viz-pin-anchor, and any overlay into
  #explore-overlay-anchor (same pattern as ConnectionsGraph in #4039). The
  active tab comes from $route.meta.tab, so the URL, navbar, tab strip, and
  keyboard shortcuts stay in sync off a single source of truth.
-->
<template>
  <page-layout
    ref="pageLayout"
    class="explore-page">
    <template #chrome>
      <ArkimeCollapsible>
        <div class="page-toolbar">
          <!-- search navbar (keyed per tab so Search re-reads the route in
               created: basePath + per-tab sticky/hide-viz localStorage) -->
          <arkime-search
            :key="activeTab"
            v-bind="searchMeta"
            @change-search="onSearch"
            @set-view="onSetView"
            @set-columns="onSetColumns" /> <!-- /search navbar -->

          <!-- tab strip -->
          <v-tabs
            :model-value="activeTab"
            :height="36"
            density="compact"
            class="explore-tabs ms-2 mt-1">
            <v-tab
              value="arkime"
              :to="tabTo('arkime')">
              {{ $t('navigation.summary') }}
            </v-tab>
            <v-tab
              value="sessions"
              :to="tabTo('sessions')">
              {{ $t('navigation.sessions') }}
            </v-tab>
            <v-tab
              value="spiview"
              :to="tabTo('spiview')">
              {{ $t('navigation.spiview') }}
            </v-tab>
            <v-tab
              value="spigraph"
              :to="tabTo('spigraph')">
              {{ $t('navigation.spigraph') }}
            </v-tab>
          </v-tabs> <!-- /tab strip -->

          <!-- active tab teleports its page-specific subnav here -->
          <div id="tab-subnav-anchor" />
        </div>
      </ArkimeCollapsible>
      <!-- active tab teleports its pinned visualizations here -->
      <div id="viz-pin-anchor" />
    </template>

    <!-- only the active tab body mounts (v-if semantics via :is), so there is
         never more than one teleport into each shared anchor -->
    <component
      :is="activeBodyComponent"
      ref="body"
      @search-meta="searchMeta = $event" />

    <!-- active tab teleports floating overlay content here -->
    <template #overlay>
      <div id="explore-overlay-anchor" />
    </template>
  </page-layout>
</template>

<script>
import ArkimeSearch from '../search/Search.vue';
import PageLayout from '../utils/PageLayout.vue';
import ArkimeCollapsible from '../utils/CollapsibleWrapper.vue';

import ArkimeSummary from '../arkime/Arkime.vue';
import ArkimeSessions from '../sessions/Sessions.vue';
import ArkimeSpiview from '../spiview/Spiview.vue';
import ArkimeSpigraph from '../spigraph/Spigraph.vue';

const TAB_TO_COMPONENT = {
  arkime: 'ArkimeSummary',
  sessions: 'ArkimeSessions',
  spiview: 'ArkimeSpiview',
  spigraph: 'ArkimeSpigraph'
};

export default {
  name: 'Explore',
  components: {
    ArkimeSearch,
    PageLayout,
    ArkimeCollapsible,
    ArkimeSummary,
    ArkimeSessions,
    ArkimeSpiview,
    ArkimeSpigraph
  },
  data: function () {
    return {
      // per-tab props the active body feeds up to the shared search bar
      searchMeta: {}
    };
  },
  computed: {
    activeTab: function () {
      return this.$route.meta?.tab || 'sessions';
    },
    activeBodyComponent: function () {
      return TAB_TO_COMPONENT[this.activeTab] || 'ArkimeSessions';
    }
  },
  watch: {
    activeTab: function () {
      // drop the previous tab's search-bar props until the new body emits
      this.searchMeta = {};
    }
  },
  methods: {
    tabTo: function (tab) {
      return { path: `/${tab}`, query: this.$route.query };
    },
    onSearch: function () {
      this.$refs.body?.onSearch?.();
    },
    onSetView: function (...args) {
      this.$refs.body?.loadNewView?.(...args);
    },
    onSetColumns: function (...args) {
      this.$refs.body?.loadColumns?.(...args);
    }
  }
};
</script>

<style scoped>
/* the tab strip is its own tinted band so it reads as a distinct layer
   between the dark navbar above and the content below; inactive tabs are
   dimmed, the active tab is full-strength, bold, and accent-colored with a
   matching underline */
.explore-tabs {
  min-height: 0;
  background-color: rgb(var(--v-theme-neutral-light));
  border-radius: 4px;
}
.explore-tabs :deep(.v-tab) {
  text-transform: none;
  letter-spacing: normal;
  opacity: 0.7;
}
.explore-tabs :deep(.v-tab--selected) {
  opacity: 1;
  font-weight: 700;
}
/* selected tab uses the theme accent token (foreground-accent — the same
   --v-theme-* the app's .text-theme-accent utility uses, so it tracks each
   theme's accent); !important beats Vuetify's default v-tab text color.
   cover the content node + the slider underline too */
.explore-tabs :deep(.v-tab--selected),
.explore-tabs :deep(.v-tab--selected .v-btn__content) {
  color: rgb(var(--v-theme-foreground-accent)) !important;
}
.explore-tabs :deep(.v-tab__slider) {
  height: 3px;
  opacity: 1;
  background-color: rgb(var(--v-theme-foreground-accent)) !important;
}
/* #explore-overlay-anchor uses display:contents so its box is removed and the
   teleported panel (sticky sessions / connections buttons) lays out + positions
   directly against PageLayout's .page-overlay, exactly as before. PageLayout's
   ".page-overlay > *" pointer-events rule only reaches the anchor (its direct
   child), so re-grant pointer-events to the teleported content here. */
#explore-overlay-anchor {
  display: contents;
}
#explore-overlay-anchor > :deep(*) {
  pointer-events: auto;
}
</style>
