<template>
  <div>
    <div class="row">
      <div class="col-12">
        <hunt-status :status="job.status"
          :queue-count="job.queueCount">
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
        <span class="fa fa-fw fa-exclamation-triangle">
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
        <span class="fa fa-fw fa-clock-o">
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
        <span class="fa fa-fw fa-clock-o">
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
        <span class="fa fa-fw fa-bell">
        </span>&nbsp;
        Notifying: {{ job.notifier }}
      </div>
    </div>
    <div class="row">
      <div class="col-12">
        <span class="fa fa-fw fa-search">
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
        <span class="fa fa-fw fa-search">
        </span>&nbsp;
        The sessions query expression was:
        <strong>{{ job.query.expression }}</strong>
      </div>
    </div>
    <div v-if="job.query.view"
      class="row">
      <div class="col-12">
        <span class="fa fa-fw fa-search">
        </span>&nbsp;
        The sessions query view was:
        <strong>{{ job.query.view }}</strong>
      </div>
    </div>
    <div class="row">
      <div class="col-12">
        <span class="fa fa-fw fa-clock-o">
        </span>&nbsp;
        The sessions query time range was from
        <strong>{{ job.query.startTime * 1000 | timezoneDateString(user.settings.timezone, false) }}</strong>
        to
        <strong>{{ job.query.stopTime * 1000 | timezoneDateString(user.settings.timezone, false) }}</strong>
      </div>
    </div>
    <template v-if="!anonymousMode">
      <div class="row mb-2"
        v-if="user.userId === job.userId || user.createEnabled">
        <div class="col-12">
          <span class="fa fa-fw fa-share-alt">
          </span>&nbsp;
          <template v-if="job.users && job.users.length">
            This job is being shared with these other users:
            <span v-for="user in job.users"
              :key="user"
              class="badge badge-secondary ml-1">
              {{ user }}
              <button
                type="button"
                class="close"
                title="Remove this user's access from this hunt"
                @click="removeUser(user, job)">
                &times;
              </button>
            </span>
          </template>
          <template v-else-if="job.users && !job.users.length">
            This hunt is not being shared.
            Click this button to share it with other users:
          </template>
          <button class="btn btn-xs btn-theme-secondary ml-1"
            title="Share this hunt with user(s)"
            v-b-tooltip.hover.right
            @click="toggleAddUsers">
            <span class="fa fa-plus-circle">
            </span>
          </button>
          <template v-if="showAddUsers">
            <div class="input-group input-group-sm mb-3 mt-2">
              <div class="input-group-prepend cursor-help"
                v-b-tooltip.hover
                title="Let these users view the results of this hunt">
                <span class="input-group-text">
                  Users
                </span>
              </div>
              <input type="text"
                v-model="newUsers"
                class="form-control"
                v-focus-input="focusInput"
                @keyup.enter="addUsers(newUsers, job)"
                placeholder="Comma separated list of user IDs"
              />
              <div class="input-group-append">
                <button class="btn btn-warning"
                  @click="toggleAddUsers">
                  Cancel
                </button>
                <button
                  class="btn btn-theme-tertiary"
                  title="Give these users access to this hunt"
                  @click="addUsers(newUsers, job)">
                  Add User(s)
                </button>
              </div>
            </div>
          </template>
        </div>
      </div>
    </template>
    <div class="row mb-2"
      v-else-if="job.users.indexOf(user.userId) > -1">
      <div class="col-12">
        <span class="fa fa-fw fa-share-alt">
        </span>&nbsp;
        This job is being shared with you. You can view the results and rerun it.
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
import FocusInput from '../utils/FocusInput';

export default {
  name: 'HuntData',
  props: {
    job: Object,
    user: Object
  },
  components: { HuntStatus },
  directives: { FocusInput },
  data: function () {
    return {
      newUsers: '',
      showAddUsers: false,
      focusInput: false,
      anonymousMode: this.$constants.MOLOCH_ANONYMOUS_MODE
    };
  },
  methods: {
    removeJob: function (job, list) {
      this.$emit('removeJob', job, list);
    },
    removeUser: function (user, job) {
      this.$emit('removeUser', user, job);
    },
    addUsers: function (users, job) {
      this.$emit('addUsers', users, job);
      this.toggleAddUsers();
    },
    toggleAddUsers: function () {
      this.newUsers = '';
      this.showAddUsers = !this.showAddUsers;
      this.focusInput = this.showAddUsers;
    }
  }
};
</script>
