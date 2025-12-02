<!--
Copyright Yahoo Inc.
SPDX-License-Identifier: Apache-2.0
-->
<script setup>
import { ref, nextTick, onUpdated } from 'vue';
import { useStore } from 'vuex';
import { useGetters } from '@/vue3-helpers';
import HighlightableText from '@/utils/HighlightableText.vue';

const props = defineProps({
  field: { // the field for which to display the array value
    type: Object,
    required: true
  },
  arrayData: { // the data to display the array
    type: Array,
    require: true,
    default: () => []
  },
  size: { // the rows of data to display initially and increment or
    type: Number, // decrement thereafter (by clicking more/less)
    default: 50
  },
  highlightsArray: {
    type: Array,
    default () {
      return null;
    }
  },
  highlightColor: { // highlight color: 'yellow' or 'pink'
    type: String,
    default: 'yellow'
  }
});
const store = useStore();
const { getRenderingArray } = useGetters(store);

const arrayLen = ref(Math.min(props.arrayData.length, props.size));

function showMore () {
  arrayLen.value = Math.min(arrayLen.value + props.size, props.arrayData.length);
}
function showLess () {
  arrayLen.value = Math.max(arrayLen.value - props.size, props.size);
}
function showAll () {
  store.commit('SET_RENDERING_ARRAY', true);
  setTimeout(() => { // need settimeout for rendering to take effect
    arrayLen.value = props.arrayData.length;
  });
}

onUpdated(() => { // data is rendered
  nextTick(() => {
    store.commit('SET_RENDERING_ARRAY', false);
  });
});
</script>

<template>
  <span class="position-relative">
    <v-overlay
      :model-value="getRenderingArray"
      class="align-center justify-center blur-overlay"
      contained>
      <div class="d-flex flex-column align-center justify-center">
        <v-progress-circular
          color="info"
          size="64"
          indeterminate />
        <p>Rendering array...</p>
      </div>
    </v-overlay>

    <template v-if="field.join">
      {{ arrayData.join(field.join || ', ') }}
    </template>
    <template v-else>
      <div
        :key="index"
        v-for="index in (Math.max(arrayLen, 0))">
        <highlightable-text
          :content="arrayData[index - 1]"
          :highlights="highlightsArray ? highlightsArray[index - 1] : null"
          :highlight-color="highlightColor" />
      </div>
      <div
        class="d-flex justify-space-between"
        v-if="arrayData.length > arrayLen || arrayLen > size">
        <v-btn
          @click="showLess"
          size="x-small"
          variant="text"
          color="primary"
          :disabled="arrayLen <= size">
          show less...
        </v-btn>
        <v-btn
          @click="showAll"
          size="x-small"
          variant="text"
          color="primary"
          :disabled="arrayLen >= arrayData.length">
          show ALL
        </v-btn>
        <v-btn
          @click="showMore"
          size="x-small"
          variant="text"
          color="primary"
          :disabled="arrayLen >= arrayData.length">
          show more...
        </v-btn>
      </div>
    </template>
  </span>
</template>
