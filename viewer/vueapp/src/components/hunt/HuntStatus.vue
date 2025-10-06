<!--
Copyright Yahoo Inc.
SPDX-License-Identifier: Apache-2.0
-->
<template>
  <span>
    <template v-if="status === 'running'">
      <span
        ref="running"
        class="fa fa-play-circle fa-fw cursor-help"
      >
        <BTooltip :target="getTarget('running')">
          {{ $t('hunts.status.runningTip') }}
        </BTooltip>
      </span>
    </template>
    <template v-else-if="status === 'paused'">
      <span
        ref="paused"
        class="fa fa-pause fa-fw cursor-help"
      >
        <BTooltip :target="getTarget('paused')">
          {{ $t('hunts.status.pausedTip') }}
        </BTooltip>
      </span>
    </template>
    <template v-else-if="status === 'queued'">
      <span
        ref="queued"
        class="fa fa-clock-o fa-fw cursor-help"
      >
        <BTooltip :target="getTarget('queued')">
          {{ $t('hunts.status.queuedTip', {count: queueCount}) }}
        </BTooltip>
      </span>
    </template>
    <template v-else-if="status === 'finished'">
      <span
        ref="finished"
        class="fa fa-check fa-fw cursor-help"
      >
        <BTooltip :target="getTarget('finished')">
          {{ $t('hunts.status.finishedTip') }}
        </BTooltip>
      </span>
    </template>
  </span>
  <span
    class="ps-2"
    v-if="!hideText"
  >{{ $t(`hunts.status.${status}`) }}</span>
</template>

<script>
export default {
  name: 'HuntStatus',
  props: {
    status: {
      type: String,
      default: ''
    },
    hideText: Boolean,
    queueCount: {
      type: Number,
      default: 0
    }
  },
  methods: {
    getTarget (ref) {
      return this.$refs[ref];
    }
  }
};
</script>
