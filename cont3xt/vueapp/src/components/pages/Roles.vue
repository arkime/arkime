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
        <div class="text-center margin-for-nav-and-progress">
          <span class="fa fa-circle-o-notch fa-spin fa-2x" />
          <p>Loading roles...</p>
        </div>
      </template> <!-- /loading overlay template -->

      <b-table
        v-if="!loading"
        small
        hover
        striped
        show-empty
        :dark="getDarkThemeEnabled"
        :fields="fields"
        :items="roleData"
        :sort-by.sync="sortBy"
        :sort-desc.sync="sortDesc"
        empty-text="There is no history to show"
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
            :role-id="data.item.value" @selected-users-updated="updateUsers"
            :request-role-status="true" :initialize-selection-with-role="true"
            v-slot="{ count, filter }">
            {{ count }} {{ count === 1 ? 'user' : 'users' }} with <strong>{{ data.item.text }}</strong>{{ filter ? ` (that match${count === 1 ? 'es' : ''} filter: "${filter}")` : '' }}
          </UserDropdown>
        </template> <!--   /members cell     -->
      </b-table>
    </b-overlay>
  </div>
</template>

<script>
import { mapGetters } from 'vuex';
import UserDropdown from '../../../../../common/vueapp/UserDropdown';
import UserService from '../../../../../common/vueapp/UserService';

export default {
  name: 'Roles',
  components: {
    UserDropdown
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
      pageTip: 'These are roles you manage. Assign users to them with the dropdowns under <strong>Members</strong>.'
    };
  },
  computed: {
    ...mapGetters(['getUser', 'getDarkThemeEnabled']),
    loading () {
      return this.getUser?.assignableRoles == null;
    },
    roleData () {
      const assignableRoles = this.getUser?.assignableRoles || [];
      return this.$options.filters.parseRoles(assignableRoles);
    }
  },
  methods: {
    /* page functions ------------------------------------------------------ */
    updateUsers ({ changedUser }, roleId) {
      UserService.updateUserRole({
        userId: changedUser.userId,
        roleId,
        newRoleState: changedUser.newState
      });
    }
  }
};
</script>

<style scoped>

</style>
