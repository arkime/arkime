<!--
Copyright Yahoo Inc.
SPDX-License-Identifier: Apache-2.0
-->
<template>

  <!-- export pcap form -->
  <div class="row"
    @keyup.stop.prevent.enter="exportPcap">

    <!-- segments select input -->
    <SegmentSelect :segments.sync="segments"/> <!-- /segments select input -->
    <div class="col-md-5">

      <!-- filename input -->
      <div class="input-group input-group-sm">
        <div class="input-group-prepend">
          <span class="input-group-text">
            Filename
          </span>
        </div>
        <b-form-input
          autofocus
          type="text"
          v-model="filename"
          class="form-control"
          placeholder="Enter a filename"
        />
      </div> <!-- /filename input -->

      <!-- error -->
      <p v-if="error"
        class="small text-danger mb-0">
        <span class="fa fa-exclamation-triangle">
        </span>&nbsp;
        {{ error }}
      </p> <!-- /error -->

    </div>

    <!-- cancel button -->
    <div class="col-md-3">
      <div class="pull-right">
        <button class="btn btn-sm btn-theme-tertiary"
          title="Export PCAP"
          @click="exportPcap"
          type="button">
          <span class="fa fa-paper-plane-o">
          </span>&nbsp;
          Export PCAP
        </button>
        <button class="btn btn-sm btn-warning"
          v-b-tooltip.hover
          title="cancel"
          @click="done(null)"
          type="button">
          <span class="fa fa-ban">
          </span>
        </button>
      </div>
    </div> <!-- /cancel button -->

  </div> <!-- /export pcap form -->

</template>

<script>
import SessionsService from './SessionsService';
import SegmentSelect from './SegmentSelect';

export default {
  name: 'MolochExportPcap',
  components: { SegmentSelect },
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
      segments: 'no',
      filename: 'sessions.pcap'
    };
  },
  methods: {
    /* exposed functions ----------------------------------------- */
    exportPcap: function () {
      if (this.filename === '') {
        this.error = 'No filename specified.';
        return;
      }

      const data = {
        start: this.start,
        applyTo: this.applyTo,
        filename: this.filename,
        segments: this.segments,
        sessions: this.sessions,
        numVisible: this.numVisible,
        numMatching: this.numMatching
      };

      SessionsService.exportPcap(data, this.$route.query)
        .then((response) => {
          this.done(response.text, true);
        })
        .catch((error) => {
          this.error = error.text;
        });
    }
  }
};
</script>
