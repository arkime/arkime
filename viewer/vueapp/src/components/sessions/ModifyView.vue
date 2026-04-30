<!--
Copyright Yahoo Inc.
SPDX-License-Identifier: Apache-2.0
-->
<template>
  <div>
    <div
      class="d-flex flex-nowrap gap-1 align-items-start text-start"
      @keyup.stop.prevent.enter="modifyView">
      <v-text-field
        density="compact"
        variant="outlined"
        hide-details
        v-model="viewName"
        :label="$t('sessions.views.viewName')"
        :placeholder="$t('sessions.views.viewNamePlaceholder')"
        @keydown.enter.stop
        class="modify-view-name" />

      <div class="flex-fill d-flex align-items-center gap-1">
        <div class="flex-fill input-group input-group-sm">
          <span class="input-group-text">
            {{ $t('sessions.views.expression') }}
          </span>
          <ExpressionAutocompleteInput
            v-model="viewExpression"
            :placeholder="$t('sessions.views.expressionPlaceholder')"
            @apply="modifyView"
            @keyup.enter.stop />
          <button
            type="button"
            id="expandViewExpressionBtn"
            :aria-label="$t('sessions.views.expandExpressionTip')"
            class="btn btn-outline-secondary btn-clear-input"
            @click="showBigExpression = !showBigExpression">
            <span
              class="fa"
              :class="showBigExpression ? 'fa-compress' : 'fa-expand'" />
            <v-tooltip activator="parent">
              {{ $t('sessions.views.expandExpressionTip') }}
            </v-tooltip>
          </button>
        </div>
      </div>

      <v-text-field
        density="compact"
        variant="outlined"
        hide-details
        v-model="viewUsers"
        :label="$t('sessions.views.users')"
        :placeholder="$t('sessions.views.usersPlaceholder')"
        @keydown.enter.stop
        class="modify-view-users" />

      <RoleDropdown
        :roles="userRoles"
        :selected-roles="viewRoles"
        :display-text="$t('common.shareWithRoles')"
        @selected-roles-updated="updateViewRoles" />

      <span
        v-if="sessionsPage"
        id="useColConfigWrap"
        class="d-inline-block">
        <v-checkbox
          v-model="useColConfig"
          density="compact"
          hide-details
          inline
          :label="$t('sessions.views.saveColumns')" />
        <v-tooltip activator="#useColConfigWrap">
          {{ $t('sessions.views.saveColumnsTip') }}
        </v-tooltip>
      </span>

      <div>
        <button
          type="button"
          @click="modifyView"
          :class="{'disabled':loading}"
          class="btn btn-sm btn-theme-tertiary me-1">
          <span v-if="!loading">
            <span v-if="mode === 'create'">
              <span class="fa fa-plus-circle" />&nbsp;
              {{ $t('common.create') }}
            </span>
            <span v-else-if="mode === 'edit'">
              <span class="fa fa-save" />&nbsp;
              {{ $t('common.save') }}
            </span>
          </span>
          <span v-if="loading">
            <span class="fa fa-spinner fa-spin" />&nbsp;
            <span v-if="mode === 'create'">
              {{ $t('common.creating') }}
            </span>
            <span v-else-if="mode === 'edit'">
              {{ $t('common.saving') }}
            </span>
          </span>
        </button>
        <button
          id="cancelModifyView"
          type="button"
          :aria-label="$t('common.cancel')"
          @click="$emit('done', null, false, false)"
          class="btn btn-sm btn-warning">
          <span class="fa fa-ban" />
          <v-tooltip activator="parent">
            {{ $t('common.cancel') }}
          </v-tooltip>
        </button>
      </div>
    </div>

    <!-- big expression modal -->
    <v-dialog
      v-model="showBigExpression"
      max-width="900"
      persistent
      @after-enter="focusBigExpressionTextarea">
      <v-card>
        <v-card-title>
          <span class="fa fa-pencil fa-2x me-2" />
          <span>{{ $t('sessions.views.expression') }}</span>
        </v-card-title>
        <v-card-text>
          <ExpressionAutocompleteInput
            textarea
            rows="5"
            v-model="viewExpression"
            :placeholder="$t('sessions.views.expressionPlaceholder')"
            @apply="showBigExpression = false" />
        </v-card-text>
        <v-card-actions>
          <div class="d-flex w-100 justify-content-between">
            <div>
              <v-btn
                variant="flat"
                color="secondary"
                @click="showBigExpression = false">
                {{ $t('common.close') }}
              </v-btn>
              <v-btn
                variant="flat"
                color="warning"
                class="ms-2"
                @click="viewExpression = ''">
                {{ $t('common.clear') }}
              </v-btn>
            </div>
            <button
              type="button"
              class="btn btn-theme-tertiary"
              @click="showBigExpression = false">
              {{ $t('common.apply') }}
            </button>
          </div>
        </v-card-actions>
      </v-card>
    </v-dialog> <!-- /big expression modal -->

    <div
      v-if="error"
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
import { ref, computed, watch, onMounted } from 'vue';
import { useStore } from 'vuex';
import { useRoute } from 'vue-router';
import { useI18n } from 'vue-i18n';
const { t } = useI18n();

// services
import SettingsService from '../settings/SettingsService';
// utilities
import { resolveMessage } from '@common/resolveI18nMessage';
// components
import RoleDropdown from '@common/RoleDropdown.vue';
import ExpressionAutocompleteInput from '../search/ExpressionAutocompleteInput.vue';

// Define Props
const props = defineProps({
  editView: {
    type: Object,
    default: undefined
  },
  initialExpression: {
    type: String,
    default: ''
  }
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

// Initialize viewExpression from editView, initialExpression prop, or store
// Use a function to get the initial value to ensure we read the store at component creation time
const getInitialExpression = () => {
  if (props.editView && props.editView.expression) {
    return props.editView.expression;
  }
  if (props.initialExpression) {
    return props.initialExpression;
  }
  // Directly read from store as fallback
  return store.state.expression || '';
};

const viewExpression = ref(getInitialExpression());
const showBigExpression = ref(false);

// Computed properties
const sessionsPage = computed(() => route.name === 'Sessions');
const userRoles = computed(() => store.state.roles);

// Ensure expression is populated when creating a new view
onMounted(() => {
  // If creating a new view and viewExpression is empty, try to get from store
  if (!props.editView && !viewExpression.value && store.state.expression) {
    viewExpression.value = store.state.expression;
  }
});

// Watch for changes in initialExpression prop when creating a new view
watch(() => props.initialExpression, (newVal) => {
  if (!props.editView && newVal && !viewExpression.value) {
    viewExpression.value = newVal;
  }
});

// Methods
const focusBigExpressionTextarea = () => {
  setTimeout(() => {
    document.getElementById('bigViewExpression')?.focus();
  }, 100);
};

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
    emit('done', resolveMessage(response, t), true, true);
    store.commit('addView', response.view);
  } catch (err) {
    error.value = resolveMessage(err, t);
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
    emit('done', resolveMessage(response, t), true, true);
    SettingsService.getViews();
  } catch (err) {
    error.value = resolveMessage(err, t);
    loading.value = false;
  }
};

const modifyView = () => {
  if (!viewName.value) {
    error.value = t('sessions.views.noViewNameErr');
    return;
  }

  if (!viewExpression.value) {
    error.value = t('sessions.views.noExpressionErr');
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

<style scoped>
.modify-view-name {
  width: 220px;
  flex: 0 0 auto;
}
.modify-view-users {
  width: 200px;
  flex: 0 0 auto;
}
</style>
