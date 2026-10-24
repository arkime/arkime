<!--
Copyright Yahoo Inc.
SPDX-License-Identifier: Apache-2.0
-->
<!--
  Settings page for saved SPI Graphs (spigraphConfig shareables): what each one
  shows at a glance, open it on the SPI Graph page, and edit its name,
  description, sharing and graph settings.
-->
<template>
  <div>
    <h3 class="d-flex align-center">
      <span class="flex-grow-1">{{ $t('settings.graphs.title') }}</span>
    </h3>

    <p>
      {{ $t('settings.graphs.info') }}
    </p>

    <div class="d-flex align-center mb-2">
      <div class="flex-grow-1">
        <v-text-field
          density="compact"
          variant="outlined"
          hide-details
          clearable
          prepend-inner-icon="mdi-magnify"
          v-model="search"
          :placeholder="$t('settings.graphs.searchPlaceholder')" />
      </div>
    </div>

    <table class="arkime-table">
      <thead>
        <tr>
          <th>{{ $t('settings.graphs.table-name') }}</th>
          <th>{{ $t('settings.graphs.table-description') }}</th>
          <th>{{ $t('settings.graphs.table-type') }}</th>
          <th>{{ $t('settings.graphs.table-fields') }}</th>
          <th>{{ $t('settings.graphs.table-limit') }}</th>
          <th>{{ $t('settings.graphs.table-filter') }}</th>
          <th>{{ $t('settings.graphs.table-creator') }}</th>
          <th>{{ $t('settings.graphs.table-view') }}</th>
          <th>{{ $t('settings.graphs.table-edit') }}</th>
          <th>&nbsp;</th>
        </tr>
      </thead>
      <tbody>
        <tr
          v-for="item in visibleGraphs"
          :key="item.id">
          <td class="no-wrap">
            {{ item.name }}
          </td>
          <td>{{ item.description }}</td>
          <td class="no-wrap">
            <label class="arkime-badge arkime-badge--grey mb-0">{{ viewModeLabel(item.data.viewMode) }}</label>
          </td>
          <td>{{ fieldsLabel(item.data) }}</td>
          <td class="no-wrap">
            {{ limitLabel(item.data) }}
          </td>
          <td class="graph-filter">
            <code v-if="item.data.expression">{{ item.data.expression }}</code>
            <label
              v-if="item.data.view"
              class="arkime-badge arkime-badge--info mb-0 ms-1">{{ viewName(item.data.view) }}</label>
          </td>
          <td class="no-wrap">
            {{ item.creator }}
            <label
              v-if="item.shared"
              class="arkime-badge arkime-badge--grey ms-1 mb-0 cursor-help"
              :id="`graphShared-${item.id}`">
              {{ $t('common.shared') }}
              <v-tooltip :activator="`#graphShared-${item.id}`">
                {{ $t('common.sharedTip', { creator: item.creator }) }}
              </v-tooltip>
            </label>
          </td>
          <td>
            <label
              v-for="who in item.viewUsers"
              :key="`vu-${who}`"
              class="arkime-badge arkime-badge--grey me-1 mb-0">{{ who }}</label>
            <label
              v-for="who in item.viewRoles"
              :key="`vr-${who}`"
              class="arkime-badge arkime-badge--info me-1 mb-0">{{ who }}</label>
          </td>
          <td>
            <label
              v-for="who in item.editUsers"
              :key="`eu-${who}`"
              class="arkime-badge arkime-badge--grey me-1 mb-0">{{ who }}</label>
            <label
              v-for="who in item.editRoles"
              :key="`er-${who}`"
              class="arkime-badge arkime-badge--info me-1 mb-0">{{ who }}</label>
          </td>
          <td class="text-end no-wrap">
            <v-btn
              variant="flat"
              size="small"
              density="comfortable"
              icon
              :style="tertiaryBtnStyle"
              class="ms-1"
              :id="`openGraph-${item.id}`"
              :to="openLocation(item)">
              <v-icon icon="mdi-open-in-new" />
              <v-tooltip :activator="`#openGraph-${item.id}`">
                {{ $t('settings.graphs.openTip') }}
              </v-tooltip>
            </v-btn>
            <v-btn
              v-if="item.canDelete"
              color="error"
              variant="flat"
              size="small"
              density="comfortable"
              icon
              class="ms-1"
              :id="`deleteGraph-${item.id}`"
              @click="deleteGraph(item)">
              <v-icon icon="mdi-trash-can-outline" />
              <v-tooltip :activator="`#deleteGraph-${item.id}`">
                {{ $t('settings.graphs.deleteTip') }}
              </v-tooltip>
            </v-btn>
            <v-btn
              v-if="item.canEdit"
              variant="flat"
              size="small"
              density="comfortable"
              icon
              :style="tertiaryBtnStyle"
              class="ms-1"
              :id="`editGraph-${item.id}`"
              @click="editGraph(item)">
              <v-icon icon="mdi-pencil" />
              <v-tooltip :activator="`#editGraph-${item.id}`">
                {{ $t('settings.graphs.editTip') }}
              </v-tooltip>
            </v-btn>
          </td>
        </tr>
      </tbody>
    </table>

    <p
      v-if="graphs && !visibleGraphs.length && !listError"
      class="text-medium-emphasis mt-3">
      {{ search ? $t('settings.graphs.noneFiltered') : $t('settings.graphs.none') }}
    </p>

    <v-alert
      v-if="listError"
      type="error"
      variant="tonal"
      density="compact"
      class="mt-3">
      {{ listError }}
    </v-alert>

    <!-- edit form: name, description, sharing; the graph itself opens in the
         same editor the dashboard widgets use -->
    <v-dialog
      v-model="showEditModal"
      max-width="800">
      <v-card>
        <v-card-title>
          {{ $t('settings.graphs.editTitle') }}
        </v-card-title>
        <v-card-text>
          <div class="arkime-input-group arkime-input-group--fluid mb-2">
            <span class="arkime-input-label">{{ $t('settings.graphs.name') }}</span>
            <input
              type="text"
              class="arkime-input-control"
              v-model="editName"
              maxlength="256"
              :placeholder="$t('settings.graphs.namePlaceholder')">
          </div>
          <div class="arkime-input-group arkime-input-group--fluid mb-3">
            <span class="arkime-input-label">{{ $t('settings.graphs.description') }}</span>
            <input
              type="text"
              class="arkime-input-control"
              v-model="editDescription"
              :placeholder="$t('settings.graphs.descriptionPlaceholder')">
          </div>

          <div class="graph-summary d-flex align-center mb-3">
            <div class="flex-grow-1">
              <strong>{{ $t('settings.graphs.graph') }}:</strong>
              <label class="arkime-badge arkime-badge--grey mb-0 ms-1">{{ viewModeLabel(editData?.viewMode) }}</label>
              <span class="ms-1">{{ fieldsLabel(editData) }}</span>
              <span class="text-medium-emphasis ms-1">({{ limitLabel(editData) }})</span>
              <code
                v-if="editData?.expression"
                class="ms-1">{{ editData.expression }}</code>
            </div>
            <v-btn
              variant="flat"
              size="small"
              density="comfortable"
              :style="tertiaryBtnStyle"
              prepend-icon="mdi-pencil"
              @click="showGraphEditor = true">
              {{ $t('settings.graphs.editGraph') }}
            </v-btn>
          </div>

          <ShareInputs
            v-model="editShare"
            :roles="roles" />

          <v-alert
            v-if="formError"
            type="error"
            variant="tonal"
            density="compact"
            class="mt-3">
            {{ formError }}
          </v-alert>
        </v-card-text>
        <v-card-actions>
          <v-spacer />
          <v-btn
            variant="text"
            @click="showEditModal = false">
            {{ $t('settings.graphs.cancel') }}
          </v-btn>
          <v-btn
            color="success"
            variant="flat"
            @click="saveGraph">
            {{ $t('settings.graphs.save') }}
          </v-btn>
        </v-card-actions>
      </v-card>
    </v-dialog>

    <SummaryWidgetEditModal
      :show="showGraphEditor"
      :widget="editWidget"
      graph-only
      @close="showGraphEditor = false"
      @save="onGraphEdited" />
  </div>
