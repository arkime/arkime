<!--
Copyright Yahoo Inc.
SPDX-License-Identifier: Apache-2.0
-->
<template>
  <div
    class="d-flex flex-nowrap gap-1 align-items-start text-start"
    @keyup.stop.prevent.enter="applyAction(props.add)">
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
        v-if="props.add"
        type="button"
        @click="applyAction(true)"
        :class="{'disabled':loading}"
        class="btn btn-sm btn-theme-tertiary me-1">
        <span v-if="!loading">
          <span class="fa fa-plus-circle" />&nbsp;
          {{ $t('sessions.tag.addTags') }}
        </span>
        <span v-else>
          <span class="fa fa-spinner fa-spin" />&nbsp;
          {{ $t('sessions.tag.addingTags') }}
        </span>
      </button>
      <button
        v-else
        type="button"
        @click="applyAction(false)"
        :class="{'disabled':loading}"
        class="btn btn-sm btn-danger me-1">
        <span v-if="!loading">
          <span class="fa fa-trash-o" />&nbsp;
          {{ $t('sessions.tag.removeTags') }}
        </span>
        <span v-else>
          <span class="fa fa-spinner fa-spin" />&nbsp;
          {{ $t('sessions.tag.removingTags') }}
        </span>
      </button>
      <button
        id="cancelTagSessionsBtn"
        type="button"
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
  add: Boolean,
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
const loading = ref(false);
const segments = ref('no');
const tags = ref('');

// Access route
const route = useRoute();

// Methods
const applyAction = async (addTagsOperation) => {
  if (!tags.value) {
    error.value = t('sessions.tag.noTagsErr');
    return;
  }

  error.value = ''; // Clear previous error
  loading.value = true;

  const data = {
    tags: tags.value,
    start: props.start,
    applyTo: props.applyTo,
    segments: segments.value,
    sessions: props.sessions,
    numVisible: props.numVisible,
    numMatching: props.numMatching
  };

  try {
    // The first argument to SessionsService.tag determines if it's add or remove
    const response = await SessionsService.tag(addTagsOperation, data, route.query);
    tags.value = ''; // Clear tags input on success
    loading.value = false;
    emit('done', resolveMessage(response, t), true, true); // Emit the done event with the response text
  } catch (err) {
    // display the error under the form so that user
    // has an opportunity to try again (don't close the form)
    error.value = resolveMessage(err, t) || t('sessions.tag.unknownErr');
    loading.value = false;
  }
};
</script>
