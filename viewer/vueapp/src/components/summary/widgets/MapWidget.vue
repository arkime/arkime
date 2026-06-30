<!--
Copyright Yahoo Inc.
SPDX-License-Identifier: Apache-2.0

Session-wide widget: the world map of session geo data, fed by the host's global
stats chunk. Src/Dst/XFF layer toggles reuse the shared store state; clicking a
country adds a `country == <code>` term to the search (same as the pinned map).
-->
<template>
  <WidgetCard
    :title="displayTitle"
    :has-data="hasData"
    :info-items="infoItems"
    @edit="$emit('edit')"
    @remove="$emit('remove')">
    <div class="map-widget">
      <div class="map-widget__toggles">
        <v-btn
          :variant="src ? 'flat' : 'outlined'"
          :color="src ? 'primary' : undefined"
          size="x-small"
          @click="src = !src">
          <strong>S</strong>
          <v-tooltip activator="parent">
            {{ $t('vis.toggleSrcCountry') }}
          </v-tooltip>
        </v-btn>
        <v-btn
          :variant="dst ? 'flat' : 'outlined'"
          :color="dst ? 'primary' : undefined"
          size="x-small"
          @click="dst = !dst">
          <strong>D</strong>
          <v-tooltip activator="parent">
            {{ $t('vis.toggleDstCountry') }}
          </v-tooltip>
        </v-btn>
        <v-btn
          :variant="xffGeo ? 'flat' : 'outlined'"
          :color="xffGeo ? 'primary' : undefined"
          size="x-small"
          @click="xffGeo = !xffGeo">
          <small>XFF</small>
        </v-btn>
      </div>
      <div class="map-widget__map">
        <WorldMap
          :map-data="mapData"
          :src="src"
          :dst="dst"
          :xff-geo="xffGeo"
          @region-click="onRegionClick" />
      </div>
    </div>
  </WidgetCard>
</template>

<script setup>
import { computed } from 'vue';
import { useStore } from 'vuex';
import { useI18n } from 'vue-i18n';
import WidgetCard from './WidgetCard.vue';
import WorldMap from '../../visualizations/WorldMap.vue';

const { t } = useI18n();
const store = useStore();

const props = defineProps({
  mapData: { type: Object, default: () => null },
  title: { type: String, default: '' },
  infoItems: { type: Array, default: () => [] }
});

defineEmits(['edit', 'remove']);

const hasData = computed(() => !!props.mapData && Object.keys(props.mapData).length > 0);
const displayTitle = computed(() => props.title || t('sessions.summary.mapView'));

// layer toggles share the global map state (same as the pinned map)
const src = computed({ get: () => store.state.mapSrc, set: (v) => store.commit('toggleMapSrc', v) });
const dst = computed({ get: () => store.state.mapDst, set: (v) => store.commit('toggleMapDst', v) });
const xffGeo = computed({ get: () => store.state.xffGeo, set: (v) => store.commit('toggleMapXffGeo', v) });

const onRegionClick = (code) => {
  store.commit('addToExpression', { expression: `country == ${code}` });
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
.map-widget__toggles {
  display: flex;
  justify-content: flex-end;
  gap: 4px;
  margin-bottom: 4px;
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
