<!--
Copyright Yahoo Inc.
SPDX-License-Identifier: Apache-2.0
-->
<template>
  <div class="container-fluid">
    <div
      v-if="alertMessage"
      style="z-index: 2000;"
      class="alert alert-danger position-fixed fixed-bottom m-0 rounded-0">
      {{ alertMessage }}
      <button
        type="button"
        class="btn-close pull-right"
        @click="alertMessage = ''" />
    </div>

    <div class="row">
      <div class="col-12">
        <div class="input-group mb-1">
          <span class="input-group-text">
            <span class="fa fa-search fa-fw" />
          </span>
          <input
            type="text"
            class="form-control"
            v-model="searchTerm"
            @input="debounceInput"
            :placeholder="$t('wise.stats.searchPlaceholder')">
        </div>
      </div>
    </div>

    <b-tabs
      class="mt-3"
      :dark="getTheme === 'dark'">
      <b-tab
        :title="$t('wise.stats.sources')"
        @click="clickTab('sources')"
        :active="activeTab === 'sources'">
        <div v-if="sourceStats.length > 0">
          <BTable
            small
            striped
            :items="sourceStats"
            :fields="sourceTableFields"
            :dark="getTheme === 'dark'" />
        </div>
        <div
          v-else-if="searchTerm"
          class="vertical-center info-area mt-5 pt-5">
          <div class="text-center">
            <h1><span class="fa fa-folder-open fa-2x" /></h1>
            {{ $t('wise.stats.noSourceMatches') }}
          </div>
        </div>
      </b-tab>
      <b-tab
        :title="$t('wise.stats.types')"
        @click="clickTab('types')"
        :active="activeTab === 'types'">
        <div v-if="typeStats.length > 0">
          <BTable
            small
            striped
            :items="typeStats"
            :fields="typeTableFields"
            :dark="getTheme === 'dark'" />
        </div>
        <div
          v-else-if="searchTerm"
          class="vertical-center info-area mt-5 pt-5">
          <div class="text-center">
            <h1><span class="fa fa-folder-open fa-2x" /></h1>
            {{ $t('wise.stats.noTypeMatches') }}
          </div>
        </div>
      </b-tab>
      <template #tabs-end>
        <li
          role="presentation"
          class="nav-item align-self-center startup-time">
          {{ $t('wise.stats.startedAt') }}
          <strong>{{ startTime }}</strong>
        </li>
      </template>
    </b-tabs>

    <div
      v-if="showEmpty && !searchTerm && !sourceStats.length"
      class="vertical-center info-area mt-5 pt-5">
      <div>
        <h1><span class="fa fa-folder-open fa-2x" /></h1>
        <p v-html="$t('wise.noSourcesHtml')" />
      </div>
    </div>
  </div>
</template>

<script>
import moment from 'moment-timezone';
import { mapGetters } from 'vuex';

import WiseService from './wise.service.js';

let dataInterval;
let searchTimeout;

export default {
  name: 'Stats',
  data () {
    return {
      showEmpty: false,
      alertMessage: '',
      sourceStats: [],
      typeStats: [],
      sourceTableFields: [],
      typeTableFields: [],
      startTime: undefined,
      searchTerm: '',
      sortBySources: 'source',
      sortDescSources: false,
      sortByTypes: 'type',
      sortDescTypes: false,
      activeTab: 'sources'
    };
  },
  mounted () {
    // set active tab
    const hash = location.hash.substring(1, location.hash.length);
    if (hash === 'types') {
      this.activeTab = 'types';
    }

    this.loadResourceStats();
    this.setLoadInterval();
  },
  computed: {
    ...mapGetters(['getTheme', 'getStatsDataInterval'])
  },
  watch: {
    getStatsDataInterval (newVal, oldVal) {
      this.setLoadInterval();
    }
  },
  methods: {
    loadResourceStats () {
      WiseService.getResourceStats({ search: this.searchTerm }).then((data) => {
        this.showEmpty = true;
        this.alertMessage = '';
        if (data && data.startTime) {
          this.startTime = moment.tz(data.startTime, Intl.DateTimeFormat().resolvedOptions().timeZone).format('YYYY/MM/DD HH:mm:ss z');
        }
        if (data && data.sources) {
          this.sourceTableFields = []; // clear fields before creating them again if there are data sources
          if (data.sources.length === 0) {
            this.sourceStats = [];
          } else {
            this.sourceStats = data.sources;
            Object.keys(this.sourceStats[0]).forEach(key => {
              const obj = { key, label: this.$t(`wise.stats.source-${key}`), sortable: true };
              if (key !== 'source') {
                obj.formatter = (value) => value.toString().replace(/\B(?<!\.\d*)(?=(\d{3})+(?!\d))/g, ',');
                obj.tdClass = 'text-right';
                obj.thClass = 'text-right';
              }
              this.sourceTableFields.push(obj);
            });
          }
        }
        if (data && data.types) {
          if (data.types.length === 0) {
            this.typeStats = [];
          } else {
            this.typeTableFields = []; // clear fields before creating them again if there are data types
            this.typeStats = data.types;
            Object.keys(this.typeStats[0]).forEach(key => {
              const obj = { key, label: this.$t(`wise.stats.type-${key}`), sortable: true };
              if (key !== 'type') {
                obj.formatter = (value) => value.toString().replace(/\B(?<!\.\d*)(?=(\d{3})+(?!\d))/g, ',');
                obj.tdClass = 'text-right';
                obj.thClass = 'text-right';
              }
              this.typeTableFields.push(obj);
            });
          }
        }
      }).catch((error) => {
        this.showEmpty = true;
        this.alertMessage = error.text ||
          'Error fetching resource stats for wise.';
      });
    },
    setLoadInterval () {
      if (dataInterval) { clearInterval(dataInterval); }
      if (!this.getStatsDataInterval) { return; }

      dataInterval = setInterval(() => {
        this.loadResourceStats();
      }, this.getStatsDataInterval);
    },
    debounceInput () {
      if (!this.searchTerm) { this.showEmpty = false; }
      if (searchTimeout) { clearTimeout(searchTimeout); }
      searchTimeout = setTimeout(() => {
        searchTimeout = null;
        this.loadResourceStats();
      }, 500);
    },
    clickTab (tab) {
      location.hash = tab;
      this.activeTab = tab;
    }
  },
  beforeUnmount () {
    if (dataInterval) { clearInterval(dataInterval); }
  }
};
</script>

<style scoped>
.startup-time {
  right: 15px;
  position: absolute;
}
</style>
