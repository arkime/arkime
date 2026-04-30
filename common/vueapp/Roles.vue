<!--
Copyright Yahoo Inc.
SPDX-License-Identifier: Apache-2.0
-->
<template>
  <div class="container-fluid roles-page">
    <div class="d-flex align-items-center mt-3 mb-2">
      <div class="me-2 flex-grow-1">
        <v-text-field
          autofocus
          density="compact"
          variant="outlined"
          hide-details
          clearable
          prepend-inner-icon="fa-search"
          v-model="searchTerm"
          :placeholder="$t('users.rolesSearchPlaceholder')" />
      </div>
      <h4>
        <span
          id="roles-page-tip"
          class="fa fa-info-circle ms-2 cursor-help" />
        <v-tooltip activator="#roles-page-tip">
          <span v-html="$t('roles.pageTip')" />
        </v-tooltip>
      </h4>
    </div>

    <!-- loading -->
    <div
      v-if="loading"
      class="text-center mt-4 mb-4">
      <span class="fa fa-circle-o-notch fa-spin fa-2x" />
      <p>{{ $t('common.loading') }}</p>
    </div> <!-- /loading -->

    <v-data-table
      v-else
      density="compact"
      :items="roleData"
      :headers="tableHeaders"
      :items-per-page="-1"
      hide-default-footer
      class="roles-table-striped">
      <template #no-data>
        {{ emptyTableText }}
      </template>
      <template #[`item.members`]="{ item }">
        <UserDropdown
          :selected-tooltip="true"
          :role-id="item.value"
          @selected-users-updated="updateUserRole"
          :request-role-status="true"
          :initialize-selection-with-role="true"
          v-slot="{ count, filter, unknown }">
          {{ $t(filter ? 'roles.summaryFilter' : 'roles.summary', {
            users: unknown ? '?' : $t('common.userCount', count),
            matches: $t('common.matchWordCount', count),
            filter}) }}
        </UserDropdown>
      </template>
    </v-data-table>

    <!-- roles error -->
    <div
      v-if="error.length"
      class="mt-2 alert alert-warning">
      <span class="fa fa-exclamation-triangle" />&nbsp;
      {{ error }}
      <button
        type="button"
        @click="error = ''"
        class="close cursor-pointer">
        <span>&times;</span>
      </button>
    </div> <!-- /roles error -->
  </div>
</template>

<script>
import UserDropdown from './UserDropdown.vue';
import UserService from './UserService';
import { parseRoles, searchRoles } from './vueFilters.js';

export default {
  name: 'RolesCommon',
  emits: ['update-current-user'],
  components: {
    UserDropdown
  },
  props: {
    currentUser: {
      type: Object,
      default: () => ({})
    },
    cont3xtDarkTheme: Boolean
  },
  data () {
    return {
      error: '',
      searchTerm: ''
    };
  },
  computed: {
    tableHeaders () {
      return [
        {
          title: this.$t('roles.name'),
          key: 'text',
          width: '10rem',
          sortable: true
        },
        {
          title: this.$t('roles.members'),
          key: 'members',
          sortable: false
        }
      ];
    },
    loading () {
      return this.currentUser?.assignableRoles == null;
    },
    roleData () {
      const assignableRoles = this.currentUser?.assignableRoles || [];
      const roles = parseRoles(assignableRoles);
      return searchRoles(roles, this.searchTerm);
    },
    emptyTableText () {
      if (!this.searchTerm) { return this.$t('roles.noManagedRoles'); }
      return this.$t('roles.noMatchedRoles');
    }
  },
  methods: {
    /* page functions ------------------------------------------------------ */
    updateUserRole ({ changedUser }, roleId) {
      UserService.updateUserRole({
        userId: changedUser.userId,
        roleId,
        newRoleState: changedUser.newState
      }).then(() => {
        this.$emit('update-current-user');
      }).catch((err) => {
        this.error = err;
      });
    }
  }
};
</script>

<style scoped>
.roles-table-striped :deep(tbody tr:nth-of-type(odd) > td) {
  background-color: var(--color-gray-lighter) !important;
}
</style>
