<template>
  <div class="chart-wrapper">
    <div
      ref="chartContainer"
      class="chart" />
  </div>
</template>

<script setup>
import { ref, watch, onMounted, nextTick, computed } from 'vue';

const props = defineProps({
  data: {
    type: Array,
    required: true
  },
  svgId: {
    type: String,
    required: true
  },
  colorScheme: {
    type: String,
    default: 'schemeCategory10'
  },
  fieldName: {
    type: String,
    required: true
  },
  fieldExp: {
    type: String,
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

const fieldConfig = computed(() => ({
  friendlyName: props.fieldName,
  exp: props.fieldExp,
  dbField: props.fieldExp
}));

const showTooltip = (data, evt) => {
  emit('show-tooltip', {
    data,
    position: {
      x: evt.clientX + 1,
      y: evt.clientY + 1
    },
    fieldConfig: fieldConfig.value
  });
};
const MARGIN = { top: 20, right: 30, bottom: 120, left: 60 };
const MIN_BAR_WIDTH = 30; // Minimum width per bar for readability

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

  // Calculate width based on number of bars for better readability
  const calculatedWidth = Math.max(props.width, props.data.length * MIN_BAR_WIDTH + MARGIN.left + MARGIN.right);
  const width = calculatedWidth - MARGIN.left - MARGIN.right;
  const height = props.height - MARGIN.top - MARGIN.bottom;

  const svg = container.append('svg')
    .attr('id', props.svgId)
    .attr('width', calculatedWidth)
    .attr('height', props.height)
    .append('g')
    .attr('transform', `translate(${MARGIN.left},${MARGIN.top})`);

  const x = d3.scaleBand()
    .range([0, width])
    .domain(props.data.map(d => d.item))
    .padding(0.1); // Reduced padding for wider bars

  const y = d3.scaleLinear()
    .domain([0, d3.max(props.data, d => d[props.metricType])])
    .range([height, 0]);

  // Get the D3 color scheme
  const colorSchemeFunc = d3[props.colorScheme];
  const colors = d3.scaleOrdinal(colorSchemeFunc);

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

// Watch for data changes
watch(() => props.data, async () => {
  await nextTick();
  renderChart();
}, { deep: true });

// Watch for metric type changes
watch(() => props.metricType, async () => {
  await nextTick();
  renderChart();
});

// Initial render
onMounted(async () => {
  await nextTick();
  renderChart();
});
</script>

<style scoped>
.chart-wrapper {
  position: relative;
}

.chart {
  min-height: 300px;
  overflow-x: auto;
  overflow-y: hidden;
  width: 100%; /* Constrain to parent width */
}
</style>
