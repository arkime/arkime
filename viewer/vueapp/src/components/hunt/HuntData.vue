<!--
Copyright Yahoo Inc.
SPDX-License-Identifier: Apache-2.0
-->
<template>
  <div>
    <v-row>
      <v-col cols="12">
        <hunt-status
          :id="localJob.id"
          :status="localJob.status"
          :queue-count="localJob.queueCount" />
      </v-col>
    </v-row>
    <v-row>
      <v-col
        cols="12"
        class="d-flex">
        <v-icon
          icon="mdi-file-document"
          class="mt-1" />&nbsp;
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
          <v-btn
            v-if="canEdit"
            :id="'edit-description-' + localJob.id"
            @click="editDescription = true"
            :aria-label="$t('hunts.editDescriptionTip')"
            icon
            variant="flat"
            size="x-small"
            density="comfortable"
            :style="secondaryBtnStyle"
            class="ms-1">
            <v-icon icon="mdi-pencil" />
            <v-tooltip :activator="`[id='edit-description-${localJob.id}']`">
              {{ $t('hunts.editDescriptionTip') }}
            </v-tooltip>
          </v-btn>
        </template>
        <div
          v-else-if="canEdit"
          class="flex-grow-1">
          <div class="arkime-input-group arkime-input-group--fluid">
            <span class="arkime-input-label">
              {{ $t('hunts.jobDescription') }}
            </span>
            <input
              type="text"
              class="arkime-input-control"
              v-model="newDescription"
              @keyup.enter="updateJobDescription"
              :placeholder="$t('hunts.jobDescriptionPlaceholder')">
            <v-btn
              color="warning"
              variant="flat"
              size="small"
              density="comfortable"
              class="me-1"
              @click="editDescription = false"
              :title="$t('hunts.cancelDescriptionTip')">
              {{ $t('common.cancel') }}
            </v-btn>
            <v-btn
              color="success"
              variant="flat"
              size="small"
              density="comfortable"
              class="me-1"
              @click="updateJobDescription"
              :title="$t('hunts.saveDescriptionTip')">
              {{ $t('common.save') }}
            </v-btn>
          </div>
        </div>
      </v-col>
    </v-row>
    <div>
      <v-icon icon="mdi-eye" />&nbsp;
      <span
        v-html="$t('hunts.results-searchedHtml', {
          matched: commaString(localJob.matchedSessions),
          search: escapeHtml(localJob.search),
          searchType: localJob.searchType,
          searched: localJob.failedSessionIds && localJob.failedSessionIds.length ? commaString(localJob.searchedSessions - localJob.failedSessionIds.length) : commaString(localJob.searchedSessions),
          total: commaString(localJob.totalSessions)
        })" />
      <span v-if="localJob.failedSessionIds && localJob.failedSessionIds.length">
        <br>
        <v-icon icon="mdi-magnify-plus" />&nbsp;
        <span
          v-html="$t('hunts.results-stillNeedHtml', {
            remaining: commaString(localJob.totalSessions - localJob.searchedSessions + localJob.failedSessionIds.length),
            total: commaString(localJob.totalSessions),
          })" />
        <br>
        <v-icon icon="mdi-alert" />&nbsp;
        <span
          v-html="$t('hunts.results-failedHtml', {
            failed: commaString(localJob.failedSessionIds.length)
          })" />
      </span>
      <span v-else-if="localJob.totalSessions !== localJob.searchedSessions">
        <br>
        <v-icon icon="mdi-magnify-plus" />&nbsp;
        <span
          v-html="$t('hunts.results-stillNeedHtml', {
            remaining: commaString(localJob.totalSessions - localJob.searchedSessions),
            total: commaString(localJob.totalSessions),
          })" />
      </span>
    </div>
    <v-row>
      <v-col cols="12">
        <v-icon icon="mdi-clock-outline" />&nbsp;
        {{ $t('common.created') }}:
        <strong>
          {{ timezoneDateString(localJob.created * 1000, user.settings.timezone, false) }}
        </strong>
      </v-col>
    </v-row>
    <v-row
      v-if="localJob.lastUpdated">
      <v-col cols="12">
        <v-icon icon="mdi-clock-outline" />&nbsp;
        {{ $t('common.lastUpdated') }}:
        <strong>
          {{ timezoneDateString(localJob.lastUpdated * 1000, user.settings.timezone, false) }}
        </strong>
      </v-col>
    </v-row>
    <v-row
      v-if="localJob.notifier">
      <v-col cols="12">
        <v-icon icon="mdi-bell" />&nbsp;
        Notifying: {{ getNotifierNames(localJob.notifier) }}
      </v-col>
    </v-row>
    <v-row>
      <v-col cols="12">
        <v-icon icon="mdi-magnify" />&nbsp;
        <span
          v-html="$t('hunts.results-examiningHtml', {
            size: localJob.size > 0 ? localJob.size : $t('common.all'),
            type: localJob.type,
            srcdst: (localJob.src ? '<strong>' + $t('common.sourceLC') + '</strong>' : '') + (localJob.src && localJob.dst ? ' and ' : '') + (localJob.dst ? '<strong>' + $t('common.destinationLC') + '</strong>' : '')
          })" />
      </v-col>
    </v-row>
    <v-row
      v-if="localJob.query.expression">
      <v-col cols="12">
        <v-icon icon="mdi-magnify" />&nbsp;
        {{ $t('hunts.results-queryExpression') }}:
        <strong>{{ localJob.query.expression }}</strong>
      </v-col>
    </v-row>
    <v-row
      v-if="localJob.query.view">
      <v-col cols="12">
        <v-icon icon="mdi-magnify" />&nbsp;
        {{ $t('hunts.results-queryView') }}:
        <strong>{{ getViewName(localJob.query.view) }}</strong>
      </v-col>
    </v-row>
    <v-row>
      <v-col cols="12">
        <v-icon icon="mdi-clock-outline" />&nbsp;
        <span
          v-html="$t('hunts.results-timeRangeHtml', {
            start: timezoneDateString(localJob.query.startTime * 1000, user.settings.timezone, false),
            stop: timezoneDateString(localJob.query.stopTime * 1000, user.settings.timezone, false)
          })" />
      </v-col>
    </v-row>
    <template v-if="canEdit">
      <v-row>
        <v-col cols="12">
          <v-icon icon="mdi-share-variant" />&nbsp;
          <template v-if="localJob.users && localJob.users.length">
            {{ $t('hunts.sharedWithUsers') }}:
            <v-chip
              v-for="username in localJob.users"
              :key="username"
              size="small"
              variant="flat"
              color="grey"
              closable
              class="ms-1"
              :close-label="$t('hunts.removeUserTip')"
              @click:close="removeUser(username, localJob)">
              {{ username }}
            </v-chip>
          </template>
          <template v-else-if="localJob.users && !localJob.users.length">
            {{ $t('hunts.notSharedWithUsers') }}
          </template>
          <v-btn
            :id="'add-users-' + localJob.id"
            icon
            variant="flat"
            size="x-small"
            density="comfortable"
            :style="secondaryBtnStyle"
            class="ms-1"
            :aria-label="$t('hunts.addUserTip')"
            @click="toggleAddUsers">
            <v-icon icon="mdi-plus-circle" />
            <v-tooltip :activator="`[id='add-users-${localJob.id}']`">
              {{ $t('hunts.addUserTip') }}
            </v-tooltip>
          </v-btn>
          <template v-if="showAddUsers">
            <div class="arkime-input-group arkime-input-group--fluid mb-3 mt-2">
              <span
                :id="'users-' + localJob.id"
                class="arkime-input-label cursor-help">
                Users
                <v-tooltip :activator="`[id='users-${localJob.id}']`">
                  {{ $t('hunts.addedUserTip') }}
                </v-tooltip>
              </span>
              <input
                type="text"
                v-model="newUsers"
                class="arkime-input-control"
                v-focus="focusInput"
                @keyup.enter="addUsers(newUsers, localJob)"
                :placeholder="$t('hunts.jobUsersPlaceholder')">
              <v-btn
                color="warning"
                variant="flat"
                size="small"
                density="comfortable"
                class="me-1"
                @click="toggleAddUsers">
                {{ $t('common.cancel') }}
              </v-btn>
              <v-btn
                variant="flat"
                size="small"
                density="comfortable"
                :style="tertiaryBtnStyle"
                class="me-1"
                :title="$t('hunts.addedUserTip')"
                @click="addUsers(newUsers, localJob)">
                {{ $t('hunts.addUser') }}
              </v-btn>
            </div>
          </template>
        </v-col>
      </v-row>
      <v-row>
        <v-col cols="12">
          <v-icon icon="mdi-share-variant" />&nbsp;
          <template v-if="localJob.roles && localJob.roles.length">
            {{ $t('hunts.addRoles') }}:
          </template>
          <template v-else-if="!localJob.roles || !localJob.roles.length">
            {{ $t('hunts.noRoles') }}:
          </template>
          <RoleDropdown
            size="large"
            class="d-inline"
            :roles="roles"
            :selected-roles="localJob.roles"
            @selected-roles-updated="updateJobRoles" />
        </v-col>
      </v-row>
    </template>
    <v-row
      class="mb-2"
      v-else-if="isShared">
      <v-col cols="12">
        <v-icon icon="mdi-share-variant" />&nbsp;
        {{ $t('hunts.haveAccess') }}
      </v-col>
    </v-row>
    <template v-if="localJob.errors">
      <v-row
        v-for="(error, index) in localJob.errors"
        :key="index"
        class="text-danger">
        <v-col cols="12">
          <v-icon icon="mdi-alert" />&nbsp;
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
        </v-col>
      </v-row>
    </template>
  </div>
</template>

<script>
import HuntStatus from './HuntStatus.vue';
import HuntService from './HuntService';
import Focus from '@common/Focus.vue';
import RoleDropdown from '@common/RoleDropdown.vue';
import { commaString, timezoneDateString, escapeHtml } from '@common/vueFilters.js';

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
      localJob: JSON.parse(JSON.stringify(this.job)), // Deep copy to avoid mutating the original job object
      // Arkime theme-color v-btn styles. Vuetify :color can't take CSS vars.
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
    escapeHtml,
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
    updateJobDescription: function () {
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
