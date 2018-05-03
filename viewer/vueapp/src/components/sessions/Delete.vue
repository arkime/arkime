<template>

  <!-- delete sessions form -->
  <div class="row">

    <!-- deletion warning! -->
    <div class="col-md-4">
      <em class="small">
        <strong class="text-danger">
          <span class="fa fa-exclamation-triangle"></span>&nbsp;
          This will perform a three pass overwrite of all packet data for matching
          packets. SPI data will be non forensically removed for matching sessions.
        </strong>
      </em>
    </div> <!-- /deletion warning! -->

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
      <!-- delete error -->
      <p v-if="error"
        class="small text-danger mb-0">
        <span class="fa fa-exclamation-triangle">
        </span>&nbsp;
        {{ error }}
      </p> <!-- /delete error -->
    </div> <!-- /segments select input -->

    <!-- delete button -->
    <div class="col-md-3">
      <button class="btn btn-danger btn-sm pull-right"
        @click="deleteSessions()"
        :class="{'disabled':loading}"
        type="button">
        <span v-if="!loading">
          <span class="fa fa-trash-o">
          </span>&nbsp;
          Destructively Delete Data
        </span>
        <span v-else>
          <span class="fa fa-spinner fa-spin">
          </span>&nbsp;
          Destructively Deleting Data
        </span>
      </button>
    </div> <!-- /delete button -->

    <!-- cancel button -->
    <div class="col-md-1">
      <div class="btn btn-sm btn-warning pull-right"
        @click="done()">
        <span class="fa fa-ban">
        </span>&nbsp;
        Cancel
      </div>
    </div> <!-- /cancel button -->

  </div> <!-- /delete sessions form -->

</template>

<script>
import SessionsService from './SessionsService';

export default {
  name: 'MolochDeleteSessions',
  props: {
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
      segments: 'no'
    };
  },
  methods: {
    /* exposed functions ----------------------------------------- */
    deleteSessions: function () {
      this.loading = true;

      let data = {
        start: this.start,
        applyTo: this.applyTo,
        segments: this.segments,
        sessions: this.sessions,
        numVisible: this.numVisible,
        numMatching: this.numMatching
      };

      SessionsService.remove(data, this.$route.query)
        .then((response) => {
          this.loading = false;

          let reloadData = false;
          //  only reload data if only one was deleted
          if (data.sessions && data.sessions.length === 1) {
            reloadData = true;
          }

          // notify parent to close form
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
