<!--
Copyright Yahoo Inc.
SPDX-License-Identifier: Apache-2.0
-->
<template>
  <div class="container-fluid">
    <div class="d-flex justify-content-between mt-3 mb-2">
      <div class="me-2 flex-grow-1">
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
            :placeholder="$t('users.rolesSearchPlaceholder')" />
          <template #append>
            <b-button
              :disabled="!searchTerm"
              @click="searchTerm = ''"
              variant="outline-secondary">
              <span class="fa fa-close" />
            </b-button>
          </template>
        </b-input-group>
      </div>
      <h4>
        <span
          id="roles-page-tip"
          class="fa fa-info-circle ms-2 cursor-help">
          <BTooltip target="roles-page-tip">
            <span v-html="$t('roles.pageTip')" />
          </BTooltip>
        </span>
      </h4>
    </div>
    <b-overlay
      rounded="sm"
      blur="0.2rem"
      opacity="0.9"
      :show="loading"
      variant="transparent">
      <!-- loading overlay template -->
      <template #overlay>
        <slot name="loading">
          <div class="text-center">
            <span class="fa fa-circle-o-notch fa-spin fa-2x" />
            <p>{{ $t('common.loading') }}</p>
          </div>
        </slot>
      </template> <!-- /loading overlay template -->

      <BTable
        small
        hover
        striped
        show-empty
        :fields="fields"
        :items="roleData"
        :empty-text="emptyTableText">
        <!-- customize column sizes -->
        <template #table-colgroup="scope">
          <col
            v-for="field in scope.fields"
            :key="field.key"
            :style="{ width: field.setWidth }">
        </template>
        <!-- /customize column sizes -->

        <!-- members cell -->
        <template #cell(members)="data">
          <UserDropdown
            :selected-tooltip="true"
            :role-id="data.item.value"
            @selected-users-updated="updateUserRole"
            :request-role-status="true"
            :initialize-selection-with-role="true"
            v-slot="{ count, filter, unknown }">
            {{ $t(filter ? 'roles.summaryFilter' : 'roles.summary', {
              users: unknown ? '?' : $t('common.userCount', count),
              matches: $t('common.matchWordCount', count),
              filter}) }}
          </UserDropdown>
        </template> <!-- /members cell -->
      </BTable>
    </b-overlay>

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
      sortBy: 'text',
      sortDesc: true,
      fields: [
        {
          label: this.$t('roles.name'),
          key: 'text',
          setWidth: '10rem'
        },
        { // virtual members field
          label: this.$t('roles.members'),
          key: 'members'
        }
      ],
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
