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
        :status="job.status"
        :queue-count="job.queueCount"
        :hide-text="true" />
      &nbsp;
      <span
        class="badge bg-secondary cursor-help percent-done-badge"
        v-if="job.failedSessionIds && job.failedSessionIds.length"
        :id="`jobmatches${job.id}`">
        {{ round((((job.searchedSessions - job.failedSessionIds.length) / job.totalSessions) * 100), 1) }}%
      </span>
      <span
        v-else
        class="badge bg-secondary cursor-help percent-done-badge"
        :id="`jobmatches${job.id}`">
        {{ round(((job.searchedSessions / job.totalSessions) * 100), 1) }}%
        <BTooltip
          v-if="job.failedSessionIds && job.failedSessionIds.length"
          :target="`jobmatches${job.id}`">
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
        </BTooltip>
        <BTooltip
          v-else
          :target="`jobmatches${job.id}`">
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
        </BTooltip>
      </span>
      <template v-if="job.errors && job.errors.length">
        <span
          :id="`joberrors${job.id}`"
          class="badge bg-danger cursor-help">
          <span class="fa fa-exclamation-triangle" />
          <BTooltip :target="`joberrors${job.id}`">
            {{ $t('hunts.hadErrorsTip') }}
          </BTooltip>
        </span>
      </template>
    </td>
    <td>
      {{ commaString(job.matchedSessions) }}
      <template v-if="job.removed">
        <span :id="`removed${job.id}`">
          <span class="fa fa-info-circle fa-fw cursor-help text-warning" />
          <BTooltip :target="`removed${job.id}`">
            {{ $t('hunts.huntRemovedTip') }}
          </BTooltip>
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
    <td class="no-wrap">
      <template v-if="canEdit">
        <button
          :id="`removejob${job.id}`"
          @click="$emit('removeJob', job, arrayName)"
          :disabled="job.loading"
          type="button"
          class="ms-1 pull-right btn btn-sm btn-danger">
          <span
            v-if="!job.loading"
            class="fa fa-trash-o fa-fw" />
          <span
            v-else
            class="fa fa-spinner fa-spin fa-fw" />
          <BTooltip :target="`removejob${job.id}`">
            {{ $t('hunts.removeHuntTip') }}
          </BTooltip>
        </button>
      </template>
      <button
        type="button"
        :id="`remove${job.id}`"
        @click="$emit('removeFromSessions', job)"
        class="ms-1 pull-right btn btn-sm btn-danger"
        v-if="canEdit && canRemoveFromSessions"
        :disabled="job.loading || !job.matchedSessions || job.removed || !user.removeEnabled">
        <span
          v-if="!job.loading"
          class="fa fa-times fa-fw" />
        <span
          v-else
          class="fa fa-spinner fa-spin fa-fw" />
        <BTooltip
          v-if="job.matchedSessions && !job.removed && user.removeEnabled"
          :target="`remove${job.id}`">
          <span v-html="$t('hunts.removeFromSessionsTipHtml')" />
        </BTooltip>
      </button>
      <span v-if="canView">
        <button
          type="button"
          @click="$emit('openSessions', job)"
          :disabled="!job.matchedSessions || job.removed"
          :id="`openresults${job.id}`"
          class="ms-1 pull-right btn btn-sm btn-theme-primary">
          <span class="fa fa-folder-open fa-fw" />
          <BTooltip
            v-if="job.matchedSessions && !job.removed"
            :target="`openresults${job.id}`">
            <span v-html="$t('hunts.openSessionsTipHtml')" />
          </BTooltip>
        </button>
      </span>
      <template v-if="canRerun && !job.unrunnable && (canView)">
        <button
          :id="`rerun${job.id}`"
          type="button"
          @click="$emit('rerunJob', job)"
          class="ms-1 pull-right btn btn-sm btn-theme-secondary">
          <span class="fa fa-refresh fa-fw" />
          <BTooltip :target="`rerun${job.id}`">
            {{ $t('hunts.rerunTip') }}
          </BTooltip>
        </button>
      </template>
      <template v-if="canRepeat && !job.unrunnable && canEdit">
        <button
          :id="`repeat${job.id}`"
          type="button"
          @click="$emit('repeatJob', job)"
          class="ms-1 pull-right btn btn-sm btn-theme-tertiary">
          <span class="fa fa-repeat fa-fw" />
          <BTooltip :target="`repeat${job.id}`">
            {{ $t('hunts.repeatTip') }}
          </BTooltip>
        </button>
      </template>
      <template v-if="canCancel && canEdit">
        <button
          :id="`cancel${job.id}`"
          @click="$emit('cancelJob', job)"
          :disabled="job.loading"
          type="button"
          class="ms-1 pull-right btn btn-sm btn-danger">
          <span
            v-if="!job.loading"
            class="fa fa-ban fa-fw" />
          <span
            v-else
            class="fa fa-spinner fa-spin fa-fw" />
          <BTooltip :target="`cancel${job.id}`">
            {{ $t('hunts.cancelTip') }}
          </BTooltip>
        </button>
      </template>
      <template v-if="(job.status === 'running' || job.status === 'queued') && canEdit">
        <button
          :id="`pause${job.id}`"
          :disabled="job.loading"
          @click="$emit('pauseJob', job)"
          type="button"
          class="ms-1 pull-right btn btn-sm btn-warning">
          <span
            v-if="!job.loading"
            class="fa fa-pause fa-fw" />
          <span
            v-else
            class="fa fa-spinner fa-spin fa-fw" />
          <BTooltip :target="`pause${job.id}`">
            {{ $t('hunts.pauseTip') }}
          </BTooltip>
        </button>
      </template>
      <template v-else-if="job.status === 'paused' && canEdit">
        <button
          :id="`resume${job.id}`"
          :disabled="job.loading"
          @click="$emit('playJob', job)"
          type="button"
          class="ms-1 pull-right btn btn-sm btn-theme-secondary">
          <span
            v-if="!job.loading"
            class="fa fa-play fa-fw" />
          <span
            v-else
            class="fa fa-spinner fa-spin fa-fw" />
          <BTooltip :target="`resume${job.id}`">
            {{ $t('hunts.resumeTip') }}
          </BTooltip>
        </button>
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
