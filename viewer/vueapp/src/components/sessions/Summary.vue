<template>
  <BRow
    gutter-x="1"
    align-h="start"
    class="text-start flex-nowrap d-flex justify-content-between"
    @keyup.stop.prevent.enter="generateSummaryAction">
    <BCol cols="auto">
      <SegmentSelect v-model:segments="segments" />
    </BCol>

    <BCol cols="auto">
      <button
        class="btn btn-sm btn-theme-tertiary me-1"
        @click="generateSummaryAction"
        type="button">
        <span class="fa fa-file-text-o" />&nbsp;
        {{ $t('sessions.summary.generateSummary') }}
      </button>
      <button
        id="cancelGenerateSummary"
        class="btn btn-sm btn-warning"
        @click="$emit('done', null, false, false)"
        type="button">
        <span class="fa fa-ban" />
        <BTooltip target="cancelGenerateSummary">
          {{ $t('common.cancel') }}
        </BTooltip>
      </button>
    </BCol>
  </BRow>
</template>

<script setup>
import { ref } from 'vue';
import { useRoute } from 'vue-router';

import { useI18n } from 'vue-i18n';
const { t } = useI18n();

import SessionsService from './SessionsService';
import SegmentSelect from './SegmentSelect.vue';

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
  }
});

// Define Emits
const emit = defineEmits(['done']);

// Reactive state
const error = ref('');
const segments = ref('no');

// Access route
const route = useRoute();

// Methods
const generateSummaryAction = async () => {
  const data = {
    start: props.start,
    applyTo: props.applyTo,
    segments: segments.value,
    sessions: props.sessions,
    numVisible: props.numVisible,
    numMatching: props.numMatching
  };

  try {
    const response = await SessionsService.openSummary(data, route.query);
    emit('done', response.text, true, true); // Emit the done event with the response text
  } catch (err) {
    emit('done', String(err), false, false); // Emit the done event with the error message
  }
};
</script>
