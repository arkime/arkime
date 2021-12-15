<template>
  <span>
    <span v-if="status === 'running'"
      v-b-tooltip.hover.right
      title="Running (just started)"
      class="fa fa-play-circle fa-fw cursor-help">
    </span>
    <span v-else-if="status === 'paused'"
      v-b-tooltip.hover.right
      title="Paused"
      class="fa fa-pause fa-fw cursor-help">
    </span>
    <span v-else-if="status === 'queued'"
      v-b-tooltip.hover.right
      :title="`Queued (#${queueCount} in the queue)`"
      class="fa fa-clock-o fa-fw cursor-help">
    </span>
    <span v-else-if="status === 'finished'"
      v-b-tooltip.hover.right
      title="Finished"
      class="fa fa-check fa-fw cursor-help">
    </span>
    &nbsp;<span v-if="!hideText">This hunt is
      <strong>{{ status }}</strong>
    </span>
  </span>
</template>

<script>
export default {
  name: 'HuntStatus',
  props: {
    status: String,
    hideText: Boolean,
    queueCount: Number
  }
};
</script>
