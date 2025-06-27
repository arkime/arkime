<!--
Copyright Yahoo Inc.
SPDX-License-Identifier: Apache-2.0
-->
<template>
  <div @keyup.stop.prevent.enter="exportCsvAction">

    <div class="row">

      <SegmentSelect v-model:segments="segments" />

      <div class="col-md-4">
        <div class="input-group input-group-sm">
          <span class="input-group-text">
            Filename
          </span>
          <b-form-input
            autofocus
            type="text"
            :model-value="filename"
            class="form-control"
            placeholder="Enter a filename"
            @update:model-value="filename = $event"
          />
        </div> <p v-if="error"
          class="small text-danger mb-0">
          <span class="fa fa-exclamation-triangle me-2"></span>
          {{ error }}
        </p>
      </div>

      <div class="col-md-4">
        <div class="pull-right">
          <button type="button"
            @click="toggleChangeFields"
            class="btn btn-sm btn-theme-secondary me-1">
            Change Fields
          </button>
          <button
            type="button"
            @click="exportCsvAction"
            class="btn btn-sm btn-theme-tertiary me-1">
            <span class="fa fa-paper-plane-o me-2"></span>
            Export CSV
          </button>
          <button id="cancelExportCsv"
            class="btn btn-sm btn-warning me-1"
            @click="$emit('done', null, false, false)"
            type="button">
            <span class="fa fa-ban"></span>
            <BTooltip target="cancelExportCsv">Cancel</BTooltip>
          </button>
        </div>
      </div>
    </div>

    <div v-if="changeFields"
      class="row mt-1">
      <div class="col">
        <div class="input-group input-group-sm">
          <div id="exportFields"
             class="input-group-text cursor-help">
              <Fieldset></Fieldset> <BTooltip target="exportFields">Comma separated list of fields to export (in database field format - see help page)</BTooltip>
          </div>
          <input type="text"
            class="form-control"
            :model-value="exportFields"
            @update:model-value="exportFields = $event"
            placeholder="Comma separated list of fields (in database field format - see help page)"
          />
          <div id="exportFieldsHelp"
            class="input-group-text cursor-help">
            <span class="fa fa-question-circle"></span>
            <BTooltip target="exportFieldsHelp">This is a list of Database Fields, please consult the help page for field Database values (click the owl, then the fields section)</BTooltip>
          </div>
        </div>
      </div>
    </div>

  </div>
</template>

<script setup>
import { ref, onMounted, watch } from 'vue';
import { useRoute } from 'vue-router';
import SessionsService from './SessionsService';
import SegmentSelect from './SegmentSelect.vue';

// Define Props
const props = defineProps({
  start: Number,
  applyTo: String,
  sessions: Array,
  numVisible: Number,
  numMatching: Number,
  fields: Array
});

// Define Emits
const emit = defineEmits(['done']);

// Reactive state
const error = ref('');
const segments = ref('no');
const filename = ref('sessions.csv');
const changeFields = ref(false);
const exportFields = ref(''); // Initialize as empty string, will be computed

// Access route
const route = useRoute();

// Helper function to compute export fields string
const computeExportFields = () => {
  const fieldDbList = [];
  if (props.fields) {
    for (const field of props.fields) {
      if (field.children) {
        for (const child of field.children) {
          if (child && child.dbField) { // Added check for child.dbField
            fieldDbList.push(child.dbField);
          }
        }
      } else if (field.dbField) { // Added check for field.dbField
        fieldDbList.push(field.dbField);
      }
    }
  }
  exportFields.value = fieldDbList.join(',');
};

// Lifecycle hooks
onMounted(() => {
  computeExportFields();
});

// Watch for changes in props.fields if they can change after mount
watch(() => props.fields, () => {
  computeExportFields();
}, { deep: true });

// Methods
const toggleChangeFields = () => {
  changeFields.value = !changeFields.value;
};

const exportCsvAction = async () => {
  if (filename.value === '') {
    error.value = 'No filename specified.';
    return;
  }

  if (!exportFields.value) {
    error.value = 'No fields to export. Make sure the sessions table has columns or fields are correctly configured.';
    return;
  }

  const data = {
    start: props.start,
    applyTo: props.applyTo,
    filename: filename.value,
    segments: segments.value,
    sessions: props.sessions,
    numVisible: props.numVisible,
    numMatching: props.numMatching,
    fields: exportFields.value
  };

  try {
    const response = await SessionsService.exportCsv(data, route.query);
    emit('done', response.text, true, true); // Emit the done event with the response text
  } catch (err) {
    error.value = err.text || 'An unexpected error occurred during CSV export.';
  }
};
</script>
