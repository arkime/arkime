<template>
  <span>
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
            <!-- TODO ECR overflow panel or truncate large names with tooltip? -->
            <!-- view selector -->
            <div class="d-inline">
              <b-dropdown
                size="sm"
                class="view-dropdown"
                text="Integration Views">
                <b-dropdown-item
                  class="small"
                  v-if="!views.length"
                  @click="toggleViewForm">
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
                <b-dropdown-item
                  class="small"
                  :key="view.id"
                  v-for="view in filteredViews"
                  @click="selectView(view)">
                  {{ view.name }}
                  <b-button
                    size="xs"
                    variant="danger"
                    class="pull-right ml-1"
                    @click.stop.prevent="deleteView(view.id)"
                    v-b-tooltip.hover.top="'Delete this view.'">
                    <span class="fa fa-trash-o" />
                  </b-button>
                  <b-button
                    size="xs"
                    variant="warning"
                    class="pull-right ml-1"
                    @click.stop.prevent="editView(view)"
                    v-b-tooltip.hover.top="'Edit this view.'">
                    <span class="fa fa-edit" />
                  </b-button>
                </b-dropdown-item>
              </b-dropdown>
            </div> <!-- /view selector -->
            <!-- TODO ECR position of this tooltip is annoying -->
            <b-button
              size="sm"
              @click="toggleViewForm"
              :variant="viewForm ? 'warning': 'success'"
              v-b-tooltip.hover="'Save these integrations as a view'">
              <span v-if="!viewForm" class="fa fa-save" />
              <span v-else class="fa fa-ban" />
            </b-button>
          </div>
          <b-alert
            size="sm"
            class="mt-2"
            :show="!!viewsError">
            {{ viewsError }}
          </b-alert>
          <hr>
          <!-- create view form -->
          <b-form v-if="viewForm">
            <b-input-group size="sm">
              <template #prepend>
                <b-input-group-text>
                  View Name
                </b-input-group-text>
              </template>
              <b-form-input
                v-model="newView.name"
                :state="!!newView.length"
              />
            </b-input-group>
            <small
              class="text-danger"
              v-if="viewFormError">
              {{ viewFormError }}
            </small>
            <div class="d-flex justify-content-between mt-1">
              <b-dropdown
                size="sm"
                text="Who Can View"
                class="roles-dropdown mb-2">
                <b-dropdown-form>
                  <b-form-checkbox-group
                    v-model="newView.viewRoles">
                    <b-form-checkbox
                      v-for="role in getRoles"
                      :value="role.value"
                      :key="role.value">
                      {{ role.text }}
                      <span
                        v-if="role.userDefined"
                        class="fa fa-user cursor-help ml-2"
                        v-b-tooltip.hover="'User defined role'"
                      />
                    </b-form-checkbox>
                    <template v-for="role in newView.viewRoles">
                      <b-form-checkbox
                        :key="role"
                        :value="role"
                        v-if="!getRoles.find(r => r.value === role)">
                        {{ role }}
                        <span
                          class="fa fa-times-circle cursor-help ml-2"
                          v-b-tooltip.hover="'This role no longer exists'"
                        />
                      </b-form-checkbox>
                    </template>
                  </b-form-checkbox-group>
                </b-dropdown-form>
              </b-dropdown>
              <b-dropdown
                size="sm"
                text="Who Can View"
                class="roles-dropdown mb-2">
                <b-dropdown-form>
                  <b-form-checkbox-group
                    v-model="newView.editRoles">
                    <b-form-checkbox
                      v-for="role in getRoles"
                      :value="role.value"
                      :key="role.value">
                      {{ role.text }}
                      <span
                        v-if="role.userDefined"
                        class="fa fa-user cursor-help ml-2"
                        v-b-tooltip.hover="'User defined role'"
                      />
                    </b-form-checkbox>
                    <template v-for="role in newView.editRoles">
                      <b-form-checkbox
                        :key="role"
                        :value="role"
                        v-if="!getRoles.find(r => r.value === role)">
                        {{ role }}
                        <span
                          class="fa fa-times-circle cursor-help ml-2"
                          v-b-tooltip.hover="'This role no longer exists'"
                        />
                      </b-form-checkbox>
                    </template>
                  </b-form-checkbox-group>
                </b-dropdown-form>
              </b-dropdown>
            </div>
            <div class="d-flex justify-content-between align-items-center">
              <b-button
                size="sm"
                variant="warning"
                @click="toggleViewForm"
                v-b-tooltip.hover="'Cancel'">
                <span class="fa fa-ban" />
              </b-button>
              <span class="fa fa-question-circle fa-lg cursor-help"
                v-b-tooltip.hover="'Uses the currently selected integrations below'"
              />
              <b-button
                size="sm"
                variant="success"
                @click="saveView"
                v-b-tooltip.hover="'Save'">
                <span class="fa fa-save" />
              </b-button>
            </div>
            <hr>
          </b-form> <!-- /create view form -->
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
                v-for="integration in sortedIntegrations">
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

