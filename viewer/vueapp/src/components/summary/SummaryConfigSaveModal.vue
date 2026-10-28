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
        {{ modalTitle }}
      </v-card-title>
      <v-card-text>
        <!-- Name input -->
        <div class="arkime-input-group arkime-input-group--fluid mb-2">
          <span
            id="configName"
            class="arkime-input-label cursor-help">
            {{ $t('sessions.summary.config.name') }}<sup>*</sup>
            <v-tooltip activator="#configName">
              {{ kind === 'graph' ? $t('spigraph.graphs.nameTip') : $t('sessions.summary.config.nameTip') }}
            </v-tooltip>
          </span>
          <input
            type="text"
            class="arkime-input-control"
            v-model="configName"
            maxlength="256"
            :placeholder="kind === 'graph' ? $t('spigraph.graphs.namePlaceholder') : $t('sessions.summary.config.namePlaceholder')">
        </div>

        <!-- Description input -->
        <div class="arkime-input-group arkime-input-group--fluid mb-2">
          <span
            id="configDescription"
            class="arkime-input-label cursor-help">
            {{ $t('sessions.summary.config.description') }}
            <v-tooltip activator="#configDescription">
              {{ $t('sessions.summary.config.descriptionTip') }}
            </v-tooltip>
          </span>
          <input
            type="text"
            class="arkime-input-control"
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
          <div class="arkime-input-group arkime-input-group--fluid">
            <span
              id="configViewUsers"
              class="arkime-input-label cursor-help">
              {{ $t('sessions.summary.config.viewUsers') }}
              <v-tooltip activator="#configViewUsers">
                {{ $t('sessions.summary.config.viewUsersTip') }}
              </v-tooltip>
            </span>
            <input
              type="text"
              class="arkime-input-control"
              v-model="viewUsers"
              :placeholder="$t('sessions.summary.config.usersPlaceholder')">
          </div>

          <div class="arkime-input-group arkime-input-group--fluid">
            <span
              id="configEditUsers"
              class="arkime-input-label cursor-help">
              {{ $t('sessions.summary.config.editUsers') }}
              <v-tooltip activator="#configEditUsers">
                {{ $t('sessions.summary.config.editUsersTip') }}
              </v-tooltip>
            </span>
            <input
              type="text"
              class="arkime-input-control"
              v-model="editUsers"
              :placeholder="$t('sessions.summary.config.usersPlaceholder')">
          </div>
        </div>

        <!-- Configuration preview -->
        <div class="config-preview mt-3 p-2 border rounded">
          <strong>{{ kind === 'graph' ? $t('spigraph.graphs.preview') : $t('sessions.summary.config.preview') }}</strong>
          <div
            v-if="kind !== 'graph'"
            class="mt-1 small">
            <span class="text-medium-emphasis">{{ $t('sessions.summary.config.widgetsCount') }}:</span>
            {{ config?.widgets?.length || 0 }}
          </div>
          <div class="mt-1 small text-medium-emphasis">
            {{ fieldsList }}
          </div>
        </div>

        <!-- Error message -->
        <v-alert
          v-if="error"
          type="error"
          variant="tonal"
          density="compact"
          class="mt-2 mb-0">
          {{ error }}
        </v-alert>
      </v-card-text>
      <v-card-actions>
        <div class="w-100 d-flex justify-space-between">
          <v-btn
            color="error"
            variant="flat"
            size="large"
            @click="$emit('close')">
            <v-icon
              icon="mdi-close"
              class="me-1" />
            {{ $t('common.cancel') }}
          </v-btn>
          <v-btn
            color="success"
            variant="flat"
            size="large"
            :disabled="saving"
            @click="saveConfig">
            <v-icon
              icon="mdi-loading"
              class="mdi-spin me-1"
              v-if="saving" />
            <v-icon
              icon="mdi-content-save"
              class="me-1"
              v-else />
            {{ $t('common.save') }}
          </v-btn>
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
import { viewModeLabelKey } from './widgets/viewModes';

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
  },
  // what is being saved: a 'dashboard' (summaryConfig) or a 'graph' (spigraphConfig)
  kind: {
    type: String,
    default: 'dashboard'
  }
});

const service = computed(() => createShareableService(props.kind === 'graph' ? 'spigraphConfig' : 'summaryConfig'));

const modalTitle = computed(() => {
  if (props.kind === 'graph') { return props.editing ? t('spigraph.graphs.editGraph') : t('spigraph.graphs.saveGraph'); }
  return props.editing ? t('sessions.summary.config.editConfig') : t('sessions.summary.config.saveConfig');
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

// Preview text showing widget field friendly names (a graph: its type and fields)
const fieldsList = computed(() => {
  if (props.kind === 'graph') {
    if (!props.config) { return ''; }
    const fields = (props.config.fields?.length ? props.config.fields : [props.config.field]).filter(Boolean);
    const names = fields.map(f => FieldService.getField(f, true)?.friendlyName || f);
    if (props.config.splitField) { names.push(FieldService.getField(props.config.splitField, true)?.friendlyName || props.config.splitField); }
    const labelKey = viewModeLabelKey(props.config.viewMode);
    const label = labelKey ? t(labelKey) : props.config.viewMode;
    return `${label}: ${names.join(', ')}`;
  }
  if (!props.config?.widgets?.length) return '';
  return props.config.widgets.map(w => {
    const fieldObj = FieldService.getField(w.field, true);
    // fall back to viewMode for field-less widgets so the preview has no blank segments
    return w.title || fieldObj?.friendlyName || w.field || w.viewMode;
  }).filter(Boolean).join(', ');
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
      response = await service.value.update(props.editing.id, configData);
    } else {
      response = await service.value.save(configData);
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
  background-color: rgb(var(--v-theme-quaternary-lightest)) !important;
  border-color: rgb(var(--v-theme-neutral)) !important;
}
</style>
