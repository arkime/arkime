<template>
  <div
    ref="chartContainer"
    class="chart chart-pie" />
</template>

<script setup>
import { ref, watch, onMounted, nextTick } from 'vue';

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
  labelFontSize: {
    type: String,
    default: '12px'
  },
  labelRadius: {
    type: Number,
    default: 40
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

const emit = defineEmits(['show-popup']);

const chartContainer = ref(null);
let d3 = null;
let popupTimer = null;

const POPUP_DELAY = 400;
const LABEL_MAX_LENGTH = 12;
const LABEL_TRUNCATE_LENGTH = 10;
const MIN_SLICE_PERCENTAGE = 0.02; // Hide labels for slices < 2%

const createChartHoverHandlers = (showPopupCallback) => {
  return {
    mouseover: function (e, d) {
      d3.select(this).style('opacity', 0.7);
      if (popupTimer) clearTimeout(popupTimer);
      popupTimer = setTimeout(() => {
        showPopupCallback(d);
      }, POPUP_DELAY);
    },
    mouseleave: function () {
      d3.select(this).style('opacity', 1);
      if (popupTimer) clearTimeout(popupTimer);
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
    .innerRadius(radius - props.labelRadius)
    .outerRadius(radius - props.labelRadius);

  const arcs = svg.selectAll('.arc')
    .data(pie(props.data))
    .enter()
    .append('g')
    .attr('class', 'arc');

  const handlers = createChartHoverHandlers((d) => {
    emit('show-popup', {
      data: d.data,
      fieldName: props.fieldName,
      fieldExp: props.fieldExp
    });
  });

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
    .style('font-size', props.labelFontSize)
    .style('fill', 'white')
    .style('font-weight', props.labelFontSize === '12px' ? 'bold' : 'normal')
    .text(d => {
      // Calculate percentage of this slice
      const percentage = (d.endAngle - d.startAngle) / (2 * Math.PI);

      // Hide label if slice is less than 2%
      if (percentage < MIN_SLICE_PERCENTAGE) {
        return '';
      }

      const itemName = d.data.item;
      // Only truncate for tags (smaller font size)
      if (props.labelFontSize === '10px' && itemName.length > LABEL_MAX_LENGTH) {
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
