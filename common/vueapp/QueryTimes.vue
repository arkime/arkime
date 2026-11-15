<!--
Copyright Andy Wick
SPDX-License-Identifier: Apache-2.0
-->
<template>
  <span
    v-if="current"
    class="query-times">
    <v-menu
      location="top"
      :close-on-content-click="false">
      <template #activator="{ props: menuProps }">
        <a
          v-bind="menuProps"
          role="button"
          tabindex="0"
          class="query-times-link"
          :title="tip"
          @keyup.enter="menuProps.onClick">
          <v-icon
            v-if="current.pending"
            icon="mdi-loading"
            size="x-small"
            class="mdi-spin me-1" />
          <template v-if="longest(current) !== undefined">{{ commaString(longest(current)) }}ms</template>
          <template v-if="current.entries.length > 1">
            ({{ $t('queryTimes.count', current.entries.length) }})
          </template>
        </a>
      </template>
      <v-card class="query-times-card">
        <div
          v-for="(batch, index) in batches"
          :key="batch.id"
          class="query-times-batch">
          <div
            class="query-times-summary"
            :class="{ 'cursor-pointer': index > 0 }"
            @click="index > 0 && toggle(batch.id)">
            <v-icon
              v-if="index > 0"
              size="small"
              :icon="opened === batch.id ? 'mdi-chevron-down' : 'mdi-chevron-right'" />
            <strong>{{ timeString(batch.start) }}</strong>
            <span class="ms-2">{{ $t('queryTimes.longest') }}: {{ ms(longest(batch)) }}</span>
            <span class="ms-2">{{ $t('queryTimes.sum') }}: {{ ms(sum(batch)) }}</span>
            <span class="ms-2">{{ $t('queryTimes.elapsed') }}: {{ ms(batch.end - batch.start) }}</span>
            <span class="ms-2">{{ $t('queryTimes.count', batch.entries.length) }}</span>
          </div>
          <table
            v-if="index === 0 || opened === batch.id"
            class="query-times-table">
            <thead>
              <tr>
                <th>{{ $t('queryTimes.query') }}</th>
                <th class="text-end">
                  {{ $t('queryTimes.server') }}
                </th>
                <th class="text-end">
                  {{ $t('queryTimes.client') }}
                </th>
              </tr>
            </thead>
            <tbody>
              <tr
                v-for="entry in sorted(batch)"
                :key="entry.id"
                :class="`query-${entry.status}`">
                <td :title="`${entry.method} ${entry.path}`">
                  {{ entry.label || entry.path }}
                  <small
                    v-if="entry.label"
                    class="text-medium-emphasis">{{ entry.path }}</small>
                  <small v-if="entry.status !== 'ok'">({{ $t(`queryTimes.${entry.status}`) }})</small>
                </td>
                <td class="text-end">
                  {{ ms(entry.serverMs) }}
                </td>
                <td class="text-end">
                  {{ ms(entry.clientMs) }}
                </td>
              </tr>
            </tbody>
          </table>
        </div>
      </v-card>
    </v-menu>
  </span>
</template>

<script setup>
import { computed, ref } from 'vue';
import { useI18n } from 'vue-i18n';
import { commaString } from './vueFilters.js';
import { shownBatches, batchLongest as longest, batchSum as sum } from './queryTimes.js';

const { t } = useI18n();

const batches = computed(() => shownBatches());
const current = computed(() => batches.value[0]);

const opened = ref(undefined);
const toggle = (id) => { opened.value = opened.value === id ? undefined : id; };

const ms = (value) => value === undefined ? '-' : `${commaString(value)}ms`;
const timeString = (time) => new Date(time).toLocaleTimeString();
// the ones still running on top, then slowest first, the ones without a time last
const sorted = (batch) => [...batch.entries].sort((a, b) =>
  (b.status === 'pending') - (a.status === 'pending') || (b.serverMs ?? -1) - (a.serverMs ?? -1));

const tip = computed(() => t('queryTimes.tip', {
  longest: ms(longest(current.value)),
  elapsed: ms(current.value.end - current.value.start),
  time: timeString(current.value.start)
}));
</script>

<style scoped>
.query-times-link {
  cursor: pointer;
  text-decoration: underline dotted;
}
.query-times-card {
  padding: 8px 12px;
  max-height: 60vh;
  max-width: 90vw;
  overflow: auto;
  font-size: 0.8rem;
}
.query-times-batch + .query-times-batch {
  margin-top: 6px;
  padding-top: 6px;
  border-top: 1px solid rgb(var(--v-theme-neutral-light));
}
.query-times-summary {
  white-space: nowrap;
}
.query-times-table {
  width: 100%;
  margin-top: 4px;
  border-collapse: collapse;
}
.query-times-table th,
.query-times-table td {
  padding: 1px 8px;
  white-space: nowrap;
}
.query-times-table th:first-child {
  text-align: start;
}
.query-times-table td:first-child {
  max-width: 420px;
  overflow: hidden;
  text-overflow: ellipsis;
}
.query-error td:first-child {
  color: rgb(var(--v-theme-error));
}
.query-cancelled {
  opacity: 0.6;
}
</style>
