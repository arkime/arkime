<!--
Copyright Yahoo Inc.
SPDX-License-Identifier: Apache-2.0

Saved graphs (spigraphConfig shareables) on the SPI Graph page: save what the
page shows, load a saved graph, share it, and add it (or the current graph) to
a dashboard as a widget.
-->
<template>
  <v-menu
    v-model="menuOpen"
    :close-on-content-click="false"
    @update:model-value="(opened) => { if (opened) loadGraphs(); }">
    <template #activator="{ props: activatorProps }">
      <v-btn
        v-bind="activatorProps"
        variant="outlined"
        size="small"
        density="comfortable"
        class="ms-1 px-2"
        min-width="0"
        :aria-label="$t('spigraph.graphs.graphs')">
        <v-icon icon="mdi-content-save" />
        <v-tooltip activator="parent">
          {{ $t('spigraph.graphs.tip') }}
        </v-tooltip>
      </v-btn>
    </template>

    <v-list
      density="compact"
      class="graphs-list">
      <v-list-item>
        <v-text-field
          v-model="search"
          density="compact"
          variant="outlined"
          hide-details
          clearable
          prepend-inner-icon="mdi-magnify"
          :placeholder="$t('spigraph.graphs.searchGraphs')" />
      </v-list-item>

      <v-divider />

      <!-- the loaded graph, if any, takes the page's current state -->
      <v-list-item
        v-if="loadedGraph && loadedGraph.canEdit"
        :disabled="!pageGraph || !dirty"
        @click="saveOver">
        <v-icon
          icon="mdi-content-save"
          class="me-1" />
        {{ $t('spigraph.graphs.saveOver', { name: loadedGraph.name }) }}
        <v-tooltip
          activator="parent"
          location="end">
          {{ $t('spigraph.graphs.saveOverTip') }}
        </v-tooltip>
      </v-list-item>
      <v-list-item
        :disabled="!pageGraph"
        @click="openSaveModal(null)">
        <v-icon
          :icon="loadedGraph ? 'mdi-content-save-plus' : 'mdi-content-save'"
          class="me-1" />
        {{ loadedGraph ? $t('spigraph.graphs.saveAs') : $t('spigraph.graphs.saveCurrent') }}
      </v-list-item>
      <v-list-item
        :disabled="!pageGraph"
        @click="openAddModal(pageGraph)">
        <v-icon
          icon="mdi-view-dashboard-outline"
          class="me-1" />
        {{ $t('spigraph.graphs.addCurrentToDashboard') }}
      </v-list-item>

      <v-divider v-if="filteredGraphs.length > 0 || loading" />

      <v-list-item
        v-if="loading"
        disabled>
        <v-icon
          icon="mdi-loading"
          class="mdi-spin me-1" />
        {{ $t('common.loading') }}
      </v-list-item>

      <v-list-item
        v-if="error && !loading"
        disabled
        class="text-danger">
        <v-icon
          icon="mdi-alert"
          class="me-1" />
        {{ error }}
      </v-list-item>

      <v-list-item
        v-for="graph in filteredGraphs"
        :key="graph.id"
        @click="loadGraph(graph)">
        <div class="d-flex justify-space-between align-center w-100">
          <span class="graph-name">
            <v-icon
              v-if="graph.id === loadedGraphId"
              icon="mdi-check"
              class="me-1 text-theme-accent"
              :title="$t('spigraph.graphs.loaded')" />
            <v-icon
              v-if="graph.shared"
              icon="mdi-share-all"
              class="me-1" />
            {{ graph.name }}
            <small
              v-if="graph.creator && graph.creator !== user.userId"
              class="text-medium-emphasis ms-1">
              ({{ graph.creator }})
            </small>
          </span>
          <span
            class="graph-actions"
            @click.stop>
            <v-btn
              variant="flat"
              size="small"
              density="comfortable"
              icon
              :style="tertiaryBtnStyle"
              class="ms-1"
              :title="$t('spigraph.graphs.addToDashboard')"
              @click.stop="openAddModal(graph.data)">
              <v-icon icon="mdi-view-dashboard-outline" />
            </v-btn>
            <v-btn
              v-if="graph.canEdit"
              variant="flat"
              size="small"
              density="comfortable"
              icon
              :style="tertiaryBtnStyle"
              class="ms-1"
              :title="$t('spigraph.graphs.editGraph')"
              :aria-label="$t('spigraph.graphs.editGraph')"
              @click.stop="openSaveModal(graph)">
              <v-icon icon="mdi-pencil" />
            </v-btn>
            <v-btn
              v-if="graph.canDelete"
              color="error"
              variant="flat"
              size="small"
              density="comfortable"
              icon
              class="ms-1"
              :title="$t('spigraph.graphs.delete')"
              :aria-label="$t('spigraph.graphs.delete')"
              @click.stop="deleteGraph(graph)">
              <v-icon icon="mdi-trash-can-outline" />
            </v-btn>
          </span>
        </div>
      </v-list-item>

      <v-list-item
        v-if="!loading && !error && graphs.length === 0"
        disabled>
        <span class="text-medium-emphasis">
          {{ $t('spigraph.graphs.noGraphs') }}
        </span>
      </v-list-item>
    </v-list>
  </v-menu>
  <!-- the dialogs teleport to body; siblings so they don't wrap the menu -->
  <SummaryConfigSaveModal
    kind="graph"
    :show="showSaveModal"
    :config="editingGraph ? editingGraph.data : pageGraph"
    :editing="editingGraph"
    @close="closeSaveModal"
    @saved="onGraphSaved" />
  <AddToDashboardModal
    :show="showAddModal"
    :graph="graphToAdd"
    @close="showAddModal = false"
    @added="onAdded" />
