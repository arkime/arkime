<template>
  <div
    class="d-flex flex-nowrap gap-1 align-items-start text-start"
    @keyup.stop.prevent.enter="exportPcapAction">
    <SegmentSelect v-model:segments="segments" />

    <div class="flex-fill">
      <v-text-field
        autofocus
        density="compact"
        variant="outlined"
        hide-details
        :model-value="filename"
        :label="$t('sessions.exports.filename')"
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
        class="btn btn-sm btn-theme-tertiary me-1"
        @click="exportPcapAction"
        type="button">
        <span class="fa fa-paper-plane-o" />&nbsp;
        {{ $t('sessions.exports.exportPCAP') }}
      </button>
      <button
        id="cancelExportPcap"
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
</template>

<script setup>
import { ref } from 'vue';
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
    error.value = t('sessions.exports.missingFilenameErr');
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
    emit('done', resolveMessage(response, t), true, true); // Emit the done event with the response text
  } catch (err) {
    error.value = resolveMessage(err, t) || t('sessions.exports.unknownErr');
  }
};
</script>
