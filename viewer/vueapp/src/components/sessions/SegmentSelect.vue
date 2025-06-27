<!--
Copyright Yahoo Inc.
SPDX-License-Identifier: Apache-2.0
-->
<template>
  <div class="col-md-4">
    <div class="input-group input-group-sm">
      <span class="input-group-text">
        Include
      </span>
      <select
        role="listbox"
        :value="segments"
        class="form-control"
        style="-webkit-appearance:none;"
        @input="$emit('update:segments', $event.target.value)">
        <option value="no">no</option>
        <option value="all">all</option>
        <option value="time">same time period</option>
      </select>
      <span class="input-group-text">
        linked segments (slow)
      </span>
    </div>
    <slot/>
  </div>
</template>

<script>
export default {
  name: 'SegmentSelect',
  props: ['segments']
};
</script>
