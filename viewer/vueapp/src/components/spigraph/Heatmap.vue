<!--
Copyright Yahoo Inc.
SPDX-License-Identifier: Apache-2.0
-->
<template>
  <div
    ref="root"
    class="spigraph-heatmap">
    <template v-if="rows.length && cols.length">
      <div class="heatmap-legend">
        <v-checkbox-btn
          v-model="showCount"
          :label="$t('spigraph.showCount')"
          density="compact"
          hide-details
          class="heatmap-count-toggle me-auto" />
        <span class="heatmap-legend-label">{{ metricLabel }}</span>
        <span class="heatmap-legend-min">0</span>
        <span
          class="heatmap-legend-bar"
          :style="legendGradient" />
        <span class="heatmap-legend-max">{{ formatVal(maxVal) }}</span>
      </div>

      <div class="heatmap-body">
        <div
          class="heatmap-labels"
          :style="{ flex: '0 0 ' + labelW + 'px', width: labelW + 'px', paddingTop: axisH + 'px' }">
          <div
            v-for="(row, r) in rows"
            :key="row.name"
            class="heatmap-row-label"
            :class="{ 'heatmap-row-alt': r % 2 }"
            :style="{ height: rowH + 'px' }"
            :title="row.name">
            <span
              v-if="showCount"
              class="heatmap-row-count">{{ commaString(row.total) }}</span>
            <arkime-session-field
              :field="fieldObj"
              :value="row.name"
              :expr="fieldObj.exp"
              :parse="true"
              :pull-left="true"
              :session-btn="true" />
          </div>
        </div>

        <div
          class="heatmap-divider"
          @mousedown="startDrag" />

        <div
          ref="grid"
          class="heatmap-grid">
          <svg
            :width="gridWidth"
            :height="rows.length * rowH + axisH * 2"
            @mousemove="onMove"
            @mouseleave="tooltip = null">
            <!-- top time axis -->
            <g>
              <text
                v-for="tick in ticks"
                :key="'top' + tick.x"
                :x="tick.x"
                y="14"
                class="heatmap-tick"
                :text-anchor="tick.anchor">
                {{ tick.label }}
              </text>
            </g>
            <!-- rows + cells (offset below the top axis) -->
            <g :transform="`translate(0, ${axisH})`">
              <rect
                v-for="(row, r) in rows"
                :key="'t' + r"
                x="0"
                :y="r * rowH"
                :width="gridWidth"
                :height="rowH"
                :class="['heatmap-track', { 'heatmap-track-alt': r % 2 }]" />
              <rect
                v-for="cell in cells"
                :key="cell.k"
                :x="cell.x"
                :y="cell.y"
                :width="cell.w"
                :height="rowH - 2"
                :style="{ fill: cell.fill }" />
            </g>
            <!-- bottom time axis -->
            <g :transform="`translate(0, ${axisH + rows.length * rowH})`">
              <text
                v-for="tick in ticks"
                :key="'bot' + tick.x"
                :x="tick.x"
                y="14"
                class="heatmap-tick"
                :text-anchor="tick.anchor">
                {{ tick.label }}
              </text>
            </g>
          </svg>
        </div>
      </div>

      <div
        v-if="tooltip"
        class="heatmap-tooltip"
        :style="{ left: tooltip.x + 'px', top: tooltip.y + 'px' }">
        <div><strong>{{ tooltip.name }}</strong></div>
        <div>{{ tooltip.time }}</div>
        <div>{{ metricLabel }}: {{ tooltip.value }}</div>
      </div>
    </template>
  </div>
</template>

<script>
import moment from 'moment-timezone';
import { metricHistoKeys } from '../visualizations/metrics.js';
import { heatmapInterpolator } from '../summary/widgets/chartColors';
import { commaString, timezoneDateString, humanReadableBytes, humanReadableNumber } from '@common/vueFilters.js';

const ROW_H = 26;
const AXIS_H = 18;
const LABEL_W = 280; // value-label gutter width (px), draggable
const MIN_COL_PX = 6; // rebucket columns once cells would be narrower than this

