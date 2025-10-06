<template>
  <BRow
    gutter-x="1"
    class="text-start flex-nowrap d-flex justify-content-between"
    align-h="start"
    @keyup.stop.prevent.enter="exportPcapAction"
  >
    <BCol cols="auto">
      <SegmentSelect v-model:segments="segments" />
    </BCol>

    <BCol
      cols="auto"
      class="flex-fill"
    >
      <div class="input-group input-group-sm">
        <span class="input-group-text">
          {{ $t('sessions.exports.filename') }}
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
      <p
        v-if="error"
        class="small text-danger mb-0"
      >
        <span class="fa fa-exclamation-triangle" />&nbsp;
        {{ error }}
      </p>
    </BCol>

    <BCol cols="auto">
      <button
        class="btn btn-sm btn-theme-tertiary me-1"
        @click="exportPcapAction"
        type="button"
      >
        <span class="fa fa-paper-plane-o" />&nbsp;
        {{ $t('sessions.exports.exportPCAP') }}
      </button>
      <button
        id="cancelExportPcap"
        class="btn btn-sm btn-warning"
        @click="$emit('done', null, false, false)"
        type="button"
      >
        <span class="fa fa-ban" />
        <BTooltip target="cancelExportPcap">
          {{ $t('common.cancel') }}
        </BTooltip>
      </button>
    </BCol>
  </BRow>
</template>

<script setup>
import { ref } from 'vue';
import { useRoute } from 'vue-router';
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
const filename = ref('sessions.pcap');

// Access route
const route = useRoute();

// Methods
const exportPcapAction = async () => {
  if (filename.value === '') {
    error.value = this.$t('sessions.exports.missingFilenameErr');
    return;
  }

  const data = {
    start: props.start,
    applyTo: props.applyTo,
    filename: filename.value,
    segments: segments.value,
    sessions: props.sessions,
    numVisible: props.numVisible,
    numMatching: props.numMatching
  };

  try {
    const response = await SessionsService.exportPcap(data, route.query);
    emit('done', response.text, true, true); // Emit the done event with the response text
  } catch (err) {
    error.value = err.text || this.$t('sessions.exports.unknownErr');
  }
};
</script>
