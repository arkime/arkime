<!--
Copyright Yahoo Inc.
SPDX-License-Identifier: Apache-2.0
-->
<template>
  <v-select
    :model-value="segments"
    :items="items"
    item-title="title"
    item-value="value"
    density="compact"
    variant="outlined"
    hide-details
    class="segment-select"
    @update:model-value="$emit('update:segments', $event)" />
  <slot />
</template>

<script>
export default {
  name: 'SegmentSelect',
  props: {
    segments: {
      type: String,
      default: 'no'
    }
  },
  emits: ['update:segments'],
  computed: {
    items () {
      return [
        { value: 'no', title: this.$t('sessions.segmentSelect.no') },
        { value: 'time', title: this.$t('sessions.segmentSelect.time') },
        { value: 'all', title: this.$t('sessions.segmentSelect.all') }
      ];
    }
  }
};
</script>

<style scoped>
.segment-select {
  width: 200px;
  flex: 0 0 auto;
}
</style>
