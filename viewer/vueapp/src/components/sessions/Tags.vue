<!--
Copyright Yahoo Inc.
SPDX-License-Identifier: Apache-2.0
-->
<template>
  <div class="row"
    @keyup.stop.prevent.enter="applyAction(props.add)">

    <SegmentSelect v-model:segments="segments" />

    <div class="col-md-5">
      <div class="input-group input-group-sm">
        <span class="input-group-text">
          Tags
        </span>
        <input
          autofocus
          type="text"
          v-model="tags"
          class="form-control"
          placeholder="Enter a comma separated list of tags"
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
          v-if="props.add"
          type="button"
          title="Add Tags"
          @click="applyAction(true)"
          :class="{'disabled':loading}"
          class="btn btn-sm btn-theme-tertiary me-1">
          <span v-if="!loading">
            <span class="fa fa-plus-circle me-2"></span>
            Add Tags
          </span>
          <span v-else>
            <span class="fa fa-spinner fa-spin me-2"></span>
            Adding Tags
          </span>
        </button>
        <button
          v-else
          type="button"
          title="Remove Tags"
          @click="applyAction(false)"
          :class="{'disabled':loading}"
          class="btn btn-sm btn-danger me-1">
          <span v-if="!loading">
            <span class="fa fa-trash-o me-2"></span>
            Remove Tags
          </span>
          <span v-else>
            <span class="fa fa-spinner fa-spin me-2"></span>
            Removing Tags
          </span>
        </button>
        <button
          id="cancelTagSessionsBtn"
          type="button"
          @click="$emit('done', null)"
          class="btn btn-sm btn-warning">
          <span class="fa fa-ban"></span>
          <BTooltip target="cancelTagSessionsBtn">Cancel</BTooltip>
        </button>
      </div>
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
    error.value = 'No tag(s) specified.';
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
    emit('done', response.text, true); // Emit the done event with the response text
  } catch (err) {
    // display the error under the form so that user
    // has an opportunity to try again (don't close the form)
    error.value = err.text || err.message || 'An error occurred while tagging sessions.';
    loading.value = false;
  }
};
</script>
