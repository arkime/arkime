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

        <!-- text widget: just a title and the text -->
        <template v-if="staticMode">
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
          <v-textarea
            v-model="form.text"
            :label="$t('sessions.summary.widget.text')"
            :placeholder="$t('sessions.summary.widget.textHint')"
            rows="6"
            auto-grow
            density="compact"
            variant="outlined"
            hide-details
            class="mb-3" />
        </template>

        <!-- connections is an ordered src -> dst pair, so each end gets its own
             picker over its own list (ip.dst:port only resolves as a destination) -->
        <template v-else-if="fieldPair">
          <div class="arkime-input-group arkime-input-group--fluid mb-3">
            <span
              id="widgetSrcField"
              class="arkime-input-label cursor-help">
              {{ $t('common.source') }}<sup>*</sup>
              <v-tooltip activator="#widgetSrcField">
                {{ $t('connections.sourceFieldTip') }}
              </v-tooltip>
            </span>
            <ArkimeFieldTypeahead
              :fields="srcFields"
              :initial-value="pairFriendlyName(0)"
              @field-selected="(f) => onPairFieldSelected(0, f)" />
          </div>
          <div class="arkime-input-group arkime-input-group--fluid mb-3">
            <span
              id="widgetDstField"
              class="arkime-input-label cursor-help">
              {{ $t('common.destination') }}<sup>*</sup>
              <v-tooltip activator="#widgetDstField">
                {{ $t('connections.dstFieldTip') }}
              </v-tooltip>
            </span>
            <ArkimeFieldTypeahead
              :fields="dstFields"
              :initial-value="pairFriendlyName(1)"
              @field-selected="(f) => onPairFieldSelected(1, f)" />
          </div>
        </template>

        <!-- Field selector: a chips multi-select (up to 3) for pie/treemap/table/
             intersection, otherwise a single typeahead. Grayed for session types. -->
        <div
          v-else
          class="arkime-input-group arkime-input-group--fluid mb-3"
          :class="{ 'input-disabled': !needsField && !optionalField }">
          <span
            id="widgetField"
            class="arkime-input-label"
            :class="{ 'cursor-help': optionalField }">
            {{ multiField ? $t('sessions.summary.widget.fields') : $t(optionalField ? 'sessions.summary.widget.fieldOptional' : 'sessions.summary.widget.field') }}<sup v-if="needsField">*</sup>
            <v-tooltip
              v-if="optionalField"
              activator="#widgetField">
              {{ $t('sessions.summary.widget.fieldOptionalHint') }}
            </v-tooltip>
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
            :fields="usableFields"
            :initial-value="fieldFriendlyName"
            @field-selected="onFieldSelected" />
          <v-btn
            v-if="optionalField && form.field"
            variant="text"
            size="small"
            icon
            :title="$t('common.clear')"
            @click="onFieldSelected(null)">
            <v-icon icon="mdi-close" />
          </v-btn>
        </div>

        <!-- Title override -->
        <div
          v-if="!staticMode"
          class="arkime-input-group arkime-input-group--fluid mb-3">
          <span class="arkime-input-label">
            {{ $t('sessions.summary.widget.title') }}
          </span>
          <input
            v-model="form.title"
            type="text"
            class="arkime-input-control"
            :placeholder="titlePlaceholder">
        </div>

        <div
          v-if="!staticMode"
          class="d-flex flex-wrap gap-3 mb-3">
          <!-- Metric basis: Sessions count, or numeric field(s) aggregated per
               value. Tables take several metric columns + a sort-by; charts one. -->
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
          <!-- How the metric field(s) combine per value; only the summary-endpoint
               modes take one, and it means nothing for the session count -->
          <span
            v-if="aggEnabled"
            id="widgetMetricAgg"
            style="min-width: 140px">
            <v-select
              v-model="form.metricAgg"
              :items="aggItems"
              :label="$t('sessions.summary.widget.metricAgg')"
              :disabled="!aggApplies"
              density="compact"
              variant="outlined"
              hide-details />
            <v-tooltip activator="#widgetMetricAgg">
              {{ $t('sessions.summary.widget.metricAggTip') }}
            </v-tooltip>
          </span>
        </div>

        <!-- bar: split each value by a second field (stacked or grouped segments) -->
        <div
          v-if="splitEnabled"
          class="d-flex flex-wrap gap-3 mb-3 align-center">
          <div
            class="arkime-input-group arkime-input-group--fluid flex-grow-1"
            style="min-width: 220px">
            <span
              id="widgetSplitField"
              class="arkime-input-label cursor-help">
              {{ $t('sessions.summary.widget.splitField') }}
              <v-tooltip activator="#widgetSplitField">
                {{ $t('sessions.summary.widget.splitFieldTip') }}
              </v-tooltip>
            </span>
            <ArkimeFieldTypeahead
              :fields="splitFields"
              :initial-value="splitFieldFriendlyName"
              @field-selected="onSplitFieldSelected" />
            <v-btn
              v-if="form.splitField"
              variant="text"
              size="small"
              icon
              :title="$t('common.clear')"
              @click="form.splitField = ''">
              <v-icon icon="mdi-close" />
            </v-btn>
          </div>
          <v-select
            v-model="form.splitLength"
            :items="splitLengthItems"
            :label="$t('sessions.summary.widget.splitLength')"
            :disabled="!form.splitField"
            density="compact"
            variant="outlined"
            hide-details
            style="min-width: 120px" />
          <v-select
            v-model="form.barMode"
            :items="barModeItems"
            :label="$t('sessions.summary.widget.barMode')"
            :disabled="!form.splitField || !additiveMetric"
            density="compact"
            variant="outlined"
            hide-details
            style="min-width: 120px" />
        </div>

        <div
          v-if="!staticMode"
          class="d-flex flex-wrap gap-3 mb-3">
          <!-- Data limit (Top/Bottom N, or a session sample for connections) -->
          <v-select
            v-model="form.length"
            :items="lengthItems"
            :label="$t(sampleSize ? 'sessions.summary.widget.sampleSize' : 'sessions.summary.widget.limit')"
            :disabled="!lengthEnabled"
            density="compact"
            variant="outlined"
            hide-details
            style="min-width: 140px" />

          <!-- Order (direction) — only for the modes whose fetch passes one -->
          <v-select
            v-if="orderEnabled"
            v-model="form.order"
            :items="orderItems"
            :label="$t('sessions.summary.widget.order')"
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
          v-if="viewsSupported && !staticMode"
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
          v-if="!staticMode"
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
import { metricIsAdditive, fieldIsAggregatable } from './widgets/widgetData';
import { hasMetric, hasMetricAgg, hasLength, hasOrder, isSampleSize, lengthOptions, defaultLength, fieldCountLimits, fieldUsableBy, isFieldPair, isFieldMode, isGeoFieldMode, isOptionalFieldMode, isStaticMode, allowsMultiField, allowsMultiMetric, allowsSplit, hasLocalFilter, METRIC_AGGS, SPLIT_LENGTHS, BAR_MODES } from './widgets/viewModes';

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
  metricAgg: 'sum',
  sortMetric: 'sessions',
  splitField: '',
  splitLength: 5,
  barMode: 'stacked',
  text: '',
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

