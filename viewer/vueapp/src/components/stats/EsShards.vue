<!--
Copyright Yahoo Inc.
SPDX-License-Identifier: Apache-2.0
-->
<template>
  <div class="arkime-container-fluid mt-3">
    <arkime-loading v-if="initialLoading && !error" />

    <v-alert
      v-if="error"
      type="warning"
      variant="tonal"
      density="compact"
      style="z-index: 2000;"
      class="position-fixed fixed-bottom m-0 rounded-0">
      {{ error }}
    </v-alert>

    <div>
      <div
        v-if="stats.indices && !stats.indices.length"
        class="text-center">
        <h3>
          <v-icon
            icon="mdi-folder-open"
            size="large"
            class="text-medium-emphasis" />
        </h3>
        <h5 class="lead">
          {{ $t( cluster ? 'stats.esShards.noResultsCluster' : 'stats.esShards.noResults' ) }}
        </h5>
      </div>

      <table
        v-if="stats.indices && stats.indices.length"
        class="arkime-table small block-table mt-1">
        <thead>
          <tr>
            <th />
            <th
              v-for="column in columns"
              :key="column.name"
              class="hover-menu"
              :width="column.width">
              <div>
                <!-- column dropdown menu -->
                <span
                  v-if="column.hasDropdown"
                  v-has-role="{user:user,roles:'arkimeAdmin,dbAdmin'}">
                  <v-menu location="bottom end">
                    <template #activator="{ props: activatorProps }">
                      <v-btn
                        v-bind="activatorProps"
                        variant="outlined"
                        size="x-small"
                        density="comfortable"
                        icon
                        class="column-actions-btn float-right mb-1">
                        <v-icon icon="mdi-menu-down" />
                      </v-btn>
                    </template>
                    <v-list density="compact">
                      <v-list-item
                        v-if="!column.nodeExcluded"
                        @click="exclude('name', column)">
                        {{ $t('stats.excludeNode') }}: {{ column.name }}
                      </v-list-item>
                      <v-list-item
                        v-if="column.nodeExcluded"
                        @click="include('name', column)">
                        {{ $t('stats.includeNode') }}: {{ column.name }}
                      </v-list-item>
                      <v-list-item
                        v-if="!column.ipExcluded"
                        @click="exclude('ip', column)">
                        {{ $t('stats.excludeIp') }}: {{ column.ip }}
                      </v-list-item>
                      <v-list-item
                        v-if="column.ipExcluded"
                        @click="include('ip', column)">
                        {{ $t('stats.includeIp') }}: {{ column.ip }}
                      </v-list-item>
                    </v-list>
                  </v-menu>
                </span> <!-- /column dropdown menu -->
                <div
                  class="header-text"
                  :class="{'cursor-pointer':column.sort !== undefined}"
                  @click="columnClick(column.sort)">
                  {{ column.name }}
                  <span v-if="column.sort !== undefined">
                    <v-icon
                      icon="mdi-chevron-up"
                      v-show="query.sortField === column.sort && !query.desc" />
                    <v-icon
                      icon="mdi-chevron-down"
                      v-show="query.sortField === column.sort && query.desc" />
                  </span>
                </div>
              </div>
            </th>
          </tr>
        </thead>
        <tbody>
          <tr
            v-for="(stat, index) in stats.indices"
            :key="stat.name">
            <td>
              <span
                v-has-role="{user:user,roles:'arkimeAdmin,dbAdmin'}"
                v-if="stat.nodes && stat.nodes.Unassigned && stat.nodes.Unassigned.length">
                <transition name="buttons">
                  <v-btn
                    v-if="!stat.confirmDelete"
                    color="error"
                    variant="flat"
                    size="small"
                    density="comfortable"
                    icon
                    :id="`deleteUnassignedShards${index}`"
                    @click="deleteUnassignedShards(stat, index)">
                    <v-icon icon="mdi-delete" />
                    <v-tooltip
                      :activator="`[id='deleteUnassignedShards${index}']`"
                      location="right">
                      {{ $t('stats.esShards.deleteUnassignedTip') }}
                    </v-tooltip>
                  </v-btn>
                  <v-btn
                    v-else
                    color="warning"
                    variant="flat"
                    size="small"
                    density="comfortable"
                    icon
                    :id="`confirmDeleteUnassignedShards${index}`"
                    @click="confirmDeleteUnassignedShards(stat, index)">
                    <v-icon icon="mdi-check" />
                    <v-tooltip
                      :activator="`[id='confirmDeleteUnassignedShards${index}']`"
                      location="right">
                      {{ $t('stats.esShards.confirmDeleteUnassignedTip') }}
                    </v-tooltip>
                  </v-btn>
                </transition>
              </span>
            </td>
            <td>
              {{ stat.name }}
            </td>
            <td
              v-for="node in nodes"
              :key="node">
              <template v-if="stat.nodes[node]">
                <template
                  v-for="item in stat.nodes[node]"
                  :key="node + '-' + stat.name + '-' + item.shard + '-shard'">
                  <span
                    class="shard-badge cursor-help"
                    :class="{'shard-badge--primary':item.prirep === 'p', 'shard-badge--notstarted':item.state !== 'STARTED','render-tooltip-bottom':index < 5}"
                    :id="node + '-' + stat.name + '-' + item.shard + '-btn'"
                    @mouseenter="showDetails(item, stat.name)"
                    @mouseleave="hideDetails(item)">
                    {{ item.shard }}
                    <span
                      v-if="item.showDetails"
                      class="shard-detail"
                      :class="{'shard-detail-interactive': node === 'Unassigned' && user.esAdminUser}"
                      @mouseenter="keepTooltipOpen(item)"
                      @mouseleave="scheduleTooltipClose(item)">
                      <dl class="dl-horizontal">
                        <dt>{{ $t('stats.esShards.table-name') }}</dt>
                        <dd>{{ stat.name }}</dd>
                        <dt>{{ $t('stats.esShards.table-node') }}</dt>
                        <dd>{{ node }}</dd>
                        <template v-if="item.oldNode">
                          <dt>{{ $t('stats.esShards.table-oldNode') }}</dt>
                          <dd>{{ item.oldNode }}</dd>
                        </template>
                        <template v-if="item.ip">
                          <dt>{{ $t('stats.esShards.table-ip') }}</dt>
                          <dd>{{ item.ip }}</dd>
                        </template>
                        <dt>{{ $t('stats.esShards.table-shard') }}</dt>
                        <dd>{{ item.shard }}</dd>
                        <dt>{{ $t('stats.esShards.table-state') }}</dt>
                        <dd>{{ item.state }}</dd>
                        <template v-if="item.uf">
                          <dt>{{ $t('stats.esShards.table-uf') }}</dt>
                          <dd>{{ item.uf }}</dd>
                        </template>
                        <template v-if="item.ur">
                          <dt>{{ $t('stats.esShards.table-ur') }}</dt>
                          <dd>{{ item.ur }}</dd>
                        </template>
                        <template v-if="item.store">
                          <dt>{{ $t('stats.esShards.table-store') }}</dt>
                          <dd>{{ humanReadableBytes(item.store) }}</dd>
                        </template>
                        <template v-if="item.docs">
                          <dt>{{ $t('stats.esShards.table-docs') }}</dt>
                          <dd>{{ roundCommaString(item.docs) }}</dd>
                        </template>
                        <template v-if="item.fm">
                          <dt>{{ $t('stats.esShards.table-fm') }}</dt>
                          <dd>{{ humanReadableBytes(item.fm) }}</dd>
                        </template>
                        <template v-if="item.sm">
                          <dt>{{ $t('stats.esShards.table-sm') }}</dt>
                          <dd>{{ humanReadableBytes(item.sm) }}</dd>
                        </template>
                        <dt>{{ $t('stats.esShards.table-prirep') }}</dt>
                        <template v-if="item.prirep === 'p'">
                          <dd>{{ $t('stats.esShards.table-prirep-p') }}</dd>
                        </template>
                        <template v-else>
                          <dd>{{ $t('stats.esShards.table-prirep-else') }}</dd>
                        </template>
                      </dl>
                      <!-- Explain allocation button for unassigned shards (ES Admin only) -->
                      <div
                        v-if="node === 'Unassigned' && user.esAdminUser"
                        class="mt-2 pt-2"
                        style="border-top: 1px solid #555;">
                        <v-btn
                          variant="flat"
                          size="x-small"
                          density="comfortable"
                          block
                          :style="primaryBtnStyle"
                          :disabled="loadingAllocationExplain"
                          @click="openAllocationModal(item, stat.name)">
                          {{ $t('stats.esShards.explainAllocation') }}
                        </v-btn>
                      </div>
                    </span>
                  </span>
                </template>
              </template>
            </td>
          </tr>
        </tbody>
      </table>
    </div>

    <!-- Allocation Explain Modal -->
    <v-dialog
      v-model="showAllocationModal"
      max-width="1140">
      <v-card density="compact">
        <v-card-title class="bg-dark text-white">
          {{ allocationModalTitle }}
        </v-card-title>
        <v-card-text class="p-0">
          <div
            v-if="loadingAllocationExplain"
            class="text-center p-4">
            <v-icon
              icon="mdi-loading"
              size="large"
              class="mdi-spin" />
            <p class="mt-2">
              {{ $t('common.loading') }}
            </p>
          </div>
          <div
            v-else-if="allocationExplainData"
            class="allocation-explain-modal">
            <pre class="mb-0">{{ JSON.stringify(allocationExplainData, null, 2) }}</pre>
          </div>
          <v-alert
            v-else-if="allocationExplainError"
            type="error"
            variant="tonal"
            density="compact"
            class="m-3">
            {{ allocationExplainError }}
          </v-alert>
        </v-card-text>
        <v-card-actions>
          <v-btn
            variant="flat"
            size="small"
            density="comfortable"
            :style="primaryBtnStyle"
            @click="showAllocationModal = false">
            {{ $t('common.close') }}
          </v-btn>
        </v-card-actions>
      </v-card>
    </v-dialog>
  </div>