export default {
  name: 'ArkimeHeatmap',
  props: {
    items: { type: Array, required: true }, // top-N values, each with item.graph
    graph: { type: Object, required: true }, // aggregate graph (xmin/xmax/interval)
    fieldObj: { type: Object, required: true },
    metric: { type: String, default: 'sessionsHisto' },
    timelineDataFilters: { type: Array, default: () => [] },
    sortBy: { type: String, default: 'graph' }, // 'name' | 'graph'
    // optional dashboard palette; maps to a sequential interpolator for the
    // cells. Empty / categorical-only palettes keep the themed accent intensity.
    colorScheme: { type: String, default: '' }
  },
  data () {
    return {
      rowH: ROW_H,
      axisH: AXIS_H,
      gridWidth: 600,
      labelW: LABEL_W,
      showCount: true,
      tooltip: null,
      interp: null // sequential interpolator from colorScheme (or null)
    };
  },
  computed: {
    timezone () {
      return this.$store.state.user.settings.timezone;
    },
    metricKeys () {
      return metricHistoKeys(this.metric);
    },
    metricLabel () {
      if (this.metric === 'sessionsHisto') { return this.$t('spigraph.sessions'); }
      const filter = this.timelineDataFilters.find(f => f.dbField === this.metric.slice(0, -5));
      return filter?.friendlyName || this.metric;
    },
    // legend bar gradient matching the cell palette (empty -> CSS accent gradient)
    legendGradient () {
      if (!this.interp) { return {}; }
      const stops = [0, 0.25, 0.5, 0.75, 1].map(t => this.interp(0.12 + 0.88 * t));
      return { background: `linear-gradient(to right, ${stops.join(', ')})` };
    },
    isBytes () {
      return this.metric.toLowerCase().includes('bytes');
    },
    intervalMs () {
      return (this.graph?.interval || 60) * 1000;
    },
    // backend xmin/xmax are null for relative ranges; fall back to the data
    bounds () {
      let xmin = this.graph?.xmin;
      let xmax = this.graph?.xmax;
      if (!xmin || !xmax) {
        let lo = Infinity; let hi = -Infinity;
        for (const item of this.items) {
          for (const key of this.metricKeys) {
            for (const [ts] of (item.graph?.[key] || [])) {
              if (ts < lo) { lo = ts; }
              if (ts > hi) { hi = ts; }
            }
          }
        }
        if (Number.isFinite(lo) && Number.isFinite(hi)) { xmin = lo; xmax = hi; }
      }
      return { xmin, xmax };
    },
    rawCols () {
      const { xmin, xmax } = this.bounds;
      if (!xmin || !xmax || !this.intervalMs) { return []; }
      const out = [];
      for (let t = xmin; t <= xmax; t += this.intervalMs) { out.push(t); }
      return out;
    },
    factor () {
      const maxCols = Math.max(1, Math.floor(this.gridWidth / MIN_COL_PX));
      return this.rawCols.length > maxCols ? Math.ceil(this.rawCols.length / maxCols) : 1;
    },
    cols () {
      const out = [];
      for (let i = 0; i < this.rawCols.length; i += this.factor) { out.push(this.rawCols[i]); }
      return out;
    },
    colSpanMs () {
      return this.intervalMs * this.factor;
    },
    colW () {
      return this.cols.length ? this.gridWidth / this.cols.length : 0;
    },
    rows () {
      const { xmin } = this.bounds;
      if (!xmin || !this.cols.length) { return []; }
      const span = this.colSpanMs;
      const rows = this.items.map((item) => {
        const values = new Array(this.cols.length).fill(0);
        for (const key of this.metricKeys) {
          for (const [ts, val] of (item.graph?.[key] || [])) {
            const idx = Math.floor((ts - xmin) / span);
            if (idx >= 0 && idx < values.length) { values[idx] += val; }
          }
        }
        const total = values.reduce((a, b) => a + b, 0);
        return { name: item.name, values, total };
      });
      if (this.sortBy === 'name') {
        rows.sort((a, b) => String(a.name).localeCompare(String(b.name)));
      } else {
        rows.sort((a, b) => b.total - a.total);
      }
      return rows;
    },
    maxVal () {
      let max = 0;
      for (const row of this.rows) {
        for (const v of row.values) { if (v > max) { max = v; } }
      }
      return max;
    },
    cells () {
      const out = [];
      const w = Math.max(1, this.colW - 1);
      for (let r = 0; r < this.rows.length; r++) {
        const values = this.rows[r].values;
        for (let c = 0; c < values.length; c++) {
          const v = values[c];
          if (v <= 0) { continue; }
          out.push({
            k: r + '-' + c,
            x: c * this.colW + 0.5,
            y: r * this.rowH + 1,
            w,
            fill: this.cellFill(v)
          });
        }
      }
      return out;
    },
    // compact axis-tick format chosen by the visible time span: time-of-day for
    // sub-day ranges, MM/DD within a year, otherwise YYYY/MM/DD
    tickFormat () {
      const { xmin, xmax } = this.bounds;
      const span = (xmax || 0) - (xmin || 0);
      const DAY = 86400000;
      if (!span || span <= 2 * DAY) { return 'HH:mm'; }
      if (span <= 365 * DAY) { return 'MM/DD'; }
      return 'YYYY/MM/DD';
    },
    ticks () {
      if (!this.cols.length) { return []; }
      const count = Math.min(6, this.cols.length);
      const out = [];
      for (let i = 0; i < count; i++) {
        const idx = Math.round((this.cols.length - 1) * (i / Math.max(1, count - 1)));
        const x = idx * this.colW + this.colW / 2;
        out.push({
          x,
          label: this.formatTick(this.cols[idx]),
          anchor: i === 0 ? 'start' : (i === count - 1 ? 'end' : 'middle')
        });
      }
      return out;
    }
  },
  watch: {
    // resizing the label gutter changes the grid width — remeasure after layout
    labelW () {
      this.$nextTick(() => this.measure());
    },
    // resolve the dashboard palette to a sequential interpolator (or null)
    colorScheme: {
      immediate: true,
      async handler (scheme) {
        this.interp = await heatmapInterpolator(scheme);
      }
    }
  },
  mounted () {
    this.measure();
    this._resize = () => this.measure();
    window.addEventListener('resize', this._resize);
    if (typeof ResizeObserver !== 'undefined' && this.$refs.root) {
      // observe root (always present); the grid may mount later once data arrives
      this._ro = new ResizeObserver(() => this.measure());
      this._ro.observe(this.$refs.root);
    }
  },
  beforeUnmount () {
    window.removeEventListener('resize', this._resize);
    if (this._ro) { this._ro.disconnect(); }
  },
  methods: {
    commaString,
    measure () {
      const w = this.$refs.grid?.clientWidth;
      if (w && w > 0) { this.gridWidth = w; }
    },
    startDrag (e) {
      e.preventDefault();
      const startX = e.clientX;
      const startW = this.labelW;
      const bodyW = this.$refs.root?.clientWidth || 1000;
      const onMove = (ev) => {
        const next = startW + (ev.clientX - startX);
        this.labelW = Math.max(80, Math.min(bodyW - 120, next));
      };
      const onUp = () => {
        window.removeEventListener('mousemove', onMove);
        window.removeEventListener('mouseup', onUp);
      };
      window.addEventListener('mousemove', onMove);
      window.addEventListener('mouseup', onUp);
    },
    cellFill (v) {
      const norm = this.maxVal > 0 ? v / this.maxVal : 0;
      // dashboard palette: sequential interpolator encodes intensity by hue
      if (this.interp) { return this.interp(0.12 + 0.88 * norm); }
      // default: themed accent intensity encoded by opacity
      const alpha = (0.12 + 0.88 * norm).toFixed(3);
      return `rgba(var(--v-theme-foreground-accent), ${alpha})`;
    },
    formatVal (v) {
      return this.isBytes ? humanReadableBytes(v) : humanReadableNumber(v);
    },
    // short axis-tick label (tickFormat) honoring the user's timezone setting
    formatTick (ms) {
      const fmt = this.tickFormat;
      if (this.timezone === 'gmt') { return moment.tz(ms, 'utc').format(fmt); }
      if (this.timezone === 'localtz') {
        return moment.tz(ms, Intl.DateTimeFormat().resolvedOptions().timeZone).format(fmt);
      }
      return moment(ms).format(fmt);
    },
    onMove (e) {
      const svgRect = e.currentTarget.getBoundingClientRect();
      const c = Math.floor((e.clientX - svgRect.left) / this.colW);
      // rows are offset below the top axis band
      const r = Math.floor((e.clientY - svgRect.top - this.axisH) / this.rowH);
      if (r < 0 || r >= this.rows.length || c < 0 || c >= this.cols.length) {
        this.tooltip = null;
        return;
      }
      const rootRect = this.$refs.root.getBoundingClientRect();
      this.tooltip = {
        x: e.clientX - rootRect.left + 14,
        y: e.clientY - rootRect.top + 8,
        name: this.rows[r].name,
        time: timezoneDateString(this.cols[c], this.timezone),
        value: this.formatVal(this.rows[r].values[c])
      };
    }
  }
};
</script>

