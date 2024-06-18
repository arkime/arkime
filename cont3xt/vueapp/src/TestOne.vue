<script setup>
import TestTwo from '@/TestTwo.vue';
console.log('hi test one');
</script>

<template>
  <div class="what">
    Hello
    <test-two foo="no"/>
    <test-two foo="what"/>
  </div>
</template>

<style scoped>
.what {
  background-color: red;
}
</style>
