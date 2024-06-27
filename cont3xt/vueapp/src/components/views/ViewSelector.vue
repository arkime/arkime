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

    <v-menu v-model="menuOpen" activator="parent" location="bottom right"
      :close-on-content-click="false"
      >
      <v-card class="view-selector-menu">
        <div class="d-flex flex-column">
          <div class="ml-1 mr-1 mb-2">
            <v-text-field
              class="mb-1"
              density="compact"
              hide-details
              prepend-inner-icon="fa fa-search fa-fw"
              variant="outlined"
              v-model="viewSearch"
              ref="integrationViewDropdownSearchRef"
              @keydown.enter="attemptTopFilteredView"
              @focusin="setBarFocused"
              @focusout="setBarUnfocused"
            />

            <template v-for="(view, index) in filteredViews"
                :key="view._id"
              >
              <div class="d-flex flex-column">
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

              </div>
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
        </div>
      </v-card>
    </v-menu>
  </v-btn>
</template>

<script setup>
import UserService from '@/components/services/UserService';

import { reactive, ref, watch, defineProps, onMounted } from 'vue';
import { useStore } from 'vuex';
import { useGetters } from '@/vue3-helpers';
import { useRouter, useRoute } from 'vue-router';

const router = useRouter();
const route = useRoute();

const store = useStore();
const {
  getViews, getUser, getSelectedView,
  getAllViews, getShiftKeyHold, getFocusViewSearch
} = useGetters(store);

// Data
const error = ref('');
const viewSearch = ref('');
const filteredViews = ref([]);
const barFocused = ref('false');
const confirmDeleteView = reactive({});
const menuOpen = ref(false);
const integrationViewDropdownSearchRef = ref(null);

// Props
const props = defineProps({
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
});

// Methods
function selectView (view) {
  store.commit('SET_SELECTED_VIEW', view);
  store.commit('SET_SELECTED_INTEGRATIONS', view.integrations);
}

function toggleDeleteView (viewId) {
  confirmDeleteView[viewId] = !confirmDeleteView[viewId];
}

function deleteView (view) {
  // NOTE: this function handles fetching the updated view list and storing it
  UserService.deleteIntegrationsView(view._id).then(() => {
    if (view.name === getSelectedView.value) {
      menuOpen.value = false;
      store.commit('SET_SELECTED_VIEW', undefined);
    }
  }).catch((err) => {
    err.value = err.text || err;
    setTimeout(() => { err.value = ''; }, 5000);
  });
}

function filterViews (searchTerm) {
  if (!searchTerm) {
    filteredViews.value = JSON.parse(JSON.stringify(getAllViews.value));
    return;
  }

  const query = searchTerm.toLowerCase();
  filteredViews.value = getAllViews.value.filter((view) => {
    return view.name.toString().toLowerCase().match(query)?.length > 0;
  });
}

function attemptTopFilteredView () {
  if (filteredViews.value.length) {
    selectView(filteredViews.value[0]);
    menuOpen.value = false;
    viewSearch.value = '';
  }
}

function setBarFocused () {
  barFocused.value = true;
}

function setBarUnfocused () {
  barFocused.value = false;
}

// Watch
watch(menuOpen, (newOpen) => {
  if (newOpen) {
    viewSearch.value = '';
  }
});

watch(viewSearch, (searchTerm) => {
  filterViews(searchTerm);
});

watch(getViews, () => {
  filterViews(viewSearch.value);
});

watch(getSelectedView, (newView) => {
  const newViewIDParam = newView?._id;
  if (route.query.view !== newViewIDParam) {
    router.push({ query: { ...route.query, view: newViewIDParam } });
  }
});

watch(getFocusViewSearch, (val) => {
  if (props.hotKeyEnabled && val) { // shortcut for view dropdown search
    menuOpen.value = true;
    // we need a short timeout before we can focus the search bar within
    // the menu, since the input element is not rendered when closed
    setTimeout(() => {
      integrationViewDropdownSearchRef.value?.select();
    }, 100);
  }
});

onMounted(() => {
  filterViews(viewSearch.value);
});
</script>

<style>
.view-selector-menu {
  min-width: 12rem;
}
/* TODO: toby look at these other classes */
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
