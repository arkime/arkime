<template>
  <div class="container-fluid overflow-auto pt-3">
    <b-overlay
      rounded="sm"
      blur="0.2rem"
      opacity="0.9"
      :show="loading"
      variant="transparent">

      <!-- loading overlay template -->
      <template #overlay>
        <div class="text-center">
          <span class="fa fa-circle-o-notch fa-spin fa-2x" />
          <p>Loading stats...</p>
        </div>
      </template> <!-- /loading overlay template -->

      <!-- search -->
      <b-input-group class="mb-3">
        <template #prepend>
          <b-input-group-text>
            <span class="fa fa-search" />
          </b-input-group-text>
        </template>
        <b-form-input
          debounce="400"
          v-model="filter"
          placeholder="Search by name"
        />
      </b-input-group> <!-- /search -->

      <b-tabs content-class="mt-3">
        <!-- general stats table -->
        <b-tab
          title="Integrations"
          @click="clickTab('integrations')"
          :active="activeTab === 'integrations'">
          <b-table
            small
            hover
            striped
            show-empty
            :dark="getDarkThemeEnabled"
            :filter="filter"
            :fields="fields"
            :items="data.stats"
            :sort-by.sync="sortBy"
            :sort-desc.sync="sortDesc"
            :filter-included-fields="filterOn"
            empty-text="There are no integrations to show stats for"
            :empty-filtered-text="`There are no integrations that match the name: ${filter}`">
          </b-table>
        </b-tab> <!-- /general stats table -->
        <!-- itype stats table -->
        <b-tab
          title="ITypes"
          @click="clickTab('itypes')"
          :active="activeTab === 'itypes'">
          <b-table
            small
            hover
            striped
            show-empty
            :dark="getDarkThemeEnabled"
            :filter="filter"
            :fields="fields"
            :sort-by.sync="sortBy"
            :items="data.itypeStats"
            :sort-desc.sync="sortDesc"
            :filter-included-fields="filterOn"
            empty-text="There are no itypes to show stats for"
            :empty-filtered-text="`There are no itypes that match the name: ${filter}`">
          </b-table>
        </b-tab>  <!-- /itype stats table -->
        <template #tabs-end>
          <li role="presentation"
            class="nav-item align-self-center startup-time">
            Started at
            <strong>{{ data.startTime | dateString }}</strong>
          </li>
        </template>
      </b-tabs>

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
import { mapGetters } from 'vuex';

export default {
  name: 'Cont3xtStats',
  computed: {
    ...mapGetters(['getDarkThemeEnabled'])
  },
  data () {
    return {
      data: {},
      error: '',
      loading: true,
      sortBy: 'name',
      sortDesc: false,
      filter: '',
      filterOn: ['name'],
      activeTab: 'integrations',
      fields: [{
        key: 'name',
        sortable: true
      }, {
        key: 'cacheLookup',
        sortable: true,
        formatter: this.commaString,
        tdClass: 'text-right',
        thClass: 'text-right'
      }, {
        key: 'cacheFound',
        sortable: true,
        formatter: this.commaString,
        tdClass: 'text-right',
        thClass: 'text-right'
      }, {
        key: 'cacheGood',
        sortable: true,
        formatter: this.commaString,
        tdClass: 'text-right',
        thClass: 'text-right'
      }, {
        key: 'cacheRecentAvgMS',
        sortable: true,
        formatter: this.commaStringRound,
        tdClass: 'text-right',
        thClass: 'text-right'
      }, {
        key: 'directLookup',
        sortable: true,
        formatter: this.commaString,
        tdClass: 'text-right',
        thClass: 'text-right'
      }, {
        key: 'directFound',
        sortable: true,
        formatter: this.commaString,
        tdClass: 'text-right',
        thClass: 'text-right'
      }, {
        key: 'directGood',
        sortable: true,
        formatter: this.commaString,
        tdClass: 'text-right',
        thClass: 'text-right'
      }, {
        key: 'directError',
        sortable: true,
        formatter: this.commaString,
        tdClass: 'text-right',
        thClass: 'text-right'
      }, {
        key: 'directRecentAvgMS',
        sortable: true,
        formatter: this.commaStringRound,
        tdClass: 'text-right',
        thClass: 'text-right'
      }, {
        key: 'total',
        sortable: true,
        formatter: this.commaString,
        tdClass: 'text-right',
        thClass: 'text-right'
      }]
    };
  },
  mounted () {
    // set active tab
    const hash = location.hash.substring(1, location.hash.length);
    if (hash === 'itypes') {
      this.activeTab = 'itypes';
    }

    Cont3xtService.getStats().then((response) => {
      this.loading = false;
      this.data = response;
    }).catch((err) => {
      this.error = err;
      this.loading = false;
    });
  },
  methods: {
    commaString (val) {
      return this.$options.filters.commaString(val);
    },
    commaStringRound (val) {
      return this.$options.filters.roundCommaString(val, 2);
    },
    clickTab (tab) {
      location.hash = tab;
      this.activeTab = tab;
    }
  }
};
</script>

<style scoped>
.startup-time {
  right: 15px;
  position: absolute;
}
</style>
