<template>

  <div class="info-area vertical-horizontal-center">

    <div v-if="recordsTotal === '0'">
      <span class="fa fa-3x text-muted-more fa-exclamation-triangle">
      </span>&nbsp;
      Oh no, Moloch is empty! There is no data to search.
    </div>

    <div v-else-if="!recordsTotal || recordsTotal > 0">
      <span class="fa fa-3x text-muted-more fa-folder-open">
      </span>&nbsp;
      No results or none that match your search within your time range.
      <small v-if="view"
        class="text-theme-primary">
        <br>
        Don't forget! You have a view applied to your search:
        <strong>{{ view }}</strong>
      </small>
    </div>

  </div>

</template>

<script>
export default {
  name: 'MolochNoResults',
  props: [ 'recordsTotal', 'view' ]
};
</script>

<style scoped>
.info-area {
  font-size: var(--px-xxlg);
}
</style>
