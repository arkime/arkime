<!--
Copyright Yahoo Inc.
SPDX-License-Identifier: Apache-2.0
-->
<template>
  <div class="chart-wrapper">
    <div
      ref="chartContainer"
      class="chart chart-pie" />
  </div>
</template>

<script setup>
import { ref, computed, watch, onMounted } from 'vue';
import { useI18n } from 'vue-i18n';
import { colorRange, OTHER_COLOR } from './widgets/chartColors';
import { metricIsAdditive } from './widgets/widgetData';

const props = defineProps({
  data: {
    type: Array,
    required: true
  },
  // the sessions outside the top N ({ sessions, value }), drawn as a last slice
  other: {
    type: Object,
    default: null
  },
  svgId: {
    type: String,
    required: true
  },
  fieldConfig: {
    type: Object,
    required: true
  },
  width: {
    type: Number,
    default: 400
  },
  height: {
    type: Number,
    default: 400
  },
  metricType: {
    type: String,
    default: 'sessions'
  },
  metricAgg: {
    type: String,
    default: 'sum'
  },
  colorScheme: {
    type: String,
    default: 'rainbow'
  }
});

const emit = defineEmits(['show-tooltip']);
const { t } = useI18n();

// the slices: the top N, then "Other" when it holds anything
const slices = computed(() => (props.other?.value > 0
  ? [...props.data, { item: t('sessions.summary.other'), other: true, sessions: props.other.sessions, value: props.other.value }]
  : props.data));

const chartContainer = ref(null);
let d3 = null;

// Label styling constants
const LABEL_FONT_SIZE = '11px';

const showTooltip = (data, evt, percentage) => {
  emit('show-tooltip', {
    data,
    position: {
      x: evt.clientX + 1,
      y: evt.clientY + 1
    },
    percentage,
    fieldConfig: props.fieldConfig,
    metricType: props.metricType,
    metricAgg: props.metricAgg
  });
};

const getSlicePercentage = (d) => {
  return (d.endAngle - d.startAngle) / (2 * Math.PI);
};

const MIN_SLICE_PERCENTAGE = 0.02; // Hide labels for slices < 2%

const createChartHoverHandlers = () => {
  return {
    mouseover: function (e, d) {
      d3.select(this).style('opacity', 0.7);
      // averages, extremes and unique counts have no meaningful share of a total
      const percentage = metricIsAdditive(props.metricType, props.metricAgg) ? getSlicePercentage(d) * 100 : null;
      showTooltip(d.data, e, percentage);
    },
    mouseleave: function () {
      d3.select(this).style('opacity', 1);
    }
  };
};

const renderChart = async () => {
  if (!chartContainer.value) return;
  if (!slices.value.length) return;

  // Load D3 if not already loaded
  if (!d3) {
    d3 = await import('d3');
  }

  const container = d3.select(chartContainer.value);
  container.selectAll('*').remove();

  const radius = Math.min(props.width, props.height) / 2;

  const svg = container.append('svg')
    .attr('id', props.svgId)
    .attr('width', props.width)
    .attr('height', props.height)
    .append('g')
    .attr('transform', `translate(${props.width / 2},${props.height / 2})`);

  // Dashboard palette (shared with the bar/treemap charts)
  const color = d3.scaleOrdinal(colorRange(d3, props.colorScheme, props.data.length));
  const fill = (d, i) => (d.data.other ? OTHER_COLOR : color(i));

  const pie = d3.pie()
    .value(d => d.value)
    .sort(null);

  const innerRadius = radius * 0.5; // Donut chart with 50% inner radius
  const outerRadius = radius - 10;
  const arc = d3.arc()
    .innerRadius(innerRadius)
    .outerRadius(outerRadius);

  // labels sit midway through the ring, so they stay on it at any size
  const labelRadius = (innerRadius + outerRadius) / 2;
  const labelArc = d3.arc()
    .innerRadius(labelRadius)
    .outerRadius(labelRadius);

  const arcs = svg.selectAll('.arc')
    .data(pie(slices.value))
    .enter()
    .append('g')
    .attr('class', 'arc');

  const handlers = createChartHoverHandlers();

  // Draw pie slices
  arcs.append('path')
    .attr('d', arc)
    .attr('fill', fill)
    .attr('stroke', 'white')
    .style('stroke-width', '2px')
    .style('cursor', 'pointer')
    .on('mouseover', handlers.mouseover)
    .on('mouseleave', handlers.mouseleave);

  // Add labels (only for slices >= 2%)
  arcs.append('text')
    .attr('transform', d => `translate(${labelArc.centroid(d)})`)
    .attr('text-anchor', 'middle')
    .style('font-size', LABEL_FONT_SIZE)
    .style('fill', 'white')
    .style('font-weight', 'normal')
    .text(d => {
      // Hide label if slice is less than 2%
      if (getSlicePercentage(d) < MIN_SLICE_PERCENTAGE) {
        return '';
      }

      return d.data.item;
    });
};

// Watch for data, metric type, or dimension changes
watch([slices, () => props.metricType, () => props.width, () => props.height, () => props.colorScheme], () => {
  renderChart();
}, { deep: true });

// Initial render
onMounted(() => {
  renderChart();
});
</script>

<style scoped>
.chart-wrapper {
  position: relative;
  width: 100%;
  height: 100%;
  flex: 1;
}

.chart {
  min-height: 200px;
  height: 100%;
  display: flex;
  justify-content: center;
  align-items: center;
}

.chart-pie {
  min-height: 300px;
}
</style>
