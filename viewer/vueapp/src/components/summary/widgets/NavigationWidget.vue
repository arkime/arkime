<!--
Copyright Yahoo Inc.
SPDX-License-Identifier: Apache-2.0

Navigation widget: links to the system dashboards (one per protocol) and
the user's saved dashboards, so a dashboard can lead to the others the way a
navigation panel does. Each link opens the Arkime tab with that dashboard
(?dashboard=<id>) and keeps the current search and time range. It is one
wrapping strip of chips, so it fits a one-row widget: each folder's name sits
inline before its dashboards.
-->
<template>
  <WidgetCard
    :title="title"
    :loading="loading"
    :has-data="true"
    :info-items="infoItems"
    :link="link"
    @edit="$emit('edit')"
    @remove="$emit('remove')">
    <!-- one wrapping strip: a section label, its ungrouped dashboards, then
         each folder's name followed by its dashboards -->
    <div class="nav-widget">
      <template
        v-for="section in sections"
        :key="section.key">
        <span class="nav-group-label text-medium-emphasis">
          {{ section.label }}
        </span>
        <router-link
          v-for="d in section.top"
          :key="d.id"
          :to="linkTo(d.id)"
          class="nav-chip"
          :class="{ 'nav-chip--current': d.id === currentId }"
          :title="d.description || d.name">
          <v-icon
            v-if="d.shared"
            icon="mdi-share-all"
            size="x-small"
            class="me-1" />
          {{ d.name }}
        </router-link>
        <span
          v-if="section.empty"
          class="text-medium-emphasis nav-none">
          {{ $t('sessions.summary.navigation.none') }}
        </span>
        <template
          v-for="f in section.folders"
          :key="section.key + f.name">
          <span class="nav-folder-label text-medium-emphasis">
            <v-icon
              icon="mdi-folder-outline"
              size="x-small"
              class="me-1" />{{ f.name }}
          </span>
          <router-link
            v-for="d in f.items"
            :key="d.id"
            :to="linkTo(d.id)"
            class="nav-chip"
            :class="{ 'nav-chip--current': d.id === currentId }"
            :title="d.description || d.name">
            <v-icon
              v-if="d.shared"
              icon="mdi-share-all"
              size="x-small"
              class="me-1" />
            {{ d.name }}
          </router-link>
        </template>
      </template>
    </div>
  </WidgetCard>
</template>

<script setup>
import { ref, computed, onMounted, watch } from 'vue';
import { useRoute } from 'vue-router';
import { useI18n } from 'vue-i18n';
import WidgetCard from './WidgetCard.vue';
import { createShareableService } from '../../users/ShareableService';
import { loadSystemDashboards } from '../systemDashboards';
import { groupDashboards } from '../dashboardFolders';

const DashboardService = createShareableService('summaryConfig');
const { t, locale } = useI18n();
const route = useRoute();

const props = defineProps({
  widget: { type: Object, required: true },
  // bumped by the page when a dashboard is saved, renamed or deleted
  dashboardsVersion: { type: Number, default: 0 },
  infoItems: { type: Array, default: () => [] },
  link: { type: Object, default: null }
});

defineEmits(['edit', 'remove']);

const title = computed(() => props.widget.title || t('sessions.summary.navigationView'));
const saved = ref([]);
const system = ref([]);
const loading = ref(false);

// the system and saved dashboards, each grouped by folder and sorted by
// name (the same grouping the dashboards dropdown shows)
const sections = computed(() => [
  { key: 'system', label: t('sessions.summary.navigation.system'), ...groupDashboards(system.value, locale.value) },
  { key: 'saved', label: t('sessions.summary.navigation.saved'), empty: !loading.value && !saved.value.length, ...groupDashboards(saved.value, locale.value) }
]);

const currentId = computed(() => (typeof route.query.dashboard === 'string' ? route.query.dashboard : ''));

// the same search and time, a different dashboard
const linkTo = (id) => ({ path: '/arkime', query: { ...route.query, dashboard: id } });

const load = async () => {
  loading.value = true;
  try {
    const [systemList, response] = await Promise.all([
      loadSystemDashboards({ refresh: true }).catch(() => []),
      DashboardService.list({ length: 1000 })
    ]);
    system.value = systemList.map(d => ({ id: d.id, name: d.name, description: d.description, folder: d.data?.folder }));
    saved.value = (response?.data || []).map(d => ({ id: d.id, name: d.name, description: d.description, shared: d.shared, folder: d.data?.folder }));
  } catch (err) {
    saved.value = [];
  } finally {
    loading.value = false;
  }
};

onMounted(load);
// the lists don't depend on the search or on which dashboard is shown (the
// highlight follows the url on its own), so only a save, rename or delete
// refetches them
watch(() => props.dashboardsVersion, load);
</script>

<style scoped>
.nav-widget {
  flex: 1;
  min-height: 0;
  overflow: auto;
  display: flex;
  flex-wrap: wrap;
  align-content: flex-start;
  /* labels and chips differ in font size: aligning their baselines puts every
     piece of the strip on one line (centering them would float the small ones) */
  align-items: baseline;
  gap: 0.25rem 0.3rem;
}
.nav-group-label,
.nav-folder-label {
  display: inline;
  white-space: nowrap;
}
/* the folder icon rides on the label's baseline, not above it */
.nav-folder-label :deep(.v-icon) {
  vertical-align: -0.15em;
}
.nav-group-label {
  font-size: 0.68rem;
  text-transform: uppercase;
  letter-spacing: 0.04em;
  margin-right: 0.15rem;
}
.nav-group-label:not(:first-child) {
  margin-left: 0.6rem;
}
.nav-folder-label {
  font-size: 0.7rem;
  margin-left: 0.45rem;
  margin-right: 0.1rem;
}
.nav-chip {
  display: inline-flex;
  align-items: center;
  padding: 0 0.5rem;
  line-height: 1.35rem;
  border-radius: 999px;
  font-size: 0.78rem;
  text-decoration: none;
  color: rgb(var(--v-theme-foreground));
  background: rgb(var(--v-theme-quaternary-lightest));
  border: 1px solid transparent;
  white-space: nowrap;
}
.nav-chip:hover {
  border-color: rgb(var(--v-theme-foreground-accent));
}
.nav-chip--current {
  background: rgb(var(--v-theme-foreground-accent));
  color: rgb(var(--v-theme-button-fg));
}
.nav-none {
  font-size: 0.78rem;
}
</style>
