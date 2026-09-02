<!--
Copyright Yahoo Inc.
SPDX-License-Identifier: Apache-2.0
-->
<template>
  <div class="pa-3">
    <h2 class="mb-3">
      {{ $t('navigation.featherprint') }}
      <span class="text-caption text-disabled ms-2">— {{ section }}</span>
    </h2>

    <v-btn-toggle
      :model-value="section"
      @update:model-value="goToSection"
      density="compact"
      variant="text"
      color="primary"
      mandatory
      class="mb-3">
      <v-btn value="tracked">
        <v-icon
          start
          icon="mdi-format-list-bulleted" />
        {{ $t('featherprint.trackedIps') }}
      </v-btn>
      <v-btn value="lookup">
        <v-icon
          start
          icon="mdi-magnify" />
        {{ $t('featherprint.lookup') }}
      </v-btn>
      <v-btn value="alerts">
        <v-icon
          start
          icon="mdi-alert-circle-outline" />
        {{ $t('featherprint.alerts') }}
        <v-chip
          v-if="openAlertCount"
          size="x-small"
          class="ms-2">
          {{ openAlertCount }}
        </v-chip>
      </v-btn>
      <v-btn
        v-if="isAdmin"
        value="admin">
        <v-icon
          start
          icon="mdi-cog-outline" />
        {{ $t('navigation.admin') }}
      </v-btn>
    </v-btn-toggle>

    <v-card
      class="mb-3">
      <v-card-text class="d-flex align-center">
        <div class="flex-grow-1">
          {{ $t('featherprint.modeLabel') }} <strong>viewer-resident</strong> |
          {{ $t('featherprint.engineLabel') }} <strong>{{ engineMode }}</strong>
          <span
            v-if="monitorState"
            class="text-caption text-disabled ms-2">
            | {{ $t('featherprint.cursorInfo', {
              cursor: fmtTs(monitorState.lpValue * 1000),
              first: fmtTs(monitorState.firstProcessedTs),
              last: fmtTs(monitorState.lastProcessedTs)
            }) }}
          </span>
        </div>
        <v-switch
          v-model="autoRefresh"
          color="primary"
          density="compact"
          hide-details
          :label="$t('featherprint.autoRefreshLabel')"
          class="me-3" />
        <v-btn
          size="small"
          variant="text"
          @click="refreshAll">
          {{ $t('common.refresh') }}
        </v-btn>
      </v-card-text>
    </v-card>

    <v-card
      v-if="section === 'lookup'"
      class="mb-3">
      <v-card-title class="pb-1">
        {{ $t('featherprint.lookupTitle') }}
        <span class="text-caption text-disabled ms-2">
          {{ $t('featherprint.lookupTransientNote') }}
        </span>
      </v-card-title>
      <v-card-text class="d-flex align-center flex-wrap ga-2">
        <v-text-field
          v-model="lookupIp"
          density="compact"
          variant="outlined"
          hide-details
          :placeholder="$t('featherprint.ipPlaceholder')"
          style="max-width: 260px;"
          @keyup.enter="doLookup" />
        <v-select
          v-model="lookupRange"
          :items="rangeOptions"
          item-title="label"
          item-value="seconds"
          density="compact"
          variant="outlined"
          hide-details
          :label="$t('featherprint.timeRangeLabel')"
          style="max-width: 200px;" />
        <v-btn
          color="primary"
          size="small"
          :loading="lookupBusy"
          :disabled="!lookupIp"
          @click="doLookup">
          {{ $t('featherprint.lookup') }}
        </v-btn>
        <span
          v-if="lookupResultTs"
          class="text-caption text-disabled ms-2">
          {{ $t('featherprint.lookedUpAt', { ts: fmtTs(lookupResultTs) }) }}
        </span>
      </v-card-text>
    </v-card>

    <v-card
      class="mb-3"
      v-if="error">
      <v-card-text class="text-error">
        {{ error }}
      </v-card-text>
    </v-card>

    <v-card
      v-if="section === 'admin' && isAdmin"
      class="mb-3">
      <v-card-title class="pb-1">
        {{ $t('navigation.admin') }}
        <v-btn
          size="x-small"
          variant="tonal"
          class="ms-2"
          @click="showConfig = !showConfig">
          {{ showConfig ? $t('featherprint.hideSubnets') : $t('featherprint.viewSubnets') }}
        </v-btn>
        <v-btn
          size="x-small"
          color="primary"
          variant="tonal"
          class="ms-2"
          :loading="tickBusy"
          @click="runTickNow">
          {{ $t('featherprint.runTickNow') }}
        </v-btn>
        <span
          v-if="tickStatus"
          class="text-caption text-disabled ms-3">{{ tickStatus }}</span>
      </v-card-title>
      <v-card-text v-if="showConfig">
        <div v-if="!adminConfig">
          {{ $t('common.loading') }}
        </div>
        <div v-else>
          <strong>{{ $t('featherprint.defaultsLabel') }}</strong>
          <code>{{ JSON.stringify(adminConfig.defaults) }}</code>
          <div class="mt-2">
            <strong>{{ $t('featherprint.subnetsCountLabel', { count: adminConfig.subnets.length }) }}</strong>
          </div>
          <v-table
            density="compact"
            class="mt-1">
            <thead>
              <tr><th>{{ $t('featherprint.colCidr') }}</th><th>{{ $t('featherprint.colFlags') }}</th></tr>
            </thead>
            <tbody>
              <tr
                v-for="s in adminConfig.subnets"
                :key="s.cidr">
                <td><code>{{ s.cidr }}</code></td>
                <td><code>{{ JSON.stringify(s.flags) }}</code></td>
              </tr>
              <tr v-if="!adminConfig.subnets.length">
                <td
                  colspan="2"
                  class="text-disabled">
                  {{ $t('featherprint.noSubnetsConfigured') }}
                </td>
              </tr>
            </tbody>
          </v-table>
          <div class="mt-2">
            <strong>{{ $t('featherprint.settingsLabel') }}</strong>
          </div>
          <pre class="text-caption">{{ JSON.stringify(adminConfig.settings, null, 2) }}</pre>
        </div>
      </v-card-text>
    </v-card>

    <v-card
      v-if="section === 'alerts'"
      class="mb-3">
      <v-card-title class="d-flex align-center">
        {{ $t('featherprint.alerts') }}
        <v-chip
          size="x-small"
          class="ms-2">
          {{ filteredAlerts.length }} / {{ alerts.length }}
        </v-chip>
        <v-spacer />
        <v-btn-toggle
          v-model="alertFilter"
          density="compact"
          mandatory
          color="primary"
          variant="outlined">
          <v-btn
            value="open"
            size="small">
            {{ $t('featherprint.filterOpenCount', { count: openAlertCount }) }}
          </v-btn>
          <v-btn
            value="acked"
            size="small">
            {{ $t('featherprint.filterAcked') }}
          </v-btn>
          <v-btn
            value="all"
            size="small">
            {{ $t('common.all') }}
          </v-btn>
        </v-btn-toggle>
      </v-card-title>
      <v-card-text class="py-1">
        <v-text-field
          v-model="alertSearch"
          density="compact"
          variant="outlined"
          hide-details
          clearable
          :placeholder="$t('featherprint.alertFilterPlaceholder')"
          prepend-inner-icon="mdi-magnify" />
      </v-card-text>
      <v-data-table
        :headers="alertHeaders"
        :items="filteredAlerts"
        :items-per-page="25"
        :items-per-page-options="[10, 25, 50, 100, -1]"
        :sort-by="[{ key: 'ts', order: 'desc' }]"
        density="compact"
        item-value="_id"
        class="featherprint-alert-table"
        must-sort
        hover>
        <template #item.ts="{ item }">
          {{ fmtTs(item.ts) }}
        </template>
        <template #item.kind="{ item }">
          <code>{{ item.kind }}</code>
        </template>
        <template #item.ip="{ item }">
          <a
            href="#"
            @click.prevent="goToInfoForIp(item.ip)">{{ item.ip }}</a>
        </template>
        <template #item.details="{ item }">
          <span class="text-caption">{{ describeHistory(item) }}</span>
        </template>
        <template #item.acked="{ item }">
          <span
            v-if="item.acked"
            class="text-success">
            <v-icon
              size="small"
              icon="mdi-check-circle" />
            {{ item.ackedBy || $t('featherprint.unknownUser') }}
            <span
              v-if="item.ackedAt"
              class="text-caption ms-1">{{ fmtTs(item.ackedAt) }}</span>
          </span>
          <span
            v-else
            class="text-disabled">—</span>
        </template>
        <template #item.actions="{ item }">
          <v-btn
            v-if="!item.acked"
            size="x-small"
            variant="tonal"
            color="primary"
            :loading="ackBusy === item._id"
            @click.stop="ack(item._id)">
            {{ $t('featherprint.ack') }}
          </v-btn>
        </template>
        <template #no-data>
          <span class="text-disabled">
            <span v-if="alertFilter === 'open'">{{ $t('featherprint.noOpenAlerts') }}</span>
            <span v-else-if="alertFilter === 'acked'">{{ $t('featherprint.noAckedAlerts') }}</span>
            <span v-else>{{ $t('featherprint.noAlerts') }}</span>
          </span>
        </template>
      </v-data-table>
    </v-card>

    <div v-if="section === 'tracked' || section === 'lookup'">
      <v-card
        v-if="detail"
        class="mb-3">
        <v-card-title>
          <arkime-session-field
            :field="ipFieldDef"
            expr="ip"
            :value="detail.ip"
            :pull-left="true"
            :session-btn="true" />
        </v-card-title>
        <v-card-text>
          <div v-if="detail.classification">
            <strong>{{ $t('featherprint.classificationLabel') }}</strong>
            {{ detail.classification }}
          </div>
          <div><strong>{{ $t('featherprint.firstSeenLabel') }}</strong> {{ fmtTs(detail.firstSeen) }}</div>
          <div><strong>{{ $t('featherprint.lastSeenLabel') }}</strong> {{ fmtTs(detail.lastSeen) }}</div>
          <div>
            <strong>{{ $t('featherprint.macLabel') }}</strong>&nbsp;
            <span v-if="detail.mac && detail.mac.value">
              <arkime-session-field
                :field="macFieldDef"
                expr="mac"
                :value="detail.mac.value"
                :pull-left="true"
                :session-btn="true" />
              <span
                v-if="detail.mac.source"
                class="text-disabled">{{ $t('featherprint.viaSource', { source: detail.mac.source }) }}</span>
            </span>
            <span v-else>—</span>
          </div>
          <div v-if="previousMacs.length">
            <strong>{{ $t('featherprint.previousMacsLabel') }}</strong>
            {{ previousMacs.join(', ') }}
          </div>
          <div>
            <strong>{{ $t('featherprint.namesLabel') }}</strong>&nbsp;
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
            <span v-else>—</span>
          </div>
          <div v-if="detail.dhcp">
            <strong>{{ $t('featherprint.dhcpLabel') }}</strong>
            vendor={{ detail.dhcp.vendorClass || '—' }},
            paramReqList={{ detail.dhcp.paramReqList || '—' }}
          </div>
          <div>
            <strong>{{ $t('featherprint.servicesLabel') }}</strong>&nbsp;
            <span v-if="detail.services && detail.services.length">{{ detail.services.length }}</span>
            <span v-else>0</span>
          </div>

          <v-divider class="my-2" />
          <h4>{{ $t('featherprint.historyTitle') }}</h4>
          <div
            v-if="history.length === 0"
            class="text-disabled">
            {{ $t('featherprint.noHistoryRecords') }}
          </div>
          <v-list
            density="compact"
            v-else
            max-height="500"
            class="overflow-y-auto">
            <v-list-item
              v-for="(h, i) in history"
              :key="i">
              <v-list-item-title>
                <code>{{ h.kind }}</code>
                <span class="ms-2">{{ describeHistory(h) }}</span>
              </v-list-item-title>
              <v-list-item-subtitle>{{ fmtTs(h.ts) }}</v-list-item-subtitle>
            </v-list-item>
          </v-list>
        </v-card-text>
      </v-card>
      <v-card
        v-else
        class="mb-3">
        <v-card-text class="text-disabled">
          {{ section === 'lookup' ? $t('featherprint.enterIpPrompt') : $t('featherprint.selectIpPrompt') }}
        </v-card-text>
      </v-card>

      <v-card v-if="section === 'tracked'">
        <v-card-title>
          {{ $t('featherprint.trackedIps') }}
          <v-chip
            size="x-small"
            class="ms-2">
            {{ filteredIps.length }} / {{ ips.length }}
          </v-chip>
        </v-card-title>
        <v-card-text class="py-1">
          <v-text-field
            v-model="filter"
            density="compact"
            variant="outlined"
            hide-details
            clearable
            :placeholder="$t('featherprint.trackedFilterPlaceholder')"
            prepend-inner-icon="mdi-magnify" />
        </v-card-text>
        <v-data-table
          :headers="ipHeaders"
          :items="filteredIps"
          :items-per-page="25"
          :items-per-page-options="[10, 25, 50, 100, -1]"
          density="compact"
          item-value="ip"
          class="featherprint-ip-table"
          must-sort
          hover
          @click:row="onRowClick">
          <template #item.ip="{ item }">
            <span @click.stop>
              <arkime-session-field
                :field="ipFieldDef"
                expr="ip"
                :value="item.ip"
                :pull-left="true"
                :session-btn="true" />
            </span>
          </template>
          <template #item.classification="{ item }">
            <span v-if="item.classification && item.classification !== 'unknown'">
              {{ item.classification }}
            </span>
            <span v-else>—</span>
          </template>
          <template #item.name="{ item }">
            <span
              v-if="item.names && item.names.length"
              @click.stop>
              <arkime-session-field
                :field="hostFieldDef"
                expr="host"
                :value="item.names[0].name"
                :pull-left="true"
                :session-btn="true" />
            </span>
            <span v-else>—</span>
          </template>
          <template #item.mac.value="{ item }">
            <span
              v-if="item.mac && item.mac.value"
              @click.stop>
              <arkime-session-field
                :field="macFieldDef"
                expr="mac"
                :value="item.mac.value"
                :pull-left="true"
                :session-btn="true" />
            </span>
            <span v-else>—</span>
          </template>
          <template #item.lastSeen="{ item }">
            {{ fmtTs(item.lastSeen) }}
          </template>
          <template #no-data>
            <span class="text-disabled">
              {{ ips.length ? $t('featherprint.noMatches') : $t('featherprint.noTrackedIps') }}
            </span>
          </template>
        </v-data-table>
      </v-card>
    </div>
  </div>
