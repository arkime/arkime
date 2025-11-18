<template>
  <div class="chart-wrapper">
    <div
      ref="chartContainer"
      class="chart chart-pie" />
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
    default: 'sessions',
    validator: (value) => ['sessions', 'packets', 'bytes'].includes(value)
  }
});

const emit = defineEmits(['show-tooltip']);

const chartContainer = ref(null);
let d3 = null;

// Label styling constants
const LABEL_FONT_SIZE = '11px';
const LABEL_RADIUS = 45;

const showTooltip = (data, evt, percentage) => {
  emit('show-tooltip', {
    data,
    position: {
      x: evt.clientX + 1,
      y: evt.clientY + 1
    },
    percentage,
    fieldConfig: props.fieldConfig,
    metricType: props.metricType
  });
};
const LABEL_MAX_LENGTH = 12;
const LABEL_TRUNCATE_LENGTH = 10;
const MIN_SLICE_PERCENTAGE = 0.02; // Hide labels for slices < 2%

const createChartHoverHandlers = () => {
  return {
    mouseover: function (e, d) {
      d3.select(this).style('opacity', 0.7);
      // Calculate percentage for this slice
      const percentage = (d.endAngle - d.startAngle) / (2 * Math.PI) * 100;
      showTooltip(d.data, e, percentage);
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

  const radius = Math.min(props.width, props.height) / 2;

  const svg = container.append('svg')
    .attr('id', props.svgId)
    .attr('width', props.width)
    .attr('height', props.height)
    .append('g')
    .attr('transform', `translate(${props.width / 2},${props.height / 2})`);

  // Get the D3 color scheme
  const colorSchemeFunc = d3[props.colorScheme];
  const color = d3.scaleOrdinal(colorSchemeFunc);

  const pie = d3.pie()
    .value(d => d[props.metricType])
    .sort(null);

  const arc = d3.arc()
    .innerRadius(radius * 0.5) // Donut chart with 50% inner radius
    .outerRadius(radius - 10);

  const labelArc = d3.arc()
    .innerRadius(radius - LABEL_RADIUS)
    .outerRadius(radius - LABEL_RADIUS);

  const arcs = svg.selectAll('.arc')
    .data(pie(props.data))
    .enter()
    .append('g')
    .attr('class', 'arc');

  const handlers = createChartHoverHandlers();

  // Draw pie slices
  arcs.append('path')
    .attr('d', arc)
    .attr('fill', (d, i) => color(i))
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
    .style('font-weight', LABEL_FONT_SIZE === '12px' ? 'bold' : 'normal')
    .text(d => {
      // Calculate percentage of this slice
      const percentage = (d.endAngle - d.startAngle) / (2 * Math.PI);

      // Hide label if slice is less than 2%
      if (percentage < MIN_SLICE_PERCENTAGE) {
        return '';
      }

      const itemName = d.data.item;
      // Only truncate for tags (smaller font size)
      if (LABEL_FONT_SIZE === '10px' && itemName.length > LABEL_MAX_LENGTH) {
        return itemName.substring(0, LABEL_TRUNCATE_LENGTH) + '...';
      }
      return itemName;
    });
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
  display: flex;
  justify-content: center;
  align-items: center;
}

.chart-pie {
  min-height: 400px;
}
</style>