// how many fields the chosen type takes: [1,3] for the nested/side-by-side
// types, [2,2] for the connections source + destination pair
const multiField = computed(() => allowsMultiField(form.value.viewMode));
const minFields = computed(() => fieldCountLimits(form.value.viewMode)[0]);
const maxFields = computed(() => fieldCountLimits(form.value.viewMode)[1]);
// only offer fields the chosen view mode's endpoint can actually resolve, so a
// pick can't fail later inside Elasticsearch (see fieldUsableBy)
const usableFields = computed(() => fields.value.filter(f => fieldUsableBy(form.value.viewMode, f)));
// connections: an ordered pair, so each end filters the list for its own position
const fieldPair = computed(() => isFieldPair(form.value.viewMode));
const srcFields = computed(() => fields.value.filter(f => fieldUsableBy(form.value.viewMode, f, 'src')));
const dstFields = computed(() => fields.value.filter(f => fieldUsableBy(form.value.viewMode, f, 'dst')));
const pairFriendlyName = (i) => {
  const exp = form.value.fields?.[i];
  return exp ? (FieldService.getField(exp, true)?.friendlyName || exp) : '';
};
// keep both slots present so an unset end stays an empty string rather than a
// hole that would make fields.length look complete
const onPairFieldSelected = (i, field) => {
  const next = [form.value.fields?.[0] || '', form.value.fields?.[1] || ''];
  next[i] = field?.exp || '';
  form.value.fields = next;
};
const fieldItems = computed(() => usableFields.value.map(f => ({ title: f.friendlyName || f.exp, value: f.exp })));

