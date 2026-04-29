<!--
Copyright Yahoo Inc.
SPDX-License-Identifier: Apache-2.0
-->
<template>
  <div>
    <h3>
      {{ $t('settings.cron.title') }}
      <v-btn-toggle
        v-if="user.roles.includes('arkimeAdmin')"
        density="compact"
        variant="outlined"
        color="secondary"
        class="ms-1"
        multiple
        :model-value="seeAll ? ['seeAll'] : []"
        @update:model-value="(val) => updateSeeAll(val.includes('seeAll'))">
        <v-btn value="seeAll" id="seeAllPeriodicQueries">
          {{ $t(seeAll ? 'settings.cron.allPeriodicQueries' : 'settings.cron.myPeriodicQueries') }}
          <v-tooltip activator="parent" location="top">
            {{ $t(seeAll ? 'settings.cron.allPeriodicQueriesTip' : 'settings.cron.myPeriodicQueriesTip') }}
          </v-tooltip>
        </v-btn>
      </v-btn-toggle>
      <button
        type="button"
        class="btn btn-sm btn-success pull-right d-inline"
        @click="showCronModal = true">
        <span class="fa fa-plus-circle me-1" />
        {{ $t('settings.cron.newPeriodicQuery') }}
      </button>
    </h3>

    <p>
      {{ $t('settings.cron.description') }}
    </p>

    <hr>

    <!-- query list error -->
    <div
      v-if="cronQueryListError"
      style="z-index: 2000;"
      class="mt-2 mb-2 alert alert-danger">
      <span class="fa fa-exclamation-triangle me-1" />
      {{ cronQueryListError }}
    </div> <!-- /query list error -->

    <!-- no results -->
    <div
      class="text-center mt-4"
      v-if="!cronQueries || !cronQueries.length">
      <h3>
        <span class="fa fa-folder-open fa-2x" />
      </h3>
      <h5 v-html="$t('settings.cron.noPeriodicQueriesHtml')" />
    </div> <!-- /no results -->

    <!-- new cron query form -->
    <v-dialog
      :model-value="showCronModal"
      @update:model-value="(val) => { if (!val) showCronModal = false; }"
      max-width="1140">
      <v-card density="compact">
        <v-card-title>Create New Periodic Query</v-card-title>
        <v-card-text>
          <!-- create form -->
          <form>
            <div class="row mb-2">
              <div class="col-md-4">
                <div class="input-group input-group-sm">
                  <span id="newCronQueryName" class="input-group-text cursor-help">
                    {{ $t('settings.cron.queryName') }}<sup>*</sup>
                  </span>
                  <input
                    type="text"
                    class="form-control"
                    maxlength="20"
                    :value="newCronQueryName"
                    @input="newCronQueryName = $event.target.value"
                    :placeholder="$t('settings.cron.queryNamePlaceholder')">
                </div>
              </div>
              <div class="col-md-4">
                <div class="input-group input-group-sm">
                  <span id="newCronQueryAction" class="input-group-text cursor-help">
                    {{ $t('settings.cron.queryAction') }}<sup>*</sup>
                    <v-tooltip activator="#newCronQueryAction">
                      {{ $t('settings.cron.queryActionTip') }}
                    </v-tooltip>
                  </span>
                  <select
                    class="form-control form-control-sm"
                    :value="newCronQueryAction"
                    @input="newCronQueryAction = $event.target.value">
                    <option value="tag">
                      {{ $t('settings.cron.queryAction-tag') }}
                    </option>
                    <option
                      v-for="(cluster, key) in clusters"
                      :key="key"
                      :value="`forward:${key}`">
                      {{ $t('settings.cron.queryAction-forward', { cluster: cluster.name }) }}
                    </option>
                  </select>
                </div>
              </div>
              <div class="col-md-4">
                <div class="input-group input-group-sm">
                  <span id="newCronQueryTags" class="input-group-text cursor-help">
                    {{ $t('settings.cron.tags') }}<sup>*</sup>
                    <v-tooltip activator="#newCronQueryTags">
                      {{ $t('settings.cron.tagsTip') }}
                    </v-tooltip>
                  </span>
                  <input
                    type="text"
                    class="form-control"
                    :value="newCronQueryTags"
                    @input="newCronQueryTags = $event.target.value"
                    :placeholder="$t('settings.cron.tagsPlaceholder')">
                </div>
              </div>
            </div>
            <div class="row mb-2">
              <div class="col">
                <div class="input-group input-group-sm">
                  <span id="newCronQueryExpression" class="input-group-text cursor-help">
                    {{ $t('settings.cron.searchExpression') }}<sup>*</sup>
                    <v-tooltip activator="#newCronQueryExpression">
                      {{ $t('settings.cron.searchExpressionTip') }}
                    </v-tooltip>
                  </span>
                  <ExpressionAutocompleteInput
                    :model-value="newCronQueryExpression"
                    @update:model-value="newCronQueryExpression = $event"
                    :placeholder="$t('settings.cron.searchExpressionPlaceholder')" />
                </div>
              </div>
            </div>
            <div class="row mb-2">
              <div class="col-md-6">
                <div class="input-group input-group-sm">
                  <span id="newCronQueryProcess" class="input-group-text cursor-help">
                    {{ $t('settings.cron.querySince') }}<sup>*</sup>
                    <v-tooltip activator="#newCronQueryProcess">
                      {{ $t('settings.cron.querySinceTip') }}
                    </v-tooltip>
                  </span>
                  <select
                    class="form-control form-control-sm"
                    :value="newCronQueryProcess"
                    @input="newCronQueryProcess = parseInt($event.target.value)">
                    <option
                      v-for="opt in cronProcessOptions"
                      :key="opt.value"
                      :value="opt.value">
                      {{ opt.text }}
                    </option>
                  </select>
                </div>
              </div>
              <div class="col-md-6">
                <NotifierDropdown
                  :notifiers="notifiers"
                  :selected-notifiers="newCronQueryNotifier"
                  @selected-notifiers-updated="newCronQueryNotifier = $event"
                  :display-text="newCronQueryNotifier.length > 0 ? $t('common.notifierCount', newCronQueryNotifier.length) : $t('settings.cron.selectNotifier')" />
              </div>
            </div>
            <div class="row mb-2">
              <div class="col">
                <div class="input-group input-group-sm">
                  <span id="newCronQueryDescription" class="input-group-text cursor-help">
                    {{ $t('settings.cron.queryDescription') }}
                    <v-tooltip activator="#newCronQueryDescription">
                      {{ $t('settings.cron.queryDescriptionTip') }}
                    </v-tooltip>
                  </span>
                  <textarea
                    class="form-control"
                    :value="newCronQueryDescription"
                    @input="newCronQueryDescription = $event.target.value"
                    :placeholder="$t('settings.cron.queryDescriptionPlaceholder')" />
                </div>
              </div>
            </div>
            <div class="d-flex">
              <div class="me-3 flex-grow-1 no-wrap">
                <RoleDropdown
                  :roles="roles"
                  class="d-inline"
                  :display-text="$t('common.rolesCanView')"
                  :selected-roles="newCronQueryRoles"
                  @selected-roles-updated="updateNewCronQueryRoles" />
                <RoleDropdown
                  :roles="roles"
                  class="d-inline ms-1"
                  :display-text="$t('common.rolesCanEdit')"
                  :selected-roles="newCronQueryEditRoles"
                  @selected-roles-updated="updateNewCronQueryEditRoles" />
              </div>
              <div class="input-group input-group-sm flex-grow-1">
                <span id="newCronQueryUsers" class="input-group-text cursor-help">
                  {{ $t('common.shareWithUsers') }}
                  <v-tooltip activator="#newCronQueryUsers">
                    {{ $t('settings.cron.shareWithUsersTip') }}
                  </v-tooltip>
                </span>
                <input
                  type="text"
                  class="form-control"
                  :value="newCronQueryUsers"
                  @input="newCronQueryUsers = $event.target.value"
                  :placeholder="$t('settings.cron.shareWithUsersPlaceholder')">
              </div>
            </div>
          </form> <!-- /create form -->
          <!-- create error -->
          <div
            v-if="cronQueryFormError"
            style="z-index: 2000;"
            class="mt-2 mb-0 alert alert-danger">
            <span class="fa fa-exclamation-triangle me-1" />
            {{ cronQueryFormError }}
          </div> <!-- /create error -->
        </v-card-text>
        <v-card-actions>
          <div class="w-100 d-flex justify-content-between">
            <button
              type="button"
              class="btn btn-danger"
              :title="$t('common.cancel')"
              @click="showCronModal = false">
              <span class="fa fa-times" />
              {{ $t('common.cancel') }}
            </button>
            <button
              type="button"
              class="btn btn-success"
              :disabled="cronLoading"
              @click="createCronQuery"
              :class="{'disabled':cronLoading}">
              <template v-if="!cronLoading">
                <span class="fa fa-plus-circle me-1" />
                {{ $t('common.create') }}
              </template>
              <template v-else>
                <span class="fa fa-spinner fa-spin me-1" />
                {{ $t('common.creating') }}
              </template>
            </button>
          </div>
        </v-card-actions>
      </v-card>
    </v-dialog> <!-- /new cron query form -->

    <!-- cron queries -->
    <div class="cron-card-columns mb-2">
      <v-card
        :key="query.key"
        class="cron-card mb-2"
        @keyup.esc="getCronQueries"
        v-for="(query, index) in cronQueries"
        @keyup.enter="updateCronQuery(query, index)">
        <v-card-title class="cron-card-header">
          <h6 class="mb-0 d-flex">
            <div class="input-group input-group-sm flex-grow-1">
              <span class="input-group-text">
                {{ $t('settings.cron.queryName') }}<sup>*</sup>
              </span>
              <input
                type="text"
                class="form-control"
                maxlength="20"
                :value="query.name"
                @input="query.name = $event.target.value; cronQueryChanged(query)"
                :disabled="!canEditCronQuery(query)">
            </div>
            <div class="ms-2 mt-1">
              <input
                type="checkbox"
                class="form-check-input pull-right"
                :checked="query.enabled"
                :id="`queryEnabled${index}`"
                :disabled="!canEditCronQuery(query)"
                @change="query.enabled = $event.target.checked; toggleCronQueryEnabled(index)">
              <v-tooltip :activator="`#queryEnabled${index}`">
                {{ query.enabled ? 'Enabled' : 'Disabled' }}
              </v-tooltip>
            </div>
          </h6>
        </v-card-title>
        <v-card-text>
          <div class="input-group input-group-sm mb-2">
            <span :id="`queryDescription${index}`" class="input-group-text cursor-help">
              {{ $t('settings.cron.queryDescription') }}
              <v-tooltip :activator="`#queryDescription${index}`">
                {{ $t('settings.cron.queryDescriptionTip') }}
              </v-tooltip>
            </span>
            <textarea
              class="form-control form-control-sm"
              :value="query.description"
              @input="query.description = $event.target.value; cronQueryChanged(query)"
              :disabled="!canEditCronQuery(query)" />
          </div>
          <div class="input-group input-group-sm mb-2">
            <span :id="`queryAction${index}`" class="input-group-text cursor-help">
              {{ $t('settings.cron.queryAction') }}<sup>*</sup>
              <v-tooltip :activator="`#queryAction${index}`">
                Action to perform when a session matches this query
              </v-tooltip>
            </span>
            <select
              class="form-control form-control-sm"
              :value="query.action"
              @input="query.action = $event.target.value; cronQueryChanged(query)"
              :disabled="!canEditCronQuery(query)">
              <option value="tag">
                {{ $t('settings.cron.queryAction-tag') }}
              </option>
              <option
                v-for="(cluster, key) in clusters"
                :key="key"
                :value="`forward:${key}`">
                {{ $t('settings.cron.queryAction-forward', { cluster: cluster.name }) }}
              </option>
            </select>
          </div>
          <div class="input-group input-group-sm mb-2">
            <span class="input-group-text cursor-help" :id="`queryTags${index}`">
              {{ $t('settings.cron.tags') }}<sup>*</sup>
              <v-tooltip :activator="`#queryTags${index}`">
                {{ $t('settings.cron.tagsTip') }}
              </v-tooltip>
            </span>
            <input
              type="text"
              class="form-control"
              :value="query.tags"
              @input="query.tags = $event.target.value; cronQueryChanged(query)"
              :disabled="!canEditCronQuery(query)">
          </div>
          <div class="input-group input-group-sm mb-2">
            <span class="input-group-text cursor-help" :id="`queryExpression${index}`">
              {{ $t('settings.cron.searchExpression') }}<sup>*</sup>
              <v-tooltip :activator="`#queryExpression${index}`">
                {{ $t('settings.cron.searchExpressionTip') }}
              </v-tooltip>
            </span>
            <ExpressionAutocompleteInput
              :model-value="query.query"
              @update:model-value="query.query = $event; cronQueryChanged(query)"
              :disabled="!canEditCronQuery(query)" />
          </div>
          <div
            class="input-group input-group-sm mb-2"
            v-if="canEditCronQuery(query)">
            <span class="input-group-text cursor-help" :id="`queryUsers${index}`">
              {{ $t('common.shareWithUsers') }}
              <v-tooltip :activator="`#queryUsers${index}`">
                {{ $t('settings.cron.shareWithUsersTip') }}
              </v-tooltip>
            </span>
            <input
              type="text"
              class="form-control"
              :value="query.users"
              @input="query.users = $event.target.value; cronQueryChanged(query)">
          </div>
          <div
            class="mb-2 no-wrap"
            v-if="canEditCronQuery(query)">
            <NotifierDropdown
              class="d-inline"
              :notifiers="notifiers"
              :selected-notifiers="query.notifier || []"
              @selected-notifiers-updated="query.notifier = $event; cronQueryChanged(query)"
              :display-text="query.notifier?.length > 0 ? $t('common.notifierCount', query.notifier.length) : $t('settings.cron.selectNotifier')" />
            <RoleDropdown
              :roles="roles"
              :id="query.key"
              class="d-inline ms-1"
              :selected-roles="query.roles"
              @selected-roles-updated="updateCronQueryRoles"
              :display-text="query.roles && query.roles.length ? undefined : $t('common.rolesCanView')" />
            <RoleDropdown
              :roles="roles"
              :id="query.key"
              class="d-inline ms-1"
              :selected-roles="query.editRoles"
              @selected-roles-updated="updateCronQueryEditRoles"
              :display-text="query.editRoles && query.editRoles.length ? undefined : $t('common.rolesCanEdit')" />
          </div>
        </v-card-text>
        <v-card-text>
          <div class="row">
            <div class="col">
              <strong>{{ $t('settings.cron.matches') }}</strong>:
              {{ query.count }}
            </div>
          </div>
          <div
            class="row"
            v-if="query.creator">
            <div class="col">
              <strong>{{ $t('settings.cron.createdBy') }}</strong>:
              {{ query.creator }}
            </div>
          </div>
          <div
            class="row"
            v-if="query.created">
            <div class="col">
              <strong>{{ $t('settings.cron.createdAt') }}</strong>:
              {{ timezoneDateString(query.created * 1000, user.settings.timezone, false) }}
            </div>
          </div>
          <div
            class="row"
            v-if="query.lastRun">
            <div class="col">
              <strong>{{ $t('settings.cron.lastRun') }}</strong>:
              {{ timezoneDateString(query.lastRun * 1000, user.settings.timezone, false) }}
            </div>
          </div>
          <div
            class="row"
            v-if="query.lastRun">
            <div class="col">
              <strong>{{ $t('settings.cron.lastMatches') }}</strong>:
              {{ query.lastCount || 0 }}
            </div>
          </div>
          <div
            class="row"
            v-if="query.lastToggled">
            <div class="col">
              <strong>{{ query.enabled ? 'Enabled' : 'Disabled' }} at</strong>:
              {{ timezoneDateString(query.lastToggled * 1000, user.settings.timezone, false) }}
            </div>
          </div>
          <div
            class="row"
            v-if="query.lastToggled">
            <div class="col">
              <strong>{{ $t('settings.cron.lastToggledBy') }}</strong>:
              by {{ query.lastToggledBy }}
            </div>
          </div>
        </v-card-text>
        <v-card-actions>
          <button
            type="button"
            class="btn btn-sm btn-warning"
            :id="`openMatches${index}`"
            @click="openCronSessions(query)">
            <span class="fa fa-folder-open fa-fw me-1" />
            {{ $t('settings.cron.openMatches') }}
            <v-tooltip :activator="`#openMatches${index}`">
              {{ $t('settings.cron.openMatchesTip') }}
            </v-tooltip>
          </button>
          <template v-if="canEditCronQuery(query)">
            <template v-if="query.changed">
              <button
                type="button"
                class="btn btn-sm btn-warning ms-1"
                @click="getCronQueries"
                :id="`cancel${index}`">
                <span class="fa fa-ban fa-fw me-1" />
                {{ $t('common.cancel') }}
                <v-tooltip :activator="`#cancel${index}`">
                  {{ $t('settings.cron.cancelTip') }}
                </v-tooltip>
              </button>
              <button
                type="button"
                class="btn btn-sm btn-theme-tertiary pull-right ms-1"
                :id="`save${index}`"
                @click="updateCronQuery(query, index)">
                <span class="fa fa-save fa-fw me-1" />
                {{ $t('common.save') }}
                <v-tooltip :activator="`#save${index}`">
                  {{ $t('settings.cron.saveTip') }}
                </v-tooltip>
              </button>
            </template>
            <template v-else>
              <button
                type="button"
                class="btn btn-sm btn-danger pull-right ms-1"
                :id="`delete${index}`"
                @click="deleteCronQuery(query, index)">
                <span class="fa fa-trash-o fa-fw me-1" />
                {{ $t('common.delete') }}
                <v-tooltip :activator="`#delete${index}`">
                  {{ $t('settings.cron.deleteTip') }}
                </v-tooltip>
              </button>
              <button
                type="button"
                class="btn btn-sm btn-info ms-1"
                v-if="canTransfer(query)"
                :id="`transfer${index}`"
                @click="openTransferQuery(query)">
                <span class="fa fa-share fa-fw" />
                <v-tooltip :activator="`#transfer${index}`">
                  {{ $t('settings.cron.transferTip') }}
                </v-tooltip>
              </button>
            </template>
          </template>
        </v-card-actions>
      </v-card>
    </div> <!-- /cron queries -->

    <transfer-resource
      :show-modal="showTransferModal"
      @transfer-resource="submitTransferQuery" />
  </div>
