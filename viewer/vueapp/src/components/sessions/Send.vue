<!--
Copyright Yahoo Inc.
SPDX-License-Identifier: Apache-2.0
-->
<template>
  <div class="row"
    @keyup.stop.prevent.enter="sendAction">

    <SegmentSelect v-model:segments="segments" />

    <div class="col-md-5">
      <div class="input-group input-group-sm">
        <span class="input-group-text">
          {{ $t('sessions.tags') }}
        </span>
        <input
          autofocus
          type="text"
          v-model="tags"
          class="form-control"
          :placeholder="$t('sessions.tagsPlaceholder')"
        />
      </div>
      <p v-if="error"
        class="small text-danger mb-0">
        <span class="fa fa-exclamation-triangle me-2"></span>
        {{ error }}
      </p>
    </div>

    <div class="col-md-3">
      <div class="pull-right">
        <button
          type="button"
          @click="sendAction"
          :title="$t('sessions.send.send')"
          :class="{'disabled':loading}"
          class="btn btn-sm btn-theme-tertiary me-1">
          <span v-if="!loading">
            <span class="fa fa-paper-plane-o me-2"></span>
            {{ $t('sessions.send.send') }}
          </span>
          <span v-else>
            <span class="fa fa-spinner fa-spin me-2"></span>
            {{ $t('common.sending') }}
          </span>
        </button>
        <button
          type="button"
          id="cancelSendBtn"
          @click="$emit('done', null, false, false)"
          class="btn btn-sm btn-warning">
          <span class="fa fa-ban"></span>
          <BTooltip target="cancelSendBtn">{{ $t('common.cancel') }}</BTooltip>
        </button>
      </div>
    </div>

    <div class="col-md-12 mt-2">
      <p class="text-info small mb-0">
        <em>
          <strong>
            <span class="fa fa-info-circle me-2"></span>
            {{ $t('sessions.send.info') }}
          </strong>
        </em>
      </p>
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
  cluster: String,
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
const loading = ref(false);
const segments = ref('no');
const tags = ref(''); // This is named 'tags' but might be used for other purposes in send context or just for consistency

// Access route
const route = useRoute();

// Methods
const sendAction = async () => {
  error.value = ''; // Clear previous errors
  loading.value = true;

  const data = {
    tags: tags.value, // If tags are not relevant for "send", this might need adjustment based on actual API
    start: props.start,
    cluster: props.cluster,
    applyTo: props.applyTo,
    segments: segments.value,
    sessions: props.sessions,
    numVisible: props.numVisible,
    numMatching: props.numMatching
  };

  try {
    const response = await SessionsService.send(data, route.query);
    tags.value = ''; // Clear input on success
    loading.value = false;

    let reloadData = false;
    // Only reload data if action was on a single session
    if (data.sessions && data.sessions.length === 1) {
      reloadData = true;
    }

    emit('done', response.text, true, reloadData); // Emit the done event with the response text
  } catch (err) {
    // Display the error under the form so that user
    // has an opportunity to try again (don't close the form)
    error.value = err.text || err.message || this.$t('sessions.send.unknownErr');
    loading.value = false;
  }
};
</script>
