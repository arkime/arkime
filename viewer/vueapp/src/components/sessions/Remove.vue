<template>

  <!-- delete sessions form -->
  <div class="row">

    <div class="col-md-4">
      <div class="form-check form-check-inline"
        v-b-tooltip.hover
        title="Perform a three pass overwrite of all packet data for matching sessions.">
        <input type="checkbox"
          class="form-check-input"
          v-model="pcap"
          id="pcap"
        />
        <label class="form-check-label"
          for="pcap">
          Scrub PCAP
        </label>
      </div>
      <div class="form-check form-check-inline"
        v-b-tooltip.hover
        title="Non forensically remove SPI data for matching sessions.">
        <input type="checkbox"
          class="form-check-input"
          v-model="spi"
          id="spi"
        />
        <label class="form-check-label"
          for="spi">
          Delete SPI Data
        </label>
      </div>
    </div>

    <!-- segments select input -->
    <SegmentSelect :segments.sync="segments">
      <!-- delete error -->
      <p v-if="error"
        class="small text-danger mb-0">
        <span class="fa fa-exclamation-triangle">
        </span>&nbsp;
        {{ error }}
      </p> <!-- /delete error -->
    </SegmentSelect>

    <!-- buttons -->
    <div class="col-md-4">
      <div class="pull-right">
        <!-- delete button -->
        <button
          type="button"
          title=" Remove Data"
          @click="deleteSessions"
          :class="{'disabled':loading}"
          class="btn btn-danger btn-sm">
          <span v-if="!loading">
            <span class="fa fa-trash-o">
            </span>&nbsp;
            Remove Data
          </span>
          <span v-else>
            <span class="fa fa-spinner fa-spin">
            </span>&nbsp;
            Removing Data
          </span>
        </button> <!-- /delete button -->
        <!-- cancel button -->
        <button class="btn btn-sm btn-warning"
          v-b-tooltip.hover
          title="cancel"
          @click="done(null)"
          type="button">
          <span class="fa fa-ban">
          </span>
        </button> <!-- /cancel button -->
      </div>
    </div> <!-- /buttons -->

  </div> <!-- /delete sessions form -->

</template>

<script>
import SessionsService from './SessionsService';
import SegmentSelect from './SegmentSelect';

export default {
  name: 'MolochRemoveData',
  components: { SegmentSelect },
  props: {
    start: Number,
    done: Function,
    single: Boolean,
    applyTo: String,
    sessions: Array,
    numVisible: Number,
    numMatching: Number
  },
  data: function () {
    return {
      error: '',
      spi: false,
      pcap: true,
      loading: false,
      segments: 'no'
    };
  },
  methods: {
    /* exposed functions ----------------------------------------- */
    deleteSessions: function () {
      this.loading = true;

      const data = {
        start: this.start,
        removeSpi: this.spi,
        removePcap: this.pcap,
        applyTo: this.applyTo,
        segments: this.segments,
        sessions: this.sessions,
        numVisible: this.numVisible,
        numMatching: this.numMatching
      };

      SessionsService.remove(data, this.$route.query)
        .then((response) => {
          this.loading = false;
          // notify parent to close form
          this.done(response.data.text, response.data.success, this.single && this.pcap && !this.spi);
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
