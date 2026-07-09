<!--
Copyright Yahoo Inc.
SPDX-License-Identifier: Apache-2.0
-->
<template>
  <v-dialog
    :model-value="show"
    max-width="700"
    @update:model-value="(val) => { if (!val) $emit('close'); }">
    <v-card density="compact">
      <v-card-title>
        {{ $t('sessions.summary.editWidget') }}
      </v-card-title>
      <v-card-text>
        <!-- Visualization type leads: it decides which inputs below apply -->
        <v-select
          v-model="form.viewMode"
          :items="viewModeItems"
          :label="$t('sessions.summary.widget.viewMode')"
          density="compact"
          variant="outlined"
          hide-details
          class="mb-3" />

        <!-- Field selector: a chips multi-select (up to 3) for pie/treemap/table/
             intersection, otherwise a single typeahead. Grayed for session types. -->
        <div
          class="arkime-input-group arkime-input-group--fluid mb-3"
          :class="{ 'input-disabled': !needsField }">
          <span class="arkime-input-label">
            {{ multiField ? $t('sessions.summary.widget.fields') : $t('sessions.summary.widget.field') }}<sup v-if="needsField">*</sup>
          </span>
          <v-autocomplete
            v-if="multiField"
            v-model="form.fields"
            :items="fieldItems"
            :placeholder="$t('sessions.summary.widget.fieldsHint')"
            multiple
            chips
            closable-chips
            density="compact"
            variant="outlined"
            hide-details
            auto-select-first />
          <!-- map: geo fields only -->
          <ArkimeFieldTypeahead
            v-else-if="geoMode"
            :fields="geoFields"
            :initial-value="fieldFriendlyName"
            @field-selected="onFieldSelected" />
          <ArkimeFieldTypeahead
            v-else
            :fields="fields"
            :initial-value="fieldFriendlyName"
            @field-selected="onFieldSelected" />
        </div>

        <!-- Title override -->
        <div class="arkime-input-group arkime-input-group--fluid mb-3">
          <span class="arkime-input-label">
            {{ $t('sessions.summary.widget.title') }}
          </span>
          <input
            v-model="form.title"
            type="text"
            class="arkime-input-control"
            :placeholder="titlePlaceholder">
        </div>

        <div class="d-flex flex-wrap gap-3 mb-3">
          <!-- Metric basis: Sessions count, or numeric field(s) summed per value.
               Tables take several metric columns + a sort-by; charts take one. -->
          <template v-if="multiMetric">
            <v-autocomplete
              v-model="form.metrics"
              :items="metricItems"
              :label="$t('sessions.summary.widget.metrics')"
              :disabled="!metricEnabled"
              multiple
              chips
              closable-chips
              density="compact"
              variant="outlined"
              hide-details
              auto-select-first
              style="min-width: 260px" />
            <v-select
              v-model="form.sortMetric"
              :items="sortByItems"
              :label="$t('sessions.summary.widget.sortBy')"
              :disabled="!metricEnabled"
              density="compact"
              variant="outlined"
              hide-details
              style="min-width: 160px" />
          </template>
          <v-autocomplete
            v-else
            v-model="form.metricType"
            :items="metricItems"
            :label="$t('sessions.summary.widget.metric')"
            :disabled="!metricEnabled"
            density="compact"
            variant="outlined"
            hide-details
            auto-select-first
            style="min-width: 200px" />
        </div>

        <div class="d-flex flex-wrap gap-3 mb-3">
          <!-- Data limit (Top/Bottom N) -->
          <v-select
            v-model="form.length"
            :items="lengthItems"
            :label="$t('sessions.summary.widget.limit')"
            :disabled="!aggEnabled"
            density="compact"
            variant="outlined"
            hide-details
            style="min-width: 120px" />

          <!-- Order (direction) -->
          <v-select
            v-model="form.order"
            :items="orderItems"
            :label="$t('sessions.summary.widget.order')"
            :disabled="!aggEnabled"
            density="compact"
            variant="outlined"
            hide-details
            style="min-width: 120px" />
        </div>

        <div class="d-flex flex-wrap gap-3 mb-3">
          <!-- Width -->
          <v-select
            v-model="form.width"
            :items="widthItems"
            :label="$t('sessions.summary.widget.width')"
            density="compact"
            variant="outlined"
            hide-details
            style="min-width: 150px" />

          <!-- Height -->
          <v-select
            v-model="form.height"
            :items="heightItems"
            :label="$t('sessions.summary.widget.height')"
            density="compact"
            variant="outlined"
            hide-details
            style="min-width: 150px" />
        </div>

        <!-- Local filter: optional saved View + expression (combined with AND, on
             top of the global search). Disabled for session-wide widgets, which
             always describe the whole result set. -->
        <v-select
          v-if="viewsSupported"
          v-model="form.view"
          :items="viewItems"
          :label="$t('sessions.summary.widget.localView')"
          :disabled="!localFilterEnabled"
          density="compact"
          variant="outlined"
          hide-details
          class="mb-3">
          <template #append-item>
            <v-list-item
              v-if="!hasViews"
              disabled
              class="text-caption text-medium-emphasis">
              {{ $t('sessions.summary.widget.noViewsHint') }}
            </v-list-item>
          </template>
        </v-select>

        <div
          class="arkime-input-group arkime-input-group--fluid mb-1"
          :class="{ 'input-disabled': !localFilterEnabled }">
          <span
            id="widgetExpression"
            class="arkime-input-label cursor-help">
            {{ $t('sessions.summary.widget.localFilter') }}
            <v-tooltip activator="#widgetExpression">
              {{ $t('sessions.summary.widget.localFilterTip') }}
            </v-tooltip>
          </span>
          <input
            v-model="form.expression"
            type="text"
            class="arkime-input-control"
            spellcheck="false"
            :disabled="!localFilterEnabled"
            placeholder="protocols == tls">
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
            @click="save">
            <v-icon
              icon="mdi-check"
              class="me-1" />
            {{ $t('common.apply') }}
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
import ArkimeFieldTypeahead from '../utils/FieldTypeahead.vue';
import FieldService from '../search/FieldService';
import { hasMetric, hasAgg, isFieldMode, isGeoFieldMode, allowsMultiField, allowsMultiMetric, hasLocalFilter } from './widgets/viewModes';

