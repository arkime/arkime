<!--
Copyright Yahoo Inc.
SPDX-License-Identifier: Apache-2.0
-->
<template>

  <div class="container-fluid mt-3">

    <arkime-loading v-if="initialLoading && !error">
    </arkime-loading>

    <div v-if="error"
      class="alert alert-warning position-fixed fixed-bottom m-0 rounded-0"
      style="z-index: 2000;">
      {{ error }}
    </div>

    <div>

      <div v-if="stats.indices && !stats.indices.length"
        class="text-center">
        <h3>
          <span class="fa fa-folder-open fa-2x text-muted" />
        </h3>
        <h5 class="lead">
          No data. You may need to adjust the "Show" pulldown
          <template v-if="cluster">
            <br>
            Or try selecting a different cluster.
          </template>
        </h5>
      </div>

      <table v-if="stats.indices && stats.indices.length"
        class="table table-sm table-hover small block-table mt-1">
        <thead>
          <tr>
            <th></th>
            <th v-for="column in columns"
              :key="column.name"
              class="hover-menu"
              :width="column.width">
              <div>
                <!-- column dropdown menu -->
                <b-dropdown right
                  size="sm"
                  v-if="column.hasDropdown"
                  class="column-actions-btn pull-right mb-1"
                  v-has-role="{user:user,roles:'arkimeAdmin'}">
                  <b-dropdown-item v-if="!column.nodeExcluded"
                    @click="exclude('name', column)">
                    Exclude node {{ column.name }}
                  </b-dropdown-item>
                  <b-dropdown-item v-if="column.nodeExcluded"
                    @click="include('name', column)">
                    Include node {{ column.name }}
                  </b-dropdown-item>
                  <b-dropdown-item v-if="!column.ipExcluded"
                    @click="exclude('ip', column)">
                    Exclude IP {{ column.ip }}
                  </b-dropdown-item>
                  <b-dropdown-item v-if="column.ipExcluded"
                    @click="include('ip', column)">
                    Include IP {{ column.ip }}
                  </b-dropdown-item>
                </b-dropdown> <!-- /column dropdown menu -->
                <div class="header-text"
                  :class="{'cursor-pointer':column.sort !== undefined}"
                  @click="columnClick(column.sort)">
                  {{ column.name }}
                  <span v-if="column.sort !== undefined">
                    <span v-show="query.sortField === column.sort && !query.desc" class="fa fa-sort-asc"></span>
                    <span v-show="query.sortField === column.sort && query.desc" class="fa fa-sort-desc"></span>
                    <span v-show="query.sortField !== column.sort" class="fa fa-sort"></span>
                  </span>
                </div>
              </div>
            </th>
          </tr>
        </thead>
        <tbody>
          <tr v-for="(stat, index) in stats.indices"
            :key="stat.name">
            <td>
              <span v-has-role="{user:user,roles:'arkimeAdmin'}" v-if="stat.nodes && stat.nodes.Unassigned && stat.nodes.Unassigned.length">
                <transition name="buttons">
                  <BButton
                    v-if="!stat.confirmDelete"
                    size="xs"
                    variant="danger"
                    :id="`deleteUnassignedShards${index}`"
                    @click="deleteUnassignedShards(stat, index)">
                    <span class="fa fa-trash fa-fw" />
                    <BTooltip :target="`deleteUnassignedShards${index}`">Delete Unassigned Shards</BTooltip>
                  </BButton>
                  <BButton
                    v-else
                    size="xs"
                    variant="warning"
                    :id="`confirmDeleteUnassignedShards${index}`"
                    @click="confirmDeleteUnassignedShards(stat, index)">
                    <span class="fa fa-check fa-fw" />
                    <BTooltip :target="`confirmDeleteUnassignedShards${index}`">Confirm delete of all unassigned shards</BTooltip>
                  </BButton>
                </transition>
              </span>
            </td>
            <td>
              {{ stat.name }}
            </td>
            <td v-for="node in nodes"
              :key="node">
              <template v-if="stat.nodes[node]">
                <template v-for="item in stat.nodes[node]"
                  :key="node + '-' + stat.name + '-' + item.shard + '-shard'">
                  <span class="badge badge-pill bg-secondary cursor-help"
                    :class="{'bg-primary':item.prirep === 'p', 'badge-notstarted':item.state !== 'STARTED','render-tooltip-bottom':index < 5}"
                    :id="node + '-' + stat.name + '-' + item.shard + '-btn'"
                    @mouseenter="showDetails(item)"
                    @mouseleave="hideDetails(item)">
                    {{ item.shard }}
                    <span v-if="item.showDetails"
                      class="shard-detail"
                      @mouseenter="hideDetails(item)">
                      <dl class="dl-horizontal">
                        <dt>Index</dt>
                        <dd>{{ stat.name }}</dd>
                        <dt>Node</dt>
                        <dd>{{ node }}</dd>
                        <template v-if="item.ip">
                          <dt>IP</dt>
                          <dd>{{ item.ip }}</dd>
                        </template>
                        <dt>Shard</dt>
                        <dd>{{ item.shard }}</dd>
                        <dt>State</dt>
                        <dd>{{ item.state }}</dd>
                        <template v-if="item.ur">
                          <dt>Reason</dt>
                          <dd>{{ item.uf }}</dd>
                        </template>
                        <template v-if="item.uf">
                          <dt>For</dt>
                          <dd>{{ item.ur }}</dd>
                        </template>
                        <template v-if="item.store">
                          <dt>Size</dt>
                          <dd>{{ humanReadableBytes(item.store) }}</dd>
                        </template>
                        <template v-if="item.docs">
                          <dt>Documents</dt>
                          <dd>{{ roundCommaString(item.docs) }}</dd>
                        </template>
                        <template v-if="item.fm">
                          <dt>Field Mem</dt>
                          <dd>{{ humanReadableBytes(item.fm) }}</dd>
                        </template>
                        <template v-if="item.sm">
                          <dt>Segment Mem</dt>
                          <dd>{{ humanReadableBytes(item.sm) }}</dd>
                        </template>
                        <dt>Shard Type</dt>
                        <template v-if="item.prirep === 'p'">
                          <dd>primary</dd>
                        </template>
                        <template v-else>
                          <dd>replicate</dd>
                        </template>
                      </dl>
                    </span>
                  </span>
                </template>
              </template>
            </td>
          </tr>
        </tbody>
      </table>

    </div>

  </div>

