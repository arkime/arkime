<template>

  <div class="container-fluid mt-3">

    <moloch-loading v-if="initialLoading && !error">
    </moloch-loading>

    <moloch-error v-if="error"
      :message="error">
    </moloch-error>

    <div v-if="!error">

      <div v-if="stats.indices && !stats.indices.length"
        class="text-danger text-center mt-4 mb-4">
        <span class="fa fa-warning"></span>&nbsp;
        No results match your search.
        <br>
        You may need to adjust the "Show" pulldown
      </div>

      <table v-if="stats.indices && stats.indices.length"
        class="table table-sm small block-table mt-1">
        <thead>
          <tr>
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
                  v-has-permission="'createEnabled'">
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
              {{ stat.name }}
            </td>
            <td v-for="node in nodes"
              :key="node">
              <template v-if="stat.nodes[node]">
                <template v-for="item in stat.nodes[node]">
                  <span :key="node + '-' + stat.name + '-' + item.shard + '-shard'"
                    class="badge badge-pill badge-secondary cursor-help"
                    :class="{'badge-primary':item.prirep === 'p', 'badge-notstarted':item.state !== 'STARTED','render-tooltip-bottom':index < 5}"
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
                        <span v-if="item.ip">
                          <dt>IP</dt>
                          <dd>{{ item.ip }}</dd>
                        </span>
                        <dt>Shard</dt>
                        <dd>{{ item.shard }}</dd>
                        <dt>State</dt>
                        <dd>{{ item.state }}</dd>
                        <span v-if="item.ur">
                          <dt>Reason</dt>
                          <dd>{{ item.uf }}</dd>
                        </span>
                        <span v-if="item.uf">
                          <dt>For</dt>
                          <dd>{{ item.ur }}</dd>
                        </span>
                        <span v-if="item.store">
                          <dt>Size</dt>
                          <dd>{{ item.store | humanReadableBytes }}</dd>
                        </span>
                        <span v-if="item.docs">
                          <dt>Documents</dt>
                          <dd>{{ item.docs | round(0) | commaString }}</dd>
                        </span>
                        <span v-if="item.fm">
                          <dt>Field Mem</dt>
                          <dd>{{ item.fm | humanReadableBytes }}</dd>
                        </span>
                        <span v-if="item.sm">
                          <dt>Segment Mem</dt>
                          <dd>{{ item.sm | humanReadableBytes }}</dd>
                        </span>
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
import MolochError from '../utils/Error';
import MolochLoading from '../utils/Loading';

let reqPromise; // promise returned from setInterval for recurring requests
let respondedAt; // the time that the last data load succesfully responded

export default {
  name: 'EsShards',
  components: { MolochError, MolochLoading },
  props: [
    'dataInterval',
    'shardsShow',
    'refreshData',
    'searchTerm'
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
        show: this.shardsShow || 'notstarted'
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
    }
  },
  created: function () {
    this.loadData();
    if (this.dataInterval !== '0') {
      this.setRequestInterval();
    }
  },
  methods: {
    /* exposed page functions ------------------------------------ */
    columnClick (name) {
      if (!name) { return; }

      this.query.sortField = name;
      this.query.desc = !this.query.desc;
      this.loadData();
    },
    exclude: function (type, column) {
      this.$http.post(`esshard/exclude/${type}/${column[type]}`)
        .then((response) => {
          if (type === 'name') {
            column.nodeExcluded = true;
          } else {
            column.ipExcluded = true;
          }
        }, (error) => {
          this.error = error.text || error;
        });
    },
    include: function (type, column) {
      this.$http.post(`esshard/include/${type}/${column[type]}`)
        .then((response) => {
          if (type === 'name') {
            column.nodeExcluded = false;
          } else {
            column.ipExcluded = false;
          }
        }, (error) => {
          this.error = error.text || error;
        });
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
    loadData: function () {
      this.loading = true;
      respondedAt = undefined;

      this.query.filter = this.searchTerm;

      this.$http.get('esshard/list', { params: this.query })
        .then((response) => {
          respondedAt = Date.now();
          this.error = '';
          this.loading = false;
          this.initialLoading = false;
          this.stats = response.data;

          this.columns.splice(1);

          this.nodes = Object.keys(response.data.nodes).sort(function (a, b) {
            return a.localeCompare(b);
          });

          for (var node of this.nodes) {
            if (node === 'Unassigned') {
              this.columns.push({ name: node, doClick: false, hasDropdown: false });
            } else {
              this.columns.push({
                name: node,
                doClick: (node.indexOf('->') === -1),
                ip: response.data.nodes[node].ip,
                ipExcluded: response.data.nodes[node].ipExcluded,
                nodeExcluded: response.data.nodes[node].nodeExcluded,
                hasDropdown: true
              });
            }
          }
        }, (error) => {
          respondedAt = undefined;
          this.error = error.text || error;
          this.loading = false;
          this.initialLoading = false;
        });
    }
  },
  beforeDestroy: function () {
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
.badge.badge-primary {
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
.badge.badge-secondary:not(.badge-notstarted):not(.badge-primary) {
  border: 2px dotted #6c757d;
}
.badge.badge-primary {
  border: 2px dotted var(--color-primary);
}
.badge-notstarted {
  border: 2px dotted var(--color-quaternary);
}
</style>
