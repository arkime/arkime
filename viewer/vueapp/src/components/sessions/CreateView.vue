<template>

  <!-- create view form -->
  <div class="row"
    @keyup.stop.prevent.enter="createView">

    <!-- view name input -->
    <div class="col-md-4">
      <div class="input-group input-group-sm">
        <div class="input-group-prepend">
          <span class="input-group-text">
            View Name
          </span>
        </div>
        <input v-model="viewName"
          v-focus-input="true"
          type="text"
          maxlength="20"
          class="form-control"
          placeholder="Enter a (short) view name"
        />
      </div>
    </div> <!-- /view name input -->

    <div class="col-md-7">

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
        <div class="input-group-append">
          <button class="btn btn-theme-tertiary"
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
        </div>
      </div> <!-- /view expression input -->

      <!-- error -->
      <p v-if="error"
        class="small text-danger mb-0">
        <span class="fa fa-exclamation-triangle">
        </span>&nbsp;
        {{ error }}
      </p> <!-- /error -->

    </div>

    <!-- cancel button -->
    <div class="col-md-1">
      <div class="btn btn-sm btn-warning pull-right"
        @click="done()">
        <span class="fa fa-ban">
        </span>&nbsp;
        Cancel
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
      viewName: '',
      loading: false,
      error: '',
      viewExpression: ''
    };
  },
  methods: {
    /* exposed functions ----------------------------------------- */
    createView: function () {
      if (!this.viewName) {
        this.error = 'No view name specified.';
        return;
      }

      if (!this.viewExpression) {
        this.error = 'No expression specified.';
        return;
      }

      this.loading = true;

      let data = {
        viewName: this.viewName,
        expression: this.viewExpression
      };

      UserService.createView(data)
        .then((response) => {
          this.loading = false;
          // close the form and display success/error message
          this.done(response.text, response.success);
          // add the new view to the views dropdown
          this.$emit('newView', response.views);
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
