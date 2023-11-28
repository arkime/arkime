<!--
Copyright Yahoo Inc.
SPDX-License-Identifier: Apache-2.0
-->
<template>
  <!-- container -->
  <div class="container-fluid">
    <div class="d-flex flex-row">
      <!-- source select -->
      <div class="form-group">
        <div class="input-group">
          <span class="input-group-prepend cursor-help"
            placement="topright"
            v-b-tooltip.hover
            title="Which source, as defined in the config, to fetch data from">
            <span class="input-group-text">
              Source
            </span>
          </span>
          <select class="form-control"
            v-model="chosenSource"
            @change="debounceSearch"
            tabindex="1">
            <option value="">
              Any
            </option>
            <option v-for="source in sources" :value="source" :key="source">
              {{ source }}
            </option>
          </select>
        </div>
      </div> <!-- /source select -->

      <!-- type select -->
      <div class="form-group ml-3">
        <div class="input-group">
          <span class="input-group-prepend cursor-help"
            placement="topright"
            v-b-tooltip.hover
            title="Which data type to target">
            <span class="input-group-text">
              Type
            </span>
          </span>
          <select class="form-control"
            @change="sendSearchQuery"
            v-model="chosenType"
            tabindex="2">
            <option v-for="type in types" :value="type" :key="type">
              {{ type }}
            </option>
          </select>
        </div>
      </div> <!-- /type select -->

      <!-- search -->
      <div class=" flex-grow-1 ml-3">
        <div class="input-group">
          <span class="input-group-prepend">
            <span class="input-group-text">
              <span v-if="!loading" class="fa fa-search fa-fw">
              </span>
              <span v-else class="fa fa-spinner fa-spin fa-fw">
              </span>
            </span>
          </span>
          <input type="text"
            tabindex="3"
            v-model="searchTerm"
            class="form-control"
            :placeholder="`Search ${chosenType} values for WISE data`"
            @input="debounceSearch"
            @keyup.enter="sendSearchQuery"
          />
          <span class="input-group-append">
            <button type="button"
              @click="clear"
              :disabled="!searchTerm"
              class="btn btn-outline-secondary btn-clear-input">
              <span class="fa fa-close">
              </span>
            </button>
          </span>
        </div>
      </div> <!-- /search -->
    </div>

    <Alert
      variant="alert-danger"
      :initialAlert="alertMessage"
      v-on:clear-initialAlert="alertMessage = ''"
    />

    <!-- empty search -->
    <div v-if="!hasMadeASearch">
      <div class="vertical-center info-area mt-5">
        <div>
          <span class="fa fa-3x fa-search">
          </span>
          Try adding a search query!
          <template v-if="!sources.length">
            <br>
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
          </template>
        </div>
      </div>
    </div> <!-- /empty search -->

    <!-- tabbed view options -->
    <b-tabs content-class="mt-3" v-else-if="searchResult.length > 0">
      <b-tab title="Table View" active>
        <b-table striped hover small borderless
          :dark="getTheme ==='dark'"
          :items="searchResult"
          :fields="tableFields">
        </b-table>
      </b-tab>

      <b-tab title="JSON View">
        <pre>{{JSON.stringify(searchResult, null, 2)}}</pre>
      </b-tab>

      <b-tab title="CSV View">
        <pre>{{calcCSV()}}</pre>
      </b-tab>
    </b-tabs> <!-- /tabbed view options -->

    <!-- no results -->
    <div v-else>
      <div class="vertical-center info-area mt-5">
        <div>
          <span class="fa fa-3x fa-search-minus">
          </span>
          No Results
          <template v-if="!sources.length">
            <br>
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
          </template>
        </div>
      </div>
    </div> <!-- /no results -->

  </div>  <!-- /container -->

</template>

<script>
import { mapGetters } from 'vuex';

import WiseService from './wise.service';
import Alert from './Alert';

let timeout;

export default {
  name: 'Query',
  components: {
    Alert
  },
  data: function () {
    return {
      alertMessage: '',
      loading: false,
      hasMadeASearch: false,
      searchResult: [],
      tableFields: [],
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
          this.tableFields = [];
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
            this.tableFields = Object.keys(data[0]).map(key => {
              return { key, sortable: true };
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
