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
        @toggle="$emit('toggle', job)">
      </toggle-btn>
    </td>
    <td class="no-wrap">
      <hunt-status :status="job.status"
        :queue-count="job.queueCount"
        :hide-text="true">
      </hunt-status>
      &nbsp;
      <span class="badge badge-secondary cursor-help percent-done-badge"
        v-if="job.failedSessionIds && job.failedSessionIds.length"
        :id="`jobmatches${job.id}`">
        {{ round((((job.searchedSessions - job.failedSessionIds.length) / job.totalSessions) * 100), 1) }}%
      </span>
      <span v-else
        class="badge badge-secondary cursor-help percent-done-badge"
        :id="`jobmatches${job.id}`">
        {{ round(((job.searchedSessions / job.totalSessions) * 100), 1) }}%
        <BTooltip v-if="job.failedSessionIds && job.failedSessionIds.length"
          :target="`jobmatches${job.id}`">
          Found {{ commaString(job.matchedSessions) }} out of {{ commaString(job.searchedSessions - job.failedSessionIds.length) }} sessions searched.
          <div v-if="job.status !== 'finished'">
            Still need to search {{ commaString(job.totalSessions - job.searchedSessions + job.failedSessionIds.length) }} sessions.
          </div>
        </BTooltip>
        <BTooltip v-else :target="`jobmatches${job.id}`">
          Found {{ commaString(job.matchedSessions) }} out of {{ commaString(job.searchedSessions) }} sessions searched.
          <div v-if="job.status !== 'finished'">
            Still need to search {{ commaString(job.totalSessions - job.searchedSessions) }} sessions.
          </div>
        </BTooltip>
      </span>
      <template v-if="job.errors && job.errors.length">
        <span
          :id="`joberrors${job.id}`"
          class="badge badge-danger cursor-help">
          <span class="fa fa-exclamation-triangle">
          </span>
          <BTooltip :target="`joberrors${job.id}`">
            Errors were encountered while running this hunt job. Open the job to view the error details.
          </BTooltip>
        </span>
      </template>
    </td>
    <td>
      {{ commaString(job.matchedSessions) }}
      <template v-if="job.removed">
        <div :id="`removed${job.id}`">
          <span class="fa fa-info-circle fa-fw cursor-help text-warning"></span>
          <BTooltip :target="`removed${job.id}`">
            This hunt's ID and name have been removed from these matched sessions.
          </BTooltip>
        </div>
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
      {{ notifierName }}
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
          <span v-if="!job.loading"
            class="fa fa-trash-o fa-fw">
          </span>
          <span v-else
            class="fa fa-spinner fa-spin fa-fw">
          </span>
        <BTooltip :target="`removejob${job.id}`">Remove this hunt.</BTooltip>
        </button>
      </template>
      <button
        type="button"
        :id="`remove${job.id}`"
        @click="$emit('removeFromSessions', job)"
        class="ms-1 pull-right btn btn-sm btn-danger"
        title="Remove the hunt name and ID fields from the matched sessions."
        v-if="canEdit && canRemoveFromSessions"
        :disabled="job.loading || !job.matchedSessions || job.removed || !user.removeEnabled">
        <span v-if="!job.loading"
          class="fa fa-times fa-fw">
        </span>
        <span v-else
          class="fa fa-spinner fa-spin fa-fw">
        </span>
        <BTooltip v-if="job.matchedSessions && !job.removed && user.removeEnabled"
          :target="`remove${job.id}`">
          Remove the hunt name and ID fields from the matched sessions.
          <br>
          <strong>Note:</strong> ES takes a while to update sessions, so scrubbing these fields
          might take a minute.
        </BTooltip>
      </button>
      <span v-if="canView">
        <button type="button"
          title="Open results in a new Sessions tab."
          @click="$emit('openSessions', job)"
          :disabled="!job.matchedSessions || job.removed"
          :id="`openresults${job.id}`"
          class="ms-1 pull-right btn btn-sm btn-theme-primary">
          <span class="fa fa-folder-open fa-fw">
          </span>
          <BTooltip v-if="job.matchedSessions && !job.removed"
            :target="`openresults${job.id}`">
            Open results in a new Sessions tab.
            <br>
            <strong>Note:</strong> ES takes a while to update sessions, so your results
            might take a minute to show up.
          </BTooltip>
        </button>
      </span>
      <template v-if="canRerun && !job.unrunnable && (canView)">
        <button
          :id="`rerun${job.id}`"
          type="button"
          @click="$emit('rerunJob', job)"
          class="ms-1 pull-right btn btn-sm btn-theme-secondary">
          <span class="fa fa-refresh fa-fw">
          </span>
          <BTooltip :target="`rerun${job.id}`">
            Rerun this hunt using the current time frame and search criteria.
          </BTooltip>
        </button>
      </template>
      <template v-if="canRepeat && !job.unrunnable && canEdit">
        <button
          :id="`repeat${job.id}`"
          type="button"
          @click="$emit('repeatJob', job)"
          class="ms-1 pull-right btn btn-sm btn-theme-tertiary">
          <span class="fa fa-repeat fa-fw">
          </span>
          <BTooltip :target="`repeat${job.id}`">
            Repeat this hunt using its time frame and search criteria.
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
          <span v-if="!job.loading"
            class="fa fa-ban fa-fw">
          </span>
          <span v-else
            class="fa fa-spinner fa-spin fa-fw">
          </span>
          <BTooltip :target="`cancel${job.id}`">
            Cancel this hunt. It can be viewed in the history after the cancellation is complete.
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
          <span v-if="!job.loading"
            class="fa fa-pause fa-fw">
          </span>
          <span v-else
            class="fa fa-spinner fa-spin fa-fw">
          </span>
          <BTooltip :target="`pause${job.id}`">
            Pause this hunt.
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
          <span v-if="!job.loading"
            class="fa fa-play fa-fw">
          </span>
          <span v-else
            class="fa fa-spinner fa-spin fa-fw">
          </span>
          <BTooltip :target="`resume${job.id}`">
            Play this hunt.
          </BTooltip>
        </button>
      </template>
    </td>
  </tr>
</template>

<script>
import ToggleBtn from '@real_common/ToggleBtn.vue';
import HuntStatus from './HuntStatus.vue';
import HuntService from './HuntService';
import { round, commaString, timezoneDateString } from '@real_common/vueFilters.js';

export default {
  name: 'HuntRow',
  props: {
    job: Object,
    user: Object,
    arrayName: String,
    canRerun: Boolean,
    canRepeat: Boolean,
    canCancel: Boolean,
    notifierName: String,
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