</template>

<script>
import Utils from '../utils/utils';
import ArkimeLoading from '../utils/Loading.vue';
import StatsService from './StatsService';
import { roundCommaString, humanReadableBytes } from '@real_common/vueFilters.js';

let reqPromise; // promise returned from setInterval for recurring requests
let respondedAt; // the time that the last data load successfully responded

export default {
  name: 'EsShards',
  components: {
    ArkimeLoading
  },
  props: [
    'dataInterval',
    'shardsShow',
    'refreshData',
    'searchTerm',
    'cluster'
  ],
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
        { name: 'Index', sort: 'index', doClick: false, hasDropdown: false, width: '200px' }
      ]
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
      this.$set(shard, 'confirmDelete', true);
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
          console.log('ERROR UR MOM', error); // TODO ECR
          this.error = error.text || error;
        }
      }

      if (count === 0) { // all shards have been deleted
        this.stats.indices.splice(index, 1);
      }

      this.$set(shard, 'confirmDelete', false); // reset the confirmDelete flag
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
        this.error = error.text || error;
      }
    },
    async include (type, column) {
      if (!Utils.checkClusterSelection(this.query.cluster, this.$store.state.esCluster.availableCluster.active, this).valid) {
        return;
      }

      try {
        await StatsService.includeShard(type, column[type], { cluster: this.query.cluster });
        if (type === 'name') {
          column.nodeExcluded = true;
        } else {
          column.ipExcluded = true;
        }
      } catch (error) {
        this.error = error.text || error;
      }
    },
    showDetails: function (item) {
      this.$set(item, 'showDetails', true);
    },
    hideDetails: function (item) {
      this.$set(item, 'showDetails', false);
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
            this.columns.push({ name: node, doClick: false, hasDropdown: false });
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
        this.error = error.text || error;
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
table .hover-menu > div > .btn-group.column-actions-btn > .btn-sm {
  padding: 1px 4px;
  font-size: 13px;
  line-height: 1.2;
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
table.table .hover-menu:hover .btn-group {
  visibility: visible;
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

.badge {
  padding: .1em .4em;
  font-weight: 500;
  font-size: 14px;
  white-space: normal;
}
.badge.bg-primary {
  font-weight: bold;
  background-color: var(--color-primary);
}
.badge:hover {
  position: relative;
}
.badge > span {
  display: none;
}
.badge:hover > span.shard-detail {
  z-index: 2;
  display: block;
}
.badge > span:before {
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
.badge > span.shard-detail {
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
.badge > span.shard-detail dl {
  margin-bottom: 0;
}
.badge > span.shard-detail dt {
  width: 85px;
}
.badge > span.shard-detail dd {
  margin-left: 90px;
  text-align: left;
  overflow-wrap: break-word;
}

.badge.render-tooltip-bottom:hover > span {
  bottom: -120px;
}
.badge.render-tooltip-bottom:hover > span:before {
  bottom: 113px;
}
.badge.bg-secondary:not(.badge-notstarted):not(.bg-primary) {
  border: 2px dotted #6c757d;
}
.badge.bg-primary {
  border: 2px dotted var(--color-primary);
}
.badge-notstarted {
  border: 2px dotted var(--color-quaternary);
}
</style>
