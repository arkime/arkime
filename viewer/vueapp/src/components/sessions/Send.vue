<template>

  <!-- send sessions form -->
  <div class="row"
    @keyup.stop.prevent.enter="send">

    <SegmentSelect :segments.sync="segments" />

    <div class="col-md-5">

      <!-- tags input -->
      <div class="input-group input-group-sm">
        <div class="input-group-prepend">
          <span class="input-group-text">
            Tags
          </span>
        </div>
        <input
          type="text"
          v-model="tags"
          v-focus-input="true"
          class="form-control"
          placeholder="Enter a comma separated list of tags"
        />
      </div> <!-- /tags input -->

      <!-- error -->
      <p v-if="error"
        class="small text-danger mb-0">
        <span class="fa fa-exclamation-triangle">
        </span>&nbsp;
        {{ error }}
      </p> <!-- /error -->

    </div>

    <!-- buttons -->
    <div class="col-md-3">
      <div class="pull-right">
        <button
          type="button"
          @click="send"
          title="Send Session(s)"
          :class="{'disabled':loading}"
          class="btn btn-sm btn-theme-tertiary">
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
        <button
          type="button"
          title="cancel"
          v-b-tooltip.hover
          @click="done(null)"
          class="btn btn-sm btn-warning">
          <span class="fa fa-ban">
          </span>
        </button>
      </div>
    </div> <!-- /buttons -->

    <!-- info -->
    <div class="col-md-12">
      <p class="text-info small mb-0">
        <em>
          <strong>
            <span class="fa fa-info-circle"></span>&nbsp;
            This will send the SPI and PCAP data to the remote Arkime instance.
          </strong>
        </em>
      </p>
    </div> <!-- /info -->

  </div> <!-- /send sessions form -->

</template>

<script>
import FocusInput from '../utils/FocusInput';
import SessionsService from './SessionsService';
import SegmentSelect from './SegmentSelect';

export default {
  name: 'MolochTagSessions',
  directives: { FocusInput },
  components: { SegmentSelect },
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

      const data = {
        tags: this.tags,
        start: this.start,
        cluster: this.cluster,
        applyTo: this.applyTo,
        segments: this.segments,
        sessions: this.sessions,
        numVisible: this.numVisible,
        numMatching: this.numMatching
      };

      SessionsService.send(data, this.$route.query).then((response) => {
        this.tags = '';
        this.loading = false;

        let reloadData = false;
        //  only reload data if tags were added to only one
        if (data.sessions && data.sessions.length === 1) {
          reloadData = true;
        }

        this.done(response.data.text, response.data.success, reloadData);
      }).catch((error) => {
        // display the error under the form so that user
        // has an oportunity to try again (don't close the form)
        this.error = error.text;
        this.loading = false;
      });
    }
  }
};
</script>
