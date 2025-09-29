<!--
Copyright Yahoo Inc.
SPDX-License-Identifier: Apache-2.0
-->
<template>
  <div class="input-group input-group-sm">
    <select
      role="listbox"
      :value="segments"
      class="form-control"
      style="-webkit-appearance:none;"
      @input="$emit('update:segments', $event.target.value)">
      <option value="no">{{ $t('sessions.segmentSelect.no') }}</option>
      <option value="time">{{ $t('sessions.segmentSelect.time') }}</option>
      <option value="all">{{ $t('sessions.segmentSelect.all') }}</option>
    </select>
  </div>
  <slot/>
</template>

<script>
export default {
  name: 'SegmentSelect',
  props: ['segments'],
  emits: ['update:segments']
};
</script>
