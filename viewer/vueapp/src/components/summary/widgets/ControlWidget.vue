<!--
Copyright Yahoo Inc.
SPDX-License-Identifier: Apache-2.0

Filter control: picks of one field, as some of its top values (each with
its session count, searched without the control's own pick), a low to high
range of a numeric field (a slider between the field's smallest and largest
value), or typed text (* wildcards allowed). The picks go in the url as the
control's variable (?var.<name>=...), which filters every widget (when the
control applies to all) and fills ${name} in the other widgets' titles and
filters. Picks made while the list is open apply when it closes, a range when
the slider is let go and text on Enter, so each reloads the dashboard once.
-->
<template>
  <WidgetCard
    :title="title"
    :loading="loading && !hasBounds && !error && !taken"
    :error="taken || error"
    :has-data="kind !== 'range' || hasBounds"
    :info-items="infoItems"
    @edit="$emit('edit')"
    @remove="$emit('remove')"
    @retry="fetchData">
    <div class="control-body">
      <v-autocomplete
        v-if="kind === 'list'"
        v-model="picked"
        v-model:menu="menuOpen"
        :items="items"
        :loading="loading"
        :placeholder="$t('sessions.summary.controlAll')"
        :aria-label="title"
        multiple
        chips
        closable-chips
        clearable
        density="compact"
        variant="outlined"
        hide-details
        class="control-picker">
        <template #item="{ props: itemProps, item }">
          <v-list-item
            v-bind="itemProps"
            density="compact">
            <template #prepend="{ isSelected }">
              <v-checkbox-btn
                :model-value="isSelected"
                density="compact" />
            </template>
            <template #append>
              <span class="control-count text-medium-emphasis">{{ commaString(item.raw.sessions) }}</span>
            </template>
          </v-list-item>
        </template>
      </v-autocomplete>
      <!-- range: low to high between the field's smallest and largest value -->
      <div
        v-else-if="kind === 'range'"
        class="control-range">
        <v-range-slider
          v-if="bounds.max > bounds.min"
          v-model="rangePick"
          :min="bounds.min"
          :max="bounds.max"
          :step="1"
          :aria-label="title"
          strict
          density="compact"
          hide-details
          color="primary"
          @end="applyRange" />
        <div class="control-range__text">
          <span>{{ applied.length ? `${commaString(Number(applied[0]))} – ${commaString(Number(applied[1]))}` : $t('sessions.summary.controlAll') }}</span>
          <v-btn
            v-if="applied.length"
            size="x-small"
            variant="text"
            icon
            :title="$t('common.clear')"
            @click="setPick([])">
            <v-icon icon="mdi-close" />
          </v-btn>
        </div>
      </div>
      <!-- text: matched as typed, * wildcards allowed -->
      <v-text-field
        v-else
        v-model="typed"
        :placeholder="$t('sessions.summary.controlTextHint')"
        :aria-label="title"
        clearable
        density="compact"
        variant="outlined"
        hide-details
        @keydown.enter="applyText"
        @blur="applyText"
        @click:clear="setPick([])" />
      <div class="control-meta text-medium-emphasis">
        <code>${{ '{' + variable + '}' }}</code>
        <span v-if="widget.applyAll !== false">{{ $t('sessions.summary.controlFiltersAll') }}</span>
      </div>
    </div>
  </WidgetCard>
</template>

<script setup>
import { ref, computed, watch } from 'vue';
import { useRoute, useRouter } from 'vue-router';
import { useStore } from 'vuex';
import { useI18n } from 'vue-i18n';
import WidgetCard from './WidgetCard.vue';
import FieldService from '../../search/FieldService';
import { commaString } from '@common/vueFilters.js';
import { useSpigraphWidget } from './useSpigraphWidget';
import { fetchControlValues, fetchControlRange } from './widgetData';
import { controlVariable, controlType, urlVariableValues, VARIABLE_PARAM_PREFIX, MAX_VARIABLE_VALUES } from './dashboardVariables';

const props = defineProps({
  widget: { type: Object, required: true },
  reloadNonce: { type: Number, default: 0 },
  infoItems: { type: Array, default: () => [] }
});

defineEmits(['edit', 'remove']);

const route = useRoute();
const router = useRouter();
const store = useStore();
const { t } = useI18n();

