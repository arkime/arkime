<template>

  <div @keyup.stop.prevent.enter="exportCsv">

    <!-- export csv form -->
    <div class="row">

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

      <div class="col-md-4">

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
        </div> <!-- /filename input -->
        <!-- error -->
        <p v-if="error"
          class="small text-danger mb-0">
          <span class="fa fa-exclamation-triangle">
          </span>&nbsp;
          {{ error }}
        </p> <!-- /error -->

      </div>

      <!-- buttons -->
      <div class="col-md-4">
        <div class="pull-right">
          <button type="button"
            @click="toggleChangeFields"
            class="btn btn-sm btn-theme-secondary">
            Change Fields
          </button>
          <button class="btn btn-sm btn-theme-tertiary"
            @click="exportCsv"
            type="button">
            <span class="fa fa-paper-plane-o">
            </span>&nbsp;
            Export Csv
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
      </div> <!-- /buttons -->

    </div> <!-- /export csv form -->

    <div v-if="changeFields"
      class="row mt-1">
      <div class="col">
        <div class="input-group input-group-sm">
          <div class="input-group-prepend cursor-help"
            v-b-tooltip.hover
            title="Comma separated list of fields to export the intersection of">
            <span class="input-group-text">
              Fields
            </span>
          </div>
          <input type="text"
            class="form-control"
            v-model="exportFields"
          />
          <div class="input-group-prepend cursor-help"
            v-b-tooltip.hover
            title="This is a list of field expressions, please consult the help page for field expression values (click the owl, then the fields section)">
            <span class="input-group-text">
              <span class="fa fa-question-circle">
              </span>
            </span>
          </div>
        </div>
      </div>
    </div>

  </div>

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
    fields: Array
  },
  data: function () {
    return {
      error: '',
      segments: 'no',
      filename: 'sessions.csv',
      changeFields: false,
      exportFields: undefined
    };
  },
  mounted: function () {
    this.computeExportFields();
  },
  methods: {
    /* exposed functions ----------------------------------------- */
    toggleChangeFields: function () {
      this.changeFields = !this.changeFields;
    },
    exportCsv: function () {
      if (this.filename === '') {
        this.error = 'No filename specified.';
        return;
      }

      if (!this.exportFields) {
        this.error = 'No fields to export. Make sure the sessions table has columns.';
        return;
      }

      const data = {
        start: this.start,
        applyTo: this.applyTo,
        filename: this.filename,
        segments: this.segments,
        sessions: this.sessions,
        numVisible: this.numVisible,
        numMatching: this.numMatching,
        fields: this.exportFields
      };

      SessionsService.exportCsv(data, this.$route.query)
        .then((response) => {
          this.done(response.text, true);
        })
        .catch((error) => {
          this.error = error.text;
        });
    },
    /* helper functions ------------------------------------------ */
    /* compute the string of comma separated field db values */
    computeExportFields: function () {
      const fieldDbList = [];

      if (this.fields) {
        for (let i = 0; i < this.fields.length; ++i) {
          const field = this.fields[i];
          if (field.children) {
            for (let j = 0; j < field.children.length; ++j) {
              const child = field.children[j];
              if (child) { fieldDbList.push(child.dbField); }
            }
          } else {
            fieldDbList.push(field.dbField);
          }
        }
      }
      this.$set(this, 'exportFields', fieldDbList.join(','));
    }
  }
};
</script>