</template>

<script setup>
import { ref, computed } from 'vue';
import { useStore } from 'vuex';
import { useI18n } from 'vue-i18n';
import { createShareableService } from '../users/ShareableService';
import { graphFromWidget, widgetFromGraph, spigraphQueryFromGraph } from '../summary/dashboardConfig';
import { viewModeLabelKey } from '../summary/widgets/viewModes';
import SummaryWidgetEditModal from '../summary/SummaryWidgetEditModal.vue';
import FieldService from '../search/FieldService';
import { resolveMessage } from '@common/resolveI18nMessage';
import ShareInputs from './ShareInputs.vue';

const GraphService = createShareableService('spigraphConfig');

const emit = defineEmits(['display-message']);
const store = useStore();
const { t } = useI18n();

const graphs = ref(undefined);
const search = ref('');
const listError = ref('');
const formError = ref('');
const showEditModal = ref(false);
const showGraphEditor = ref(false);
const editing = ref(undefined);
const editName = ref('');
const editDescription = ref('');
const editData = ref(null);
const editWidget = ref(null);
const editShare = ref({ viewRoles: [], editRoles: [], viewUsers: [], editUsers: [] });
// Arkime theme-color v-btn styles. Vuetify :color can't take CSS vars.
const tertiaryBtnStyle = {
  backgroundColor: 'rgb(var(--v-theme-tertiary))',
  color: 'rgb(var(--v-theme-button-fg))'
};

