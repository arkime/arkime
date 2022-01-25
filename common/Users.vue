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

        <!-- TODO ECR ONLY SHOW ROLES v-if="tmpRolesSupport" -->
        <!-- column headers -->
        <template v-slot:head()="data">
          <span v-b-tooltip.hover="data.field.help">
            {{ data.label }}
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
              @click="updateUser(data.item)"
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
              @click="openSettings(data.item.userId)"
              v-b-tooltip.hover="`Settings for ${data.item.userId}`">
              <span class="fa fa-gear" />
            </b-button>
            <b-button
              size="sm"
              variant="theme-secondary"
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
            v-model="data.value"
            v-else-if="data.field.type === 'checkbox'"
            @change="userHasChanged(data.item.userId)"
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
                  @change="userHasChanged(data.item.userId)">
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
                    @change="userHasChanged(data.item.userId)">
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
        <template #row-details="row">
          {{ row.item }} <!-- TODO ECR -->
        </template> <!-- /detail row -->
      </b-table>
    </div> <!-- /users table -->

  </div>
</template>

<script>
import axios from 'axios';

import ToggleBtn from '@/components/utils/ToggleBtn';

export default {
  name: 'UsersCommon',
  components: { ToggleBtn },
  props: {
    roles: Array
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
        // { name: 'Email Search', key: 'emailSearch', sortable: true, type: 'checkbox', help: 'Can perform email searches' },
        // { name: 'Can Remove Data', key: 'removeEnabled', sortable: true, type: 'checkbox', help: 'Can delete tags or delete/scrub pcap data' },
        // { name: 'Can Create Hunts', key: 'packetSearch', sortable: true, type: 'checkbox', help: 'Can create a packet search job (hunt)' },
        { name: 'Roles', key: 'roles', sortable: false, type: 'select', help: 'Roles assigned to this user' },
        { name: 'Last Used', key: 'lastUsed', sortable: true, type: 'checkbox', help: 'The last time this user used Arkime' },
        { label: '', key: 'action', sortable: false, thStyle: 'width:190px' }
        // { additional: true, name: 'Forced Expression', key: 'expression', help: 'A Arkime search expression that is silently added to all queries. Useful to limit what data a user can access (e.g. which nodes or IPs)' },
        // { additional: true, name: 'Query Time Limit', key: 'timeLimit', help: 'Restrict the maximum time window of a user\'s query' },
        // { additional: true, name: 'Hide Stats Page', key: 'hideStats', help: 'Hide the Stats page from this user' },
        // { additional: true, name: 'Hide Files Page', key: 'hideFiles', help: 'Hide the Files page from this user' },
        // { additional: true, name: 'Hide PCAP', key: 'hidePcap', help: 'Hide PCAP (and only show metadata/session detail) for this user when they open a Session' },
        // { additional: true, name: 'Disable PCAP Download', key: 'disablePcapDownload', help: 'Do not allow this user to download PCAP files' }
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

      // TODO ECR why are they always equal!?
      this.$set(this.changed, userId, JSON.stringify(newUser) !== JSON.stringify(oldUser));
      console.log('CHANGED!?', this.changed, newUser.userName, oldUser.userName); // TODO ECR REMOVE
    },
    updateUser (user) { // TODO ECR
      this.$set(user, 'expanded', undefined);
      // UserService.updateUser(user).then((response) => {
      //   this.msg = response.data.text;
      //   this.msgType = 'success';
      //   this.reloadData();
      //   // update the current user if they were changed
      //   if (this.user.userId === user.userId) {
      //     const combined = { // last object property overwrites the previous one
      //       ...this.user,
      //       ...user
      //     };
      //     // time limit is special because it can be undefined
      //     combined.timeLimit = user.timeLimit || undefined;
      //     this.$set(this, 'user', combined);
      //   }
      // }).catch((error) => {
      //   this.msg = error.text;
      //   this.msgType = 'danger';
      // });
    },
    deleteUser (user, index) { // TODO ECR
      // UserService.deleteUser(user).then((response) => {
      //   this.users.splice(index, 1);
      //   this.msg = response.data.text;
      //   this.msgType = 'success';
      // }).catch((error) => {
      //   this.msg = error.text;
      //   this.msgType = 'danger';
      // });
    },
    cancelEdits (userId) { // TODO ECR
      const canceledUser = this.users.find(u => u.userId === userId);
      const oldUser = this.dbUserList.find(u => u.userId === userId);
      Object.assign(canceledUser, oldUser);
    },
    openSettings (userId) { // TODO ECR
      this.$router.push({
        path: '/settings',
        query: {
          ...this.$route.query,
          userId: userId
        }
      });
    },
    openHistory (userId) { // TODO ECR - ONLY ON ARKIME PAGE
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
      const options = {
        method: 'POST',
        url: 'api/users',
        data: {
          desc: this.desc,
          length: this.perPage,
          filter: this.searchTerm,
          sortField: this.sortField,
          start: (this.currentPage - 1) * this.perPage
        }
      };

      axios(options).then((response) => {
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
