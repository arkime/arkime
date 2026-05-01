<!--
Copyright Yahoo Inc.
SPDX-License-Identifier: Apache-2.0
-->
<template>
  <div class="d-flex flex-nowrap gap-2 align-items-start text-start">
    <div>
      <span
        id="pcapCheckboxWrap"
        class="d-inline-block">
        <v-checkbox
          v-model="pcap"
          density="compact"
          hide-details
          inline
          :label="$t('sessions.remove.scrubPCAP')" />
        <v-tooltip activator="#pcapCheckboxWrap">
          {{ $t('sessions.remove.scrubPCAPTip') }}
        </v-tooltip>
      </span>
      <span
        id="spiCheckboxWrap"
        class="d-inline-block ms-2">
        <v-checkbox
          v-model="spi"
          density="compact"
          hide-details
          inline
          :label="$t('sessions.remove.deleteSPIData')" />
        <v-tooltip activator="#spiCheckboxWrap">
          {{ $t('sessions.remove.deleteSPIDataTip') }}
        </v-tooltip>
      </span>
    </div>

    <div>
      <SegmentSelect v-model:segments="segments" />
      <p
        v-if="error"
        class="small text-danger mb-0">
        <span class="fa fa-exclamation-triangle" />&nbsp;
        {{ error }}
      </p>
    </div>

    <div class="ms-auto">
      <button
        type="button"
        @click="deleteSessionsAction"
        :class="{'disabled':loading}"
        class="btn btn-danger btn-sm me-1">
        <span v-if="!loading">
          <span class="fa fa-trash-o" />&nbsp;
          {{ $t('common.remove') }}
        </span>
        <span v-else>
          <span class="fa fa-spinner fa-spin" />&nbsp;
          {{ $t('common.removing') }}
        </span>
      </button>
      <button
        class="btn btn-sm btn-warning"
        id="cancelRemoveDataBtn"
        :aria-label="$t('common.cancel')"
        @click="emit('done', null, false, false)"
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
  single: Boolean,
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
const spi = ref(false);
const pcap = ref(true);
const loading = ref(false);
const segments = ref('no');

// Access route
const route = useRoute();

// Methods
const deleteSessionsAction = async () => {
  error.value = ''; // Clear previous error
  loading.value = true;

  const data = {
    start: props.start,
    removeSpi: spi.value,
    removePcap: pcap.value,
    applyTo: props.applyTo,
    segments: segments.value,
    sessions: props.sessions,
    numVisible: props.numVisible,
    numMatching: props.numMatching
  };

  try {
    const response = await SessionsService.remove(data, route.query);
    loading.value = false;
    emit('done', resolveMessage(response, t), true, true); // Emit the done event with the response text
  } catch (err) {
    // Display the error under the form so that user
    // has an opportunity to try again (don't close the form)
    error.value = resolveMessage(err, t) || t('sessions.remove.unknownError');
    loading.value = false;
  }
};

</script>
