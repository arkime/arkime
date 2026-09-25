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
      :class="{'is-dragging': isDragging}"
      :style="{backgroundColor: waterColor}"
      @wheel.prevent="onWheel"
      @mousedown="onMouseDown"
      @mousemove="onMouseMove"
      @mouseup="onMouseUp"
      @mouseleave="onMouseUp">
      <g
        class="world-map-zoom"
        :class="{'world-map-zoom--instant': wheeling}"
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
      <v-btn
        variant="outlined"
        size="default"
        icon
        aria-label="Zoom in"
        @click="zoomIn">
        <v-icon icon="mdi-plus" />
      </v-btn>
      <v-btn
        variant="outlined"
        size="default"
        icon
        aria-label="Zoom out"
        @click="zoomOut">
        <v-icon icon="mdi-minus" />
      </v-btn>
    </div>
  </div>
</template>

<script>
import { commaString } from '@common/vueFilters.js';
import { themedColor } from '@common/themes/themedColor.js';
import { ALPHA2_TO_NUMERIC } from '@common/alpha2ToNumeric.js';

const ZOOM_STEP = 1.4;
const ZOOM_MIN = 1;
const ZOOM_MAX = 8;
// Countries with no traffic render white so they read clearly as "no
// data" instead of blending into the low (lightest) end of the
// land-color scale, which a country with a small-but-nonzero count gets.
const NO_DATA_COLOR = '#ffffff';

