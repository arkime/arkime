<script setup>
import {reactive} from 'vue';
const state = reactive({count:0});
function increment () {
  state.count++;
}
</script>

<template>
  <div>
    <p>Times clicked: {{ state.count }}</p>
    <button @click="increment">increment</button>

    <div v-for="n of [...(Array(count).fill(0).map((_, i) => i))]" :key="n">
      <span>what {{n}}</span>
    </div>
  </div>
</template>
