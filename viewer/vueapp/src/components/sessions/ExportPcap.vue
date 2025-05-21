<template>
  <div class="row"
    @keyup.stop.prevent.enter="exportPcapAction">

    <SegmentSelect v-model:segments="segments" />

    <div class="col-md-5">

      <div class="input-group input-group-sm">
        <span class="input-group-text">
          Filename
        </span>
        <b-form-input
          autofocus
          type="text"
          v-model="filename"
          class="form-control"
          placeholder="Enter a filename"
        />
      </div> <p v-if="error"
        class="small text-danger mb-0">
        <span class="fa fa-exclamation-triangle">
        </span>&nbsp;
        {{ error }}
      </p> </div>

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
          @click="handleDone(null)"
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
  done: Function, // This is an event callback passed as a prop
  applyTo: String,
  sessions: Array,
  numVisible: Number,
  numMatching: Number
});

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
    console.log('HELP!', route.query); // TODO ECR REMOVE
    const response = await SessionsService.exportPcap(data, route.query);
    // TODO VUE3 Assuming `props.done` is intended to be called like an event handler
    if (props.done && typeof props.done === 'function') {
      props.done(response.text, true);
    }
  } catch (err) {
    error.value = err.text || 'An unexpected error occurred.'; // Ensure err.text exists
  }
};

const handleDone = (value) => {
  if (props.done && typeof props.done === 'function') {
    props.done(value);
  }
};
</script>
