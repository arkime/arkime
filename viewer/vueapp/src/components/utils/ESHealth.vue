<template>

  <span>

    <!-- error -->
    <div v-if="error"
      v-b-tooltip.hover
      :title="errorTitle"
      class="error-div text-muted-more mt-2 text-right pull-right cursor-help">
      <small>
        {{ error || 'Network Error' }} - try
        <a @click="window.location.reload()"
          class="cursor-pointer reload-btn mr-2">
          reloading the page
        </a>
      </small>
    </div> <!-- /error -->

    <!-- info icon -->
    <span class="cursor-help"
      id="infoTooltip">
      <span class="fa fa-info-circle fa-lg"
        :class="esHealthClass"
        v-if="!error && esHealth">
      </span>
    </span> <!-- /info icon -->

    <!-- tooltip content -->
    <b-tooltip
      target="infoTooltip"
      placement="bottom"
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
import StatsService from '../stats/StatsService';
let interval;

export default {
  name: 'ESHealth',
  created: function () {
    interval = setInterval(() => {
      StatsService.getESHealth();
    }, 10000);
  },
  computed: {
    esHealth () {
      return this.$store.state.esHealth;
    },
    error () {
      // truncate the error and show the full error in a title attribute
      let error = this.$store.state.esHealthError || '';
      if (error.length > 50) {
        error = error.substring(0, 50) + '...';
      }
      return error;
    },
    errorTitle () {
      return this.$store.state.esHealthError;
    },
    esHealthClass: function () {
      return {
        'health-green': this.esHealth.status === 'green',
        'health-yellow': this.esHealth.status === 'yellow',
        'health-red': this.esHealth.status === 'red'
      };
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

.fa-info-circle {
  margin-top: 9px;
}
</style>