</template>

<script setup>
import { ref, computed, onMounted } from 'vue';
import { useStore } from 'vuex';
import { useI18n } from 'vue-i18n';
import { createShareableService } from '../users/ShareableService';
import SummaryConfigSaveModal from '../summary/SummaryConfigSaveModal.vue';
import AddToDashboardModal from '../summary/AddToDashboardModal.vue';
import { graphFromWidget, graphOnSpigraph } from '../summary/dashboardConfig';

const GraphService = createShareableService('spigraphConfig');

const store = useStore();
const { t } = useI18n();

const props = defineProps({
  // the graph the page is showing (null when its type has no graph)
  currentGraph: { type: Object, default: null },
  // the saved graph the page loaded (from the url), if any
  loadedGraphId: { type: String, default: '' }
});

const emit = defineEmits(['load', 'message', 'saved-as']);

const menuOpen = ref(false);
const loading = ref(false);
const error = ref('');
const graphs = ref([]);
const search = ref('');
const showSaveModal = ref(false);
const editingGraph = ref(null);
const showAddModal = ref(false);
const graphToAdd = ref(null);

const tertiaryBtnStyle = {
  backgroundColor: 'rgb(var(--v-theme-tertiary))',
  color: 'rgb(var(--v-theme-button-fg))'
};

const user = computed(() => store.state.user);

// the loaded graph's details come from the list, fetched up front when the
// page starts with a graph in its url
const loadedGraph = computed(() => (props.loadedGraphId ? graphs.value.find(g => g.id === props.loadedGraphId) || null : null));
// what the page shows on top of the loaded graph, so the settings SPI Graph
// can't show (metric columns, aggregation, order...) aren't lost on a save
const pageGraph = computed(() => graphOnSpigraph(loadedGraph.value?.data || null, props.currentGraph));
const sameGraph = (a, b) => JSON.stringify(graphFromWidget(a)) === JSON.stringify(graphFromWidget(b));
const dirty = computed(() => !!(loadedGraph.value && pageGraph.value && !sameGraph(loadedGraph.value.data, pageGraph.value)));
onMounted(() => { if (props.loadedGraphId) { loadGraphs(); } });

const filteredGraphs = computed(() => {
  const term = search.value?.trim().toLowerCase();
  if (!term) { return graphs.value; }
  return graphs.value.filter(g => (g.name || '').toLowerCase().includes(term));
});

const loadGraphs = async () => {
  loading.value = true;
  error.value = '';
  try {
    const response = await GraphService.list();
    // shared graphs are other users' data: normalize them before use
    graphs.value = (response.data || []).map(g => ({ ...g, data: graphFromWidget(g.data) }));
  } catch (err) {
    error.value = err.text || String(err);
    graphs.value = [];
  } finally {
    loading.value = false;
  }
};

const loadGraph = (graph) => {
  emit('load', graph);
  menuOpen.value = false;
};

// the loaded graph takes the page's current state; name and sharing stay
const saveOver = async () => {
  if (!loadedGraph.value || !pageGraph.value) { return; }
  try {
    await GraphService.update(loadedGraph.value.id, { data: pageGraph.value });
    await loadGraphs();
    emit('message', { msg: t('spigraph.graphs.saved'), type: 'success' });
    menuOpen.value = false;
  } catch (err) {
    emit('message', { msg: err.text || String(err), type: 'danger' });
  }
};

const openSaveModal = (graph) => {
  editingGraph.value = graph;
  showSaveModal.value = true;
};

const closeSaveModal = () => {
  showSaveModal.value = false;
  editingGraph.value = null;
};

const onGraphSaved = (saved) => {
  // a new graph becomes the loaded one, so later changes save back to it
  if (!editingGraph.value && saved?.id) { emit('saved-as', saved.id); }
  loadGraphs();
  emit('message', { msg: t('spigraph.graphs.saved'), type: 'success' });
};

const openAddModal = (graph) => {
  graphToAdd.value = graph;
  showAddModal.value = true;
  menuOpen.value = false;
};

const onAdded = ({ name: dashboardName }) => {
  emit('message', { msg: t('sessions.summary.addToDashboard.added', { name: dashboardName }), type: 'success', link: '/arkime' });
};

const deleteGraph = async (graph) => {
  try {
    await GraphService.delete(graph.id);
    graphs.value = graphs.value.filter(g => g.id !== graph.id);
    emit('message', { msg: t('spigraph.graphs.deleted'), type: 'success' });
  } catch (err) {
    emit('message', { msg: err.text || String(err), type: 'danger' });
  }
};
</script>

<style scoped>
.graph-name {
  flex-grow: 1;
  overflow: hidden;
  text-overflow: ellipsis;
  white-space: nowrap;
  max-width: 220px;
}
.graph-actions {
  flex-shrink: 0;
  opacity: 0;
  transition: opacity 0.15s;
}
.graphs-list :deep(.v-list-item:hover) .graph-actions {
  opacity: 1;
}
.graphs-list {
  min-width: 320px;
}
</style>
