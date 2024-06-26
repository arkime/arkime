<!--
Copyright Yahoo Inc.
SPDX-License-Identifier: Apache-2.0
-->
<template>
  <v-btn color="secondary">
    <template v-if="hotKeyEnabled && getShiftKeyHold">
      <span class="text-warning integration-view-hotkey-v">V</span>
    </template>
    <template v-else>
      <slot>
        Integration Views
      </slot>
    </template>
    <span v-if="showSelectedView && getSelectedView">
      {{ getSelectedView.name }}
    </span>
    <span v-if="!noCaret" class="fa fa-lg fa-caret-down ml-1" />

    <v-menu activator="parent" location="bottom right"
      :close-on-content-click="false"
      >
      <v-card>
        <v-list class="d-flex flex-column">
          <div class="ml-1 mr-1 mb-2">
            <v-text-field
              density="compact"
              prepend-inner-icon="fa fa-search fa-fw"
              variant="outlined"
              v-model="viewSearch"
              ref="integrationViewDropdownSearch"
              @keydown.enter="attemptTopFilteredView"
              @focusin="setBarFocused"
              @focusout="setBarUnfocused"
            />

            <template v-for="(view, index) in filteredViews"
                :key="view._id"
              >
              <v-btn
                variant="text"
                v-tooltip:right.close-on-content-click="(view.name.length > 24) ? view.name : ''"
                :class="{ small: true, 'top-searched-dropdown': index === 0 && barFocused }"
                @click="selectView(view)">
                <div class="d-flex justify-content-between">
                  <div class="d-inline no-wrap no-overflow ellipsis flex-grow-1">
                    <span
                        class="fa fa-share-alt mr-1 cursor-help"
                        v-if="getUser && view.creator !== getUser.userId && !view._systemDefault"
                        v-tooltip="`Shared with you by ${view.creator}`"
                    />
                    {{ view.name }}
                  </div>
                  <template v-if="view._editable">
                    <!-- cancel confirm delete button -->
                    <transition name="buttons">
                      <v-btn
                        size="x-small"
                        color="warning"
                        v-tooltip="'Cancel'"
                        title="Cancel"
                        class="pull-right ml-1"
                        v-if="confirmDeleteView[view._id]"
                        @click.stop.prevent="toggleDeleteView(view._id)">
                        <span class="fa fa-ban" />
                      </v-btn>
                    </transition> <!-- /cancel confirm delete button -->
                    <!-- confirm delete button -->
                    <transition name="buttons">
                      <v-btn
                        size="x-small"
                        color="danger"
                        v-tooltip="'Are you sure?'"
                        title="Are you sure?"
                        class="pull-right ml-1"
                        v-if="confirmDeleteView[view._id]"
                        @click.stop.prevent="deleteView(view)">
                        <span class="fa fa-check" />
                      </v-btn>
                    </transition> <!-- /confirm delete button -->
                    <!-- delete button -->
                    <transition name="buttons">
                      <v-btn
                        size="x-small"
                        color="danger"
                        class="pull-right ml-1"
                        v-if="!confirmDeleteView[view._id]"
                        v-tooltip:top="'Delete this view.'"
                        @click.stop.prevent="toggleDeleteView(view._id)">
                        <span class="fa fa-trash-o" />
                      </v-btn>
                    </transition> <!-- /delete button -->
                  </template>
                </div>
              </v-btn>
              <hr :key="view._id + '-separator'"
                  v-if="view._systemDefault && ((filteredViews[index + 1] && !filteredViews[index + 1]._systemDefault) || (!filteredViews[index + 1] && getViews.length === 0))"
                  class="border-secondary my-0"/>
            </template>
            <v-list-item
                class="small"
                v-b-modal.view-form
                v-if="!getViews.length || !filteredViews.length">
              <template v-if="!getViews.length">
                No saved views.
              </template>
              <template v-else>
                No views match your search.
              </template>
              <br>
              Click to create one.
            </v-list-item>
            <v-list-item
                v-if="error"
                variant="danger">
              <span
                  class="fa fa-exclamation-triangle"
              />
              {{ error }}
            </v-list-item>
          </div>
        </v-list>
      </v-card>
    </v-menu>
  </v-btn>
