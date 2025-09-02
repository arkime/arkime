<!--
Copyright Yahoo Inc.
SPDX-License-Identifier: Apache-2.0
-->
<template>
   <div>

    <h3>
      Periodic Queries
      <BFormCheckbox
        inline
        button
        size="sm"
        class="ms-1"
        v-if="user.roles.includes('arkimeAdmin')"
        id="seeAllPeriodicQueries"
        @update:model-value="updateSeeAll"
        :model-value="seeAll">
        See {{ seeAll ? ' MY ' : ' ALL ' }} Periodic Queries
        <BTooltip target="seeAllPeriodicQueries">
          {{ seeAll ? 'Just show the periodic queries created by you and shared with you' : 'See all the periodic queries that exist for all users (you can because you are an ADMIN!)' }}
        </BTooltip>
      </BFormCheckbox>
      <b-button
        size="sm"
        variant="success"
        class="pull-right d-inline"
        @click="showCronModal = true">
        <span class="fa fa-plus-circle me-1" />
        New Periodic Query
      </b-button>
    </h3>

    <p>
      Run Arkime queries periodically that can perform actions on matching sessions.
      The query runs a search against sessions delayed by 90 seconds to make sure all
      updates have been completed for that session.
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
    <div class="text-center mt-4"
      v-if="!cronQueries || !cronQueries.length">
      <h3>
        <span class="fa fa-folder-open fa-2x" />
      </h3>
      <h5>
        No periodic queries have been created.
        <br>
        Click the create button above to create one!
      </h5>
    </div> <!-- /no results -->

    <!-- new cron query form -->
    <b-modal
      size="xl"
      :model-value="showCronModal"
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
                  Query Name<sup>*</sup>
                  <BTooltip target="newCronQueryName">
                    Name of the periodic query (20 characters or less)
                  </BTooltip>
                </b-input-group-text>
              </template>
              <b-form-input
                maxlength="20"
                :model-value="newCronQueryName"
                @update:model-value="newCronQueryName = $event"
                placeholder="Periodic query name (20 chars or less)"
              />
            </b-input-group>
          </div>
          <div class="col-md-4">
            <b-input-group size="sm">
              <template #prepend>
                <b-input-group-text
                  class="cursor-help"
                  id="newCronQueryAction">
                  Query Action<sup>*</sup>
                  <BTooltip target="newCronQueryAction">
                    Action to perform when a session matches this query
                  </BTooltip>
                </b-input-group-text>
              </template>
              <BFormSelect
                :model-value="newCronQueryAction"
                @update:model-value="newCronQueryAction = $event"
                class="form-control form-control-sm">
                <option value="tag">Tag</option>
                <option v-for="(cluster, key) in clusters"
                  :key="key"
                  :value="`forward:${key}`">
                  Tag & Export to {{ cluster.name }}
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
                  Tags<sup>*</sup>
                  <BTooltip target="newCronQueryTags">
                    Comma separated list of tags to add to the sessions that match this query
                  </BTooltip>
                </b-input-group-text>
              </template>
              <b-form-input
                :model-value="newCronQueryTags"
                @update:model-value="newCronQueryTags = $event"
                placeholder="Comma separated list of tags"
              />
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
                  Search Expression<sup>*</sup>
                  <BTooltip target="newCronQueryExpression">
                    Sessions search expression
                  </BTooltip>
                </b-input-group-text>
              </template>
              <b-form-input
                :model-value="newCronQueryExpression"
                @update:model-value="newCronQueryExpression = $event"
                placeholder="Periodic query expression"
              />
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
                  Process Query Since<sup>*</sup>
                  <BTooltip target="newCronQueryProcess">
                    Enter the time period to process the query
                  </BTooltip>
                </b-input-group-text>
              </template>
              <b-form-select
                class="form-control"
                :model-value="newCronQueryProcess"
                @update:model-value="newCronQueryProcess = $event"
                :options="[
                  { value: 0, text: 'Now' },
                  { value: 1, text: '1 hour ago' },
                  { value: 6, text: '6 hours ago' },
                  { value: 24, text: '24 hours ago' },
                  { value: 48, text: '48 hours ago' },
                  { value: 72, text: '72 hours ago' },
                  { value: 168, text: '1 week ago' },
                  { value: 336, text: '2 weeks ago' },
                  { value: 720, text: '1 month ago' },
                  { value: 1440, text: '2 months ago' },
                  { value: 4380, text: '6 months ago' },
                  { value: 8760, text: '1 year ago' },
                  { value: -1, text: 'All (careful)' }
                ]"
              />
            </b-input-group>
          </div>
          <div class="col-md-6">
            <b-input-group size="sm">
              <template #prepend>
                <b-input-group-text
                  class="cursor-help"
                  id="newCronQueryNotifier">
                  Notify
                  <BTooltip target="newCronQueryNotifier">
                    Send a notification when there are matches to this periodic query (optional)
                  </BTooltip>
                </b-input-group-text>
              </template>
              <BFormSelect
                :model-value="newCronQueryNotifier"
                @update:model-value="newCronQueryNotifier = $event"
                class="form-control form-control-sm">
                <option value=undefined>none</option>
                <option v-for="notifier in notifiers"
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
                  Description
                  <BTooltip target="newCronQueryDescription">
                    Enter a description to explain the reason for this query (optional)
                  </BTooltip>
                </b-input-group-text>
              </template>
              <b-form-textarea
                :model-value="newCronQueryDescription"
                @update:model-value="newCronQueryDescription = $event"
                placeholder="Periodic query description"
              />
            </b-input-group>
          </div>
        </div>
        <div class="d-flex">
           <div class="me-3 flex-grow-1 no-wrap">
            <RoleDropdown
              :roles="roles"
              class="d-inline"
              display-text="Who can view"
              :selected-roles="newCronQueryRoles"
              @selected-roles-updated="updateNewCronQueryRoles"
            />
            <RoleDropdown
              :roles="roles"
              class="d-inline ms-1"
              display-text="Who can edit"
              :selected-roles="newCronQueryEditRoles"
              @selected-roles-updated="updateNewCronQueryEditRoles"
            />
          </div>
          <b-input-group
            size="sm"
            class="flex-grow-1">
            <template #prepend>
              <b-input-group-text
                class="cursor-help"
                id="newCronQueryUsers">
                Share with users
                <BTooltip target="newCronQueryUsers">
                  Comma separated list of users that can view this periodic query (optional)
                </BTooltip>
              </b-input-group-text>
            </template>
            <b-form-input
              :model-value="newCronQueryUsers"
              @update:model-value="newCronQueryUsers = $event"
              placeholder="Comma separated list of users"
            />
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
            title="Cancel"
            variant="danger"
            @click="showCronModal = false">
            <span class="fa fa-times" />
            Cancel
          </b-button>
          <b-button
            variant="success"
            :disabled="cronLoading"
            @click="createCronQuery"
            :class="{'disabled':cronLoading}">
            <template v-if="!cronLoading">
              <span class="fa fa-plus-circle me-1" />
              Create
            </template>
            <template v-else>
              <span class="fa fa-spinner fa-spin me-1" />
              Creating
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
                  Query Name<sup>*</sup>
                </b-input-group-text>
              </template>
              <b-form-input
                maxlength="20"
                :model-value="query.name"
                @update:model-value="query.name = $event; cronQueryChanged(query)"
                :disabled="!canEditCronQuery(query)"
              />
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
                Description
                <BTooltip :target="`queryDescription${index}`">
                  Enter a description to explain the reason for this query (optional)
                </BTooltip>
              </b-input-group-text>
            </template>
            <b-form-textarea
              :model-value="query.description"
              @update:model-value="query.description = $event; cronQueryChanged(query)"
              class="form-control form-control-sm"
              :disabled="!canEditCronQuery(query)"
            />
          </b-input-group>
          <b-input-group
            size="sm"
            class="mb-2">
            <template #prepend>
              <b-input-group-text
                :id="`queryAction${index}`"
                class="cursor-help">
                Query Action<sup>*</sup>
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
              <option value="tag">Tag</option>
              <option v-for="(cluster, key) in clusters"
                :value="`forward:${key}`"
                :key="key">
                Tag & Export to {{ cluster.name }}
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
                Tags<sup>*</sup>
                <BTooltip :target="`queryTags${index}`">
                  Comma separated list of tags to add to the sessions that match this query
                </BTooltip>
              </b-input-group-text>
            </template>
            <b-form-input
              :model-value="query.tags"
              @update:model-value="query.tags = $event; cronQueryChanged(query)"
              :disabled="!canEditCronQuery(query)"
            />
          </b-input-group>
          <b-input-group
            size="sm"
            class="mb-2">
            <template #prepend>
              <b-input-group-text
                class="cursor-help"
                :id="`queryExpression${index}`">
                Search Expression<sup>*</sup>
                <BTooltip :target="`queryExpression${index}`">
                  Sessions search expression
                </BTooltip>
              </b-input-group-text>
            </template>
            <b-form-input
              :model-value="query.query"
              @update:model-value="query.query = $event; cronQueryChanged(query)"
              :disabled="!canEditCronQuery(query)"
            />
          </b-input-group>
          <b-input-group
            size="sm"
            class="mb-2">
            <template #prepend>
              <b-input-group-text
                class="cursor-help"
                :id="`queryProcess${index}`">
                Notify
                <BTooltip :target="`queryProcess${index}`">
                  Send a notification when there are matches to this periodic query (optional)
                </BTooltip>
              </b-input-group-text>
            </template>
            <BFormSelect
              :model-value="query.notifier"
              @update:model-value="query.notifier = $event; cronQueryChanged(query)"
              class="form-control form-control-sm"
              :disabled="!canEditCronQuery(query)">
              <option value=undefined>none</option>
              <option v-for="notifier in notifiers"
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
                Share with users
                <BTooltip :target="`queryUsers${index}`">
                  Comma separated list of users that can view this periodic query (optional)
                </BTooltip>
              </b-input-group-text>
            </template>
            <b-form-input
              :model-value="query.users"
              @update:model-value="query.users = $event; cronQueryChanged(query)"
            />
          </b-input-group>
          <div class="mb-2"
            v-if="canEditCronQuery(query)">
            <RoleDropdown
              :roles="roles"
              :id="query.key"
              class="d-inline"
              tooltip="Who can view"
              :selected-roles="query.roles"
              @selected-roles-updated="updateCronQueryRoles"
              :display-text="query.roles && query.roles.length ? undefined : 'Who can view'"
            />
            <RoleDropdown
              :roles="roles"
              :id="query.key"
              tooltip="Who can edit"
              class="d-inline ms-1"
              :selected-roles="query.editRoles"
              @selected-roles-updated="updateCronQueryEditRoles"
              :display-text="query.editRoles && query.editRoles.length ? undefined : 'Who can edit'"
            />
          </div>
        </b-card-text>
        <b-card-text>
          <div class="row">
            <div class="col">
              <strong>Matches</strong>:
              {{ query.count }}
            </div>
          </div>
          <div class="row"
            v-if="query.creator">
            <div class="col">
              <strong>Created by</strong>:
              {{ query.creator }}
            </div>
          </div>
          <div class="row"
            v-if="query.created">
            <div class="col">
              <strong>Created at</strong>:
              {{ timezoneDateString(query.created * 1000, user.settings.timezone, false) }}
            </div>
          </div>
          <div class="row"
            v-if="query.lastRun">
            <div class="col">
              <strong>Last run at</strong>:
              {{ timezoneDateString(query.lastRun * 1000, user.settings.timezone, false) }}
              and matched {{ query.lastCount || 0 }} new sessions
            </div>
          </div>
          <div class="row"
            v-if="query.lastToggled">
            <div class="col">
              <strong>{{ query.enabled ? 'Enabled' : 'Disabled'}} at</strong>:
              {{ timezoneDateString(query.lastToggled * 1000, user.settings.timezone, false) }}
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
            Open Matches
            <BTooltip :target="`openMatches${index}`">Open sessions that this query tagged in the last hour.</BTooltip>
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
                Cancel
                <BTooltip :target="`cancel${index}`">Undo changes to this query</BTooltip>
              </b-button>
              <b-button
                size="sm"
                class="pull-right ms-1"
                variant="theme-tertiary"
                :id="`save${index}`"
                @click="updateCronQuery(query, index)">
                <span class="fa fa-save fa-fw me-1" />
                Save
                <BTooltip :target="`save${index}`">Save changes to this query</BTooltip>
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
                Delete
                <BTooltip :target="`delete${index}`">Delete this query</BTooltip>
              </b-button>
              <b-button
                size="sm"
                variant="info"
                class="ms-1"
                v-if="canTransfer(query)"
                :id="`transfer${index}`"
                @click="openTransferQuery(query)">
                <span class="fa fa-share fa-fw" />
                <BTooltip :target="`transfer${index}`">Transfer this query to another user</BTooltip>
              </b-button>
            </template>
          </template>
        </template>
      </b-card>
    </b-card-group> <!-- /cron queries -->

    <transfer-resource
      @transfer-resource="submitTransferQuery"
    />

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
  components: {
    RoleDropdown,
    TransferResource
  },
  props: {
    userId: String // the setting user id
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
      showCronModal: false
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
        this.cronQueryFormError = 'No query name specified.';
        return;
      }

      if (!this.newCronQueryExpression || this.newCronQueryExpression === '') {
        this.cronQueryFormError = 'No query expression specified.';
        return;
      }

      if (!this.newCronQueryTags || this.newCronQueryTags === '') {
        this.cronQueryFormError = 'No query tags specified.';
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
        this.$emit('display-message', { msg: 'This query has not tagged any sessions', type: 'danger' });
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
      this.$bvModal.show('transfer-modal');
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
        this.$bvModal.hide('transfer-modal');
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
