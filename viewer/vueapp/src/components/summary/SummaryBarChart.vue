<template>
  <div
    ref="chartContainer"
    class="chart" />
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
  width: {
    type: Number,
    default: 500
  },
  height: {
    type: Number,
    default: 350
  }
});

const emit = defineEmits(['show-popup']);

const chartContainer = ref(null);
let d3 = null;
let popupTimer = null;

const POPUP_DELAY = 400;
const MARGIN = { top: 20, right: 30, bottom: 120, left: 60 };

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

  const width = props.width - MARGIN.left - MARGIN.right;
  const height = props.height - MARGIN.top - MARGIN.bottom;

  const svg = container.append('svg')
    .attr('id', props.svgId)
    .attr('width', props.width)
    .attr('height', props.height)
    .append('g')
    .attr('transform', `translate(${MARGIN.left},${MARGIN.top})`);

  const x = d3.scaleBand()
    .range([0, width])
    .domain(props.data.map(d => d.item))
    .padding(0.2);

  const y = d3.scaleLinear()
    .domain([0, d3.max(props.data, d => d.sessions)])
    .range([height, 0]);

  // Get the D3 color scheme
  const colorSchemeFunc = d3[props.colorScheme];
  const colors = d3.scaleOrdinal(colorSchemeFunc);

  const handlers = createChartHoverHandlers((d) => {
    emit('show-popup', {
      data: d,
      fieldName: props.fieldName,
      fieldExp: props.fieldExp
    });
  });

  // Draw bars
  svg.selectAll('.bar')
    .data(props.data)
    .enter()
    .append('rect')
    .attr('class', 'bar')
    .attr('x', d => x(d.item))
    .attr('width', x.bandwidth())
    .attr('y', d => y(d.sessions))
    .attr('height', d => height - y(d.sessions))
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

  // Value labels
  svg.selectAll('.label')
    .data(props.data)
    .enter()
    .append('text')
    .attr('class', 'label')
    .attr('x', d => x(d.item) + x.bandwidth() / 2)
    .attr('y', d => y(d.sessions) - 5)
    .attr('text-anchor', 'middle')
    .style('font-size', '11px')
    .text(d => d.sessions);
};

// Watch for data changes
watch(() => props.data, async () => {
  await nextTick();
  renderChart();
}, { deep: true });

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
</style>
