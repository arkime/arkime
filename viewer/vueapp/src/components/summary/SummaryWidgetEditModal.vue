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
          :class="{ 'input-disabled': !fieldMode }">
          <span class="arkime-input-label">
            {{ multiField ? $t('sessions.summary.widget.fields') : $t('sessions.summary.widget.field') }}<sup v-if="fieldMode">*</sup>
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
          <!-- Metric basis: Sessions count, or any numeric field summed per value -->
          <v-autocomplete
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
import { hasMetric, hasAgg, isFieldMode, allowsMultiField } from './widgets/viewModes';

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
const metricEnabled = computed(() => hasMetric(form.value.viewMode));
const aggEnabled = computed(() => hasAgg(form.value.viewMode));
// session-wide widgets (timeline/map/stats/time) describe the whole result set
const localFilterEnabled = computed(() => fieldMode.value);

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
  } else if (fieldMode.value) {
    fieldList = form.value.field ? [form.value.field] : [];
  } else {
    fieldList = []; // session-wide widgets have no field
  }
  // field-bound types require at least one field
  if (fieldMode.value && fieldList.length === 0) {
    error.value = t('sessions.summary.widget.fieldRequired');
    return;
  }
  emit('save', {
    ...props.widget,
    field: fieldList[0] || '',
    fields: fieldList,
    title: form.value.title?.trim() || '',
    viewMode: form.value.viewMode,
    metricType: form.value.metricType,
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
</style>