const defaultView = {
  name: '',
  editRoles: [],
  viewRoles: [],
  integrations: []
};

export default {
  name: 'IntegrationPanel',
  props: {
    sidebarHover: Boolean
  },
  data () {
    return {
      views: [],
      viewSearch: '',
      viewForm: false,
      filteredViews: [],
      viewsError: false,
      viewFormError: '',
      allSelected: false,
      indeterminate: false,
      sidebarOpen: this.$store.state.sidebarKeepOpen,
      newView: { ...defaultView, integrations: this.$store.state.selectedIntegrations }
    };
  },
  computed: {
    ...mapGetters(['getIntegrations', 'getRoles']),
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
    sortedIntegrations () {
      const integrations = [];
      for (const integration in this.$store.state.integrations) {
        integrations.push({ ...this.$store.state.integrations[integration], key: integration });
      }
      integrations.sort((a, b) => { return a.key.localeCompare(b.key); });
      return integrations;
    }
  },
  watch: {
    viewSearch (searchTerm) {
      if (!searchTerm) {
        this.filteredViews = JSON.parse(JSON.stringify(this.views));
        return;
      }

      const query = searchTerm.toLowerCase();

      this.filteredViews = this.views.filter((view) => {
        return view.name.toString().toLowerCase().match(query)?.length > 0;
      });
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
      this.selectedIntegrations = checked ? Object.keys(this.getIntegrations) : [];
    },
    toggleViewForm () {
      this.editMode = false;
      this.viewForm = !this.viewForm;
      this.viewFormError = '';
      this.newView = { ...defaultView, integrations: this.selectedIntegrations };
    },
    selectView (view) {
      this.selectedIntegrations = view.integrations;
    },
    saveView () {
      if (!this.newView.name) {
        this.viewFormError = 'Name required';
        return;
      }

      const view = { ...this.newView, integrations: this.selectedIntegrations };

      if (this.editMode) {
        this.updateView(view);
        return;
      }

      UserService.saveIntegrationsView(view).then((response) => {
        this.views.push(view);
        this.filteredViews.push(view);
        this.toggleViewForm();
      }).catch((error) => {
        this.viewFormError = error.text || error;
      });
    },
    editView (view) {
      this.toggleViewForm();
      this.editMode = true;
      this.newView = view;
      this.selectedIntegrations = view.integrations;
    },
    updateView (view) {
      UserService.updateView(view).then((response) => {
        // TODO ECR update view in place
        this.toggleViewForm();
      }).catch((error) => {
        this.viewFormError = error.text || error;
      });
    },
    deleteView (id) {
      UserService.deleteView(id).then((response) => {
        // TODO ECR remove view from dropdown
      }).catch((error) => {
        this.viewsError = error.text || error;
      });
    },
    /* helpers ------------------------------------------------------------- */
    calculateSelectAll (list) {
      if (list.length === 0) {
        this.allSelected = false;
        this.indeterminate = false;
      } else if (list.length === Object.keys(this.getIntegrations).length) {
        this.allSelected = true;
        this.indeterminate = false;
      } else {
        this.allSelected = false;
        this.indeterminate = true;
      }
    },
    // TODO ECR searchable dropdown for views
    // TODO ECR display that a view is shared with me
    getViews () {
      UserService.getIntegrationViews().then((response) => {
        this.viewsError = '';
        this.views = response.views;
        this.filteredViews = JSON.parse(JSON.stringify(response.views));
      }).catch((error) => {
        this.viewsError = error.text || error;
      });
    }
  }
};
</script>

<style>
/* TODO ECR dark theme */
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

/* margin to flank content associated with sidebar
   transition to side it over when opening sidebar */
.main-content,
.main-content .search-nav {
  margin-left: 1.5rem;
  margin-right: 1rem;
  transition: 0.5s;
}
/* push over the content if there sidebar is open and not just hovering */
.main-content.with-sidebar,
.main-content.with-sidebar .search-nav {
  margin-left: 262px;
}

.view-dropdown .dropdown-item {
  padding: 0.1rem 0.5rem;
}
</style>
