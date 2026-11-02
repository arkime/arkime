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

      <!-- Save back into the shown saved dashboard (only when editable) -->
      <v-list-item
        v-if="currentSaved"
        :disabled="savingCurrent"
        @click="saveToCurrent">
        <v-icon
          icon="mdi-content-save-edit"
          class="me-1" />
        {{ $t('sessions.summary.config.saveTo', { name: currentSaved.name }) }}
      </v-list-item>

      <!-- Save current config as a new dashboard -->
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

      <v-divider />

      <!-- System dashboards (the viewer's *.dashboard.json files), grouped by
           folder (a folder row opens/closes it) -->
      <v-list-subheader v-if="systemRows.length">
        {{ $t('sessions.summary.config.system') }}
      </v-list-subheader>
      <template
        v-for="row in systemRows"
        :key="row.key">
        <v-list-item
          v-if="row.kind === 'folder'"
          class="folder-row"
          @click="toggleFolder('y:' + row.name)">
          <v-icon
            :icon="row.open ? 'mdi-folder-open-outline' : 'mdi-folder-outline'"
            class="me-1" />
          {{ row.name }}
          <small class="text-medium-emphasis ms-1">({{ row.count }})</small>
        </v-list-item>
        <v-list-item
          v-else
          :class="{ 'ps-8': row.indent, 'config-current': row.item.id === currentId }"
          @click="loadConfig(row.item)">
          <div class="d-flex justify-space-between align-center w-100">
            <span class="config-name">
              <v-icon
                v-if="row.item.id === defaultId"
                icon="mdi-star"
                class="me-1 text-theme-accent"
                :title="$t('sessions.summary.config.isDefault')" />
              <v-icon
                v-else
                icon="mdi-view-dashboard-outline"
                class="me-1" />
              {{ row.item.name }}
            </span>
            <span
              class="config-actions"
              @click.stop>
              <v-btn
                variant="flat"
                size="small"
                density="comfortable"
                icon
                :style="row.item.id === defaultId ? accentBtnStyle : tertiaryBtnStyle"
                class="ms-1"
                :title="row.item.id === defaultId ? $t('sessions.summary.config.unsetDefault') : $t('sessions.summary.config.setDefault')"
                @click.stop="toggleDefault(row.item)">
                <v-icon :icon="row.item.id === defaultId ? 'mdi-star' : 'mdi-star-outline'" />
              </v-btn>
            </span>
          </div>
          <v-tooltip
            activator="parent"
            location="end"
            :open-delay="500">
            {{ row.item.description }}
          </v-tooltip>
        </v-list-item>
      </template>

      <v-divider v-if="savedRows.length > 0 || loading" />

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

      <!-- Saved dashboards, grouped by folder -->
      <v-list-subheader v-if="savedRows.length">
        {{ $t('sessions.summary.config.savedDashboards') }}
      </v-list-subheader>
      <template
        v-for="row in savedRows"
        :key="row.key">
        <v-list-item
          v-if="row.kind === 'folder'"
          class="folder-row"
          @click="toggleFolder('s:' + row.name)">
          <v-icon
            :icon="row.open ? 'mdi-folder-open-outline' : 'mdi-folder-outline'"
            class="me-1" />
          {{ row.name }}
          <small class="text-medium-emphasis ms-1">({{ row.count }})</small>
        </v-list-item>
        <v-list-item
          v-else
          :class="{ 'ps-8': row.indent, 'config-current': row.item.id === currentId }"
          @click="loadConfig(row.item)">
          <div class="d-flex justify-space-between align-center w-100">
            <span class="config-name">
              <v-icon
                v-if="row.item.id === defaultId"
                icon="mdi-star"
                class="me-1 text-theme-accent"
                :title="$t('sessions.summary.config.isDefault')" />
              <v-icon
                v-if="row.item.shared"
                icon="mdi-share-all"
                class="me-1" />
              {{ row.item.name }}
              <small
                v-if="row.item.creator && row.item.creator !== user.userId"
                class="text-medium-emphasis ms-1">
                ({{ row.item.creator }})
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
                :style="row.item.id === defaultId ? accentBtnStyle : tertiaryBtnStyle"
                class="ms-1"
                :title="row.item.id === defaultId ? $t('sessions.summary.config.unsetDefault') : $t('sessions.summary.config.setDefault')"
                @click.stop="toggleDefault(row.item)">
                <v-icon :icon="row.item.id === defaultId ? 'mdi-star' : 'mdi-star-outline'" />
              </v-btn>
              <v-btn
                variant="flat"
                size="small"
                density="comfortable"
                icon
                :style="tertiaryBtnStyle"
                class="ms-1"
                :title="$t('sessions.summary.config.export')"
                @click.stop="exportConfig(row.item)">
                <v-icon icon="mdi-download" />
              </v-btn>
              <v-btn
                v-if="row.item.canEdit"
                variant="flat"
                size="small"
                density="comfortable"
                icon
                :style="tertiaryBtnStyle"
                class="ms-1"
                :title="$t('sessions.summary.config.edit')"
                :aria-label="$t('sessions.summary.config.edit')"
                @click.stop="openEditModal(row.item)">
                <v-icon icon="mdi-pencil" />
              </v-btn>
              <v-btn
                v-if="row.item.canDelete"
                color="error"
                variant="flat"
                size="small"
                density="comfortable"
                icon
                class="ms-1"
                :title="$t('sessions.summary.config.delete')"
                :aria-label="$t('sessions.summary.config.delete')"
                @click.stop="deleteConfig(row.item)">
                <v-icon icon="mdi-trash-can-outline" />
              </v-btn>
            </span>
          </div>
        </v-list-item>
      </template>

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
  <!-- editing a saved dashboard changes its name, folder and sharing and keeps
       its own layout; the shown layout is written by "save to current" -->
  <SummaryConfigSaveModal
    :show="showSaveModal"
    :config="editingConfig ? editingConfig.data : currentConfig"
    :editing="editingConfig"
    @close="closeSaveModal"
    @saved="onConfigSaved" />
</template>

<script setup>
import { ref, computed } from 'vue';
import { useStore } from 'vuex';
import { useRoute } from 'vue-router';
import { useI18n } from 'vue-i18n';
import { createShareableService } from '../users/ShareableService';
import UserService from '../users/UserService';
import SummaryConfigSaveModal from './SummaryConfigSaveModal.vue';
import { loadSystemDashboards } from './systemDashboards';
import { dashboardRows, folderOf, loadOpenFolders, saveOpenFolders } from './dashboardFolders';

const SummaryConfigService = createShareableService('summaryConfig');

const store = useStore();
const route = useRoute();
const { t, locale } = useI18n();

const props = defineProps({
  currentConfig: {
    type: Object,
    required: true
  }
});

const emit = defineEmits(['load', 'message', 'reset', 'saved', 'changed']);

// State
const menuOpen = ref(false);
const loading = ref(false);
const error = ref('');
const configs = ref([]);
const systemDashboards = ref([]); // the viewer's *.dashboard.json files, read only
const search = ref('');
const showSaveModal = ref(false);
const editingConfig = ref(null);
const importInput = ref(null);
const savingCurrent = ref(false);

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

// the dashboard the page shows (the ?dashboard url param), highlighted in the lists
const currentId = computed(() => (typeof route.query.dashboard === 'string' ? route.query.dashboard : ''));
// the shown dashboard when it is a saved one this user may edit
const currentSaved = computed(() => configs.value.find(c => c.id === currentId.value && c.canEdit) || null);

// the folders left open, remembered per browser; system ('y:') and saved
// ('s:') folders of the same name are tracked apart
const openFolders = ref(loadOpenFolders());
const toggleFolder = (key) => {
  const next = new Set(openFolders.value);
  if (next.has(key)) { next.delete(key); } else { next.add(key); }
  openFolders.value = next;
  saveOpenFolders(next);
};
const openIn = (prefix) => new Set([...openFolders.value].filter(k => k.startsWith(prefix)).map(k => k.slice(prefix.length)));

// both lists as rows: ungrouped dashboards, then folders (open ones with
// their dashboards), everything sorted by name; the search box filters the
// dashboards and opens every folder holding a match
const systemRows = computed(() => dashboardRows(systemDashboards.value, { locale: locale.value, openFolders: openIn('y:'), term: search.value }));
const savedRows = computed(() => dashboardRows(configs.value, { locale: locale.value, openFolders: openIn('s:'), term: search.value }));

// Load the system dashboards (re-read so one added on the viewer shows up)
// and the saved configurations
const loadConfigs = async () => {
  loading.value = true;
  error.value = '';

  try {
    const [systemList, response] = await Promise.all([
      loadSystemDashboards({ refresh: true }).catch(() => systemDashboards.value),
      SummaryConfigService.list()
    ]);
    systemDashboards.value = systemList;
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

// Write the current layout back into the shown saved dashboard, keeping its
// name, description, folder and sharing as they are
const saveToCurrent = async () => {
  const target = currentSaved.value;
  if (!target || savingCurrent.value) { return; }
  savingCurrent.value = true;
  try {
    const folder = folderOf(target);
    const data = folder ? { ...props.currentConfig, folder } : props.currentConfig;
    const response = await SummaryConfigService.update(target.id, { data });
    onConfigSaved(response.shareable);
    menuOpen.value = false;
  } catch (err) {
    emit('message', {
      msg: err.text || String(err),
      type: 'danger'
    });
  } finally {
    savingCurrent.value = false;
  }
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
  // the shown dashboard was saved as a new one (not another one edited in
  // place): the page now shows that saved dashboard
  if (!editingConfig.value && savedConfig?.id) { emit('saved', savedConfig); }
  // the lists elsewhere on the page (the navigation widget) follow
  emit('changed');
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
    emit('changed');
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

// Toggle a saved or system dashboard as the default landing dashboard
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
.folder-row {
  font-weight: 600;
}
.config-current {
  background-color: rgba(var(--v-theme-foreground-accent), 0.15);
}
</style>