const store = useStore();
const { t } = useI18n();

const props = defineProps({
  show: {
    type: Boolean,
    default: false
  },
  widget: {
    type: Object,
    default: () => null
  }
});

const emit = defineEmits(['close', 'save']);

const error = ref('');

const blankForm = () => ({
  id: undefined,
  field: '',
  fields: [],
  title: '',
  viewMode: 'bar',
  metricType: 'sessions',
  metrics: [],
  sortMetric: 'sessions',
  length: 20,
  order: 'desc',
  width: 2,
  height: 3,
  expression: '',
  view: ''
});

const form = ref(blankForm());

// Field list (includes the special summary fields like All IP / Dst IP:Dst Port)
const fields = computed(() => FieldService.addSummarySpecialFields(store.state.fieldsArr || []));

const fieldFriendlyName = computed(() => {
  if (!form.value.field) { return ''; }
  return FieldService.getField(form.value.field, true)?.friendlyName || form.value.field;
});

// pie/treemap/table/intersection accept up to 3 fields (chips multi-select)
const multiField = computed(() => allowsMultiField(form.value.viewMode));
const fieldItems = computed(() => fields.value.map(f => ({ title: f.friendlyName || f.exp, value: f.exp })));

// Capability flags drive which inputs are enabled for the chosen visualization type
const fieldMode = computed(() => isFieldMode(form.value.viewMode));
// the map takes a single geo field (country/*.geo) rather than a general field
const geoMode = computed(() => isGeoFieldMode(form.value.viewMode));
// a field selection is required by both general field-bound and geo (map) types
const needsField = computed(() => fieldMode.value || geoMode.value);
const metricEnabled = computed(() => hasMetric(form.value.viewMode));
// tables take multiple metric columns + a sort-by; charts take a single metric
const multiMetric = computed(() => allowsMultiMetric(form.value.viewMode));
const aggEnabled = computed(() => hasAgg(form.value.viewMode));
// field-bound, geo (map) and timeline widgets all support a local filter; only
// the global capture-stats widgets (stats/time) don't
const localFilterEnabled = computed(() => hasLocalFilter(form.value.viewMode));

