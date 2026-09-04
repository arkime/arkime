<!--
Copyright Yahoo Inc.
SPDX-License-Identifier: Apache-2.0
-->
<template>
  <page-layout class="featherprint-content">
    <template #chrome>
      <ArkimeCollapsible>
        <div class="page-toolbar">
          <v-row class="g-1 featherprint-form px-1 pt-2 pb-1 align-center justify-start page-subnav">
            <!-- tracked: name/ip/mac filter -->
            <v-col v-if="section === 'tracked'">
              <div class="arkime-input-group arkime-input-group--fluid">
                <span class="arkime-input-label arkime-input-label-fw">
                  <v-icon
                    icon="mdi-magnify"
                    v-if="!shiftKeyHold" />
                  <span
                    v-else
                    class="query-shortcut">
                    Q
                  </span>
                </span>
                <input
                  type="text"
                  class="arkime-input-control"
                  v-model="filter"
                  v-focus="focusInput"
                  @blur="onOffFocus"
                  :placeholder="$t('featherprint.trackedFilterPlaceholder')">
                <v-btn
                  variant="text"
                  size="small"
                  density="comfortable"
                  icon
                  class="arkime-input-append-btn"
                  :disabled="!filter"
                  :aria-label="$t('common.clear')"
                  @click="filter = ''">
                  <v-icon icon="mdi-close" />
                </v-btn>
              </div>
            </v-col>

            <!-- lookup: ip + range + go -->
            <template v-else-if="section === 'lookup'">
              <v-col>
                <div class="arkime-input-group arkime-input-group--fluid">
                  <span class="arkime-input-label arkime-input-label-fw">
                    <v-icon icon="mdi-magnify" />
                  </span>
                  <input
                    type="text"
                    class="arkime-input-control"
                    v-model="lookupIp"
                    v-focus="focusInput"
                    @blur="onOffFocus"
                    @keydown.stop.prevent.enter="doLookup"
                    :placeholder="$t('featherprint.ipPlaceholder')">
                  <v-btn
                    variant="text"
                    size="small"
                    density="comfortable"
                    icon
                    class="arkime-input-append-btn"
                    :disabled="!lookupIp"
                    :aria-label="$t('common.clear')"
                    @click="lookupIp = ''">
                    <v-icon icon="mdi-close" />
                  </v-btn>
                </div>
              </v-col>
              <v-col cols="auto">
                <div class="arkime-input-group">
                  <span class="arkime-input-label">
                    {{ $t('featherprint.timeRangeLabel') }}
                  </span>
                  <select
                    class="arkime-input-control"
                    v-model="lookupRange">
                    <option
                      v-for="opt in rangeOptions"
                      :key="opt.seconds"
                      :value="opt.seconds">
                      {{ opt.label }}
                    </option>
                  </select>
                </div>
              </v-col>
              <v-col cols="auto">
                <v-btn
                  color="primary"
                  variant="flat"
                  size="small"
                  density="comfortable"
                  :loading="lookupBusy"
                  :disabled="!lookupIp"
                  @click="doLookup">
                  {{ $t('featherprint.lookup') }}
                </v-btn>
              </v-col>
            </template>

            <!-- alerts: text filter + ack state -->
            <template v-else-if="section === 'alerts'">
              <v-col>
                <div class="arkime-input-group arkime-input-group--fluid">
                  <span class="arkime-input-label arkime-input-label-fw">
                    <v-icon icon="mdi-magnify" />
                  </span>
                  <input
                    type="text"
                    class="arkime-input-control"
                    v-model="alertSearch"
                    v-focus="focusInput"
                    @blur="onOffFocus"
                    :placeholder="$t('featherprint.alertFilterPlaceholder')">
                  <v-btn
                    variant="text"
                    size="small"
                    density="comfortable"
                    icon
                    class="arkime-input-append-btn"
                    :disabled="!alertSearch"
                    :aria-label="$t('common.clear')"
                    @click="alertSearch = ''">
                    <v-icon icon="mdi-close" />
                  </v-btn>
                </div>
              </v-col>
              <v-col cols="auto">
                <div class="arkime-input-group">
                  <span class="arkime-input-label">
                    {{ $t('featherprint.colAcked') }}
                  </span>
                  <select
                    class="arkime-input-control"
                    v-model="alertFilter">
                    <option value="open">
                      {{ $t('featherprint.filterOpenCount', { count: openAlertCount }) }}
                    </option>
                    <option value="acked">
                      {{ $t('featherprint.filterAcked') }}
                    </option>
                    <option value="all">
                      {{ $t('common.all') }}
                    </option>
                  </select>
                </div>
              </v-col>
            </template>

            <!-- auto refresh interval -->
            <v-col cols="auto">
              <div class="arkime-input-group">
                <span class="arkime-input-label">{{ $t('featherprint.refreshEvery') }}</span>
                <select
                  class="arkime-input-control"
                  v-model="refreshInterval">
                  <option value="5000">
                    {{ $t('common.secondCount', 5) }}
                  </option>
                  <option value="15000">
                    {{ $t('common.secondCount', 15) }}
                  </option>
                  <option value="30000">
                    {{ $t('common.secondCount', 30) }}
                  </option>
                  <option value="60000">
                    {{ $t('common.minuteCount', 1) }}
                  </option>
                  <option value="600000">
                    {{ $t('common.minuteCount', 10) }}
                  </option>
                  <option value="0">
                    {{ $t('common.never') }}
                  </option>
                </select>
              </div>
            </v-col>

            <!-- manual refresh -->
            <v-col cols="auto">
              <v-tooltip location="bottom">
                <template #activator="{ props }">
                  <v-btn
                    v-bind="props"
                    variant="flat"
                    size="small"
                    density="comfortable"
                    :style="tertiaryBtnStyle"
                    @click="refreshAll">
                    <v-icon
                      start
                      icon="mdi-refresh" />
                    {{ $t('common.refresh') }}
                  </v-btn>
                </template>
                {{ $t('featherprint.refreshTip') }}
              </v-tooltip>
            </v-col>

            <!-- monitor cursor -->
            <v-col
              cols="auto"
              v-if="monitorState">
              <v-tooltip location="bottom">
                <template #activator="{ props }">
                  <v-icon
                    v-bind="props"
                    icon="mdi-progress-clock"
                    size="small"
                    class="cursor-help" />
                </template>
                {{ $t('featherprint.cursorInfo', {
                  cursor: fmtTs(monitorState.lpValue * 1000),
                  first: fmtTs(monitorState.firstProcessedTs),
                  last: fmtTs(monitorState.lastProcessedTs)
                }) }}
              </v-tooltip>
            </v-col>
          </v-row>
        </div>
      </ArkimeCollapsible>

      <!-- tab strip: outside the collapsible so it survives a collapsed toolbar -->
      <div class="page-tab-bar">
        <v-btn-toggle
          :model-value="section"
          @update:model-value="goToSection"
          density="compact"
          variant="text"
          color="primary"
          mandatory
          class="page-tab-strip">
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
        </v-btn-toggle>
      </div>
    </template>

    <!-- featherprint content -->
    <div class="mt-4 px-3">
      <!-- alerts -->
      <div
        v-if="section === 'alerts'"
        class="ms-2 me-2">
        <arkime-table
          id="featherprintAlertsTable"
          :data="sortedAlerts"
          :load-data="loadAlertTable"
          :columns="alertColumns"
          :no-results="true"
          :action-column="true"
          :desc="alertSort.desc"
          :sort-field="alertSort.field"
          :no-results-msg="noAlertsMsg"
          page="featherprintAlerts"
          table-animation="list"
          table-state-name="featherprintAlertsCols"
          table-widths-state-name="featherprintAlertsColWidths">
          <template #actions="{ item }">
            <v-btn
              v-if="!item.acked"
              color="primary"
              variant="flat"
              size="small"
              density="comfortable"
              icon
              :loading="ackBusy === item._id"
              :aria-label="$t('featherprint.ack')"
              @click.stop="ack(item._id)">
              <v-icon icon="mdi-check" />
            </v-btn>
          </template>
          <template #cell-ts="{ item }">
            {{ fmtTs(item.ts) }}
          </template>
          <template #cell-kind="{ item }">
            <code>{{ item.kind }}</code>
          </template>
          <template #cell-ip="{ item }">
            <a
              href="#"
              @click.prevent="goToInfoForIp(item.ip)">{{ item.ip }}</a>
          </template>
          <template #cell-details="{ item }">
            <small>{{ item.details }}</small>
          </template>
          <template #cell-acked="{ item }">
            <span
              v-if="item.acked"
              class="text-theme-accent">
              <v-icon
                size="small"
                icon="mdi-check-circle" />
              {{ item.ackedBy || $t('featherprint.unknownUser') }}
              <small
                v-if="item.ackedAt"
                class="ms-1">{{ fmtTs(item.ackedAt) }}</small>
            </span>
            <span
              v-else
              class="text-muted-more">&mdash;</span>
          </template>
        </arkime-table>
      </div>

      <!-- lookup: a single transient result, so it owns its own layout rather
           than borrowing the tracked list's detail block -->
      <div
        v-else-if="section === 'lookup'"
        class="ms-2 me-2">
        <div v-if="detail">
          <h4 class="mb-2">
            <arkime-session-field
              :field="ipFieldDef"
              expr="ip"
              :value="detail.ip"
              :pull-left="true"
              :session-btn="true" />
            <small
              v-if="lookupResultTs"
              class="text-muted-more ms-2">
              {{ $t('featherprint.lookedUpAt', { ts: fmtTs(lookupResultTs) }) }}
            </small>
          </h4>
          <featherprint-device
            :device="detail"
            :history="lookupHistory" />
        </div>
        <div
          v-else
          class="ms-1 me-1">
          <div class="mb-5 info-area horizontal-center">
            <div>
              <v-icon
                icon="mdi-magnify"
                size="x-large"
                class="text-muted-more" />&nbsp;
              {{ $t('featherprint.enterIpPrompt') }}
            </div>
          </div>
        </div>
      </div>

      <!-- tracked devices: hand-rolled so each row can expand inline the way
           session rows do (arkime-table's info row is imperative DOM, which
           the session-field pivot menus in the detail pane can't use) -->
      <div
        v-else
        class="ms-2 me-2">
        <arkime-paging
          class="mb-1"
          :records-total="ips.length"
          :records-filtered="sortedIps.length"
          :length-default="50"
          @change-paging="changeTrackedPaging" />
        <table class="arkime-table featherprint-tracked-table">
          <thead>
            <tr>
              <th class="featherprint-toggle-col" />
              <th
                v-for="col in trackedColumns"
                :key="col.id"
                class="cursor-pointer text-start"
                @click="sortTrackedBy(col.sort)">
                {{ col.name }}
                <v-icon
                  v-if="trackedSort.field === col.sort"
                  size="x-small"
                  :icon="trackedSort.desc ? 'mdi-chevron-down' : 'mdi-chevron-up'" />
              </th>
            </tr>
          </thead>
          <tbody>
            <template
              v-for="d in pagedIps"
              :key="d.ip">
              <tr>
                <td class="featherprint-toggle-col">
                  <toggle-btn
                    :opened="!!expanded[d.ip]"
                    @toggle="toggleDevice(d.ip)" />
                </td>
                <td>
                  <arkime-session-field
                    :field="ipFieldDef"
                    expr="ip"
                    :value="d.ip"
                    :pull-left="true"
                    :session-btn="true" />
                </td>
                <td>
                  <arkime-session-field
                    v-if="d.mac && d.mac.value"
                    :field="macFieldDef"
                    expr="mac"
                    :value="d.mac.value"
                    :pull-left="true"
                    :session-btn="true" />
                  <span
                    v-else
                    class="text-muted-more">&mdash;</span>
                </td>
                <td>
                  <arkime-session-field
                    v-if="d.names && d.names.length"
                    :field="hostFieldDef"
                    expr="host"
                    :value="d.names[0].name"
                    :pull-left="true"
                    :session-btn="true" />
                  <span
                    v-else
                    class="text-muted-more">&mdash;</span>
                </td>
                <td>
                  <span v-if="d.classification && d.classification !== 'unknown'">
                    {{ d.classification }}
                  </span>
                  <span
                    v-else
                    class="text-muted-more">&mdash;</span>
                </td>
                <td>{{ fmtTs(d.lastSeen) }}</td>
              </tr>
              <tr
                v-if="expanded[d.ip]"
                class="featherprint-detail-row">
                <td :colspan="trackedColumns.length + 1">
                  <featherprint-device
                    :device="d"
                    :history="detailHistory[d.ip]" />
                </td>
              </tr>
            </template>
            <tr v-if="!pagedIps.length">
              <td
                :colspan="trackedColumns.length + 1"
                class="text-danger text-center">
                <v-icon icon="mdi-alert" />&nbsp;{{ noTrackedMsg }}
              </td>
            </tr>
          </tbody>
        </table>
      </div>
    </div>

    <!-- transient failures (list/lookup/ack/tick): bottom alert, dismissed by
         the user rather than timing out, so a failure can't scroll away -->
    <v-snackbar
      :model-value="!!error"
      @update:model-value="(val) => { if (!val) { error = null; } }"
      color="error"
      location="bottom"
      timeout="-1"
      variant="flat">
      {{ error }}
      <template #actions>
        <v-btn
          variant="text"
          icon="$close"
          @click="error = null" />
      </template>
    </v-snackbar>
  </page-layout> <!-- /featherprint content -->
</template>

<script>
import { fetchWrapper } from '@common/fetchWrapper';
import { fmtTs, describeHistory, IP_FIELD, MAC_FIELD, HOST_FIELD, TERTIARY_BTN_STYLE } from './featherprintUtils.js';
import { resolveMessage } from '@common/resolveI18nMessage';
import Focus from '@common/Focus.vue';
import ArkimeTable from '../utils/Table.vue';
import PageLayout from '../utils/PageLayout.vue';
import ArkimeCollapsible from '../utils/CollapsibleWrapper.vue';
import ToggleBtn from '@common/ToggleBtn.vue';
import ArkimePaging from '@common/Pagination.vue';
import FeatherprintDevice from './FeatherprintDevice.vue';

// pull a sortable scalar out of a device/alert record for the given column
function sortValue (item, field) {
  switch (field) {
  case 'mac': return item.mac?.value ?? '';
  case 'name': return item.names?.[0]?.name ?? '';
  default: return item[field] ?? '';
  }
}

export default {
  name: 'Featherprint',
  components: {
    ArkimeTable,
    PageLayout,
    ArkimeCollapsible,
    ToggleBtn,
    ArkimePaging,
    FeatherprintDevice
  },
  directives: { Focus },
  data () {
    return {
      ips: [],
      filter: '',
      detail: null,          // lookup only: one transient result
      lookupHistory: [],     // lookup: persisted history for that ip, if any
      expanded: {},          // tracked: ip -> row expanded
      detailHistory: {},     // tracked: ip -> history rows, fetched on expand
      alerts: [],
      alertFilter: 'open', // 'open' | 'acked' | 'all'
      alertSearch: '',
      error: null,
      refreshInterval: '0', // ms between auto refreshes; '0' = never
      ackBusy: null,
      timer: null,
      lookupIp: '',
      lookupRange: 3600,
      lookupCustomRange: null, // { startSec, stopSec } when arriving from a value-action URL
      lookupBusy: false,
      lookupResultTs: null,
      monitorState: null,
      trackedSort: { field: 'lastSeen', desc: true },
      trackedPaging: { start: 0, length: 50 },
      alertSort: { field: 'ts', desc: true },
    };
  },
  computed: {
    ipFieldDef: () => IP_FIELD,
    macFieldDef: () => MAC_FIELD,
    hostFieldDef: () => HOST_FIELD,
    tertiaryBtnStyle: () => TERTIARY_BTN_STYLE,
    shiftKeyHold () {
      return this.$store.state.shiftKeyHold;
    },
    // same store binding as files/history/stats -- without it v-focus is inert
    // and the shift-key "Q" badge advertises a shortcut that does nothing
    focusInput: {
      get () { return this.$store.state.focusSearch; },
      set (v) { this.$store.commit('setFocusSearch', v); }
    },
    section () {
      const n = this.$route?.name || '';
      if (n === 'FeatherprintAlerts') return 'alerts';
      if (n === 'FeatherprintLookup') return 'lookup';
      return 'tracked';
    },
    trackedColumns () {
      return [
        { id: 'ip', name: this.$t('featherprint.colIp'), sort: 'ip' },
        { id: 'mac', name: this.$t('featherprint.colMac'), sort: 'mac' },
        { id: 'name', name: this.$t('featherprint.colName'), sort: 'name' },
        { id: 'classification', name: this.$t('featherprint.colClassification'), sort: 'classification' },
        { id: 'lastSeen', name: this.$t('featherprint.colLastSeen'), sort: 'lastSeen' }
      ];
    },
    alertColumns () {
      return [
        {
          id: 'ts',
          name: this.$t('featherprint.colTime'),
          classes: 'text-start',
          sort: 'ts',
          default: true,
          width: 200
        },
        { id: 'kind', name: this.$t('featherprint.colKind'), classes: 'text-start', sort: 'kind', default: true, width: 170 },
        { id: 'ip', name: this.$t('featherprint.colIp'), classes: 'text-start', sort: 'ip', default: true, width: 190 },
        { id: 'details', name: this.$t('featherprint.colDetails'), classes: 'text-start', default: true, width: 420 },
        { id: 'acked', name: this.$t('featherprint.colAcked'), classes: 'text-start', sort: 'acked', default: true, width: 250 }
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
        if (a.details && a.details.toLowerCase().includes(q)) return true;
        return false;
      });
    },
    sortedAlerts () {
      return this.sortList(this.filteredAlerts, this.alertSort);
    },
    noAlertsMsg () {
      if (this.alertFilter === 'open') return this.$t('featherprint.noOpenAlerts');
      if (this.alertFilter === 'acked') return this.$t('featherprint.noAckedAlerts');
      return this.$t('featherprint.noAlerts');
    },
    noTrackedMsg () {
      return this.ips.length ? this.$t('featherprint.noMatches') : this.$t('featherprint.noTrackedIps');
    },
    openAlertCount () {
      return this.alerts.filter(a => !a.acked).length;
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
    },
    sortedIps () {
      return this.sortList(this.filteredIps, this.trackedSort);
    },
    pagedIps () {
      const { start, length: size } = this.trackedPaging;
      return this.sortedIps.slice(start, start + size);
    }
  },
  watch: {
    refreshInterval (ms) {
      this.stopTimer();
      const n = Number(ms);
      if (n > 0) this.timer = setInterval(this.refreshAll, n);
    },
    filter () {
      this.trackedPaging = { ...this.trackedPaging, start: 0 };
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
    onOffFocus () {
      this.focusInput = false;
    },
    /* header click toggles direction; the list is already local */
    changeTrackedPaging (v) {
      this.trackedPaging = { start: v.start, length: v.length };
    },
    sortTrackedBy (field) {
      if (!field) { return; }
      this.trackedSort = this.trackedSort.field === field
        ? { field, desc: !this.trackedSort.desc }
        : { field, desc: true };
    },
    /* The tracked list already carries the full device record, so expanding a
       row only needs its change history. Fetched once and kept. */
    async expandDevice (ip) {
      this.expanded[ip] = true;
      if (this.detailHistory[ip]) { return; }
      this.detailHistory[ip] = [];
      try {
        const h = await fetchWrapper({ url: `api/featherprint/history/${encodeURIComponent(ip)}?limit=50` });
        this.detailHistory[ip] = h?.history || [];
      } catch (err) {
        this.detailHistory[ip] = [];
        this.error = this.$t('featherprint.errorLoadHistory', { ip, reason: resolveMessage(err, this.$t) });
      }
    },
    toggleDevice (ip) {
      if (this.expanded[ip]) { delete this.expanded[ip]; return; }
      this.expandDevice(ip);
    },
    loadAlertTable (sortField, desc) {
      if (sortField) this.alertSort = { field: sortField, desc: !!desc };
    },
    sortList (list, { field, desc }) {
      const dir = desc ? -1 : 1;
      return [...list].sort((a, b) => {
        const av = sortValue(a, field);
        const bv = sortValue(b, field);
        if (av === bv) return 0;
        return (av > bv ? 1 : -1) * dir;
      });
    },
    describeHistory,
    fmtTs (ts) {
      return fmtTs(ts, this.$store?.state?.user?.settings);
    },
    refreshAll () {
      this.loadIps();
      this.loadAlerts();
      this.loadState();
    },
    async goToInfoForIp (ip) {
      await this.$router.push({ name: 'FeatherprintTracked' });
      this.expandDevice(ip);
    },
    goToSection (section) {
      if (!section || section === this.section) return;
      const routeName =
        section === 'lookup' ? 'FeatherprintLookup' :
          section === 'alerts' ? 'FeatherprintAlerts' :
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
        this.error = null;
      } catch (err) {
        this.error = this.$t('featherprint.errorListIps', { reason: resolveMessage(err, this.$t) });
      }
    },
    async loadAlerts () {
      try {
        const r = await fetchWrapper({ url: 'api/featherprint/alerts?limit=500' });
        // precompute the details string and an `id` here: the search filter and
        // the cell both need details, and arkime-table keys on item.id (alerts
        // carry _id, so without this every row falls back to an index key)
        this.alerts = (r.alerts || []).map(a => ({ ...a, id: a._id, details: describeHistory(a) }));
        this.error = null;
      } catch (err) {
        this.error = this.$t('featherprint.errorLoadAlerts', { reason: resolveMessage(err, this.$t) });
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
        this.error = this.$t('featherprint.errorAckFailed', { reason: resolveMessage(err, this.$t) });
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
        this.lookupHistory = [];
        this.lookupResultTs = Date.now();
        if (!this.detail) {
          this.error = this.$t('featherprint.errorNoSignals', { ip });
          return;
        }
        // History is keyed by ip and written only by the monitor, so it is
        // independent of this transient scan: a monitored ip has real rows
        // here, anything else correctly comes back empty.
        try {
          const h = await fetchWrapper({ url: `api/featherprint/history/${encodeURIComponent(ip)}?limit=50` });
          this.lookupHistory = h?.history || [];
        } catch (err) {
          this.lookupHistory = [];
          this.error = this.$t('featherprint.errorLoadHistory', { ip, reason: resolveMessage(err, this.$t) });
        }
      } catch (err) {
        this.detail = null;
        this.error = this.$t('featherprint.errorLookupFailed', { ip, reason: resolveMessage(err, this.$t) });
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
    }
  }
};
</script>

<style scoped>

/* search sub-navbar: secondary-lightest to match the search band on
   sessions/spiview/arkime and the filter bands on files/history/stats
   (the tab strip below is the quaternary-lightest sub-sub navbar) */
.featherprint-form {
  z-index: 6;
  background-color: rgb(var(--v-theme-secondary-lightest));
  /* reserve a constant height so the tab strip below doesn't shift as the
     toolbar swaps controls between tabs (alerts adds a select, lookup a
     full input + select + button) */
  min-height: 52px;
}

/* expanded detail row: tinted like the session detail row so it reads as
   belonging to the row above rather than as another data row */
.featherprint-tracked-table tbody tr.featherprint-detail-row {
  background-color: rgb(var(--v-theme-quaternary-lightest)) !important;
}
.featherprint-tracked-table tbody tr:not(.featherprint-detail-row):hover td {
  background-color: rgb(var(--v-theme-tertiary-lightest));
}
/* leftmost toggle column is always the same narrow width */
.featherprint-tracked-table .featherprint-toggle-col {
  width: 24px;
}

/* .info-area defaults to --px-xxxlg; knock it down a notch like Hunt does */
.info-area {
  font-size: var(--px-xxlg);
}

</style>
