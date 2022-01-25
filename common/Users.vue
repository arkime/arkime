<template>
  <div>

    <!-- search -->
    <div class="d-flex justify-content-between mt-3">
      <div class="mr-2 flex-grow-1 ">
        <b-input-group size="sm">
          <template #prepend>
            <b-input-group-text>
              <span class="fa fa-search fa-fw" />
            </b-input-group-text>
          </template>
          <b-form-input
            autofocus
            type="text"
            debounce="400"
            v-model="searchTerm"
            placeholder="Begin typing to search for users by name"
          />
          <template #append>
            <b-button
              :disabled="!searchTerm"
              @click="searchTerm = ''"
              variant="outline-secondary"
              v-b-tooltip.hover="'Clear search'">
              <span class="fa fa-close" />
            </b-button>
          </template>
        </b-input-group>
      </div>
      <div class="mr-2">
        <b-form-select
          size="sm"
          v-model="perPage"
          @change="perPageChange"
          :options="[
            { value: 5, text: '5 per page'},
            { value: 10, text: '10 per page'},
            { value: 20, text: '20 per page'},
            { value: 50, text: '50 per page'},
            { value: 100, text: '100 per page'}
          ]"
        />
      </div>
      <div>
        <b-pagination
          size="sm"
          :per-page="perPage"
          v-model="currentPage"
          :total-rows="recordsTotal"
        />
      </div>
    </div> <!-- /search -->

    <!-- error -->
    <div v-if="error"
      class="info-area vertical-center text-monospace">
      <div class="text-danger">
        <span class="fa fa-2x fa-warning" />
        {{ error }}
      </div>
    </div> <!-- /error -->

    <!-- loading -->
    <template v-if="loading">
      <slot name="loading">
        <div class="text-center mt-5">
          <span class="fa fa-2x fa-spin fa-spinner" />
          <br />
          Loading...
        </div>
      </slot>
    </template> <!-- /loading -->

    <!-- users table -->
    <div v-if="!error && users">
      <b-table
        small
        hover
        striped
        show-empty
        sort-icon-left
        no-local-sorting
        :items="users"
        :fields="fields"
        :sort-desc.sync="desc"
        :sort-by.sync="sortField"
        @sort-changed="sortChanged"
        :empty-text="searchTerm ? 'No users match your search' : 'No users'">

        <!-- column headers -->
        <template v-slot:head()="data">
          <span v-b-tooltip.hover="data.field.help">
            {{ data.label }}
            <span
              v-if="data.field.key === 'roles'"
              class="fa fa-info-circle fa-lg cursor-help ml-2"
              v-b-tooltip.hover="'These roles are applied to this user across apps (Arkime, Parliament, WISE, Cont3xt)'"
            />
          </span>
        </template> <!-- /column headers -->

        <!-- toggle column -->
         <template #cell(toggle)="data">
          <span :class="{'btn-indicator':data.item.hideStats || data.item.hideFiles || data.item.hidePcap || data.item.disablePcapDownload || data.item.timeLimit || data.item.expression}">
            <ToggleBtn
              class="btn-toggle-user"
              @toggle="data.toggleDetails"
              :opened="data.detailsShowing"
              :class="{expanded: data.detailsShowing}"
              v-b-tooltip.hover="data.item.hideStats || data.item.hideFiles || data.item.hidePcap || data.item.disablePcapDownload || data.item.timeLimit || data.item.expression ? 'This user has additional restricted permissions' : ''"
            />
          </span>
        </template> <!-- /toggle column -->
        <!-- action column -->
        <template #cell(action)="data">
          <div class="pull-right">
            <b-button v-if="changed[data.item.userId]"
              size="sm"
              variant="success"
              @click="updateUser(data)"
              v-b-tooltip.hover="`Save the updated settings for ${data.item.userId}`">
              <span class="fa fa-save" />
            </b-button>
            <b-button v-if="changed[data.item.userId]"
              size="sm"
              variant="warning"
              @click="cancelEdits(data.item.userId)"
              v-b-tooltip.hover="`Cancel changed settings for ${data.item.userId}`">
              <span class="fa fa-ban" />
            </b-button>
            <b-button
              size="sm"
              variant="theme-primary"
              v-has-role="'arkimeAdmin'"
              @click="openSettings(data.item.userId)"
              v-if="!data.item.userId.startsWith('role:')"
              v-b-tooltip.hover="`Settings for ${data.item.userId}`">
              <span class="fa fa-gear" />
            </b-button>
            <b-button
              size="sm"
              variant="theme-secondary"
              v-if="parentApp === 'Arkime'"
              @click="openHistory(data.item.userId)"
              v-b-tooltip.hover="`History for ${data.item.userId}`">
              <span class="fa fa-history" />
            </b-button>
            <b-button
              size="sm"
              variant="danger"
              @click="deleteUser(data.item, data.index)"
              v-b-tooltip.hover="`Delete ${data.item.userId}`">
              <span class="fa fa-trash-o" />
            </b-button>
          </div>
        </template> <!-- /action column -->
        <!-- user id column -->
        <template #cell(userId)="data">
          {{ data.value }}
        </template> <!-- /user id column -->
        <!-- last used column -->
        <template #cell(lastUsed)="data">
          {{ data.value ? (data.value | timezoneDateString(user.settings.timezone, user.settings.ms)) : 'Never' }}
        </template> <!-- /last used column -->
        <!-- all other columns -->
        <template #cell()="data">
          <b-form-input
            size="sm"
            v-model="data.item[data.field.key]"
            v-if="data.field.type === 'text'"
            @input="userHasChanged(data.item.userId)"
          />
          <b-form-checkbox
            v-model="data.item[data.field.key]"
            v-else-if="data.field.type === 'checkbox'"
            @input="userHasChanged(data.item.userId)"
          />
          <b-dropdown
            size="sm"
            text="User's Roles"
            v-else-if="data.field.type === 'select' && roles && roles.length">
            <b-dropdown-form>
              <b-form-checkbox-group
                v-model="data.item.roles">
                <b-form-checkbox
                  v-for="role in roles"
                  :value="role.value"
                  :key="role.value"
                  @input="userHasChanged(data.item.userId)">
                  {{ role.text }}
                  <span
                    v-if="role.userDefined"
                    class="fa fa-user cursor-help ml-2"
                    v-b-tooltip.hover="'User defined role'"
                  />
                </b-form-checkbox>
                <template v-for="role in data.item.roles">
                  <b-form-checkbox
                    :key="role"
                    :value="role"
                    v-if="!roles.find(r => r.value === role)"
                    @input="userHasChanged(data.item.userId)">
                    {{ role }}
                    <span
                      class="fa fa-times-circle cursor-help ml-2"
                      v-b-tooltip.hover="'This role no longer exists'"
                    />
                  </b-form-checkbox>
                </template>
              </b-form-checkbox-group>
            </b-dropdown-form>
          </b-dropdown>
        </template> <!-- all other columns -->

        <!-- detail row -->
        <template #row-details="data">
          <div class="m-2">
            <b-form-checkbox inline
              v-model="data.item.emailSearch"
              @input="userHasChanged(data.item.userId)">
              Arkime Email Search
            </b-form-checkbox>
            <b-form-checkbox inline
              v-model="data.item.removeEnabled"
              @input="userHasChanged(data.item.userId)">
              Can Remove Arkime Data
            </b-form-checkbox>
            <b-form-checkbox inline
              v-model="data.item.packetSearch"
              @input="userHasChanged(data.item.userId)">
              Can Create Arkime Hunts
            </b-form-checkbox>
            <b-form-checkbox inline
              v-model="data.item.hideStats"
              @input="userHasChanged(data.item.userId)">
              Hide Arkime Stats Page
            </b-form-checkbox>
            <b-form-checkbox inline
              v-model="data.item.hideFiles"
              @input="userHasChanged(data.item.userId)">
              Hide Arkime Files Page
            </b-form-checkbox>
            <b-form-checkbox inline
              v-model="data.item.hidePcap"
              @input="userHasChanged(data.item.userId)">
              Hide Arkime PCAP
            </b-form-checkbox>
            <b-form-checkbox inline
              v-model="data.item.disablePcapDownload"
              @input="userHasChanged(data.item.userId)">
              Disable Arkime PCAP Download
            </b-form-checkbox>
            <b-input-group class="mt-3">
              <template #prepend>
                <b-input-group-text
                  v-b-tooltip.hover="'An Arkime search expression that is silently added to all queries. Useful to limit what data a user can access (e.g. which nodes or IPs)'">
                  Forced Expression
                </b-input-group-text>
              </template>
              <b-form-input
                v-model="data.item.expression"
                @input="userHasChanged(data.item.userId)"
              />
            </b-input-group>
            <b-input-group class="mt-3 w-25">
              <template #prepend>
                <b-input-group-text
                  v-b-tooltip.hover="'Restrict the maximum time window of a user\'s query'">
                  Query Time Limit
                </b-input-group-text>
              </template>
              <!-- NOTE: can't use b-form-select because it doesn't allow for undefined v-models -->
              <select
                class="form-control"
                v-model="data.item.timeLimit"
                @change="changeTimeLimit(data.item)">
                <option value="1">1 hour</option>
                <option value="6">6 hours</option>
                <option value="24">24 hours</option>
                <option value="48">48 hours</option>
                <option value="72">72 hours</option>
                <option value="168">1 week</option>
                <option value="336">2 weeks</option>
                <option value="720">1 month</option>
                <option value="1440">2 months</option>
                <option value="4380">6 months</option>
                <option value="8760">1 year</option>
                <option value=undefined>All (careful)</option>
              </select>
            </b-input-group>
          </div>
        </template> <!-- /detail row -->
      </b-table>
    </div> <!-- /users table -->

  </div>
