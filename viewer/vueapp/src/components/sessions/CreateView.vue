<template>

  <!-- create view form -->
  <div class="row"
    @keyup.stop.prevent.enter="createView">

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
      @click="createView"
      :class="{'disabled':loading}"
      type="button">
      <span v-if="!loading">
        <span class="fa fa-plus-circle">
        </span>&nbsp;
        Create View
      </span>
      <span v-if="loading">
        <span class="fa fa-spinner fa-spin">
        </span>&nbsp;
        Creating View
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

  </div> <!-- /create view form -->

</template>

<script>
import UserService from '../users/UserService';
import FocusInput from '../utils/FocusInput';

export default {
  name: 'MolochCreateView',
  directives: { FocusInput },
  props: {
    done: Function
  },
  data: function () {
    return {
      name: '',
      loading: false,
      error: '',
      viewExpression: '',
      useColConfig: false
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
    createView: function () {
      if (!this.name) {
        this.error = 'No view name specified.';
        return;
      }

      if (!this.viewExpression) {
        this.error = 'No expression specified.';
        return;
      }

      this.loading = true;

      let data = {
        name: this.name,
        expression: this.viewExpression
      };

      if (this.useColConfig) {
        // save the current sessions table column configuration
        data.sessionsColConfig = this.$store.getters.sessionsTableState;
      }

      UserService.createView(data)
        .then((response) => {
          this.loading = false;
          // close the form and display success/error message
          this.done(response.text, response.success);
          // add the new view to the views dropdown
          this.$emit('newView', response.view, response.viewName);
        })
        .catch((error) => {
          // display the error under the form so that user
          // has an oportunity to try again (dont' close the form)
          this.error = error;
          this.loading = false;
        });
    }
  }
};
</script>
