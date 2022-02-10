<template>
  <span>
    <!-- view create form -->
    <create-view-modal
      @update-views="getViews"
    />

    <!-- open search panel on hover button -->
    <div
      class="sidebar-btn"
      @mouseenter="mouseEnterSidebar">
      <div class="mt-3 pt-3">
        <span class="fa fa-chevron-right" />
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
            <!-- view selector -->
            <div class="d-inline">
              <b-dropdown
                size="sm"
                class="view-dropdown"
                text="Integration Views">
                <b-dropdown-item
                  class="small"
                  v-b-modal.view-form
                  v-if="!views.length">
                  No saved views.
                  <br>
                  Click to create one.
                </b-dropdown-item>
                <div class="ml-1 mr-1 mb-2" v-else>
                  <b-input-group size="sm">
                    <template #prepend>
                      <b-input-group-text>
                        <span class="fa fa-search" />
                      </b-input-group-text>
                    </template>
                    <b-form-input
                      debounce="400"
                      v-model="viewSearch"
                    />
                  </b-input-group>
                </div>
                <template v-for="view in filteredViews">
                  <b-tooltip
                    noninteractive
                    :target="view._id"
                    placement="right"
                    boundary="viewport"
                    :key="view._id + '-tooltip'"
                    v-if="view.name.length > 24">
                    {{ view.name }}
                  </b-tooltip>
                  <b-dropdown-item
                    class="small"
                    :id="view._id"
                    :key="view._id"
                    @click="selectView(view)">
                    <div class="d-flex justify-content-between">
                      <div class="d-inline no-wrap no-overflow ellipsis flex-grow-1">
                        <span
                          class="fa fa-share-alt mr-1 cursor-help"
                          v-if="getUser && view.creator !== getUser.userId"
                          v-b-tooltip.hover="`Shared with me by ${view.creator}`"
                        />
                        {{ view.name }}
                      </div>
                      <template v-if="view._editable">
                        <b-button
                          size="xs"
                          variant="danger"
                          class="pull-right ml-1"
                          @click.stop.prevent="deleteView(view._id)"
                          v-b-tooltip.hover.top="'Delete this view.'">
                          <span class="fa fa-trash-o" />
                        </b-button>
                      </template>
                    </div>
                  </b-dropdown-item>
                </template>
              </b-dropdown>
            </div> <!-- /view selector -->
            <b-button
              size="sm"
              variant="success"
              v-b-modal.view-form
              v-b-tooltip.hover.right="'Save these integrations as a view'">
              <span class="fa fa-plus-circle" />
            </b-button>
          </div>
          <b-alert
            size="sm"
            class="mt-2"
            :show="!!viewsError">
            {{ viewsError }}
          </b-alert>
          <br>
          <!-- select integrations -->
          <b-form>
            <b-form-checkbox
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
      </b-sidebar>
    </div> <!-- integrations panel -->
  </span>
</template>

<script>
import { mapGetters } from 'vuex';

import UserService from '@/components/services/UserService';
import CreateViewModal from '@/components/views/CreateViewModal';

export default {
  name: 'IntegrationPanel',
  components: { CreateViewModal },
  props: {
    sidebarHover: Boolean
  },
  data () {
    return {
      views: [],
      viewSearch: '',
      filteredViews: [],
      viewsError: false,
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
    }
  },
  watch: {
    viewSearch (searchTerm) {
      this.filterViews(searchTerm);
    }
  },
  created () {
    this.getViews();
  },
  methods: {
    /* page functions ------------------------------------------------------ */
    mouseEnterSidebar () {
      this.sidebarOpen = true;
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
    selectView (view) {
      this.selectedIntegrations = view.integrations;
    },
    deleteView (id) {
      UserService.deleteIntegrationsView(id).then((response) => {
        this.getViews();
      }).catch((error) => {
        this.viewsError = error.text || error;
      });
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
    },
    getViews () {
      UserService.getIntegrationViews().then((response) => {
        this.viewsError = '';
        this.views = response.views;
        this.filterViews(this.viewSearch);
      }).catch((error) => {
        this.viewsError = error.text || error;
      });
    },
    filterViews (searchTerm) {
      if (!searchTerm) {
        this.filteredViews = JSON.parse(JSON.stringify(this.views));
        return;
      }

      const query = searchTerm.toLowerCase();

      this.filteredViews = this.views.filter((view) => {
        return view.name.toString().toLowerCase().match(query)?.length > 0;
      });
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

.view-dropdown .dropdown-item {
  padding: 0.1rem 0.5rem;
}
.view-dropdown .dropdown-menu {
  width: 240px;
  overflow: hidden;
}
</style>
