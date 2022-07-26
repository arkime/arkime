<template>
  <b-dropdown
      size="sm"
      class="users-dropdown"
      :text="displayText || getUsersStr()">
    <b-dropdown-form v-if="!loading">

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

<!--        <b-input-group class="d-flex w-100">-->
<!--          <b-form-select-->
<!--              size="sm"-->
<!--              v-model="perPage"-->
<!--              :options="[-->
<!--            { value: 10, text: '10 per page'},-->
<!--            { value: 20, text: '20 per page'},-->
<!--            { value: 50, text: '50 per page'},-->
<!--            { value: 100, text: '100 per page'},-->
<!--            { value: 200, text: '200 per page'}-->
<!--          ]"-->
<!--          />-->
<!--          <b-pagination-->
<!--              size="sm"-->
<!--              :per-page="perPage"-->
<!--              v-model="currentPage"-->
<!--              :total-rows="recordsTotal"-->
<!--          />-->
<!--        </b-input-group>-->

      <b-form-checkbox-group v-if="!loading" class="d-flex flex-column"
          v-model="localSelectedUsers">
        <b-form-checkbox
            :key="user.userId"
            :value="user.userId"
            v-for="user in users"
            @change="updateUsers(user.userId, $event)">
          {{ user.userId }}
        </b-form-checkbox>
      </b-form-checkbox-group>
    </b-dropdown-form>
  </b-dropdown>
</template>

<script>
import UserService from './UserService';

export default {
  name: 'UserDropdown',
  props: {
    roleId: {
      type: String,
      required: false // during creation, a role will not have an ID
    },
    displayText: { type: String },
    selectedUsers: {
      type: Array,
      required: false
    },
    initializeSelectionWithRole: { type: Boolean }
  },
  data () {
    return {
      perPage: 100,
      currentPage: 1,
      searchTerm: '',
      users: undefined,
      loading: true,
      error: '',
      recordsTotal: undefined,
      localSelectedUsers: this.selectedUsers || []
    };
  },
  watch: {
    searchTerm () {
      this.loadUsers();
    },
    perPage () {
      this.loadUsers();
    },
    currentPage () {
      this.loadUsers();
    }
  },
  methods: {
    getUsersStr () {
      const userArr = [...this.localSelectedUsers];
      userArr.sort();
      return userArr.join(', ');
    },
    loadUsers () {
      const query = {
        start: (this.currentPage - 1) * this.perPage,
        length: this.perPage, // assume this will get ALL users
        filter: this.searchTerm
      };
      if (this.roleId != null) {
        query.roleId = this.roleId;
      }

      UserService.searchAssignableUsers(query).then((response) => {
        this.error = '';
        this.loading = false;
        this.recordsTotal = response.recordsTotal;
        this.users = JSON.parse(JSON.stringify(response.data));
        if (this.initializeSelectionWithRole) {
          this.localSelectedUsers = this.users.filter(u => u.hasRole).map(u => u.userId);
        }
      }).catch((error) => {
        this.loading = false;
        this.error = error.text;
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
      this.$emit('selected-users-updated', change, this.roleId);
    }
  },
  mounted () {
    this.loadUsers();
  }
};
</script>

<style scoped>

</style>