</template>

<script>
import UserService from './UserService';
import ToggleBtn from '@/components/utils/ToggleBtn';

export default {
  name: 'UsersCommon',
  components: { ToggleBtn },
  props: {
    roles: Array,
    parentApp: String
  },
  data () {
    return {
      error: '',
      loading: true,
      searchTerm: '',
      users: undefined,
      dbUserList: undefined,
      changed: {},
      recordsTotal: 0,
      perPage: 50,
      currentPage: 1,
      sortField: 'userId',
      desc: false,
      fields: [
        { label: '', key: 'toggle', sortable: false },
        { label: 'User ID', key: 'userId', sortable: true, required: true, help: 'The id used for login, can not be changed once created' },
        { name: 'User Name', key: 'userName', sortable: true, type: 'text', required: true, help: 'Friendly name for user' },
        { name: 'Enabled', key: 'enabled', sortable: true, type: 'checkbox', help: 'Is the account currently enabled for anything?' },
        { name: 'Web Interface', key: 'webEnabled', sortable: true, type: 'checkbox', help: 'Can access the web interface. When off only APIs can be used' },
        { name: 'Web Auth Header', key: 'headerAuthEnabled', sortable: true, type: 'checkbox', help: 'Can login using the web auth header. This setting doesn\'t disable the password so it should be scrambled' },
        { name: 'Roles', key: 'roles', sortable: false, type: 'select', help: 'Roles assigned to this user' },
        { name: 'Last Used', key: 'lastUsed', sortable: true, type: 'checkbox', help: 'The last time this user used Arkime' },
        { label: '', key: 'action', sortable: false, thStyle: 'width:190px' }
      ]
    };
  },
  created () {
    this.loadUsers();
  },
  watch: {
    searchTerm () {
      this.loadUsers();
    }
  },
  methods: {
    /* exposed page functions ---------------------------------------------- */
    perPageChange (newVal) {
      this.perPage = newVal;
      this.loadUsers(false);
    },
    sortChanged (ctx) {
      this.sortField = ctx.sortBy;
      this.desc = ctx.sortDesc;
      this.loadUsers();
    },
    changeTimeLimit (user) {
      if (user.timeLimit === 'undefined') {
        delete user.timeLimit;
      } else {
        user.timeLimit = parseInt(user.timeLimit);
      }

      this.userHasChanged(user.userId);
    },
    userHasChanged (userId) {
      const newUser = JSON.parse(JSON.stringify(this.users.find(u => u.userId === userId)));
      const oldUser = JSON.parse(JSON.stringify(this.dbUserList.find(u => u.userId === userId)));

      // roles might be undefined, but compare to emtpy array since toggling on
      // any roles sets roles to an array, and removing that role = empty array
      if (oldUser.roles === undefined) { oldUser.roles = []; }
      if (newUser.roles === undefined) { newUser.roles = []; }

      // make sure these fields exist or the objects will be different
      // (undefined is the same as false for these fields)
      oldUser.timeLimit = oldUser.timeLimit ? oldUser.timeLimit : undefined;
      newUser.timeLimit = newUser.timeLimit ? newUser.timeLimit : undefined;
      oldUser.hidePcap = oldUser.hidePcap ? oldUser.hidePcap : undefined;
      newUser.hidePcap = newUser.hidePcap ? newUser.hidePcap : undefined;
      oldUser.hideFiles = oldUser.hideFiles ? oldUser.hideFiles : undefined;
      newUser.hideFiles = newUser.hideFiles ? newUser.hideFiles : undefined;
      oldUser.hideStats = oldUser.hideStats ? oldUser.hideStats : undefined;
      newUser.hideStats = newUser.hideStats ? newUser.hideStats : undefined;
      oldUser.disablePcapDownload = oldUser.disablePcapDownload ? oldUser.disablePcapDownload : undefined;
      newUser.disablePcapDownload = newUser.disablePcapDownload ? newUser.disablePcapDownload : undefined;

      oldUser.expanded = undefined; // don't care about expanded field (just for UI)
      newUser.expanded = undefined;
      oldUser.lastUsed = undefined; // don't compare lastused, it might be different if the user is using the UI
      newUser.lastUsed = undefined;

      this.$set(this.changed, userId, JSON.stringify(newUser) !== JSON.stringify(oldUser));
    },
    updateUser (row) {
      if (row.detailsShowing) { row.toggleDetails(); }

      const user = row.item;
      UserService.updateUser(user).then((response) => {
        this.$set(this.changed, user.id, false);
        this.msg = response.data.text;
        this.msgType = 'success';
        this.reloadData();
        // update the current user if they were changed
        if (this.user.userId === user.userId) {
          const combined = { // last object property overwrites the previous one
            ...this.user,
            ...user
          };
          // time limit is special because it can be undefined
          combined.timeLimit = user.timeLimit || undefined;
          this.$set(this, 'user', combined);
        }
      }).catch((error) => {
        this.msg = error.text;
        this.msgType = 'danger';
      });
    },
    deleteUser (user, index) {
      UserService.deleteUser(user).then((response) => {
        this.users.splice(index, 1);
        this.msg = response.data.text;
        this.msgType = 'success';
      }).catch((error) => {
        this.msg = error.text;
        this.msgType = 'danger';
      });
    },
    cancelEdits (userId) {
      this.$set(this.changed, userId, false);
      const canceledUser = this.users.find(u => u.userId === userId);
      const oldUser = this.dbUserList.find(u => u.userId === userId);
      Object.assign(canceledUser, oldUser);
    },
    openSettings (userId) {
      this.$router.push({
        path: '/settings',
        query: {
          ...this.$route.query,
          userId: userId
        }
      });
    },
    openHistory (userId) {
      this.$router.push({
        path: '/history',
        query: {
          ...this.$route.query,
          userId: userId
        }
      });
    },
    /* helper functions ---------------------------------------------------- */
    loadUsers () {
      const query = {
        desc: this.desc,
        length: this.perPage,
        filter: this.searchTerm,
        sortField: this.sortField,
        start: (this.currentPage - 1) * this.perPage
      };

      UserService.searchUsers(query).then((response) => {
        this.error = '';
        this.loading = false;
        this.recordsTotal = response.data.recordsTotal;
        this.users = JSON.parse(JSON.stringify(response.data.data));
        // don't modify original list - used for comparing
        this.dbUserList = JSON.parse(JSON.stringify(response.data.data));
      }).catch((error) => {
        this.loading = false;
        this.error = error.text;
      });
    },
    reloadUsers () {
      const query = {
        desc: this.desc,
        length: this.perPage,
        filter: this.searchTerm,
        sortField: this.sortField,
        start: (this.currentPage - 1) * this.perPage
      };

      UserService.getUsers(query).then((response) => {
        this.error = '';
        this.loading = false;
        const userData = JSON.parse(JSON.stringify(response.data.data));
        this.recordsTotal = response.data.recordsTotal;
        // don't modify original list - used for comparing
        this.dbUserList = response.data.data;

        // Dont update users that have edits. Update dbUserList first to compare against
        // This will keep returned db sorting order regardless if sorted fields are shown on edited fields
        this.users = userData.map(u => {
          const matchedUser = this.users.find(item => item.userId === u.userId);
          // If user already exists and is still being edited, keep user obj
          return (matchedUser && this.userHasChanged(u.userId)) ? matchedUser : u;
        });
      }).catch((error) => {
        this.loading = false;
        this.error = error.text;
      });
    }
  }
};
</script>

<style>
/* center cell content vertically */
.btn-toggle-user {
  margin-top: 2px;
}
.btn-toggle-user.collapsed {
  background: var(--color-tertiary);
}

/* indication that a user has additional permissions set */
.btn-indicator .btn-toggle-user:not(.expanded) {
  background: linear-gradient(135deg, var(--color-tertiary) 1%, var(--color-tertiary) 75%, var(--color-tertiary) 75%, var(--color-tertiary-darker) 77%, var(--color-tertiary-darker) 100%);
}
</style>
