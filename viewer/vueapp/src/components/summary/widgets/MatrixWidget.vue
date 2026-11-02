<!--
Copyright Yahoo Inc.
SPDX-License-Identifier: Apache-2.0

Matrix widget: the first field's top values down the side, the second's
across the top, each cell colored by the sessions the two values share.
Self-fetches /api/spigraphhierarchy (global search AND the widget's local
filter) like the intersection table, and draws it as a colored grid.
-->
<template>
  <WidgetCard
    :title="title"
    :loading="loading"
    :error="error"
    :has-data="hasData"
    :info-items="infoItems"
    :link="link"
    :graph="graphDef"
    @edit="$emit('edit')"
    @remove="$emit('remove')"
    @retry="fetchData">
    <div
      ref="container"
      class="matrix-container">
      <SummaryMatrix
        v-if="hasData && width > 0"
        :data="hierarchy"
        :svg-id="`matrix-${widget.id}`"
        :field-config="fieldObjs[0]"
        :col-field-config="fieldObjs[1] || null"
        :width="width"
        :height="height"
        :color-scheme="colorScheme"
        @show-tooltip="$emit('show-tooltip', $event)"
        @hide-tooltip="$emit('hide-tooltip')" />
    </div>
  </WidgetCard>
</template>

<script setup>
import { ref, computed, nextTick, onBeforeUnmount } from 'vue';
import WidgetCard from './WidgetCard.vue';
import SummaryMatrix from '../SummaryMatrix.vue';
import FieldService from '../../search/FieldService';
import { useSpigraphWidget } from './useSpigraphWidget';
import { fetchHierarchy, widgetFields } from './widgetData';

const props = defineProps({
  widget: { type: Object, required: true },
  reloadNonce: { type: Number, default: 0 },
  colorScheme: { type: String, default: 'rainbow' },
  infoItems: { type: Array, default: () => [] },
  link: { type: Object, default: null },
  graphDef: { type: Object, default: null }
});

defineEmits(['edit', 'remove', 'show-tooltip', 'hide-tooltip']);

const container = ref(null);
const width = ref(0);
const height = ref(0);
const hierarchy = ref(null);

const { loading, error, fetchData } = useSpigraphWidget(
  () => props.widget,
  () => props.reloadNonce,
  (res) => { hierarchy.value = res.hierarchicalResults || null; setupObserver(); },
  fetchHierarchy
);

const fieldExps = computed(() => widgetFields(props.widget));
const fieldObjs = computed(() => fieldExps.value.map(exp => FieldService.getField(exp, true)));
const title = computed(() => props.widget.title ||
  fieldObjs.value.map((f, i) => f?.friendlyName || fieldExps.value[i]).join(' × '));
const hasData = computed(() => !!hierarchy.value?.children?.length && !!fieldObjs.value[0]);

const measure = () => {
  if (!container.value) { return; }
  width.value = container.value.clientWidth;
  height.value = container.value.clientHeight;
};
// the card only renders its content once the data is in, so the container
// to measure (and watch) exists after each result, not at mount
let ro;
const setupObserver = () => {
  nextTick(() => {
    if (!container.value) { return; }
    if (ro) { ro.disconnect(); }
    if (typeof ResizeObserver !== 'undefined') {
      ro = new ResizeObserver(measure);
      ro.observe(container.value);
    }
    measure();
  });
};
onBeforeUnmount(() => { if (ro) { ro.disconnect(); } });
</script>

<style scoped>
.matrix-container {
  position: relative;
  flex: 1;
  width: 100%;
  min-height: 0;
  overflow: hidden;
}
</style>
