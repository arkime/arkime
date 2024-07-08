<!--
Copyright Yahoo Inc.
SPDX-License-Identifier: Apache-2.0
-->
<template>
  <div class="sidebar-container d-flex flex-row" :class="{'sidebar-expand': sidebarKeepOpen}">
    <!-- open search panel on hover button -->
    <div class="side-panel-stub h-100"
         @mouseenter="mouseEnterSidebarStub"
         @mouseleave="mouseLeaveSidebarStub"
         v-if="!sidebarOpen"
    >
        <div
            @click="toggleSidebar"
            class="sidebar-btn fa fa-chevron-right py-1 pr-1 mt-2 cursor-pointer"
        />
    </div>
    <!-- /open search panel on hover button -->

    <!-- integrations panel -->
    <div @mouseleave="mouseLeaveSidebar">
      <!-- <b-sidebar -->
      <!--   no-header -->
      <!--   shadow="lg" -->
      <!--   width="250px" -->
      <!--   v-model="sidebarOpen" -->
      <!--   no-close-on-route-change -->
      <!--   id="integrations-sidebar"> -->
      <div v-if="sidebarOpen" style="width: 250px;">
        <div class="pa-1">
          <!-- header/toggle open -->
          <h4>
            Integrations
            <v-btn
              size="small"
              tabindex="-1"
              variant="link"
              class="float-right"
              @click="toggleSidebar"
              title="Toggle integration panel visibility">
              <span v-if="!sidebarKeepOpen" class="fa fa-chevron-right" />
              <span v-else class="fa fa-lg fa-angle-double-left" />
            </v-btn>
          </h4> <!-- /header/toggle open -->
          <hr>
          <div class="d-flex justify-content-between">
            <div class="d-inline">
              <ViewSelector />
            </div> <!-- /view selector -->
            <v-btn
              size="sm"
              tabindex="-1"
              variant="success"
              v-b-modal.view-form
              v-tooltip:top="'Save these integrations as a view'">
              <span class="fa fa-plus-circle" />
            </v-btn>
          </div>
          <!-- select integrations -->
          <v-checkbox
            tabindex="-1"
            role="checkbox"
            slim
            density="compact"
            @click="toggleAll"
            :model-value="allSelected"
          >
            <!-- TODO: toby add back indeterminate state (need mdi icons) -->
            <!-- indeterminate-icon="fa fa-square" -->
            <!-- :indeterminate="indeterminate" -->
            <template #label><strong>Select All</strong></template>
          </v-checkbox>
          <template v-for="integration in getSortedIntegrations"
            :key="integration.key">
            <v-checkbox
              v-if="integration.doable"
              role="checkbox"
              slim
              density="compact"
              v-model="selectedIntegrations"
              @change="changeView"
              :value="integration.key"
              :label="integration.key"
            />
          </template>
          <v-checkbox
            role="checkbox"
            slim
            density="compact"
            @click="toggleAll"
            :model-value="allSelected"
          >
            <!-- TODO: toby add back indeterminate state (need mdi icons) -->
            <!-- indeterminate-icon="fa fa-square" -->
            <!-- :indeterminate="indeterminate" -->
            <template #label><strong>Select All</strong></template>
          </v-checkbox>
           <!-- /select integrations -->
        </div>
        <!-- hover delay -->
        <!-- TODO: toby debounce 400ms removed -->
        <v-text-field
          variant="outlined"
          label="Hover Delay"
          v-model="hoverDelay"
        >
          <template #append-inner>
            ms
          </template>
        </v-text-field>
      <!-- </b-sidebar> -->
      </div>
    </div> <!-- integrations panel -->
  </div>
</template>

<script>
import { mapGetters } from 'vuex';

import ViewSelector from '@/components/views/ViewSelector.vue';

