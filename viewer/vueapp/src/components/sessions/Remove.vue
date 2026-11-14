<!--
Copyright Yahoo Inc.
SPDX-License-Identifier: Apache-2.0
-->
<template>
  <div class="d-flex flex-nowrap gap-2 align-center text-start">
    <div class="d-flex align-center">
      <span
        id="pcapCheckboxWrap"
        class="d-inline-flex align-center">
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
        class="d-inline-flex align-center ms-2">
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
        <v-icon icon="mdi-alert" />&nbsp;
        {{ error }}
      </p>
    </div>

    <div class="ms-auto d-flex gap-1">
      <v-btn
        size="large"
        color="error"
        variant="flat"
        :disabled="loading"
        @click="deleteSessionsAction">
        <span v-if="!loading">
          <v-icon
            icon="mdi-trash-can-outline"
            class="me-1" />
          {{ $t('common.remove') }}
        </span>
        <span v-else>
          <v-icon
            icon="mdi-loading"
            class="mdi-spin me-1" />
          {{ $t('common.removing') }}
        </span>
      </v-btn>
      <v-btn
        size="large"
        id="cancelRemoveDataBtn"
        color="warning"
        variant="flat"
        :aria-label="$t('common.cancel')"
        @click="emit('done', null, false, false)">
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
