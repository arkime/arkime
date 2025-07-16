<!--
Copyright Yahoo Inc.
SPDX-License-Identifier: Apache-2.0
-->
<template>
  <div>
    <div class="row">
      <div class="col-12">
        <hunt-status :status="localJob.status"
          :queue-count="localJob.queueCount">
        </hunt-status>
      </div>
    </div>
    <div class="row">
      <div class="col-12 d-flex">
        <span class="fa fa-fw fa-file-text mt-1" />&nbsp;
        <template v-if="!editDescription">
          <span v-if="localJob.description" class="ps-1">
            {{ localJob.description }}
          </span>
          <em v-else class="ps-1">
            No description
          </em>
          <button
            v-if="canEdit"
            :id="'edit-description-' + localJob.id"
            @click="editDescription = true"
            class="btn btn-xs btn-theme-secondary ms-1">
            <span class="fa fa-pencil" />
            <BTooltip :target="'edit-description-' + localJob.id">Edit description</BTooltip>
          </button>
        </template>
        <div
          v-else-if="canEdit"
          class="flex-grow-1">
          <b-input-group
            size="sm"
            prepend="Description">
            <b-form-input
              v-model="newDescription"
              placeholder="Update the description"
            />
            <b-button
              variant="warning"
              @click="editDescription = false"
              title="Cancel hunt description update">
              Cancel
            </b-button>
            <b-button
              variant="success"
              title="Save hunt description"
              @click="updateJobDescription">
              Save
            </b-button>
          </b-input-group>
        </div>
      </div>
    </div>
    <div>
      <span class="fa fa-fw fa-eye">
      </span>&nbsp;
      Found <strong>{{ commaString(localJob.matchedSessions) }}</strong> sessions
      matching <strong>{{ localJob.search }}</strong> ({{ localJob.searchType }})
      of
      <span v-if="localJob.failedSessionIds && localJob.failedSessionIds.length">
        <strong>{{ commaString(localJob.searchedSessions - localJob.failedSessionIds.length) }}</strong>
      </span>
      <span v-else>
        <strong>{{ commaString(localJob.searchedSessions) }}</strong>
      </span>
      sessions searched
      <span v-if="localJob.failedSessionIds && localJob.failedSessionIds.length">
        <br>
        <span class="fa fa-fw fa-search-plus">
        </span>&nbsp;
        Still need to search
        <strong>{{ commaString(localJob.totalSessions - localJob.searchedSessions + localJob.failedSessionIds.length) }}</strong>
        of <strong>{{ localJob.totalSessions }}</strong>
        total sessions
        <br>
        <span class="fa fa-fw fa-exclamation-triangle">
        </span>&nbsp;
        <strong>{{ commaString(localJob.failedSessionIds.length) }}</strong>
        sessions failed to load and were not searched yet
      </span>
      <span v-else-if="localJob.totalSessions !== localJob.searchedSessions">
        <br>
        <span class="fa fa-fw fa-search-plus">
        </span>&nbsp;
        Still need to search
        <strong>{{ commaString(localJob.totalSessions - localJob.searchedSessions) }}</strong>
        of <strong>{{ localJob.totalSessions }}</strong>
        total sessions
      </span>
    </div>
    <div class="row">
      <div class="col-12">
        <span class="fa fa-fw fa-clock-o">
        </span>&nbsp;
        Created:
        <strong>
          {{ timezoneDateString(localJob.created * 1000, user.settings.timezone, false) }}
        </strong>
      </div>
    </div>
    <div v-if="localJob.lastUpdated"
      class="row">
      <div class="col-12">
        <span class="fa fa-fw fa-clock-o">
        </span>&nbsp;
        Last Updated:
        <strong>
          {{ timezoneDateString(localJob.lastUpdated * 1000, user.settings.timezone, false) }}
        </strong>
      </div>
    </div>
    <div class="row"
      v-if="localJob.notifier">
      <div class="col-12">
        <span class="fa fa-fw fa-bell">
        </span>&nbsp;
        Notifying: {{ notifierName }}
      </div>
    </div>
    <div class="row">
      <div class="col-12">
        <span class="fa fa-fw fa-search">
        </span>&nbsp;
        Examining
        <strong v-if="localJob.size > 0">{{ localJob.size }}</strong>
        <strong v-else>all</strong>
        <strong>{{ localJob.type }}</strong>
        <strong v-if="localJob.src">source</strong>
        <span v-if="localJob.src && localJob.dst">
          and
        </span>
        <strong v-if="localJob.dst">destination</strong>
        packets per session
      </div>
    </div>
    <div v-if="localJob.query.expression"
      class="row">
      <div class="col-12">
        <span class="fa fa-fw fa-search">
        </span>&nbsp;
        The sessions query expression was:
        <strong>{{ localJob.query.expression }}</strong>
      </div>
    </div>
    <div v-if="localJob.query.view"
      class="row">
      <div class="col-12">
        <span class="fa fa-fw fa-search">
        </span>&nbsp;
        The sessions query view was:
        <strong>{{ getViewName(localJob.query.view) }}</strong>
      </div>
    </div>
    <div class="row">
      <div class="col-12">
        <span class="fa fa-fw fa-clock-o">
        </span>&nbsp;
        The sessions query time range was from
        <strong>{{ timezoneDateString(localJob.query.startTime * 1000, user.settings.timezone, false) }}</strong>
        to
        <strong>{{ timezoneDateString(localJob.query.stopTime * 1000, user.settings.timezone, false) }}</strong>
      </div>
    </div>
    <template v-if="canEdit">
      <div class="row mb-2">
        <div class="col-12">
          <span class="fa fa-fw fa-share-alt">
          </span>&nbsp;
          <template v-if="localJob.users && localJob.users.length">
            This job is being shared with these other users:
            <span v-for="user in localJob.users"
              :key="user"
              class="badge bg-secondary ms-1">
              {{ user }}
              <button
                type="button"
                class="btn-close"
                title="Remove this user's access from this hunt"
                @click="removeUser(user, localJob)">
                &times;
              </button>
            </span>
          </template>
          <template v-else-if="localJob.users && !localJob.users.length">
            This hunt is not being shared with specific users.
            Click this button to share it with other users:
          </template>
          <button :id="'add-users-' + localJob.id"
            class="btn btn-xs btn-theme-secondary ms-1"
            @click="toggleAddUsers">
            <span class="fa fa-plus-circle">
            </span>
            <BTooltip :target="'add-users-' + localJob.id">
              Share this hunt with other user(s)
            </BTooltip>
          </button>
          <template v-if="showAddUsers">
            <div class="input-group input-group-sm mb-3 mt-2">
              <div :id="'users-' + localJob.id"
                class="input-group-text cursor-help">
                Users
                <BTooltip :target="'users-' + localJob.id">
                  Let these users view the results of this hunt
                </BTooltip>
              </div>
              <input type="text"
                v-model="newUsers"
                class="form-control"
                v-focus="focusInput"
                @keyup.enter="addUsers(newUsers, localJob)"
                placeholder="Comma separated list of user IDs"
              />
              <button class="btn btn-warning"
                @click="toggleAddUsers">
                Cancel
              </button>
              <button
                class="btn btn-theme-tertiary"
                title="Give these users access to this hunt"
                @click="addUsers(newUsers, localJob)">
                Add User(s)
              </button>
            </div>
          </template>
        </div>
      </div>
      <div class="row mb-2">
        <div class="col-12">
          <span class="fa fa-fw fa-share-alt">
          </span>&nbsp;
          <template v-if="localJob.roles && localJob.roles.length">
            This job is being shared with these roles:
          </template>
          <template v-else-if="!localJob.roles || !localJob.roles.length">
            This hunt is not being shared with any roles.
            Add roles here:
          </template>
          <RoleDropdown
            class="d-inline"
            :roles="roles"
            :selected-roles="localJob.roles"
            @selected-roles-updated="updateJobRoles"
          />
        </div>
      </div>

    </template>
    <div class="row mb-2"
      v-else-if="isShared">
      <div class="col-12">
        <span class="fa fa-fw fa-share-alt">
        </span>&nbsp;
        This job is being shared with you. You can view the results and rerun it.
      </div>
    </div>
    <template v-if="localJob.errors">
      <div v-for="(error, index) in localJob.errors"
        :key="index"
        class="row text-danger">
        <div class="col-12">
          <span class="fa fa-fw fa-exclamation-triangle">
          </span>&nbsp;
          <span v-if="error.time">
            {{ timezoneDateString(error.time * 1000, user.settings.timezone, false) }}
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
import HuntStatus from './HuntStatus.vue';
import HuntService from './HuntService';
import Focus from '@common/Focus.vue';
import RoleDropdown from '@common/RoleDropdown.vue';
import { commaString, timezoneDateString } from '@real_common/vueFilters.js';

