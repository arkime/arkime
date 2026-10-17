<!--
Copyright Yahoo Inc.
SPDX-License-Identifier: Apache-2.0

Session-wide widget: the capture statistics (counts) for the current search,
fed by the host's global stats chunk (no self-fetch).
-->
<template>
  <WidgetCard
    :title="displayTitle"
    :has-data="hasData"
    :info-items="infoItems"
    @edit="$emit('edit')"
    @remove="$emit('remove')">
    <div class="stats-grid">
      <div class="stat-card">
        <div class="stat-label">
          {{ $t('sessions.summary.sessions') }}
        </div>
        <div class="stat-value">
          {{ formatNumber(stats.sessions) }}
        </div>
      </div>
      <div class="stat-card">
        <div class="stat-label">
          {{ $t('sessions.summary.totalPackets') }}
        </div>
        <div class="stat-value">
          {{ formatNumber(stats.packets) }}
        </div>
      </div>
      <div class="stat-card">
        <div class="stat-label">
          {{ $t('sessions.summary.dataBytes') }}
        </div>
        <div class="stat-value">
          {{ formatBytes(stats.dataBytes) }}
        </div>
      </div>
      <div class="stat-card">
        <div class="stat-label">
          {{ $t('sessions.summary.totalBytes') }}
        </div>
        <div class="stat-value">
          {{ formatBytes(stats.bytes) }}
        </div>
      </div>
      <div class="stat-card">
        <div class="stat-label">
          {{ $t('sessions.summary.downloadBytes') }}
        </div>
        <div class="stat-value">
          {{ formatBytes(stats.downloadBytes) }}
        </div>
      </div>
    </div>
  </WidgetCard>
</template>

<script setup>
import { computed } from 'vue';
import { useI18n } from 'vue-i18n';
import WidgetCard from './WidgetCard.vue';
import { commaString, humanReadableBytes } from '@common/vueFilters.js';

const { t } = useI18n();

const props = defineProps({
  stats: { type: Object, default: () => null },
  title: { type: String, default: '' },
  infoItems: { type: Array, default: () => [] }
});

defineEmits(['edit', 'remove']);

const hasData = computed(() => !!props.stats);
const displayTitle = computed(() => props.title || t('sessions.summary.captureStatistics'));

const formatNumber = (num) => commaString(num || 0);
const formatBytes = (bytes) => humanReadableBytes(bytes || 0);
</script>

<style scoped>
.stats-grid {
  display: grid;
  grid-template-columns: repeat(auto-fit, minmax(88px, 1fr));
  gap: 0.3rem;
  align-content: center;
  flex: 1;
}
.stat-card {
  padding: 0.25rem 0.4rem;
  border-radius: 6px;
  background: rgb(var(--v-theme-quaternary-lightest));
  text-align: center;
}
.stat-label {
  font-size: 0.7rem;
  opacity: 0.8;
}
.stat-value {
  font-size: 0.95rem;
  font-weight: 600;
}
</style>
