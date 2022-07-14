<template>
  <b-dropdown
    size="sm"
    :no-caret="noCaret"
    class="view-dropdown"
    ref="integrationViewsDropdown">
    <template #button-content>
      <template v-if="hotKeyEnabled && getShiftKeyHold">
        <span class="text-warning">V</span>
      </template>
      <template v-else>
        <slot name="title">
          Integration Views
        </slot>
      </template>
      <span v-if="showSelectedView && getSelectedView">
        {{ getSelectedView.name }}
      </span>
    </template>
    <div class="ml-1 mr-1 mb-2">
      <b-input-group size="sm">
        <template #prepend>
          <b-input-group-text>
            <span class="fa fa-search" />
          </b-input-group-text>
        </template>
        <b-form-input
          ref="integrationViewDropdownSearch"
          @keydown.enter="attemptTopFilteredView"
          @focusin="setBarFocused"
          @focusout="setBarUnfocused"
          v-model="viewSearch"
        />
      </b-input-group>
    </div>
    <template v-for="(view, index) in filteredViews">
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
        :class="{ small: true, 'top-searched-dropdown': index === 0 && barFocused }"
        :id="view._id"
        :key="view._id"
        @click="selectView(view)">
        <div class="d-flex justify-content-between">
          <div class="d-inline no-wrap no-overflow ellipsis flex-grow-1">
            <span
                class="fa fa-share-alt mr-1 cursor-help"
                v-if="getUser && view.creator !== getUser.userId && !view._systemDefault"
                v-b-tooltip.hover="`Shared with you by ${view.creator}`"
            />
            {{ view.name }}
          </div>
          <template v-if="view._editable">
            <b-button
                size="xs"
                variant="danger"
                class="pull-right ml-1"
                @click.stop.prevent="deleteView(view)"
                v-b-tooltip.hover.top="'Delete this view.'">
              <span class="fa fa-trash-o" />
            </b-button>
          </template>
        </div>
      </b-dropdown-item>
      <hr :key="view._id + '-separator'"
          v-if="view._systemDefault && ((filteredViews[index + 1] && !filteredViews[index + 1]._systemDefault) || (!filteredViews[index + 1] && getViews.length === 0))"
          class="border-secondary my-0"/>
    </template>
    <b-dropdown-item
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
    </b-dropdown-item>
    <b-dropdown-text
        v-if="error"
        variant="danger">
      <span
          class="fa fa-exclamation-triangle"
      />
      {{ error }}
    </b-dropdown-text>
  </b-dropdown>
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
      dropdownVisible: false
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
</style>