</template>

<script>
// services
import SettingsService from './SettingsService.js';
import UserService from '@common/UserService.js';
// components
import RoleDropdown from '@common/RoleDropdown.vue';
import NotifierDropdown from '@common/NotifierDropdown.vue';
import TransferResource from '@common/TransferResource.vue';
import ExpressionAutocompleteInput from '../search/ExpressionAutocompleteInput.vue';
// utils
import { timezoneDateString } from '@common/vueFilters.js';
import { resolveMessage } from '@common/resolveI18nMessage';

export default {
  name: 'PeriodicQueries',
  emits: ['display-message'],
  components: {
    RoleDropdown,
    NotifierDropdown,
    TransferResource,
    ExpressionAutocompleteInput
  },
  props: {
    userId: {
      type: String,
      default: ''
    } // the setting user id
  },
  data () {
    return {
      cronLoading: false,
      cronQueries: undefined,
      cronQueryListError: '',
      cronQueryFormError: '',
      newCronQueryName: '',
      newCronQueryDescription: '',
      newCronQueryExpression: '',
      newCronQueryTags: '',
      newCronQueryNotifier: [],
      newCronQueryProcess: '0',
      newCronQueryAction: 'tag',
      newCronQueryUsers: '',
      newCronQueryRoles: [],
      newCronQueryEditRoles: [],
      seeAll: false,
      transferQuery: undefined,
      showCronModal: false,
      showTransferModal: false
    };
  },
  computed: {
    user () {
      return this.$store.state.user;
    },
    roles () {
      return this.$store.state.roles;
    },
    notifiers () {
      return this.$store.state.notifiers;
    },
    clusters () {
      return this.$store.state.remoteclusters;
    },
    cronProcessOptions () {
      return [
        { value: 0, text: 'Now' },
        { value: 1, text: this.$t('common.hourAgoCount', 1) },
        { value: 6, text: this.$t('common.hourAgoCount', 6) },
        { value: 24, text: this.$t('common.hourAgoCount', 24) },
        { value: 48, text: this.$t('common.hourAgoCount', 48) },
        { value: 72, text: this.$t('common.hourAgoCount', 72) },
        { value: 168, text: this.$t('common.weekAgoCount', 1) },
        { value: 336, text: this.$t('common.weekAgoCount', 2) },
        { value: 720, text: this.$t('common.monthAgoCount', 1) },
        { value: 1440, text: this.$t('common.monthAgoCount', 2) },
        { value: 4380, text: this.$t('common.monthAgoCount', 6) },
        { value: 8760, text: this.$t('common.yearAgoCount', 1) },
        { value: -1, text: this.$t('common.allCareful') }
      ];
    }
  },
  mounted () {
    this.getCronQueries();

    if (this.$route.query.expression) {
      this.newCronQueryExpression = this.$route.query.expression;
    }

    if (this.$route.query.process) {
      this.newCronQueryProcess = this.$route.query.process;
      this.$router.replace({ // remove process query param as nothing else uses it
        query: {
          ...this.$route.query,
          process: undefined
        }
      });
    }
  },
  methods: {
    timezoneDateString,
    // EXPOSED PAGE FUNCTIONS ---------------------------------------------- //
    updateSeeAll (newSeeAll) {
      this.seeAll = newSeeAll;
      this.getCronQueries();
    },
    canEditCronQuery (query) {
      return this.user.roles.includes('arkimeAdmin') ||
        (query.creator && query.creator === this.user.userId) ||
        (query.editRoles && UserService.hasRole(this.user, query.editRoles.join(',')));
    },
    canTransfer (query) {
      return this.user.roles.includes('arkimeAdmin') ||
        (query.creator && query.creator === this.user.userId);
    },
    updateNewCronQueryRoles (roles) {
      this.newCronQueryRoles = roles;
    },
    updateNewCronQueryEditRoles (roles) {
      this.newCronQueryEditRoles = roles;
    },
    /* creates a cron query given the name, expression, process, and tags */
    createCronQuery () {
      if (!this.newCronQueryName || this.newCronQueryName === '') {
        this.cronQueryFormError = this.$t('settings.cron.queryNameEmpty');
        return;
      }

      if (!this.newCronQueryExpression || this.newCronQueryExpression === '') {
        this.cronQueryFormError = this.$t('settings.cron.searchExpressionEmpty');
        return;
      }

      if (!this.newCronQueryTags || this.newCronQueryTags === '') {
        this.cronQueryFormError = this.$t('settings.cron.tagsEmpty');
        return;
      }

      this.cronLoading = true;

      const data = {
        enabled: true,
        tags: this.newCronQueryTags,
        name: this.newCronQueryName,
        users: this.newCronQueryUsers,
        roles: this.newCronQueryRoles,
        since: this.newCronQueryProcess,
        action: this.newCronQueryAction,
        query: this.newCronQueryExpression,
        editRoles: this.newCronQueryEditRoles,
        description: this.newCronQueryDescription
      };

      if (this.newCronQueryNotifier && this.newCronQueryNotifier.length > 0) {
        data.notifier = this.newCronQueryNotifier;
      }

      SettingsService.createCronQuery(data, this.userId).then((response) => {
        // add the cron query to the view
        this.cronQueries.push(response.query);
        // reset fields and remove form error
        this.newForm = false;
        this.clearNewFormInputs();
        this.cronLoading = false;
        this.showCronModal = false;
        // display success message to user
        let msg = resolveMessage(response, this.$t) || 'Successfully created periodic query.';
        if (response.invalidUsers && response.invalidUsers.length) {
          msg += ` Could not add these users: ${response.invalidUsers.join(',')}`;
        }
        this.$emit('display-message', { msg });
      }).catch((error) => {
        // display error message to user
        this.cronLoading = false;
        this.cronQueryFormError = resolveMessage(error, this.$t);
      });
    },
    /**
     * Opens the matching sessions in a new tab
     * @param {object} query The query object to open sessions for
     */
    openCronSessions (query) {
      if (query.tags) {
        const tags = query.tags.split(',');
        let url = 'sessions?expression=';
        for (let t = 0, tlen = tags.length; t < tlen; t++) {
          const tag = tags[t];
          url += `tags%20%3D%3D%20${tag}`; // encoded ' == '
          if (t !== tlen - 1) { url += '%20%26%26%20'; } // encoded ' && '
        }
        window.open(url, '_blank'); // open in new tab
      } else {
        this.$emit('display-message', { msg: this.$t('settings.cron.queryNotTagged'), type: 'danger' });
      }
    },
    /**
     * Deletes a query
     * @param {object} query The query object to delete
     * @param {number} index The index of the query in the list
     */
    deleteCronQuery (query, index) {
      SettingsService.deleteCronQuery(query.key, this.userId).then((response) => {
        // remove the query from the list
        this.cronQueries.splice(index, 1);
        // display success message to user
        this.$emit('display-message', { msg: resolveMessage(response, this.$t) });
      }).catch((error) => {
        // display error message to user
        this.$emit('display-message', { msg: resolveMessage(error, this.$t), type: 'danger' });
      });
    },
    /**
     * Opens the transfer resource modal
     * @param {Object} query The periodic query to transfer
     */
    openTransferQuery (query) {
      this.transferQuery = query;
      this.showTransferModal = true;
    },
    /**
     * Submits the transfer resource modal contents and updates the periodic query
     * @param {Object} userId The user id to transfer the periodic query to
     */
    submitTransferQuery ({ userId }) {
      if (!userId) {
        this.transferQuery = undefined;
        return;
      }

      const data = JSON.parse(JSON.stringify(this.transferQuery));
      data.creator = userId;

      SettingsService.updateCronQuery(data, this.userId).then((response) => {
        this.getCronQueries();
        this.transferQuery = undefined;
        this.$emit('display-message', { msg: resolveMessage(response, this.$t) });
        this.showTransferModal = false;
      }).catch((error) => {
        // display error message to user
        this.$emit('display-message', { msg: resolveMessage(error, this.$t), type: 'danger' });
      });
    },
    /**
     * Enables/Disables a query given its key and updates the query
     * @param {number} index The index of the query in the list
     */
    toggleCronQueryEnabled (index) {
      this.cronQueryChanged(this.cronQueries[index]);
      this.updateCronQuery(this.cronQueries[index], index);
    },
    cronQueryNotifierChanged (query) {
      this.cronQueryChanged(query);
    },
    /* updates the roles on a cron query from the RoleDropdown component */
    updateCronQueryRoles (roles, id) {
      for (const query of this.cronQueries) {
        if (query.key === id) {
          query.roles = roles;
          this.cronQueryChanged(query);
          return;
        }
      }
    },
    /* updates the edit roles on a cron query from the RoleDropdown component */
    updateCronQueryEditRoles (roles, id) {
      for (const query of this.cronQueries) {
        if (query.key === id) {
          query.editRoles = roles;
          this.cronQueryChanged(query);
          return;
        }
      }
    },
    /**
     * Sets a cron query as having been changed
     * @param {object} query The query object that changed
     */
    cronQueryChanged (query) {
      query.changed = true;
    },
    /**
     * Updates a cron query
     * @param {object} query The query to update
     * @param {number} index The index of the query in the list
     */
    updateCronQuery (query, index) {
      if (!this.canEditCronQuery(query)) {
        this.$emit('display-message', { msg: 'Permission denied!', type: 'danger' });
        return;
      }

      if (!query.name) {
        this.$emit('display-message', { msg: 'Missing query name', type: 'warning' });
        return;
      }

      if (!query.query) {
        this.$emit('display-message', { msg: 'Missing query expression', type: 'warning' });
        return;
      }

      if (!query.tags) {
        this.$emit('display-message', { msg: 'Missing query tags', type: 'warning' });
        return;
      }

      if (!query.changed) {
        this.$emit('display-message', { msg: 'This query has not changed', type: 'warning' });
        return;
      }

      SettingsService.updateCronQuery(query, this.userId).then((response) => {
        this.cronQueries[index] = response.query;
        // display success message to user
        let msg = resolveMessage(response, this.$t) || 'Successfully updated periodic query.';
        if (response.invalidUsers && response.invalidUsers.length) {
          msg += ` Could not add these users: ${response.invalidUsers.join(',')}`;
        }
        this.$emit('display-message', { msg });
      }).catch((error) => {
        // display error message to user
        this.$emit('display-message', { msg: resolveMessage(error, this.$t), type: 'danger' });
      });
    },
    /* retrieves the specified user's cron queries */
    getCronQueries () {
      const queryParams = {};

      if (this.seeAll) { queryParams.all = true; }
      if (this.userId) { queryParams.userId = this.userId; }

      SettingsService.getCronQueries(queryParams).then((response) => {
        this.cronQueries = response;
      }).catch((error) => {
        this.cronQueryListError = resolveMessage(error, this.$t);
      });
    },
    // HELPER FUNCTIONS ---------------------------------------------------- //
    clearNewFormInputs () {
      this.cronQueryFormError = '';
      this.newCronQueryName = '';
      this.newCronQueryTags = '';
      this.newCronQueryUsers = '';
      this.newCronQueryRoles = [];
      this.newCronQueryEditRoles = [];
      this.newCronQueryExpression = '';
      this.newCronQueryDescription = '';
      this.newCronQueryNotifier = [];
    }
  }
};
</script>

<style scoped>
/* Pinterest-style masonry layout to replace b-card-group's columns prop. */
.cron-card-columns {
  column-count: 3;
  column-gap: 0.75rem;
}
.cron-card {
  display: inline-block;
  width: 100%;
  break-inside: avoid;
}
@media (max-width: 1199.98px) {
  .cron-card-columns { column-count: 2; }
}
@media (max-width: 767.98px) {
  .cron-card-columns { column-count: 1; }
}
</style>
