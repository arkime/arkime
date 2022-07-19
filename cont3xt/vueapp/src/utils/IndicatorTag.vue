<template>
  <span class="px-1 ml-1 rounded bg-danger tag">
    {{ value }}
  </span>
</template>

<script>
export default {
  name: 'IndicatorTag',
  props: {
    value: {
      type: String,
      required: true
    }
  }
};
</script>

<style scoped>

</style>
