<template>
  <div class="row" @keyup.stop.prevent.enter="exportPcapAction">

    <SegmentSelect v-model:segments="segments" />

    <div class="col-md-5">
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
      </div>
      <p v-if="error"
        class="small text-danger mb-0">
        <span class="fa fa-exclamation-triangle">
        </span>&nbsp;
        {{ error }}
      </p>
    </div>

    <div class="col-md-3">
      <div class="pull-right">
        <button class="btn btn-sm btn-theme-tertiary me-1"
          title="Export PCAP"
          @click="exportPcapAction"
          type="button">
          <span class="fa fa-paper-plane-o"></span>&nbsp;
          Export PCAP
        </button>
        <button id="cancelExportPcap"
          class="btn btn-sm btn-warning"
          @click="$emit('done', false)"
          type="button">
          <span class="fa fa-ban"></span>
          <BTooltip target="cancelExportPcap">Cancel</BTooltip>
        </button>
      </div>
    </div>

  </div>
</template>

<script setup>
import { ref } from 'vue';
import { useRoute } from 'vue-router';
import SessionsService from './SessionsService';
import SegmentSelect from './SegmentSelect.vue';

// Define Props
const props = defineProps({
  start: Number,
  applyTo: String,
  sessions: Array,
  numVisible: Number,
  numMatching: Number
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
    error.value = 'No filename specified.';
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
    emit('done', response.text, true); // Emit the done event with the response text
  } catch (err) {
    error.value = err.text || 'An unexpected error occurred.'; // Ensure err.text exists
  }
};
</script>
