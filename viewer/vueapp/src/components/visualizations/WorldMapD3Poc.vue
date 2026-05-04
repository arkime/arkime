<!--
Copyright Yahoo Inc.
SPDX-License-Identifier: Apache-2.0

Phase 2a POC: validates D3 + world-atlas topojson as a jvectormap
replacement. Renders the same `mapData` shape Visualizations.vue feeds
into jvectormap, so it can be mounted side-by-side under the
`?vizpoc=d3map` query param for visual parity comparison.

Throwaway code. Library-fit decision happens in the Phase 2 decision doc;
2b will rewrite the map using whatever lib is chosen.

Note on country codes: jvectormap world_en uses ISO-3166-1 alpha-2 codes,
which is what Arkime's `country` field stores. world-atlas topojson uses
numeric M49 ids. The POC ships an inline alpha-2 ↔ numeric lookup for
the ~250 countries that have both codes. 2b will probably move this to
a shared util.
-->
<template>
  <div class="world-map-d3-poc">
    <div class="poc-banner">
      D3 + world-atlas POC — {{ topCount }} countries with data, total = {{ totalLabel }}
    </div>
    <div
      ref="mapHost"
      class="d3-map-host">
      <svg
        v-if="ready"
        ref="svgEl"
        :viewBox="`0 0 ${width} ${height}`"
        preserveAspectRatio="xMidYMid meet">
        <g class="countries">
          <path
            v-for="feat in features"
            :key="feat.id"
            :d="pathFor(feat)"
            :fill="fillFor(feat)"
            class="country"
            @mouseenter="(e) => onHover(e, feat)"
            @mouseleave="onLeave"
            @click="onClick(feat)" />
        </g>
      </svg>
      <div
        v-if="hover"
        class="d3-map-tooltip"
        :style="{left: hover.x + 'px', top: hover.y + 'px'}">
        <strong>{{ hover.name }}</strong> ({{ hover.code || '?' }})
        — {{ commaString(hover.value || 0) }}
      </div>
    </div>
    <div
      v-if="legend.length"
      class="d3-map-legend">
      <strong>Top 10</strong>
      <span
        v-for="(item, idx) in legend"
        :key="idx"
        class="legend-item"
        :style="{backgroundColor: legendColor(item.value)}">
        {{ item.code }} ({{ commaString(item.value) }})
      </span>
    </div>
  </div>
</template>

<script>
import { commaString } from '@common/vueFilters.js';
import { ALPHA2_TO_NUMERIC } from './alpha2ToNumeric.js';

