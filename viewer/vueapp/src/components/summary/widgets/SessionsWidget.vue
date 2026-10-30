<!--
Copyright Andy Wick
SPDX-License-Identifier: Apache-2.0

Sessions List widget: the sessions behind the dashboard's charts as a compact
table (the dashboard's take on a saved-search document table). Self-fetches
/api/sessions (see sessionsWidgetData.js) for the widget's columns, sort and
limit; each row expands inline into the same session detail the Sessions page
shows.
-->
<template>
  <WidgetCard
    :title="title"
    :loading="loading"
    :error="error"
    :has-data="hasData"
    :info-items="infoItems"
    :link="link"
    :graph="graphDef"
    :show-export="hasData"
    :export-label="exportLabel"
    @edit="$emit('edit')"
    @remove="$emit('remove')"
    @export="exportCSV"
    @retry="fetchData">
    <div
      v-if="hasData"
      class="sessions-widget">
      <!-- its own scroller (not WidgetCard's) so the count stays put below -->
      <div class="sessions-scroll">
        <table class="sessions-widget-table">
          <thead>
            <tr>
              <th class="toggle-col" />
              <th
                v-for="col in columns"
                :key="col.exp">
                {{ col.label }}
                <v-icon
                  v-if="col.sortDb && col.sortDb === sortDb"
                  :icon="order === 'asc' ? 'mdi-chevron-up' : 'mdi-chevron-down'"
                  size="x-small" />
              </th>
            </tr>
          </thead>
          <tbody>
            <template
              v-for="(row, r) in rows"
              :key="row.id">
              <!-- striped by row, not by tr, so an open detail doesn't flip it -->
              <tr :class="{ 'row-odd': r % 2 === 0 }">
                <td class="toggle-col">
                  <toggle-btn
                    :opened="expanded.has(row.id)"
                    @toggle="toggleRow(row.id)" />
                </td>
                <td
                  v-for="col in columns"
                  :key="col.exp">
                  <!-- a custom column's parts stack, like the Sessions page -->
                  <component
                    :is="col.parts.length > 1 ? 'div' : 'span'"
                    v-for="part in col.parts"
                    :key="part.dbField">
                    <!-- each value of a multi-valued field gets its own menu -->
                    <arkime-session-field
                      v-for="(value, i) in cellValues(row, part)"
                      :key="i"
                      :field="part"
                      :session="row"
                      :expr="part.exp"
                      :value="value"
                      :parse="true"
                      :session-btn="true"
                      :pull-left="true" />
                  </component>
                </td>
              </tr>
              <tr
                v-if="expanded.has(row.id)"
                class="session-detail-row">
                <td :colspan="columns.length + 1">
                  <!-- pinned to the visible width when the table scrolls sideways -->
                  <div class="session-detail-wrap">
                    <suspense>
                      <arkime-session-detail :session="row" />
                      <template #fallback>
                        <div class="mt-1 mb-1">
                          <v-icon
                            icon="mdi-loading"
                            class="mdi-spin me-2" />
                          {{ $t('sessions.sessions.loadingSessionDetail') }}
                        </div>
                      </template>
                    </suspense>
                  </div>
                </td>
              </tr>
            </template>
          </tbody>
        </table>
      </div>
      <div class="sessions-count text-medium-emphasis">
        {{ $t('common.showingAll', { count: commaString(rows.length), total: commaString(total) }) }}
      </div>
    </div>
  </WidgetCard>
</template>

<script setup>
import { ref, reactive, computed } from 'vue';
import { useStore } from 'vuex';
import { useI18n } from 'vue-i18n';
import WidgetCard from './WidgetCard.vue';
import ToggleBtn from '@common/ToggleBtn.vue';
import ArkimeSessionField from '../../sessions/SessionField.vue';
// the Sessions page's own detail (packets, tshark, actions); it's in the main
// bundle already, so loading it lazily here would split nothing off
import ArkimeSessionDetail from '../../sessions/SessionDetail.vue';
import { commaString } from '@common/vueFilters.js';
import { useSpigraphWidget } from './useSpigraphWidget';
import { widgetFields, downloadCSV } from './widgetData';
import { fetchSessions, sessionsColumns, widgetSortDbField, cellValues, sessionsCSV } from './sessionsWidgetData';

const { t } = useI18n();
const store = useStore();

const props = defineProps({
  widget: { type: Object, required: true },
  reloadNonce: { type: Number, default: 0 },
  infoItems: { type: Array, default: () => [] },
  link: { type: Object, default: null },
  graphDef: { type: Object, default: null }
});

defineEmits(['edit', 'remove']);

const rows = ref([]);
const total = ref(0);
// ids of the rows showing their session detail
const expanded = reactive(new Set());

const { loading, error, fetchData } = useSpigraphWidget(
  () => props.widget,
  () => props.reloadNonce,
  (res) => {
    rows.value = res.rows;
    total.value = res.total;
    // keep a refetched session open; forget the ones that left the list
    const ids = new Set(res.rows.map(r => r.id));
    for (const id of [...expanded]) { if (!ids.has(id)) { expanded.delete(id); } }
  },
  fetchSessions
);

const columns = computed(() => sessionsColumns(widgetFields(props.widget)));
const sortDb = computed(() => widgetSortDbField(props.widget));
const order = computed(() => (props.widget.order === 'asc' ? 'asc' : 'desc'));

const title = computed(() => props.widget.title || t('sessions.summary.sessionsView'));

const hasData = computed(() => rows.value.length > 0);

const toggleRow = (id) => {
  if (expanded.has(id)) { expanded.delete(id); } else { expanded.add(id); }
};

const exportLabel = computed(() => t('sessions.summary.downloadCSV'));
const exportCSV = () => {
  const settings = store.state.user?.settings || {};
  const csv = sessionsCSV(columns.value, rows.value, { timezone: settings.timezone, ms: settings.ms });
  downloadCSV(csv.headers, csv.rows, 'arkime-summary-sessions.csv');
};
</script>

<style scoped>
.sessions-widget {
  flex: 1;
  display: flex;
  flex-direction: column;
  min-height: 0;
}
.sessions-scroll {
  flex: 1;
  min-height: 0;
  overflow: auto;
  /* lets the detail size itself to the visible width (cqw) */
  container-type: inline-size;
}
.sessions-count {
  flex: none;
  font-size: 0.75rem;
  text-align: right;
  padding-top: 2px;
}
.sessions-widget-table {
  width: 100%;
  border-collapse: collapse;
  font-size: 0.85rem;
}
.sessions-widget-table th,
.sessions-widget-table td {
  padding: 2px 8px;
  text-align: left;
  white-space: nowrap;
  vertical-align: middle;
}
.sessions-widget-table th {
  position: sticky;
  top: 0;
  background: rgb(var(--v-theme-quaternary-lightest));
  font-weight: 600;
  /* above the detail's own positioned bits (tabs, toolbars) */
  z-index: 2;
}
.sessions-widget-table .toggle-col {
  width: 1%;
  padding-right: 0;
}
.sessions-widget-table tbody tr.row-odd > td {
  background-color: rgb(var(--v-theme-neutral-lighter));
}
.sessions-widget-table tr.session-detail-row > td {
  white-space: normal;
  padding: 0;
}
.session-detail-wrap {
  position: sticky;
  left: 0;
  width: 100cqw;
  box-sizing: border-box;
  padding: 0 4px;
}
</style>
