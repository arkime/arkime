<template>
   <form class="form-horizontal">

    <h3>
      Periodic Queries
      <b-button
        size="sm"
        variant="success"
        class="pull-right"
        v-b-modal.create-periodic-query-modal>
        <span class="fa fa-plus-circle mr-1" />
        New Periodic Query
      </b-button>
    </h3>

    <p>
      Run Arkime queries periodically that can perform actions on matching sessions.
      The query runs a search against sessions delayed by 90 seconds to make sure all
      updates have been completed for that session.
    </p>

    <hr>

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

    <!-- cron query list error -->
    <b-alert
      variant="danger"
      class="mt-2 mb-0"
      :show="!!cronQueryListError">
      <span class="fa fa-exclamation-triangle mr-1" />
      {{ cronQueryListError }}
    </b-alert> <!-- /cron query list error -->

    <!-- new cron query form -->
    <b-modal
      size="xl"
      id="create-periodic-query-modal"
      title="Create New Periodic Query">
      <!-- create form -->
      <b-form>
        <div class="row mb-2">
          <div class="col-md-4">
            <b-input-group size="sm">
              <template #prepend>
                <b-input-group-text
                  v-b-tooltip.hover
                  class="cursor-help"
                  title="Enter a query name (20 chars or less)">
                  Query Name<sup>*</sup>
                </b-input-group-text>
              </template>
              <b-form-input
                maxlength="20"
                v-model="newCronQueryName"
                placeholder="Periodic query name (20 chars or less)"
              />
            </b-input-group>
          </div>
          <div class="col-md-4">
            <b-input-group size="sm">
              <template #prepend>
                <b-input-group-text
                  v-b-tooltip.hover
                  class="cursor-help"
                  title="Action to perform when a session matches this query">
                  Query Action<sup>*</sup>
                </b-input-group-text>
              </template>
              <select
                v-model="newCronQueryAction"
                class="form-control form-control-sm">
                <option value="tag">Tag</option>
                <option v-for="(cluster, key) in clusters"
                  :key="key"
                  :value="`forward:${key}`">
                  Tag & Export to {{ cluster.name }}
                </option>
              </select>
            </b-input-group>
          </div>
          <div class="col-md-4">
            <b-input-group size="sm">
              <template #prepend>
                <b-input-group-text
                  v-b-tooltip.hover
                  class="cursor-help"
                  title="Enter a comma separated list of tags to add to the sessions that match this query">
                  Tags<sup>*</sup>
                </b-input-group-text>
              </template>
              <b-form-input
                v-model="newCronQueryTags"
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
                  v-b-tooltip.hover
                  class="cursor-help"
                  title="Enter a sessions search expression">
                  Search Expression<sup>*</sup>
                </b-input-group-text>
              </template>
              <b-form-input
                v-model="newCronQueryExpression"
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
                  v-b-tooltip.hover
                  class="cursor-help"
                  title="Start processing query since">
                  Process Query Since<sup>*</sup>
                </b-input-group-text>
              </template>
              <b-form-select
                class="form-control"
                v-model="newCronQueryProcess"
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
                  v-b-tooltip.hover
                  class="cursor-help"
                  title="Send a notification when there are matches to this periodic query">
                  Notify
                </b-input-group-text>
              </template>
              <select
                v-model="newCronQueryNotifier"
                class="form-control form-control-sm">
                <option value=undefined>none</option>
                <option v-for="notifier in notifiers"
                  :key="notifier.id"
                  :value="notifier.id">
                  {{ notifier.name }} ({{ notifier.type }})
                </option>
              </select>
            </b-input-group>
          </div>
        </div>
        <div class="row mb-2">
          <div class="col">
            <b-input-group size="sm">
              <template #prepend>
                <b-input-group-text
                  v-b-tooltip.hover
                  class="cursor-help"
                  title="Enter an optional description to explain the reason for this query">
                  Description
                </b-input-group-text>
              </template>
              <b-form-textarea
                v-model="newCronQueryDescription"
                placeholder="Periodic query description"
              />
            </b-input-group>
          </div>
        </div>
        <div class="d-flex">
          <div class="mr-3">
            <RoleDropdown
              :roles="roles"
              display-text="Share with roles"
              :selected-roles="newCronQueryRoles"
              @selected-roles-updated="updateNewCronQueryRoles"
            />
          </div>
          <b-input-group
            size="sm"
            class="flex-grow-1">
            <template #prepend>
              <b-input-group-text
                v-b-tooltip.hover
                class="cursor-help"
                title="Enter a comma separated list of users that can view this periodic query">
                Share with users
              </b-input-group-text>
            </template>
            <b-form-input
              v-model="newCronQueryUsers"
              placeholder="Comma separated list of users"
            />
          </b-input-group>
        </div>
      </b-form> <!-- /create form -->
      <!-- create form error -->
      <b-alert
        variant="danger"
        class="mt-2 mb-0"
        :show="!!cronQueryFormError">
        <span class="fa fa-exclamation-triangle mr-1" />
        {{ cronQueryFormError }}
      </b-alert> <!-- /create form error -->
      <template #modal-footer>
        <div class="w-100 d-flex justify-content-between">
          <b-button
            title="Cancel"
            variant="danger"
            @click="$bvModal.hide('create-periodic-query-modal')">
            <span class="fa fa-times" />
            Cancel
          </b-button>
          <b-button
            variant="success"
            v-b-tooltip.hover
            :disabled="cronLoading"
            @click="createCronQuery"
            :class="{'disabled':cronLoading}"
            title="Create new periodic query">
            <template v-if="!cronLoading">
              <span class="fa fa-plus-circle mr-1" />
              Create
            </template>
            <template v-else>
              <span class="fa fa-spinner fa-spin mr-1" />
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
                v-model="query.name"
                @input="cronQueryChanged(query)"
                :disabled="!canEditCronQuery(query)"
              />
            </b-input-group>
            <div class="ml-2 mt-1">
              <b-form-checkbox
                v-b-tooltip.hover
                class="pull-right"
                v-model="query.enabled"
                :disabled="!canEditCronQuery(query)"
                @input="toggleCronQueryEnabled(index)"
                :title="query.enabled ? 'Enabled' : 'Disabled'"
              />
            </div>
          </h6>
        </template>
        <b-card-text>
          <b-input-group
            size="sm"
            class="mb-2">
            <template #prepend>
              <b-input-group-text
                v-b-tooltip.hover
                class="cursor-help"
                title="Enter an optional description to explain the reason for this query">
                Description
              </b-input-group-text>
            </template>
            <textarea
              v-model="query.description"
              @input="cronQueryChanged(query)"
              class="form-control form-control-sm"
              :disabled="!canEditCronQuery(query)"
            />
          </b-input-group>
          <b-input-group
            size="sm"
            class="mb-2">
            <template #prepend>
              <b-input-group-text
                v-b-tooltip.hover
                class="cursor-help"
                title="Action to perform when a session matches this query">
                Query Action<sup>*</sup>
              </b-input-group-text>
            </template>
            <select
              v-model="query.action"
              @change="cronQueryChanged(query)"
              class="form-control form-control-sm"
              :disabled="!canEditCronQuery(query)">
              <option value="tag">Tag</option>
              <option v-for="(cluster, key) in clusters"
                :value="`forward:${key}`"
                :key="key">
                Tag & Export to {{ cluster.name }}
              </option>
            </select>
          </b-input-group>
          <b-input-group
            size="sm"
            class="mb-2">
            <template #prepend>
              <b-input-group-text
                v-b-tooltip.hover
                class="cursor-help"
                title="Enter a comma separated list of tags to add to the sessions that match this query">
                Tags<sup>*</sup>
              </b-input-group-text>
            </template>
            <b-form-input
              v-model="query.tags"
              @input="cronQueryChanged(query)"
              :disabled="!canEditCronQuery(query)"
            />
          </b-input-group>
          <b-input-group
            size="sm"
            class="mb-2">
            <template #prepend>
              <b-input-group-text
                v-b-tooltip.hover
                class="cursor-help"
                title="Enter a sessions search expression">
                Search Expression<sup>*</sup>
              </b-input-group-text>
            </template>
            <b-form-input
              v-model="query.query"
              @input="cronQueryChanged(query)"
              :disabled="!canEditCronQuery(query)"
            />
          </b-input-group>
          <b-input-group
            size="sm"
            class="mb-2">
            <template #prepend>
              <b-input-group-text
                v-b-tooltip.hover
                class="cursor-help"
                title="Send a notification when there are matches to this periodic query">
                Notify
              </b-input-group-text>
            </template>
            <select
              v-model="query.notifier"
              @change="cronQueryChanged(query)"
              class="form-control form-control-sm"
              :disabled="!canEditCronQuery(query)">
              <option value=undefined>none</option>
              <option v-for="notifier in notifiers"
                :key="notifier.id"
                :value="notifier.id">
                {{ notifier.name }} ({{ notifier.type }})
              </option>
            </select>
          </b-input-group>
          <b-input-group
            size="sm"
            class="mb-2"
            v-if="canEditCronQuery(query)">
            <template #prepend>
              <b-input-group-text
                v-b-tooltip.hover
                class="cursor-help"
                title="Enter a comma separated list of users that can view this periodic query">
                Share with users
              </b-input-group-text>
            </template>
            <b-form-input
              v-model="query.users"
              @input="cronQueryChanged(query)"
            />
          </b-input-group>
          <div class="mb-2"
            v-if="canEditCronQuery(query)">
            <RoleDropdown
              :roles="roles"
              :id="query.key"
              :selected-roles="query.roles"
              @selected-roles-updated="updateCronQueryRoles"
              :display-text="query.roles && query.roles.length ? undefined : 'Share with roles'"
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
              {{ query.created * 1000 | timezoneDateString(user.settings.timezone, false) }}
            </div>
          </div>
          <div class="row"
            v-if="query.lastRun">
            <div class="col">
              <strong>Last run at</strong>:
              {{ query.lastRun * 1000 | timezoneDateString(user.settings.timezone, false) }}
              and matched {{ query.lastCount || 0 }} new sessions
            </div>
          </div>
          <div class="row"
            v-if="query.lastToggled">
            <div class="col">
              <strong>{{ query.enabled ? 'Enabled' : 'Disabled'}} at</strong>:
              {{ query.lastToggled * 1000 | timezoneDateString(user.settings.timezone, false) }}
              by {{ query.lastToggledBy }}
            </div>
          </div>
        </b-card-text>
        <template #footer>
          <template
            v-if="query.changed && canEditCronQuery(query)">
            <b-button
              size="sm"
              variant="warning"
              v-b-tooltip.hover
              @click="getCronQueries"
              title="Undo changes to this query">
              <span class="fa fa-ban fa-fw mr-1" />
              Cancel
            </b-button>
            <b-button
              size="sm"
              v-b-tooltip.hover
              class="pull-right"
              variant="theme-tertiary"
              title="Save changes to this query"
              @click="updateCronQuery(query, index)">
              <span class="fa fa-save fa-fw mr-1" />
              Save
            </b-button>
          </template>
          <template v-else>
            <b-button
              size="sm"
              variant="info"
              v-if="!query.changed"
              @click="openCronSessions(query)"
              v-b-tooltip.hover="'Open sessions that this query tagged in the last hour.'">
              <span class="fa fa-folder-open fa-fw mr-1" />
              Open Matching Sessions
            </b-button>
            <b-button
              size="sm"
              variant="danger"
              v-b-tooltip.hover
              class="pull-right"
              title="Delete this periodic query"
              @click="deleteCronQuery(query, index)"
              v-if="!query.changed && canEditCronQuery(query)">
              <span class="fa fa-trash-o fa-fw mr-1" />
              Delete
            </b-button>
          </template>
        </template>
      </b-card>
    </b-card-group> <!-- /cron queries -->

  </form>