export default {
  name: 'WorldMapD3Poc',
  props: {
    mapData: { type: Object, required: true },
    src: { type: Boolean, default: true },
    dst: { type: Boolean, default: true },
    xffGeo: { type: Boolean, default: false }
  },
  emits: ['regionClick'],
  data () {
    return {
      ready: false,
      width: 800,
      height: 360,
      features: [],
      d3: null,
      pathGen: null,
      colorScale: null,
      values: {},
      legend: [],
      hover: null,
      topCount: 0,
      totalLabel: '0'
    };
  },
  watch: {
    mapData: { handler () { this.recomputeValues(); }, deep: true },
    src () { this.recomputeValues(); },
    dst () { this.recomputeValues(); },
    xffGeo () { this.recomputeValues(); }
  },
  async mounted () {
    await this.loadDeps();
    this.recomputeValues();
  },
  methods: {
    commaString,
    async loadDeps () {
      // Lazy-load d3 / topojson / world-atlas — mirrors Connections.vue
      // and Hierarchy.vue precedent so the deps don't bloat the main bundle.
      const [d3, topojson, world] = await Promise.all([
        import('d3'),
        import('topojson-client'),
        import('world-atlas/countries-110m.json')
      ]);
      this.d3 = d3;

      const featCol = topojson.feature(world.default, world.default.objects.countries);
      this.features = featCol.features;

      // Project + size to host width
      const host = this.$refs.mapHost;
      this.width = host?.clientWidth || 800;
      this.height = Math.round(this.width * 0.45);

      const projection = d3.geoNaturalEarth1()
        .fitSize([this.width, this.height], featCol);
      this.pathGen = d3.geoPath(projection);

      this.ready = true;
    },
    recomputeValues () {
      if (!this.mapData) return;
      const tot = {};
      const add = (bucket) => {
        if (!bucket) return;
        for (const k in bucket) {
          tot[k] = (tot[k] || 0) + bucket[k];
        }
      };
      if (this.src) add(this.mapData.src);
      if (this.dst) add(this.mapData.dst);
      if (this.xffGeo) add(this.mapData.xffGeo);

      // Translate alpha-2 keys to numeric so we can look up topojson features
      const numericValues = {};
      for (const code in tot) {
        const numeric = ALPHA2_TO_NUMERIC[code];
        if (numeric) numericValues[numeric] = tot[code];
      }
      this.values = numericValues;
      this.topCount = Object.keys(numericValues).length;
      const total = Object.values(numericValues).reduce((a, b) => a + b, 0);
      this.totalLabel = commaString(total);

      const max = Math.max(...Object.values(numericValues), 1);
      if (this.d3) {
        this.colorScale = this.d3.scaleSequential(this.d3.interpolateBlues)
          .domain([0, max]);
      }

      // Top-10 legend (alpha-2 codes for human readability)
      const legend = Object.entries(tot)
        .sort((a, b) => b[1] - a[1])
        .slice(0, 10)
        .map(([code, value]) => ({ code, value }));
      this.legend = legend;
    },
    pathFor (feat) {
      return this.pathGen ? this.pathGen(feat) : '';
    },
    fillFor (feat) {
      const v = this.values[feat.id];
      if (!v || !this.colorScale) return '#2a2a2a';
      return this.colorScale(v);
    },
    legendColor (value) {
      return this.colorScale ? this.colorScale(value) : '#888';
    },
    numericToAlpha2 (numeric) {
      // Reverse lookup is rare — only used for hover labels. Linear scan fine.
      for (const [a2, n] of Object.entries(ALPHA2_TO_NUMERIC)) {
        if (n === numeric) return a2;
      }
      return null;
    },
    onHover (e, feat) {
      const code = this.numericToAlpha2(feat.id);
      const rect = this.$refs.mapHost.getBoundingClientRect();
      this.hover = {
        x: e.clientX - rect.left + 8,
        y: e.clientY - rect.top + 8,
        name: feat.properties?.name || feat.id,
        code,
        value: this.values[feat.id] || 0
      };
    },
    onLeave () {
      this.hover = null;
    },
    onClick (feat) {
      const code = this.numericToAlpha2(feat.id);
      if (!code) return;
      this.$emit('regionClick', code);
    }
  }
};
</script>

<style scoped>
.world-map-d3-poc {
  margin-top: 12px;
  padding: 8px;
  border: 2px dashed #39c;
  background: rgba(51, 153, 204, 0.05);
  border-radius: 4px;
}
.poc-banner {
  font-family: ui-monospace, monospace;
  font-size: 11px;
  color: #39c;
  margin-bottom: 6px;
  text-transform: uppercase;
  letter-spacing: 0.05em;
}
.d3-map-host {
  position: relative;
  width: 100%;
}
.d3-map-host svg {
  display: block;
  width: 100%;
  height: auto;
  background: #0d1117;
}
.country {
  stroke: #1a1a1a;
  stroke-width: 0.3;
  cursor: pointer;
  transition: stroke 0.1s, stroke-width 0.1s;
}
.country:hover {
  stroke: #fff;
  stroke-width: 1;
}
.d3-map-tooltip {
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
.d3-map-legend {
  margin-top: 6px;
  font-family: ui-monospace, monospace;
  font-size: 11px;
}
.d3-map-legend .legend-item {
  display: inline-block;
  margin: 2px 4px 2px 0;
  padding: 1px 5px;
  border-radius: 2px;
  color: #fff;
  text-shadow: 0 0 2px rgba(0, 0, 0, 0.7);
}
</style>