// geo fields (country codes) offered for the map: exp `country.*` or a *GEO dbField
const geoFields = computed(() => fields.value.filter(f =>
  f.exp?.startsWith('country.') || /GEO$/.test(f.dbField || '') || /GEO$/.test(f.dbField2 || '')));

const titlePlaceholder = computed(() => fieldFriendlyName.value || viewModeLabel(form.value.viewMode));

function viewModeLabel (vm) {
  return viewModeItems.value.find(i => i.value === vm)?.title || vm;
}

const viewModeItems = computed(() => [
  { title: t('sessions.summary.barChart'), value: 'bar' },
  { title: t('sessions.summary.pieChart'), value: 'pie' },
  { title: t('sessions.summary.tableView'), value: 'table' },
  { title: t('sessions.summary.intersectionView'), value: 'intersection' },
  { title: t('sessions.summary.heatmapView'), value: 'heatmap' },
  { title: t('sessions.summary.treemapView'), value: 'treemap' },
  { title: t('sessions.summary.timelineView'), value: 'timeline' },
  { title: t('sessions.summary.mapView'), value: 'map' },
  { title: t('sessions.summary.statsView'), value: 'stats' },
  { title: t('sessions.summary.timeView'), value: 'time' }
]);

// Metric basis: session count plus every numeric (integer) field, summed per
// value. Mirrors the timeline graph's metric set — Settings.vue uses the same
// `type === 'integer'` criterion for its timeline data filters.
const numericFields = computed(() => (store.state.fieldsArr || [])
  .filter(f => f.type === 'integer')
  .map(f => ({ title: f.friendlyName || f.exp, value: f.exp }))
  .sort((a, b) => a.title.localeCompare(b.title)));

const metricItems = computed(() => [
  { title: t('sessions.summary.sessions'), value: 'sessions' },
  ...numericFields.value
]);

// Sort-by options for the table: Sessions plus the chosen metric columns
const sortByItems = computed(() => {
  const opts = [];
  const seen = new Set();
  for (const m of ['sessions', ...(form.value.metrics || [])]) {
    if (seen.has(m)) { continue; }
    seen.add(m);
    opts.push({ title: m === 'sessions' ? t('sessions.summary.sessions') : (FieldService.getField(m, true)?.friendlyName || m), value: m });
  }
  return opts;
});

// Standard limits, plus the widget's current value if it's a legacy/imported
// number outside the set (so the select doesn't render blank)
const lengthItems = computed(() => {
  const opts = [10, 20, 50, 100];
  if (form.value.length && !opts.includes(form.value.length)) {
    opts.push(form.value.length);
    opts.sort((a, b) => a - b);
  }
  return opts.map(n => ({ title: String(n), value: n }));
});

const orderItems = computed(() => [
  { title: t('sessions.summary.top'), value: 'desc' },
  { title: t('sessions.summary.bottom'), value: 'asc' }
]);

// widgets span 1-4 columns (grid is 4 wide) and 1-8 rows (160px row unit)
const widthItems = [1, 2, 3, 4].map(n => ({ title: String(n), value: n }));
const heightItems = [1, 2, 3, 4, 5, 6, 7, 8].map(n => ({ title: String(n), value: n }));

// views are supported unless the user backend is redis/lmdb (then state.views
// is undefined); show the picker whenever supported, even with no views yet
const viewsSupported = computed(() => Array.isArray(store.state.views));
const hasViews = computed(() => (store.state.views?.length || 0) > 0);

// saved Views the user can use as a per-widget filter
const viewItems = computed(() => [
  { title: t('sessions.summary.widget.noView'), value: '' },
  ...(store.state.views || []).map(v => ({ title: v.name, value: v.id }))
]);

