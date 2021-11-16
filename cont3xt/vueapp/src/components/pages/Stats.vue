<template>
  <div class="container-fluid">
    <b-overlay
      rounded="sm"
      blur="0.2rem"
      opacity="0.9"
      :show="loading"
      variant="transparent">
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
      <!-- table -->
      <b-table
        small
        hover
        striped
        show-empty
        :items="data"
        :filter="filter"
        :fields="fields"
        :filter-included-fields="filterOn"
        empty-text="There are no integrations to show stats for"
        :empty-filtered-text="`There are no integrations that match the name: ${filter}`">
      </b-table> <!-- /table -->
      <!-- loading overlay template -->
      <template #overlay>
        <div class="text-center margin-for-nav-and-progress">
          <span class="fa fa-circle-o-notch fa-spin fa-2x" />
          <p>Loading stats...</p>
        </div>
      </template> <!-- /loading overlay template -->
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
      filter: '',
      filterOn: ['name'],
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
    Cont3xtService.getStats().then((response) => {
      this.loading = false;
      this.data = response.stats;
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
    }
  }
};
</script>
