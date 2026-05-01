<!--
Copyright Yahoo Inc.
SPDX-License-Identifier: Apache-2.0
-->
<template>
  <v-dialog
    :model-value="show"
    @update:model-value="(val) => { if (!val) $emit('close'); }"
    max-width="900">
    <v-card density="compact">
      <v-card-title>
        {{ editing ? $t('sessions.summary.config.editConfig') : $t('sessions.summary.config.saveConfig') }}
      </v-card-title>
      <v-card-text>
        <!-- Name input -->
        <div class="input-group input-group-sm mb-2">
          <span
            id="configName"
            class="input-group-text cursor-help">
            {{ $t('sessions.summary.config.name') }}<sup>*</sup>
            <v-tooltip activator="#configName">
              {{ $t('sessions.summary.config.nameTip') }}
            </v-tooltip>
          </span>
          <input
            type="text"
            class="form-control"
            v-model="configName"
            :placeholder="$t('sessions.summary.config.namePlaceholder')">
        </div>

        <!-- Description input -->
        <div class="input-group input-group-sm mb-2">
          <span
            id="configDescription"
            class="input-group-text cursor-help">
            {{ $t('sessions.summary.config.description') }}
            <v-tooltip activator="#configDescription">
              {{ $t('sessions.summary.config.descriptionTip') }}
            </v-tooltip>
          </span>
          <input
            type="text"
            class="form-control"
            v-model="configDescription"
            :placeholder="$t('sessions.summary.config.descriptionPlaceholder')">
        </div>

        <!-- Sharing controls -->
        <div class="d-flex flex-wrap gap-2 mb-2">
          <RoleDropdown
            :roles="roles"
            class="d-inline"
            :selected-roles="viewRoles"
            :display-text="$t('common.rolesCanView')"
            @selected-roles-updated="viewRoles = $event" />
          <RoleDropdown
            :roles="roles"
            class="d-inline"
            :selected-roles="editRoles"
            :display-text="$t('common.rolesCanEdit')"
            @selected-roles-updated="editRoles = $event" />
        </div>

        <div class="d-flex gap-2 mb-2">
          <div class="input-group input-group-sm flex-grow-1">
            <span
              id="configViewUsers"
              class="input-group-text cursor-help">
              {{ $t('sessions.summary.config.viewUsers') }}
              <v-tooltip activator="#configViewUsers">
                {{ $t('sessions.summary.config.viewUsersTip') }}
              </v-tooltip>
            </span>
            <input
              type="text"
              class="form-control"
              v-model="viewUsers"
              :placeholder="$t('sessions.summary.config.usersPlaceholder')">
          </div>

          <div class="input-group input-group-sm flex-grow-1">
            <span
              id="configEditUsers"
              class="input-group-text cursor-help">
              {{ $t('sessions.summary.config.editUsers') }}
              <v-tooltip activator="#configEditUsers">
                {{ $t('sessions.summary.config.editUsersTip') }}
              </v-tooltip>
            </span>
            <input
              type="text"
              class="form-control"
              v-model="editUsers"
              :placeholder="$t('sessions.summary.config.usersPlaceholder')">
          </div>
        </div>

        <!-- Configuration preview -->
        <div class="config-preview mt-3 p-2 border rounded bg-light">
          <strong>{{ $t('sessions.summary.config.preview') }}</strong>
          <div class="mt-1 small">
            <span class="text-muted">{{ $t('sessions.summary.config.fieldsCount') }}:</span>
            {{ config?.fields?.length || 0 }}
            <span class="ms-3 text-muted">{{ $t('sessions.summary.config.resultsLimit') }}:</span>
            {{ config?.resultsLimit || 20 }}
          </div>
          <div class="mt-1 small text-muted">
            {{ fieldsList }}
          </div>
        </div>

        <!-- Error message -->
        <div
          v-if="error"
          class="alert alert-danger alert-sm mt-2 mb-0">
          <span class="fa fa-exclamation-triangle me-1" />
          {{ error }}
        </div>
      </v-card-text>
      <v-card-actions>
        <div class="w-100 d-flex justify-content-between">
          <button
            type="button"
            class="btn btn-danger"
            @click="$emit('close')">
            <span class="fa fa-times" />
            {{ $t('common.cancel') }}
          </button>
          <button
            type="button"
            class="btn btn-success"
            :disabled="saving"
            @click="saveConfig">
            <span
              v-if="saving"
              class="fa fa-spinner fa-spin me-1" />
            <span
              v-else
              class="fa fa-save me-1" />
            {{ $t('common.save') }}
          </button>
        </div>
      </v-card-actions>
    </v-card>
  </v-dialog>
