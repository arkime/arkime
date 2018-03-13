<template>

  <span>

    <!-- info/error icon -->
    <span class="cursor-help"
      id="infoTooltip">
      <span v-if="!error && esHealth"
        class="fa fa-info-circle fa-2x"
        :class="esHealthClass">
      </span>
      <span v-if="error"
        class="fa fa-exclamation-triangle fa-2x">
      </span>
    </span> <!-- /info/error icon -->

    <!-- tooltip content -->
    <b-tooltip
      :showTooltip.sync="showTooltip"
      target="infoTooltip"
      placement="left">
      <div v-if="!error && esHealth">
        <strong>Elasticsearch</strong><br>
        ES Version: <strong>{{ esHealth.version }}</strong><br>
        DB Version: <strong>{{ esHealth.molochDbVersion }}</strong><br>
        Cluster: <strong>{{ esHealth.cluster_name }}</strong><br>
        Status: <strong>{{ esHealth.status }}</strong><br>
        Nodes: <strong>{{ esHealth.number_of_nodes }}</strong><br>
        Shards: <strong>{{ esHealth.active_shards }}</strong><br>
        Relocating Shards: <strong>{{ esHealth.relocating_shards }}</strong><br>
        Unassigned Shards: <strong>{{ esHealth.unassigned_shards }}</strong>
      </div>
      <div v-if="error">
        {{ error }}
      </div>
    </b-tooltip> <!-- /tooltip content -->

  </span>

</template>

<script>
export default {
  // TODO on destroy remove interval for loading data
  name: 'ESHealth',
  data: function () {
    return {
      error: null,
      esHealth: null,
      showTooltip: true
    };
  },
  created: function () {
    this.loadData();
    setInterval(() => {
      this.loadData();
    }, 10000);
  },
  computed: {
    esHealthClass: function () {
      return {
        'health-green': this.esHealth.status === 'green',
        'health-yellow': this.esHealth.status === 'yellow',
        'health-red': this.esHealth.status === 'red'
      };
    }
  },
  methods: {
    loadData: function () {
      this.error = null;

      this.$http.get('eshealth.json')
        .then((response) => {
          this.esHealth = response.data;
        }, (error) => {
          this.error = error;
        });
    }
  }
};
</script>

<style scoped>
.health-red {
  color: #ff0000;
}
.health-yellow {
  color: #ffff00;
}
.health-green {
  color: #00aa00;
}
</style>
