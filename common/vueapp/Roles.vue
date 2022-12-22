<template>
  <div class="container-fluid">
    <div class="d-flex justify-content-between mt-3 mb-2">
      <div class="mr-2 flex-grow-1">
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
            placeholder="Begin typing to search for roles"
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
      <h4>
        <span
          v-b-tooltip.hover.html="pageTip"
          class="fa fa-info-circle ml-2 cursor-help"
        />
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
          <div class="text-center margin-for-nav-and-progress">
            <span class="fa fa-circle-o-notch fa-spin fa-2x" />
            <p>Loading roles...</p>
          </div>
        </slot>
      </template> <!-- /loading overlay template -->

      <b-table
        small
        hover
        striped
        show-empty
        :dark="cont3xtDarkTheme"
        :fields="fields"
        :items="roleData"
        :sort-by.sync="sortBy"
        :sort-desc.sync="sortDesc"
        :empty-text="emptyTableText"
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
        <template #cell(members)="data">
          <UserDropdown :selected-tooltip="true"
            :role-id="data.item.value" @selected-users-updated="updateUserRole"
            :request-role-status="true" :initialize-selection-with-role="true"
            v-slot="{ count, filter, unknown }">
            {{ userCountMemberString(count, unknown) }} with <strong>{{ data.item.text }}</strong>{{ filter ? ` (that match${count === 1 ? 'es' : ''} filter: "${filter}")` : '' }}
          </UserDropdown>
        </template> <!-- /members cell -->
      </b-table>
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
import UserDropdown from './UserDropdown';
import UserService from './UserService';

export default {
  name: 'RolesCommon',
  components: {
    UserDropdown
  },
  props: {
    currentUser: Object,
    cont3xtDarkTheme: Boolean
  },
  data () {
    return {
      sortBy: 'text',
      sortDesc: true,
      fields: [
        {
          label: 'Name',
          key: 'text',
          setWidth: '10rem'
        },
        { // virtual members field
          label: 'Members',
          key: 'members'
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
      const roles = this.$options.filters.parseRoles(assignableRoles);
      return this.$options.filters.searchRoles(roles, this.searchTerm);
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
