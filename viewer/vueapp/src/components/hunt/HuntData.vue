<!--
Copyright Yahoo Inc.
SPDX-License-Identifier: Apache-2.0
-->
<template>
  <div>
    <div class="row">
      <div class="col-12">
        <hunt-status
          :id="localJob.id"
          :status="localJob.status"
          :queue-count="localJob.queueCount" />
      </div>
    </div>
    <div class="row">
      <div class="col-12 d-flex">
        <span class="fa fa-fw fa-file-text mt-1" />&nbsp;
        <template v-if="!editDescription">
          <span
            v-if="localJob.description"
            class="ps-1">
            {{ localJob.description }}
          </span>
          <em
            v-else
            class="ps-1">
            {{ $t('hunts.noDescription') }}
          </em>
          <button
            v-if="canEdit"
            :id="'edit-description-' + localJob.id"
            @click="editDescription = true"
            class="btn btn-xs btn-theme-secondary ms-1">
            <span class="fa fa-pencil" />
            <BTooltip :target="'edit-description-' + localJob.id">
              {{ $t('hunts.editDescriptionTip') }}
            </BTooltip>
          </button>
        </template>
        <div
          v-else-if="canEdit"
          class="flex-grow-1">
          <b-input-group
            size="sm"
            :prepend="$t('hunts.jobDescription')">
            <b-form-input
              v-model="newDescription"
              @keyup.enter="updateJobDescription"
              :placeholder="$t('hunts.jobDescriptionPlaceholder')" />
            <b-button
              variant="warning"
              @click="editDescription = false"
              :title="$t('hunts.cancelDescriptionTip')">
              {{ $t('common.cancel') }}
            </b-button>
            <b-button
              variant="success"
              @click="updateJobDescription"
              :title="$t('hunts.saveDescriptionTip')">
              {{ $t('common.save') }}
            </b-button>
          </b-input-group>
        </div>
      </div>
    </div>
    <div>
      <span class="fa fa-fw fa-eye" />&nbsp;
      <span
        v-html="$t('hunts.results-searchedHtml', {
          matched: commaString(localJob.matchedSessions),
          search: localJob.search,
          searchType: localJob.searchType,
          searched: localJob.failedSessionIds && localJob.failedSessionIds.length ? commaString(localJob.searchedSessions - localJob.failedSessionIds.length) : commaString(localJob.searchedSessions),
          total: commaString(localJob.totalSessions)
        })" />
      <span v-if="localJob.failedSessionIds && localJob.failedSessionIds.length">
        <br>
        <span class="fa fa-fw fa-search-plus" />&nbsp;
        <span
          v-html="$t('hunts.results-stillNeedHtml', {
            remaining: commaString(localJob.totalSessions - localJob.searchedSessions + localJob.failedSessionIds.length),
            total: commaString(localJob.totalSessions),
          })" />
        <br>
        <span class="fa fa-fw fa-exclamation-triangle" />&nbsp;
        <span
          v-html="$t('hunts.results-failedHtml', {
            failed: commaString(localJob.failedSessionIds.length)
          })" />
      </span>
      <span v-else-if="localJob.totalSessions !== localJob.searchedSessions">
        <br>
        <span class="fa fa-fw fa-search-plus" />&nbsp;
        <span
          v-html="$t('hunts.results-stillNeedHtml', {
            remaining: commaString(localJob.totalSessions - localJob.searchedSessions),
            total: commaString(localJob.totalSessions),
          })" />
      </span>
    </div>
    <div class="row">
      <div class="col-12">
        <span class="fa fa-fw fa-clock-o" />&nbsp;
        {{ $t('common.created') }}:
        <strong>
          {{ timezoneDateString(localJob.created * 1000, user.settings.timezone, false) }}
        </strong>
      </div>
    </div>
    <div
      v-if="localJob.lastUpdated"
      class="row">
      <div class="col-12">
        <span class="fa fa-fw fa-clock-o" />&nbsp;
        {{ $t('common.lastUpdated') }}:
        <strong>
          {{ timezoneDateString(localJob.lastUpdated * 1000, user.settings.timezone, false) }}
        </strong>
      </div>
    </div>
    <div
      class="row"
      v-if="localJob.notifier">
      <div class="col-12">
        <span class="fa fa-fw fa-bell" />&nbsp;
        Notifying: {{ getNotifierNames(localJob.notifier) }}
      </div>
    </div>
    <div class="row">
      <div class="col-12">
        <span class="fa fa-fw fa-search" />&nbsp;
        <span
          v-html="$t('hunts.results-examiningHtml', {
            size: localJob.size > 0 ? localJob.size : $t('common.all'),
            type: localJob.type,
            srcdst: (localJob.src ? '<strong>' + $t('common.sourceLC') +'</strong>' : '') + (localJob.src && localJob.dst ? ' and ' : '') + (localJob.dst ? '<strong>' + $t('common.destinationLC') + '</strong>' : '')
          })" />
      </div>
    </div>
    <div
      v-if="localJob.query.expression"
      class="row">
      <div class="col-12">
        <span class="fa fa-fw fa-search" />&nbsp;
        {{ $t('hunts.results-queryExpression') }}:
        <strong>{{ localJob.query.expression }}</strong>
      </div>
    </div>
    <div
      v-if="localJob.query.view"
      class="row">
      <div class="col-12">
        <span class="fa fa-fw fa-search" />&nbsp;
        {{ $t('hunts.results-queryView') }}:
        <strong>{{ getViewName(localJob.query.view) }}</strong>
      </div>
    </div>
    <div class="row">
      <div class="col-12">
        <span class="fa fa-fw fa-clock-o" />&nbsp;
        <span
          v-html="$t('hunts.results-timeRangeHtml', {
            start: timezoneDateString(localJob.query.startTime * 1000, user.settings.timezone, false),
            stop: timezoneDateString(localJob.query.stopTime * 1000, user.settings.timezone, false)
          })" />
      </div>
    </div>
    <template v-if="canEdit">
      <div class="row mb-2">
        <div class="col-12">
          <span class="fa fa-fw fa-share-alt" />&nbsp;
          <template v-if="localJob.users && localJob.users.length">
            {{ $t('hunts.sharedWithUsers') }}:
            <span
              v-for="username in localJob.users"
              :key="username"
              class="badge bg-secondary ms-1">
              {{ username }}
              <button
                type="button"
                class="btn-close"
                :title="$t('hunts.removeUserTip')"
                @click="removeUser(username, localJob)">
                &times;
              </button>
            </span>
          </template>
          <template v-else-if="localJob.users && !localJob.users.length">
            {{ $t('hunts.notSharedWithUsers') }}
          </template>
          <button
            :id="'add-users-' + localJob.id"
            class="btn btn-xs btn-theme-secondary ms-1"
            @click="toggleAddUsers">
            <span class="fa fa-plus-circle" />
            <BTooltip :target="'add-users-' + localJob.id">
              {{ $t('hunts.addUserTip') }}
            </BTooltip>
          </button>
          <template v-if="showAddUsers">
            <div class="input-group input-group-sm mb-3 mt-2">
              <div
                :id="'users-' + localJob.id"
                class="input-group-text cursor-help">
                Users
                <BTooltip :target="'users-' + localJob.id">
                  {{ $t('hunts.addedUserTip') }}
                </BTooltip>
              </div>
              <input
                type="text"
                v-model="newUsers"
                class="form-control"
                v-focus="focusInput"
                @keyup.enter="addUsers(newUsers, localJob)"
                :placeholder="$t('hunts.jobUsersPlaceholder')">
              <button
                class="btn btn-warning"
                @click="toggleAddUsers">
                {{ $t('common.cancel') }}
              </button>
              <button
                class="btn btn-theme-tertiary"
                :title="$t('hunts.addedUserTip')"
                @click="addUsers(newUsers, localJob)">
                {{ $t('hunts.addUser') }}
              </button>
            </div>
          </template>
        </div>
      </div>
      <div class="row mb-2">
        <div class="col-12">
          <span class="fa fa-fw fa-share-alt" />&nbsp;
          <template v-if="localJob.roles && localJob.roles.length">
            {{ $t('hunts.noRoles') }}:
          </template>
          <template v-else-if="!localJob.roles || !localJob.roles.length">
            {{ $t('hunts.addRoles') }}:
          </template>
          <RoleDropdown
            class="d-inline"
            :roles="roles"
            :selected-roles="localJob.roles"
            @selected-roles-updated="updateJobRoles" />
        </div>
      </div>
    </template>
    <div
      class="row mb-2"
      v-else-if="isShared">
      <div class="col-12">
        <span class="fa fa-fw fa-share-alt" />&nbsp;
        {{ $t('hunts.haveAccess') }}
      </div>
    </div>
    <template v-if="localJob.errors">
      <div
        v-for="(error, index) in localJob.errors"
        :key="index"
        class="row text-danger">
        <div class="col-12">
          <span class="fa fa-fw fa-exclamation-triangle" />&nbsp;
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
import { commaString, timezoneDateString } from '@common/vueFilters.js';

export default {
  name: 'HuntData',
  emits: ['removeJob', 'removeUser', 'addUsers', 'updateHunt'],
  props: {
    job: {
      type: Object,
      default: () => ({})
    },
    user: {
      type: Object,
      default: () => ({})
    }
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
    },
    notifiers () {
      return this.$store.state.notifiers;
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
    },
    getNotifierNames: function (notifierIds) {
      const notifierNames = notifierIds
        .map(id => {
          const notifier = this.notifiers.find(n => n.id === id);
          return notifier ? `${notifier.name} (${notifier.type})` : id;
        })
        .sort();
      return notifierNames.join(', ');
    }
  }
};
</script>
