<template>
  <div class="container-fluid">
    <b-overlay
      rounded="sm"
      blur="0.2rem"
      opacity="0.9"
      :show="loading"
      variant="transparent">
      <b-table
        small
        hover
        striped
        :items="data"
        :fields="fields">
      </b-table>
      <template #overlay>
        <div class="text-center margin-for-nav-and-progress">
          <span class="fa fa-circle-o-notch fa-spin fa-2x" />
          <p>Loading stats...</p>
        </div>
      </template>
      <!-- stats error -->
      <div
        v-if="error.length"
        class="mt-2 alert alert-warning">
        <span class="fa fa-exclamation-triangle" />&nbsp;
        {{ error }}
        <button
          type="button"
          @click="error = ''"
          class="close cursor-pointer">
          <span>&times;</span>
        </button>
      </div> <!-- /stats error -->
    </b-overlay>
  </div>
</template>

<script>
import Cont3xtService from '@/components/services/Cont3xtService';

export default {
  name: 'Cont3xtStats',
  data () {
    return {
      data: [],
      error: '',
      loading: true,
      fields: [{
        key: 'name',
        sortable: true
      }, {
        key: 'cacheLookup',
        sortable: true
      }, {
        key: 'cacheFound',
        sortable: true
      }, {
        key: 'cacheGood',
        sortable: true
      }, {
        key: 'cacheRecentAvgMS',
        sortable: true
      }, {
        key: 'directLookup',
        sortable: true
      }, {
        key: 'directFound',
        sortable: true
      }, {
        key: 'directGood',
        sortable: true
      }, {
        key: 'directError',
        sortable: true
      }, {
        key: 'directRecentAvgMS',
        sortable: true
      }, {
        key: 'total',
        sortable: true
      }]
    };
  },
  mounted () {
    Cont3xtService.getStats().then((response) => {
      this.loading = false;
      this.data = response.stats;
    }).catch((err) => {
      this.error = err;
      this.loading = false;
    });
  },
  methods: {
    toBool (val) {
      return Boolean(val);
    }
  }
};
</script>
