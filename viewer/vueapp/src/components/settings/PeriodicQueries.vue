<!--
Copyright Yahoo Inc.
SPDX-License-Identifier: Apache-2.0
-->
<template>
  <div>
    <h3>
      {{ $t('settings.cron.title') }}
      <BFormCheckbox
        inline
        button
        size="sm"
        class="ms-1"
        v-if="user.roles.includes('arkimeAdmin')"
        id="seeAllPeriodicQueries"
        @update:model-value="updateSeeAll"
        :model-value="seeAll">
        {{ $t(seeAll ? 'settings.cron.allPeriodicQueries' : 'settings.cron.myPeriodicQueries') }}
        <BTooltip target="seeAllPeriodicQueries">
          {{ $t(seeAll ? 'settings.cron.allPeriodicQueriesTip' : 'settings.cron.myPeriodicQueriesTip') }}
        </BTooltip>
      </BFormCheckbox>
      <b-button
        size="sm"
        variant="success"
        class="pull-right d-inline"
        @click="showCronModal = true">
        <span class="fa fa-plus-circle me-1" />
        {{ $t('settings.cron.newPeriodicQuery') }}
      </b-button>
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
    <b-modal
      size="xl"
      :model-value="showCronModal"
      @hidden="showCronModal = false"
      title="Create New Periodic Query">
      <!-- create form -->
      <b-form>
        <div class="row mb-2">
          <div class="col-md-4">
            <b-input-group size="sm">
              <template #prepend>
                <b-input-group-text
                  id="newCronQueryName"
                  class="cursor-help">
                  {{ $t('settings.cron.queryName') }}<sup>*</sup>
                </b-input-group-text>
              </template>
              <b-form-input
                maxlength="20"
                :model-value="newCronQueryName"
                @update:model-value="newCronQueryName = $event"
                :placeholder="$t('settings.cron.queryNamePlaceholder')" />
            </b-input-group>
          </div>
          <div class="col-md-4">
            <b-input-group size="sm">
              <template #prepend>
                <b-input-group-text
                  class="cursor-help"
                  id="newCronQueryAction">
                  {{ $t('settings.cron.queryAction') }}<sup>*</sup>
                  <BTooltip target="newCronQueryAction">
                    {{ $t('settings.cron.queryActionTip') }}
                  </BTooltip>
                </b-input-group-text>
              </template>
              <BFormSelect
                :model-value="newCronQueryAction"
                @update:model-value="newCronQueryAction = $event"
                class="form-control form-control-sm">
                <option value="tag">
                  {{ $t('settings.cron.queryAction-tag') }}
                </option>
                <option
                  v-for="(cluster, key) in clusters"
                  :key="key"
                  :value="`forward:${key}`">
                  {{ $t('settings.cron.queryAction-forward', { cluster: cluster.name }) }}
                </option>
              </BFormSelect>
            </b-input-group>
          </div>
          <div class="col-md-4">
            <b-input-group size="sm">
              <template #prepend>
                <b-input-group-text
                  class="cursor-help"
                  id="newCronQueryTags">
                  {{ $t('settings.cron.tags') }}<sup>*</sup>
                  <BTooltip target="newCronQueryTags">
                    {{ $t('settings.cron.tagsTip') }}
                  </BTooltip>
                </b-input-group-text>
              </template>
              <b-form-input
                :model-value="newCronQueryTags"
                @update:model-value="newCronQueryTags = $event"
                :placeholder="$t('settings.cron.tagsPlaceholder')" />
            </b-input-group>
          </div>
        </div>
        <div class="row mb-2">
          <div class="col">
            <b-input-group size="sm">
              <template #prepend>
                <b-input-group-text
                  class="cursor-help"
                  id="newCronQueryExpression">
                  {{ $t('settings.cron.searchExpression') }}<sup>*</sup>
                  <BTooltip target="newCronQueryExpression">
                    {{ $t('settings.cron.searchExpressionTip') }}
                  </BTooltip>
                </b-input-group-text>
              </template>
              <b-form-input
                :model-value="newCronQueryExpression"
                @update:model-value="newCronQueryExpression = $event"
                :placeholder="$t('settings.cron.searchExpressionPlaceholder')" />
            </b-input-group>
          </div>
        </div>
        <div class="row mb-2">
          <div class="col-md-6">
            <b-input-group size="sm">
              <template #prepend>
                <b-input-group-text
                  class="cursor-help"
                  id="newCronQueryProcess">
                  {{ $t('settings.cron.querySince') }}<sup>*</sup>
                  <BTooltip target="newCronQueryProcess">
                    {{ $t('settings.cron.querySinceTip') }}
                  </BTooltip>
                </b-input-group-text>
              </template>
              <b-form-select
                class="form-control"
                :model-value="newCronQueryProcess"
                @update:model-value="newCronQueryProcess = $event"
                :options="[
                  { value: 0, text: 'Now' },
                  { value: 1, text: $t('common.hourAgoCount', 1) },
                  { value: 6, text: $t('common.hourAgoCount', 6) },
                  { value: 24, text: $t('common.hourAgoCount', 24) },
                  { value: 48, text: $t('common.hourAgoCount', 48) },
                  { value: 72, text: $t('common.hourAgoCount', 72) },
                  { value: 168, text: $t('common.weekAgoCount', 1) },
                  { value: 336, text: $t('common.weekAgoCount', 2) },
                  { value: 720, text: $t('common.monthAgoCount', 1) },
                  { value: 1440, text: $t('common.monthAgoCount', 2) },
                  { value: 4380, text: $t('common.monthAgoCount', 6) },
                  { value: 8760, text: $t('common.yearAgoCount', 1) },
                  { value: -1, text: $t('common.allCareful') }
                ]" />
            </b-input-group>
          </div>
          <div class="col-md-6">
            <b-input-group size="sm">
              <template #prepend>
                <b-input-group-text
                  class="cursor-help"
                  id="newCronQueryNotifier">
                  {{ $t('settings.cron.queryNotify') }}
                  <BTooltip target="newCronQueryNotifier">
                    {{ $t('settings.cron.queryNotifyTip') }}
                  </BTooltip>
                </b-input-group-text>
              </template>
              <BFormSelect
                :model-value="newCronQueryNotifier"
                @update:model-value="newCronQueryNotifier = $event"
                class="form-control form-control-sm">
                <option value="undefined">
                  none
                </option>
                <option
                  v-for="notifier in notifiers"
                  :key="notifier.id"
                  :value="notifier.id">
                  {{ notifier.name }} ({{ notifier.type }})
                </option>
              </BFormSelect>
            </b-input-group>
          </div>
        </div>
        <div class="row mb-2">
          <div class="col">
            <b-input-group size="sm">
              <template #prepend>
                <b-input-group-text
                  class="cursor-help"
                  id="newCronQueryDescription">
                  {{ $t('settings.cron.queryDescription') }}
                  <BTooltip target="newCronQueryDescription">
                    {{ $t('settings.cron.queryDescriptionTip') }}
                  </BTooltip>
                </b-input-group-text>
              </template>
              <b-form-textarea
                :model-value="newCronQueryDescription"
                @update:model-value="newCronQueryDescription = $event"
                :placeholder="$t('settings.cron.queryDescriptionPlaceholder')" />
            </b-input-group>
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
          <b-input-group
            size="sm"
            class="flex-grow-1">
            <template #prepend>
              <b-input-group-text
                class="cursor-help"
                id="newCronQueryUsers">
                {{ $t('common.shareWithUsers') }}
                <BTooltip target="newCronQueryUsers">
                  {{ $t('settings.cron.shareWithUsersTip') }}
                </BTooltip>
              </b-input-group-text>
            </template>
            <b-form-input
              :model-value="newCronQueryUsers"
              @update:model-value="newCronQueryUsers = $event"
              :placeholder="$t('settings.cron.shareWithUsersPlaceholder')" />
          </b-input-group>
        </div>
      </b-form> <!-- /create form -->
      <!-- create error -->
      <div
        v-if="cronQueryFormError"
        style="z-index: 2000;"
        class="mt-2 mb-0 alert alert-danger">
        <span class="fa fa-exclamation-triangle me-1" />
        {{ cronQueryFormError }}
      </div> <!-- /create error -->
      <template #footer>
        <div class="w-100 d-flex justify-content-between">
          <b-button
            :title="$t('common.cancel')"
            variant="danger"
            @click="showCronModal = false">
            <span class="fa fa-times" />
            {{ $t('common.cancel') }}
          </b-button>
          <b-button
            variant="success"
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
          </b-button>
        </div>
      </template> <!-- /modal footer -->
    </b-modal> <!-- /new cron query form -->

    <!-- cron queries -->
    <b-card-group
      columns
      class="mb-2">
      <b-card
        :key="query.key"
        @keyup.esc="getCronQueries"
        v-for="(query, index) in cronQueries"
        @keyup.enter="updateCronQuery(query, index)">
        <template #header>
          <h6 class="mb-0 d-flex">
            <b-input-group
              size="sm"
              class="flex-grow-1">
              <template #prepend>
                <b-input-group-text>
                  {{ $t('settings.cron.queryName') }}<sup>*</sup>
                </b-input-group-text>
              </template>
              <b-form-input
                maxlength="20"
                :model-value="query.name"
                @update:model-value="query.name = $event; cronQueryChanged(query)"
                :disabled="!canEditCronQuery(query)" />
            </b-input-group>
            <div class="ms-2 mt-1">
              <BFormCheckbox
                class="pull-right"
                :model-value="query.enabled"
                :id="`queryEnabled${index}`"
                :disabled="!canEditCronQuery(query)"
                @update:model-value="query.enabled = $event; toggleCronQueryEnabled(index)">
                <BTooltip :target="`queryEnabled${index}`">
                  {{ query.enabled ? 'Enabled' : 'Disabled' }}
                </BTooltip>
              </BFormCheckbox>
            </div>
          </h6>
        </template>
        <b-card-text>
          <b-input-group
            size="sm"
            class="mb-2">
            <template #prepend>
              <b-input-group-text
                :id="`queryDescription${index}`"
                class="cursor-help">
                {{ $t('settings.cron.queryDescription') }}
                <BTooltip :target="`queryDescription${index}`">
                  {{ $t('settings.cron.queryDescriptionTip') }}
                </BTooltip>
              </b-input-group-text>
            </template>
            <b-form-textarea
              :model-value="query.description"
              @update:model-value="query.description = $event; cronQueryChanged(query)"
              class="form-control form-control-sm"
              :disabled="!canEditCronQuery(query)" />
          </b-input-group>
          <b-input-group
            size="sm"
            class="mb-2">
            <template #prepend>
              <b-input-group-text
                :id="`queryAction${index}`"
                class="cursor-help">
                {{ $t('settings.cron.queryAction') }}<sup>*</sup>
                <BTooltip :target="`queryAction${index}`">
                  Action to perform when a session matches this query
                </BTooltip>
              </b-input-group-text>
            </template>
            <BFormSelect
              :model-value="query.action"
              @update:model-value="query.action = $event; cronQueryChanged(query)"
              class="form-control form-control-sm"
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
            </BFormSelect>
          </b-input-group>
          <b-input-group
            size="sm"
            class="mb-2">
            <template #prepend>
              <b-input-group-text
                class="cursor-help"
                :id="`queryTags${index}`">
                {{ $t('settings.cron.tags') }}<sup>*</sup>
                <BTooltip :target="`queryTags${index}`">
                  {{ $t('settings.cron.tagsTip') }}
                </BTooltip>
              </b-input-group-text>
            </template>
            <b-form-input
              :model-value="query.tags"
              @update:model-value="query.tags = $event; cronQueryChanged(query)"
              :disabled="!canEditCronQuery(query)" />
          </b-input-group>
          <b-input-group
            size="sm"
            class="mb-2">
            <template #prepend>
              <b-input-group-text
                class="cursor-help"
                :id="`queryExpression${index}`">
                {{ $t('settings.cron.searchExpression') }}<sup>*</sup>
                <BTooltip :target="`queryExpression${index}`">
                  {{ $t('settings.cron.searchExpressionTip') }}
                </BTooltip>
              </b-input-group-text>
            </template>
            <b-form-input
              :model-value="query.query"
              @update:model-value="query.query = $event; cronQueryChanged(query)"
              :disabled="!canEditCronQuery(query)" />
          </b-input-group>
          <b-input-group
            size="sm"
            class="mb-2">
            <template #prepend>
              <b-input-group-text
                class="cursor-help"
                :id="`queryProcess${index}`">
                {{ $t('settings.cron.queryNotify') }}
                <BTooltip :target="`queryProcess${index}`">
                  {{ $t('settings.cron.queryNotifyTip') }}
                </BTooltip>
              </b-input-group-text>
            </template>
            <BFormSelect
              :model-value="query.notifier"
              @update:model-value="query.notifier = $event; cronQueryChanged(query)"
              class="form-control form-control-sm"
              :disabled="!canEditCronQuery(query)">
              <option value="undefined">
                none
              </option>
              <option
                v-for="notifier in notifiers"
                :key="notifier.id"
                :value="notifier.id">
                {{ notifier.name }} ({{ notifier.type }})
              </option>
            </BFormSelect>
          </b-input-group>
          <b-input-group
            size="sm"
            class="mb-2"
            v-if="canEditCronQuery(query)">
            <template #prepend>
              <b-input-group-text
                class="cursor-help"
                :id="`queryUsers${index}`">
                {{ $t('common.shareWithUsers') }}
                <BTooltip :target="`queryUsers${index}`">
                  {{ $t('settings.cron.shareWithUsersTip') }}
                </BTooltip>
              </b-input-group-text>
            </template>
            <b-form-input
              :model-value="query.users"
              @update:model-value="query.users = $event; cronQueryChanged(query)" />
          </b-input-group>
          <div
            class="mb-2"
            v-if="canEditCronQuery(query)">
            <RoleDropdown
              :roles="roles"
              :id="query.key"
              class="d-inline"
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
        </b-card-text>
        <b-card-text>
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
        </b-card-text>
        <template #footer>
          <b-button
            size="sm"
            variant="warning"
            :id="`openMatches${index}`"
            @click="openCronSessions(query)">
            <span class="fa fa-folder-open fa-fw me-1" />
            {{ $t('settings.cron.openMatches') }}
            <BTooltip :target="`openMatches${index}`">
              {{ $t('settings.cron.openMatchesTip') }}
            </BTooltip>
          </b-button>
          <template v-if="canEditCronQuery(query)">
            <template v-if="query.changed">
              <b-button
                size="sm"
                class="ms-1"
                variant="warning"
                @click="getCronQueries"
                :id="`cancel${index}`">
                <span class="fa fa-ban fa-fw me-1" />
                {{ $t('common.cancel') }}
                <BTooltip :target="`cancel${index}`">
                  {{ $t('settings.cron.cancelTip') }}
                </BTooltip>
              </b-button>
              <b-button
                size="sm"
                class="pull-right ms-1"
                variant="theme-tertiary"
                :id="`save${index}`"
                @click="updateCronQuery(query, index)">
                <span class="fa fa-save fa-fw me-1" />
                {{ $t('common.save') }}
                <BTooltip :target="`save${index}`">
                  {{ $t('settings.cron.saveTip') }}
                </BTooltip>
              </b-button>
            </template>
            <template v-else>
              <b-button
                size="sm"
                variant="danger"
                class="pull-right ms-1"
                :id="`delete${index}`"
                @click="deleteCronQuery(query, index)">
                <span class="fa fa-trash-o fa-fw me-1" />
                {{ $t('common.delete') }}
                <BTooltip :target="`delete${index}`">
                  {{ $t('settings.cron.deleteTip') }}
                </BTooltip>
              </b-button>
              <b-button
                size="sm"
                variant="info"
                class="ms-1"
                v-if="canTransfer(query)"
                :id="`transfer${index}`"
                @click="openTransferQuery(query)">
                <span class="fa fa-share fa-fw" />
                <BTooltip :target="`transfer${index}`">
                  {{ $t('settings.cron.transferTip') }}
                </BTooltip>
              </b-button>
            </template>
          </template>
        </template>
      </b-card>
    </b-card-group> <!-- /cron queries -->

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
import TransferResource from '@common/TransferResource.vue';
// utils
import { timezoneDateString } from '@common/vueFilters.js';

