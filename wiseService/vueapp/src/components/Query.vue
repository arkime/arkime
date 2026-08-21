<!--
Copyright Yahoo Inc.
SPDX-License-Identifier: Apache-2.0
-->
<template>
  <!-- container -->
  <v-container fluid>
    <div class="d-flex flex-row align-center arkime-toolbar ga-2">
      <!-- source select -->
      <v-select
        v-model="chosenSource"
        :items="['', ...sources]"
        :label="$t('wise.query.source')"
        density="compact"
        style="max-width: 220px;"
        tabindex="1"
        @update:model-value="debounceSearch">
        <template #selection="{ item }">
          <span>{{ item.value || $t('wise.query.any') }}</span>
        </template>
        <template #item="{ item, props: itemProps }">
          <v-list-item
            v-bind="itemProps"
            :title="item.value || $t('wise.query.any')" />
        </template>
      </v-select>

      <!-- type select -->
      <v-select
        v-model="chosenType"
        :items="types"
        :label="$t('wise.query.type')"
        density="compact"
        class="ms-3"
        style="max-width: 220px;"
        tabindex="2"
        @update:model-value="sendSearchQuery" />

      <!-- search -->
      <v-text-field
        v-model="searchTerm"
        :placeholder="$t('wise.query.searchTermPlaceholder', { type: chosenType })"
        :prepend-inner-icon="loading ? 'mdi-loading mdi-spin' : 'mdi-magnify'"
        clearable
        density="compact"
        class="ms-3 flex-grow-1"
        tabindex="3"
        @input="debounceSearch"
        @keyup.enter="sendSearchQuery"
        @click:clear="clear" />
    </div>

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

    <!-- empty search -->
    <div v-if="!hasMadeASearch">
      <div class="vertical-center info-area mt-5">
        <div>
          <v-icon
            icon="mdi-magnify"
            size="x-large" />
          {{ $t('wise.query.noSearchTerm') }}
          <span
            v-if="!sources.length"
            v-html="$t('wise.noSourcesHtml')" />
        </div>
      </div>
    </div> <!-- /empty search -->

    <!-- tabbed view options -->
    <div
      v-else-if="searchResult.length > 0"
      class="mt-3">
      <v-tabs
        v-model="resultTab"
        density="compact"
        color="primary">
        <v-tab value="table">
          {{ $t('wise.query.tableView') }}
        </v-tab>
        <v-tab value="json">
          {{ $t('wise.query.jsonView') }}
        </v-tab>
        <v-tab value="csv">
          {{ $t('wise.query.csvView') }}
        </v-tab>
      </v-tabs>
      <div v-if="resultTab === 'table'">
        <v-data-table
          density="compact"
          :items="searchResult"
          :headers="tableHeaders" />
      </div>
      <div v-if="resultTab === 'json'">
        <vue-json-pretty
          :data="searchResult"
          :show-line-number="true"
          :show-double-quotes="false" />
      </div>
      <div v-if="resultTab === 'csv'">
        <pre>{{ calcCSV() }}</pre>
      </div>
    </div> <!-- /tabbed view options -->

    <!-- no results -->
    <div v-else>
      <div class="vertical-center info-area mt-5">
        <div>
          <v-icon
            icon="mdi-magnify-minus"
            size="x-large" />
          {{ $t('wise.query.noResults') }}
          <span
            v-if="!sources.length"
            v-html="$t('wise.noSourcesHtml')" />
        </div>
      </div>
    </div> <!-- /no results -->
  </v-container>  <!-- /container -->
</template>

<script>
import { mapGetters } from 'vuex';
import VueJsonPretty from 'vue-json-pretty';

import WiseService from './wise.service.js';

let timeout;

