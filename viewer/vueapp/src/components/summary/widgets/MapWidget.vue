<!--
Copyright Yahoo Inc.
SPDX-License-Identifier: Apache-2.0

Field widget: a world-map choropleth of a chosen geo field (country codes).
Self-fetches /api/spigraph for the field (global search AND the widget's local
filter) and colors each country by its session count. Clicking a country adds a
`<field> == <code>` term to the search.
-->
<template>
  <WidgetCard
    :title="title"
    :loading="loading"
    :error="error"
    :has-data="hasData"
    :info-items="infoItems"
    @edit="$emit('edit')"
    @remove="$emit('remove')"
    @retry="fetchData">
    <div class="map-widget">
      <div class="map-widget__map">
        <WorldMap
          :layer-values="values"
          @region-click="onRegionClick" />
      </div>
    </div>
  </WidgetCard>
</template>

<script setup>
import { ref, computed } from 'vue';
import { useStore } from 'vuex';
import WidgetCard from './WidgetCard.vue';
import WorldMap from '../../visualizations/WorldMap.vue';
import FieldService from '../../search/FieldService';
import { useSpigraphWidget } from './useSpigraphWidget';

// countries are ~250; ask for enough spigraph values to cover them all
const MAP_TOP = 1000;

const store = useStore();

const props = defineProps({
  widget: { type: Object, required: true },
  reloadNonce: { type: Number, default: 0 },
  infoItems: { type: Array, default: () => [] }
});

defineEmits(['edit', 'remove']);

const values = ref({});

// self-fetch spigraph for the geo field; request all countries (ignore the
// widget's top-N, which the map doesn't expose), then map value -> count
const { loading, error, fetchData } = useSpigraphWidget(
  () => ({ ...props.widget, length: MAP_TOP, metricType: 'sessions' }),
  () => props.reloadNonce,
  (res) => {
    const v = {};
    for (const it of res.items || []) {
      if (it.count > 0) { v[it.name] = it.count; }
    }
    values.value = v;
  }
);

const fieldObj = computed(() => FieldService.getField(props.widget.field, true));
const title = computed(() => props.widget.title || fieldObj.value?.friendlyName || props.widget.field);
const hasData = computed(() => Object.keys(values.value).length > 0);

const onRegionClick = (code) => {
  if (!props.widget.field) { return; }
  store.commit('addToExpression', { expression: `${props.widget.field} == ${code}` });
};
</script>

<style scoped>
.map-widget {
  position: relative;
  display: flex;
  flex-direction: column;
  flex: 1;
  min-height: 0;
}
.map-widget__map {
  flex: 1;
  min-height: 220px;
}
.map-widget__map :deep(.map) {
  height: 100%;
  width: 100%;
}
</style>
