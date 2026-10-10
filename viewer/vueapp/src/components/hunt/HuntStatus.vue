<!--
Copyright Yahoo Inc.
SPDX-License-Identifier: Apache-2.0
-->
<template>
  <span>
    <template v-if="status === 'running'">
      <v-icon
        :id="`hunt-status-running-${id}`"
        icon="mdi-play-circle"
        class="cursor-help">
        <v-tooltip :activator="`[id='hunt-status-running-${id}']`">
          {{ $t('hunts.status.runningTip') }}
        </v-tooltip>
      </v-icon>
    </template>
    <template v-else-if="status === 'paused'">
      <v-icon
        :id="`hunt-status-paused-${id}`"
        icon="mdi-pause"
        class="cursor-help">
        <v-tooltip :activator="`[id='hunt-status-paused-${id}']`">
          {{ $t('hunts.status.pausedTip') }}
        </v-tooltip>
      </v-icon>
    </template>
    <template v-else-if="status === 'queued'">
      <v-icon
        :id="`hunt-status-queued-${id}`"
        icon="mdi-clock-outline"
        class="cursor-help">
        <v-tooltip :activator="`[id='hunt-status-queued-${id}']`">
          {{ $t('hunts.status.queuedTip', {count: queueCount}) }}
        </v-tooltip>
      </v-icon>
    </template>
    <template v-else-if="status === 'finished'">
      <v-icon
        :id="`hunt-status-finished-${id}`"
        icon="mdi-check"
        class="cursor-help">
        <v-tooltip :activator="`[id='hunt-status-finished-${id}']`">
          {{ $t('hunts.status.finishedTip') }}
        </v-tooltip>
      </v-icon>
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
