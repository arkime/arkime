<template>
  <!-- container -->
  <div class="container-fluid">
    <Error :initialError="error" v-on:clear-initialError="error = ''"/>

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
              <span v-if="!loading" class="fa fa-search">
              </span>
              <span v-else class="spinner-border spinner-border-sm">
              </span>
            </span>
          </span>
          <input type="text"
            tabindex="3"
            v-model="searchTerm"
            class="form-control"
            placeholder="Search wise data"
            @input="debounceSearch"
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

    <!-- empty search -->
    <div v-if="!hasMadeASearch">
      <div class="vertical-center info-area mt-5">
        <div>
          <span class="fa fa-3x fa-search">
          </span>
          Try adding a search query!
        </div>
      </div>
    </div> <!-- /empty search -->

    <!-- tabbed view options -->
    <b-tabs content-class="mt-3" v-else-if="searchResult.length > 0">
      <b-tab title="Table View" active>
        <b-table striped hover small borderless :items="searchResult" :fields="tableFields"></b-table>
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
        </div>
      </div>
    </div> <!-- /no results -->

  </div>  <!-- /container -->

</template>

<script>
import WiseService from './wise.service';
import Error from './Error';

let timeout;

export default {
  name: 'Query',
  components: {
    Error
  },
  data: function () {
    return {
      error: '',
      loading: false,
      hasMadeASearch: false,
      searchTerm: '',
      searchResult: [],
      tableFields: [],
      chosenSource: '',
      chosenType: '',
      sources: [],
      types: []
    };
  },
  computed: {
    // loggedIn: function () {
    //   return this.$store.state.loggedIn;
    // }
  },
  watch: {
    chosenSource: function () {
      this.loadTypeOptions();
    }
  },
  mounted: function () {
    this.loadSourceOptions();
    this.loadTypeOptions();

    if (this.$route.query.searchTerm) {
      this.chosenSource = this.$route.query.searchSource;
      this.chosenType = this.$route.query.searchType;
      this.searchTerm = this.$route.query.searchTerm;
      this.debounceSearch();
    }
  },
  methods: {
    calcCSV: function () {
      let csv = '';
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
      }, 400);
    },
    loadSourceOptions: function () {
      WiseService.getSources()
        .then((data) => {
          this.error = '';
          this.sources = data;
        })
        .catch((error) => {
          this.error = error.text ||
            `Error fetching source options for wise.`;
        });
    },
    loadTypeOptions: function () {
      WiseService.getTypes(this.chosenSource)
        .then((data) => {
          this.error = '';
          this.types = data;
          if (data.length >= 1 && !data.includes(this.chosenType)) {
            this.chosenType = data[0];
          }
        })
        .catch((error) => {
          this.error = error.text ||
            `Error fetching type options for wise.`;
        });
    },
    sendSearchQuery: function () {
      if (!this.searchTerm) {
        this.searchResult = [];
        this.tableFields = [];
        this.hasMadeASearch = false;
        return;
      }

      this.loading = true;

      this.$router.push({
        query: {
          ...this.$route.query,
          searchSource: this.chosenSource,
          searchType: this.chosenType,
          searchTerm: this.searchTerm
        }
      }).catch((err) => {
        console.log(err);
      });

      this.hasMadeASearch = true;

      this.error = '';
      WiseService.search(this.chosenSource, this.chosenType, this.searchTerm)
        .then((data) => {
          this.error = '';
          this.loading = false;
          this.searchResult = data;
          if (data.length >= 1) {
            this.tableFields = Object.keys(data[0]).map(key => {
              return { key: key, sortable: true };
            });
          }
        })
        .catch((error) => {
          this.loading = false;
          this.error = error.text ||
            `Error getting search result for wise.`;
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
