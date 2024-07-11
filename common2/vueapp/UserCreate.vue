<!--
Copyright Yahoo Inc.
SPDX-License-Identifier: Apache-2.0
-->
<template>
  <v-dialog
    v-model="modalOpen"
    id="create-user-modal">
    <v-card :title="createMode === 'user' ? 'Create a New User' : 'Create a New Role'">
      <!-- create form -->
      <v-form class="d-flex flex-column ga-3 mx-4">
        <v-row>
          <!-- TODO: toby, lazy? -->
          <v-col no-gutters :md="6">
            <v-text-field
              size="small"
              autofocus
              autocomplete="userid"
              placeholder="Unique ID"
              v-model.lazy="newUser.userId"
              :rules="[newUser.userId.length > 0]"
              >
              <template #label>
                {{ createMode === 'user' ? 'User' : 'Role' }}
                ID<sup>*</sup>
              </template>
            </v-text-field>
          </v-col>
          <!-- <b-input-group -->
          <!--   size="sm" -->
          <!--   class="col-md-6 mt-2"> -->
          <!--   <template #prepend> -->
          <!--     <b-input-group-text> -->
          <!--       {{ createMode === 'user' ? 'User' : 'Role' }} -->
          <!--       ID<sup>*</sup> -->
          <!--     </b-input-group-text> -->
          <!--   </template> -->
          <!--   <b-form-input -->
          <!--     autofocus -->
          <!--     autocomplete="userid" -->
          <!--     placeholder="Unique ID" -->
          <!--     v-model.lazy="newUser.userId" -->
          <!--     :state="newUser.userId.length > 0" -->
          <!--   /> -->
          <!-- </b-input-group> -->

          <v-col :md="6">
            <v-text-field
              size="small"
              autocomplete="username"
              placeholder="Readable name"
              v-model.lazy="newUser.userName"
              :rules="[newUser.userName.length > 0]"
              >
              <template #label>
                {{ createMode === 'user' ? 'User' : 'Role' }}
                Name<sup>*</sup>
              </template>
            </v-text-field>
          </v-col>
          <!-- <b-input-group -->
          <!--   size="sm" -->
          <!--   class="col-md-6 mt-2"> -->
          <!--   <template #prepend> -->
          <!--     <b-input-group-text> -->
          <!--       {{ createMode === 'user' ? 'User' : 'Role' }} -->
          <!--       Name<sup>*</sup> -->
          <!--     </b-input-group-text> -->
          <!--   </template> -->
          <!--   <b-form-input -->
          <!--     autocomplete="username" -->
          <!--     placeholder="Readable name" -->
          <!--     v-model.lazy="newUser.userName" -->
          <!--     :state="newUser.userName.length > 0" -->
          <!--   /> -->
          <!-- </b-input-group> -->
        </v-row>
        <v-text-field
          size="small"
          v-tooltip="'An Arkime search expression that is silently added to all queries. Useful to limit what data can be accessed (e.g. which nodes or IPs)'"
          autocomplete="expression"
          label="Forced Expression"
          placeholder="node == test"
          v-model.lazy="newUser.expression"
        />
        <v-row no-gutters>
          <v-col md="6">
            <v-select
              label="Query Time Limit"
              v-tooltip="'Restrict the maximum time window of a query'"
              :items="[
                { value: 1, text: '1 hour' },
                { value: 6, text: '6 hours' },
                { value: 24, text: '24 hours' },
                { value: 48, text: '48 hours' },
                { value: 72, text: '72 hours' },
                { value: 168, text: '1 week' },
                { value: 336, text: '2 weeks' },
                { value: 720, text: '1 month' },
                { value: 1440, text: '2 months' },
                { value: 4380, text: '6 months' },
                { value: 8760, text: '1 year' },
                // null, since v-select will falls-back to `text` if `value` is undefined
                { value: null, text: 'All (careful)' }
              ]"
              item-title="text"
              item-value="value"
              :model-value="newUser.timeLimit"
              @update:model-value="val => newUser.timeLimit = val ?? undefined"
            />
          </v-col>
        </v-row>

        <v-row no-gutters>
          <v-col md="6" class="d-inline-flex align-center">
            <template v-if="roles">
              <RoleDropdown
                  :roles="roles"
                  display-text="Roles"
                  @selected-roles-updated="updateNewUserRoles"
              />
              <span
                  class="fa fa-info-circle fa-lg cursor-help ml-2"
                  v-tooltip="'These roles are applied across apps (Arkime, Parliament, WISE, Cont3xt)'"
              />
            </template>
            <template v-if="createMode === 'role'">
              <UserDropdown class="ml-3" display-text="Role Assigners" :selected-users="newUser.roleAssigners" @selected-users-updated="updateNewRoleAssigners">
                Role Assigners
              </UserDropdown>
              <span
                  class="fa fa-info-circle fa-lg cursor-help ml-2"
                  v-tooltip="'These users can manage who has this role'"
              />
            </template>
          </v-col>
        </v-row>
        <v-text-field
          v-if="createMode === 'user'"
          size="small"
          type="password"
          :state="validatePassword"
          placeholder="New password"
          autocomplete="new-password"
          v-model.lazy="newUser.password"
        >
          <template #label>
            Password<sup>*</sup>
          </template>
        </v-text-field>

        <v-container fluid class="d-flex flex-row flex-grow-1 flex-wrap ga-2">
          <v-checkbox inline
            v-model="newUser.enabled"
            label="Enabled"/>
          <template v-if="createMode === 'user'">
            <v-checkbox inline
              v-model="newUser.webEnabled"
              label="Web Interface"/>
            <v-checkbox inline
              v-model="newUser.headerAuthEnabled"
              label="Web Auth Header"/>
            <v-checkbox inline
              :model-value="!newUser.emailSearch"
              @update:model-value="newVal => negativeToggle(newVal, newUser, 'emailSearch')"
              label="Disable Arkime Email Search" />
            <v-checkbox inline
              :model-value="!newUser.removeEnabled"
              @update:model-value="newVal => negativeToggle(newVal, newUser, 'removeEnabled')"
              label="Disable Arkime Data Removal" />
            <v-checkbox inline
              :model-value="!newUser.packetSearch"
              @update:model-value="newVal => negativeToggle(newVal, newUser, 'packetSearch')"
              label="Disable Arkime Hunting" />
            <v-checkbox inline
              v-model="newUser.hideStats"
              label="Hide Arkime Stats Page" />
            <v-checkbox inline
              v-model="newUser.hideFiles"
              label="Hide Arkime Files Page" />
            <v-checkbox inline
              v-model="newUser.hidePcap"
              label="Hide Arkime PCAP" />
            <v-checkbox inline
              v-model="newUser.disablePcapDownload"
              label="Disable Arkime PCAP Download" />
          </template>
        </v-container>
      </v-form> <!-- /create form -->
      <!-- create form error -->
      <v-alert
        color="error"
        class="mt-2 mb-0 mx-4"
        v-if="!!createError"
        closable
        >
        {{ createError }}
      </v-alert> <!-- /create form error -->
      <!-- modal footer -->
      <template #actions>
        <div class="w-100 d-flex justify-space-between">
          <v-btn
            title="Cancel"
            color="error"
            @click="modalOpen = false">
            <span class="fa fa-times" />
            Cancel
          </v-btn>
          <div>
            <v-btn
              color="primary"
              @click="createUser(true)"
              v-if="roles && createMode === 'role'"
              v-tooltip="'Create New Role'">
              <span class="fa fa-plus-circle mr-1" />
              Create Role
            </v-btn>
            <v-btn
              color="primary"
              v-tooltip="'Create New User'"
              title="Create New User"
              @click="createUser(false)"
              v-if="createMode === 'user'">
              <span class="fa fa-plus-circle mr-1" />
              Create User
            </v-btn>
          </div>
        </div>
      </template> <!-- /modal footer -->
    </v-card>
  </v-dialog>
