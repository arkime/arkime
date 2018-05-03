<template>

  <!-- send sessions form -->
  <div class="row"
    @keyup.stop.prevent.enter="send()">

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
            @click="send()"
            :class="{'disabled':loading}"
            type="button">
            <span v-if="!loading">
              <span class="fa fa-paper-plane-o">
              </span>&nbsp;
              Send Session(s)
            </span>
            <span v-else>
              <span class="fa fa-spinner fa-spin">
              </span>&nbsp;
              Sending Session(s)
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

    <!-- info -->
    <div class="col-md-12">
      <p class="text-info small mb-0">
        <em>
          <strong>
            <span class="fa fa-info-circle"></span>&nbsp;
            This will send the SPI and PCAP data to the remote Moloch instance.
          </strong>
        </em>
      </p>
    </div> <!-- /info -->

  </div> <!-- /send sessions form -->

</template>

<script>
import FocusInput from '../utils/FocusInput';
import SessionsService from './SessionsService';

export default {
  name: 'MolochTagSessions',
  directives: { FocusInput },
  props: {
    cluster: String,
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
    send: function () {
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

      SessionsService.send(data, this.$route.query)
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
