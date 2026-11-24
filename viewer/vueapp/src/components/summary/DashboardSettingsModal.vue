<!--
Copyright Yahoo Inc.
SPDX-License-Identifier: Apache-2.0

Dashboard settings: what belongs to the dashboard as a whole rather than to
one widget (today its filter; the home for the next ones). The form is a copy
of the settings while the dialog is open, applied on Apply, so a half-typed
filter never reloads the page.
-->
<template>
  <v-dialog
    :model-value="show"
    max-width="640"
    @update:model-value="(val) => { if (!val) $emit('close'); }">
    <v-card density="compact">
      <v-card-title>
        {{ $t('sessions.summary.dashboardSettings') }}
      </v-card-title>
      <v-card-text>
        <!-- the dashboard's own filter: ANDed with the search for every widget -->
        <div class="arkime-input-group arkime-input-group--fluid mb-1">
          <span class="arkime-input-label">
            {{ $t('sessions.summary.dashboardFilter') }}
          </span>
          <input
            v-model="form.expression"
            type="text"
            class="arkime-input-control"
            spellcheck="false"
            placeholder="protocols == tls"
            @keyup.enter="apply">
        </div>
        <div class="text-caption text-medium-emphasis">
          {{ $t('sessions.summary.dashboardFilterTip') }}
        </div>
      </v-card-text>
      <v-card-actions>
        <div class="w-100 d-flex justify-space-between">
          <v-btn
            color="error"
            variant="flat"
            size="large"
            @click="$emit('close')">
            <v-icon
              icon="mdi-close"
              class="me-1" />
            {{ $t('common.cancel') }}
          </v-btn>
          <v-btn
            color="success"
            variant="flat"
            size="large"
            @click="apply">
            <v-icon
              icon="mdi-check"
              class="me-1" />
            {{ $t('common.apply') }}
          </v-btn>
        </div>
      </v-card-actions>
    </v-card>
  </v-dialog>
</template>

<script setup>
import { ref, watch } from 'vue';

const props = defineProps({
  show: { type: Boolean, default: false },
  // the dashboard's current settings
  expression: { type: String, default: '' }
});

const emit = defineEmits(['close', 'apply']);

const form = ref({ expression: '' });

// start from the current settings each time the dialog opens
watch(() => props.show, (isOpen) => {
  if (isOpen) { form.value = { expression: props.expression || '' }; }
}, { immediate: true });

const apply = () => {
  emit('apply', { expression: (form.value.expression || '').trim() });
  emit('close');
};
</script>
