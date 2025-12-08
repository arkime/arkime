<!--
Copyright Yahoo Inc.
SPDX-License-Identifier: Apache-2.0
-->
<template>
  <div class="info-area vertical-horizontal-center">
    <div v-if="parseInt(recordsTotal) === 0">
      <span class="fa fa-3x text-muted-more fa-exclamation-triangle" />&nbsp;
      {{ $t('utils.arkimeEmpty') }}
    </div>

    <div v-else-if="!recordsTotal || recordsTotal > 0">
      <span class="fa fa-3x text-muted-more fa-folder-open" />&nbsp;
      {{ $t('utils.arkimeNoResults') }}
      <small
        v-if="view"
        class="text-theme-primary">
        <br>
        {{ $t('utils.arkimeEmptyView') }}:
        <strong>{{ viewName }}</strong>
      </small>
    </div>
  </div>
</template>

<script>
export default {
  name: 'ArkimeNoResults',
  props: {
    recordsTotal: {
      type: Number,
      default: undefined
    },
    view: {
      type: String,
      default: ''
    }
  },
  computed: {
    viewName () {
      const view = this.$store.state.views.find(v => v.id === this.view || v.name === this.view);
      return view?.name || this.$t('utils.unknownView');
    }
  }
};
</script>

<style scoped>
.info-area {
  font-size: var(--px-xxlg);
}
</style>