// Capability flags drive which inputs are enabled for the chosen visualization type
const fieldMode = computed(() => isFieldMode(form.value.viewMode));
// the map takes a single geo field (country/*.geo) rather than a general field
const geoMode = computed(() => isGeoFieldMode(form.value.viewMode));
// a field selection is required by both general field-bound and geo (map) types
const needsField = computed(() => fieldMode.value || geoMode.value);
// metric: a field splits the number per top value but isn't required
const optionalField = computed(() => isOptionalFieldMode(form.value.viewMode));
// text: no data inputs at all
const staticMode = computed(() => isStaticMode(form.value.viewMode));
const metricEnabled = computed(() => hasMetric(form.value.viewMode));
// aggregation choice: only the summary-endpoint modes, and only once a metric
// field (not the session count) is chosen
const aggEnabled = computed(() => hasMetricAgg(form.value.viewMode));
const aggApplies = computed(() => multiMetric.value
  ? (form.value.metrics || []).some(m => m !== 'sessions')
  : (form.value.metricType && form.value.metricType !== 'sessions'));
const aggItems = computed(() => METRIC_AGGS.map(a => ({ title: t(`sessions.summary.agg.${a}`), value: a })));
// bar: optional split field
const splitEnabled = computed(() => allowsSplit(form.value.viewMode));
const splitFields = computed(() => fields.value.filter(f => fieldIsAggregatable(f) && f.exp !== 'ip' && f.exp !== 'ip.dst:port' && f.exp !== form.value.field));
// averages, extremes and unique counts can't stack; the chart draws them
// grouped whatever is stored, and the editor says so by pinning the choice
const additiveMetric = computed(() => metricIsAdditive(form.value.metricType, form.value.metricAgg));
watch(additiveMetric, (additive) => { if (!additive) { form.value.barMode = 'grouped'; } });
const splitFieldFriendlyName = computed(() => (form.value.splitField
  ? (FieldService.getField(form.value.splitField, true)?.friendlyName || form.value.splitField)
  : ''));
const onSplitFieldSelected = (field) => { form.value.splitField = field?.exp || ''; };
const splitLengthItems = SPLIT_LENGTHS.map(n => ({ title: String(n), value: n }));
const barModeItems = computed(() => BAR_MODES.map(m => ({ title: t(`sessions.summary.widget.barMode-${m}`), value: m })));
// tables take multiple metric columns + a sort-by; charts take a single metric
const multiMetric = computed(() => allowsMultiMetric(form.value.viewMode));
const lengthEnabled = computed(() => hasLength(form.value.viewMode));
// only the summary-endpoint modes pass an order through; showing the control
// for the others would silently do nothing
const orderEnabled = computed(() => hasOrder(form.value.viewMode));
// connections' limit is a session sample, so it gets its own label + scale
const sampleSize = computed(() => isSampleSize(form.value.viewMode));
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
  { title: t('sessions.summary.sankeyView'), value: 'sankey' },
  { title: t('sessions.summary.connectionsView'), value: 'connections' },
  { title: t('sessions.summary.timelineView'), value: 'timeline' },
  { title: t('sessions.summary.mapView'), value: 'map' },
  { title: t('sessions.summary.statsView'), value: 'stats' },
  { title: t('sessions.summary.timeView'), value: 'time' },
  { title: t('sessions.summary.metricView'), value: 'metric' },
  { title: t('sessions.summary.textView'), value: 'text' }
]);

// Metric basis: session count plus every numeric (integer) field, aggregated per
// value. Mirrors the timeline graph's metric set — Settings.vue uses the same
// `type === 'integer'` criterion for its timeline data filters. A unique count
// (cardinality) works on any field, so that aggregation offers them all. The
// "All X" groups have no real column, so neither list offers them.
const metricFields = computed(() => (store.state.fieldsArr || [])
  .filter(f => f.dbField && fieldIsAggregatable(f) && (f.type === 'integer' || (aggEnabled.value && form.value.metricAgg === 'cardinality')))
  .map(f => ({ title: f.friendlyName || f.exp, value: f.exp }))
  .sort((a, b) => a.title.localeCompare(b.title)));

const metricItems = computed(() => [
  { title: t('sessions.summary.sessions'), value: 'sessions' },
  ...metricFields.value
]);

// leaving the unique-count aggregation drops any non-numeric metric field(s)
watch(() => form.value.metricAgg, (agg) => {
  if (agg === 'cardinality') { return; }
  const numeric = (exp) => exp === 'sessions' || FieldService.getField(exp, true)?.type === 'integer';
  if (!numeric(form.value.metricType)) { form.value.metricType = 'sessions'; }
  if (Array.isArray(form.value.metrics)) { form.value.metrics = form.value.metrics.filter(numeric); }
});

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

