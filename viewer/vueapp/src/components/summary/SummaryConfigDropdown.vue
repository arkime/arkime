<!--
Copyright Yahoo Inc.
SPDX-License-Identifier: Apache-2.0
-->
<template>
  <v-menu
    v-model="menuOpen"
    :close-on-content-click="false"
    @update:model-value="(opened) => { if (opened) loadConfigs(); }">
    <template #activator="{ props: activatorProps }">
      <v-btn
        v-bind="activatorProps"
        id="summary-config-dropdown-btn">
        <v-icon icon="mdi-content-save" />
      </v-btn>
      <v-tooltip
        activator="#summary-config-dropdown-btn"
        location="right">
        {{ $t('sessions.summary.config.configurations') }}
      </v-tooltip>
    </template>

    <v-list
      density="compact"
      class="summary-config-list">
      <!-- Search -->
      <v-list-item>
        <v-text-field
          v-model="search"
          density="compact"
          variant="outlined"
          hide-details
          clearable
          prepend-inner-icon="mdi-magnify"
          :placeholder="$t('sessions.summary.config.searchConfigs')" />
      </v-list-item>

      <v-divider />

      <!-- Save current config -->
      <v-list-item @click="openSaveModal">
        <v-icon
          icon="mdi-content-save"
          class="me-1" />
        {{ $t('sessions.summary.config.saveCurrent') }}
      </v-list-item>

      <!-- Import config from JSON -->
      <v-list-item @click="triggerImport">
        <v-icon
          icon="mdi-upload"
          class="me-1" />
        {{ $t('sessions.summary.config.import') }}
      </v-list-item>
      <input
        ref="importInput"
        type="file"
        accept="application/json,.json"
        class="d-none"
        @change="onImportFile">

      <!-- Export current config to JSON -->
      <v-list-item @click="exportCurrent">
        <v-icon
          icon="mdi-download"
          class="me-1" />
        {{ $t('sessions.summary.config.exportCurrent') }}
      </v-list-item>

      <!-- Reset to defaults -->
      <v-list-item @click="resetToDefaults">
        <v-icon
          icon="mdi-refresh"
          class="me-1" />
        {{ $t('sessions.summary.config.resetToDefault') }}
      </v-list-item>

      <v-divider v-if="filteredConfigs.length > 0 || loading" />

      <!-- Loading indicator -->
      <v-list-item
        v-if="loading"
        disabled>
        <v-icon
          icon="mdi-loading"
          class="mdi-spin me-1" />
        {{ $t('common.loading') }}
      </v-list-item>

      <!-- Error message -->
      <v-list-item
        v-if="error && !loading"
        disabled
        class="text-danger">
        <v-icon
          icon="mdi-alert"
          class="me-1" />
        {{ error }}
      </v-list-item>

      <!-- Saved configurations -->
      <v-list-item
        v-for="config in filteredConfigs"
        :key="config.id"
        @click="loadConfig(config)">
        <div class="d-flex justify-space-between align-center w-100">
          <span class="config-name">
            <v-icon
              v-if="config.id === defaultId"
              icon="mdi-star"
              class="me-1 text-theme-accent"
              :title="$t('sessions.summary.config.isDefault')" />
            <v-icon
              v-if="config.shared"
              icon="mdi-share-all"
              class="me-1" />
            {{ config.name }}
            <small
              v-if="config.creator && config.creator !== user.userId"
              class="text-medium-emphasis ms-1">
              ({{ config.creator }})
            </small>
          </span>
          <span
            class="config-actions"
            @click.stop>
            <v-btn
              variant="flat"
              size="small"
              density="comfortable"
              icon
              :style="config.id === defaultId ? accentBtnStyle : tertiaryBtnStyle"
              class="ms-1"
              :title="config.id === defaultId ? $t('sessions.summary.config.unsetDefault') : $t('sessions.summary.config.setDefault')"
              @click.stop="toggleDefault(config)">
              <v-icon :icon="config.id === defaultId ? 'mdi-star' : 'mdi-star-outline'" />
            </v-btn>
            <v-btn
              variant="flat"
              size="small"
              density="comfortable"
              icon
              :style="tertiaryBtnStyle"
              class="ms-1"
              :title="$t('sessions.summary.config.export')"
              @click.stop="exportConfig(config)">
              <v-icon icon="mdi-download" />
            </v-btn>
            <v-btn
              v-if="config.canEdit"
              variant="flat"
              size="small"
              density="comfortable"
              icon
              :style="tertiaryBtnStyle"
              class="ms-1"
              :title="$t('sessions.summary.config.edit')"
              :aria-label="$t('sessions.summary.config.edit')"
              @click.stop="openEditModal(config)">
              <v-icon icon="mdi-pencil" />
            </v-btn>
            <v-btn
              v-if="config.canDelete"
              color="error"
              variant="flat"
              size="small"
              density="comfortable"
              icon
              class="ms-1"
              :title="$t('sessions.summary.config.delete')"
              :aria-label="$t('sessions.summary.config.delete')"
              @click.stop="deleteConfig(config)">
              <v-icon icon="mdi-trash-can-outline" />
            </v-btn>
          </span>
        </div>
      </v-list-item>

      <!-- No configs message -->
      <v-list-item
        v-if="!loading && !error && configs.length === 0"
        disabled>
        <span class="text-medium-emphasis">
          {{ $t('sessions.summary.config.noConfigs') }}
        </span>
      </v-list-item>
    </v-list>
  </v-menu>
  <!-- Save/Edit Modal: rendered as a fragment sibling so it doesn't
       wrap the v-menu (which would break the parent v-btn-group). The
       v-dialog inside teleports to body anyway. -->
  <SummaryConfigSaveModal
    :show="showSaveModal"
    :config="currentConfig"
    :editing="editingConfig"
    @close="closeSaveModal"
    @saved="onConfigSaved" />
