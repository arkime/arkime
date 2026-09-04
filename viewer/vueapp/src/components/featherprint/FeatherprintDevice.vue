<!--
Copyright Yahoo Inc.
SPDX-License-Identifier: Apache-2.0
-->
<template>
  <div>
    <dl class="dl-horizontal">
      <template v-if="device.classification">
        <dt>{{ $t('featherprint.classificationLabel') }}</dt>
        <dd>{{ device.classification }}</dd>
      </template>
      <dt>{{ $t('featherprint.firstSeenLabel') }}</dt>
      <dd>{{ ts(device.firstSeen) }}</dd>
      <dt>{{ $t('featherprint.lastSeenLabel') }}</dt>
      <dd>{{ ts(device.lastSeen) }}</dd>
      <dt>{{ $t('featherprint.macLabel') }}</dt>
      <dd>
        <span v-if="device.mac && device.mac.value">
          <arkime-session-field
            :field="macFieldDef"
            expr="mac"
            :value="device.mac.value"
            :pull-left="true"
            :session-btn="true" />
          <small
            v-if="device.mac.source"
            class="text-muted-more">{{ $t('featherprint.viaSource', { source: device.mac.source }) }}</small>
        </span>
        <span
          v-else
          class="text-muted-more">&mdash;</span>
      </dd>
      <template v-if="previousMacs.length">
        <dt>{{ $t('featherprint.previousMacsLabel') }}</dt>
        <dd>{{ previousMacs.join(', ') }}</dd>
      </template>
      <dt>{{ $t('featherprint.namesLabel') }}</dt>
      <dd>
        <span v-if="uniqueNames.length">
          <template
            v-for="(name, i) in uniqueNames"
            :key="name">
            <span v-if="i">, </span>
            <arkime-session-field
              :field="hostFieldDef"
              expr="host"
              :value="name"
              :pull-left="true"
              :session-btn="true" />
          </template>
        </span>
        <span
          v-else
          class="text-muted-more">&mdash;</span>
      </dd>
      <template v-if="device.dhcp">
        <dt>{{ $t('featherprint.dhcpLabel') }}</dt>
        <dd>vendor={{ device.dhcp.vendorClass || '—' }}, paramReqList={{ device.dhcp.paramReqList || '—' }}</dd>
      </template>
      <dt>{{ $t('featherprint.servicesLabel') }}</dt>
      <dd>{{ device.services ? device.services.length : 0 }}</dd>
    </dl>

    <v-card
      density="compact"
      class="arkime-card">
      <!-- +/- collapse header, same idiom as the spiview category cards -->
      <v-card-title
        class="d-flex align-center ga-2 cursor-pointer"
        :class="{ collapsed: !historyOpen }"
        @click="historyOpen = !historyOpen">
        {{ $t('featherprint.historyTitle') }}
        <v-chip
          v-if="history.length"
          size="x-small">
          {{ history.length }}
        </v-chip>
        <v-icon
          icon="mdi-minus"
          class="when-opened ms-auto" />
        <v-icon
          icon="mdi-plus"
          class="when-closed ms-auto" />
      </v-card-title>
      <v-expand-transition>
        <v-card-text v-show="historyOpen">
          <div
            v-if="!history.length"
            class="text-muted-more">
            {{ $t('featherprint.noHistoryRecords') }}
          </div>
          <table
            v-else
            class="arkime-table featherprint-history-table">
            <thead>
              <tr>
                <th class="text-start">
                  {{ $t('featherprint.colTime') }}
                </th>
                <th class="text-start">
                  {{ $t('featherprint.colKind') }}
                </th>
                <th class="text-start">
                  {{ $t('featherprint.colDetails') }}
                </th>
              </tr>
            </thead>
            <tbody>
              <tr
                v-for="(h, i) in history"
                :key="i">
                <td>{{ ts(h.ts) }}</td>
                <td><code>{{ h.kind }}</code></td>
                <td><small>{{ describeHistory(h) }}</small></td>
              </tr>
            </tbody>
          </table>
        </v-card-text>
      </v-expand-transition>
    </v-card>
  </div>
</template>

<script>
import { fmtTs, describeHistory, MAC_FIELD, HOST_FIELD } from './featherprintUtils.js';

export default {
  name: 'FeatherprintDevice',
  props: {
    device: { type: Object, required: true },
    history: { type: Array, default: () => [] }
  },
  data () {
    return {
      historyOpen: true
    };
  },
  computed: {
    macFieldDef: () => MAC_FIELD,
    hostFieldDef: () => HOST_FIELD,
    uniqueNames () {
      return [...new Set((this.device.names ?? []).map(n => n.name))];
    },
    previousMacs () {
      const current = this.device.mac?.value;
      return [...new Set((this.device.mac?.history ?? []).map(h => h.mac).filter(m => m && m !== current))];
    }
  },
  methods: {
    describeHistory,
    ts (v) {
      return fmtTs(v, this.$store?.state?.user?.settings);
    }
  }
};
</script>

<style scoped>
/* the shared dl-horizontal leaves dt at normal weight; bold the labels here
   without touching Help / History / EsShards / ESHealth which also use it */
:deep(dl.dl-horizontal dt) {
  font-weight: bold;
}

.featherprint-history-table {
  width: 100%;
}
</style>