export default {
  name: 'ArkimeWorldMap',
  props: {
    mapData: { type: Object, default: () => ({}) },
    src: { type: Boolean, default: true },
    dst: { type: Boolean, default: true },
    xffGeo: { type: Boolean, default: false },
    // single pre-merged layer { alpha2Code: count } (dashboard geo-field map);
    // when set it replaces the src/dst/xffGeo layers
    layerValues: { type: Object, default: null }
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
      panX: 0,
      panY: 0,
      isDragging: false,
      dragStart: null,
      wasDragged: false,
      wheeling: false,
      // theme colors — read from CSS custom props on mount
      waterColor: '#162a3d',
      landColorLight: '#2c3e50',
      landColorDark: '#65a3ff'
    };
  },
  computed: {
    zoomTransform () {
      return {
        transform: `translate(${this.panX}px, ${this.panY}px) scale(${this.zoomLevel})`,
        transformOrigin: 'center'
      };
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
    layerValues: { handler () { this.recomputeValues(); }, deep: true },
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
    clearTimeout(this._wheelTimer);
  },
  methods: {
    commaString,
    readThemeColors () {
      const water = themedColor('water');
      let lLight = themedColor('land-light');
      let lDark = themedColor('land-dark');
      if (!lLight || !lDark) {
        lLight = themedColor('primary-lightest');
        lDark = themedColor('primary-dark');
      }
      if (water) this.waterColor = water;
      if (lLight) this.landColorLight = lLight;
      if (lDark) this.landColorDark = lDark;
    },
    async loadDeps () {
      // Lazy-load d3 / topojson / world-atlas — mirrors the
      // ConnectionsGraph.vue / Hierarchy.vue precedent so the deps don't
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
      const tot = {};
      if (this.layerValues) {
        // single pre-merged geo-field layer (dashboard map widget)
        for (const k in this.layerValues) tot[k] = this.layerValues[k];
      } else {
        if (!this.mapData) return;
        const add = (bucket) => {
          if (!bucket) return;
          for (const k in bucket) tot[k] = (tot[k] || 0) + bucket[k];
        };
        if (this.src) add(this.mapData.src);
        if (this.dst) add(this.mapData.dst);
        if (this.xffGeo) add(this.mapData.xffGeo);
      }

      // Convert alpha-2 keys to numeric M49 ids for topojson lookup.
      const numericValues = {};
      for (const code in tot) {
        const n = ALPHA2_TO_NUMERIC[code];
        if (n) numericValues[n] = tot[code];
      }
      this.values = numericValues;

      const max = Math.max(...Object.values(numericValues), 1);
      if (this.d3) {
        // 4th-root curve: sqrt (0.5) was still crushing #5-10 to
        // near-light under US-heavy traffic. exponent 0.25 lifts a
        // value at 10% of max to ~56% of light->dark instead of ~32%,
        // so the top-10 legend reads as ten distinct shades.
        this.colorScale = this.d3.scalePow().exponent(0.25)
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
      if (!v || !this.colorScale) return NO_DATA_COLOR;
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
      // Suppress click-to-filter when the user just panned. wasDragged
      // resets on the next mousedown.
      if (this.wasDragged) return;
      const code = this.numericToAlpha2[feat.id];
      if (!code) return;
      this.$emit('regionClick', code);
    },
    /* Pan handlers — left-button drag translates the zoom group. A 3 px
       movement threshold separates a pan from a click on a country path. */
    onMouseDown (e) {
      if (e.button !== 0) return;
      this.dragStart = {
        x: e.clientX,
        y: e.clientY,
        panX: this.panX,
        panY: this.panY
      };
      this.wasDragged = false;
    },
    onMouseMove (e) {
      if (!this.dragStart) return;
      const dx = e.clientX - this.dragStart.x;
      const dy = e.clientY - this.dragStart.y;
      if (!this.isDragging && (dx * dx + dy * dy) > 9) {
        this.isDragging = true;
        this.wasDragged = true;
        this.hover = null; // hide hover label while panning
      }
      if (this.isDragging) {
        this.panX = this.dragStart.panX + dx;
        this.panY = this.dragStart.panY + dy;
      }
    },
    onMouseUp () {
      this.dragStart = null;
      this.isDragging = false;
    },
    /* Mouse-wheel zoom anchored on the cursor; transition is suppressed
       (wheeling) so rapid scrolls track the pointer instead of lagging. */
    onWheel (e) {
      const rect = this.$refs.svgEl.getBoundingClientRect();
      const oldZoom = this.zoomLevel;
      const factor = e.deltaY < 0 ? ZOOM_STEP : 1 / ZOOM_STEP;
      const newZoom = Math.min(ZOOM_MAX, Math.max(ZOOM_MIN, oldZoom * factor));
      if (newZoom === oldZoom) return;
      // Keep the point under the cursor fixed (cursor in viewBox units).
      const cx = (e.clientX - rect.left) * (this.width / rect.width);
      const cy = (e.clientY - rect.top) * (this.height / rect.height);
      this.panX -= (newZoom - oldZoom) * (cx - this.width / 2 - this.panX) / oldZoom;
      this.panY -= (newZoom - oldZoom) * (cy - this.height / 2 - this.panY) / oldZoom;
      this.zoomLevel = newZoom;
      if (newZoom <= ZOOM_MIN) { this.panX = 0; this.panY = 0; }
      this.wheeling = true;
      clearTimeout(this._wheelTimer);
      this._wheelTimer = setTimeout(() => { this.wheeling = false; }, 200);
    },
    /* Public API for parent: zoom controls drive the SVG via $ref. */
    zoomIn () {
      this.zoomLevel = Math.min(ZOOM_MAX, this.zoomLevel * ZOOM_STEP);
    },
    zoomOut () {
      this.zoomLevel = Math.max(ZOOM_MIN, this.zoomLevel / ZOOM_STEP);
      // Re-center when fully zoomed out so the map doesn't sit off-screen.
      if (this.zoomLevel <= ZOOM_MIN) { this.panX = 0; this.panY = 0; }
    },
    resetZoom () { this.zoomLevel = 1; this.panX = 0; this.panY = 0; }
  }
};
</script>

<style scoped>
.arkime-world-map {
  position: relative;
  width: 100%;
  height: 100%;
  min-height: 120px;
  /* Recessed panel treatment to match the timeline graph: rounded
     corners, layered inset shadows on all four sides for a "sunken
     into the page" feel. The SVG inside is clipped to the rounded
     corners via overflow: hidden. */
  border-radius: 6px;
  overflow: hidden;
  box-shadow:
    inset 0 3px 8px rgba(0, 0, 0, 0.22),
    inset 0 1px 0 rgba(0, 0, 0, 0.30),
    inset 1px 0 2px rgba(0, 0, 0, 0.14),
    inset -1px 0 2px rgba(0, 0, 0, 0.14),
    inset 0 -1px 0 rgba(255, 255, 255, 0.05);
}
.arkime-world-map svg {
  display: block;
  width: 100%;
  height: 100%;
  cursor: grab;
  user-select: none;
}
.arkime-world-map svg.is-dragging {
  cursor: grabbing;
}
.arkime-world-map svg.is-dragging .world-map-country {
  /* Don't waste paint on the per-country stroke transition during a pan. */
  pointer-events: none;
}
.world-map-zoom {
  transition: transform 200ms ease-out;
}
.world-map-zoom--instant {
  transition: none;
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
  background: rgba(0, 0, 0, 0.88);
  color: #fff;
  padding: 3px 6px;
  font-size: 10px;
  line-height: 1.35;
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
