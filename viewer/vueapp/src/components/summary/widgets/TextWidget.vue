<!--
Copyright Yahoo Inc.
SPDX-License-Identifier: Apache-2.0

Text widget: static notes/instructions on the dashboard. No fetch, no field;
the text is part of the widget definition and edited in the widget modal.
-->
<template>
  <WidgetCard
    :title="title"
    :has-data="true"
    :info-items="infoItems"
    @edit="$emit('edit')"
    @remove="$emit('remove')">
    <div
      v-if="widget.text"
      class="text-widget">
      {{ widget.text }}
    </div>
    <div
      v-else
      class="text-widget text-widget--empty text-medium-emphasis">
      {{ $t('sessions.summary.widget.textHint') }}
    </div>
  </WidgetCard>
</template>

<script setup>
import { computed } from 'vue';
import { useI18n } from 'vue-i18n';
import WidgetCard from './WidgetCard.vue';

const { t } = useI18n();

const props = defineProps({
  widget: { type: Object, required: true },
  infoItems: { type: Array, default: () => [] }
});

defineEmits(['edit', 'remove']);

const title = computed(() => props.widget.title || t('sessions.summary.textView'));
</script>

<style scoped>
.text-widget {
  flex: 1;
  overflow: auto;
  white-space: pre-wrap;
  word-break: break-word;
  font-size: 0.9rem;
  line-height: 1.4;
  padding: 0.15rem 0.1rem;
}
.text-widget--empty {
  font-style: italic;
}
</style>
