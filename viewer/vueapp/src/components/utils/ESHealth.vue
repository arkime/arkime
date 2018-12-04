<template>

  <span>

    <!-- error -->
    <div v-if="error"
      class="error-div text-muted-more mt-2 text-right">
      <small>
        {{ error || 'Network Error' }} - try
        <a @click="reload"
          class="cursor-pointer reload-btn">
          reloading the page
        </a>
      </small>
    </div> <!-- /error -->

    <!-- info icon -->
    <span class="cursor-help"
      id="infoTooltip">
      <span class="fa fa-info-circle fa-2x"
        :class="esHealthClass"
        v-if="!error && esHealth">
      </span>
    </span> <!-- /info icon -->

    <!-- tooltip content -->
    <b-tooltip
      :showTooltip.sync="showTooltip"
      target="infoTooltip"
      placement="left"
      boundary="viewport">
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
        Initializing Shards: <strong>{{ esHealth.initializing_shards }}</strong>
      </div>
      <div v-if="error">

      </div>
    </b-tooltip> <!-- /tooltip content -->

  </span>

</template>

<script>
let interval;

export default {
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
    interval = setInterval(() => {
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
          console.log(error);
        });
    },
    reload: function () {
      window.location.reload();
    }
  },
  beforeDestroy: function () {
    if (interval) { clearInterval(interval); }
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

.error-div {
  line-height: 1;
  margin-right: -12px;
}

.reload-btn {
  color: var(--color-tertiary-light) !important;
}
.reload-btn:hover {
  color: var(--color-tertiary) !important;
}
</style>
