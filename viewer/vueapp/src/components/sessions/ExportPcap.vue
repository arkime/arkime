<!--
Copyright Yahoo Inc.
SPDX-License-Identifier: Apache-2.0
-->
<template>
  <div
    class="d-flex flex-nowrap gap-1 align-start text-start"
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
        <v-icon icon="mdi-alert" />&nbsp;
        {{ error }}
        <v-btn
          v-if="needsPcapng"
          size="x-small"
          variant="tonal"
          class="ms-2"
          @click="exportAsPcapng">
          {{ $t('sessions.exports.exportPCAPNG') }}
        </v-btn>
      </p>
    </div>

    <div class="d-flex gap-1">
      <v-btn
        size="large"
        variant="flat"
        :style="tertiaryBtnStyle"
        @click="exportPcapAction">
        <v-icon
          icon="mdi-send-outline"
          class="me-1" />
        {{ format === 'pcapng' ? $t('sessions.exports.exportPCAPNG') : $t('sessions.exports.exportPCAP') }}
      </v-btn>
      <v-btn
        size="large"
        id="cancelExportPcap"
        color="warning"
        variant="flat"
        :aria-label="$t('common.cancel')"
        @click="$emit('done', null, false, false)">
        <v-icon icon="mdi-cancel" />
        <v-tooltip activator="parent">
          {{ $t('common.cancel') }}
        </v-tooltip>
      </v-btn>
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
  },
  format: {
    type: String,
    default: 'pcap' // pcap | pcapng, chosen from the export menu
  }
});

// Define Emits
const emit = defineEmits(['done']);

// Reactive state
const error = ref('');
const needsPcapng = ref(false);
const segments = ref('no');
// The chosen format can change locally when a pcap export is bumped to pcapng
const format = ref(props.format === 'pcapng' ? 'pcapng' : 'pcap');
const filename = ref(`sessions.${format.value}`);

// Arkime theme-color v-btn style. Vuetify :color can't take CSS vars.
const tertiaryBtnStyle = {
  backgroundColor: 'rgb(var(--v-theme-tertiary))',
  color: 'rgb(var(--v-theme-button-fg))'
};

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
    format: format.value,
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
    needsPcapng.value = err?.needsPcapng === true;
  }
};

const exportAsPcapng = () => {
  format.value = 'pcapng';
  error.value = '';
  needsPcapng.value = false;
  if (/\.pcapng?$/i.test(filename.value)) {
    filename.value = filename.value.replace(/\.pcapng?$/i, '.pcapng');
  }
  exportPcapAction();
};
</script>