export default {
  name: 'HuntData',
  props: {
    job: Object,
    user: Object,
    notifierName: String
  },
  components: {
    HuntStatus,
    RoleDropdown
  },
  directives: { Focus },
  data: function () {
    return {
      newUsers: '',
      focusInput: false,
      showAddUsers: false,
      editDescription: false,
      newDescription: this.job.description,
      anonymousMode: this.$constants.ANONYMOUS_MODE,
      localJob: JSON.parse(JSON.stringify(this.job)) // Deep copy to avoid mutating the original job object
    };
  },
  computed: {
    roles () {
      return this.$store.state.roles;
    },
    views () {
      return this.$store.state.views;
    },
    canEdit () {
      return !this.anonymousMode && HuntService.canEditHunt(this.user, this.localJob);
    },
    isShared () {
      return HuntService.isShared(this.user, this.localJob);
    }
  },
  methods: {
    commaString,
    timezoneDateString,
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
    },
    updateJobRoles: function (roles) {
      this.localJob.roles = roles;
      this.$emit('updateHunt', this.localJob);
    },
    updateJobDescription: function (roles) {
      this.localJob.description = this.newDescription;
      this.$emit('updateHunt', this.localJob);
      this.editDescription = false;
    },
    getViewName: function (viewId) {
      const view = this.views.find(v => v.id === viewId || v.name === viewId);
      return view?.name || 'unknown or deleted view';
    }
  }
};
</script>
