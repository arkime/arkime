<template>
  <div class="container-fluid">
    <span class="d-inline-flex align-items-center">
      <h1>Role Management</h1>
      <h5>
        <span class="fa fa-info-circle ml-2" v-b-tooltip.hover.html="pageTip"/>
      </h5>
    </span>
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
        empty-text="No roles to manage"
      >
        <!--   customize column sizes   -->
        <template #table-colgroup="scope">
          <col
              v-for="field in scope.fields"
              :key="field.key"
              :style="{ width: field.setWidth }"
          >
        </template>
        <!--   /customize column sizes   -->

        <!--   members cell     -->
        <template #cell(members)="data">
          <UserDropdown :selected-tooltip="true"
            :role-id="data.item.value" @selected-users-updated="updateUserRole"
            :request-role-status="true" :initialize-selection-with-role="true"
            v-slot="{ count, filter, unknown }">
            {{ userCountMemberString(count, unknown) }} with <strong>{{ data.item.text }}</strong>{{ filter ? ` (that match${count === 1 ? 'es' : ''} filter: "${filter}")` : '' }}
          </UserDropdown>
        </template> <!--   /members cell     -->
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
      error: ''
    };
  },
  computed: {
    loading () {
      return this.currentUser?.assignableRoles == null;
    },
    roleData () {
      const assignableRoles = this.currentUser?.assignableRoles || [];
      return this.$options.filters.parseRoles(assignableRoles);
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