// The chosen type's limit scale (top-N counts, or connections' session sample
// sizes), plus the widget's current value if it's a legacy/imported number
// outside the set (so the select doesn't render blank)
const lengthItems = computed(() => {
  const opts = [...lengthOptions(form.value.viewMode)];
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
      metricAgg: METRIC_AGGS.includes(props.widget.metricAgg) ? props.widget.metricAgg : 'sum',
      splitField: props.widget.splitField || '',
      splitLength: SPLIT_LENGTHS.includes(props.widget.splitLength) ? props.widget.splitLength : 5,
      barMode: BAR_MODES.includes(props.widget.barMode) ? props.widget.barMode : 'stacked',
      text: props.widget.text || '',
      expression: props.widget.expression || '',
      view: props.widget.view || '',
      title: props.widget.title || ''
    };
  }
});

// drop fields the chosen view mode can't resolve, cap the selection (3, or 2 for
// the connections pair), and mirror the first field into the single-field model
// so switching between single/multi view types never persists a stale field
watch([() => form.value.fields, maxFields, () => form.value.viewMode], ([v, max, viewMode]) => {
  if (!Array.isArray(v)) { return; }
  // a pair's slots are positional, so blank an end this mode can't use rather
  // than compacting — dropping the source would otherwise slide the destination
  // into it, leaving both pickers looking filled with the wrong pairing
  if (isFieldPair(viewMode)) {
    const pair = [0, 1].map((i) => {
      const exp = v[i] || '';
      return fieldUsableBy(viewMode, FieldService.getField(exp, true), i === 0 ? 'src' : 'dst') ? exp : '';
    });
    if (pair.length !== v.length || pair.some((exp, i) => exp !== v[i])) {
      form.value.fields = pair;
      return;
    }
    form.value.field = v[0] || '';
    return;
  }
  const usable = v.filter(exp => fieldUsableBy(viewMode, FieldService.getField(exp, true)));
  if (usable.length !== v.length) { form.value.fields = usable; return; }
  if (v.length > max) { form.value.fields = v.slice(0, max); return; }
  form.value.field = v.length ? v[0] : '';
});

// the top-N and session-sample scales don't overlap, so switching between them
// snaps the limit to the new scale's default rather than keeping a value that
// means something entirely different (a 20-session connections sample, say)
watch(() => form.value.viewMode, (mode, prev) => {
  if (!prev || mode === prev) { return; }
  if (isSampleSize(mode) !== isSampleSize(prev)) {
    form.value.length = defaultLength(mode);
  }
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
    fieldList = (form.value.fields || []).filter(Boolean).slice(0, maxFields.value);
  } else if (needsField.value || optionalField.value) { // single field-bound types + the map's geo field
    fieldList = form.value.field ? [form.value.field] : [];
  } else {
    fieldList = []; // session-wide widgets have no field
  }
  // field-bound types (incl. the map's geo field) require at least one field
  if (needsField.value && fieldList.length === 0) {
    error.value = t('sessions.summary.widget.fieldRequired');
    return;
  }
  // types wanting an exact count (connections' src + dst) need all of them
  if (needsField.value && fieldList.length < minFields.value) {
    error.value = t('sessions.summary.widget.exactFieldsRequired', { count: minFields.value });
    return;
  }
  // resolve the metric(s): tables carry a metrics[] list + a sort-by; charts one.
  // metricType stays the primary (sort) metric for charts + back-compat.
  const metrics = multiMetric.value ? (form.value.metrics || []).slice(0, 4) : [];
  let sortMetric = form.value.sortMetric || 'sessions';
  if (multiMetric.value && sortMetric !== 'sessions' && !metrics.includes(sortMetric)) {
    sortMetric = metrics[0] || 'sessions';
  }
  let metricType = multiMetric.value ? sortMetric : form.value.metricType;
  // only a unique count (on a mode that offers one) can use a non-numeric
  // metric field; anything else would ask for a sum of text
  const metricAgg = aggEnabled.value ? (form.value.metricAgg || 'sum') : 'sum';
  if (metricAgg !== 'cardinality') {
    const numeric = (exp) => exp === 'sessions' || FieldService.getField(exp, true)?.type === 'integer';
    if (!numeric(metricType)) { metricType = 'sessions'; }
    for (let i = metrics.length - 1; i >= 0; i--) { if (!numeric(metrics[i])) { metrics.splice(i, 1); } }
    if (!numeric(sortMetric)) { sortMetric = metrics[0] || 'sessions'; }
  }
  emit('save', {
    ...props.widget,
    field: fieldList[0] || '',
    fields: fieldList,
    title: form.value.title?.trim() || '',
    viewMode: form.value.viewMode,
    metricType,
    metrics,
    metricAgg,
    sortMetric: multiMetric.value ? sortMetric : metricType,
    splitField: splitEnabled.value ? (form.value.splitField || '') : '',
    splitLength: form.value.splitLength || 5,
    barMode: additiveMetric.value ? (form.value.barMode || 'stacked') : 'grouped',
    text: staticMode.value ? (form.value.text || '') : '',
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
