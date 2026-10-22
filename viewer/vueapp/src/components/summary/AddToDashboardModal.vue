<!--
Copyright Yahoo Inc.
SPDX-License-Identifier: Apache-2.0

Adds a graph (a saved one, or what SPI Graph is showing) to a dashboard as a
new widget: the dashboard the Arkime tab shows, or any saved dashboard the
user can edit. The widget is a copy of the graph plus the layout chosen here.
-->
<template>
  <v-dialog
    :model-value="show"
    max-width="600"
    @update:model-value="(val) => { if (!val) $emit('close'); }">
    <v-card density="compact">
      <v-card-title>
        {{ $t('sessions.summary.addToDashboard.title') }}
      </v-card-title>
      <v-card-text>
        <v-select
          v-model="target"
          :items="targets"
          :label="$t('sessions.summary.addToDashboard.dashboard')"
          :loading="loading"
          density="compact"
          variant="outlined"
          hide-details
          class="mb-3" />

        <div class="arkime-input-group arkime-input-group--fluid mb-3">
          <span class="arkime-input-label">
            {{ $t('sessions.summary.widget.title') }}
          </span>
          <input
            v-model="title"
            type="text"
            class="arkime-input-control"
            :placeholder="defaultTitle">
        </div>

        <div class="d-flex flex-wrap gap-3 mb-1">
          <v-select
            v-model="width"
            :items="widthItems"
            :label="$t('sessions.summary.widget.width')"
            density="compact"
            variant="outlined"
            hide-details
            style="min-width: 150px" />
          <v-select
            v-model="height"
            :items="heightItems"
            :label="$t('sessions.summary.widget.height')"
            density="compact"
            variant="outlined"
            hide-details
            style="min-width: 150px" />
        </div>

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
            :disabled="saving || !graph || !target"
            @click="add">
            <v-icon
              v-if="saving"
              icon="mdi-loading"
              class="mdi-spin me-1" />
            <v-icon
              v-else
              icon="mdi-view-dashboard-outline"
              class="me-1" />
            {{ $t('sessions.summary.addToDashboard.add') }}
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
import { createShareableService } from '../users/ShareableService';
import UserService from '../users/UserService';
import FieldService from '../search/FieldService';
import { widgetFromGraph, widgetsFromConfig, toV6Shape } from './dashboardConfig';

const DashboardService = createShareableService('summaryConfig');

const store = useStore();
const { t } = useI18n();

const props = defineProps({
  show: { type: Boolean, default: false },
  // saved-graph data (a widget definition without layout)
  graph: { type: Object, default: null }
});

const emit = defineEmits(['close', 'added']);

const CURRENT = '__current__';

const target = ref(CURRENT);
const title = ref('');
const width = ref(2);
const height = ref(3);
const dashboards = ref([]);
// the dashboard the Arkime tab shows: { id, name, canEdit } when that is the
// user's default dashboard shareable, { id: null } for the per-user state
const current = ref({ id: null });
const loading = ref(false);
const saving = ref(false);
const error = ref('');

const widthItems = [1, 2, 3, 4].map(n => ({ title: String(n), value: n }));
const heightItems = [1, 2, 3, 4, 5, 6, 7, 8].map(n => ({ title: String(n), value: n }));

// the current dashboard (named when it is a shared one, which can only be
// picked when the user may edit it), then the other saved ones they may edit
const targets = computed(() => {
  const cur = current.value;
  let currentItem = { title: t('sessions.summary.addToDashboard.current'), value: CURRENT };
  if (cur.id) {
    currentItem = cur.canEdit
      ? { title: t('sessions.summary.addToDashboard.currentNamed', { name: cur.name }), value: CURRENT }
      : { title: t('sessions.summary.addToDashboard.currentReadOnly', { name: cur.name }), value: CURRENT, props: { disabled: true } };
  }
  return [
    currentItem,
    ...dashboards.value.filter(d => d.canEdit && d.id !== cur.id).map(d => ({ title: d.name, value: d.id }))
  ];
});

// found the way the Arkime tab loads its dashboard: the default dashboard
// shareable when one is set and still readable, else the per-user state
const resolveCurrent = async () => {
  const defaultId = store.state.user?.settings?.defaultDashboardId;
  if (!defaultId) { return { id: null }; }
  try {
    const response = await DashboardService.get(defaultId);
    const shareable = response?.shareable;
    if (shareable?.data) { return { id: defaultId, name: shareable.name || defaultId, canEdit: !!shareable.canEdit }; }
  } catch (err) {
    // deleted or no longer shared: the Arkime tab falls back to the state too
  }
  return { id: null };
};

const defaultTitle = computed(() => {
  const exp = props.graph?.fields?.[0] || props.graph?.field;
  return (exp && FieldService.getField(exp, true)?.friendlyName) || exp || '';
});

watch(() => props.show, async (isOpen) => {
  if (!isOpen) { return; }
  error.value = '';
  title.value = '';
  width.value = 2;
  height.value = 3;
  target.value = CURRENT;
  current.value = { id: null };
  loading.value = true;
  try {
    // every dashboard the user may edit, including ones only shared for editing
    const [response, cur] = await Promise.all([DashboardService.list({ viewOnly: false, length: 1000 }), resolveCurrent()]);
    dashboards.value = response.data || [];
    current.value = cur;
  } catch (err) {
    dashboards.value = [];
    error.value = err.text || String(err);
  } finally {
    loading.value = false;
  }
  // a view-only current dashboard can't take the widget: start on another
  if (current.value.id && !current.value.canEdit) {
    target.value = targets.value.find(i => i.value !== CURRENT)?.value || null;
  }
});

// The per-user state when that is what the Arkime tab shows, else the default
// dashboard shareable (an older dashboard keeps its widgets: a v6 fields[]
// one is migrated first)
const appendToCurrent = async (widget) => {
  if (current.value.id) { return appendToShareable(current.value.id, widget); }

  let cfg;
  try {
    const response = await UserService.getPageConfig('summary');
    cfg = response?.summaryConfig;
  } catch (err) {
    cfg = undefined;
  }
  const all = [...widgetsFromConfig(cfg), widget];
  await UserService.saveState({ colorScheme: cfg?.colorScheme || 'rainbow', widgets: all, ...toV6Shape(all) }, 'summary');
  return t('sessions.summary.addToDashboard.current');
};

const appendToShareable = async (id, widget) => {
  const response = await DashboardService.get(id);
  const data = response?.shareable?.data || {};
  const all = [...widgetsFromConfig(data), widget];
  await DashboardService.update(id, { data: { ...data, widgets: all, ...toV6Shape(all) } });
  return response?.shareable?.name || id;
};

const add = async () => {
  if (!props.graph) { return; }
  error.value = '';
  saving.value = true;
  try {
    const widget = widgetFromGraph(props.graph, { title: title.value.trim(), width: width.value, height: height.value });
    const targetName = target.value === CURRENT
      ? await appendToCurrent(widget)
      : await appendToShareable(target.value, widget);
    emit('added', { name: targetName, current: target.value === CURRENT });
    emit('close');
  } catch (err) {
    error.value = err.text || err.message || String(err);
  } finally {
    saving.value = false;
  }
};
</script>

<style scoped>
.arkime-input-group {
  background-color: rgb(var(--v-theme-input-bg));
  border: 1px solid rgb(var(--v-theme-input-border));
}
</style>
