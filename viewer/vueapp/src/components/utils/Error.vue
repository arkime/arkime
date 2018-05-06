<template>

  <div class="info-area vertical-center">
    <div class="text-danger">
      <span class="fa fa-2x fa-warning"></span>
      <span v-if="message">{{ message }}</span>
    </div>
  </div>

</template>

<script>
export default {
  name: 'MolochError',
  props: [ 'message' ]
};
</script>

<style scoped>
.info-area > .text-danger {
  white-space: pre-line;
  line-height: 1.1em;
  font-size: var(--px-xxlg);
}
</style>
