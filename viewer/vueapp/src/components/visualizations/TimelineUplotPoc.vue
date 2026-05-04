<!--
Copyright Yahoo Inc.
SPDX-License-Identifier: Apache-2.0

Phase 2a POC: validates uPlot as a Flot replacement for the Visualizations
timeline graph. Renders the same `graphData` shape Visualizations.vue feeds
into Flot, so it can be mounted side-by-side under the `?vizpoc=uplot`
query param for visual parity comparison.

Throwaway code. Library-fit decision happens in the Phase 2 decision doc;
2b will rewrite the timeline using whatever lib is chosen.
-->
<template>
  <div class="timeline-uplot-poc">
    <div class="poc-banner">
      uPlot POC ({{ uplotVersion }}) — graphType={{ graphType }} seriesType={{ seriesType }}
      <span
        v-if="bucketFactor > 1"
        class="poc-rebucketed">· rebucketed {{ bucketFactor }}× client-side</span>
    </div>
    <div class="poc-controls">
      <button
        type="button"
        class="btn btn-xs btn-default"
        @click="panLeft">
        ←
      </button>
      <button
        type="button"
        class="btn btn-xs btn-default"
        @click="zoomIn">
        zoom in
      </button>
      <button
        type="button"
        class="btn btn-xs btn-default"
        @click="zoomOut">
        zoom out
      </button>
      <button
        type="button"
        class="btn btn-xs btn-default"
        @click="panRight">
        →
      </button>
      <span class="poc-status">{{ pointCount }} pts · {{ rangeLabel }}</span>
    </div>
    <div
      ref="chartEl"
      class="uplot-host" />
  </div>
</template>

<script>
import uPlot from 'uplot';
import 'uplot/dist/uPlot.min.css';
import moment from 'moment';

// Match Flot palette (Visualizations.vue srcColor / dstColor / foregroundColor)
const SRC_COLOR = '#0099CC';
const DST_COLOR = '#FF6633';
const FG_COLOR = '#666666';
const RESTART_COLOR = 'rgba(220, 53, 69, 0.7)';
const BUSINESS_COLOR = 'rgba(255, 210, 50, 0.2)';

