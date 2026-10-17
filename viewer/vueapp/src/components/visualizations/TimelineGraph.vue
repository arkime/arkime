<!--
Copyright Yahoo Inc.
SPDX-License-Identifier: Apache-2.0
-->
<template>
  <div
    class="timeline-graph"
    :class="{ 'timeline-graph--fit': fitHeight }">
    <div
      ref="host"
      class="timeline-host" />
    <div
      v-if="tooltip"
      class="timeline-tooltip"
      :class="{'flip-x': tooltip.flipX}"
      :style="{left: tooltip.x + 'px', top: tooltip.y + 'px'}"
      v-html="tooltip.html" />
  </div>
</template>

<script>
import uPlot from 'uplot';
import { themedColor } from '@common/themes/themedColor.js';
import 'uplot/dist/uPlot.min.css';
import { commaString, timezoneDateString, humanReadableBytes, humanReadableNumber } from '@common/vueFilters.js';
import { COMPOSITE_METRICS } from './metrics.js';
import moment from 'moment-timezone';

const HOST_HEIGHT = 180;
const Y_AXIS_RESERVE = 44;
const X_AXIS_SIZE = 28;
const PADDING = 16;
const MIN_PX_PER_BAR = 40;

export default {
  name: 'ArkimeTimelineGraph',
  props: {
    graphData: { type: Object, required: true },
    graphType: { type: String, default: 'sessionsHisto' },
    seriesType: { type: String, default: 'bars' },
    yScale: { type: String, default: 'linear' }, // 'linear' | 'log'
    timelineDataFilters: { type: Array, required: true },
    capStartTimes: { type: Array, default: () => [] },
    showCapStartTimes: { type: Boolean, default: false },
    timezone: { type: String, default: 'local' },
    // fill the container height (dashboard timeline widget) instead of the fixed
    // 180px host; the ResizeObserver rebuilds at the new height as the card resizes
    fitHeight: { type: Boolean, default: false }
  },
  emits: ['updateTimeRange'],
  data () {
    return {
      tooltip: null
    };
  },
  watch: {
    graphData: { handler () { this.rebuild(); }, deep: true },
    graphType () { this.rebuild(); },
    seriesType () { this.rebuild(); },
    yScale () { this.rebuild(); },
    showCapStartTimes () { this.rebuild(); },
    timezone () { this.rebuild(); }
  },
  mounted () {
    this.readThemeColors();
    this.rebuild();
    this._resizeHandler = () => this.resize();
    window.addEventListener('resize', this._resizeHandler);
    // ResizeObserver catches container-width changes that don't fire a
    // window resize (e.g. when the map opens/closes and shrinks plot-container).
    if (typeof ResizeObserver !== 'undefined') {
      this._resizeObserver = new ResizeObserver(() => this.resize());
      this._resizeObserver.observe(this.$refs.host);
    }
    // Belt for mouseleave: uPlot's setCursor doesn't always fire when the
    // pointer exits via the chart edges, so wire it explicitly.
    this._mouseLeaveHandler = () => { this.tooltip = null; };
    this.$refs.host?.addEventListener('mouseleave', this._mouseLeaveHandler);
  },
  beforeUnmount () {
    window.removeEventListener('resize', this._resizeHandler);
    if (this._resizeObserver) { this._resizeObserver.disconnect(); }
    this.$refs.host?.removeEventListener('mouseleave', this._mouseLeaveHandler);
    this.destroyPlot();
  },
  methods: {
    /** Pull theme colors from CSS custom properties, matching Flot's setup. */
    readThemeColors () {
      this.foregroundColor = themedColor('foreground', '#666');
      this.srcColor = themedColor('src', '#CA0404');
      this.dstColor = themedColor('dst', '#0000FF');
      this.axisColor = themedColor('neutral', '#888');
      this.gridColor = 'rgba(128,128,128,0.15)';
      this.businessColor = 'rgba(255, 210, 50, 0.2)';
      this.restartColor = this.foregroundColor;
    },
    seriesDefsFor (type) {
      const defs = COMPOSITE_METRICS[type];
      if (defs) {
        return defs.map(d => ({
          label: d.label,
          key: d.key,
          color: d.role === 'src' ? this.srcColor : this.dstColor
        }));
      }
      return [{ label: this.friendlyTypeName(type), key: type, color: this.foregroundColor }];
    },
    friendlyTypeName (type) {
      if (type === 'sessionsHisto') return 'Sessions';
      const filter = this.timelineDataFilters?.find((f) => f.dbField === type.slice(0, -5));
      return filter?.friendlyName || type;
    },
    /**
     * Build a uniform time grid using the backend-provided interval so
     * uPlot can size bars correctly. Flot used barWidth = interval*1000/1.7
     * regardless of data sparsity. uPlot auto-derives bar width from the
     * minimum gap between consecutive data points, which collapses to the
     * source interval rather than the displayed slot when Arkime ships
     * sparse non-zero buckets — bars come out as 1-2 px ticks. Generating
     * a regular grid from xmin/xmax/interval and binning data into it
     * gives uPlot uniform spacing to work with.
     */
    buildAlignedData (defs) {
      const intervalMs = (this.graphData.interval || 60) * 1000;
      const xmin = this.graphData.xmin;
      const xmax = this.graphData.xmax;

      if (!xmin || !xmax || !intervalMs) {
        // Fall back to data-driven timestamps when bounds are missing.
        const tsSet = new Set();
        for (const d of defs) {
          for (const [ts] of (this.graphData[d.key] || [])) tsSet.add(ts);
        }
        const sortedMs = [...tsSet].sort((a, b) => a - b);
        const fallbackXs = sortedMs.map((ms) => ms / 1000);
        const fallbackYs = defs.map((d) => {
          const map = new Map(this.graphData[d.key] || []);
          return sortedMs.map((ms) => map.get(ms) ?? 0);
        });
        return [fallbackXs, ...fallbackYs];
      }

      // Uniform grid at interval spacing
      const xsMs = [];
      for (let t = xmin; t <= xmax; t += intervalMs) {
        xsMs.push(t);
      }
      const xs = xsMs.map((ms) => ms / 1000);

      // Bin each series' points into the grid
      const ys = defs.map((d) => {
        const seriesData = this.graphData[d.key] || [];
        const arr = new Array(xsMs.length).fill(0);
        for (const [ts, val] of seriesData) {
          if (ts < xmin || ts > xmax) continue;
          const idx = Math.floor((ts - xmin) / intervalMs);
          if (idx >= 0 && idx < arr.length) arr[idx] += val;
        }
        return arr;
      });

      return [xs, ...ys];
    },
    /**
     * Adaptive client-side rebucketing: when the backend hands us more buckets
     * than the chart can render at >= MIN_PX_PER_BAR pixels each, sum every N
     * adjacent buckets into one. Trades timestamp precision for legible bars.
     * v7 idea #6 ("better timeline graph") may move this to backend interval
     * negotiation eventually.
     */
    aggregateIfDense (cols, hostWidth) {
      const drawable = Math.max(100, hostWidth - Y_AXIS_RESERVE - PADDING);
      const xs = cols[0];
      const ys = cols.slice(1);
      const maxBars = Math.max(1, Math.floor(drawable / MIN_PX_PER_BAR));
      if (xs.length <= maxBars) return cols;

      const factor = Math.ceil(xs.length / maxBars);
      const reXs = [];
      const reYs = ys.map(() => []);
      for (let i = 0; i < xs.length; i += factor) {
        reXs.push(xs[i]);
        ys.forEach((y, idx) => {
          let sum = 0;
          for (let j = 0; j < factor && (i + j) < y.length; j++) {
            sum += y[i + j];
          }
          reYs[idx].push(sum);
        });
      }
      return [reXs, ...reYs];
    },
    /**
     * uPlot draw hook (z-order from back to front):
     *  - business-hours bands
     *  - "now" indicator (only when current time is within the chart range)
     *  - capture-restart markers (line + bottom notch)
     */
    drawMarkings (u) {
      const ctx = u.ctx;
      const xMin = u.scales.x.min;
      const xMax = u.scales.x.max;
      ctx.save();

      // Business-hour bands
      const bands = this.computeBusinessHourBands(xMin, xMax);
      ctx.fillStyle = this.businessColor;
      for (const b of bands) {
        const left = u.valToPos(b.from, 'x', true);
        const right = u.valToPos(b.to, 'x', true);
        ctx.fillRect(left, u.bbox.top, Math.max(1, right - left), u.bbox.height);
      }

      // "Now" indicator — vertical dashed line + label at current time
      const nowSec = Date.now() / 1000;
      if (nowSec >= xMin && nowSec <= xMax) {
        const x = u.valToPos(nowSec, 'x', true);
        ctx.save();
        ctx.strokeStyle = '#00ff88';
        ctx.lineWidth = 1;
        ctx.setLineDash([4, 4]);
        ctx.beginPath();
        ctx.moveTo(x, u.bbox.top);
        ctx.lineTo(x, u.bbox.top + u.bbox.height);
        ctx.stroke();
        ctx.restore();

        ctx.save();
        ctx.fillStyle = '#00ff88';
        ctx.font = '9px ui-monospace, monospace';
        ctx.textAlign = 'left';
        ctx.fillText(' NOW', x, u.bbox.top + 9);
        ctx.restore();
      }

      // Capture-restart markers — dashed red vertical + bottom triangle notch
      if (this.showCapStartTimes && this.capStartTimes?.length) {
        const restartC = '#dc3545';
        ctx.strokeStyle = restartC;
        ctx.fillStyle = restartC;
        ctx.lineWidth = 1;
        for (const cap of this.capStartTimes) {
          if (!cap.startTime) continue;
          const xSec = cap.startTime / 1000;
          if (xSec < xMin || xSec > xMax) continue;
          const x = u.valToPos(xSec, 'x', true);
          ctx.save();
          ctx.setLineDash([2, 3]);
          ctx.beginPath();
          ctx.moveTo(x, u.bbox.top);
          ctx.lineTo(x, u.bbox.top + u.bbox.height);
          ctx.stroke();
          ctx.restore();
          const baseY = u.bbox.top + u.bbox.height;
          ctx.beginPath();
          ctx.moveTo(x - 4, baseY);
          ctx.lineTo(x + 4, baseY);
          ctx.lineTo(x, baseY - 5);
          ctx.fill();
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
      // Log scale can emit null/-Infinity ticks for values below the floor;
      // the humanReadable* helpers crash on those, so render them as blank.
      if (v == null || !Number.isFinite(v)) return '';
      const isBytes = this.graphType === 'totBytesHisto' ||
                      this.graphType === 'totDataBytesHisto' ||
                      this.graphType === 'network.bytesHisto';
      return isBytes ? humanReadableBytes(v) : humanReadableNumber(v);
    },
    destroyPlot () {
      if (this.plot) {
        this.plot.destroy();
        this.plot = null;
      }
      this.tooltip = null;
    },
    rebuild () {
      const host = this.$refs.host;
      if (!host || !this.graphData) return;
      this.destroyPlot();

      const defs = this.seriesDefsFor(this.graphType);
      const data = this.aggregateIfDense(this.buildAlignedData(defs), host.clientWidth || 800);

      const isBars = this.seriesType === 'bars';
      const barsPath = isBars ? uPlot.paths.bars() : undefined;
      const linesPath = !isBars
        ? uPlot.paths.linear({ alignGaps: 0 })
        : undefined;

      const monoFont = '11px ui-monospace, "SF Mono", Menlo, Consolas, monospace';

      const opts = {
        width: host.clientWidth || 800,
        height: this.fitHeight ? Math.max(120, host.clientHeight || HOST_HEIGHT) : HOST_HEIGHT,
        // [top, right, bottom, left] — chart hugs left/bottom; tiny top
        // gap is just enough to keep the y-axis top tick from getting
        // clipped by the canvas edge.
        padding: [4, 4, 0, 0],
        legend: { show: false }, // tooltip overlay carries the same info
        scales: {
          x: { time: true },
          // asinh scale (distr:4) compresses outliers like log but is
          // defined at 0 — so empty histogram buckets still render at the
          // baseline instead of disappearing the way log(0) would. Behaves
          // linear near 0 and log-like far from it. Labeled "Log" in the UI
          // since that matches user expectations from other tools.
          y: this.yScale === 'log'
            ? { distr: 4, range: (u, dmin, dmax) => [0, Math.max(10, (dmax || 1) * 1.5)] }
            : { auto: true, range: (u, dmin, dmax) => [0, (dmax || 1) * 1.05] }
        },
        axes: [
          {
            // Let uPlot's built-in multi-resolution time formatter pick HH:mm
            // / MM/DD / YYYY as appropriate for the current zoom level.
            stroke: this.axisColor,
            font: monoFont,
            ticks: { stroke: this.gridColor, width: 1 },
            grid: { stroke: this.gridColor, width: 1 },
            size: X_AXIS_SIZE
          },
          {
            stroke: this.axisColor,
            font: monoFont,
            ticks: { stroke: this.gridColor, width: 1 },
            grid: { stroke: this.gridColor, width: 1 },
            values: (u, vals) => vals.map((v) => this.formatY(v)),
            // Force power-of-10 splits in log mode. uPlot's default asinh
            // splits are symmetric around 0 (..., -1, 0, 1, ...), so a -1
            // tick shows up even when the data is non-negative. Generating
            // splits ourselves keeps the axis to {0, 1, 10, 100, ...}.
            splits: this.yScale === 'log'
              ? (u, axisIdx, scaleMin, scaleMax) => {
                const out = [0];
                for (let p = 0; Math.pow(10, p) <= scaleMax; p++) {
                  out.push(Math.pow(10, p));
                }
                return out;
              }
              : undefined,
            size: Y_AXIS_RESERVE
          }
        ],
        series: [
          {},
          ...defs.map((d) => ({
            label: d.label,
            stroke: d.color,
            fill: isBars ? d.color : d.color + '33',
            paths: isBars ? barsPath : linesPath,
            width: isBars ? 1 : 1.5,
            points: { show: false }
          }))
        ],
        hooks: {
          draw: [(u) => this.drawMarkings(u)],
          setSelect: [(u) => this.onSelect(u)],
          // Belt-and-suspenders: clear tooltip whenever cursor goes off-canvas
          // (uPlot signals this by setting cursor.left to -10).
          setCursor: [(u) => {
            if (u.cursor.left == null || u.cursor.left < 0) {
              this.tooltip = null;
            }
          }]
        },
        cursor: {
          // Disable crosshairs (the dashed x/y lines that follow the
          // cursor) — the tooltip alone carries all the hover affordance
          // we need; crosshairs added visual noise on top of bars.
          x: false,
          y: false,
          drag: { x: true, y: false, setScale: false },
          // The default cursor dot defaults to the series color, which
          // disappears against the bar of the same color. Hide it — the
          // tooltip carries the same info plus more context.
          points: { show: false },
          dataIdx: (u, seriesIdx, hoveredIdx) => {
            this.updateTooltip(u, seriesIdx, hoveredIdx);
            return hoveredIdx;
          }
        },
        select: { show: true }
      };

      this.plot = new uPlot(opts, data, host);
    },
    /**
     * Replicate Flot's hover tooltip. For multi-series stacked types, label
     * Src/Dst by seriesIdx. Show "<value> <type> out of <total> filtered
     * <type> on <date>". When hovering a capture-restart marker (no series
     * point), show the restart message instead.
     */
    updateTooltip (u, seriesIdx, dataIdx) {
      const overEl = u.over;
      const rect = overEl.getBoundingClientRect();
      const hostRect = this.$refs.host.getBoundingClientRect();
      const cursorX = u.cursor.left;
      const cursorY = u.cursor.top;
      if (cursorX == null || cursorX < 0) {
        this.tooltip = null;
        return;
      }

      // Capture-restart hover: only when not hovering a real data point and
      // the cursor x is near a restart marker (within half a bucket).
      if ((dataIdx == null || seriesIdx === 0) && this.showCapStartTimes && this.capStartTimes?.length) {
        const xSec = u.posToVal(cursorX, 'x');
        const tolSec = this.graphData?.interval || 60;
        for (const cap of this.capStartTimes) {
          if (!cap.startTime) continue;
          const capXSec = cap.startTime / 1000;
          if (Math.abs(capXSec - xSec) <= tolSec) {
            const capDateStr = timezoneDateString(cap.startTime, this.timezone, false);
            const message = this.$t('vis.capNodeRestarted', { node: cap.nodeName, when: capDateStr });
            const capXRel = rect.left - hostRect.left + cursorX;
            const capFlipX = capXRel + 12 > hostRect.width * 0.6;
            this.tooltip = {
              x: capFlipX ? capXRel - 12 : capXRel + 12,
              y: rect.top - hostRect.top + cursorY - 28,
              flipX: capFlipX,
              html: `<div class="graph-tooltip-inner">${this.escapeHtml(message)}</div>`
            };
            return;
          }
        }
      }

      if (dataIdx == null || seriesIdx == null || seriesIdx === 0) {
        this.tooltip = null;
        return;
      }

      const defs = this.seriesDefsFor(this.graphType);
      const ts = u.data[0][dataIdx] * 1000;
      const dateStr = timezoneDateString(ts, this.timezone, false);

      // Compact format: per-series value chips on one row, date on the
      // next. For multi-series (src/dst, client/server) each chip is
      // colored to match its bar.
      const chips = [];
      defs.forEach((def, i) => {
        const v = u.data[i + 1]?.[dataIdx];
        if (v == null) return;
        const valStr = commaString(Math.round(v * 100) / 100);
        if (defs.length > 1) {
          const label = def.label.split(' ')[0];
          chips.push(`<span style="color:${def.color}">${this.escapeHtml(label)}</span> <strong>${valStr}</strong>`);
        } else {
          chips.push(`<strong>${valStr}</strong>`);
        }
      });
      if (!chips.length) { this.tooltip = null; return; }
      const lines = [chips.join(' · '), this.escapeHtml(dateStr)];

      const xRel = rect.left - hostRect.left + cursorX;
      const flipX = xRel + 12 > hostRect.width * 0.6;
      this.tooltip = {
        x: flipX ? xRel - 12 : xRel + 12,
        y: rect.top - hostRect.top + cursorY - 28,
        flipX,
        html: `<div class="graph-tooltip-inner">${lines.join('<br>')}</div>`
      };
    },
    escapeHtml (s) {
      return String(s).replace(/[&<>"']/g, (c) => ({
        '&': '&amp;', '<': '&lt;', '>': '&gt;', '"': '&quot;', "'": '&#39;'
      }[c]));
    },
    resize () {
      if (!this.$refs.host) return;
      // Full rebuild rather than plot.setSize: width change also affects how
      // many buckets we aggregate (maxBars depends on drawable px), so the
      // bar count needs to recompute when the container resizes.
      this.rebuild();
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
    /* Public API for parent: pan/zoom controls drive the plot via $ref. */
    panLeft (frac = 0.1) {
      if (!this.plot) return;
      const { min, max } = this.plot.scales.x;
      const delta = (max - min) * frac;
      this.plot.setScale('x', { min: min - delta, max: max - delta });
    },
    panRight (frac = 0.1) {
      if (!this.plot) return;
      const { min, max } = this.plot.scales.x;
      const delta = (max - min) * frac;
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
    },
    /** Read current x scale (in seconds). Returns null until the plot mounts. */
    getXRange () {
      if (!this.plot) return null;
      const { min, max } = this.plot.scales.x;
      return { startTime: min.toFixed(), stopTime: max.toFixed() };
    }
  }
};
</script>

<style scoped>
.timeline-graph {
  position: relative;
  width: 100%;
  max-width: 100%;
  /* Controls now live in their own toolbar panel above. Tight top
     padding here, generous bottom margin to separate from the
     sessions table. Right padding keeps the rightmost bar off the
     panel edge. */
  padding: 6px 22px 0 4px;
  margin-bottom: 14px;
  /* Recessed panel: gradient is darker at the top (light hitting the
     rim casts a shadow into the well) and fades to lighter near the
     bottom. Layered inset shadows on all four sides reinforce the
     "sunken into the page" feel. */
  background: linear-gradient(
    180deg,
    rgba(0, 0, 0, 0.10) 0%,
    rgba(0, 0, 0, 0.05) 35%,
    rgba(0, 0, 0, 0.02) 100%
  );
  border-radius: 6px;
  box-shadow:
    inset 0 3px 8px rgba(0, 0, 0, 0.18),
    inset 0 1px 0 rgba(0, 0, 0, 0.22),
    inset 1px 0 2px rgba(0, 0, 0, 0.10),
    inset -1px 0 2px rgba(0, 0, 0, 0.10),
    inset 0 -1px 0 rgba(255, 255, 255, 0.06);
  /* Hard guard against horizontal overflow — kept after the table-clipping
     fix so chart-side overflow (long tooltips, etc.) can't propagate up. */
  overflow: hidden;
  box-sizing: border-box;
}
/* dashboard timeline widget: fill the card instead of the fixed 180px host */
.timeline-graph--fit {
  height: 100%;
  margin-bottom: 0;
}
.timeline-graph--fit .timeline-host {
  height: 100%;
}
.timeline-host {
  width: 100%;
  max-width: 100%;
  height: 180px;
  overflow: hidden;
  box-sizing: border-box;
  /* On mount and on rebuild, fade + grow the canvas in. The animation
     re-fires on each rebuild (uPlot is destroyed + recreated), giving the
     chart a "fresh data" feel on every refresh. */
  animation: timeline-grow 220ms ease-out;
}
@keyframes timeline-grow {
  from { opacity: 0; transform: scaleY(0.92); transform-origin: bottom; }
  to { opacity: 1; transform: scaleY(1); transform-origin: bottom; }
}
/* Selection range glow: when the user drags to select a time window,
   give the highlighted region a neon-yellow outline + soft glow instead
   of uPlot's default flat tint. */
.timeline-graph :deep(.u-select) {
  background: rgba(255, 216, 0, 0.10) !important;
  border: 1px solid rgba(255, 216, 0, 0.7) !important;
  box-shadow: 0 0 12px rgba(255, 216, 0, 0.35), inset 0 0 8px rgba(255, 216, 0, 0.15) !important;
}
/* uPlot renders its wrapper as inline-block; force it to behave like a
   block-level element bound to host width so it can't extend past it. */
.timeline-graph :deep(.uplot) {
  display: block;
  max-width: 100%;
}
.timeline-tooltip {
  position: absolute;
  pointer-events: none;
  z-index: 5;
  background: rgba(0, 0, 0, 0.88);
  color: #fff;
  padding: 3px 6px;
  font-size: 10px;
  line-height: 1.35;
  border-radius: 3px;
  white-space: nowrap;
}
/* When the cursor is past the chart's mid-right, flip the tooltip so it
   grows to the LEFT of the cursor instead of to the right. */
.timeline-tooltip.flip-x {
  transform: translateX(-100%) translateX(-24px);
}
.timeline-tooltip :deep(.graph-tooltip-inner) {
  line-height: 1.4;
}
</style>
