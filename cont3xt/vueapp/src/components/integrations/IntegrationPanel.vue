<template>
  <span>
    <!-- open search panel on hover button -->
    <div
      class="sidebar-btn"
      @mouseenter="mouseEnterSidebar">
      <div class="mt-3 pt-1">
        <span
          @click="toggleSidebar"
          class="fa fa-chevron-right cursor-pointer"
        />
      </div>
    </div> <!-- /open search panel on hover button -->

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
              <span v-else class="fa fa-chevron-left" />
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
  </span>
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
      sidebarOpen: this.$store.state.sidebarKeepOpen
    };
  },
  computed: {
    ...mapGetters(['getDoableIntegrations', 'getRoles', 'getUser', 'getSortedIntegrations']),
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
  methods: {
    /* page functions ------------------------------------------------------ */
    mouseEnterSidebar () {
      setTimeout(() => {
        this.sidebarOpen = true;
      }, this.hoverDelay || 400);
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
    },
    /* helpers ------------------------------------------------------------- */
    calculateSelectAll (list) {
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
  margin-top: 60px !important;
  height: calc(100vh - 60px);
}

/* sidebar button is full height to the left of the content */
.sidebar-btn {
  left: -3px;
  z-index: 4;
  color: black;
  height: 100%;
  position: fixed;
  padding: 0.2rem;
  margin-top: -5rem;
  background-color: #ececec;
}

/* darken sidebar btn */
body.dark .sidebar-btn {
  color: #EEE;
  background-color: #555;
}

/* margin to flank content associated with sidebar
   transition to side it over when opening sidebar */
.main-content,
.main-content .search-nav {
  margin-left: 1rem;
  transition: 0.5s;
}
/* push over the content if there sidebar is open and not just hovering */
.main-content.with-sidebar,
.main-content.with-sidebar .search-nav {
  margin-left: 252px;
}
</style>
