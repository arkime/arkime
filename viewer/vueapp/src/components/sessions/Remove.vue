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
          Delete PCAP
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
        @click="deleteSessions"
        :class="{'disabled':loading}"
        type="button">
        <span v-if="!loading">
          <span class="fa fa-trash-o">
          </span>&nbsp;
          Remove Data
        </span>
        <span v-else>
          <span class="fa fa-spinner fa-spin">
          </span>&nbsp;
          Remove Data
        </span>
      </button>
    </div> <!-- /delete button -->

    <!-- cancel button -->
    <div class="col-md-1">
      <button class="btn btn-sm btn-warning pull-right"
        v-b-tooltip.hover
        title="cancel"
        @click="done(null)"
        type="button">
        <span class="fa fa-ban">
        </span>
      </button>
    </div> <!-- /cancel button -->

  </div> <!-- /delete sessions form -->

</template>

<script>
import SessionsService from './SessionsService';

export default {
  name: 'MolochRemoveData',
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
