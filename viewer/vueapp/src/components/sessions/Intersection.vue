<template>

  <!-- export intersection form -->
  <div>

    <div class="row"
      @keyup.stop.prevent.enter="openIntersection">
      <div class="col">

        <div class="pull-left mr-2">
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
        </div>

        <!-- buttons -->
        <div class="pull-right ml-2">
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
        </div> <!-- /buttons -->

      </div>
    </div>

    <div class="row mt-1">
      <div class="col">

        <!-- fields -->
        <div class="input-group input-group-sm fields-input">
          <div class="input-group-prepend cursor-help"
            v-b-tooltip.hover
            title="Comma separated list of fields to export the intersection of">
            <span class="input-group-text">
              Fields
            </span>
          </div>
          <input type="text"
            class="form-control"
            v-model="intersectionFields"
          />
          <div class="input-group-prepend cursor-help"
            v-b-tooltip.hover
            title="This is a list of field expressions, please consult the help page for field expression values (click the owl, then the fields section)">
            <span class="input-group-text">
              <span class="fa fa-question-circle">
              </span>
            </span>
          </div>
        </div> <!-- /fields -->

        <!-- error -->
        <p v-if="error"
          class="small text-danger mb-0">
          <span class="fa fa-exclamation-triangle">
          </span>&nbsp;
          {{ error }}
        </p> <!-- /error -->

      </div>
    </div>

  </div> <!-- /export intersection form -->

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
      sort: 'count',
      intersectionFields: undefined
    };
  },
  mounted: function () {
    this.computeIntersectionFields();
  },
  methods: {
    /* exposed functions ----------------------------------------- */
    openIntersection: function () {
      if (!this.intersectionFields) {
        this.error = 'No fields to display. Make sure there is a comma separated list of field expression values.';
        return;
      }

      const data = {
        exp: this.intersectionFields,
        counts: 0,
        sort: this.sort
      };

      if (this.counts) { data.counts = 1; }

      SessionsService.viewIntersection(data, this.$route.query);

      this.done('Intersection opened', true);
    },
    /* helper functions ------------------------------------------ */
    /* compute the string of comma separated field exp values */
    computeIntersectionFields: function () {
      const fieldExpList = [];

      for (const field of this.fields) {
        if (field.exp === 'info' || field.type === 'seconds') {
          continue;
        } else if (field.children) {
          for (const child of field.children) {
            fieldExpList.push(child.exp);
          }
        } else {
          fieldExpList.push(field.exp);
        }
      }

      this.$set(this, 'intersectionFields', fieldExpList.join(','));
    }
  }
};
</script>

<style scoped>
.fields-input > input {
  width: auto;
}
</style>