watch(() => props.show, (isOpen) => {
  if (isOpen && props.widget) {
    error.value = '';
    form.value = {
      ...blankForm(),
      ...props.widget,
      fields: Array.isArray(props.widget.fields) && props.widget.fields.length
        ? [...props.widget.fields]
        : (props.widget.field ? [props.widget.field] : []),
      // migrate legacy single metricType into the metrics[] list
      metrics: Array.isArray(props.widget.metrics) && props.widget.metrics.length
        ? [...props.widget.metrics]
        : (props.widget.metricType && props.widget.metricType !== 'sessions' ? [props.widget.metricType] : []),
      sortMetric: props.widget.sortMetric || props.widget.metricType || 'sessions',
      expression: props.widget.expression || '',
      view: props.widget.view || '',
      title: props.widget.title || ''
    };
  }
});

// cap multi-field selection at 3, and mirror the first field into the single-field
// model so switching between single/multi view types never persists a stale field
watch(() => form.value.fields, (v) => {
  if (!Array.isArray(v)) { return; }
  if (v.length > 3) { form.value.fields = v.slice(0, 3); return; }
  form.value.field = v.length ? v[0] : '';
});

// cap metric columns at 4 and keep the sort-by pointed at a chosen metric (or Sessions)
watch(() => form.value.metrics, (v) => {
  if (!Array.isArray(v)) { return; }
  if (v.length > 4) { form.value.metrics = v.slice(0, 4); return; }
  if (form.value.sortMetric !== 'sessions' && !v.includes(form.value.sortMetric)) {
    form.value.sortMetric = v[0] || 'sessions';
  }
});

const onFieldSelected = (field) => {
  const exp = field?.exp || '';
  form.value.field = exp;
  // keep the multi-field model in sync so a later switch to a multi-field view
  // carries the just-picked field instead of the original
  form.value.fields = exp ? [exp] : [];
};

const save = () => {
  // resolve the field list for the chosen type
  let fieldList;
  if (multiField.value) {
    fieldList = (form.value.fields || []).slice(0, 3);
  } else if (needsField.value) { // single field-bound types + the map's geo field
    fieldList = form.value.field ? [form.value.field] : [];
  } else {
    fieldList = []; // session-wide widgets have no field
  }
  // field-bound types (incl. the map's geo field) require at least one field
  if (needsField.value && fieldList.length === 0) {
    error.value = t('sessions.summary.widget.fieldRequired');
    return;
  }
  // resolve the metric(s): tables carry a metrics[] list + a sort-by; charts one.
  // metricType stays the primary (sort) metric for charts + back-compat.
  const metrics = multiMetric.value ? (form.value.metrics || []).slice(0, 4) : [];
  let sortMetric = form.value.sortMetric || 'sessions';
  if (multiMetric.value && sortMetric !== 'sessions' && !metrics.includes(sortMetric)) {
    sortMetric = metrics[0] || 'sessions';
  }
  const metricType = multiMetric.value ? sortMetric : form.value.metricType;
  emit('save', {
    ...props.widget,
    field: fieldList[0] || '',
    fields: fieldList,
    title: form.value.title?.trim() || '',
    viewMode: form.value.viewMode,
    metricType,
    metrics,
    sortMetric: multiMetric.value ? sortMetric : metricType,
    length: form.value.length,
    order: form.value.order,
    width: form.value.width,
    height: form.value.height,
    // local filter only applies to field-bound widgets
    expression: localFilterEnabled.value ? (form.value.expression?.trim() || '') : '',
    view: localFilterEnabled.value ? (form.value.view || '') : ''
  });
};
</script>

<style scoped>
/* grayed, non-interactive state for inputs that don't apply to the chosen type */
.input-disabled {
  opacity: 0.5;
  pointer-events: none;
}

/* the plain inputs are flat and vanish against the card (background === surface
   in light themes); give them the themed input bg + border so they read as fields */
.arkime-input-group {
  background-color: rgb(var(--v-theme-input-bg));
  border: 1px solid rgb(var(--v-theme-input-border));
}
</style>
