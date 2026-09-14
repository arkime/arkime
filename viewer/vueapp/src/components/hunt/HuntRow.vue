<!--
Copyright Yahoo Inc.
SPDX-License-Identifier: Apache-2.0
-->
<template>
  <tr class="align-middle">
    <td>
      <toggle-btn
        v-if="canView"
        :opened="job.expanded"
        @toggle="$emit('toggle', job)" />
    </td>
    <td class="no-wrap">
      <hunt-status
        :id="job.id"
        :status="job.status"
        :queue-count="job.queueCount"
        :hide-text="true" />
      &nbsp;
      <v-chip
        size="x-small"
        variant="flat"
        color="grey"
        class="cursor-help percent-done-badge"
        v-if="job.failedSessionIds && job.failedSessionIds.length"
        :id="`jobmatches${job.id}`">
        {{ round((((job.searchedSessions - job.failedSessionIds.length) / job.totalSessions) * 100), 1) }}%
      </v-chip>
      <v-chip
        v-else
        size="x-small"
        variant="flat"
        color="grey"
        class="cursor-help percent-done-badge"
        :id="`jobmatches${job.id}`">
        {{ round(((job.searchedSessions / job.totalSessions) * 100), 1) }}%
        <v-tooltip
          v-if="job.failedSessionIds && job.failedSessionIds.length"
          :activator="`[id='jobmatches${job.id}']`">
          <span
            v-html="$t('hunts.row-foundHtml', {
              matched: commaString(job.matchedSessions),
              total: commaString(job.searchedSessions - job.failedSessionIds.length)
            })" />
          <div v-if="job.status !== 'finished'">
            <span
              v-html="$t('hunts.row-stillHtml', {
                remaining: commaString(job.totalSessions - job.searchedSessions + job.failedSessionIds.length)
              })" />
          </div>
        </v-tooltip>
        <v-tooltip
          v-else
          :activator="`[id='jobmatches${job.id}']`">
          <span
            v-html="$t('hunts.row-foundHtml', {
              matched: commaString(job.matchedSessions),
              total: commaString(job.searchedSessions)
            })" />
          <div v-if="job.status !== 'finished'">
            <span
              v-html="$t('hunts.row-stillHtml', {
                remaining: commaString(job.totalSessions - job.searchedSessions)
              })" />
          </div>
        </v-tooltip>
      </v-chip>
      <template v-if="job.errors && job.errors.length">
        <v-chip
          :id="`joberrors${job.id}`"
          size="x-small"
          variant="flat"
          color="error"
          class="cursor-help ms-1">
          <v-icon icon="mdi-alert" />
          <v-tooltip :activator="`[id='joberrors${job.id}']`">
            {{ $t('hunts.hadErrorsTip') }}
          </v-tooltip>
        </v-chip>
      </template>
    </td>
    <td>
      {{ commaString(job.matchedSessions) }}
      <template v-if="job.removed">
        <span :id="`removed${job.id}`">
          <v-icon
            icon="mdi-information"
            class="cursor-help text-warning" />
          <v-tooltip :activator="`[id='removed${job.id}']`">
            {{ $t('hunts.huntRemovedTip') }}
          </v-tooltip>
        </span>
      </template>
    </td>
    <td class="word-break">
      {{ job.name }}
    </td>
    <td>
      {{ job.userId }}
    </td>
    <td class="word-break">
      <span v-if="canView">
        {{ job.search }} ({{ job.searchType }})
      </span>
    </td>
    <td>
      {{ timezoneDateString(job.created * 1000, user.settings.timezone, false) }}
    </td>
    <td>
      <span v-if="canView">
        {{ job.id }}
      </span>
    </td>
    <td class="no-wrap text-end">
      <template v-if="canEdit">
        <v-btn
          :id="`removejob${job.id}`"
          @click="$emit('removeJob', job, arrayName)"
          :disabled="job.loading"
          icon
          color="error"
          variant="flat"
          size="small"
          density="comfortable"
          :aria-label="$t('hunts.removeHuntTip')"
          class="ms-1">
          <v-icon
            icon="mdi-trash-can-outline"
            v-if="!job.loading" />
          <v-icon
            icon="mdi-loading"
            class="mdi-spin"
            v-else />
          <v-tooltip :activator="`[id='removejob${job.id}']`">
            {{ $t('hunts.removeHuntTip') }}
          </v-tooltip>
        </v-btn>
      </template>
      <v-btn
        :id="`remove${job.id}`"
        @click="$emit('removeFromSessions', job)"
        :aria-label="$t('common.remove')"
        icon
        color="error"
        variant="flat"
        size="small"
        density="comfortable"
        class="ms-1"
        v-if="canEdit && canRemoveFromSessions"
        :disabled="job.loading || !job.matchedSessions || job.removed || !user.removeEnabled">
        <v-icon
          icon="mdi-close"
          v-if="!job.loading" />
        <v-icon
          icon="mdi-loading"
          class="mdi-spin"
          v-else />
        <v-tooltip
          v-if="job.matchedSessions && !job.removed && user.removeEnabled"
          :activator="`[id='remove${job.id}']`">
          <span v-html="$t('hunts.removeFromSessionsTipHtml')" />
        </v-tooltip>
      </v-btn>
      <template v-if="canView">
        <v-btn
          @click="$emit('openSessions', job)"
          :disabled="!job.matchedSessions || job.removed"
          :id="`openresults${job.id}`"
          :aria-label="$t('common.open')"
          icon
          variant="flat"
          size="small"
          density="comfortable"
          :style="primaryBtnStyle"
          class="ms-1">
          <v-icon icon="mdi-folder-open" />
          <v-tooltip
            v-if="job.matchedSessions && !job.removed"
            :activator="`[id='openresults${job.id}']`">
            <span v-html="$t('hunts.openSessionsTipHtml')" />
          </v-tooltip>
        </v-btn>
      </template>
      <template v-if="canRerun && !job.unrunnable && (canView)">
        <v-btn
          :id="`rerun${job.id}`"
          @click="$emit('rerunJob', job)"
          :aria-label="$t('hunts.rerunTip')"
          icon
          variant="flat"
          size="small"
          density="comfortable"
          :style="secondaryBtnStyle"
          class="ms-1">
          <v-icon icon="mdi-refresh" />
          <v-tooltip :activator="`[id='rerun${job.id}']`">
            {{ $t('hunts.rerunTip') }}
          </v-tooltip>
        </v-btn>
      </template>
      <template v-if="canRepeat && !job.unrunnable && canEdit">
        <v-btn
          :id="`repeat${job.id}`"
          @click="$emit('repeatJob', job)"
          :aria-label="$t('hunts.repeatTip')"
          icon
          variant="flat"
          size="small"
          density="comfortable"
          :style="tertiaryBtnStyle"
          class="ms-1">
          <v-icon icon="mdi-repeat" />
          <v-tooltip :activator="`[id='repeat${job.id}']`">
            {{ $t('hunts.repeatTip') }}
          </v-tooltip>
        </v-btn>
      </template>
      <template v-if="canCancel && canEdit">
        <v-btn
          :id="`cancel${job.id}`"
          @click="$emit('cancelJob', job)"
          :disabled="job.loading"
          :aria-label="$t('hunts.cancelTip')"
          icon
          color="error"
          variant="flat"
          size="small"
          density="comfortable"
          class="ms-1">
          <v-icon
            icon="mdi-cancel"
            v-if="!job.loading" />
          <v-icon
            icon="mdi-loading"
            class="mdi-spin"
            v-else />
          <v-tooltip :activator="`[id='cancel${job.id}']`">
            {{ $t('hunts.cancelTip') }}
          </v-tooltip>
        </v-btn>
      </template>
      <template v-if="(job.status === 'running' || job.status === 'queued') && canEdit">
        <v-btn
          :id="`pause${job.id}`"
          :disabled="job.loading"
          @click="$emit('pauseJob', job)"
          :aria-label="$t('hunts.pauseTip')"
          icon
          color="warning"
          variant="flat"
          size="small"
          density="comfortable"
          class="ms-1">
          <v-icon
            icon="mdi-pause"
            v-if="!job.loading" />
          <v-icon
            icon="mdi-loading"
            class="mdi-spin"
            v-else />
          <v-tooltip :activator="`[id='pause${job.id}']`">
            {{ $t('hunts.pauseTip') }}
          </v-tooltip>
        </v-btn>
      </template>
      <template v-else-if="job.status === 'paused' && canEdit">
        <v-btn
          :id="`resume${job.id}`"
          :disabled="job.loading"
          @click="$emit('playJob', job)"
          :aria-label="$t('hunts.resumeTip')"
          icon
          variant="flat"
          size="small"
          density="comfortable"
          :style="secondaryBtnStyle"
          class="ms-1">
          <v-icon
            icon="mdi-play"
            v-if="!job.loading" />
          <v-icon
            icon="mdi-loading"
            class="mdi-spin"
            v-else />
          <v-tooltip :activator="`[id='resume${job.id}']`">
            {{ $t('hunts.resumeTip') }}
          </v-tooltip>
        </v-btn>
      </template>
    </td>
  </tr>
