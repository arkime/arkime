<!--
Copyright Yahoo Inc.
SPDX-License-Identifier: Apache-2.0
-->
<template>
  <v-container fluid>
    <v-snackbar
      :model-value="!!alertMessage"
      color="error"
      location="bottom"
      :timeout="-1"
      @update:model-value="(v) => { if (!v) alertMessage = '' }">
      {{ alertMessage }}
      <template #actions>
        <v-btn
          variant="text"
          icon="mdi-close"
          @click="alertMessage = ''" />
      </template>
    </v-snackbar>

    <v-row class="arkime-toolbar">
      <v-col cols="12">
        <v-text-field
          v-model="searchTerm"
          :placeholder="$t('wise.stats.searchPlaceholder')"
          prepend-inner-icon="mdi-magnify"
          density="compact"
          hide-details
          @input="debounceInput" />
      </v-col>
    </v-row>

    <div class="mt-3 d-flex align-center">
      <v-tabs
        v-model="activeTab"
        density="compact"
        color="primary">
        <v-tab
          value="sources"
          @click="clickTab('sources')">
          {{ $t('wise.stats.sources') }}
        </v-tab>
        <v-tab
          value="types"
          @click="clickTab('types')">
          {{ $t('wise.stats.types') }}
        </v-tab>
      </v-tabs>
      <v-spacer />
      <span class="startup-time text-medium-emphasis">
        {{ $t('wise.stats.startedAt') }}
        <strong>{{ startTime }}</strong>
      </span>
    </div>

    <div v-if="activeTab === 'sources'">
      <v-data-table
        v-if="sourceStats.length > 0"
        density="compact"
        :items="sourceStats"
        :headers="sourceTableHeaders" />
      <div
        v-else-if="searchTerm"
        class="vertical-center info-area mt-5 pt-5">
        <div class="text-center">
          <h1>
            <v-icon
              icon="mdi-folder-open"
              size="large" />
          </h1>
          {{ $t('wise.stats.noSourceMatches') }}
        </div>
      </div>
    </div>

    <div v-if="activeTab === 'types'">
      <v-data-table
        v-if="typeStats.length > 0"
        density="compact"
        :items="typeStats"
        :headers="typeTableHeaders" />
      <div
        v-else-if="searchTerm"
        class="vertical-center info-area mt-5 pt-5">
        <div class="text-center">
          <h1>
            <v-icon
              icon="mdi-folder-open"
              size="large" />
          </h1>
          {{ $t('wise.stats.noTypeMatches') }}
        </div>
      </div>
    </div>

    <div
      v-if="showEmpty && !searchTerm && !sourceStats.length"
      class="vertical-center info-area mt-5 pt-5">
      <div>
        <h1>
          <v-icon
            icon="mdi-folder-open"
            size="large" />
        </h1>
        <p v-html="$t('wise.noSourcesHtml')" />
      </div>
    </div>
  </v-container>
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
      sourceTableHeaders: [],
      typeTableHeaders: [],
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
        const commaFormat = (v) => v == null ? '' : v.toString().replace(/\B(?<!\.\d*)(?=(\d{3})+(?!\d))/g, ',');

        if (data && data.sources) {
          this.sourceTableHeaders = [];
          if (data.sources.length === 0) {
            this.sourceStats = [];
          } else {
            this.sourceStats = data.sources;
            Object.keys(this.sourceStats[0]).forEach(key => {
              const header = { title: this.$t(`wise.stats.source-${key}`), key };
              if (key !== 'source') {
                header.align = 'end';
                header.value = (item) => commaFormat(item[key]);
              }
              this.sourceTableHeaders.push(header);
            });
          }
        }
        if (data && data.types) {
          if (data.types.length === 0) {
            this.typeStats = [];
          } else {
            this.typeTableHeaders = [];
            this.typeStats = data.types;
            Object.keys(this.typeStats[0]).forEach(key => {
              const header = { title: this.$t(`wise.stats.type-${key}`), key };
              if (key !== 'type') {
                header.align = 'end';
                header.value = (item) => commaFormat(item[key]);
              }
              this.typeTableHeaders.push(header);
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