const kind = computed(() => controlType(props.widget));
const rows = ref([]); // a list's top values
const bounds = ref({}); // a range's { min, max }
const { loading, error, fetchData } = useSpigraphWidget(
  () => props.widget,
  () => props.reloadNonce,
  (res) => {
    if (kind.value === 'range') { bounds.value = res || {}; resetRange(); } else { rows.value = res || []; }
  },
  // a text box has nothing to fetch
  (r, s, w, opts) => {
    const k = controlType(w);
    if (k === 'range') { return fetchControlRange(r, s, w, controlVariable(w), opts); }
    return k === 'list' ? fetchControlValues(r, s, w, controlVariable(w), opts) : Promise.resolve([]);
  }
);
// a range with no bounds (no session has the field) is the card's empty state
const hasBounds = computed(() => (kind.value === 'range' ? bounds.value.max !== undefined : kind.value === 'text' || rows.value.length > 0));

const variable = computed(() => controlVariable(props.widget));
const fieldObj = computed(() => FieldService.getField(props.widget.field, true));
// another control owns this variable (an imported dashboard can carry two):
// say so rather than pick this field's values into its filter
const taken = computed(() => {
  const owner = store.state.dashboardVariables?.[variable.value]?.owner;
  return owner && owner !== props.widget.id ? t('sessions.summary.widget.variableTaken', { ref: `\${${variable.value}}` }) : '';
});
const title = computed(() => props.widget.title || fieldObj.value?.friendlyName || props.widget.field);

// what the url picks (as the dashboard reads it, so a bad range is no pick);
// the picker edits a copy until its list closes
const applied = computed(() => store.state.dashboardVariables?.[variable.value]?.owner === props.widget.id
  ? store.state.dashboardVariables[variable.value].values
  : urlVariableValues(route.query, variable.value));
const picked = ref([...applied.value]);
const menuOpen = ref(false);
// the variables are rebuilt (new arrays) on every url change, so only a
// change of the pick itself resets the picker, not another control's or the
// time range's (which would discard text typed but not yet applied)
watch(() => applied.value.join('\u0000'), () => {
  const v = applied.value;
  if (!menuOpen.value) { picked.value = [...v]; }
  resetRange();
  typed.value = kind.value === 'text' ? (v[0] || '') : '';
});

const setPick = (values) => {
  router.replace({ query: { ...route.query, [`${VARIABLE_PARAM_PREFIX}${variable.value}`]: values.length ? values : undefined } });
};

// range: the slider shows the pick, or the whole span when there is none; a
// pick of the whole span is no pick
const rangePick = ref([0, 0]);
const resetRange = () => {
  if (kind.value !== 'range') { return; }
  const { min, max } = bounds.value;
  rangePick.value = applied.value.length ? applied.value.map(Number) : [min ?? 0, max ?? 0];
};
const applyRange = () => {
  const [lo, hi] = rangePick.value.map(Math.round);
  if (lo <= bounds.value.min && hi >= bounds.value.max) { setPick([]); return; }
  setPick([String(lo), String(hi)]);
};

// text: applied on Enter or leaving the box
const typed = ref(kind.value === 'text' ? (applied.value[0] || '') : '');
const applyText = () => {
  const text = (typed.value || '').trim();
  if (text === (applied.value[0] || '')) { return; }
  setPick(text ? [text] : []);
};

// the top values, plus any picked value that isn't among them (an old link)
const items = computed(() => {
  const list = rows.value.map(r => ({ title: String(r.item), value: String(r.item), sessions: r.sessions }));
  for (const v of picked.value) {
    if (!list.some(i => i.value === v)) { list.push({ title: v, value: v, sessions: 0 }); }
  }
  return list;
});

const apply = () => {
  const values = picked.value.slice(0, MAX_VARIABLE_VALUES);
  if (values.join('\u0000') === applied.value.join('\u0000')) { return; }
  setPick(values);
};
watch(menuOpen, (isOpen) => { if (!isOpen) { apply(); } });
// a chip closed or the picker cleared with the list shut applies right away
watch(picked, () => { if (!menuOpen.value) { apply(); } });
</script>

<style scoped>
.control-body {
  display: flex;
  flex-direction: column;
  gap: 4px;
  padding: 2px 4px;
}
.control-picker :deep(.v-field__input) {
  max-height: 72px;
  overflow-y: auto;
}
.control-range {
  padding: 0 8px;
}
.control-range__text {
  display: flex;
  align-items: center;
  gap: 4px;
  font-size: 0.85rem;
}
.control-count {
  font-size: 0.75rem;
  margin-left: 12px;
}
.control-meta {
  display: flex;
  gap: 8px;
  font-size: 0.75rem;
}
</style>
