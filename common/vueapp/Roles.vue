<!--
Copyright Yahoo Inc.
SPDX-License-Identifier: Apache-2.0
-->
<template>
  <div class="container-fluid roles-page">
    <div class="d-flex align-center mt-2 mb-2">
      <div class="ms-1 me-1 flex-grow-1">
        <div class="arkime-input-group arkime-input-group--fluid">
          <span class="arkime-input-label arkime-input-label-fw">
            <v-icon icon="mdi-magnify" />
          </span>
          <input
            type="text"
            class="arkime-input-control"
            v-focus="true"
            v-model="searchTerm"
            :placeholder="$t('users.rolesSearchPlaceholder')">
          <v-btn
            v-if="searchTerm"
            variant="text"
            size="x-small"
            density="comfortable"
            icon
            class="arkime-input-append-btn"
            :aria-label="$t('common.clear')"
            @click="searchTerm = ''">
            <v-icon icon="mdi-close" />
          </v-btn>
        </div>
      </div>
      <v-icon
        icon="mdi-information"
        size="large"
        class="me-1 cursor-help align-self-center"
        id="roles-page-tip" />
      <v-tooltip activator="#roles-page-tip">
        <span v-html="$t('roles.pageTip')" />
      </v-tooltip>
    </div>

    <!-- loading -->
    <div
      v-if="loading"
      class="text-center mt-4 mb-4">
      <v-icon
        icon="mdi-loading"
        size="large"
        class="mdi-spin" />
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
    <v-alert
      v-if="error.length"
      type="warning"
      variant="tonal"
      density="compact"
      closable
      class="mt-2"
      @click:close="error = ''">
      {{ error }}
    </v-alert> <!-- /roles error -->
  </div>
</template>

<script>
import Focus from './Focus.vue';
import UserDropdown from './UserDropdown.vue';
import UserService from './UserService';
import { parseRoles, searchRoles } from './vueFilters.js';

export default {
  name: 'RolesCommon',
  emits: ['update-current-user'],
  directives: { Focus },
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
  background-color: rgb(var(--v-theme-neutral-lighter)) !important;
}
</style>
