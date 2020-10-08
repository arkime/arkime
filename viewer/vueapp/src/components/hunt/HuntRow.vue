<template>
  <tr>
    <td>
      <toggle-btn
        v-if="user.userId === job.userId || user.createEnabled || job.users.indexOf(user.userId) > -1"
        :opened="job.expanded"
        @toggle="toggleJobDetail(job)">
      </toggle-btn>
    </td>
    <td>
      <hunt-status :status="job.status"
        :queue-count="job.queueCount"
        :hide-text="true">
      </hunt-status>
      &nbsp;
      <span class="badge badge-secondary cursor-help"
        v-if="job.failedSessionIds && job.failedSessionIds.length"
        :id="`jobmatches${job.id}`">
        {{ (((job.searchedSessions - job.failedSessionIds.length) / job.totalSessions) * 100) | round(1) }}%
      </span>
      <span v-else
        class="badge badge-secondary cursor-help"
        :id="`jobmatches${job.id}`">
        {{ ((job.searchedSessions / job.totalSessions) * 100) | round(1) }}%
      </span>
      <b-tooltip v-if="job.failedSessionIds && job.failedSessionIds.length"
        :target="`jobmatches${job.id}`"
        placement="right">
        Found {{ job.matchedSessions | commaString }} out of {{ job.searchedSessions - job.failedSessionIds.length | commaString }} sessions searched.
        <div v-if="job.status !== 'finished'">
          Still need to search {{ (job.totalSessions - job.searchedSessions + job.failedSessionIds.length) | commaString }} sessions.
        </div>
      </b-tooltip>
      <b-tooltip v-else :target="`jobmatches${job.id}`"
        placement="right">
        Found {{ job.matchedSessions | commaString }} out of {{ job.searchedSessions | commaString }} sessions searched.
        <div v-if="job.status !== 'finished'">
          Still need to search {{ (job.totalSessions - job.searchedSessions) | commaString }} sessions.
        </div>
      </b-tooltip>
      <span v-if="job.errors && job.errors.length"
        class="badge badge-danger cursor-help">
        <span class="fa fa-exclamation-triangle"
          v-b-tooltip.hover.right
          title="Errors were encountered while running this hunt job. Open the job to view the error details.">
        </span>
      </span>
    </td>
    <td>
      {{ job.matchedSessions | commaString }}
    </td>
    <td>
      {{ job.name }}
    </td>
    <td>
      {{ job.userId }}
    </td>
    <td>
      <span v-if="user.userId === job.userId || user.createEnabled || job.users.indexOf(user.userId) > -1">
        {{ job.search }} ({{ job.searchType }})
      </span>
    </td>
    <td>
      {{ job.notifier }}
    </td>
    <td>
      {{ job.created * 1000 | timezoneDateString(user.settings.timezone, false) }}
    </td>
    <td>
      <span v-if="user.userId === job.userId || user.createEnabled || job.users.indexOf(user.userId) > -1">
        {{ job.id }}
      </span>
    </td>
    <td>
      <button v-if="user.userId === job.userId || user.createEnabled"
        @click="removeJob(job, 'results')"
        :disabled="job.loading"
        type="button"
        v-b-tooltip.hover
        title="Remove this job from history"
        class="ml-1 pull-right btn btn-sm btn-danger">
        <span v-if="!job.loading"
          class="fa fa-trash-o fa-fw">
        </span>
        <span v-else
          class="fa fa-spinner fa-spin fa-fw">
        </span>
      </button>
      <span v-if="user.userId === job.userId || user.createEnabled || job.users.indexOf(user.userId) > -1">
        <button type="button"
          @click="openSessions(job)"
          v-if="job.matchedSessions"
          :id="`openresults${job.id}`"
          class="ml-1 pull-right btn btn-sm btn-theme-primary">
          <span class="fa fa-folder-open fa-fw">
          </span>
        </button>
        <b-tooltip v-if="job.matchedSessions"
          :target="`openresults${job.id}`">
          Open results in a new Sessions tab.
          <br>
          <strong>Note:</strong> ES takes a while to update sessions, so your results
          might take a minute to show up.
        </b-tooltip>
      </span>
      <button v-if="(job.status === 'running' || job.status === 'queued') && (user.userId === job.userId || user.createEnabled)"
        :disabled="job.loading"
        @click="pauseJob(job)"
        type="button"
        v-b-tooltip.hover
        title="Pause this job"
        class="ml-1 pull-right btn btn-sm btn-warning">
        <span v-if="!job.loading"
          class="fa fa-pause fa-fw">
        </span>
        <span v-else
          class="fa fa-spinner fa-spin fa-fw">
        </span>
      </button>
      <button v-else-if="job.status === 'paused' && (user.userId === job.userId || user.createEnabled)"
        :disabled="job.loading"
        @click="playJob(job)"
        type="button"
        v-b-tooltip.hover
        title="Play this job"
        class="ml-1 pull-right btn btn-sm btn-theme-secondary">
        <span v-if="!job.loading"
          class="fa fa-play fa-fw">
        </span>
        <span v-else
          class="fa fa-spinner fa-spin fa-fw">
        </span>
      </button>
      <button v-if="canRerun && !job.unrunnable && (user.userId === job.userId || user.createEnabled || job.users.indexOf(user.userId) > -1)"
        type="button"
        @click="rerunJob(job)"
        v-b-tooltip.hover
        title="Rerun this hunt job using the current time frame and search criteria."
        class="ml-1 pull-right btn btn-sm btn-theme-secondary">
        <span class="fa fa-refresh fa-fw">
        </span>
      </button>
      <button v-if="canRepeat && !job.unrunnable && (user.userId === job.userId || user.createEnabled)"
        type="button"
        @click="repeatJob(job)"
        v-b-tooltip.hover
        title="Repeat this hunt job using its time frame and search criteria."
        class="ml-1 pull-right btn btn-sm btn-theme-tertiary">
        <span class="fa fa-repeat fa-fw">
        </span>
      </button>
    </td>
  </tr>
</template>

<script>
import ToggleBtn from '../utils/ToggleBtn';
import HuntStatus from './HuntStatus';

export default {
  name: 'HuntRow',
  props: {
    job: Object,
    user: Object,
    canRerun: Boolean,
    canRepeat: Boolean
  },
  components: {
    ToggleBtn,
    HuntStatus
  },
  methods: {
    removeJob: function (job, list) {
      this.$emit('removeJob', job, list);
    },
    openSessions: function (job) {
      this.$emit('openSessions', job);
    },
    pauseJob: function (job) {
      this.$emit('pauseJob', job);
    },
    playJob: function (job) {
      this.$emit('playJob', job);
    },
    rerunJob: function (job) {
      this.$emit('rerunJob', job);
    },
    repeatJob: function (job) {
      this.$emit('repeatJob', job);
    },
    toggleJobDetail: function (job) {
      this.$emit('toggle', job);
    }
  }
};
</script>
