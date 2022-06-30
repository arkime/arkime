<template>
  <div>
    <!-- modify view form -->
    <div class="d-flex flex-row"
      @keyup.stop.prevent.enter="modifyView">

      <!-- view name input -->
      <div>
        <div class="input-group input-group-sm">
          <div class="input-group-prepend">
            <span class="input-group-text">
              View Name
            </span>
          </div>
          <b-form-input
            autofocus
            type="text"
            maxlength="20"
            v-model="name"
            class="form-control"
            placeholder="Enter a (short) view name"
            v-on:keydown.enter="$event.stopPropagation()"
          />
        </div>
      </div> <!-- /view name input -->

      <!-- view expression input -->
      <div class="flex-grow-1 ml-2">
        <div class="input-group input-group-sm">
          <div class="input-group-prepend">
            <span class="input-group-text">
              Expression
            </span>
          </div>
          <input
            type="text"
            class="form-control"
            v-model="viewExpression"
            placeholder="Enter a query expression"
            v-on:keydown.enter="$event.stopPropagation()"
          />
        </div>
      </div> <!-- /view expression input -->

      <!-- view users input -->
      <div class="ml-2">
        <div class="input-group input-group-sm">
          <div class="input-group-prepend">
            <span class="input-group-text">
              Users
            </span>
          </div>
          <input
            type="text"
            v-model="viewUsers"
            class="form-control"
            v-on:keydown.enter="$event.stopPropagation()"
            placeholder="Enter a comma separated list of users who can view this view"
          />
        </div>
      </div> <!-- /view users input -->

      <!-- view roles input -->
      <div class="ml-2">
        <RoleDropdown
          :roles="roles"
          :selected-roles="viewRoles"
          display-text="Share with roles"
          @selected-roles-updated="updateViewRoles"
        />
      </div> <!-- /view roles input -->

      <!-- save sessions cols -->
      <div v-if="sessionsPage" class="ml-2">
        <div
          v-b-tooltip.hover
          class="form-check"
          title="Save the visible sessions table columns and sort order with this view. When applying this view, the sessions table will be updated.">
          <input
            type="checkbox"
            id="useColConfig"
            v-model="useColConfig"
            class="form-check-input">
          <label
            for="useColConfig"
            class="form-check-label">
            Save Columns
          </label>
        </div>
      </div> <!-- /save sessions cols -->

      <!-- cancel button -->
      <div class="ml-2">
        <button
          type="button"
          @click="modifyView"
          :class="{'disabled':loading}"
          class="btn btn-sm btn-theme-tertiary"
          :title="`${mode === 'create' ? 'Create View' : 'Save View'}`">
          <span v-if="!loading">
            <span v-if="mode === 'create'">
              <span class="fa fa-plus-circle" />&nbsp;
              Create View
            </span>
            <span v-else-if="mode === 'edit'">
              <span class="fa fa-save" />&nbsp;
              Save View
            </span>
          </span>
          <span v-if="loading">
            <span class="fa fa-spinner fa-spin" />&nbsp;
            <span v-if="mode === 'create'">
              Creating View
            </span>
            <span v-else-if="mode === 'edit'">
              Saving View
            </span>
          </span>
        </button>
        <div @click="done(null)"
          v-b-tooltip.hover="'cancel'"
          class="btn btn-sm btn-warning">
          <span class="fa fa-ban" />
        </div>
      </div> <!-- /cancel button -->

    </div> <!-- /modify view form -->

    <!-- error -->
    <div v-if="error"
      class="row small text-danger mb-0">
      <div class="col">
        <span class="fa fa-exclamation-triangle mr-1" />
        {{ error }}
      </div>
    </div> <!-- /error -->
  </div>
</template>

<script>
// services
import SettingsService from '../settings/SettingsService';
// components
import RoleDropdown from '../../../../../common/vueapp/RoleDropdown';

export default {
  name: 'MolochModifyView',
  components: {
    RoleDropdown
  },
  props: {
    done: Function,
    editView: Object,
    initialExpression: String
  },
  data () {
    return {
      error: '',
      loading: false,
      mode: (this.editView) ? 'edit' : 'create',
      name: (this.editView) ? this.editView.name : '',
      viewUsers: (this.editView && this.editView.users) ? this.editView.users : '',
      viewRoles: (this.editView && this.editView.roles) ? this.editView.roles : [],
      useColConfig: (this.editView && (this.editView.sessionsColConfig !== undefined)),
      viewExpression: (this.editView) ? this.editView.expression : (this.initialExpression || '')
    };
  },
  computed: {
    // only display the useColConfig checkbox on the sessions page
    sessionsPage () {
      return this.$route.name === 'Sessions';
    },
    appliedView () {
      return this.$route.query.view || undefined;
    },
    roles () {
      return this.$store.state.roles;
    }
  },
  methods: {
    /* exposed functions ----------------------------------------- */
    updateViewRoles (roles) {
      this.viewRoles = roles;
    },
    modifyView () {
      if (!this.name) {
        this.error = 'No view name specified.';
        return;
      }

      if (!this.viewExpression) {
        this.error = 'No expression specified.';
        return;
      }

      this.loading = true;

      this.mode === 'edit' ? this.updateView() : this.createView();
    },
    createView () {
      const data = {
        name: this.name,
        users: this.viewUsers,
        roles: this.viewRoles,
        expression: this.viewExpression
      };

      if (this.useColConfig) {
        // save the current sessions table column configuration
        data.sessionsColConfig = JSON.parse(JSON.stringify(this.$store.getters.sessionsTableState));
      }

      SettingsService.createView(data, undefined).then((response) => {
        this.loading = false;
        // close the form and display success/error message
        this.done(response.text, response.success);
        // add the new view to the views dropdown
        this.$store.commit('addView', response.view);
      }).catch((error) => {
        // display the error under the form so that user
        // has an opportunity to try again (don't close the form)
        this.error = error;
        this.loading = false;
      });
    },
    updateView () {
      const data = JSON.parse(JSON.stringify(this.editView));

      data.name = this.name;
      data.users = this.viewUsers;
      data.roles = this.viewRoles;
      data.expression = this.viewExpression;

      if (this.useColConfig === true) {
        // save the current sessions table column configuration
        const tableClone = JSON.parse(JSON.stringify(this.$store.getters.sessionsTableState));
        data.sessionsColConfig = tableClone;
      } else if (data.sessionsColConfig) {
        // If unselected, delete table cols
        delete data.sessionsColConfig;
      }

      SettingsService.updateView(data, undefined).then((response) => {
        this.loading = false;
        // close the form and display success/error message
        this.done(response.text, response.success);
        SettingsService.getViews();
      }).catch((error) => {
        // display the error under the form so that user
        // has an opportunity to try again (don't close the form)
        this.error = error;
        this.loading = false;
      });
    }
  }
};
</script>
