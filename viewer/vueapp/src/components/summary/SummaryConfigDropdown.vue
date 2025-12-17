<!--
Copyright Yahoo Inc.
SPDX-License-Identifier: Apache-2.0
-->
<template>
  <div class="summary-config-dropdown d-inline-block">
    <b-dropdown
      size="sm"
      no-caret
      variant="theme-primary"
      @show="loadConfigs">
      <template #button-content>
        <span
          class="fa fa-save"
          id="summary-config-dropdown-btn">
          <BTooltip
            target="summary-config-dropdown-btn"
            noninteractive
            placement="right"
            boundary="viewport"
            teleport-to="body">{{ $t('sessions.summary.config.configurations') }}</BTooltip>
        </span>
      </template>
      <!-- Loading indicator -->
      <b-dropdown-item
        v-if="loading"
        disabled>
        <span class="fa fa-spinner fa-spin me-1" />
        {{ $t('common.loading') }}
      </b-dropdown-item>

      <!-- Error message -->
      <b-dropdown-item
        v-if="error && !loading"
        disabled
        class="text-danger">
        <span class="fa fa-exclamation-triangle me-1" />
        {{ error }}
      </b-dropdown-item>

      <!-- Save current config -->
      <b-dropdown-item @click="openSaveModal">
        <span class="fa fa-save me-1" />
        {{ $t('sessions.summary.config.saveCurrent') }}
      </b-dropdown-item>

      <!-- Reset to defaults -->
      <b-dropdown-item @click="resetToDefaults">
        <span class="fa fa-refresh me-1" />
        {{ $t('sessions.summary.config.resetToDefault') }}
      </b-dropdown-item>

      <b-dropdown-divider v-if="configs.length > 0" />

      <!-- Saved configurations -->
      <b-dropdown-item
        v-for="config in configs"
        :key="config.id"
        @click="loadConfig(config)">
        <div class="d-flex justify-content-between align-items-center w-100">
          <span class="config-name">
            {{ config.name }}
            <small
              v-if="config.creator && config.creator !== user.userId"
              class="text-muted ms-1">
              ({{ config.creator }})
            </small>
          </span>
          <span
            class="config-actions"
            @click.stop>
            <button
              v-if="config.canEdit"
              class="btn btn-xs btn-theme-tertiary ms-1"
              :title="$t('sessions.summary.config.edit')"
              @click.stop="openEditModal(config)">
              <span class="fa fa-pencil" />
            </button>
            <button
              v-if="config.canDelete"
              class="btn btn-xs btn-danger ms-1"
              :title="$t('sessions.summary.config.delete')"
              @click.stop="deleteConfig(config)">
              <span class="fa fa-trash-o" />
            </button>
          </span>
        </div>
      </b-dropdown-item>

      <!-- No configs message -->
      <b-dropdown-item
        v-if="!loading && !error && configs.length === 0"
        disabled>
        <span class="text-muted">
          {{ $t('sessions.summary.config.noConfigs') }}
        </span>
      </b-dropdown-item>
    </b-dropdown>

    <!-- Save/Edit Modal -->
    <SummaryConfigSaveModal
      :show="showSaveModal"
      :config="currentConfig"
      :editing="editingConfig"
      @close="closeSaveModal"
      @saved="onConfigSaved" />
  </div>
</template>

<script setup>
import { ref, computed } from 'vue';
import { useStore } from 'vuex';
import { useI18n } from 'vue-i18n';
import { createShareableService } from '../users/ShareableService';
import SummaryConfigSaveModal from './SummaryConfigSaveModal.vue';

const SummaryConfigService = createShareableService('summaryConfig');

const store = useStore();
const { t } = useI18n();

defineProps({
  currentConfig: {
    type: Object,
    required: true
  }
});

const emit = defineEmits(['load', 'message', 'reset']);

// State
const loading = ref(false);
const error = ref('');
const configs = ref([]);
const showSaveModal = ref(false);
const editingConfig = ref(null);

// Get user from store
const user = computed(() => store.state.user);

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
};

// Reset to default configuration
const resetToDefaults = () => {
  emit('reset');
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

.dropdown-item:hover .config-actions {
  opacity: 1;
}

.btn-xs {
  padding: 0.1rem 0.3rem;
  font-size: 0.7rem;
}
</style>
