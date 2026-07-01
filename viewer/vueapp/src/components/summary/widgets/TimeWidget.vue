<!--
Copyright Yahoo Inc.
SPDX-License-Identifier: Apache-2.0

Session-wide widget: time information (duration, first/last packet, current time)
for the current search, fed by the host's global stats chunk (no self-fetch).
-->
<template>
  <WidgetCard
    :title="displayTitle"
    :has-data="hasData"
    :info-items="infoItems"
    @edit="$emit('edit')"
    @remove="$emit('remove')">
    <div class="time-grid">
      <div class="time-item">
        <span class="time-label">{{ $t('sessions.summary.duration') }}:</span>
        <span class="time-value">{{ formatDuration(stats.firstPacket, stats.lastPacket) }}</span>
      </div>
      <div class="time-item">
        <span class="time-label">{{ $t('sessions.summary.firstPacket') }}:</span>
        <span class="time-value">{{ formatTimestamp(stats.firstPacket) }}</span>
      </div>
      <div class="time-item">
        <span class="time-label">{{ $t('sessions.summary.lastPacket') }}:</span>
        <span class="time-value">{{ formatTimestamp(stats.lastPacket) }}</span>
      </div>
      <div class="time-item">
        <span class="time-label">{{ $t('sessions.summary.currentTime') }}:</span>
        <span class="time-value">{{ getCurrentTime() }}</span>
      </div>
    </div>
  </WidgetCard>
</template>

<script setup>
import { computed } from 'vue';
import { useStore } from 'vuex';
import { useI18n } from 'vue-i18n';
import WidgetCard from './WidgetCard.vue';
import { readableTime, timezoneDateString } from '@common/vueFilters.js';

const { t } = useI18n();
const store = useStore();

const props = defineProps({
  stats: { type: Object, default: () => null },
  title: { type: String, default: '' },
  infoItems: { type: Array, default: () => [] }
});

defineEmits(['edit', 'remove']);

const hasData = computed(() => !!props.stats);
const displayTitle = computed(() => props.title || t('sessions.summary.timeInformation'));

const timezone = computed(() => store.state.user?.settings?.timezone || 'local');
const showMs = computed(() => store.state.user?.settings?.ms === true);

const formatDuration = (start, end) => {
  if (!start || !end) { return 'N/A'; }
  return readableTime(end - start);
};

const formatTimestamp = (timestamp) => {
  if (!timestamp) { return 'N/A'; }
  // Arkime stores timestamps in seconds; convert to ms when needed
  const ms = timestamp < 10000000000 ? timestamp * 1000 : timestamp;
  return timezoneDateString(ms, timezone.value, showMs.value);
};

const getCurrentTime = () => timezoneDateString(Date.now(), timezone.value, showMs.value);
</script>

<style scoped>
.time-grid {
  display: flex;
  flex-direction: column;
  gap: 0.2rem;
  justify-content: center;
  flex: 1;
  font-size: 0.85rem;
}
.time-item {
  display: flex;
  justify-content: space-between;
  gap: 0.75rem;
}
.time-label {
  font-weight: 500;
  opacity: 0.85;
}
.time-value {
  text-align: right;
}
</style>
