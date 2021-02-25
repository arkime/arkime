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
        <input v-model="name"
          v-focus-input="true"
          type="text"
          maxlength="20"
          class="form-control"
          v-on:keydown.enter="$event.stopPropagation()"
          placeholder="Enter a (short) view name"
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
        <input v-model="viewExpression"
          type="text"
          class="form-control"
          v-on:keydown.enter="$event.stopPropagation()"
          placeholder="Enter a query expression"
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
      <div class="form-check small mt-1 pl-0"
        v-b-tooltip.hover
        title="Save the visible sessions table columns and sort order with this view. When applying this view, the sessions table will be updated.">
        <input v-model="useColConfig"
          class="form-check-input"
          type="checkbox"
          id="useColConfig">
        <label class="form-check-label"
          for="useColConfig">
          Save Columns
        </label>
      </div>
    </div>

    <!-- cancel button -->
    <div class="col-md-3">
    <button class="btn btn-sm btn-theme-tertiary pull-right ml-1"
      @click="modifyView"
      :class="{'disabled':loading}"
      type="button">
      <span v-if="!loading">
        <span v-if="mode === 'create'">
          <span class="fa fa-plus-circle">
          </span>&nbsp;
          Create View
        </span>
        <span v-else-if="mode === 'edit'">
          <span class="fa fa-save">
          </span>&nbsp;
          Save Edits
        </span>
      </span>
      <span v-if="loading">
        <span class="fa fa-spinner fa-spin">
        </span>&nbsp;

        <span v-if="mode === 'create'">
          Creating View
        </span>
        <span v-else-if="mode === 'edit'">
          Saving View
        </span>
      </span>
    </button>
      <div class="btn btn-sm btn-warning pull-right"
        @click="done(null)">
        <span class="fa fa-ban">
        </span>
        <span class="d-sm-none d-md-none d-lg-none d-xl-inline">
          &nbsp;Cancel
        </span>
      </div>
    </div> <!-- /cancel button -->

  </div> <!-- /modify view form -->

</template>

<script>
import UserService from '../users/UserService';
import FocusInput from '../utils/FocusInput';

export default {
  name: 'MolochModifyView',
  directives: { FocusInput },
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

      UserService.createView(data)
        .then((response) => {
          this.loading = false;
          // close the form and display success/error message
          this.done(response.text, response.success);
          // add the new view to the views dropdown
          this.$store.commit('addViews', data);
          this.$emit('setView', data.name);
        })
        .catch((error) => {
          // display the error under the form so that user
          // has an oportunity to try again (dont' close the form)
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

      this.$store.commit('updateViews', JSON.parse(JSON.stringify(data)));

      UserService.updateView(data, this.userId)
        .then((response) => {
          this.loading = false;
          // close the form and display success/error message
          this.done(response.text, response.success);
          // display success message to user
          this.msg = response.text;
          this.msgType = 'success';
        })
        .catch((error) => {
          this.loading = false;
          // display error message to user
          this.msg = error.text;
          this.msgType = 'danger';
        });
    }
  }
};
</script>
