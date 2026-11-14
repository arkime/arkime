<!--
Copyright Yahoo Inc.
SPDX-License-Identifier: Apache-2.0
-->
<template>
  <v-card
    class="connections-popup"
    elevation="8"
    rounded="lg">
    <!-- header -->
    <div class="connections-popup-header d-flex align-center ga-2 px-3 py-2">
      <v-icon
        icon="mdi-vector-polyline"
        color="primary"
        size="small" />
      <strong class="flex-grow-1 text-body-2">
        Link
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

    <!-- endpoints: source over target, each on its own line so long
         values never wrap around the connector -->
    <div class="connections-popup-endpoints px-3 py-2">
      <div class="connections-popup-endpoint">
        <span class="connections-popup-endpoint-label text-primary">Source</span>
        <arkime-session-field
          :value="dataLink.source.id"
          :session="dataLink"
          :expr="dataLink.srcExp"
          :field="fields[dataLink.srcDbField]"
          :pull-left="true" />
      </div>
      <v-icon
        icon="mdi-arrow-down-thin"
        size="small"
        class="text-medium-emphasis connections-popup-arrow" />
      <div class="connections-popup-endpoint">
        <span class="connections-popup-endpoint-label text-secondary">Target</span>
        <arkime-session-field
          :value="dataLink.target.id"
          :session="dataLink"
          :expr="dataLink.dstExp"
          :field="fields[dataLink.dstDbField]"
          :pull-left="true" />
      </div>
    </div>

    <v-divider />

    <!-- key / value details -->
    <dl class="connections-popup-grid px-3 py-2">
      <dt>{{ $t('common.sessions') }}</dt>
      <dd>{{ commaString(dataLink.value) }}</dd>

      <template
        v-for="field in linkFields"
        :key="field">
        <template v-if="fields[field]">
          <dt>{{ fields[field].friendlyName }}</dt>
          <dd>
            <template v-if="!Array.isArray(dataLink[field])">
              <arkime-session-field
                :value="dataLink[field]"
                :session="dataLink"
                :expr="fields[field].exp"
                :field="fields[field]"
                :pull-left="true" />
            </template>
            <template v-else>
              <arkime-session-field
                v-for="value in dataLink[field]"
                :key="`${field}-${value}`"
                :value="value"
                :session="dataLink"
                :expr="fields[field].exp"
                :field="fields[field]"
                :pull-left="true" />
            </template>
          </dd>
        </template>
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
        @click.stop.prevent="$emit('hideLink')">
        {{ $t('connections.hideLink') }}
      </v-btn>
    </div>
  </v-card>
</template>

<script setup>
import ArkimeSessionField from '../sessions/SessionField.vue';
import { commaString } from '@common/vueFilters.js';

// Define Props
defineProps({
  dataLink: {
    type: Object,
    required: true
  },
  fields: {
    type: Object,
    required: true
  },
  linkFields: {
    type: Array,
    required: true
  }
});

// Define Emits
defineEmits(['hideLink', 'close']);
</script>

<style scoped>
.connections-popup-endpoints {
  display: flex;
  flex-direction: column;
  align-items: flex-start;
  gap: 2px;
}
.connections-popup-endpoint {
  font-weight: 600;
  min-width: 0;
  width: 100%;
}
.connections-popup-endpoint-label {
  display: block;
  font-size: 0.6875rem;
  font-weight: 600;
  text-transform: uppercase;
  letter-spacing: 0.04em;
  margin-bottom: 1px;
}
.connections-popup-arrow {
  margin-left: 2px;
}
</style>
