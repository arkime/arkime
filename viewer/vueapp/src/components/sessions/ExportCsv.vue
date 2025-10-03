<!--
Copyright Yahoo Inc.
SPDX-License-Identifier: Apache-2.0
-->
<template>
  <BRow gutter-x="1" class="text-start flex-nowrap d-flex justify-content-between" align-h="start" @keyup.stop.prevent.enter="exportCsvAction">

    <BCol cols="auto">
      <SegmentSelect v-model:segments="segments" />
    </BCol>

    <BCol cols="auto" class="flex-fill">
      <div class="input-group input-group-sm">
        <span class="input-group-text">
          Filename
        </span>
        <b-form-input
          autofocus
          type="text"
          :model-value="filename"
          class="form-control"
          :placeholder="$t('sessions.exports.filenamePlaceholder')"
          @update:model-value="filename = $event"
        />
      </div>
      <p v-if="error"
        class="small text-danger mb-0">
        <span class="fa fa-exclamation-triangle">
        </span>&nbsp;
        {{ error }}
      </p>
    </BCol>

    <BCol cols="auto">
      <button type="button"
        @click="toggleChangeFields"
        class="btn btn-sm btn-theme-secondary me-1">
        {{ $t('sessions.exports.changeFields') }}
      </button>
      <button
        type="button"
        @click="exportCsvAction"
        class="btn btn-sm btn-theme-tertiary me-1">
        <span class="fa fa-paper-plane-o"></span>&nbsp;
        {{ $t('sessions.exports.exportCSV') }}
      </button>
      <button id="cancelExportCsv"
        class="btn btn-sm btn-warning"
        @click="$emit('done', null, false, false)"
        type="button">
        <span class="fa fa-ban"></span>
        <BTooltip target="cancelExportCsv">{{ $t('common.cancel') }}</BTooltip>
      </button>
    </BCol>

  </BRow>

  <div v-if="changeFields"
    class="row mt-1">
    <div class="col">
      <div class="input-group input-group-sm">
        <div id="exportFields"
          class="input-group-text cursor-help">
          {{ $t('sessions.exports.exportFields') }}
          <BTooltip target="exportFields">{{ $t('sessions.exports.exportFieldsTip') }}</BTooltip>
        </div>
        <input type="text"
          class="form-control"
          :model-value="exportFields"
          @update:model-value="exportFields = $event"
          :placeholder="$t('sessions.exports.exportFieldsTip')"
        />
        <div id="exportFieldsHelp"
          class="input-group-text cursor-help">
          <span class="fa fa-question-circle"></span>
          <BTooltip target="exportFieldsHelp">{{ $t('sessions.exports.exportFieldsHelp') }}</BTooltip>
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
    error.value = this.$t('sessions.exports.missingFilenameErr');
    return;
  }

  if (!exportFields.value) {
    error.value = this.$t('sessions.exports.missingFieldsErr');
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
    error.value = err.text || this.$t('sessions.exports.unknownErr');
  }
};
</script>