</template>

<script>
import { fetchWrapper } from '@common/fetchWrapper';
import { timezoneDateString } from '@common/vueFilters.js';

const REFRESH_MS = 30000;

export default {
  name: 'Featherprint',
  data () {
    return {
      engineMode: 'monitor + lookup',
      ips: [],
      filter: '',
      selected: null,
      detail: null,
      history: [],
      alerts: [],
      alertFilter: 'open', // 'open' | 'acked' | 'all'
      alertSearch: '',
      error: null,
      autoRefresh: false,
      ackBusy: null,
      timer: null,
      lookupIp: '',
      lookupRange: 3600,
      lookupCustomRange: null, // { startSec, stopSec } when arriving from a value-action URL
      lookupBusy: false,
      lookupResultTs: null,
      monitorState: null,
      adminConfig: null,
      showConfig: false,
      tickBusy: false,
      tickStatus: '',
      // grouped-expression field defs for arkime-session-field pivot menus.
      // `category` drives which value actions appear (SessionField filters on
      // it) -- ip => reverseDNS, featherprintLookup, admin ip actions; host
      // => host actions. (mac fields have no category anywhere.)
      ipFieldDef: { dbField: 'ip', exp: 'ip', type: 'ip', group: 'general', category: 'ip', friendlyName: 'All IP fields' },
      macFieldDef: { dbField: 'mac', exp: 'mac', type: 'lotermfield', group: 'general', friendlyName: 'All MAC fields' },
      hostFieldDef: { dbField: 'host', exp: 'host', type: 'lotermfield', group: 'general', category: 'host', friendlyName: 'All host fields' }
    };
  },
  computed: {
    isAdmin () {
      return !!this.$store?.state?.user?.roles?.includes('arkimeAdmin');
    },
    section () {
      const n = this.$route?.name || '';
      if (n === 'FeatherprintAlerts') return 'alerts';
      if (n === 'FeatherprintAdmin') return 'admin';
      if (n === 'FeatherprintLookup') return 'lookup';
      return 'tracked';
    },
    ipHeaders () {
      return [
        { title: this.$t('featherprint.colIp'), key: 'ip', sortable: true },
        { title: this.$t('featherprint.colMac'), key: 'mac.value', sortable: true },
        { title: this.$t('featherprint.colName'), key: 'name', sortable: true, value: item => item.names?.[0]?.name ?? '' },
        { title: this.$t('featherprint.colClassification'), key: 'classification', sortable: true, value: item => item.classification ?? '' },
        { title: this.$t('featherprint.colLastSeen'), key: 'lastSeen', sortable: true }
      ];
    },
    rangeOptions () {
      return [
        { label: this.$t('search.lastRange', { range: this.$t('common.minuteCount', 15) }), seconds: 900 },
        { label: this.$t('search.lastRange', { range: this.$t('common.hourCount', 1) }), seconds: 3600 },
        { label: this.$t('search.lastRange', { range: this.$t('common.hourCount', 6) }), seconds: 21600 },
        { label: this.$t('search.lastRange', { range: this.$t('common.hourCount', 24) }), seconds: 86400 },
        { label: this.$t('search.lastRange', { range: this.$t('common.dayCount', 7) }), seconds: 604800 },
        { label: this.$t('search.lastRange', { range: this.$t('common.dayCount', 30) }), seconds: 2592000 }
      ];
    },
    filteredAlerts () {
      let list = this.alerts;
      if (this.alertFilter === 'acked') list = list.filter(a => a.acked);
      else if (this.alertFilter === 'open') list = list.filter(a => !a.acked);
      const q = (this.alertSearch || '').trim().toLowerCase();
      if (!q) return list;
      return list.filter(a => {
        if (a.kind && String(a.kind).toLowerCase().includes(q)) return true;
        if (a.ip && String(a.ip).toLowerCase().includes(q)) return true;
        if (a.ackedBy && String(a.ackedBy).toLowerCase().includes(q)) return true;
        const details = this.describeHistory(a);
        if (details && details.toLowerCase().includes(q)) return true;
        return false;
      });
    },
    alertHeaders () {
      return [
        { title: this.$t('featherprint.colTime'), key: 'ts', sortable: true },
        { title: this.$t('featherprint.colKind'), key: 'kind', sortable: true },
        { title: this.$t('featherprint.colIp'), key: 'ip', sortable: true },
        { title: this.$t('featherprint.colDetails'), key: 'details', sortable: false, value: item => this.describeHistory(item) },
        { title: this.$t('featherprint.colAcked'), key: 'acked', sortable: true, value: item => item.ackedBy || '' },
        { title: '', key: 'actions', sortable: false, width: 80 }
      ];
    },
    openAlertCount () {
      return this.alerts.filter(a => !a.acked).length;
    },
    uniqueNames () {
      return [...new Set((this.detail?.names ?? []).map(n => n.name))];
    },
    previousMacs () {
      const current = this.detail?.mac?.value;
      const macHistory = this.detail?.mac?.history ?? [];
      return [...new Set(macHistory.map(h => h.mac).filter(m => m && m !== current))];
    },
    filteredIps () {
      const q = (this.filter || '').trim().toLowerCase();
      if (!q) return this.ips;
      return this.ips.filter(r => {
        if (r.ip && r.ip.toLowerCase().includes(q)) return true;
        if (r.names && r.names.some(n => n && n.name && n.name.toLowerCase().includes(q))) return true;
        if (r.mac && r.mac.value && r.mac.value.toLowerCase().includes(q)) return true;
        if (r.mac && r.mac.history && r.mac.history.some(h => h && h.mac && h.mac.toLowerCase().includes(q))) return true;
        if (r.classification && String(r.classification).toLowerCase().includes(q)) return true;
        return false;
      });
    }
  },
  watch: {
    autoRefresh (on) {
      this.stopTimer();
      if (on) this.timer = setInterval(this.refreshAll, REFRESH_MS);
    },
    showConfig (v) {
      if (v && !this.adminConfig) this.loadAdminConfig();
    },
    lookupRange () {
      // User picked a preset; drop any URL-provided custom window.
      this.lookupCustomRange = null;
    },
    '$route' () {
      this.applyLookupRouteQuery();
    }
  },
  mounted () {
    this.refreshAll();
    this.applyLookupRouteQuery();
  },
  beforeUnmount () {
    this.stopTimer();
  },
  methods: {
    stopTimer () {
      if (this.timer) { clearInterval(this.timer); this.timer = null; }
    },
    onRowClick (_event, row) {
      const ip = row?.item?.ip ?? row?.item?.raw?.ip;
      if (ip) this.selectIp(ip);
    },
    fmtTs (ts) {
      if (!ts) return '—';
      // Render in the user's configured timezone like every other viewer page,
      // not hard-coded UTC.
      const settings = this.$store?.state?.user?.settings;
      return timezoneDateString(ts, settings?.timezone ?? 'local', settings?.ms ?? false);
    },
    describeHistory (h) {
      const b = h.before;
      const a = h.after;
      switch (h.kind) {
      case 'newIp':
        return a?.ip ? `${a.ip}${a.classification ? ` (${a.classification})` : ''}` : '';
      case 'newMac':
        return a?.value ? `${a.value}${a.source ? ` via ${a.source}` : ''}` : '';
      case 'changeMac':
        return `${b?.value ?? '?'} → ${a?.value ?? '?'}`;
      case 'changeIp':
        return `${b?.mac ?? '?'}: ${b?.ip ?? '?'} → ${a?.ip ?? '?'}`;
      case 'newName':
        return a?.name ? `${a.name}${a.source ? ` (${a.source})` : ''}` : '';
      case 'changeName':
        if (b && a) return `${b?.name ?? '?'} → ${a?.name ?? '?'}`;
        return a?.name ? `${a.name}${a.source ? ` (${a.source})` : ''}` : '';
      case 'newService':
        if (!a) return '';
        return `${a.type ?? '?'} (${a.proto ?? '?'}${a.port !== undefined ? '/' + a.port : ''})`;
      default:
        if (b && a) return `${JSON.stringify(b)} → ${JSON.stringify(a)}`;
        if (a) return JSON.stringify(a);
        return '';
      }
    },
    refreshAll () {
      this.loadIps();
      this.loadAlerts();
      this.loadState();
      // On the lookup tab `detail` holds a transient (unpersisted) result;
      // re-selecting would fetch the persisted /ip/:ip record and 404, wiping
      // it. The lookup section refreshes via doLookup instead.
      if (this.selected && this.section !== 'lookup') this.selectIp(this.selected);
    },
    async goToInfoForIp (ip) {
      await this.$router.push({ name: 'FeatherprintTracked' });
      this.selectIp(ip);
    },
    goToSection (section) {
      if (!section || section === this.section) return;
      const routeName =
        section === 'lookup' ? 'FeatherprintLookup' :
          section === 'alerts' ? 'FeatherprintAlerts' :
            section === 'admin' ? 'FeatherprintAdmin' :
              'FeatherprintTracked';
      this.$router.push({ name: routeName });
    },
    async loadState () {
      try {
        const r = await fetchWrapper({ url: 'api/featherprint/state' });
        this.monitorState = r?.state || null;
      } catch { /* state is informational only */ }
    },
    async loadIps () {
      try {
        const r = await fetchWrapper({ url: 'api/featherprint/search?limit=500' });
        this.ips = r.devices || [];
      } catch (err) {
        this.error = this.$t('featherprint.errorListIps', { reason: err.text ?? err.message ?? err });
      }
    },
    async loadAlerts () {
      try {
        const r = await fetchWrapper({ url: 'api/featherprint/alerts?limit=500' });
        this.alerts = r.alerts || [];
      } catch (err) {
        this.error = this.$t('featherprint.errorLoadAlerts', { reason: err.text ?? err.message ?? err });
      }
    },
    async selectIp (ip) {
      this.selected = ip;
      this.error = null;
      this.lookupResultTs = null;
      try {
        const d = await fetchWrapper({ url: `api/featherprint/ip/${encodeURIComponent(ip)}` });
        this.detail = d?.device ?? null;
      } catch (err) {
        this.detail = null;
        this.error = this.$t('featherprint.errorLoadDetail', { ip, reason: err.text ?? err.message ?? err });
      }
      try {
        const h = await fetchWrapper({ url: `api/featherprint/history/${encodeURIComponent(ip)}?limit=50` });
        this.history = h?.history || [];
      } catch (err) {
        this.history = [];
        this.error = this.$t('featherprint.errorLoadHistory', { ip, reason: err.text ?? err.message ?? err });
      }
    },
    async ack (alertId) {
      this.ackBusy = alertId;
      try {
        const r = await fetchWrapper({
          url: `api/featherprint/ack/${encodeURIComponent(alertId)}`,
          method: 'POST',
          data: {}
        });
        const idx = this.alerts.findIndex(a => a._id === alertId);
        if (idx >= 0) {
          this.alerts[idx] = {
            ...this.alerts[idx],
            acked: true,
            ackedBy: r?.ackedBy ?? this.$t('featherprint.me'),
            ackedAt: r?.ackedAt ?? Date.now()
          };
        }
      } catch (err) {
        this.error = this.$t('featherprint.errorAckFailed', { reason: err.text ?? err.message ?? err });
      } finally {
        this.ackBusy = null;
      }
    },
    async doLookup () {
      const ip = (this.lookupIp || '').trim();
      if (!ip) return;
      this.lookupBusy = true;
      this.error = null;
      try {
        let startSec, stopSec;
        if (this.lookupCustomRange) {
          startSec = this.lookupCustomRange.startSec;
          stopSec = this.lookupCustomRange.stopSec;
        } else {
          stopSec = Math.floor(Date.now() / 1000);
          startSec = stopSec - this.lookupRange;
        }
        const qs = `ip=${encodeURIComponent(ip)}&start=${startSec}` +
          (stopSec ? `&stop=${stopSec}` : '');
        const url = `api/featherprint/lookup?${qs}`;
        const r = await fetchWrapper({ url });
        this.detail = r?.device ?? null;
        this.history = [];
        this.selected = ip;
        this.lookupResultTs = Date.now();
        if (!this.detail) {
          this.error = this.$t('featherprint.errorNoSignals', { ip });
        }
      } catch (err) {
        this.detail = null;
        this.error = this.$t('featherprint.errorLookupFailed', { ip, reason: err.text ?? err.message ?? err });
      } finally {
        this.lookupBusy = false;
      }
    },
    // Populate the lookup form from the route query and auto-run. Supports
    // Arkime's standard time params:
    //   startTime=<sec>&stopTime=<sec>  -- absolute window
    //   date=<hours>                    -- last N hours, -1 means ALL
    //   start=<sec|iso>&stop=<sec|iso>  -- featherprint-native form
    applyLookupRouteQuery () {
      if (this.section !== 'lookup') return;
      const q = this.$route?.query || {};
      if (!q.ip) return;
      const parseTs = (v) => {
        if (v === undefined || v === null || v === '') return null;
        if (/^-?\d+$/.test(String(v))) return parseInt(v, 10);
        const ms = Date.parse(v);
        return isNaN(ms) ? null : Math.floor(ms / 1000);
      };
      const nowSec = Math.floor(Date.now() / 1000);
      let startSec = null;
      let stopSec = nowSec;
      if (q.startTime || q.stopTime) {
        startSec = parseTs(q.startTime);
        stopSec = parseTs(q.stopTime) ?? nowSec;
      } else if (q.date !== undefined) {
        const d = parseInt(q.date, 10);
        if (d === -1) {
          // ALL -- scan from epoch zero up to now.
          startSec = 0;
        } else if (!isNaN(d) && d > 0) {
          startSec = nowSec - d * 3600;
        }
      } else if (q.start || q.stop) {
        startSec = parseTs(q.start);
        stopSec = parseTs(q.stop) ?? nowSec;
      }
      this.lookupIp = String(q.ip);
      this.lookupCustomRange = startSec !== null ? { startSec, stopSec } : null;
      this.doLookup();
    },
    async loadAdminConfig () {
      try {
        const r = await fetchWrapper({ url: 'api/featherprint/config' });
        this.adminConfig = r?.config ?? null;
      } catch (err) {
        this.error = this.$t('featherprint.errorLoadAdminConfig', { reason: err.text ?? err.message ?? err });
      }
    },
    async runTickNow () {
      this.tickBusy = true;
      this.tickStatus = '';
      try {
        const r = await fetchWrapper({
          url: 'api/featherprint/tick',
          method: 'POST',
          data: {}
        });
        if (r?.alreadyRunning) this.tickStatus = this.$t('featherprint.tickAlreadyRunning');
        else if (r?.notPrimary) this.tickStatus = this.$t('featherprint.tickNotPrimary');
        else if (r?.monitorDisabled) this.tickStatus = this.$t('featherprint.tickMonitorDisabled');
        else this.tickStatus = this.$t('featherprint.tickCompleted');
        this.refreshAll();
      } catch (err) {
        this.tickStatus = this.$t('featherprint.tickFailed', { reason: err.text ?? err.message ?? err });
      } finally {
        this.tickBusy = false;
      }
    }
  }
};
</script>

<style scoped>
.featherprint-ip-table :deep(tbody tr) {
  cursor: pointer;
}
</style>
