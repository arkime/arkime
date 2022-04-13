<template>
  <b-dropdown
    size="sm"
    class="view-dropdown">
    <template #button-content>
      <slot name="title">
        Integration Views
      </slot>
    </template>
    <div class="ml-1 mr-1 mb-2" v-if="getViews.length">
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
              v-b-tooltip.hover="`Shared with you by ${view.creator}`"
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
</template>

<script>
import { mapGetters } from 'vuex';

import UserService from '@/components/services/UserService';

export default {
  name: 'ViewSelector',
  data () {
    return {
      error: '',
      viewSearch: '',
      filteredViews: []
    };
  },
  created () {
    this.filterViews(this.viewSearchTerm);
  },
  computed: {
    ...mapGetters(['getViews', 'getUser'])
  },
  watch: {
    viewSearch (searchTerm) {
      this.filterViews(searchTerm);
    },
    getViews () {
      this.filterViews(this.viewSearch);
    }
  },
  methods: {
    selectView (view) {
      this.$store.commit('SET_SELECTED_INTEGRATIONS', view.integrations);
    },
    deleteView (id) {
      // NOTE: this function handles fetching the updated view list and storing it
      UserService.deleteIntegrationsView(id).catch((error) => {
        this.error = error.text || error;
        setTimeout(() => { this.error = ''; }, 5000);
      });
    },
    filterViews (searchTerm) {
      if (!searchTerm) {
        this.filteredViews = JSON.parse(JSON.stringify(this.getViews));
        return;
      }

      const query = searchTerm.toLowerCase();

      this.filteredViews = this.getViews.filter((view) => {
        return view.name.toString().toLowerCase().match(query)?.length > 0;
      });
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
</style>
