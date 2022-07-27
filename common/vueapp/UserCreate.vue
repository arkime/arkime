<template>
  <b-modal
    size="xl"
    id="create-user-modal"
    :title="createMode === 'user' ? 'Create a New User' : 'Create a New Role'">
    <!-- create form -->
    <b-form>
      <div class="row">
        <b-input-group
          size="sm"
          class="col-md-6 mt-2">
          <template #prepend>
            <b-input-group-text>
              {{ createMode === 'user' ? 'User' : 'Role' }}
              ID<sup>*</sup>
            </b-input-group-text>
          </template>
          <b-form-input
            autofocus
            autocomplete="userid"
            placeholder="Unique ID"
            v-model.lazy="newUser.userId"
            :state="newUser.userId.length > 0"
          />
        </b-input-group>
        <b-input-group
          size="sm"
          class="col-md-6 mt-2">
          <template #prepend>
            <b-input-group-text>
              {{ createMode === 'user' ? 'User' : 'Role' }}
              Name<sup>*</sup>
            </b-input-group-text>
          </template>
          <b-form-input
            autocomplete="username"
            placeholder="Readable name"
            v-model.lazy="newUser.userName"
            :state="newUser.userName.length > 0"
          />
        </b-input-group>
      </div>
      <b-input-group
        size="sm"
        class="mt-2">
        <template #prepend>
          <b-input-group-text
            class="cursor-help"
            v-b-tooltip.hover="'An Arkime search expression that is silently added to all queries. Useful to limit what data can be accessed (e.g. which nodes or IPs)'">
            Forced Expression
          </b-input-group-text>
        </template>
        <b-form-input
          autocomplete="expression"
          placeholder="node == test"
          v-model.lazy="newUser.expression"
        />
      </b-input-group>
      <div class="row">
        <div class="col-md-6">
          <b-input-group
            size="sm"
            class="mt-2">
            <template #prepend>
              <b-input-group-text
                class="cursor-help"
                v-b-tooltip.hover="'Restrict the maximum time window of a query'">
                Query Time Limit
              </b-input-group-text>
            </template>
            <!-- NOTE: can't use b-form-select because it doesn't allow for undefined v-models -->
            <select
              class="form-control"
              v-model.lazy="newUser.timeLimit">
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
        <div class="col-md-6 mt-2 d-inline-flex align-items-center">
          <template v-if="roles">
            <RoleDropdown
                :roles="roles"
                display-text="Roles"
                @selected-roles-updated="updateNewUserRoles"
            />
            <span
                class="fa fa-info-circle fa-lg cursor-help ml-2"
                v-b-tooltip.hover="'These roles are applied across apps (Arkime, Parliament, WISE, Cont3xt)'"
            />
          </template>
          <template v-if="createMode === 'role'">
            <UserDropdown class="ml-3" display-text="Role Assigners" :selected-users="newUser.roleAssigners" @selected-users-updated="updateNewRoleAssigners">
              Role Assigners
            </UserDropdown>
            <span
                class="fa fa-info-circle fa-lg cursor-help ml-2"
                v-b-tooltip.hover="'These users can manage who has this role'"
            />
          </template>
        </div>
      </div>
      <b-input-group
        size="sm"
        class="mb-2 mt-2"
        v-if="createMode === 'user'">
        <template #prepend>
          <b-input-group-text>
            Password<sup>*</sup>
          </b-input-group-text>
        </template>
        <b-form-input
          type="password"
          :state="validatePassword"
          placeholder="New password"
          autocomplete="new-password"
          v-model.lazy="newUser.password"
        />
      </b-input-group>
      <b-form-checkbox inline
        v-model="newUser.enabled">
        Enabled
      </b-form-checkbox>
      <b-form-checkbox inline
        v-model="newUser.webEnabled">
        Web Interface
      </b-form-checkbox>
      <b-form-checkbox inline
        v-model="newUser.headerAuthEnabled">
        Web Auth Header
      </b-form-checkbox>

      <b-form-checkbox inline
        :checked="!newUser.emailSearch"
        @input="newVal => negativeToggle(newVal, newUser, 'emailSearch')">
        Disable Arkime Email Search
      </b-form-checkbox>
      <b-form-checkbox inline
        :checked="!newUser.removeEnabled"
        @input="newVal => negativeToggle(newVal, newUser, 'removeEnabled')">
        Disable Arkime Data Removal
      </b-form-checkbox>
      <b-form-checkbox inline
        :checked="!newUser.packetSearch"
        @input="newVal => negativeToggle(newVal, newUser, 'packetSearch')">
        Disable Arkime Hunting
      </b-form-checkbox>

      <b-form-checkbox inline
        v-model="newUser.hideStats">
        Hide Arkime Stats Page
      </b-form-checkbox>
      <b-form-checkbox inline
        v-model="newUser.hideFiles">
        Hide Arkime Files Page
      </b-form-checkbox>
      <b-form-checkbox inline
        v-model="newUser.hidePcap">
        Hide Arkime PCAP
      </b-form-checkbox>
      <b-form-checkbox inline
        v-model="newUser.disablePcapDownload">
        Disable Arkime PCAP Download
      </b-form-checkbox>
    </b-form> <!-- /create form -->
    <!-- create form error -->
    <b-alert
      variant="danger"
      class="mt-2 mb-0"
      :show="!!createError">
      {{ createError }}
    </b-alert> <!-- /create form error -->
    <!-- modal footer -->
    <template #modal-footer>
      <div class="w-100 d-flex justify-content-between">
        <b-button
          title="Cancel"
          variant="danger"
          @click="$bvModal.hide('create-user-modal')">
          <span class="fa fa-times" />
          Cancel
        </b-button>
        <div>
          <b-button
            variant="primary"
            @click="createUser(true)"
            v-if="roles && createMode === 'role'"
            v-b-tooltip.hover="'Create New Role'">
            <span class="fa fa-plus-circle mr-1" />
            Create Role
          </b-button>
          <b-button
            variant="primary"
            v-b-tooltip.hover
            title="Create New User"
            @click="createUser(false)"
            v-if="createMode === 'user'">
            <span class="fa fa-plus-circle mr-1" />
            Create User
          </b-button>
        </div>
      </div>
    </template> <!-- /modal footer -->
  </b-modal>