const roles = computed(() => store.state.roles);

const viewModeLabel = (viewMode) => {
  const key = viewModeLabelKey(viewMode);
  return key ? t(key) : (viewMode || '');
};
const fieldName = (exp) => FieldService.getField(exp, true)?.friendlyName || exp;
// the fields a graph draws, the bar's split field included
const fieldsLabel = (data) => {
  if (!data) { return ''; }
  const names = data.fields.filter(Boolean).map(fieldName);
  if (data.viewMode === 'bar' && data.splitField) { names.push(fieldName(data.splitField)); }
  return names.join(', ');
};
const limitLabel = (data) => {
  if (!data) { return ''; }
  const order = data.order === 'asc' ? t('sessions.summary.bottom') : t('sessions.summary.top');
  return `${order} ${data.length}`;
};
const viewName = (id) => (store.state.views || []).find(v => v.id === id || v.name === id)?.name || id;

const visibleGraphs = computed(() => {
  const term = search.value?.trim().toLowerCase();
  if (!term) { return graphs.value || []; }
  return (graphs.value || []).filter((g) => {
    return `${g.name || ''} ${g.description || ''} ${fieldsLabel(g.data)} ${g.data.expression}`.toLowerCase().includes(term);
  });
});

// yours first, then the ones shared with you, each still in the name order
// the API returned; shared graphs are other users' data, so normalized first
const loadData = async () => {
  try {
    // no pager here, so ask for more than the API's default page of 50
    const response = await GraphService.list({ viewOnly: false, length: 1000 });
    const data = (response.data || []).map(g => ({ ...g, data: graphFromWidget(g.data) }));
    listError.value = '';
    graphs.value = [...data.filter(g => !g.shared), ...data.filter(g => g.shared)];
  } catch (error) {
    graphs.value = [];
    listError.value = resolveMessage(error, t);
  }
};

// the SPI Graph page showing this graph, as loaded from the menu there
const openLocation = (item) => {
  const q = spigraphQueryFromGraph(item.data) || {};
  return { path: '/spigraph', query: { ...q, graphId: item.id, expression: item.data.expression || undefined } };
};

const editGraph = (item) => {
  formError.value = '';
  editing.value = item;
  editName.value = item.name ?? '';
  editDescription.value = item.description ?? '';
  editData.value = graphFromWidget(item.data);
  editWidget.value = widgetFromGraph(editData.value);
  editShare.value = {
    viewRoles: [...(item.viewRoles ?? [])],
    editRoles: [...(item.editRoles ?? [])],
    viewUsers: [...(item.viewUsers ?? [])],
    editUsers: [...(item.editUsers ?? [])]
  };
  showEditModal.value = true;
};

// the widget editor hands back a widget; only its graph part is kept
const onGraphEdited = (widget) => {
  showGraphEditor.value = false;
  editData.value = graphFromWidget(widget);
  editWidget.value = widgetFromGraph(editData.value);
};

const saveGraph = async () => {
  editName.value = editName.value.trim();
  if (!editName.value) {
    formError.value = t('settings.graphs.nameRequired');
    return;
  }

  const body = {
    name: editName.value,
    description: editDescription.value,
    data: editData.value,
    ...editShare.value
  };

  try {
    const response = await GraphService.update(editing.value.id, body);
    showEditModal.value = false;
    emit('display-message', { msg: resolveMessage(response, t), type: 'success' });
    loadData();
  } catch (error) {
    formError.value = resolveMessage(error, t);
  }
};

const deleteGraph = async (item) => {
  if (!window.confirm(t('settings.graphs.deleteConfirm', { name: item.name }))) {
    return;
  }

  try {
    const response = await GraphService.delete(item.id);
    graphs.value = graphs.value.filter(g => g.id !== item.id);
    emit('display-message', { msg: resolveMessage(response, t), type: 'success' });
  } catch (error) {
    emit('display-message', { msg: resolveMessage(error, t), type: 'danger' });
  }
};

loadData();
</script>

<style scoped>
.graph-filter code {
  font-size: 0.8rem;
}
.graph-summary {
  padding: 0.4rem 0.6rem;
  border: 1px solid rgb(var(--v-theme-border-color, var(--v-theme-on-surface)), 0.2);
  border-radius: 3px;
}
</style>
