<template>

  <div class="ml-1 mr-1">

    <moloch-loading v-if="loading && !error">
    </moloch-loading>

    <moloch-error v-if="error"
      :message="error">
    </moloch-error>

    <div v-if="!error && !loading"
      class="shards-container mt-1">

      <div class="input-group input-group-sm">
        <div class="input-group-prepend">
          <span class="input-group-text">
            <span class="fa fa-search"></span>
          </span>
        </div>
        <input type="text"
          class="form-control shards-search"
          v-model="query.filter"
          @keyup="searchForES()"
          placeholder="Begin typing to search for ES nodes and indices">
      </div>

      <table class="table table-sm table-hover small mt-3">
        <thead>
          <tr>
            <th v-for="column in columns"
              :key="column.name"
              class="hover-menu">
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
          <tr v-for="stat in stats.indices"
            :key="stat.name">
            <td>
              {{ stat.name }}
            </td>
            <td v-for="node in nodes"
              :key="node"
              class="text-center">
              <template v-if="stat.nodes[node]"
                v-for="item in stat.nodes[node]">
                <span :key="node + '-' + stat.name + '-' + item.shard + '-shard'"
                  class="badge badge-pill badge-secondary cursor-help"
                  :class="{'badge-primary':item.prirep === 'p'}"
                  :id="node + '-' + stat.name + '-' + item.shard + '-btn'">
                  {{ item.shard }}
                  <span>
                    <div v-if="item.ip">
                      <span>IP:</span>
                      {{ item.ip }}
                    </div>
                    <div>
                      <span>State:</span>
                      {{ item.state }}
                    </div>
                    <div v-if="item.store">
                      <span>Size:</span>
                      {{ item.store }}
                    </div>
                    <div v-if="item.docs">
                      <span>Documents:</span>
                      {{ item.docs }}
                    </div>
                    <div>
                      <span>Primary/Replicate:</span>
                      {{ item.prirep }}
                    </div>
                  </span>
                </span>
              </template>
            </td>
          </tr>
          <tr v-if="!stats.indices.length">
            <td colspan="6"
              class="text-danger text-center">
              <span class="fa fa-warning"></span>&nbsp;
              No results match your search
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
let searchInputTimeout; // timeout to debounce the search input

export default {
  name: 'Shards',
  components: { MolochError, MolochLoading },
  props: [ 'dataInterval' ],
  data: function () {
    return {
      loading: true,
      error: '',
      stats: {},
      nodes: {},
      query: {
        filter: null,
        sortField: 'index',
        desc: false
      },
      columns: [
        { name: 'Index', sort: 'index', doClick: false, hasDropdown: false }
      ]
    };
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
    searchForES () {
      if (searchInputTimeout) { clearTimeout(searchInputTimeout); }
      // debounce the input so it only issues a request after keyups cease for 400ms
      searchInputTimeout = setTimeout(() => {
        searchInputTimeout = null;
        this.loadData();
      }, 400);
    },
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
    /* helper functions ------------------------------------------ */
    setRequestInterval: function () {
      reqPromise = setInterval(() => {
        this.loadData();
      }, parseInt(this.dataInterval, 10));
    },
    loadData: function () {
      this.$http.get('esshard/list', { params: this.query })
        .then((response) => {
          this.error = '';
          this.loading = false;
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
          this.error = error;
          this.loading = false;
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
.shards-container {
  overflow: hidden;
}

table > thead > tr > th {
  border-top: none;
}

.table .hover-menu {
  vertical-align: top;
  min-width: 100px;
}
.table .hover-menu:hover .btn-group {
  visibility: visible;
}

.table .hover-menu .btn-group {
  visibility: hidden;
  margin-left: -20px !important;
  position: relative;
  top: 2px;
  right: 2px;
  margin-top: -2px;
}

.table .hover-menu .header-text {
  display: inline-block;
  width: 100%;
  word-break: break-word;
}

.table tbody > tr > td:first-child {
  width:1px;
  white-space:nowrap;
  padding-right: .5rem;
}
/* hoverable columns */
table.table.table-hover td, th {
  position: relative;
}
table.table.table-hover td:hover::after,
table.table.table-hover th:hover::after {
  content: "";
  position: absolute;
  background-color: var(--color-gray-light) !important;
  left: 0;
  top: -5000px;
  height: 10000px;
  width: 100%;
  z-index: -1;
}

.badge {
  font-weight: 500;
  font-size: 14px;
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
.badge:hover > span {
  z-index: 2;
  font-weight: normal;
  display: block;
  position: absolute;
  margin: 10px;
  bottom: -14px;
  right: 20px;
  padding: 4px 6px;
  color: white;
  text-align: center;
  background-color: black;
  border-radius: 5px;
  font-size: 90%;
}
.badge > span:before {
  content:'';
  display:block;
  width:0;
  height:0;
  position:absolute;
  border-top: 8px solid transparent;
  border-bottom: 8px solid transparent;
  border-left:8px solid black;
  right:-8px;
  bottom:7px;
}
.badge > span span {
  color: #bbb;
}

</style>
