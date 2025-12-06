<template>
  <div class="chart-wrapper">
    <div
      ref="chartContainer"
      class="chart" />
  </div>
</template>

<script setup>
import { ref, watch, onMounted } from 'vue';

const props = defineProps({
  data: {
    type: Array,
    required: true
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
    default: 500
  },
  height: {
    type: Number,
    default: 350
  },
  metricType: {
    type: String,
    default: 'sessions',
    validator: (value) => ['sessions', 'packets', 'bytes'].includes(value)
  }
});

const emit = defineEmits(['show-tooltip']);

const chartContainer = ref(null);
let d3 = null;

const showTooltip = (data, evt) => {
  emit('show-tooltip', {
    data,
    position: {
      x: evt.clientX + 1,
      y: evt.clientY + 1
    },
    fieldConfig: props.fieldConfig,
    metricType: props.metricType
  });
};
const MARGIN = { top: 20, right: 30, bottom: 120, left: 60 };
const MAX_BAR_WIDTH = 80; // Maximum width per bar to prevent overly wide bars

const createChartHoverHandlers = () => {
  return {
    mouseover: function (e, d) {
      d3.select(this).style('opacity', 0.7);
      showTooltip(d, e);
    },
    mouseleave: function () {
      d3.select(this).style('opacity', 1);
    }
  };
};

const renderChart = async () => {
  if (!chartContainer.value) return;
  if (!props.data || props.data.length === 0) return;

  // Load D3 if not already loaded
  if (!d3) {
    d3 = await import('d3');
  }

  const container = d3.select(chartContainer.value);
  container.selectAll('*').remove();

  // Use props.width as the SVG width - never exceed it to prevent resize loops
  const svgWidth = props.width;
  const svgHeight = props.height;
  const height = svgHeight - MARGIN.top - MARGIN.bottom;
  const dataCount = props.data.length;

  // Calculate the inner width available for bars
  const availableInnerWidth = svgWidth - MARGIN.left - MARGIN.right;

  // Calculate natural bar width if we use full available space
  const naturalBarWidth = availableInnerWidth / dataCount * 0.9; // 0.9 accounts for padding

  // Determine actual inner width to use (may be less than available if bars would be too wide)
  let chartInnerWidth;
  if (naturalBarWidth > MAX_BAR_WIDTH) {
    // Too few bars - cap bar width, use less space (left-aligned)
    chartInnerWidth = dataCount * MAX_BAR_WIDTH / 0.9;
  } else {
    // Use full available width (bars will be natural size or clamped to min by scrolling)
    chartInnerWidth = availableInnerWidth;
  }

  const svg = container.append('svg')
    .attr('id', props.svgId)
    .attr('width', svgWidth)
    .attr('height', svgHeight)
    .append('g')
    .attr('transform', `translate(${MARGIN.left},${MARGIN.top})`);

  const x = d3.scaleBand()
    .range([0, chartInnerWidth])
    .domain(props.data.map(d => d.item))
    .padding(0.1);

  const y = d3.scaleLinear()
    .domain([0, d3.max(props.data, d => d[props.metricType])])
    .range([height, 0]);

  // Use D3 color scheme
  const colors = d3.scaleOrdinal(d3.schemeCategory10);

  const handlers = createChartHoverHandlers();

  // Draw bars
  svg.selectAll('.bar')
    .data(props.data)
    .enter()
    .append('rect')
    .attr('class', 'bar')
    .attr('x', d => x(d.item))
    .attr('width', x.bandwidth())
    .attr('y', d => y(d[props.metricType]))
    .attr('height', d => height - y(d[props.metricType]))
    .attr('fill', (d, i) => colors(i))
    .style('cursor', 'pointer')
    .on('mouseover', handlers.mouseover)
    .on('mouseleave', handlers.mouseleave);

  // X axis
  svg.append('g')
    .attr('transform', `translate(0,${height})`)
    .call(d3.axisBottom(x))
    .selectAll('text')
    .attr('transform', 'rotate(-45)')
    .style('text-anchor', 'end')
    .style('font-size', '10px');

  // Y axis
  svg.append('g')
    .call(d3.axisLeft(y));
};

// Watch for data, metric type, or dimension changes
watch([() => props.data, () => props.metricType, () => props.width, () => props.height], () => {
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
  flex: 1;
}

.chart {
  min-height: 200px;
  height: 100%;
  overflow-x: auto;
  overflow-y: hidden;
}
</style>
