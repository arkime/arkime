<template>
  <div class="sidebar-container d-flex flex-row" :class="{'sidebar-expand': sidebarKeepOpen}">
    <!-- open search panel on hover button -->
    <div class="side-panel-stub h-100"
         @mouseenter="mouseEnterSidebarStub"
         @mouseleave="mouseLeaveSidebarStub"
    >
        <div
            @click="toggleSidebar"
            class="sidebar-btn fa fa-chevron-right py-1 pr-1 mt-2 cursor-pointer"
        />
    </div>
    <!-- /open search panel on hover button -->

    <!-- integrations panel -->
    <div @mouseleave="mouseLeaveSidebar">
      <b-sidebar
        no-header
        shadow="lg"
        width="250px"
        v-model="sidebarOpen"
        no-close-on-route-change
        id="integrations-sidebar">
        <div class="p-1">
          <!-- header/toggle open -->
          <h4>
            Integrations
            <b-button
              size="sm"
              tabindex="-1"
              variant="link"
              class="float-right"
              @click="toggleSidebar"
              title="Toggle integration panel visibility">
              <span v-if="!sidebarKeepOpen" class="fa fa-chevron-right" />
              <span v-else class="fa fa-lg fa-angle-double-left" />
            </b-button>
          </h4> <!-- /header/toggle open -->
          <hr>
          <div class="d-flex justify-content-between">
            <div class="d-inline">
              <ViewSelector />
            </div> <!-- /view selector -->
            <b-button
              size="sm"
              tabindex="-1"
              variant="success"
              v-b-modal.view-form
              v-b-tooltip.hover.top="'Save these integrations as a view'">
              <span class="fa fa-plus-circle" />
            </b-button>
          </div>
          <br>
          <!-- select integrations -->
          <b-form>
            <b-form-checkbox
              tabindex="-1"
              role="checkbox"
              @change="toggleAll"
              v-model="allSelected"
              :indeterminate="indeterminate">
              <strong>Select All</strong>
            </b-form-checkbox>
            <b-form-checkbox-group
              stacked
              v-model="selectedIntegrations">
              <template
                v-for="integration in getSortedIntegrations">
                <b-form-checkbox
                  @change="changeView"
                  :key="integration.key"
                  :value="integration.key"
                  v-if="integration.doable">
                  {{ integration.key }}
                </b-form-checkbox>
              </template>
            </b-form-checkbox-group>
            <b-form-checkbox
              role="checkbox"
              @change="toggleAll"
              v-model="allSelected"
              :indeterminate="indeterminate">
              <strong>Select All</strong>
            </b-form-checkbox>
          </b-form> <!-- /select integrations -->
        </div>
        <!-- hover delay -->
        <b-row class="m-1">
          <b-input-group
            size="sm"
            append="ms"
            prepend="Hover Delay">
            <b-form-input
              debounce="400"
              v-model="hoverDelay"
            />
          </b-input-group>
        </b-row> <!-- /hover delay -->
      </b-sidebar>
    </div> <!-- integrations panel -->
  </div>
</template>

<script>
import { mapGetters } from 'vuex';

import ViewSelector from '@/components/views/ViewSelector';

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
      'getAllViews', 'getImmediateSubmissionReady', 'getSelectedView'
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
      this.openTimeout = setTimeout(() => {
        this.sidebarOpen = true;
      }, this.hoverDelay || 400);
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
    toggleAll (checked) {
      this.selectedIntegrations = checked ? Object.keys(this.getDoableIntegrations) : [];
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

<style scoped>

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
</style>
