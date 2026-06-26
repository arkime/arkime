<!--
Copyright Yahoo Inc.
SPDX-License-Identifier: Apache-2.0

Shared chrome for non-field dashboard widgets: title, settings menu
(Edit / optional Export / Remove), and loading / error / empty states.
The widget content is provided via the default slot.
-->
<template>
  <!-- Loading state -->
  <div
    v-if="loading"
    class="widget-card loading-widget"
    aria-hidden="true">
    <h4 class="loading-title">
      {{ title }}
    </h4>
    <div class="bouncing-dots">
      <span class="dot" />
      <span class="dot" />
      <span class="dot" />
    </div>
  </div>

  <!-- Error state -->
  <div
    v-else-if="error"
    class="widget-card widget-error widget-loaded">
    <!-- keep Edit/Remove reachable so a persistently-erroring widget
         (bad field/expression) can still be fixed or removed -->
    <div class="d-flex justify-end align-center mb-2">
      <h4 class="flex-grow-1">
        {{ title }}
      </h4>
      <div class="no-wrap">
        <v-menu>
          <template #activator="{ props: activatorProps }">
            <v-btn
              v-bind="activatorProps"
              variant="outlined"
              size="large"
              icon
              title="Settings">
              <v-icon icon="mdi-cog" />
            </v-btn>
          </template>
          <v-list density="compact">
            <v-list-item @click="$emit('edit')">
              <span><v-icon icon="mdi-pencil" /> {{ $t('sessions.summary.editWidget') }}</span>
            </v-list-item>
            <v-divider />
            <v-list-item @click="$emit('remove')">
              <v-icon
                icon="mdi-close"
                class="text-danger" /> {{ $t('sessions.summary.removeWidget') }}
            </v-list-item>
          </v-list>
        </v-menu>
      </div>
    </div>
    <div class="error-content">
      <v-icon
        icon="mdi-alert-circle"
        size="large"
        class="text-danger mb-2" />
      <p class="text-danger mb-0">
        {{ error }}
      </p>
      <v-btn
        color="success"
        variant="flat"
        size="x-small"
        density="comfortable"
        class="mt-2"
        @click="$emit('retry')">
        <v-icon
          icon="mdi-refresh"
          class="me-1" />
        {{ $t('sessions.summary.retryField') }}
      </v-btn>
    </div>
  </div>

  <!-- Normal content -->
  <div
    v-else
    class="widget-card widget-loaded">
    <div class="d-flex justify-end align-center mb-2">
      <h4 class="flex-grow-1">
        {{ title }}
      </h4>
      <div class="no-wrap">
        <v-menu>
          <template #activator="{ props: activatorProps }">
            <v-btn
              v-bind="activatorProps"
              variant="outlined"
              size="large"
              icon
              title="Settings">
              <v-icon icon="mdi-cog" />
            </v-btn>
          </template>
          <v-list density="compact">
            <v-list-item @click="$emit('edit')">
              <span><v-icon icon="mdi-pencil" /> {{ $t('sessions.summary.editWidget') }}</span>
            </v-list-item>
            <template v-if="showExport">
              <v-divider />
              <v-list-item @click="$emit('export')">
                <v-icon icon="mdi-download" /> {{ exportLabel || $t('sessions.summary.downloadPNG') }}
              </v-list-item>
            </template>
            <v-divider />
            <v-list-item @click="$emit('remove')">
              <v-icon
                icon="mdi-close"
                class="text-danger" /> {{ $t('sessions.summary.removeWidget') }}
            </v-list-item>
          </v-list>
        </v-menu>
      </div>
    </div>

    <div
      v-if="hasData"
      class="widget-body"
      :class="{ 'widget-body--scroll': scroll }">
      <slot />
    </div>
    <div
      v-else
      class="empty-state">
      <v-icon
        icon="mdi-folder-open"
        size="x-large"
        class="mb-3 text-medium-emphasis" />
      <p class="empty-state-text text-medium-emphasis">
        {{ emptyText || $t(noDataMessage) }}
      </p>
    </div>
  </div>
</template>

<script setup>
defineProps({
  title: { type: String, required: true },
  loading: { type: Boolean, default: false },
  error: { type: String, default: '' },
  hasData: { type: Boolean, default: true },
  showExport: { type: Boolean, default: false },
  exportLabel: { type: String, default: '' },
  noDataMessage: { type: String, default: 'sessions.summary.noDataAvailable' },
  emptyText: { type: String, default: '' },
  // allow the body to scroll vertically for dense content (e.g. heatmap rows)
  scroll: { type: Boolean, default: false }
});

defineEmits(['edit', 'remove', 'export', 'retry']);
</script>

<style scoped>
.widget-card {
  background: rgb(var(--v-theme-quaternary-lightest));
  padding: 1rem;
  border-radius: 8px;
  box-shadow: 0 2px 4px rgba(0, 0, 0, 0.1);
  display: flex;
  flex-direction: column;
  height: 100%;
  min-height: 450px;
}

.widget-body {
  flex: 1;
  display: flex;
  flex-direction: column;
  min-height: 0;     /* charts fit their container */
  overflow: hidden;
}

/* dense content (e.g. heatmap with many rows) scrolls vertically */
.widget-body--scroll {
  overflow-y: auto;
  overflow-x: hidden;
}

.loading-widget {
  align-items: center;
  justify-content: center;
  opacity: 0.5;
  filter: saturate(0.3);
}

.widget-loaded {
  animation: fadeIn 0.4s ease-out;
}

@keyframes fadeIn {
  from { opacity: 0; transform: translateY(8px); }
  to { opacity: 1; transform: translateY(0); }
}

.loading-title {
  margin-bottom: 1.5rem;
  font-size: 1.1rem;
}

.bouncing-dots {
  display: flex;
  gap: 0.5rem;
  align-items: center;
}

.dot {
  width: 14px;
  height: 14px;
  border-radius: 50%;
  background: rgb(var(--v-theme-tertiary));
  animation: bounce 1.2s ease-in-out infinite;
}
.dot:nth-child(2) { animation-delay: 0.2s; }
.dot:nth-child(3) { animation-delay: 0.4s; }

@keyframes bounce {
  0%, 50%, 100% { transform: translateY(0) scale(1); }
  8% { transform: translateY(-20px) scale(1.15, 0.85); }
  16% { transform: translateY(0) scale(0.85, 1.15); }
  20% { transform: translateY(0) scale(1); }
}

.error-content,
.empty-state {
  flex: 1;
  display: flex;
  flex-direction: column;
  align-items: center;
  justify-content: center;
  text-align: center;
  padding: 2rem;
}

.empty-state-text {
  font-size: 1.1rem;
  margin: 0;
}
</style>
