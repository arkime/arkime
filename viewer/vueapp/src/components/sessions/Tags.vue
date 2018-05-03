<template>

  <!-- tag sessions form -->
  <div class="row"
    @keyup.stop.prevent.enter="apply(add)">

    <!-- segments select input -->
    <div class="col-md-4">
      <div class="input-group input-group-sm">
        <div class="input-group-prepend">
          <span class="input-group-text">
            Include
          </span>
        </div>
        <select v-model="segments"
          class="form-control"
          style="-webkit-appearance:none;">
          <option value="no">no</option>
          <option value="all">all</option>
          <option value="time">same time period</option>
        </select>
        <div class="input-group-append">
          <span class="input-group-text">
            linked segments (slow)
          </span>
        </div>
      </div>
    </div> <!-- /segments select input -->

    <div class="col-md-7">

      <!-- tags input -->
      <div class="input-group input-group-sm">
        <div class="input-group-prepend">
          <span class="input-group-text">
            Tags
          </span>
        </div>
        <input v-model="tags"
          v-focus-input="true"
          type="text"
          class="form-control"
          placeholder="Enter a comma separated list of tags"
        />
        <div class="input-group-append">
          <button class="btn btn-theme-tertiary"
            v-if="add"
            @click="apply(true)"
            :class="{'disabled':loading}"
            type="button">
            <span v-if="!loading">
              <span class="fa fa-plus-circle">
              </span>&nbsp;
              Add Tags
            </span>
            <span v-else>
              <span class="fa fa-spinner fa-spin">
              </span>&nbsp;
              Adding Tags
            </span>
          </button>
          <button class="btn btn-danger"
            v-else
            @click="apply(false)"
            :class="{'disabled':loading}"
            type="button">
            <span v-if="!loading">
              <span class="fa fa-trash-o">
              </span>&nbsp;
              Remove Tags
            </span>
            <span v-else>
              <span class="fa fa-spinner fa-spin">
              </span>&nbsp;
              Removing Tags
            </span>
          </button>
        </div>
      </div> <!-- /tags input -->

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

  </div> <!-- /tag sessions form -->

</template>

<script>
import FocusInput from '../utils/FocusInput';
import SessionsService from './SessionsService';

export default {
  name: 'MolochTagSessions',
  directives: { FocusInput },
  props: {
    add: Boolean,
    start: Number,
    done: Function,
    applyTo: String,
    sessions: Array,
    numVisible: Number,
    numMatching: Number
  },
  data: function () {
    return {
      error: '',
      loading: false,
      segments: 'no',
      tags: ''
    };
  },
  methods: {
    /* exposed functions ----------------------------------------- */
    apply: function (addTags) {
      if (!this.tags) {
        this.error = 'No tag(s) specified.';
        return;
      }

      this.loading = true;

      let data = {
        tags: this.tags,
        start: this.start,
        applyTo: this.applyTo,
        segments: this.segments,
        sessions: this.sessions,
        numVisible: this.numVisible,
        numMatching: this.numMatching
      };

      SessionsService.tag(addTags, data, this.$route.query)
        .then((response) => {
          this.tags = '';
          this.loading = false;

          let reloadData = false;
          //  only reload data if tags were added to only one
          if (data.sessions && data.sessions.length === 1) {
            reloadData = true;
          }

          this.done(response.data.text, response.data.success, reloadData);
        })
        .catch((error) => {
          // display the error under the form so that user
          // has an oportunity to try again (don't close the form)
          this.error = error.text;
          this.loading = false;
        });
    }
  }
};
</script>
