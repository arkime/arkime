<!--
Copyright Yahoo Inc.
SPDX-License-Identifier: Apache-2.0

Filter control (prototype): a picker of one field's top values, each with
its session count, searched without the control's own pick. The picks go in
the url as the control's variable (?var.<name>=...), which filters every
widget (when the control applies to all) and fills ${name} in the other
widgets' titles and filters. Picks made while the list is open apply when it
closes, so picking several values reloads the dashboard once.
-->
<template>
  <WidgetCard
    :title="title"
    :loading="loading && !rows.length && !error && !taken"
    :error="taken || error"
    :info-items="infoItems"
    @edit="$emit('edit')"
    @remove="$emit('remove')"
    @retry="fetchData">
    <div class="control-body">
      <v-autocomplete
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
import { fetchControlValues } from './widgetData';
import { controlVariable, urlVariableValues, VARIABLE_PARAM_PREFIX, MAX_VARIABLE_VALUES } from './dashboardVariables';

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

const rows = ref([]);
const { loading, error, fetchData } = useSpigraphWidget(
  () => props.widget,
  () => props.reloadNonce,
  (res) => { rows.value = res; },
  (r, s, w, opts) => fetchControlValues(r, s, w, controlVariable(w), opts)
);

const variable = computed(() => controlVariable(props.widget));
const fieldObj = computed(() => FieldService.getField(props.widget.field, true));
// another control owns this variable (an imported dashboard can carry two):
// say so rather than pick this field's values into its filter
const taken = computed(() => {
  const owner = store.state.dashboardVariables?.[variable.value]?.owner;
  return owner && owner !== props.widget.id ? t('sessions.summary.widget.variableTaken', { ref: `\${${variable.value}}` }) : '';
});
const title = computed(() => props.widget.title || fieldObj.value?.friendlyName || props.widget.field);

// what the url picks; the picker edits a copy until its list closes
const applied = computed(() => urlVariableValues(route.query, variable.value));
const picked = ref([...applied.value]);
const menuOpen = ref(false);
watch(applied, (v) => { if (!menuOpen.value) { picked.value = [...v]; } });

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
  router.replace({ query: { ...route.query, [`${VARIABLE_PARAM_PREFIX}${variable.value}`]: values.length ? values : undefined } });
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
