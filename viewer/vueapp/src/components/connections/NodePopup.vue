<!--
Copyright Yahoo Inc.
SPDX-License-Identifier: Apache-2.0
-->
<template>
  <v-card
    class="connections-popup"
    elevation="8"
    rounded="lg">
    <!-- header: node identity + close -->
    <div class="connections-popup-header d-flex align-center ga-2 px-3 py-2">
      <v-icon
        :icon="typeIcon"
        :color="typeColor"
        size="small" />
      <strong class="flex-grow-1 text-truncate text-body-2">
        <arkime-session-field
          :value="dataNode.id"
          :session="dataNode"
          :expr="dataNode.exp"
          :field="fields[dataNode.dbField]"
          :pull-left="true" />
      </strong>
      <v-btn
        icon="mdi-close"
        size="x-small"
        variant="text"
        density="comfortable"
        aria-label="Close"
        @click="$emit('close')" />
    </div>

    <v-divider />

    <!-- key / value details -->
    <dl class="connections-popup-grid px-3 py-2">
      <dt>{{ $t('connections.type') }}</dt>
      <dd>
        <v-chip
          :color="typeColor"
          size="x-small"
          variant="tonal"
          label>
          {{ ['', 'Source', 'Target', 'Both'][dataNode.type] }}
        </v-chip>
      </dd>

      <dt>{{ $t('connections.links') }}</dt>
      <dd>{{ commaString(dataNode.weight || dataNode.cnt) }}</dd>

      <dt>{{ $t('common.sessions') }}</dt>
      <dd>{{ commaString(dataNode.sessions) }}</dd>

      <template
        v-for="fieldKey in nodeFields"
        :key="fieldKey">
        <template v-if="fields[fieldKey]">
          <dt>{{ fields[fieldKey].friendlyName }}</dt>
          <dd>
            <template v-if="!Array.isArray(dataNode[fieldKey])">
              <arkime-session-field
                :value="dataNode[fieldKey]"
                :session="dataNode"
                :expr="fields[fieldKey].exp"
                :field="fields[fieldKey]"
                :pull-left="true" />
            </template>
            <template v-else>
              <arkime-session-field
                v-for="(value, index) in dataNode[fieldKey]"
                :key="`${fieldKey}-${index}`"
                :value="value"
                :session="dataNode"
                :expr="fields[fieldKey].exp"
                :field="fields[fieldKey]"
                :pull-left="true" />
            </template>
          </dd>
        </template>
      </template>

      <template v-if="baselineDate !== '0'">
        <dt>{{ $t('connections.resultSet') }}</dt>
        <dd>{{ ['', '✨ Actual', '🚫 Baseline', 'Both'][dataNode.inresult] }}</dd>
      </template>
    </dl>

    <v-divider />

    <!-- actions -->
    <div class="px-2 py-1">
      <v-btn
        variant="text"
        size="small"
        color="primary"
        prepend-icon="mdi-eye-off"
        @click.stop.prevent="$emit('hideNode')">
        {{ $t('connections.hideNode') }}
      </v-btn>
    </div>
  </v-card>
</template>

<script setup>
import { computed } from 'vue';
import ArkimeSessionField from '../sessions/SessionField.vue';
import { commaString } from '@common/vueFilters.js';

// Define Props
const props = defineProps({
  dataNode: {
    type: Object,
    required: true
  },
  fields: {
    type: Object,
    required: true
  },
  baselineDate: {
    type: String,
    default: '0'
  },
  nodeFields: {
    type: Array,
    required: true
  }
});

// Define Emits
defineEmits(['hideNode', 'close']);

// type 1 = source, 2 = target, 3 = both -- mirror the graph/legend colors
const typeColor = computed(() => ['', 'primary', 'secondary', 'tertiary'][props.dataNode.type] || 'primary');
const typeIcon = computed(() => ({
  1: 'mdi-ray-start-arrow',
  2: 'mdi-ray-end-arrow',
  3: 'mdi-ray-start-end'
}[props.dataNode.type] || 'mdi-circle-medium'));
</script>

<style scoped>
.connections-popup-header strong {
  min-width: 0;
}
</style>
