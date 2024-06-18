<script setup>
import { defineProps } from 'vue';
console.log('hi test two');
const props = defineProps({
  foo: String
});
</script>

<template>
  <div class="what">
    Goodbye {{ props.foo }}
  </div>
</template>

<style scoped>
.what {
  background-color: blue;
}
</style>