</template>

<script>
import ToggleBtn from '@common/ToggleBtn.vue';
import HuntStatus from './HuntStatus.vue';
import HuntService from './HuntService';
import { round, commaString, timezoneDateString } from '@common/vueFilters.js';

export default {
  name: 'HuntRow',
  emits: ['toggle', 'removeJob', 'removeFromSessions', 'openSessions', 'rerunJob', 'repeatJob', 'cancelJob', 'pauseJob', 'playJob'],
  props: {
    job: {
      type: Object,
      default: () => ({})
    },
    user: {
      type: Object,
      default: () => ({})
    },
    arrayName: {
      type: String,
      default: ''
    },
    canRerun: Boolean,
    canRepeat: Boolean,
    canCancel: Boolean,
    notifierName: {
      type: String,
      default: ''
    },
    canRemoveFromSessions: Boolean
  },
  components: {
    ToggleBtn,
    HuntStatus
  },
  data () {
    return {
      // Arkime theme-color v-btn styles. Vuetify :color can't take CSS vars.
      primaryBtnStyle: {
        backgroundColor: 'rgb(var(--v-theme-primary))',
        color: 'rgb(var(--v-theme-button-fg))'
      },
      secondaryBtnStyle: {
        backgroundColor: 'rgb(var(--v-theme-secondary))',
        color: 'rgb(var(--v-theme-button-fg))'
      },
      tertiaryBtnStyle: {
        backgroundColor: 'rgb(var(--v-theme-tertiary))',
        color: 'rgb(var(--v-theme-button-fg))'
      }
    };
  },
  computed: {
    canEdit () {
      return HuntService.canEditHunt(this.user, this.job);
    },
    canView () {
      return HuntService.canViewHunt(this.user, this.job);
    }
  },
  methods: {
    round,
    commaString,
    timezoneDateString
  }
};
</script>

<style scoped>
.percent-done-badge {
  width: 50px;
}
</style>
