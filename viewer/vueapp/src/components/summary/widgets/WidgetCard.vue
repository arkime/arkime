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
    <div class="d-flex align-center mb-1">
      <div class="widget-heading d-flex align-baseline">
        <v-icon
          icon="mdi-drag-vertical"
          size="large"
          class="widget-drag"
          :title="$t('sessions.summary.dragToReorder')" />
        <h4 class="widget-title">
          {{ title }}
        </h4>
        <span
          v-if="infoItems.length"
          class="widget-info-icon ps-1"
          tabindex="0">
          <v-icon
            icon="mdi-information-outline"
            class="text-medium-emphasis" />
          <v-tooltip
            activator="parent"
            location="bottom end"
            open-delay="100"
            max-width="340">
            <div
              v-for="row in infoItems"
              :key="row.label"
              class="widget-info-row">
              <span class="widget-info-label">{{ row.label }}:</span>
              <span class="widget-info-value">{{ row.value }}</span>
            </div>
          </v-tooltip>
        </span>
      </div>
      <div class="widget-actions no-wrap d-flex align-center ga-2">
        <v-btn
          variant="outlined"
          size="large"
          icon
          :title="$t('sessions.summary.editWidget')"
          @click="$emit('edit')">
          <v-icon icon="mdi-pencil" />
        </v-btn>
        <v-btn
          variant="outlined"
          size="large"
          icon
          :title="$t('sessions.summary.removeWidget')"
          @click="$emit('remove')">
          <v-icon
            icon="mdi-close"
            class="text-danger" />
        </v-btn>
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
    <div class="d-flex align-center mb-1">
      <div class="widget-heading d-flex align-baseline">
        <v-icon
          icon="mdi-drag-vertical"
          size="large"
          class="widget-drag"
          :title="$t('sessions.summary.dragToReorder')" />
        <h4 class="widget-title">
          {{ title }}
        </h4>
        <span
          v-if="infoItems.length"
          class="widget-info-icon ps-1"
          tabindex="0">
          <v-icon
            icon="mdi-information-outline"
            class="text-medium-emphasis" />
          <v-tooltip
            activator="parent"
            location="bottom end"
            open-delay="100"
            max-width="340">
            <div
              v-for="row in infoItems"
              :key="row.label"
              class="widget-info-row">
              <span class="widget-info-label">{{ row.label }}:</span>
              <span class="widget-info-value">{{ row.value }}</span>
            </div>
          </v-tooltip>
        </span>
      </div>
      <div class="widget-actions no-wrap d-flex align-center ga-2">
        <v-btn
          variant="outlined"
          size="large"
          icon
          :title="$t('sessions.summary.editWidget')"
          @click="$emit('edit')">
          <v-icon icon="mdi-pencil" />
        </v-btn>
        <v-btn
          v-if="showExport"
          variant="outlined"
          size="large"
          icon
          :title="exportLabel || $t('sessions.summary.downloadPNG')"
          @click="$emit('export')">
          <v-icon icon="mdi-download" />
        </v-btn>
        <v-btn
          variant="outlined"
          size="large"
          icon
          :title="$t('sessions.summary.removeWidget')"
          @click="$emit('remove')">
          <v-icon
            icon="mdi-close"
            class="text-danger" />
        </v-btn>
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
  scroll: { type: Boolean, default: false },
  // [{ label, value }] rows shown in the header info tooltip (config not
  // otherwise visible: metric / limit / local filter / view)
  infoItems: { type: Array, default: () => [] }
});

defineEmits(['edit', 'remove', 'export', 'retry']);
</script>

<style scoped>
/* title + info icon share the left; actions sit on the right */
.widget-heading {
  flex: 1 1 auto;
  min-width: 0;
}

/* reorder grip: a dots handle in the header, left of the title. Always present
   (discoverable, doesn't blend into the widget above like a floating tab did);
   brightens on hover. */
.widget-drag {
  cursor: move;
  color: rgb(var(--v-theme-neutral));
  align-self: center;
  flex-shrink: 0;
  /* pull toward the card's left edge and tighten the gap to the title */
  margin-left: -15px;
  margin-right: -2px;
  margin-bottom: 6px;
  opacity: 0.5;
  transition: opacity 0.15s ease, color 0.15s ease;
}
.widget-drag:hover {
  opacity: 1;
  color: rgb(var(--v-theme-foreground-accent));
}

.widget-title {
  min-width: 0;
  overflow: hidden;
  text-overflow: ellipsis;
  white-space: nowrap;
}

/* info icon sits just to the right of the (possibly truncated) title */
.widget-info-icon {
  display: inline-flex;
  align-items: center;
  cursor: help;
  flex-shrink: 0;
}

.widget-info-row {
  display: flex;
  justify-content: space-between;
  gap: 1rem;
}

.widget-info-label {
  font-weight: 600;
  white-space: nowrap;
}

.widget-info-value {
  text-align: right;
  word-break: break-word;
}

.widget-card {
  background: rgb(var(--v-theme-quaternary-lightest));
  padding: 0.5rem 0.6rem;
  border-radius: 8px;
  box-shadow: 0 2px 4px rgba(0, 0, 0, 0.1);
  display: flex;
  flex-direction: column;
  height: 100%;
  min-height: 0; /* height comes from the grid row span (160px units) */
}

.widget-body {
  flex: 1;
  display: flex;
  flex-direction: column;
  min-height: 0;     /* charts fit their container */
  overflow: hidden;
}

/* dense content (heatmap rows, wide multi-metric tables) scrolls both ways;
   auto shows a scrollbar only when the content actually overflows */
.widget-body--scroll {
  overflow: auto;
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
  padding: 1.25rem;
}

.empty-state-text {
  font-size: 1.1rem;
  margin: 0;
}
</style>