<style scoped>
.spigraph-heatmap {
  position: relative;
}
.spigraph-heatmap .heatmap-legend {
  display: flex;
  align-items: center;
  justify-content: flex-end;
  gap: 4px;
  font-size: 0.75rem;
  padding-bottom: 8px;
}
.spigraph-heatmap .heatmap-count-toggle {
  flex: 0 0 auto;
}
.spigraph-heatmap .heatmap-count-toggle :deep(.v-label) {
  font-size: 0.75rem;
  opacity: 1;
}
.spigraph-heatmap .heatmap-count-toggle :deep(.v-selection-control__wrapper) {
  width: 28px;
  height: 28px;
}
.spigraph-heatmap .heatmap-legend-bar {
  display: inline-block;
  width: 90px;
  height: 10px;
  border-radius: 2px;
  background: linear-gradient(
    to right,
    rgba(var(--v-theme-foreground-accent), 0.12),
    rgba(var(--v-theme-foreground-accent), 1)
  );
}
.spigraph-heatmap .heatmap-body {
  display: flex;
  align-items: flex-start;
}
.spigraph-heatmap .heatmap-labels {
  flex: 0 0 200px;
  width: 200px;
}
/* draggable divider marks where the timeline starts */
.spigraph-heatmap .heatmap-divider {
  flex: 0 0 5px;
  align-self: stretch;
  cursor: col-resize;
  border-left: 2px solid rgba(var(--v-theme-foreground), 0.45);
}
.spigraph-heatmap .heatmap-divider:hover {
  border-left-color: rgb(var(--v-theme-foreground-accent));
}
.spigraph-heatmap .heatmap-row-label {
  display: flex;
  align-items: center;
  white-space: nowrap;
  overflow: hidden;
  font-size: 0.8rem;
  padding: 0 6px;
  background-color: rgba(var(--v-theme-foreground), 0.04);
}
.spigraph-heatmap .heatmap-row-label.heatmap-row-alt {
  background-color: transparent;
}
/* fixed-width count column so the value labels all align */
.spigraph-heatmap .heatmap-row-count {
  flex: 0 0 64px;
  box-sizing: border-box;
  margin-right: 6px;
  padding: 1px 6px;
  border-radius: 9px;
  font-size: 0.7rem;
  font-weight: 600;
  text-align: center;
  background-color: rgba(var(--v-theme-foreground), 0.1);
}
.spigraph-heatmap .heatmap-grid {
  flex: 1 1 auto;
  min-width: 0;
}
.spigraph-heatmap .heatmap-track {
  fill: rgba(var(--v-theme-foreground), 0.04);
}
.spigraph-heatmap .heatmap-track-alt {
  fill: transparent;
}
.spigraph-heatmap .heatmap-tick {
  fill: rgb(var(--v-theme-foreground));
  font-size: 10px;
  opacity: 0.7;
}
.spigraph-heatmap .heatmap-tooltip {
  position: absolute;
  z-index: 10;
  pointer-events: none;
  padding: 4px 8px;
  font-size: 0.75rem;
  white-space: nowrap;
  border-radius: 3px;
  background-color: rgb(var(--v-theme-surface));
  color: rgb(var(--v-theme-foreground));
  box-shadow: 0 2px 8px rgba(0, 0, 0, 0.4);
}
</style>