</template>

<script setup>
import { ref, computed } from 'vue';
import { useStore } from 'vuex';
import { useI18n } from 'vue-i18n';
import { createShareableService } from '../users/ShareableService';
import UserService from '../users/UserService';
import SummaryConfigSaveModal from './SummaryConfigSaveModal.vue';

const SummaryConfigService = createShareableService('summaryConfig');

const store = useStore();
const { t } = useI18n();

const props = defineProps({
  currentConfig: {
    type: Object,
    required: true
  }
});

const emit = defineEmits(['load', 'message', 'reset']);

// State
const menuOpen = ref(false);
const loading = ref(false);
const error = ref('');
const configs = ref([]);
const search = ref('');
const showSaveModal = ref(false);
const editingConfig = ref(null);
const importInput = ref(null);

// Arkime theme-color v-btn styles for inline row-action buttons inside
// the dropdown menu. The trigger button takes its color from the
// parent v-btn-group.
const tertiaryBtnStyle = {
  backgroundColor: 'rgb(var(--v-theme-tertiary))',
  color: 'rgb(var(--v-theme-button-fg))'
};
const accentBtnStyle = {
  backgroundColor: 'rgb(var(--v-theme-foreground-accent))',
  color: 'rgb(var(--v-theme-button-fg))'
};

// Get user from store
const user = computed(() => store.state.user);

// The user's default dashboard id (if any)
const defaultId = computed(() => store.state.user?.settings?.defaultDashboardId || '');

// Configs filtered by the search box
const filteredConfigs = computed(() => {
  const term = search.value?.trim().toLowerCase();
  if (!term) { return configs.value; }
  return configs.value.filter(c => (c.name || '').toLowerCase().includes(term));
});

// Load saved configurations
const loadConfigs = async () => {
  loading.value = true;
  error.value = '';

  try {
    const response = await SummaryConfigService.list();
    configs.value = response.data || [];
  } catch (err) {
    error.value = err.text || String(err);
    configs.value = [];
  } finally {
    loading.value = false;
  }
};

// Load a configuration
const loadConfig = (config) => {
  emit('load', config);
  menuOpen.value = false;
};

// Reset to default configuration
const resetToDefaults = () => {
  emit('reset');
  menuOpen.value = false;
};

// Open save modal for new config
const openSaveModal = () => {
  editingConfig.value = null;
  showSaveModal.value = true;
};

// Open save modal for editing existing config
const openEditModal = async (config) => {
  try {
    // Fetch full config data for editing
    const response = await SummaryConfigService.get(config.id);
    editingConfig.value = response.shareable;
    showSaveModal.value = true;
  } catch (err) {
    emit('message', {
      msg: err.text || String(err),
      type: 'danger'
    });
  }
};