</template>

<script>
// services
import SettingsService from './SettingsService';
// components
import RoleDropdown from '../../../../../common/vueapp/RoleDropdown';

export default {
  name: 'PeriodicQueries',
  components: {
    RoleDropdown
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
      newCronQueryRoles: []
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
    // EXPOSED PAGE FUNCTIONS ---------------------------------------------- //
    canEditCronQuery (query) {
      return this.user.roles.includes('arkimeAdmin') || (query.creator && query.creator === this.user.userId);
    },
    updateNewCronQueryRoles (roles) {
      this.newCronQueryRoles = roles;
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
        this.$bvModal.hide('create-periodic-query-modal');
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
          this.$set(query, 'roles', roles);
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
      this.$set(query, 'changed', true);
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
        this.$set(this.cronQueries, index, response.query);
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
      SettingsService.getCronQueries(this.userId).then((response) => {
        this.cronQueries = response;
      }).catch((error) => {
        this.cronQueryListError = error.text;
      });
    },
    // HELPLER FUNCTIONS --------------------------------------------------- //
    clearNewFormInputs () {
      this.cronQueryFormError = false;
      this.newCronQueryName = '';
      this.newCronQueryTags = '';
      this.newCronQueryUsers = '';
      this.newCronQueryRoles = [];
      this.newCronQueryExpression = '';
      this.newCronQueryDescription = '';
      this.newCronQueryNotifier = undefined;
    }
  }
};
</script>
