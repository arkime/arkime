<!--
Copyright Yahoo Inc.
SPDX-License-Identifier: Apache-2.0
-->
<script setup>
import { defineProps, ref, nextTick, onUpdated } from 'vue';
import { useStore } from 'vuex';
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
const store = useStore();
const arrayLen = ref(Math.min(props.arrayData.length, props.size));

function showMore () {
  console.log('hi toby 1', arrayLen.value, props.size, props.arrayData.length);
  arrayLen.value = Math.min(arrayLen.value + props.size, props.arrayData.length);
}
function showLess () {
  console.log('hi toby 2');
  arrayLen.value = Math.max(arrayLen.value - props.size, props.size);
}
function showAll () {
  console.log('hi toby 1', arrayLen.value, props.size, props.arrayData.length);
  console.log('hi toby 3');
  store.commit('SET_RENDERING_ARRAY', true);
    arrayLen.value = props.arrayData.length;
  console.log('hi toby 1', arrayLen.value, props.size, props.arrayData.length);
  // setTimeout(() => { // need settimeout for rendering to take effect
  //   arrayLen.value = props.arrayData.length;
  // });
}

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
      <div class="d-flex justify-space-between"
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
