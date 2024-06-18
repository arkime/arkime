<!--
Copyright Yahoo Inc.
SPDX-License-Identifier: Apache-2.0
-->
<script setup>
import { defineProps, ref, nextTick, onUpdated } from 'vue';
// import { useStore } from 'vuex';
import HighlightableText from '@/utils/HighlightableText.vue';

const props = defineProps({
  field: { // the field for which to display the array value
    type: Object,
    required: true
  },
  arrayData: { // the data to display the array
    type: Array,
    require: true
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
  }
});
// const store = useStore();
const store = {};
const arrayLen = ref(Math.min(props.arrayData.length, props.size));

function showMore () {
  arrayLen.value = Math.min(arrayLen.value + props.size, props.arrayData.length);
};
function showLess () {
  arrayLen.value = Math.max(arrayLen.value - props.size, props.size);
};
function showAll () {
  store.commit('SET_RENDERING_ARRAY', true);
  setTimeout(() => { // need settimeout for rendering to take effect
    arrayLen.value = props.arrayData.length;
  }, 100);
};
// toby TODO: should this be on-mounted?
onUpdated(() => { // data is rendered
  nextTick(() => {
    store.commit('SET_RENDERING_ARRAY', false);
  });
});
</script>

<template>
  <span>
    <template v-if="field.join">
      {{ arrayData.join(field.join || ', ') }}
    </template>
    <template v-else>
      <div :key="index"
        v-for="index in (Math.max(arrayLen, 0))">
        <highlightable-text :content="arrayData[index - 1]" :highlights="highlightsArray ? highlightsArray[index - 1] : null"/>
      </div>
      <div class="d-flex justify-content-between"
        v-if="arrayData.length > arrayLen || arrayLen > size">
        <a
          @click="showLess"
          class="btn btn-link btn-xs"
          :class="{'disabled':arrayLen <= size}">
          show less...
        </a>
        <a
          @click="showAll"
          class="btn btn-link btn-xs"
          :class="{'disabled':arrayLen >= arrayData.length}">
          show ALL
        </a>
        <a
          @click="showMore"
          class="btn btn-link btn-xs"
          :class="{'disabled':arrayLen >= arrayData.length}">
          show more...
        </a>
      </div>
    </template>
  </span>
</template>
