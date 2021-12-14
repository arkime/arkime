<template>

  <!-- modify view form -->
  <div class="row"
    @keyup.stop.prevent.enter="modifyView">

    <!-- view name input -->
    <div class="col-md-3">
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

    <div class="col-md-5 pl-0"
      :class="{'col-md-5':sessionsPage,'col-md-6':!sessionsPage}">

      <!-- view expression input -->
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
      </div> <!-- /view expression input -->

      <!-- error -->
      <p v-if="error"
        class="small text-danger mb-0">
        <span class="fa fa-exclamation-triangle">
        </span>&nbsp;
        {{ error }}
      </p> <!-- /error -->

    </div>

    <div v-if="sessionsPage"
      class="col-md-1 no-wrap">
      <div
        v-b-tooltip.hover
        class="form-check small mt-1 pl-0"
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
    </div>

    <!-- cancel button -->
    <div class="col-md-3">
      <button
        type="button"
        @click="modifyView"
        :class="{'disabled':loading}"
        class="btn btn-sm btn-theme-tertiary pull-right ml-1"
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
      <div
        @click="done(null)"
        class="btn btn-sm btn-warning pull-right">
        <span class="fa fa-ban" />
        <span class="d-sm-none d-md-none d-lg-none d-xl-inline">
          &nbsp;Cancel
        </span>
      </div>
    </div> <!-- /cancel button -->

  </div> <!-- /modify view form -->

</template>

<script>
import UserService from '../users/UserService';

export default {
  name: 'MolochModifyView',
  props: {
    editView: Object,
    initialExpression: String,
    done: Function
  },
  data: function () {
    return {
      mode: (this.editView) ? 'edit' : 'create',
      name: (this.editView) ? this.editView.name : '',
      viewExpression: (this.editView) ? this.editView.expression : (this.initialExpression || ''),
      useColConfig: (this.editView && (this.editView.sessionsColConfig !== undefined)),
      loading: false,
      error: ''
    };
  },
  computed: {
    // only display the useColConfig checkbox on the sessions page
    sessionsPage: function () {
      return this.$route.name === 'Sessions';
    },
    appliedView: function () {
      return this.$route.query.view || undefined;
    }
  },
  methods: {
    /* exposed functions ----------------------------------------- */
    modifyView: function () {
      if (!this.name) {
        this.error = 'No view name specified.';
        return;
      }

      if (!this.viewExpression) {
        this.error = 'No expression specified.';
        return;
      }

      this.loading = true;

      if (this.mode === 'create') {
        this.createView();
      } else if (this.mode === 'edit') {
        this.updateView();
      }
    },
    createView: function () {
      const data = {
        name: this.name,
        expression: this.viewExpression
      };

      if (this.useColConfig) {
        // save the current sessions table column configuration
        data.sessionsColConfig = JSON.parse(JSON.stringify(this.$store.getters.sessionsTableState));
      }

      UserService.createView(data).then((response) => {
        this.loading = false;
        // close the form and display success/error message
        this.done(response.text, response.success);
        // add the new view to the views dropdown
        this.$store.commit('addViews', data);
        this.$emit('setView', data.name);
      }).catch((error) => {
        // display the error under the form so that user
        // has an opportunity to try again (don't close the form)
        this.error = error;
        this.loading = false;
      });
    },
    updateView: function (key) {
      const data = JSON.parse(JSON.stringify(this.editView));

      data.expression = this.viewExpression;
      data.name = this.name;

      if (this.useColConfig === true) {
        // save the current sessions table column configuration
        const tableClone = JSON.parse(JSON.stringify(this.$store.getters.sessionsTableState));
        data.sessionsColConfig = tableClone;
      } else if (data.sessionsColConfig) {
        // If unselected, delete table cols
        delete data.sessionsColConfig;
      }

      // key is always old name if data.name changes
      data.key = this.editView.name;

      UserService.updateView(data, this.userId).then((response) => {
        this.loading = false;
        // close the form and display success/error message
        this.done(response.text, response.success);
        // update the view
        this.$store.commit('updateViews', JSON.parse(JSON.stringify(data)));
        if (this.appliedView === data.key) {
          // if this is the applied view, make sure it is still applied
          this.$emit('setView', data.name);
        }
        // display success message to user
        this.msg = response.text;
        this.msgType = 'success';
      }).catch((error) => {
        this.loading = false;
        // display error message to user
        this.msg = error.text;
        this.msgType = 'danger';
      });
    }
  }
};
</script>
