<!--
Copyright Yahoo Inc.
SPDX-License-Identifier: Apache-2.0
-->
<template>
  <BRow gutter-x="1" class="text-start flex-nowrap d-flex justify-content-between" align-h="start"
    @keyup.stop.prevent.enter="applyAction(props.add)">

    <BCol cols="auto">
      <SegmentSelect v-model:segments="segments" />
    </BCol>

    <BCol cols="auto" class="flex-fill">
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
        <span class="fa fa-exclamation-triangle">
        </span>&nbsp;
        {{ error }}
      </p>
    </BCol>

    <BCol cols="auto">
      <button
        v-if="props.add"
        type="button"
        :title="$t('sessions.tag.addTags')"
        @click="applyAction(true)"
        :class="{'disabled':loading}"
        class="btn btn-sm btn-theme-tertiary me-1">
        <span v-if="!loading">
          <span class="fa fa-plus-circle"></span>&nbsp;
          {{ $t('sessions.tag.addTags') }}
        </span>
        <span v-else>
          <span class="fa fa-spinner fa-spin"></span>&nbsp;
          {{ $t('sessions.tag.addingTags') }}
        </span>
      </button>
      <button
        v-else
        type="button"
        :title="$t('sessions.tag.removeTags')"
        @click="applyAction(false)"
        :class="{'disabled':loading}"
        class="btn btn-sm btn-danger me-1">
        <span v-if="!loading">
          <span class="fa fa-trash-o"></span>&nbsp;
          {{ $t('sessions.tag.removeTags') }}
        </span>
        <span v-else>
          <span class="fa fa-spinner fa-spin"></span>&nbsp;
          {{ $t('sessions.tag.removingTags') }}
        </span>
      </button>
      <button
        id="cancelTagSessionsBtn"
        type="button"
        @click="$emit('done', null, false, false)"
        class="btn btn-sm btn-warning">
        <span class="fa fa-ban"></span>
        <BTooltip target="cancelTagSessionsBtn">{{ $t('common.cancel') }}</BTooltip>
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
  add: Boolean,
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
const loading = ref(false);
const segments = ref('no');
const tags = ref('');

// Access route
const route = useRoute();

// Methods
const applyAction = async (addTagsOperation) => {
  if (!tags.value) {
    error.value = this.$t('sessions.tag.noTagsErr');
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
    emit('done', response.text, true, true); // Emit the done event with the response text
  } catch (err) {
    // display the error under the form so that user
    // has an opportunity to try again (don't close the form)
    error.value = err.text || err.message || this.$t('sessions.tag.unknownErr');
    loading.value = false;
  }
};
</script>
