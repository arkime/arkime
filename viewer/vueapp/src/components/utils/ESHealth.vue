<!--
Copyright Yahoo Inc.
SPDX-License-Identifier: Apache-2.0
-->
<template>
  <span>

    <!-- error -->
    <div
      v-if="error"
      id="networkError"
      class="error-div text-muted pull-right"
    >
      <small>
        {{ error || 'Network Error' }} - try
        <a
          @click="window.location.reload()"
          class="cursor-pointer reload-btn me-2"
        >
          reloading the page
        </a>
      </small>
      <BTooltip target="networkError">{{ errorTitle }}</BTooltip>
    </div> <!-- /error -->

    <!-- info icon -->
    <span
      class="cursor-help"
      id="infoTooltip"
    >
      <span
        class="fa fa-info-circle fa-lg"
        :class="esHealthClass"
        v-if="!error && esHealth"
      />
      <!-- tooltip content -->
      <BTooltip target="infoTooltip">
        <div class="text-center mb-1">
          <strong>{{ $t('eshealth.title') }}</strong>
        </div>
        <dl
          v-if="!error && esHealth"
          class="dl-horizontal es-stats-dl"
        >
          <dt>{{ $t('eshealth.userName') }}</dt>
          <dd>{{ user.userName }}&nbsp;</dd>
          <dt>{{ $t('eshealth.userId') }}</dt>
          <dd>{{ user.userId }}&nbsp;</dd>
          <dt>{{ $t('eshealth.esVersion') }}</dt>
          <dd>{{ esHealth.version }}&nbsp;</dd>
          <dt>{{ $t('eshealth.dbVersion') }}</dt>
          <dd>{{ esHealth.molochDbVersion }}&nbsp;</dd>
          <dt>{{ $t('eshealth.cluster') }}</dt>
          <dd>{{ esHealth.cluster_name }}&nbsp;</dd>
          <dt>{{ $t('eshealth.status') }}</dt>
          <dd>{{ esHealth.status }}&nbsp;</dd>
          <dt>{{ $t('eshealth.nodes') }}</dt>
          <dd>{{ esHealth.number_of_nodes }}&nbsp;</dd>
          <dt>{{ $t('eshealth.shards') }}</dt>
          <dd>{{ esHealth.active_shards }}&nbsp;</dd>
          <dt>{{ $t('eshealth.relocating') }}</dt>
          <dd>{{ esHealth.relocating_shards }}&nbsp;</dd>
          <dt>{{ $t('eshealth.unassigned') }}</dt>
          <dd>{{ esHealth.unassigned_shards }}&nbsp;</dd>
          <dt>{{ $t('eshealth.initializing') }}</dt>
          <dd>{{ esHealth.initializing_shards }}&nbsp;</dd>
        </dl>
      </BTooltip> <!-- /tooltip content -->
    </span> <!-- /info icon -->

  </span>
</template>

<script>
import StatsService from '../stats/StatsService';

let interval;
const minTimeToWait = 10000;
let timeToWait = minTimeToWait;

export default {
  name: 'ESHealth',
  created () {
    this.getHealth();
  },
  computed: {
    user () {
      return this.$store.state.user;
    },
    esHealth () {
      return this.$store.state.esHealth;
    },
    error () {
      // truncate the error and show the full error in a title attribute
      let error = this.$store.state.esHealthError || '';
      if (typeof error !== 'string') {
        return 'Error loading health';
      }
      if (error.length > 50) {
        error = error.substring(0, 50) + '...';
      }
      return error;
    },
    errorTitle () {
      return this.$store.state.esHealthError.text || '';
    },
    esHealthClass: function () {
      return {
        'health-green': this.esHealth.status === 'green',
        'health-yellow': this.esHealth.status === 'yellow',
        'health-red': this.esHealth.status === 'red'
      };
    }
  },
  methods: {
    getHealth () {
      if (interval) { clearInterval(interval); }

      interval = setInterval(() => {
        StatsService.getESHealth().then(() => {
          if (timeToWait !== minTimeToWait) {
            timeToWait = minTimeToWait;
            this.getHealth();
          }
        }).catch((error) => {
          timeToWait = Math.min(timeToWait * 2, 300000); // max 5 minutes between retries
          this.getHealth();
        });
      }, timeToWait);
    }
  },
  beforeUnmount () {
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
  margin-left: 10px;
  margin-right: 10px;
  display: inline-block;
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