export default {
  name: 'TimelineUplotPoc',
  props: {
    graphData: { type: Object, required: true },
    graphType: { type: String, default: 'sessionsHisto' },
    seriesType: { type: String, default: 'bars' },
    capStartTimes: { type: Array, default: () => [] },
    showCapStartTimes: { type: Boolean, default: false }
  },
  emits: ['updateTimeRange'],
  data () {
    return {
      uplotVersion: '1.6.x',
      pointCount: 0,
      rangeLabel: '',
      bucketFactor: 1
    };
  },
  watch: {
    graphData: { handler () { this.rebuild(); }, deep: true },
    graphType () { this.rebuild(); },
    seriesType () { this.rebuild(); },
    showCapStartTimes () { this.rebuild(); }
  },
  mounted () {
    this.rebuild();
    this._resizeHandler = () => this.resize();
    window.addEventListener('resize', this._resizeHandler);
  },
  beforeUnmount () {
    window.removeEventListener('resize', this._resizeHandler);
    this.destroyPlot();
  },
  methods: {
    seriesDefsFor (type) {
      switch (type) {
      case 'totPacketsHisto':
      case 'network.packetsHisto':
        return [
          { label: 'src packets', key: 'source.packetsHisto', color: SRC_COLOR },
          { label: 'dst packets', key: 'destination.packetsHisto', color: DST_COLOR }
        ];
      case 'totBytesHisto':
      case 'network.bytesHisto':
        return [
          { label: 'src bytes', key: 'source.bytesHisto', color: SRC_COLOR },
          { label: 'dst bytes', key: 'destination.bytesHisto', color: DST_COLOR }
        ];
      case 'totDataBytesHisto':
        return [
          { label: 'client bytes', key: 'client.bytesHisto', color: SRC_COLOR },
          { label: 'server bytes', key: 'server.bytesHisto', color: DST_COLOR }
        ];
      default:
        return [{ label: type, key: type, color: FG_COLOR }];
      }
    },
    /**
     * Flot uses [[tsMs, value], ...] per series. uPlot wants columnar:
     * [xs[], y0[], y1[], ...] with timestamps in seconds. Take the union of
     * all series timestamps so the x axis covers every point.
     */
    buildAlignedData (defs) {
      const tsSet = new Set();
      for (const d of defs) {
        for (const [ts] of (this.graphData[d.key] || [])) tsSet.add(ts);
      }
      if (this.graphData.xmin) tsSet.add(this.graphData.xmin);
      if (this.graphData.xmax) tsSet.add(this.graphData.xmax);

      const sortedMs = [...tsSet].sort((a, b) => a - b);
      const xs = sortedMs.map((ms) => ms / 1000);
      const ys = defs.map((d) => {
        const map = new Map(this.graphData[d.key] || []);
        return sortedMs.map((ms) => map.get(ms) ?? 0);
      });
      return [xs, ...ys];
    },
    /**
     * Adaptive client-side rebucketing. When the backend returns more buckets
     * than the chart can render at >= MIN_PX_PER_BAR pixels each, sum every
     * `factor` adjacent buckets into one. Loses precision in exchange for
     * legible bars. Returns { data, factor }.
     *
     * Phase 2a POC trigger: Elyse's "columns are hard to see" feedback.
     * Phase 2b will revisit whether to drive bucketing from the backend
     * histogram interval instead (bigger architectural change, see v7
     * UI ideas idea #6 "better timeline graph").
     */
    aggregateIfDense (cols, hostWidth) {
      const MIN_PX_PER_BAR = 8;
      const Y_AXIS_RESERVE = 60;
      const PADDING = 16;
      const drawable = Math.max(100, hostWidth - Y_AXIS_RESERVE - PADDING);
      const xs = cols[0];
      const ys = cols.slice(1);
      const maxBars = Math.max(1, Math.floor(drawable / MIN_PX_PER_BAR));
      if (xs.length <= maxBars) return { data: cols, factor: 1 };

      const factor = Math.ceil(xs.length / maxBars);
      const reXs = [];
      const reYs = ys.map(() => []);
      for (let i = 0; i < xs.length; i += factor) {
        reXs.push(xs[i]); // anchor on first timestamp of group
        ys.forEach((y, idx) => {
          let sum = 0;
          for (let j = 0; j < factor && (i + j) < y.length; j++) {
            sum += y[i + j];
          }
          reYs[idx].push(sum);
        });
      }
      return { data: [reXs, ...reYs], factor };
    },
    /** uPlot draw hook: paints business-hours bands then capture-restart lines. */
    drawMarkings (u) {
      const ctx = u.ctx;
      const xMin = u.scales.x.min;
      const xMax = u.scales.x.max;
      ctx.save();

      // Business-hour yellow bands (drawn behind series — `draw` hook fires
      // after series; for true "behind", a dedicated plugin would inject
      // before drawSeries. POC accepts the visual order as-is.)
      const bands = this.computeBusinessHourBands(xMin, xMax);
      ctx.fillStyle = BUSINESS_COLOR;
      for (const b of bands) {
        const left = u.valToPos(b.from, 'x', true);
        const right = u.valToPos(b.to, 'x', true);
        ctx.fillRect(
          left,
          u.bbox.top,
          Math.max(1, right - left),
          u.bbox.height
        );
      }

      // Capture-restart vertical markers
      if (this.showCapStartTimes && this.capStartTimes?.length) {
        ctx.strokeStyle = RESTART_COLOR;
        ctx.lineWidth = 1;
        for (const cap of this.capStartTimes) {
          if (!cap.startTime) continue;
          const xSec = cap.startTime / 1000;
          if (xSec < xMin || xSec > xMax) continue;
          const x = u.valToPos(xSec, 'x', true);
          ctx.beginPath();
          ctx.moveTo(x, u.bbox.top);
          ctx.lineTo(x, u.bbox.top + u.bbox.height);
          ctx.stroke();
        }
      }
      ctx.restore();
    },
    computeBusinessHourBands (xMinSec, xMaxSec) {
      const out = [];
      const c = this.$constants || {};
      if (c.BUSINESS_DAY_START === undefined || c.BUSINESS_DAY_END === undefined || !c.BUSINESS_DAYS) {
        return out;
      }
      const businessDays = c.BUSINESS_DAYS.split(',');
      const startDate = moment(xMinSec * 1000);
      const stopDate = moment(xMaxSec * 1000);
      let daysInRange = stopDate.diff(startDate, 'days');
      if (daysInRange > 31) return out;

      const day = stopDate.startOf('day');
      while (daysInRange >= 0) {
        const dayOfWeek = day.day();
        if (businessDays.indexOf(dayOfWeek.toString()) >= 0) {
          const dayStart = day.clone().add(c.BUSINESS_DAY_START, 'hours');
          const dayStop = day.clone().add(c.BUSINESS_DAY_END, 'hours');
          out.push({ from: dayStart.valueOf() / 1000, to: dayStop.valueOf() / 1000 });
        }
        day.subtract(24, 'hours');
        daysInRange--;
      }
      return out;
    },
    formatY (v) {
      const isBytes = this.graphType === 'totBytesHisto' ||
                      this.graphType === 'totDataBytesHisto' ||
                      this.graphType === 'network.bytesHisto';
      if (isBytes) {
        if (v >= 1e12) return (v / 1e12).toFixed(1) + 'T';
        if (v >= 1e9) return (v / 1e9).toFixed(1) + 'G';
        if (v >= 1e6) return (v / 1e6).toFixed(1) + 'M';
        if (v >= 1e3) return (v / 1e3).toFixed(1) + 'K';
        return v.toFixed(0);
      }
      if (v >= 1e9) return (v / 1e9).toFixed(1) + 'B';
      if (v >= 1e6) return (v / 1e6).toFixed(1) + 'M';
      if (v >= 1e3) return (v / 1e3).toFixed(1) + 'K';
      return v.toString();
    },
    destroyPlot () {
      if (this.plot) {
        this.plot.destroy();
        this.plot = null;
      }
    },
    rebuild () {
      const host = this.$refs.chartEl;
      if (!host || !this.graphData) return;
      this.destroyPlot();

      const defs = this.seriesDefsFor(this.graphType);
      const raw = this.buildAlignedData(defs);
      const { data, factor } = this.aggregateIfDense(raw, host.clientWidth || 800);
      this.bucketFactor = factor;
      this.pointCount = data[0].length;
      this.rangeLabel = data[0].length
        ? `${moment(data[0][0] * 1000).format('MM/DD HH:mm')} → ${moment(data[0][data[0].length - 1] * 1000).format('MM/DD HH:mm')}`
        : '(empty)';

      const isBars = this.seriesType === 'bars';
      // 0.9 slot fill + 1px gap + center align gives discrete-looking bars.
      // POC tuned 2026-05-04 from initial 0.6/right-align.
      const barsPath = isBars
        ? uPlot.paths.bars({ size: [0.9, Infinity], align: 0, gap: 1 })
        : undefined;

      const opts = {
        width: host.clientWidth || 800,
        height: 170,
        padding: [8, 8, 8, 8],
        scales: {
          x: { time: true },
          y: { auto: true, range: (u, dmin, dmax) => [0, dmax * 1.05] }
        },
        axes: [
          { stroke: '#888', grid: { stroke: 'rgba(128,128,128,0.15)' } },
          {
            stroke: '#888',
            grid: { stroke: 'rgba(128,128,128,0.15)' },
            values: (u, vals) => vals.map((v) => this.formatY(v)),
            size: 60
          }
        ],
        series: [
          {},
          ...defs.map((d) => ({
            label: d.label,
            stroke: d.color,
            fill: isBars ? d.color : d.color + '33',
            paths: barsPath,
            width: isBars ? 0 : 1.5,
            points: { show: false }
          }))
        ],
        hooks: {
          draw: [(u) => this.drawMarkings(u)],
          setSelect: [(u) => this.onSelect(u)]
        },
        cursor: {
          drag: { x: true, y: false, setScale: false }
        },
        select: { show: true }
      };

      this.plot = new uPlot(opts, data, host);
    },
    resize () {
      if (!this.plot || !this.$refs.chartEl) return;
      this.plot.setSize({
        width: this.$refs.chartEl.clientWidth,
        height: 170
      });
    },
    onSelect (u) {
      const sel = u.select;
      if (!sel.width) return;
      const fromSec = u.posToVal(sel.left, 'x');
      const toSec = u.posToVal(sel.left + sel.width, 'x');
      this.$emit('updateTimeRange', {
        startTime: fromSec.toFixed(),
        stopTime: toSec.toFixed()
      });
    },
    panLeft () {
      if (!this.plot) return;
      const { min, max } = this.plot.scales.x;
      const delta = (max - min) * 0.1;
      this.plot.setScale('x', { min: min - delta, max: max - delta });
    },
    panRight () {
      if (!this.plot) return;
      const { min, max } = this.plot.scales.x;
      const delta = (max - min) * 0.1;
      this.plot.setScale('x', { min: min + delta, max: max + delta });
    },
    zoomIn () {
      if (!this.plot) return;
      const { min, max } = this.plot.scales.x;
      const span = max - min;
      const center = (min + max) / 2;
      this.plot.setScale('x', { min: center - span * 0.25, max: center + span * 0.25 });
    },
    zoomOut () {
      if (!this.plot) return;
      const { min, max } = this.plot.scales.x;
      const span = max - min;
      const center = (min + max) / 2;
      this.plot.setScale('x', { min: center - span, max: center + span });
    }
  }
};
</script>

<style scoped>
.timeline-uplot-poc {
  margin-top: 12px;
  padding: 8px;
  border: 2px dashed #f93;
  background: rgba(255, 153, 51, 0.05);
  border-radius: 4px;
}
.poc-banner {
  font-family: ui-monospace, monospace;
  font-size: 11px;
  color: #f93;
  margin-bottom: 6px;
  text-transform: uppercase;
  letter-spacing: 0.05em;
}
.poc-controls {
  display: flex;
  gap: 4px;
  align-items: center;
  margin-bottom: 6px;
  font-family: ui-monospace, monospace;
  font-size: 11px;
}
.poc-status {
  margin-left: auto;
  color: #888;
}
.poc-rebucketed {
  color: #ffd800;
  margin-left: 6px;
}
.uplot-host {
  width: 100%;
  height: 170px;
}
</style>