</template>

<script>
import { mapGetters } from 'vuex';

import UserService from '@/components/services/UserService';

export default {
  name: 'ViewSelector',
  props: {
    noCaret: {
      type: Boolean,
      default: false
    },
    showSelectedView: {
      type: Boolean,
      default: false
    },
    hotKeyEnabled: {
      type: Boolean,
      default: false
    }
  },
  data () {
    return {
      error: '',
      viewSearch: '',
      filteredViews: [],
      barFocused: false,
      needsFocus: false,
      dropdownVisible: false,
      confirmDeleteView: {}
    };
  },
  created () {
    this.filterViews(this.viewSearchTerm);
  },
  computed: {
    ...mapGetters([
      'getViews', 'getUser', 'getSelectedView', 'getDoableIntegrations', 'getAllViews', 'getShiftKeyHold', 'getFocusViewSearch'
    ])
  },
  watch: {
    viewSearch (searchTerm) {
      this.filterViews(searchTerm);
    },
    getViews () {
      this.filterViews(this.viewSearch);
    },
    getSelectedView (newView) {
      const newViewIDParam = newView?._id;
      if (this.$route.query.view !== newViewIDParam) {
        this.$router.push({ query: { ...this.$route.query, view: newViewIDParam } });
      }
    },
    getFocusViewSearch (val) {
      if (this.hotKeyEnabled && val) { // shortcut for view dropdown search
        if (!this.$refs.integrationViewsDropdown.visible) {
          this.$refs.integrationViewsDropdown.show();
        }
        if (this.dropdownVisible) {
          this.$refs.integrationViewDropdownSearch.select();
        } else {
          this.needsFocus = true;
        }
      }
    },
    dropdownVisible (val) {
      if (val && this.needsFocus) {
        this.$refs.integrationViewDropdownSearch.select();
        this.needsFocus = false;
      }
    }
  },
  methods: {
    selectView (view) {
      this.$store.commit('SET_SELECTED_VIEW', view);
      this.$store.commit('SET_SELECTED_INTEGRATIONS', view.integrations);
    },
    toggleDeleteView (viewId) {
      this.confirmDeleteView[viewId] = !this.confirmDeleteView[viewId];
    },
    deleteView (view) {
      // NOTE: this function handles fetching the updated view list and storing it
      UserService.deleteIntegrationsView(view._id).then(() => {
        if (view.name === this.getSelectedView) {
          this.$refs.integrationViewsDropdown.hide(true);
          this.$store.commit('SET_SELECTED_VIEW', undefined);
        }
      }).catch((error) => {
        this.error = error.text || error;
        setTimeout(() => { this.error = ''; }, 5000);
      });
    },
    filterViews (searchTerm) {
      if (!searchTerm) {
        this.filteredViews = JSON.parse(JSON.stringify(this.getAllViews));
        return;
      }

      const query = searchTerm.toLowerCase();
      this.filteredViews = this.getAllViews.filter((view) => {
        return view.name.toString().toLowerCase().match(query)?.length > 0;
      });
    },
    attemptTopFilteredView () {
      if (this.filteredViews.length) {
        this.selectView(this.filteredViews[0]);
        this.$refs.integrationViewsDropdown.hide();
        this.viewSearch = '';
      }
    },
    setBarFocused () {
      this.barFocused = true;
    },
    setBarUnfocused () {
      this.barFocused = false;
    },
    watchDropdownVisibility () {
      this.$watch(() => this.$refs?.integrationViewsDropdown?.visible, (newVisible) => {
        this.$nextTick(() => {
          this.dropdownVisible = newVisible;
        });
      });
    }
  },
  mounted () {
    if (this.hotKeyEnabled) {
      this.watchDropdownVisibility();
    }
  }
};
</script>

<style>
.view-dropdown .dropdown-item,
.view-dropdown .b-dropdown-text {
  padding: 0.1rem 0.5rem;
}
.view-dropdown .dropdown-menu {
  width: 240px;
  overflow: hidden;
}
.top-searched-dropdown {
  background-color: var(--color-gray-light);
}
.integration-view-hotkey-v {
  /* pad the V shown when shifting to keep the button the same size */
  padding-inline: 0.16rem
}
</style>