export default {
  name: 'PeriodicQueries',
  emits: ['display-message'],
  components: {
    RoleDropdown,
    TransferResource
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
      newCronQueryNotifier: undefined,
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
        this.cronQueryFormError = this.$t('settings.cron.queryNameRequired');
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

      if (this.newCronQueryNotifier) {
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
        let msg = response.text || 'Successfully created periodic query.';
        if (response.invalidUsers && response.invalidUsers.length) {
          msg += ` Could not add these users: ${response.invalidUsers.join(',')}`;
        }
        this.$emit('display-message', { msg });
      }).catch((error) => {
        // display error message to user
        this.cronLoading = false;
        this.cronQueryFormError = error.text;
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
        this.$emit('display-message', { msg: response.text });
      }).catch((error) => {
        // display error message to user
        this.$emit('display-message', { msg: error.text, type: 'danger' });
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
        this.$emit('display-message', { msg: response.text });
        this.showTransferModal = false;
      }).catch((error) => {
        // display error message to user
        this.$emit('display-message', { msg: error.text, type: 'danger' });
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
        let msg = response.text || 'Successfully updated periodic query.';
        if (response.invalidUsers && response.invalidUsers.length) {
          msg += ` Could not add these users: ${response.invalidUsers.join(',')}`;
        }
        this.$emit('display-message', { msg });
      }).catch((error) => {
        // display error message to user
        this.$emit('display-message', { msg: error.text, type: 'danger' });
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
        this.cronQueryListError = error.text;
      });
    },
    // HELPER FUNCTIONS ---------------------------------------------------- //
    clearNewFormInputs () {
      this.cronQueryFormError = false;
      this.newCronQueryName = '';
      this.newCronQueryTags = '';
      this.newCronQueryUsers = '';
      this.newCronQueryRoles = [];
      this.newCronQueryEditRoles = [];
      this.newCronQueryExpression = '';
      this.newCronQueryDescription = '';
      this.newCronQueryNotifier = undefined;
    }
  }
};
</script>
