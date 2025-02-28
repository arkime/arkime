<!--
Copyright Yahoo Inc.
SPDX-License-Identifier: Apache-2.0
-->
<template>
  <span>
    <template v-if="status === 'running'">
      <span
        ref="running"
        class="fa fa-play-circle fa-fw cursor-help">
        <BTooltip :target="getTarget('running')">
          Running (just started)
        </BTooltip>
      </span>
    </template>
    <template v-else-if="status === 'paused'">
      <span
        ref="paused"
        class="fa fa-pause fa-fw cursor-help">
        <BTooltip :target="getTarget('paused')">
          Paused
        </BTooltip>
      </span>
    </template>
    <template v-else-if="status === 'queued'">
      <span
        ref="queued"
        class="fa fa-clock-o fa-fw cursor-help">
        <BTooltip :target="getTarget('queued')">
          Queued (#{{ queueCount }} in the queue)
        </BTooltip>
      </span>
    </template>
    <template v-else-if="status === 'finished'">
      <span
        ref="finished"
        class="fa fa-check fa-fw cursor-help">
        <BTooltip :target="getTarget('finished')">
          <span v-if="!hideText">This hunt is
            <strong>{{ status }}</strong>
          </span>
        </BTooltip>
      </span>
    </template>
  </span>
</template>

<script>
export default {
  name: 'HuntStatus',
  props: {
    status: String,
    hideText: Boolean,
    queueCount: Number
  },
  methods: {
    getTarget (ref) {
      return this.$refs[ref];
    }
  }
};
</script>