export default {
  name: 'Query',
  components: {
    VueJsonPretty
  },
  data: function () {
    return {
      alertMessage: '',
      loading: false,
      hasMadeASearch: false,
      searchResult: [],
      tableHeaders: [],
      resultTab: 'table',
      searchTerm: this.$route.query.searchTerm || '',
      chosenType: this.$route.query.searchType || localStorage.getItem('search-type') || 'ip',
      chosenSource: this.$route.query.searchSource || localStorage.getItem('search-source') || '',
      sources: [],
      types: []
    };
  },
  mounted: function () {
    this.loadSourceOptions();
    this.loadTypeOptions();

    if (this.searchTerm) {
      this.debounceSearch();
    }
  },
  watch: {
    chosenSource: function () {
      this.loadTypeOptions();
    },
    '$route.query' (newParams) { // watch for url query changes and issue search
      let change = false;
      if (this.searchTerm !== newParams.searchTerm) {
        this.searchTerm = newParams.searchTerm || '';
        change = true;
      }
      if (this.chosenType !== newParams.searchType) {
        this.chosenType = newParams.searchType || 'ip';
        change = true;
      }
      if (this.chosenSource !== newParams.searchSource) {
        this.chosenSource = newParams.searchSource || '';
        change = true;
      }
      if (change) { this.debounceSearch(); }
    }
  },
  computed: {
    ...mapGetters(['getTheme'])
  },
  methods: {
    calcCSV: function () {
      let csv = '';
      if (!this.searchResult) { return csv; }

      this.searchResult.forEach((item, i) => {
        if (i === 0) {
          csv += Object.keys(item).join(',');
          csv += '\n';
        }

        csv += Object.values(item).join(',');
        csv += '\n';
      });

      return csv;
    },
    debounceSearch: function () {
      if (timeout) { clearTimeout(timeout); }
      timeout = setTimeout(() => {
        this.sendSearchQuery();
      }, 600);
    },
    loadSourceOptions: function () {
      WiseService.getSources()
        .then((data) => {
          this.alertMessage = '';
          this.sources = data;
        })
        .catch((error) => {
          this.alertMessage = error.text ||
            'Error fetching source options for wise.';
        });
    },
    loadTypeOptions: function () {
      WiseService.getTypes(this.chosenSource)
        .then((data) => {
          this.alertMessage = '';
          this.types = data;
          if (data.length >= 1 && !data.includes(this.chosenType)) {
            this.chosenType = data[0];
          }
        })
        .catch((error) => {
          this.alertMessage = error.text ||
            'Error fetching type options for wise.';
        });
    },
    sendSearchQuery: function () {
      if (!this.searchTerm) {
        if (this.hasMadeASearch) {
          this.searchResult = [];
          this.tableHeaders = [];
          this.hasMadeASearch = false;
        }
        return;
      }

      this.loading = true;

      if (this.$route.query.searchSource !== this.chosenSource ||
        this.$route.query.searchType !== this.chosenType ||
        this.$route.query.searchTerm !== this.searchTerm) {
        this.$router.push({
          query: {
            ...this.$route.query,
            searchSource: this.chosenSource,
            searchType: this.chosenType,
            searchTerm: this.searchTerm
          }
        });
      }

      // save chosen type and source in localstorage
      localStorage.setItem('search-type', this.chosenType);
      localStorage.setItem('search-source', this.chosenSource);

      this.hasMadeASearch = true;

      this.alertMessage = '';
      WiseService.search(this.chosenSource, this.chosenType, this.searchTerm)
        .then((data) => {
          this.alertMessage = '';
          setTimeout(() => {
            this.loading = false;
          }, 500);
          // this.loading = false;
          this.searchResult = data;
          if (data.length >= 1) {
            this.tableHeaders = Object.keys(data[0]).map(key => {
              return { title: key, key };
            });
          }
        })
        .catch((error) => {
          this.loading = false;
          this.alertMessage = error.text ||
            'Error getting search result for wise.';
        });
    },
    clear: function () {
      this.searchTerm = '';

      if (this.$route.query.searchTerm !== '') {
        this.$router.replace({
          query: {
            ...this.$route.query,
            searchTerm: ''
          }
        }).catch((err) => {
          console.log(err);
        });
      }
    }
  }
};
</script>

<style scoped>
.btn-clear-input {
  color: #555;
  background-color: #EEE;
  border-color: #CCC;
}
</style>
