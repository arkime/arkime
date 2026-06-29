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
        <!-- Field selector -->
        <div class="arkime-input-group arkime-input-group--fluid mb-3">
          <span class="arkime-input-label">
            {{ $t('sessions.summary.widget.field') }}<sup>*</sup>
          </span>
          <ArkimeFieldTypeahead
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
            :placeholder="fieldFriendlyName">
        </div>

        <div class="d-flex flex-wrap gap-3 mb-3">
          <!-- View mode -->
          <v-select
            v-model="form.viewMode"
            :items="viewModeItems"
            :label="$t('sessions.summary.widget.viewMode')"
            density="compact"
            variant="outlined"
            hide-details
            style="min-width: 150px" />

          <!-- Metric basis -->
          <v-select
            v-model="form.metricType"
            :items="metricItems"
            :label="$t('sessions.summary.widget.metric')"
            :disabled="metricDisabled"
            density="compact"
            variant="outlined"
            hide-details
            style="min-width: 150px" />
        </div>

        <div class="d-flex flex-wrap gap-3 mb-3">
          <!-- Data limit -->
          <v-select
            v-model="form.length"
            :items="lengthItems"
            :label="$t('sessions.summary.widget.limit')"
            density="compact"
            variant="outlined"
            hide-details
            style="min-width: 120px" />

          <!-- Order -->
          <v-select
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
            :items="sizeItems"
            :label="$t('sessions.summary.widget.width')"
            density="compact"
            variant="outlined"
            hide-details
            style="min-width: 150px" />

          <!-- Height -->
          <v-select
            v-model="form.height"
            :items="sizeItems"
            :label="$t('sessions.summary.widget.height')"
            density="compact"
            variant="outlined"
            hide-details
            style="min-width: 150px" />
        </div>

        <!-- Local filter: optional saved View + expression (combined with AND,
             on top of the global search) -->
        <v-select
          v-if="viewsSupported"
          v-model="form.view"
          :items="viewItems"
          :label="$t('sessions.summary.widget.localView')"
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

        <div class="arkime-input-group arkime-input-group--fluid mb-1">
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
import { METRICLESS_VIEW_MODES } from './widgets/viewModes';

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
  title: '',
  viewMode: 'bar',
  metricType: 'sessions',
  length: 20,
  order: 'desc',
  width: 'standard',
  height: 'standard',
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

const viewModeItems = computed(() => [
  { title: t('sessions.summary.barChart'), value: 'bar' },
  { title: t('sessions.summary.pieChart'), value: 'pie' },
  { title: t('sessions.summary.tableView'), value: 'table' },
  { title: t('sessions.summary.heatmapView'), value: 'heatmap' },
  { title: t('sessions.summary.treemapView'), value: 'treemap' }
]);

// metric basis only applies to the count-based charts (not table/heatmap/treemap)
const metricDisabled = computed(() => METRICLESS_VIEW_MODES.includes(form.value.viewMode));

const metricItems = computed(() => [
  { title: t('sessions.summary.sessions'), value: 'sessions' },
  { title: t('sessions.summary.packets'), value: 'packets' },
  { title: t('sessions.summary.bytes'), value: 'bytes' }
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

const sizeItems = computed(() => [
  { title: t('sessions.summary.widget.standard'), value: 'standard' },
  { title: t('sessions.summary.widget.double'), value: 'double' }
]);

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
      expression: props.widget.expression || '',
      view: props.widget.view || '',
      title: props.widget.title || ''
    };
  }
});

const onFieldSelected = (field) => {
  form.value.field = field.exp;
};

const save = () => {
  if (!form.value.field) {
    error.value = t('sessions.summary.widget.fieldRequired');
    return;
  }
  emit('save', {
    ...props.widget,
    field: form.value.field,
    title: form.value.title?.trim() || '',
    viewMode: form.value.viewMode,
    metricType: form.value.metricType,
    length: form.value.length,
    order: form.value.order,
    width: form.value.width,
    height: form.value.height,
    expression: form.value.expression?.trim() || '',
    view: form.value.view || ''
  });
};
</script>
