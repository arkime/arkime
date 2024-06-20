<script setup>
import { ref, computed, watchEffect } from 'vue';
import TestTwo from '@/TestTwo.vue';
console.log('hi test one');

const it = ref(2);
function incr () {
  it.value++;
}
const yo = computed(() => {
  return [...Array(it.value).fill(0)].map((_, i) => i);
})

watchEffect(() => {
  console.log('toby check', it.value);
})
</script>

<template>
  <div class="what">
    Hello
    <test-two foo="no"/>
    <test-two foo="what"/>
    <button @click="incr">click</button>

    <div v-for="em of yo" :key="em">
      <span>hi {{ em }}</span>
    </div>
  </div>
</template>

<style scoped>
.what {
  background-color: red;
}
</style>
