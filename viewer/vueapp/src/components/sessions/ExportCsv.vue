<template>

  <!-- export csv form -->
  <div class="row"
    @keyup.stop.prevent.enter="exportPcap()">

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

      <!-- filename input -->
      <div class="input-group input-group-sm">
        <div class="input-group-prepend">
          <span class="input-group-text">
            Filename
          </span>
        </div>
        <input v-model="filename"
          v-focus-input="true"
          type="text"
          class="form-control"
          placeholder="Enter a filename"
        />
        <div class="input-group-append">
          <button class="btn btn-theme-tertiary"
            @click="exportCsv()"
            type="button">
            <span class="fa fa-paper-plane-o">
            </span>&nbsp;
            Export Csv
          </button>
        </div>
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
    <div class="col-md-1">
      <div class="btn btn-sm btn-warning pull-right"
        @click="done()">
        <span class="fa fa-ban">
        </span>&nbsp;
        Cancel
      </div>
    </div> <!-- /cancel button -->

  </div> <!-- /export csv form -->

</template>

<script>
import FocusInput from '../utils/FocusInput';
import SessionsService from './SessionsService';

export default {
  name: 'MolochExportCsv',
  directives: { FocusInput },
  props: {
    start: Number,
    done: Function,
    applyTo: String,
    sessions: Array,
    numVisible: Number,
    numMatching: Number,
    Fields: Array
  },
  data: function () {
    return {
      error: '',
      segments: 'no',
      filename: 'sessions.csv'
    };
  },
  methods: {
    /* exposed functions ----------------------------------------- */
    exportCsv: function () {
      if (this.filename === '') {
        this.error = 'No filename specified.';
        return;
      }

      let data = {
        start: this.start,
        applyTo: this.applyTo,
        filename: this.filename,
        segments: this.segments,
        sessions: this.sessions,
        numVisible: this.numVisible,
        numMatching: this.numMatching,
        fields: []
      };

      if (this.fields) {
        for (let i = 0; i < this.fields.length; ++i) {
          let field = this.fields[i];
          if (field.children) {
            for (let j = 0; j < field.children.length; ++j) {
              let child = field.children[j];
              if (child) { data.fields.push(child.dbField); }
            }
          } else {
            data.fields.push(field.dbField);
          }
        }
      }

      SessionsService.exportPcap(data, this.$route.query);

      this.done('CSV Exported', true);
    }
  }
};
</script>
