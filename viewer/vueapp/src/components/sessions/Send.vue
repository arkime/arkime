<!--
Copyright Yahoo Inc.
SPDX-License-Identifier: Apache-2.0
-->
<template>
  <div
    class="d-flex flex-nowrap gap-1 align-items-start text-start"
    @keyup.stop.prevent.enter="sendAction">
    <SegmentSelect v-model:segments="segments" />

    <div class="flex-fill">
      <v-text-field
        autofocus
        density="compact"
        variant="outlined"
        hide-details
        v-model="tags"
        :label="$t('sessions.tags')"
        :placeholder="$t('sessions.tagsPlaceholder')" />
      <p
        v-if="error"
        class="small text-danger mb-0">
        <span class="fa fa-exclamation-triangle" />&nbsp;
        {{ error }}
      </p>
    </div>

    <div>
      <button
        type="button"
        @click="sendAction"
        :class="{'disabled':loading}"
        class="btn btn-sm btn-theme-tertiary me-1">
        <span v-if="!loading">
          <span class="fa fa-paper-plane-o" />&nbsp;
          {{ $t('sessions.send.send') }}
        </span>
        <span v-else>
          <span class="fa fa-spinner fa-spin" />&nbsp;
          {{ $t('common.sending') }}
        </span>
      </button>
      <button
        type="button"
        id="cancelSendBtn"
        :aria-label="$t('common.cancel')"
        @click="$emit('done', null, false, false)"
        class="btn btn-sm btn-warning">
        <span class="fa fa-ban" />
        <v-tooltip activator="parent">
          {{ $t('common.cancel') }}
        </v-tooltip>
      </button>
    </div>
  </div>

  <div class="row mt-2">
    <div class="col">
      <p class="text-info small mb-0">
        <em>
          <strong>
            <span class="fa fa-info-circle me-2" />
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
import { useI18n } from 'vue-i18n';
import { resolveMessage } from '@common/resolveI18nMessage';
const { t } = useI18n();

// Define Props
const props = defineProps({
  cluster: {
    type: String,
    default: ''
  },
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

    emit('done', resolveMessage(response, t), true, true); // Emit the done event with the response text
  } catch (err) {
    // Display the error under the form so that user
    // has an opportunity to try again (don't close the form)
    error.value = resolveMessage(err, t) || t('sessions.send.unknownErr');
    loading.value = false;
  }
};
</script>