</template>

<script setup>
import UserService from './UserService';
import RoleDropdown from './RoleDropdown.vue';
import UserDropdown from './UserDropdown.vue';
import { defineProps, defineEmits, defineModel, watch, ref } from 'vue';

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

defineProps({
  roles: {
    type: Array,
    required: true
  },
  createMode: {
    type: String,
    default: 'user'
  }
});

const emit = defineEmits(['user-created']);

const modalOpen = defineModel('modalOpen', { required: false, default: false });
watch(modalOpen, (val) => {
  console.log('toby look', val)
})

const createError = ref('');
const newUser = ref(defaultNewUser);
const validatePassword = ref(undefined);

function negativeToggle (newVal, user, field) {
  user[field] = !newVal;
}
function updateNewUserRoles (roles) {
  newUser.value.roles = roles;
}
function updateNewRoleAssigners ({ newSelection }) {
  newUser.value.roleAssigners = newSelection;
}
function createUser (createRole) {
  createError.value = '';
  validatePassword.value = undefined

  if (!newUser.value.userId) {
    createError.value = 'ID can not be empty';
    return;
  }

  if (!newUser.value.userName) {
    createError.value = 'Name can not be empty';
    return;
  }

  if (!createRole && !newUser.value.password) {
    validatePassword.value = false;
    createError.value = 'Password can not be empty';
    return;
  }

  const user = JSON.parse(JSON.stringify(newUser.value));
  if (createRole) {
    if (!user.userId.startsWith('role:')) {
      user.userId = `role:${user.userId}`;
    }
  }

  UserService.createUser(user).then((response) => {
    newUser.value = defaultNewUser;
    emit('user-created', response.text, user);
  }).catch((error) => {
    createError.value = error.text;
  });
}
</script>