</template>

<script>
import UserService from './UserService';
import RoleDropdown from './RoleDropdown';
import UserDropdown from './UserDropdown';

const defaultNewUser = {
  userId: '',
  userName: '',
  password: '',
  enabled: true,
  webEnabled: true,
  packetSearch: true,
  emailSearch: false,
  removeEnabled: false,
  roleAssigners: []
};

export default {
  name: 'CreateUser',
  components: {
    RoleDropdown,
    UserDropdown
  },
  props: {
    roles: {
      type: Array,
      required: true
    },
    createMode: {
      type: String,
      default: 'user'
    }
  },
  data () {
    return {
      createError: '',
      newUser: defaultNewUser,
      validatePassword: undefined
    };
  },
  methods: {
    negativeToggle (newVal, user, field) {
      this.$set(user, field, !newVal);
    },
    updateNewUserRoles (roles) {
      this.$set(this.newUser, 'roles', roles);
    },
    updateNewRoleAssigners ({ newSelection }) {
      this.$set(this.newUser, 'roleAssigners', newSelection);
    },
    createUser (createRole) {
      this.createError = '';
      this.validatePassword = undefined;

      if (!this.newUser.userId) {
        this.createError = 'ID can not be empty';
        return;
      }

      if (!this.newUser.userName) {
        this.createError = 'Name can not be empty';
        return;
      }

      if (!createRole && !this.newUser.password) {
        this.validatePassword = false;
        this.createError = 'Password can not be empty';
        return;
      }

      const user = JSON.parse(JSON.stringify(this.newUser));
      if (createRole) {
        if (!user.userId.startsWith('role:')) {
          user.userId = `role:${user.userId}`;
        }
      }

      UserService.createUser(user).then((response) => {
        this.newUser = defaultNewUser;
        this.$emit('user-created', response.text, user);
      }).catch((error) => {
        this.createError = error.text;
      });
    }
  }
};
</script>
