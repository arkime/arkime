<!--
Copyright Yahoo Inc.
SPDX-License-Identifier: Apache-2.0
-->
<template>
  <span>
    <template v-if="status === 'running'">
      <span
        :id="`hunt-status-running-${id}`"
        class="fa fa-play-circle fa-fw cursor-help">
        <v-tooltip :activator="`[id='hunt-status-running-${id}']`">
          {{ $t('hunts.status.runningTip') }}
        </v-tooltip>
      </span>
    </template>
    <template v-else-if="status === 'paused'">
      <span
        :id="`hunt-status-paused-${id}`"
        class="fa fa-pause fa-fw cursor-help">
        <v-tooltip :activator="`[id='hunt-status-paused-${id}']`">
          {{ $t('hunts.status.pausedTip') }}
        </v-tooltip>
      </span>
    </template>
    <template v-else-if="status === 'queued'">
      <span
        :id="`hunt-status-queued-${id}`"
        class="fa fa-clock-o fa-fw cursor-help">
        <v-tooltip :activator="`[id='hunt-status-queued-${id}']`">
          {{ $t('hunts.status.queuedTip', {count: queueCount}) }}
        </v-tooltip>
      </span>
    </template>
    <template v-else-if="status === 'finished'">
      <span
        :id="`hunt-status-finished-${id}`"
        class="fa fa-check fa-fw cursor-help">
        <v-tooltip :activator="`[id='hunt-status-finished-${id}']`">
          {{ $t('hunts.status.finishedTip') }}
        </v-tooltip>
      </span>
    </template>
  </span>
  <span
    class="ps-2"
    v-if="!hideText">{{ $t(`hunts.status.${status}`) }}</span>
</template>

<script>
export default {
  name: 'HuntStatus',
  props: {
    id: {
      type: String,
      required: true
    },
    status: {
      type: String,
      default: ''
    },
    hideText: Boolean,
    queueCount: {
      type: Number,
      default: 0
    }
  }
};
</script>