</template>

<script>
import Utils from '../utils/utils';
import ArkimeLoading from '../utils/Loading.vue';
import StatsService from './StatsService';
import { roundCommaString, humanReadableBytes } from '@common/vueFilters.js';
import { resolveMessage } from '@common/resolveI18nMessage';

let reqPromise; // promise returned from setInterval for recurring requests
let respondedAt; // the time that the last data load successfully responded

export default {
  name: 'EsShards',
  components: {
    ArkimeLoading
  },
  props: {
    dataInterval: {
      type: Number,
      default: 5000
    },
    shardsShow: {
      type: String,
      default: 'all'
    },
    refreshData: {
      type: Boolean,
      default: false
    },
    searchTerm: {
      type: String,
      default: ''
    },
    cluster: {
      type: String,
      default: ''
    }
  },
  data: function () {
    return {
      initialLoading: true,
      error: '',
      stats: {},
      nodes: {},
      query: {
        filter: this.searchTerm || undefined,
        sortField: 'index',
        desc: false,
        show: this.shardsShow || 'notstarted',
        cluster: this.cluster || undefined
      },
      columns: [
        { name: this.$t('stats.esShards.index'), sort: 'index', doClick: false, hasDropdown: false, width: '200px' }
      ],
      showAllocationModal: false,
      loadingAllocationExplain: false,
      allocationExplainData: null,
      allocationExplainError: '',
      allocationModalTitle: '',
      // Arkime theme-color v-btn style. Vuetify :color can't take CSS vars.
      primaryBtnStyle: {
        backgroundColor: 'rgb(var(--v-theme-primary))',
        color: 'rgb(var(--v-theme-button-fg))'
      }
    };
  },
  computed: {
    loading: {
      get: function () {
        return this.$store.state.loadingData;
      },
      set: function (newValue) {
        this.$store.commit('setLoadingData', newValue);
      }
    },
    user () {
      return this.$store.state.user;
    }
  },
  watch: {
    dataInterval: function () {
      if (reqPromise) { // cancel the interval and reset it if necessary
        clearInterval(reqPromise);
        reqPromise = null;

        if (this.dataInterval === '0') { return; }

        this.setRequestInterval();
      } else if (this.dataInterval !== '0') {
        this.loadData();
        this.setRequestInterval();
      }
    },
    shardsShow: function () {
      this.query.show = this.shardsShow;
      this.loadData();
    },
    refreshData: function () {
      if (this.refreshData) {
        this.loadData();
      }
    },
    cluster: function () {
      this.query.cluster = this.cluster;
      this.loadData();
    }
  },
  created: function () {
    this.loadData();
    if (this.dataInterval !== '0') {
      this.setRequestInterval();
    }
  },
  methods: {
    roundCommaString,
    humanReadableBytes,
    /* exposed page functions ------------------------------------ */
    columnClick (colName) {
      if (!colName) { return; }

      this.query.sortField = colName;
      this.query.desc = !this.query.desc;
      this.loadData();
    },
    deleteUnassignedShards (shard, index) {
      shard.confirmDelete = true;
    },
    async confirmDeleteUnassignedShards (shard, index) {
      let count = shard.nodes.Unassigned.length;

      const sent = {};
      for (const node of shard.nodes.Unassigned) { // delete each shard
        if (sent[node.shard]) { // don't send the same shard twice
          count--;
          continue;
        }
        sent[node.shard] = true;

        try {
          await StatsService.deleteShard(shard.name, node.shard, { cluster: this.query.cluster });
          count--;
        } catch (error) {
          this.error = resolveMessage(error, this.$t);
        }
      }

      if (count === 0) { // all shards have been deleted
        this.stats.indices.splice(index, 1);
      }

      shard.confirmDelete = false; // reset the confirmDelete flag
    },
    async exclude (type, column) {
      if (!Utils.checkClusterSelection(this.query.cluster, this.$store.state.esCluster.availableCluster.active, this).valid) {
        return;
      }

      try {
        await StatsService.excludeShard(type, column[type], { cluster: this.query.cluster });
        if (type === 'name') {
          column.nodeExcluded = true;
        } else {
          column.ipExcluded = true;
        }
      } catch (error) {
        this.error = resolveMessage(error, this.$t);
      }
    },
    async include (type, column) {
      if (!Utils.checkClusterSelection(this.query.cluster, this.$store.state.esCluster.availableCluster.active, this).valid) {
        return;
      }

      try {
        await StatsService.includeShard(type, column[type], { cluster: this.query.cluster });
        if (type === 'name') {
          column.nodeExcluded = false;
        } else {
          column.ipExcluded = false;
        }
      } catch (error) {
        this.error = resolveMessage(error, this.$t);
      }
    },
    showDetails: function (item, indexName) {
      item.showDetails = true;
      if (item.tooltipCloseTimeout) {
        clearTimeout(item.tooltipCloseTimeout);
        item.tooltipCloseTimeout = null;
      }
    },
    hideDetails: function (item) {
      // Only hide if we're not hovering over the tooltip itself
      if (!item.keepOpen) {
        item.showDetails = false;
      }
    },
    keepTooltipOpen: function (item) {
      item.keepOpen = true;
      if (item.tooltipCloseTimeout) {
        clearTimeout(item.tooltipCloseTimeout);
        item.tooltipCloseTimeout = null;
      }
    },
    scheduleTooltipClose: function (item) {
      item.keepOpen = false;
      // Delay closing to allow moving mouse from badge to tooltip
      item.tooltipCloseTimeout = setTimeout(() => {
        if (!item.keepOpen) {
          item.showDetails = false;
        }
      }, 400);
    },
    async openAllocationModal (item, indexName) {
      if (!Utils.checkClusterSelection(this.query.cluster, this.$store.state.esCluster.availableCluster.active, this).valid) {
        return;
      }

      // Set modal title
      this.allocationModalTitle = this.$t('stats.esShards.allocationExplainTitle', {
        index: indexName,
        shard: item.shard,
        type: item.prirep === 'p' ? 'Primary' : 'Replica'
      });

      // Open modal and start loading
      this.showAllocationModal = true;
      this.loadingAllocationExplain = true;
      this.allocationExplainData = null;
      this.allocationExplainError = '';

      try {
        const params = {
          cluster: this.query.cluster,
          index: indexName,
          shard: item.shard,
          primary: item.prirep === 'p'
        };
        const response = await StatsService.getAllocationExplain(params);
        this.allocationExplainData = response;
        this.loadingAllocationExplain = false;
      } catch (error) {
        this.loadingAllocationExplain = false;
        this.allocationExplainError = resolveMessage(error, this.$t);
      }
    },
    /* helper functions ------------------------------------------ */
    setRequestInterval: function () {
      reqPromise = setInterval(() => {
        if (respondedAt && Date.now() - respondedAt >= parseInt(this.dataInterval)) {
          this.loadData();
        }
      }, 500);
    },
    async loadData () {
      if (!Utils.checkClusterSelection(this.query.cluster, this.$store.state.esCluster.availableCluster.active, this).valid) {
        return;
      }

      this.loading = true;
      respondedAt = undefined;

      this.query.filter = this.searchTerm;

      try {
        const response = await StatsService.getShards(this.query);
        respondedAt = Date.now();
        this.error = '';
        this.loading = false;
        this.initialLoading = false;
        this.stats = response;

        this.columns.splice(1);

        this.nodes = Object.keys(response.nodes).sort(function (a, b) {
          return a.localeCompare(b);
        });

        for (const node of this.nodes) {
          if (node === 'Unassigned') {
            this.columns.push({ name: this.$t('stats.esShards.unassigned'), doClick: false, hasDropdown: false });
          } else {
            this.columns.push({
              name: node,
              doClick: (node.indexOf('->') === -1),
              ip: response.nodes[node].ip,
              ipExcluded: response.nodes[node].ipExcluded,
              nodeExcluded: response.nodes[node].nodeExcluded,
              hasDropdown: true
            });
          }
        }
      } catch (error) {
        respondedAt = undefined;
        this.loading = false;
        this.initialLoading = false;
        this.error = resolveMessage(error, this.$t);
      }
    }
  },
  beforeUnmount () {
    if (reqPromise) {
      clearInterval(reqPromise);
      reqPromise = null;
    }
  }
};
</script>

