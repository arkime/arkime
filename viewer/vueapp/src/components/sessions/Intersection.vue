<!--
Copyright Yahoo Inc.
SPDX-License-Identifier: Apache-2.0
-->
<template>
  <div>
    <div
      class="d-flex flex-nowrap gap-2 align-items-start text-start"
      @keyup.stop.prevent.enter="openIntersectionAction">
      <div class="flex-fill d-flex align-items-center gap-2">
        <v-checkbox
          v-model="counts"
          density="compact"
          hide-details
          inline
          :label="$t('sessions.intersection.includeCounts')" />

        <v-radio-group
          v-model="sort"
          density="compact"
          hide-details
          inline
          class="ms-2">
          <v-radio
            value="count"
            :label="$t('sessions.intersection.countSort')" />
          <v-radio
            value="field"
            :label="$t('sessions.intersection.fieldSort')" />
        </v-radio-group>
      </div>

      <div>
        <button
          class="btn btn-sm btn-theme-tertiary me-1"
          @click="openIntersectionAction"
          type="button">
          <span class="fa fa-venn">
            <span class="fa fa-circle-o" />
            <span class="fa fa-circle-o" />
          </span>&nbsp;
          {{ $t('sessions.intersection.title') }}
        </button>
        <button
          id="cancelExportIntersection"
          class="btn btn-sm btn-warning"
          :aria-label="$t('common.cancel')"
          @click="$emit('done', null, false, false)"
          type="button">
          <span class="fa fa-ban" />
          <v-tooltip activator="parent">
            {{ $t('common.cancel') }}
          </v-tooltip>
        </button>
      </div>
    </div>

    <div class="mt-1">
      <v-text-field
        autofocus
        density="compact"
        variant="outlined"
        hide-details
        label="Fields"
        :model-value="intersectionFields"
        :placeholder="$t('sessions.intersection.exportFieldsTip')"
        @update:model-value="intersectionFields = $event">
        <template #append-inner>
          <span
            id="intersectionFieldsHelp"
            class="cursor-help">
            <span class="fa fa-question-circle" />
            <v-tooltip activator="parent">
              {{ $t('sessions.intersection.exportFieldsHelp') }}
            </v-tooltip>
          </span>
        </template>
      </v-text-field>
      <p
        v-if="error"
        class="small text-danger mb-0">
        <span class="fa fa-exclamation-triangle" />&nbsp;
        {{ error }}
      </p>
    </div>
  </div>
</template>

<script setup>
import { ref, onMounted, watch } from 'vue';
import { useRoute } from 'vue-router';
import SessionsService from './SessionsService';
import { useI18n } from 'vue-i18n';
const { t } = useI18n();

// Define Props
const props = defineProps({
  fields: {
    type: Array,
    default: () => []
  }
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
    error.value = t('sessions.intersection.noFieldsError');
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
