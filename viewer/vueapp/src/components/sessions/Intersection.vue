<!--
Copyright Yahoo Inc.
SPDX-License-Identifier: Apache-2.0
-->
<template>
  <div>
    <div class="row"
      @keyup.stop.prevent.enter="openIntersectionAction">
      <div class="col">

        <div class="pull-left me-2">
          <div class="form-check form-check-inline">
            <input type="checkbox"
              class="form-check-input"
              :checked="counts"
              :model-value="counts"
              @click="counts = !counts"
              id="counts"
            />
            <label class="form-check-label"
              for="counts">
              {{ $t('sessions.intersection.includeCounts') }}
            </label>
          </div>

          <div class="form-check form-check-inline ms-2">
            <input class="form-check-input"
              type="radio"
              name="sort"
              id="countSort"
              value="count"
              :checked="sort === 'count'"
              @click="sort = 'count'"
            />
            <label class="form-check-label"
              for="countSort">
              {{ $t('sessions.intersection.countSort') }}
            </label>
          </div>
          <div class="form-check form-check-inline">
            <input class="form-check-input"
              type="radio"
              name="sort"
              id="fieldSort"
              value="field"
              :checked="sort === 'field'"
              @click="sort = 'field'"
            />
            <label class="form-check-label"
              for="fieldSort">
              {{ $t('sessions.intersection.fieldSort') }}
            </label>
          </div>
        </div>

        <div class="pull-right ms-2">
          <button
            id="cancelExportIntersection"
            class="btn btn-sm btn-warning pull-right"
            @click="$emit('done', null, false, false)"
            type="button">
            <span class="fa fa-ban"></span>
            <BTooltip target="cancelExportIntersection">{{ $t('common.cancel') }}</BTooltip>
          </button>
          <button class="btn btn-sm btn-theme-tertiary pull-right me-1"
            :title="$t('sessions.intersection.title')"
            @click="openIntersectionAction"
            type="button">
            <span class="fa fa-venn">
              <span class="fa fa-circle-o">
              </span>
              <span class="fa fa-circle-o">
              </span>
            </span>&nbsp;
            {{ $t('sessions.intersection.title') }}
          </button>
        </div>
      </div>
    </div>

    <div class="row mt-1">
      <div class="col">
        <div class="input-group input-group-sm fields-input">
          <div id="intersectionFields"
            class="input-group-text cursor-help">
            Fields
            <BTooltip target="intersectionFields">{{ $t('sessions.intersection.exportFieldsTip') }}</BTooltip>
          </div>
          <b-form-input
            autofocus
            type="text"
            class="form-control"
            :model-value="intersectionFields"
            @update:model-value="intersectionFields = $event"
            :placeholder="$t('sessions.intersection.exportFieldsTip')"
          />
          <div id="intersectionFieldsHelp"
            class="input-group-text cursor-help">
            <span class="fa fa-question-circle">
            </span>
            <BTooltip target="intersectionFieldsHelp">{{ $t('sessions.intersection.exportFieldsHelp') }}</BTooltip>
          </div>
        </div>
        <p v-if="error"
          class="small text-danger mb-0">
          <span class="fa fa-exclamation-triangle">
          </span>&nbsp;
          {{ error }}
        </p>
      </div>
    </div>

  </div> </template>

<script setup>
import { ref, onMounted, watch } from 'vue';
import { useRoute } from 'vue-router';
import SessionsService from './SessionsService';

// Define Props
const props = defineProps({
  fields: Array
});

// Define emits
const emit = defineEmits(['done']);

// Reactive state
const error = ref('');
const counts = ref(true);
const sort = ref('count');
const intersectionFields = ref(''); // Initialize as empty string, will be computed

// Access route
const route = useRoute();

// Helper function to compute intersection fields string
const computeIntersectionFields = () => {
  const fieldExpList = [];
  if (props.fields) {
    for (const field of props.fields) {
      if (field.exp === 'info' || field.type === 'seconds') {
        continue;
      } else if (field.children) {
        for (const child of field.children) {
          if (child && child.exp) { // Ensure child and child.exp exist
            fieldExpList.push(child.exp);
          }
        }
      } else if (field.exp) { // Ensure field.exp exists
        fieldExpList.push(field.exp);
      }
    }
  }
  intersectionFields.value = fieldExpList.join(',');
};

// Lifecycle hooks
onMounted(() => {
  computeIntersectionFields();
});

// Watch for changes in props.fields if they can change after mount
watch(() => props.fields, () => {
  computeIntersectionFields();
}, { deep: true });

// Methods
const openIntersectionAction = () => {
  if (!intersectionFields.value) {
    error.value = this.$t('sessions.intersection.noFieldsError');
    return;
  }

  const data = {
    exp: intersectionFields.value,
    counts: counts.value ? 1 : 0,
    sort: sort.value
  };

  // Assuming SessionsService.viewIntersection doesn't return a promise that needs handling here
  // or if it does, you might want to await it and handle potential errors.
  // For now, following the original logic.
  SessionsService.viewIntersection(data, route.query);
  emit('done', 'Intersection opened', true, true); // Emit the done event with a message
};

</script>

<style scoped>
.fields-input > input {
  width: auto;
}
</style>