</template>

<script setup>
import { ref, computed, watch } from 'vue';
import { useStore } from 'vuex';
import { useI18n } from 'vue-i18n';
import RoleDropdown from '@common/RoleDropdown.vue';
import { createShareableService } from '../users/ShareableService';
import FieldService from '../search/FieldService';

const SummaryConfigService = createShareableService('summaryConfig');

const store = useStore();
const { t } = useI18n();

const props = defineProps({
  show: {
    type: Boolean,
    default: false
  },
  config: {
    type: Object,
    default: () => null
  },
  editing: {
    type: Object,
    default: null // The shareable object being edited, null for new
  }
});

const emit = defineEmits(['close', 'saved']);

// Form state
const configName = ref('');
const configDescription = ref('');
const viewRoles = ref([]);
const editRoles = ref([]);
const viewUsers = ref('');
const editUsers = ref('');
const error = ref('');
const saving = ref(false);

// Get roles from store
const roles = computed(() => store.state.roles || []);

// Preview text showing field friendly names
const fieldsList = computed(() => {
  if (!props.config?.fields?.length) return '';
  return props.config.fields.map(f => {
    const fieldObj = FieldService.getField(f.field, true);
    return fieldObj?.friendlyName || f.field;
  }).join(', ');
});

// Reset form when modal opens
watch(() => props.show, (newVal) => {
  if (newVal) {
    error.value = '';
    if (props.editing) {
      // Editing existing config
      configName.value = props.editing.name || '';
      configDescription.value = props.editing.description || '';
      viewRoles.value = props.editing.viewRoles || [];
      editRoles.value = props.editing.editRoles || [];
      viewUsers.value = (props.editing.viewUsers || []).join(', ');
      editUsers.value = (props.editing.editUsers || []).join(', ');
    } else {
      // New config
      configName.value = '';
      configDescription.value = '';
      viewRoles.value = [];
      editRoles.value = [];
      viewUsers.value = '';
      editUsers.value = '';
    }
  }
});

// Parse comma-separated users string to array
const parseUsers = (str) => {
  if (!str) return [];
  return str.split(',').map(u => u.trim()).filter(u => u !== '');
};

const saveConfig = async () => {
  error.value = '';

  // Validate name
  if (!configName.value?.trim()) {
    error.value = t('sessions.summary.config.nameRequired');
    return;
  }

  saving.value = true;

  try {
    const configData = {
      name: configName.value.trim(),
      description: configDescription.value?.trim() || undefined,
      data: props.config,
      viewRoles: viewRoles.value,
      editRoles: editRoles.value,
      viewUsers: parseUsers(viewUsers.value),
      editUsers: parseUsers(editUsers.value)
    };

    let response;
    if (props.editing?.id) {
      response = await SummaryConfigService.update(props.editing.id, configData);
    } else {
      response = await SummaryConfigService.save(configData);
    }

    emit('saved', response.shareable);
    emit('close');
  } catch (err) {
    error.value = err.text || String(err);
  } finally {
    saving.value = false;
  }
};
</script>

<style scoped>
.config-preview {
  background-color: var(--color-quaternary-lightest) !important;
  border-color: var(--color-gray) !important;
}
</style>