export default {
  name: 'IntegrationPanel',
  components: { ViewSelector },
  props: {
    sidebarHover: Boolean
  },
  data () {
    return {
      allSelected: false,
      indeterminate: false,
      sidebarOpen: this.$store.state.sidebarKeepOpen,
      openTimeout: undefined
    };
  },
  computed: {
    ...mapGetters([
      'getDoableIntegrations', 'getRoles', 'getUser', 'getSortedIntegrations',
      'getAllViews', 'getImmediateSubmissionReady', 'getSelectedView', 'getToggleIntegrationPanel'
    ]),
    sidebarKeepOpen: {
      get () {
        return this.$store.state.sidebarKeepOpen;
      },
      set (newValue) {
        this.$store.commit('SET_SIDEBAR_KEEP_OPEN', newValue);
      }
    },
    selectedIntegrations: {
      get () {
        this.calculateSelectAll(this.$store.state.selectedIntegrations);
        return this.$store.state.selectedIntegrations;
      },
      set (val) { this.$store.commit('SET_SELECTED_INTEGRATIONS', val); }
    },
    hoverDelay: {
      get () { return this.$store.state.integrationsPanelHoverDelay; },
      set (val) { this.$store.commit('SET_INTEGRATIONS_PANEL_DELAY', val); }
    }
  },
  watch: {
    getToggleIntegrationPanel (val) {
      if (val) { this.toggleSidebar(); }
    },
    getDoableIntegrations (newVal) {
      // forces initialization of selectedIntegrations when without persisted storage (ex. new browser/incognito)
      if (this.selectedIntegrations == null) {
        this.selectedIntegrations = Object.keys(newVal);
      }
    },
    getImmediateSubmissionReady () {
      // view is matched to selectedIntegrations following mount (once integrations/views are properly loaded)
      this.changeView(this.selectedIntegrations);
    }
  },
  methods: {
    /* page functions ------------------------------------------------------ */
    mouseEnterSidebarStub () {
      // TODO: toby -- reinstate timeout-based opening?
      // this.openTimeout = setTimeout(() => {
      //   this.sidebarOpen = true;
      // }, this.hoverDelay || 400);
    },
    mouseLeaveSidebarStub () {
      // cancel the timeout to open sidebar if the user leaves the stub before it opens
      if (this.openTimeout != null) {
        clearTimeout(this.openTimeout);
      }
    },
    mouseLeaveSidebar () {
      if (!this.sidebarKeepOpen) {
        this.sidebarOpen = false;
      }
    },
    toggleSidebar () {
      this.sidebarKeepOpen = !this.sidebarKeepOpen;
      this.sidebarOpen = this.sidebarKeepOpen;
    },
    toggleAll () {
      this.calculateSelectAll();
      this.selectedIntegrations = !this.allSelected ? Object.keys(this.getDoableIntegrations) : [];
      this.changeView(this.selectedIntegrations);
    },
    changeView (newSelectedIntegrations) {
      // set the selected view to none/all if that is the case, otherwise, clear the selected view
      const getViewByIdOrUndefined = (viewID) => {
        return this.getAllViews.find(view => view._id === viewID);
      };
      const selectView = (() => {
        if (newSelectedIntegrations.length === 0) {
          return getViewByIdOrUndefined('none');
        }
        if (newSelectedIntegrations.length === Object.keys(this.getDoableIntegrations).length) {
          return getViewByIdOrUndefined('all');
        }
        return undefined;
      })();

      this.$store.commit('SET_SELECTED_VIEW', selectView);
    },
    /* helpers ------------------------------------------------------------- */
    calculateSelectAll (list) {
      if (list == null) {
        return; // do not try to get info from list until it has been loaded or defaulted
      }

      if (list.length === 0) {
        this.allSelected = false;
        this.indeterminate = false;
      } else if (list.length === Object.keys(this.getDoableIntegrations).length) {
        this.allSelected = true;
        this.indeterminate = false;
      } else {
        this.allSelected = false;
        this.indeterminate = true;
      }
    }
  }
};
</script>

<style>
/* margin for navbar and progress bar height */
#integrations-sidebar {
  margin-top: 62px !important;
  height: calc(100vh - 62px);
}
</style>

<style >

/* width-having container with transition to play nice with the rest of the page */
.sidebar-container {
  transition: min-width 0.5s;
  min-width: 16px;
}
.sidebar-expand {
  min-width: 252px !important;
}

.sidebar-btn {
  /* fix obscured hit-box */
  z-index: 4;
  position: relative;
  padding-left: 2px;
}

/* TODO: toby remove? */
.v-checkbox .v-selection-control {
  min-height: revert !important;
}
</style>
