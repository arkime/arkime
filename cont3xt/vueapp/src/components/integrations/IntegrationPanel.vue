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
          role="button"
          @click="toggleSidebar"
          class="sidebar-btn fa fa-chevron-right py-1 pr-1 mt-2 cursor-pointer"
      />
    </div>
    <!-- /open search panel on hover button -->

    <!-- integrations panel -->
    <div @mouseleave="mouseLeaveSidebar">

      <!-- TODO: toby -->
      <!-- <b-sidebar -->
      <!--   no-header -->
      <!--   shadow="lg" -->
      <!--   width="250px" -->
      <!--   v-model="sidebarOpen" -->
      <!--   no-close-on-route-change -->
      <!--   id="integrations-sidebar"> -->
      <div v-if="sidebarOpen" style="width: 250px;" class="d-flex flex-column justify-space-between h-100 pa-1 integration-panel bg-integration-panel">
        <div class="pa-1 d-flex flex-column h-100">
          <!-- header/toggle open -->
          <div class="d-flex flex-row justify-space-between">
            <h4>
              Integrations
            </h4>
            <v-btn
              size="small"
              tabindex="-1"
              variant="text"
              color="primary"
              class="bg-integration-panel"
              @click="toggleSidebar"
              title="Toggle integration panel visibility">
              <span v-if="!sidebarKeepOpen" class="fa fa-chevron-right" />
              <span v-else class="fa fa-lg fa-angle-double-left" />
            </v-btn>
          </div> <!-- /header/toggle open -->
          <hr class="my-1">
          <div class="d-flex justify-space-between">
            <div class="d-inline">
              <ViewSelector size="small"/>
            </div> <!-- /view selector -->
            <v-btn
              size="small"
              tabindex="-1"
              color="success"
              @click="$emit('create-view')"
              v-tooltip:top="'Save these integrations as a view'">
              <span class="fa fa-plus-circle" />
            </v-btn>
          </div>
          <div class="d-flex flex-column flex-grow-1 overflow-y-auto">
            <!-- select integrations -->
            <v-checkbox
              class="mt-2"
              tabindex="-1"
              @click="toggleAll"
              :indeterminate="indeterminate"
              :model-value="allSelected"
            >
              <template #label><strong>Select All</strong></template>
            </v-checkbox>
            <template v-for="integration in getSortedIntegrations"
              :key="integration.key">
              <v-checkbox
                v-if="integration.doable"
                v-model="selectedIntegrations"
                @change="changeView"
                :value="integration.key"
                :label="integration.key"
              />
            </template>
            <v-checkbox
              @click="toggleAll"
              :indeterminate="indeterminate"
              :model-value="allSelected"
            >
              <template #label><strong>Select All</strong></template>
            </v-checkbox>
            <!-- /select integrations -->
          </div>
          <!-- hover delay -->
          <div>
            <v-text-field
              variant="outlined"
              class="small-input mt-2"
              label="Hover Delay"
              v-model="hoverDelay"
            >
              <template #append-inner>
                ms
              </template>
            </v-text-field>
          </div>
        <!-- </b-sidebar> -->
        </div>
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
  emits: ['create-view'],
  data () {
    return {
      viewModal: false,
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
        return this.$store.state.selectedIntegrations;
      },
      set (val) { this.$store.commit('SET_SELECTED_INTEGRATIONS', val); }
    },
    hoverDelay: {
      get () { return this.$store.state.integrationsPanelHoverDelay; },
      set (val) { this.$store.commit('SET_INTEGRATIONS_PANEL_DELAY', val); }
    },
    allSelected () {
      return this.selectedIntegrations?.length >= Object.keys(this.getDoableIntegrations).length;
    },
    indeterminate () {
      const integrationsLength = this.selectedIntegrations?.length;
      return integrationsLength !== 0 && integrationsLength <= Object.keys(this.getDoableIntegrations).length;
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

.integration-panel {
  box-shadow: 4px 0px 5px 0px rgba(0,0,0,0.1);
}
</style>
