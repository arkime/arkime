<template>

  <div>
    <table v-if="!error && stats"
      class="table table-sm table-striped">
      <thead class="small">
        <tr>
          <!-- TODO no min width -->
          <th v-for="column in columns"
            :key="column.name"
            style="min-width:90px"
            class="hover-menu">
            {{ column.name }}
            <!-- column dropdown menu -->
            <b-dropdown size="sm"
              class="ml-2 column-actions-btn"
              v-has-permission="'createEnabled'">
              <b-dropdown-item v-if="!column.nodeExcluded"
                @click="exclude('node', column.name)">
                Exclude node {{ column.name }}
              </b-dropdown-item>
              <b-dropdown-item v-if="column.nodeExcluded"
                @click="include('node', column.name)">
                Include node {{ column.name }}
              </b-dropdown-item>
              <b-dropdown-item v-if="!column.ipExcluded"
                @click="exclude('ip', column.ip)">
                Exclude IP {{ column.ip }}
              </b-dropdown-item>
              <b-dropdown-item v-if="column.ipExcluded"
                @click="include('ip', column.ip)">
                Include IP {{ column.ip }}
              </b-dropdown-item>
            </b-dropdown> <!-- /column dropdown menu -->
          </th>
        </tr>
      </thead>
      <tbody>
        <tr v-for="stat in stats.indices"
          :key="stat.name">
          <td class="no-wrap">
            {{ stat.name }}
          </td>
          <td v-for="node in nodes"
            :key="node">
            <template v-if="stat.nodes[node]"
              v-for="item in stat.nodes[node]">
              <span :key="node + '-' + stat.name + '-' + item.shard + '-shard'"
                class="badge badge-pill badge-secondary cursor-help"
                :class="{'badge-primary':item.prirep === 'p'}"
                :id="node + '-' + stat.name + '-' + item.shard + '-btn'">
                {{ item.shard }}
              </span>
              <b-tooltip :key="node + '-' + stat.name + '-' + item.shard + '-tooltip'"
                :target="node + '-' + stat.name + '-' + item.shard + '-btn'"
                placement="left">
                <div v-if="item.ip">
                  <strong>IP:</strong>
                  {{ item.ip }}
                </div>
                <div>
                  <strong>State:</strong>
                  {{ item.state }}
                </div>
                <div v-if="item.store">
                  <strong>Size:</strong>
                  {{ item.store }}
                </div>
                <div v-if="item.docs">
                  <strong>Documents:</strong>
                  {{ item.docs }}
                </div>
                <div>
                  <strong>Primary/Replicate:</strong>
                  {{ item.prirep }}
                </div>
              </b-tooltip>
            </template>
          </td>
        </tr>
        <tr v-if="!stats.indices">
          <td colspan="6"
            class="text-danger">
            <span class="fa fa-warning"></span>&nbsp;
            No results match your search
          </td>
        </tr>
      </tbody>
    </table>

    <error v-if="error"
      message="error">
    </error>

  </div>

</template>

<script>
import MolochError from '../utils/Error';

let reqPromise; // promise returned from setInterval for recurring requests

export default {
  name: 'Shards',
  components: { MolochError },
  props: [ 'user', 'dataInterval' ],
  data: function () {
    return {
      loading: true,
      error: '',
      stats: {},
      nodes: {},
      columns: [
        { name: 'Index', sort: 'action', doClick: false }
      ]
    };
  },
  created: function () {
    this.loadData();
    if (this.dataInterval !== '0') {
      reqPromise = setInterval(() => {
        this.loadData();
      }, parseInt(this.dataInterval, 10));
    }
  },
  methods: {
    loadData: function () {
      this.$http.get('esshard/list')
        .then((response) => {
          this.loading = false;
          this.stats = response.data;

          this.columns.splice(1);

          this.nodes = Object.keys(response.data.nodes).sort(function (a, b) {
            return a.localeCompare(b);
          });

          for (var node of this.nodes) {
            if (node === 'Unassigned') {
              this.columns.push({ name: node, doClick: false });
            } else {
              this.columns.push({
                name: node,
                doClick: (node.indexOf('->') === -1),
                ip: response.data.nodes[node].ip,
                ipExcluded: response.data.nodes[node].ipExcluded,
                nodeExcluded: response.data.nodes[node].nodeExcluded
              });
            }
          }
        }, (error) => {
          this.error = error;
          this.loading = false;
        });
    },
    exclude: function (type, value) {
      // TODO
      console.log('TODO: exclude');
    },
    include: function (type, value) {
      // TODO
      console.log('TODO: include');
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
table .hover-menu > .btn-group.column-actions-btn > .btn-sm {
  padding: 0 .35rem;
}
</style>

<style scoped>
table .hover-menu:hover .btn-group {
  visibility: visible;
}
table .hover-menu .btn-group {
  visibility: hidden;
}

.badge {
  font-weight: 500;
  font-size: 14px;
}
.badge.badge-primary {
  font-weight: bold;
  background-color: var(--color-primary);
}
</style>
