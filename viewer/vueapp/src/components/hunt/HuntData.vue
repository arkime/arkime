<template>
  <div>
    <div class="row">
      <div class="col-12">
        <hunt-status :status="job.status"
          :queue-count="queueCount">
        </hunt-status>
      </div>
    </div>
    <div>
      <span class="fa fa-fw fa-eye">
      </span>&nbsp;
      Found <strong>{{ job.matchedSessions | commaString }}</strong> sessions
      matching <strong>{{ job.search }}</strong> ({{ job.searchType }})
      of
      <span v-if="job.failedSessionIds && job.failedSessionIds.length">
        <strong>{{ job.searchedSessions - job.failedSessionIds.length | commaString }}</strong>
      </span>
      <span v-else>
        <strong>{{ job.searchedSessions | commaString }}</strong>
      </span>
      sessions searched
      <span v-if="job.failedSessionIds && job.failedSessionIds.length">
        <br>
        <span class="fa fa-fw fa-search-plus">
        </span>&nbsp;
        Still need to search
        <strong>{{ (job.totalSessions - job.searchedSessions + job.failedSessionIds.length) | commaString }}</strong>
        of <strong>{{ job.totalSessions }}</strong>
        total sessions
        <br>
        <span class="fa fa-exclamation-triangle fa-fw">
        </span>&nbsp;
        <strong>{{ job.failedSessionIds.length | commaString }}</strong>
        sessions failed to load and were not searched yet
      </span>
      <span v-else-if="job.totalSessions !== job.searchedSessions">
        <br>
        <span class="fa fa-fw fa-search-plus">
        </span>&nbsp;
        Still need to search
        <strong>{{ (job.totalSessions - job.searchedSessions) | commaString }}</strong>
        of <strong>{{ job.totalSessions }}</strong>
        total sessions
      </span>
    </div>
    <div class="row">
      <div class="col-12">
        <span class="fa fa-fw fa-clock-o fa-fw">
        </span>&nbsp;
        Created:
        <strong>
          {{ job.created * 1000 | timezoneDateString(user.settings.timezone, false) }}
        </strong>
      </div>
    </div>
    <div v-if="job.lastUpdated"
      class="row">
      <div class="col-12">
        <span class="fa fa-fw fa-clock-o fa-fw">
        </span>&nbsp;
        Last Updated:
        <strong>
          {{ job.lastUpdated * 1000 | timezoneDateString(user.settings.timezone, false) }}
        </strong>
      </div>
    </div>
    <div class="row"
      v-if="job.notifier">
      <div class="col-12">
        <span class="fa fa-fw fa-bell fa-fw">
        </span>&nbsp;
        Notifying: {{ job.notifier }}
      </div>
    </div>
    <div class="row">
      <div class="col-12">
        <span class="fa fa-fw fa-search fa-fw">
        </span>&nbsp;
        Examining
        <strong v-if="job.size > 0">{{ job.size }}</strong>
        <strong v-else>all</strong>
        <strong>{{ job.type }}</strong>
        <strong v-if="job.src">source</strong>
        <span v-if="job.src && job.dst">
          and
        </span>
        <strong v-if="job.dst">destination</strong>
        packets per session
      </div>
    </div>
    <div v-if="job.query.expression"
      class="row">
      <div class="col-12">
        <span class="fa fa-fw fa-search fa-fw">
        </span>&nbsp;
        The sessions query expression was:
        <strong>{{ job.query.expression }}</strong>
      </div>
    </div>
    <div v-if="job.query.view"
      class="row">
      <div class="col-12">
        <span class="fa fa-fw fa-search fa-fw">
        </span>&nbsp;
        The sessions query view was:
        <strong>{{ job.query.view }}</strong>
      </div>
    </div>
    <div class="row">
      <div class="col-12">
        <span class="fa fa-fw fa-clock-o fa-fw">
        </span>&nbsp;
        The sessions query time range was from
        <strong>{{ job.query.startTime * 1000 | timezoneDateString(user.settings.timezone, false) }}</strong>
        to
        <strong>{{ job.query.stopTime * 1000 | timezoneDateString(user.settings.timezone, false) }}</strong>
      </div>
    </div>
    <template v-if="job.errors">
      <div v-for="(error, index) in job.errors"
        :key="index"
        class="row text-danger">
        <div class="col-12">
          <span class="fa fa-fw fa-exclamation-triangle">
          </span>&nbsp;
          <span v-if="error.time">
            {{ error.time * 1000 | timezoneDateString(user.settings.timezone, false) }}
          </span>
          <span v-if="error.node">
            ({{ error.node }} node)
          </span>
          <span v-if="error.time || error.node">
            -
          </span>
          {{ error.value }}
        </div>
      </div>
    </template>
  </div>
</template>

<script>
import HuntStatus from './HuntStatus';

export default {
  name: 'HuntData',
  props: [ 'job', 'user', 'queueCount' ],
  components: { HuntStatus },
  methods: {
    removeJob: function (job, list) {
      this.$emit('removeJob', job, list);
    }
  }
};
</script>
