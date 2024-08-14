<!--
Copyright Yahoo Inc.
SPDX-License-Identifier: Apache-2.0
-->
<template>
  <div class="d-flex flex-column flex-grow-1 overflow-auto pt-3 position-relative d-flex flex-grow h-100">
    <v-overlay
      :model-value="loading"
      class="align-center justify-center blur-overlay"
      contained
    >
      <div class="d-flex flex-column align-center justify-center">
        <v-progress-circular
          color="info"
          size="64"
          indeterminate
        />
        <p>Loading roles...</p>
      </div>
    </v-overlay>
    <div class="d-flex flex-row align-center mb-2 mx-4">
      <div class="flex-grow-1">
        <!-- TODO: toby, had debounce 400ms on!! -->
        <v-text-field
          prepend-inner-icon="mdi-magnify"
          variant="outlined"
          v-debounce="val => searchTerm = val"
          class="w-100 medium-input"
          placeholder="Search by name"
          clearable
        />
      </div>

      <h4>
        <span class="fa fa-info-circle ml-2 cursor-help">
          <html-tooltip :html="pageTip" location="bottom"/>
        </span>
      </h4>
    </div>

    <!-- roles table -->
    <v-data-table
      hover
      class="table-striped"
      hide-default-footer
      :search="searchTerm"
      :loading="loading"
      :headers="headers"
      :items="roleData"
      v-model:sort-by="sortBy"
      :no-data-text="emptyTableText"
      :items-per-page="-1"
    >
      <!-- customize column sizes -->
      <template #table-colgroup="scope">
        <col
            v-for="field in scope.fields"
            :key="field.key"
            :style="{ width: field.setWidth }"
        >
      </template>
      <!-- /customize column sizes -->

      <!-- members cell -->
      <template #item.members="{ item }">
        <UserDropdown :selected-tooltip="true"
          :role-id="item.value" @selected-users-updated="updateUserRole"
          :request-role-status="true" :initialize-selection-with-role="true"
          class="my-1"
          v-slot="{ count, filter, unknown }">
          <span>{{ userCountMemberString(count, unknown) }} with <strong>{{ item.text }}</strong></span>{{ filter ? ` (that match${count === 1 ? 'es' : ''} filter: "${filter}")` : '' }}
        </UserDropdown>
      </template> <!-- /members cell -->
    </v-data-table> <!-- /roles table -->

    <!-- roles error -->
    <v-alert v-if="error"
      color="error"
      @click:close="error = ''"
      closable
    >
      <span class="fa fa-exclamation-triangle" />&nbsp;
      {{ error }}
    </v-alert>
  </div>
</template>

<script>
import UserDropdown from './UserDropdown.vue';
import UserService from './UserService';
import HtmlTooltip from './HtmlTooltip.vue';
import { parseRoles, searchRoles } from './vueFilters';

export default {
  name: 'RolesCommon',
  components: {
    HtmlTooltip,
    UserDropdown
  },
  props: {
    currentUser: Object,
    cont3xtDarkTheme: Boolean
  },
  data () {
    return {
      sortBy: [{ key: 'text', order: 'desc' }],
      headers: [
        {
          title: 'Name',
          key: 'text',
          headerProps: { style: 'width: 10rem;' }
        },
        { // virtual members field
          title: 'Members',
          key: 'members',
          sortable: false
        }
      ],
      pageTip: 'These are roles you manage. Assign users to them with the dropdowns under <strong>Members</strong>.',
      error: '',
      searchTerm: ''
    };
  },
  computed: {
    loading () {
      return this.currentUser?.assignableRoles == null;
    },
    roleData () {
      const assignableRoles = this.currentUser?.assignableRoles || [];
      const roles = parseRoles(assignableRoles);
      return searchRoles(roles, this.searchTerm);
    },
    emptyTableText () {
      if (!this.searchTerm) { return 'No roles to manage'; }
      return 'No roles match your search';
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
    },
    userCountMemberString (count, unknown) {
      // the text shown on the member dropdowns
      return `${unknown ? '?' : count} ${count === 1 ? 'user' : 'users'}`;
    }
  }
};
</script>

<style scoped>

</style>
