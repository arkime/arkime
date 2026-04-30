<!--
Copyright Yahoo Inc.
SPDX-License-Identifier: Apache-2.0
-->
<template>
  <div>
    <div
      class="d-flex flex-nowrap gap-1 align-items-start text-start"
      @keyup.stop.prevent.enter="exportCsvAction">
      <SegmentSelect v-model:segments="segments" />

      <div class="flex-fill">
        <v-text-field
          autofocus
          density="compact"
          variant="outlined"
          hide-details
          :model-value="filename"
          label="Filename"
          :placeholder="$t('sessions.exports.filenamePlaceholder')"
          @update:model-value="filename = $event" />
        <p
          v-if="error"
          class="small text-danger mb-0">
          <span class="fa fa-exclamation-triangle" />&nbsp;
          {{ error }}
        </p>
      </div>

      <div>
        <button
          type="button"
          @click="toggleChangeFields"
          class="btn btn-sm btn-theme-secondary me-1">
          {{ $t('sessions.exports.changeFields') }}
        </button>
        <button
          type="button"
          @click="exportCsvAction"
          class="btn btn-sm btn-theme-tertiary me-1">
          <span class="fa fa-paper-plane-o" />&nbsp;
          {{ $t('sessions.exports.exportCSV') }}
        </button>
        <button
          id="cancelExportCsv"
          class="btn btn-sm btn-warning"
          :aria-label="$t('common.cancel')"
          @click="$emit('done', null, false, false)"
          type="button">
          <span class="fa fa-ban" />
          <v-tooltip activator="parent">
            {{ $t('common.cancel') }}
          </v-tooltip>
        </button>
      </div>
    </div>

    <div
      v-if="changeFields"
      class="mt-1">
      <v-text-field
        density="compact"
        variant="outlined"
        hide-details
        :label="$t('sessions.exports.exportFields')"
        :model-value="exportFields"
        :placeholder="$t('sessions.exports.exportFieldsTip')"
        @update:model-value="exportFields = $event">
        <template #append-inner>
          <span
            id="exportFieldsHelp"
            class="cursor-help">
            <span class="fa fa-question-circle" />
            <v-tooltip activator="parent">
              {{ $t('sessions.exports.exportFieldsHelp') }}
            </v-tooltip>
          </span>
        </template>
      </v-text-field>
    </div>
  </div>
</template>

<script setup>
import { ref, onMounted, watch } from 'vue';
import { useRoute } from 'vue-router';
import SessionsService from './SessionsService';
import SegmentSelect from './SegmentSelect.vue';
import { useI18n } from 'vue-i18n';
import { resolveMessage } from '@common/resolveI18nMessage';
const { t } = useI18n();

// Define Props
const props = defineProps({
  start: {
    type: Number,
    default: 0
  },
  applyTo: {
    type: String,
    default: 'open'
  },
  sessions: {
    type: Array,
    default: () => []
  },
  numVisible: {
    type: Number,
    default: 0
  },
  numMatching: {
    type: Number,
    default: 0
  },
  fields: {
    type: Array,
    default: () => []
  }
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
    error.value = t('sessions.exports.missingFilenameErr');
    return;
  }

  if (!exportFields.value) {
    error.value = t('sessions.exports.missingFieldsErr');
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
    emit('done', resolveMessage(response, t), true, true); // Emit the done event with the response text
  } catch (err) {
    error.value = resolveMessage(err, t) || t('sessions.exports.unknownErr');
  }
};
</script>