<style>
table .hover-menu .column-actions-btn {
  padding: 1px 4px;
  font-size: 13px;
  line-height: 1.2;
  visibility: hidden;
}
table .hover-menu:hover .column-actions-btn {
  visibility: visible;
}
</style>

<style scoped>
table.table.block-table {
  display: block;
}

table > thead > tr > th {
  border-top: none;
}

table.table .hover-menu {
  vertical-align: top;
  min-width: 100px;
}

table.table .hover-menu .btn-group {
  visibility: hidden;
  margin-left: -20px !important;
  position: relative;
  top: 2px;
  right: 2px;
  margin-top: -2px;
}

table.table .hover-menu .header-text {
  display: inline-block;
  word-break: break-word;
}

table.table tbody > tr > td:first-child {
  width:1px;
  white-space:nowrap;
  padding-right: .5rem;
}

.shard-badge {
  display: inline-block;
  padding: .1em .4em;
  font-weight: 500;
  font-size: 14px;
  white-space: normal;
  color: rgb(var(--v-theme-button-fg));
  background-color: rgb(var(--v-theme-neutral));
  border-radius: 0.375rem;
  border: 2px dotted rgb(var(--v-theme-neutral-dark));
}
.shard-badge.shard-badge--primary {
  font-weight: bold;
  background-color: rgb(var(--v-theme-primary));
  border: 2px dotted rgb(var(--v-theme-primary));
}
.shard-badge.shard-badge--notstarted {
  border: 2px dotted rgb(var(--v-theme-quaternary));
}
.shard-badge:hover {
  position: relative;
}
.shard-badge > span {
  display: none;
}
.shard-badge:hover > span.shard-detail {
  z-index: 2;
  display: block;
}
.shard-badge > span:before {
  content: '';
  display: block;
  width: 0;
  height: 0;
  position: absolute;
  border-top: 8px solid transparent;
  border-bottom: 8px solid transparent;
  border-left: 8px solid black;
  right: -8px;
  bottom: 7px;
}
.shard-badge > span.shard-detail {
  font-weight: normal;
  position: absolute;
  margin: 10px;
  bottom: -14px;
  right: 20px;
  padding: 4px 6px;
  color: white;
  background-color: black;
  border-radius: 5px;
  font-size: 85%;
  line-height: 1.2;
  max-width: 210px;
}
/* Interactive tooltip for unassigned shards */
.shard-badge > span.shard-detail-interactive {
  pointer-events: auto;
  cursor: default;
}
/* Allocation explain modal styles */
.allocation-explain-modal {
  max-height: 70vh;
  overflow-y: auto;
  overflow-x: auto;
  background-color: rgb(var(--v-theme-background));
  padding: 1rem;
}
.allocation-explain-modal pre {
  font-size: 12px;
  line-height: 1.4;
  color: rgb(var(--v-theme-foreground));
  white-space: pre;
  font-family: 'Courier New', monospace;
}
.shard-badge > span.shard-detail dl {
  margin-bottom: 0;
}
.shard-badge > span.shard-detail dt {
  width: 85px;
}
.shard-badge > span.shard-detail dd {
  margin-left: 90px;
  text-align: left;
  overflow-wrap: break-word;
}

.shard-badge.render-tooltip-bottom:hover > span {
  bottom: -120px;
}
.shard-badge.render-tooltip-bottom:hover > span:before {
  bottom: 113px;
}
</style>
