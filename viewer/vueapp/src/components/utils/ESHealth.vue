<template>

  <span>

    <!-- error -->
    <div v-if="error"
      :title="errorTitle"
      class="error-div text-muted-more mt-2 text-right pull-right cursor-help">
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
      <div class="text-center mb-1">
        <strong>Elasticsearch Stats</strong>
      </div>
      <dl v-if="!error && esHealth"
        class="dl-horizontal es-stats-dl">
        <dt>ES Version</dt>
        <dd>{{ esHealth.version }}&nbsp;</dd>
        <dt>DB Version</dt>
        <dd>{{ esHealth.molochDbVersion }}&nbsp;</dd>
        <dt>Cluster</dt>
        <dd>{{ esHealth.cluster_name }}&nbsp;</dd>
        <dt>Status</dt>
        <dd>{{ esHealth.status }}&nbsp;</dd>
        <dt>Nodes</dt>
        <dd>{{ esHealth.number_of_nodes }}&nbsp;</dd>
        <dt>Shards</dt>
        <dd>{{ esHealth.active_shards }}&nbsp;</dd>
        <dt>Relocating Shards</dt>
        <dd>{{ esHealth.relocating_shards }}&nbsp;</dd>
        <dt>Unassigned Shards</dt>
        <dd>{{ esHealth.unassigned_shards }}&nbsp;</dd>
        <dt>Initializing Shards</dt>
        <dd>{{ esHealth.initializing_shards }}&nbsp;</dd>
      </dl>
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
      errorTitle: null,
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
          this.error = null;
          this.errorTite = null;
        }, (error) => {
          console.log(error);
          this.error = error;
          // truncate the error and show the full error in a title attribute
          this.errorTitle = this.error;
          if (this.error.length > 50) {
            this.error = this.error.substring(0, 50) + '...';
          }
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

.es-stats-dl dt {
  width: 135px;
  font-weight: normal;
}
.es-stats-dl dd {
  margin-left: 145px;
  text-align: left;
  font-weight: bold;
}
</style>
