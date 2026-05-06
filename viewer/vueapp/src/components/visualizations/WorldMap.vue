<!--
Copyright Yahoo Inc.
SPDX-License-Identifier: Apache-2.0

Production world map renderer for the visualizations panel. Replaces
jvectormap (jQuery plugin, last released 2014) with d3-geo + world-atlas
topojson. The component owns SVG rendering only; the parent
Visualizations.vue still owns map state (open/closed, expanded, src/dst/
xffGeo toggles, top-10 legend), drives view changes via the public
methods exposed on this ref, and dispatches the country-click filter.
-->
<template>
  <div
    ref="host"
    class="arkime-world-map">
    <svg
      v-if="ready"
      ref="svgEl"
      :viewBox="`0 0 ${width} ${height}`"
      preserveAspectRatio="xMidYMid meet"
      :style="{backgroundColor: waterColor}">
      <g
        class="world-map-zoom"
        :style="zoomTransform">
        <path
          v-if="sphere"
          :d="pathFor(sphere)"
          class="world-map-sphere"
          :style="{fill: waterColor}" />
        <path
          v-for="feat in features"
          :key="feat.id"
          :d="pathFor(feat)"
          :fill="fillFor(feat)"
          class="world-map-country"
          @mouseenter="(e) => onHover(e, feat)"
          @mouseleave="onLeave"
          @click="onClick(feat)" />
      </g>
    </svg>
    <div
      v-if="hover"
      class="world-map-tooltip"
      :style="{left: hover.x + 'px', top: hover.y + 'px'}">
      <strong>{{ hover.name }}</strong>{{ hover.code ? ` (${hover.code})` : '' }} —
      {{ commaString(hover.value || 0) }}
    </div>
    <div class="world-map-zoom-buttons">
      <button
        type="button"
        class="btn btn-xs btn-default"
        aria-label="Zoom in"
        @click="zoomIn">
        <span class="fa fa-plus" />
      </button>
      <button
        type="button"
        class="btn btn-xs btn-default"
        aria-label="Zoom out"
        @click="zoomOut">
        <span class="fa fa-minus" />
      </button>
    </div>
  </div>
</template>

<script>
import { commaString } from '@common/vueFilters.js';
import { ALPHA2_TO_NUMERIC } from '@common/alpha2ToNumeric.js';

const ZOOM_STEP = 1.4;
const ZOOM_MIN = 1;
const ZOOM_MAX = 8;

