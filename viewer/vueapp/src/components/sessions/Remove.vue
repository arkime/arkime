<!--
Copyright Yahoo Inc.
SPDX-License-Identifier: Apache-2.0
-->
<template>
  <BRow gutter-x="1" class="text-start flex-nowrap d-flex justify-content-between" align-h="start">

    <BCol cols="auto">
      <BFormCheckbox
        inline
        :model-value="pcap"
        id="pcapCheckbox"
        name="pcap"
        @update:model-value="pcap = $event">
        {{ $t('sessions.remove.scrubPCAP') }}
        <BTooltip target="pcapCheckbox">{{ $t('sessions.remove.scrubPCAPTip') }}</BTooltip>
      </BFormCheckbox>
      <BFormCheckbox
        inline
        :model-value="spi"
        id="spiCheckbox"
        name="spi"
        @update:model-value="spi = $event">
        {{ $t('sessions.remove.deleteSPIData') }}
        <BTooltip target="spiCheckbox">{{ $t('sessions.remove.deleteSPIDataTip') }}</BTooltip>
      </BFormCheckbox>
    </BCol>

    <BCol cols="auto">
      <SegmentSelect v-model:segments="segments" />
      <p v-if="error"
        class="small text-danger mb-0">
        <span class="fa fa-exclamation-triangle">
        </span>&nbsp;
        {{ error }}
      </p>
    </BCol>

    <BCol cols="auto">
      <button
        type="button"
        :title="$t('common.remove')"
        @click="deleteSessionsAction"
        :class="{'disabled':loading}"
        class="btn btn-danger btn-sm me-1">
        <span v-if="!loading">
          <span class="fa fa-trash-o"></span>&nbsp;
          {{ $t('common.remove') }}
        </span>
        <span v-else>
          <span class="fa fa-spinner fa-spin"></span>&nbsp;
          {{ $t('common.removing') }}
        </span>
      </button>
      <button class="btn btn-sm btn-warning"
        id="cancelRemoveDataBtn"
        @click="emit('done', null, false, false)"
        type="button">
        <span class="fa fa-ban"></span>
        <BTooltip target="cancelRemoveDataBtn">{{ $t('common.cancel') }}</BTooltip>
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
  start: Number,
  single: Boolean,
  applyTo: String,
  sessions: Array,
  numVisible: Number,
  numMatching: Number
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
    emit('done', response.text, true, true); // Emit the done event with the response text
  } catch (err) {
    // Display the error under the form so that user
    // has an opportunity to try again (don't close the form)
    error.value = err.text || err.message || this.$t('sessions.remove.unknownError');
    loading.value = false;
  }
};

</script>
