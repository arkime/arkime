<template>

  <!-- export csv form -->
  <div class="row"
    @keyup.stop.prevent.enter="openIntersection">

    <div class="col-md-7">

      <div class="form-check form-check-inline">
        <input type="checkbox"
          class="form-check-input"
          v-model="counts"
          id="counts"
        />
        <label class="form-check-label"
          for="counts">
          Include counts
        </label>
      </div>

      <div class="form-check form-check-inline ml-2">
        <input class="form-check-input"
          type="radio"
          name="sort"
          id="countSort"
          value="count"
          v-model="sort"
        />
        <label class="form-check-label"
          for="countSort">
          Count sort
        </label>
      </div>
      <div class="form-check form-check-inline">
        <input class="form-check-input"
          type="radio"
          name="sort"
          id="fieldSort"
          value="field"
          v-model="sort"
        />
        <label class="form-check-label"
          for="fieldSort">
          Field sort
        </label>
      </div>

      <!-- error -->
      <p v-if="error"
        class="small text-danger mb-0">
        <span class="fa fa-exclamation-triangle">
        </span>&nbsp;
        {{ error }}
      </p> <!-- /error -->

    </div>

    <!-- cancel button -->
    <div class="col-md-5">
      <button class="btn btn-sm btn-warning pull-right"
        v-b-tooltip.hover
        title="cancel"
        @click="done(null)"
        type="button">
        <span class="fa fa-ban">
        </span>
      </button>
      <button class="btn btn-sm btn-theme-tertiary pull-right mr-1"
        @click="openIntersection"
        type="button">
        <span class="fa fa-venn">
          <span class="fa fa-circle-o">
          </span>
          <span class="fa fa-circle-o">
          </span>
        </span>&nbsp;
        Export Intersection
      </button>
    </div> <!-- /cancel button -->

  </div> <!-- /export csv form -->

</template>

<script>
import SessionsService from './SessionsService';

export default {
  name: 'MolochIntersection',
  props: {
    done: Function,
    fields: Array
  },
  data: function () {
    return {
      error: '',
      counts: true,
      sort: 'count'
    };
  },
  methods: {
    /* exposed functions ----------------------------------------- */
    openIntersection: function () {
      if (!this.fields) {
        this.error = 'No fields to display. Make sure the sessions table has columns.';
      }

      let data = {
        exp: [],
        counts: 0,
        sort: this.sort
      };

      if (this.counts) { data.counts = 1; }

      for (let field of this.fields) {
        if (field.exp === 'info' || field.type === 'seconds') {
          continue;
        } else if (field.children) {
          for (let child of field.children) {
            data.exp.push(child.exp);
          }
        } else {
          data.exp.push(field.exp);
        }
      }

      data.exp = data.exp.join(',');

      SessionsService.viewIntersection(data, this.$route.query);

      this.done('Intersection opened', true);
    }
  }
};
</script>