// Close save modal
const closeSaveModal = () => {
  showSaveModal.value = false;
  editingConfig.value = null;
};

// Handle config saved
const onConfigSaved = (savedConfig) => {
  // Refresh the configs list
  loadConfigs();
  emit('message', {
    msg: t('sessions.summary.config.saved'),
    type: 'success'
  });
};

// Delete a configuration
const deleteConfig = async (config) => {
  try {
    await SummaryConfigService.delete(config.id);
    // Remove from local list
    configs.value = configs.value.filter(c => c.id !== config.id);
    // Clear the default pointer if it referenced this config
    if (defaultId.value === config.id) {
      await persistDefault('');
    }
    emit('message', {
      msg: t('sessions.summary.config.deleted'),
      type: 'success'
    });
  } catch (err) {
    emit('message', {
      msg: err.text || String(err),
      type: 'danger'
    });
  }
};

// Persist the default dashboard id on the user's settings
const persistDefault = async (id) => {
  const settings = { ...(store.state.user?.settings || {}), defaultDashboardId: id };
  await UserService.saveSettings(settings);
};

// Toggle a config as the default landing dashboard
const toggleDefault = async (config) => {
  try {
    const newId = defaultId.value === config.id ? '' : config.id;
    await persistDefault(newId);
    emit('message', {
      msg: newId ? t('sessions.summary.config.defaultSet') : t('sessions.summary.config.defaultUnset'),
      type: 'success'
    });
  } catch (err) {
    emit('message', {
      msg: err.text || String(err),
      type: 'danger'
    });
  }
};

// Trigger the hidden file input for import
const triggerImport = () => {
  importInput.value?.click();
};

// Download a config object as a JSON file
const downloadJSON = (obj, filename) => {
  const blob = new Blob([JSON.stringify(obj, null, 2)], { type: 'application/json' });
  const url = window.URL.createObjectURL(blob);
  const a = document.createElement('a');
  a.href = url;
  a.download = filename;
  a.click();
  window.URL.revokeObjectURL(url);
};

// Export the currently-applied dashboard
const exportCurrent = () => {
  downloadJSON({
    name: t('sessions.summary.config.currentDashboard'),
    data: props.currentConfig
  }, 'arkime-dashboard.json');
  menuOpen.value = false;
};

// Export a saved dashboard
const exportConfig = (config) => {
  downloadJSON({
    name: config.name,
    description: config.description,
    data: config.data
  }, `arkime-dashboard-${(config.name || 'dashboard').replace(/[^a-z0-9-_]+/gi, '_')}.json`);
};

// Import a dashboard from an uploaded JSON file
const onImportFile = async (evt) => {
  const file = evt.target.files?.[0];
  if (!file) { return; }
  try {
    const text = await file.text();
    const parsed = JSON.parse(text);
    // Accept either { name, description, data } or a bare config object
    const data = parsed.data && typeof parsed.data === 'object' ? parsed.data : parsed;
    if (!data || (!Array.isArray(data.widgets) && !Array.isArray(data.fields))) {
      throw new Error(t('sessions.summary.config.importInvalid'));
    }
    const configName = parsed.name || file.name.replace(/\.json$/i, '');
    await SummaryConfigService.save({
      name: configName,
      description: parsed.description,
      data
    });
    await loadConfigs();
    emit('message', {
      msg: t('sessions.summary.config.imported'),
      type: 'success'
    });
  } catch (err) {
    emit('message', {
      msg: err.message || err.text || String(err),
      type: 'danger'
    });
  } finally {
    // Reset the input so the same file can be re-selected
    evt.target.value = '';
  }
};
</script>

<style scoped>
.config-name {
  flex-grow: 1;
  overflow: hidden;
  text-overflow: ellipsis;
  white-space: nowrap;
  max-width: 200px;
}

.config-actions {
  flex-shrink: 0;
  opacity: 0;
  transition: opacity 0.15s;
}

.summary-config-list :deep(.v-list-item:hover) .config-actions {
  opacity: 1;
}

.summary-config-list {
  min-width: 320px;
}
</style>
