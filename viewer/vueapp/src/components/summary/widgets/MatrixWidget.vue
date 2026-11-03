<!--
Copyright Yahoo Inc.
SPDX-License-Identifier: Apache-2.0

Matrix and mosaic widget: the first field's top values down the side and the
second's across the top, each cell colored by the sessions the two values
share (matrix), or the first field's values as columns as wide as their
sessions, each split by the second field's (mosaic), or one waffle of the
field's values per value of the split field (a split waffle). Self-fetches
/api/spigraphhierarchy (global search AND the widget's local filter) like the
intersection table.
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
      <SummaryWaffle
        v-if="hasData && width > 0 && widget.viewMode === 'waffle'"
        :groups="groups"
        :svg-id="`waffle-${widget.id}`"
        :field-config="fieldObjs[0]"
        :split-field-config="splitFieldObj"
        :width="width"
        :height="height"
        :color-scheme="colorScheme"
        @show-tooltip="$emit('show-tooltip', $event)" />
      <SummaryMosaic
        v-else-if="hasData && width > 0 && widget.viewMode === 'mosaic'"
        :data="hierarchy"
        :svg-id="`mosaic-${widget.id}`"
        :field-config="fieldObjs[0]"
        :col-field-config="fieldObjs[1] || null"
        :width="width"
        :height="height"
        :color-scheme="colorScheme"
        @show-tooltip="$emit('show-tooltip', $event)"
        @hide-tooltip="$emit('hide-tooltip')" />
      <SummaryMatrix
        v-else-if="hasData && width > 0"
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
import SummaryMosaic from '../SummaryMosaic.vue';
import SummaryWaffle from '../SummaryWaffle.vue';
import { waffleGroups } from '../../utils/waffleLayout';
import FieldService from '../../search/FieldService';
import { useSpigraphWidget } from './useSpigraphWidget';
import { fetchHierarchy, widgetFields, defaultWidgetTitle } from './widgetData';

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
// a split waffle's groups, built once per result
const groups = computed(() => (props.widget.viewMode === 'waffle' ? waffleGroups(hierarchy.value) : null));
const splitFieldObj = computed(() => (props.widget.splitField ? FieldService.getField(props.widget.splitField, true) : null));
// "A × B" for a matrix or mosaic; a split waffle is "Split / Field", as a split pie is
const title = computed(() => props.widget.title || defaultWidgetTitle(props.widget));
// a split waffle needs a split value with something to draw
const hasData = computed(() => !!fieldObjs.value[0] && (groups.value
  ? groups.value.some(g => g.items.some(i => i.value > 0) || g.other?.value > 0)
  : !!hierarchy.value?.children?.length));

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
