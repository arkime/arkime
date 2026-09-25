<!--
Copyright Yahoo Inc.
SPDX-License-Identifier: Apache-2.0
-->
<template>
  <div
    class="d-flex flex-nowrap gap-1 align-start text-start"
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
        <v-icon icon="mdi-alert" />&nbsp;
        {{ error }}
      </p>
    </div>

    <div class="d-flex gap-1">
      <v-btn
        size="large"
        v-if="props.add"
        variant="flat"
        :style="tertiaryBtnStyle"
        :disabled="loading"
        @click="applyAction(true)">
        <span v-if="!loading">
          <v-icon
            icon="mdi-plus-circle"
            class="me-1" />
          {{ $t('sessions.tag.addTags') }}
        </span>
        <span v-else>
          <v-icon
            icon="mdi-loading"
            class="mdi-spin me-1" />
          {{ $t('sessions.tag.addingTags') }}
        </span>
      </v-btn>
      <v-btn
        size="large"
        v-else
        color="error"
        variant="flat"
        :disabled="loading"
        @click="applyAction(false)">
        <span v-if="!loading">
          <v-icon
            icon="mdi-trash-can-outline"
            class="me-1" />
          {{ $t('sessions.tag.removeTags') }}
        </span>
        <span v-else>
          <v-icon
            icon="mdi-loading"
            class="mdi-spin me-1" />
          {{ $t('sessions.tag.removingTags') }}
        </span>
      </v-btn>
      <v-btn
        size="large"
        id="cancelTagSessionsBtn"
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

// Arkime theme-color v-btn style. Vuetify :color can't take CSS vars.
const tertiaryBtnStyle = {
  backgroundColor: 'rgb(var(--v-theme-tertiary))',
  color: 'rgb(var(--v-theme-button-fg))'
};

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
