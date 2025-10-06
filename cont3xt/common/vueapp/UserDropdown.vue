<!--
Copyright Yahoo Inc.
SPDX-License-Identifier: Apache-2.0
-->
<template>
  <div class="d-inline-flex align-center">
    <label
      v-if="label"
      :for="`user-dropdown-${roleId}`"
      class="mb-0 mr-1"
    >{{ label }}</label>
    <v-btn
      size="small"
      color="secondary"
      @shown="setFocus"
      class="users-dropdown no-wrap text-none"
      :id="`user-dropdown-${roleId}`"
      data-testid="user-dropdown"
      :loading="loading"
    >
      <v-tooltip
        v-if="selectedTooltip && getUsersStr()"
        activator="parent"
        location="top start"
      >
        {{ getUsersStr() }}
      </v-tooltip>
      <!--   Text on dropdown (configurable via default slot)   -->
      <template v-if="loading">
        Loading users...
      </template>
      <template v-else>
        <slot
          :count="localSelectedUsers.length"
          :filter="searchTerm"
          :unknown="loading || error"
        >
          {{ getUsersStr() }}
        </slot><!--   /Text on dropdown (configurable via default slot)   -->
      </template>
      <v-icon
        icon="mdi-menu-down"
        size="large"
        class="ml-1"
      />

      <v-menu
        activator="parent"
        location="bottom left"
        :close-on-content-click="false"
      >
        <v-card
          class="px-1 py-1 overflow-hidden"
          :loading="loading"
        >
          <div class="d-flex flex-column">
            <!-- users search -->
            <v-text-field
              block
              v-focus="focus"
              v-model="searchTerm"
              v-debounce="loadUsers"
              placeholder="Search for users..."
              clearable
            /><!-- /users search -->

            <!-- error -->
            <template v-if="error">
              <div class="mt-3 alert alert-warning">
                <v-icon icon="mdi-alert" />&nbsp;
                {{ error }}
              </div>
            </template> <!-- /error -->

            <!-- user checkboxes -->
            <template v-else>
              <v-checkbox
                v-for="user in users"
                :key="user.userId"
                :value="user.userId"
                :model-value="localSelectedUsers"
                @update:model-value="val => updateUsers(user.userId, val)"
                class="d-flex flex-column"
                :label="`${user.userName} (${user.userId})`"
              />
            </template> <!-- /user checkboxes -->

            <div
              class="text-disabled mx-2 my-2"
              v-if="users && !users.length && searchTerm"
            >
              No users match your search
            </div>
          </div>
        </v-card>
      </v-menu>
    </v-btn>
  </div>
</template>

<script>
import UserService from '@real_common/UserService';
import Focus from './Focus.vue';

export default {
  name: 'UserDropdown',
  directives: { Focus },
  props: {
    roleId: {
      type: String,
      required: false // during creation, a role will not have an ID
    },
    selectedUsers: {
      type: Array,
      required: false // can use initializeSelectionWithRole instead
    },
    requestRoleStatus: { type: Boolean },
    initializeSelectionWithRole: { type: Boolean },
    selectedTooltip: { type: Boolean },
    label: {
      type: String,
      required: false
    }
  },
  data () {
    return {
      error: '',
      focus: false,
      loading: true,
      searchTerm: '',
      users: undefined,
      localSelectedUsers: this.selectedUsers || []
    };
  },
  methods: {
    getUsersStr () {
      const userArr = [...this.localSelectedUsers];
      userArr.sort();
      return userArr.join(', ');
    },
    loadUsers () {
      const query = {
        filter: this.searchTerm
      };
      if (this.requestRoleStatus && this.roleId != null) {
        query.roleId = this.roleId;
      }

      UserService.searchUsersMin(query).then((response) => {
        this.error = '';
        this.loading = false;
        this.users = JSON.parse(JSON.stringify(response.data));
        if (this.initializeSelectionWithRole) {
          this.localSelectedUsers = this.users.filter(u => u.hasRole).map(u => u.userId);
        }
      }).catch((error) => {
        this.loading = false;
        this.error = error.text ?? 'error';
      });
    },
    updateUsers (userId, newSelection) { // emits both the new array and changed user-value
      const change = {
        newSelection,
        changedUser: {
          userId,
          newState: newSelection.length > this.localSelectedUsers.length
        }
      };
      this.localSelectedUsers = newSelection;
      this.$emit('selected-users-updated', change, this.roleId);
    },
    clearSearchTerm () {
      this.searchTerm = '';
      this.setFocus();
    },
    setFocus () {
      this.focus = true;
      setTimeout(() => {
        this.focus = false;
      }, 100);
    }
  },
  mounted () {
    this.loadUsers();
  }
};
</script>

<style scoped>
/* hides elements scrolling behind sticky search bar */
.users-dropdown .dropdown-header {
  background-color: var(--color-background);
}
.users-dropdown .dropdown-header > li {
  padding-top: 10px;
  background-color: var(--color-background);
}
.users-dropdown .dropdown-divider {
  margin-top: 0px;
}

.users-dropdown .dropdown-item,
.users-dropdown .custom-control {
  padding-left: 2rem;
}
</style>