export default {
  name: 'ArkimeWorldMap',
  props: {
    mapData: { type: Object, default: () => ({}) },
    src: { type: Boolean, default: true },
    dst: { type: Boolean, default: true },
    xffGeo: { type: Boolean, default: false }
  },
  emits: ['regionClick', 'updateLegend'],
  data () {
    return {
      ready: false,
      width: 800,
      height: 360,
      features: [],
      sphere: null,
      d3: null,
      pathGen: null,
      colorScale: null,
      values: {},
      hover: null,
      zoomLevel: 1,
      // theme colors — read from CSS custom props on mount
      waterColor: '#162a3d',
      landColorLight: '#2c3e50',
      landColorDark: '#65a3ff'
    };
  },
  computed: {
    zoomTransform () {
      return { transform: `scale(${this.zoomLevel})`, transformOrigin: 'center' };
    },
    // Reverse alpha-2 → numeric lookup, materialized once for hover labels.
    numericToAlpha2 () {
      const out = {};
      for (const [a2, n] of Object.entries(ALPHA2_TO_NUMERIC)) out[n] = a2;
      return out;
    }
  },
  watch: {
    mapData: { handler () { this.recomputeValues(); }, deep: true },
    src () { this.recomputeValues(); },
    dst () { this.recomputeValues(); },
    xffGeo () { this.recomputeValues(); }
  },
  async mounted () {
    this.readThemeColors();
    await this.loadDeps();
    this.recomputeValues();
    if (typeof ResizeObserver !== 'undefined') {
      this._resizeObserver = new ResizeObserver(() => this.refit());
      this._resizeObserver.observe(this.$refs.host);
    }
  },
  beforeUnmount () {
    if (this._resizeObserver) this._resizeObserver.disconnect();
  },
  methods: {
    commaString,
    readThemeColors () {
      const styles = window.getComputedStyle(document.body);
      const water = styles.getPropertyValue('--color-water').trim();
      let lLight = styles.getPropertyValue('--color-land-light').trim();
      let lDark = styles.getPropertyValue('--color-land-dark').trim();
      if (!lLight || !lDark) {
        lLight = styles.getPropertyValue('--color-primary-lightest').trim();
        lDark = styles.getPropertyValue('--color-primary-dark').trim();
      }
      if (water) this.waterColor = water;
      if (lLight) this.landColorLight = lLight;
      if (lDark) this.landColorDark = lDark;
    },
    async loadDeps () {
      // Lazy-load d3 / topojson / world-atlas — mirrors the
      // Connections.vue / Hierarchy.vue precedent so the deps don't
      // bloat the main bundle and the topojson is only fetched when
      // the map actually mounts.
      const [d3, topojson, world] = await Promise.all([
        import('d3'),
        import('topojson-client'),
        import('world-atlas/countries-110m.json')
      ]);
      this.d3 = d3;
      const featCol = topojson.feature(world.default, world.default.objects.countries);
      this.features = featCol.features;
      this.sphere = { type: 'Sphere' };
      this.refit(featCol);
      this.ready = true;
    },
    refit (featCol) {
      const host = this.$refs.host;
      if (!host || !this.d3) return;
      const fc = featCol || { type: 'FeatureCollection', features: this.features };
      this.width = host.clientWidth || 800;
      this.height = Math.max(120, host.clientHeight || Math.round(this.width * 0.45));
      const projection = this.d3.geoNaturalEarth1()
        .fitSize([this.width, this.height], fc);
      this.pathGen = this.d3.geoPath(projection);
    },
    recomputeValues () {
      if (!this.mapData) return;
      const tot = {};
      const add = (bucket) => {
        if (!bucket) return;
        for (const k in bucket) tot[k] = (tot[k] || 0) + bucket[k];
      };
      if (this.src) add(this.mapData.src);
      if (this.dst) add(this.mapData.dst);
      if (this.xffGeo) add(this.mapData.xffGeo);

      // Convert alpha-2 keys to numeric M49 ids for topojson lookup.
      const numericValues = {};
      for (const code in tot) {
        const n = ALPHA2_TO_NUMERIC[code];
        if (n) numericValues[n] = tot[code];
      }
      this.values = numericValues;

      const max = Math.max(...Object.values(numericValues), 1);
      if (this.d3) {
        this.colorScale = this.d3.scaleLinear()
          .domain([0, max])
          .range([this.landColorLight, this.landColorDark]);
      }

      // Emit top-10 (alpha-2 codes) so the parent can render its legend.
      const sorted = Object.entries(tot).sort((a, b) => b[1] - a[1]).slice(0, 10);
      const legend = sorted.map(([code, value]) => ({
        name: code,
        value,
        color: this.colorScale ? this.colorScale(value) : this.landColorDark
      }));
      this.$emit('updateLegend', legend);
    },
    pathFor (feat) {
      return this.pathGen ? this.pathGen(feat) : '';
    },
    fillFor (feat) {
      const v = this.values[feat.id];
      if (!v || !this.colorScale) return this.landColorLight;
      return this.colorScale(v);
    },
    onHover (e, feat) {
      const code = this.numericToAlpha2[feat.id];
      const rect = this.$refs.host.getBoundingClientRect();
      this.hover = {
        x: e.clientX - rect.left + 8,
        y: e.clientY - rect.top + 8,
        name: feat.properties?.name || feat.id,
        code,
        value: this.values[feat.id] || 0
      };
    },
    onLeave () { this.hover = null; },
    onClick (feat) {
      const code = this.numericToAlpha2[feat.id];
      if (!code) return;
      this.$emit('regionClick', code);
    },
    /* Public API for parent: zoom controls drive the SVG via $ref. */
    zoomIn () {
      this.zoomLevel = Math.min(ZOOM_MAX, this.zoomLevel * ZOOM_STEP);
    },
    zoomOut () {
      this.zoomLevel = Math.max(ZOOM_MIN, this.zoomLevel / ZOOM_STEP);
    },
    resetZoom () { this.zoomLevel = 1; }
  }
};
</script>

<style scoped>
.arkime-world-map {
  position: relative;
  width: 100%;
  height: 100%;
  min-height: 120px;
}
.arkime-world-map svg {
  display: block;
  width: 100%;
  height: 100%;
}
.world-map-zoom {
  transition: transform 200ms ease-out;
}
.world-map-country {
  stroke: rgba(0, 0, 0, 0.35);
  stroke-width: 0.3;
  cursor: pointer;
  transition: stroke 0.1s, stroke-width 0.1s;
}
.world-map-country:hover {
  stroke: #fff;
  stroke-width: 1;
}
.world-map-tooltip {
  position: absolute;
  background: rgba(0, 0, 0, 0.85);
  color: #fff;
  padding: 4px 8px;
  font-size: 11px;
  border-radius: 3px;
  pointer-events: none;
  white-space: nowrap;
  z-index: 10;
}
.world-map-zoom-buttons {
  position: absolute;
  top: 4px;
  left: 4px;
  display: flex;
  flex-direction: column;
  gap: 2px;
  z-index: 5;
}
.world-map-zoom-buttons .btn {
  width: 22px;
  padding: 0;
  line-height: 18px;
}
</style>
