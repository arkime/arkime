<template>
  <div class="col-md-4">
    <div class="input-group input-group-sm">
      <div class="input-group-prepend">
        <span class="input-group-text">
          Include
        </span>
      </div>
      <select :value="segments"
        @input="$emit('update:segments', $event.target.value)"
        class="form-control"
        style="-webkit-appearance:none;">
        <option value="no">no</option>
        <option value="all">all</option>
        <option value="time">same time period</option>
      </select>
      <div class="input-group-append">
        <span class="input-group-text">
          linked segments (slow)
        </span>
      </div>
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
