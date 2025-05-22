<!--
Copyright Yahoo Inc.
SPDX-License-Identifier: Apache-2.0
-->
<template>
  <div>

    <div class="d-flex flex-row"
      @keyup.stop.prevent.enter="modifyView">
      <div>
        <div class="input-group input-group-sm">
          <span class="input-group-text">
            View Name
          </span>
          <input
            type="text"
            class="form-control"
            v-model="name"
            placeholder="Enter a (short) view name"
            @keydown.enter.stop
          />
        </div>
      </div>
      <div class="flex-grow-1 ms-2">
        <div class="input-group input-group-sm">
          <span class="input-group-text">
            Expression
          </span>
          <input
            type="text"
            class="form-control"
            v-model="viewExpression"
            placeholder="Enter a query expression"
            @keydown.enter.stop
          />
        </div>
      </div>
      <div class="ms-2">
        <div class="input-group input-group-sm">
          <span class="input-group-text">
            Users
          </span>
          <input
            type="text"
            v-model="viewUsers"
            class="form-control"
            @keydown.enter.stop
            placeholder="Enter a comma separated list of users who can view this view"
          />
        </div>
      </div>
      <div class="ms-2">
        <RoleDropdown
          :roles="userRoles"
          :selected-roles="viewRoles"
          display-text="Share with roles"
          @selected-roles-updated="updateViewRoles"
        />
      </div>
      <div v-if="sessionsPage" class="ms-2">
        <BFormCheckbox
          id="useColConfig"
          v-model="useColConfig">
          Save Columns
          <BTooltip target="useColConfig">Save the visible sessions table columns and sort order with this view. When applying this view, the sessions table will be updated.</BTooltip>
        </BFormCheckbox>
      </div>
      <div class="ms-2">
        <button
          type="button"
          @click="modifyView"
          :class="{'disabled':loading}"
          class="btn btn-sm btn-theme-tertiary me-1"
          :title="`${mode === 'create' ? 'Create View' : 'Save View'}`">
          <span v-if="!loading">
            <span v-if="mode === 'create'">
              <span class="fa fa-plus-circle" />&nbsp;
              Create View
            </span>
            <span v-else-if="mode === 'edit'">
              <span class="fa fa-save" />&nbsp;
              Save View
            </span>
          </span>
          <span v-if="loading">
            <span class="fa fa-spinner fa-spin" />&nbsp;
            <span v-if="mode === 'create'">
              Creating View
            </span>
            <span v-else-if="mode === 'edit'">
              Saving View
            </span>
          </span>
        </button>
        <button id="cancelModifyView"
          type="button"
          @click="$emit('done', false)"
          class="btn btn-sm btn-warning">
          <span class="fa fa-ban" />
          <BTooltip target="cancelModifyView">Cancel</BTooltip>
        </button>
      </div>
    </div>

    <div v-if="error"
      class="row small text-danger mb-0 mt-1">
      <div class="col">
        <span class="fa fa-exclamation-triangle me-1" />
        {{ error }}
      </div>
    </div>

  </div>
</template>

<script setup>
// external imports
import { ref, computed } from 'vue';
import { useStore } from 'vuex';
import { useRoute } from 'vue-router';

// services
import SettingsService from '../settings/SettingsService';
// components
import RoleDropdown from '@common/RoleDropdown.vue';

// Define Props
const props = defineProps({
  editView: Object,
  initialExpression: String
});

// Define Emits
const emit = defineEmits(['done']);

// Vuex Store and Vue Router
const store = useStore();
const route = useRoute();

// Reactive state
const error = ref('');
const loading = ref(false);
const mode = ref(props.editView ? 'edit' : 'create');
const viewName = ref(props.editView ? props.editView.name : '');
const viewUsers = ref((props.editView && props.editView.users) ? props.editView.users : '');
const viewRoles = ref((props.editView && props.editView.roles) ? [...props.editView.roles] : []); // Ensure it's a new array
const useColConfig = ref(props.editView && (props.editView.sessionsColConfig !== undefined));
const viewExpression = ref(props.editView ? props.editView.expression : (props.initialExpression || ''));

// Computed properties
const sessionsPage = computed(() => route.name === 'Sessions');
const userRoles = computed(() => store.state.roles);

// Methods
const updateViewRoles = (roles) => {
  viewRoles.value = roles;
};

const createViewAction = async () => {
  const data = {
    name: viewName.value,
    users: viewUsers.value,
    roles: viewRoles.value,
    expression: viewExpression.value
  };

  if (useColConfig.value) {
    // save the current sessions table column configuration
    // Ensure sessionsTableState is a getter that returns a plain object or clone it
    data.sessionsColConfig = JSON.parse(JSON.stringify(store.getters.sessionsTableState));
  }

  try {
    const response = await SettingsService.createView(data, undefined); // Assuming second param is options/config
    loading.value = false;
    emit('done', response.text, true); // Emit the done event with the response text
    store.commit('addView', response.view);
  } catch (err) {
    error.value = err.message || err.text || 'Error creating view.'; // Use err.message if available
    loading.value = false;
  }
};

const updateViewAction = async () => {
  // Deep clone editView to avoid mutating props directly, though refs create copies of primitives.
  // For objects/arrays from props, direct assignment to ref makes them reactive but mutation should be careful.
  // Here, creating a new data object is safer.
  const data = props.editView ? JSON.parse(JSON.stringify(props.editView)) : {};

  data.name = viewName.value;
  data.users = viewUsers.value;
  data.roles = viewRoles.value; // viewRoles is already a ref based on prop or new array
  data.expression = viewExpression.value;

  if (useColConfig.value === true) {
    const tableClone = JSON.parse(JSON.stringify(store.getters.sessionsTableState));
    data.sessionsColConfig = tableClone;
  } else if (data.sessionsColConfig) {
    delete data.sessionsColConfig;
  }

  try {
    const response = await SettingsService.updateView(data, undefined);
    loading.value = false;
    emit('done', response.text, true); // Emit the done event with the response text
    SettingsService.getViews();
  } catch (err) {
    error.value = err.message || err.text || 'Error updating view.'; // Use err.message if available
    loading.value = false;
  }
};

const modifyView = () => {
  if (!viewName.value) {
    error.value = 'No view name specified.';
    return;
  }

  if (!viewExpression.value) {
    error.value = 'No expression specified.';
    return;
  }

  error.value = ''; // Clear previous errors
  loading.value = true;

  if (mode.value === 'edit') {
    updateViewAction();
  } else {
    createViewAction();
  }
};
</script>
