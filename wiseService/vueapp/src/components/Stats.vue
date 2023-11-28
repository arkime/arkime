<!--
Copyright Yahoo Inc.
SPDX-License-Identifier: Apache-2.0
-->
<template>
  <div class="container-fluid">
    <Alert :initialAlert="alertMessage"
      variant="alert-danger"
      v-on:clear-initialAlert="alertMessage = ''"
    />

    <div class="row">
      <div class="col-12">
        <div class="input-group mb-3">
          <div class="input-group-prepend">
            <span class="input-group-text">
              <span class="fa fa-search fa-fw" />
            </span>
          </div>
          <input type="text"
            class="form-control"
            v-model="searchTerm"
            @input="debounceInput"
            placeholder="Search WISE Sources..."
          />
        </div>
      </div>
    </div>

    <b-tabs content-class="mt-3"
      :dark="getTheme ==='dark'">
      <b-tab
        title="Sources"
        @click="clickTab('sources')"
        :active="activeTab === 'sources'">
        <div v-if="sourceStats.length > 0">
          <b-table striped hover small borderless
            :dark="getTheme ==='dark'"
            :items="sourceStats"
            :fields="sourceTableFields"
            :sort-by.sync="sortBySources"
            :sort-desc.sync="sortDescSources">
          </b-table>
        </div>
        <div v-else-if="searchTerm"
          class="vertical-center info-area mt-5 pt-5">
          <div class="text-center">
            <h1><b-icon-folder2-open /></h1>
            No sources match your search.
          </div>
        </div>
      </b-tab>
      <b-tab
        title="Types"
        @click="clickTab('types')"
        :active="activeTab === 'types'">
        <div v-if="typeStats.length > 0">
          <b-table striped hover small borderless
            :dark="getTheme ==='dark'"
            :items="typeStats"
            :fields="typeTableFields"
            :sort-by.sync="sortByTypes"
            :sort-desc.sync="sortDescTypes">
          </b-table>
        </div>
        <div v-else-if="searchTerm"
          class="vertical-center info-area mt-5 pt-5">
          <div class="text-center">
            <h1><b-icon-folder2-open /></h1>
            No types match your search.
          </div>
        </div>
      </b-tab>
      <template #tabs-end>
        <li role="presentation"
          class="nav-item align-self-center startup-time">
          Started at
          <strong>{{ startTime }}</strong>
        </li>
      </template>
    </b-tabs>

    <div v-if="showEmpty && !searchTerm && !sourceStats.length"
      class="vertical-center info-area mt-5 pt-5">
      <div>
        <h1><b-icon-folder2-open /></h1>
        Looks like you don't have any WISE sources yet.
        <br>
        Check out our
        <a href="help#getStarted"
          class="no-decoration">
          getting started section
        </a> for help.
        <br>
        Or add a source on the
        <a href="config"
          class="no-decoration">
          Config Page</a>.
      </div>
    </div>

  </div>
</template>

<script>
import moment from 'moment-timezone';
import { mapGetters } from 'vuex';

import WiseService from './wise.service';
import Alert from './Alert';

let dataInterval;
let searchTimeout;

export default {
  name: 'Stats',
  components: {
    Alert
  },
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
          if (data.sources.length === 0) {
            this.sourceStats = [];
          } else {
            this.sourceStats = data.sources;
            Object.keys(this.sourceStats[0]).forEach(key => {
              const obj = { key, sortable: true };
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
            this.typeStats = data.types;
            Object.keys(this.typeStats[0]).forEach(key => {
              const obj = { key, sortable: true };
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
  beforeDestroy () {
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
