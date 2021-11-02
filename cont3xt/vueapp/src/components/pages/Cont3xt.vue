<template>
  <div class="mr-2 ml-2">

    <!-- search -->
    <b-input-group>
      <template #prepend>
        <b-input-group-text>
          <span class="fa fa-search" />
        </b-input-group-text>
      </template>
      <b-form-input
        v-model="searchTerm"
        placeholder="Search"
        @keydown.enter="search"
      />
      <b-input-group-append>
        <b-button
          @click="search"
          variant="success">
          Get Cont3xt
        </b-button>
      </b-input-group-append>
    </b-input-group> <!-- /search -->

    <!-- page error -->
    <div
      v-if="error"
      class="mt-2 alert alert-danger">
      <span class="fa fa-exclamation-triangle" />&nbsp;
      {{ error }}
      <button
        type="button"
        @click="error = false"
        class="close cursor-pointer">
        <span>&times;</span>
      </button>
    </div> <!-- /page error -->

    <!-- results -->
    <div
      v-if="results"
      class="row mt-2">
      <div class="col-6">
        <cont3xt-domain
          :data="results.domain"
          @integration="displayIntegration"
          v-if="searchItype === 'domain' && results.domain"
        />
      </div>
      <div class="col-6">
        <pre v-if="Object.keys(integrationView).length">{{ integrationView }}</pre>
      </div>
    </div> <!-- /results -->

  </div>
</template>

<script>
import Cont3xtService from '@/components/services/Cont3xtService';
import Cont3xtDomain from '@/components/itypes/Domain';

export default {
  name: 'Cont3xt',
  components: {
    Cont3xtDomain
  },
  data () {
    return {
      error: '',
      results: {},
      searchItype: '',
      integrationView: {},
      searchTerm: this.$route.query.q || ''
    };
  },
  computed: {
    loading: {
      get () { return this.$store.state.loading; },
      set (val) { this.$store.commit('SET_LOADING', val); }
    }
  },
  mounted () {
    if (this.searchTerm) {
      this.search();
    }
  },
  methods: {
    displayIntegration ({ itype, source }) {
      this.integrationView = this.results[itype][source].data;
    },
    search () {
      this.error = '';
      this.results = {};
      this.searchItype = '';
      this.$store.commit('RESET_LOADING');
      let failed = 0;

      Cont3xtService.search(this.searchTerm).subscribe({
        next: (data) => {
          if (data.itype && !this.searchItype) {
            // determine the search type and save the search term
            // based of the first itype seen
            this.searchItype = data.itype;
          }

          if (data.itype && data.name) { // add the data to the page per itype
            if (!this.results[data.itype]) {
              this.$set(this.results, data.itype, { _query: data.query });
            }
            this.$set(this.results[data.itype], data.name, data);
          }

          if (data.sent && data.total) { // update the progress bar
            failed = data.failed ? ++failed : failed;
            this.loading = {
              failed: failed,
              total: data.total,
              received: data.sent
            };
          }

          if (data.finished) { // we finished receiving results
            const leftover = this.loading.total - this.loading.failed - this.loading.received;
            if (leftover) {
              this.loading = { // complete the progress bar
                received: this.loading.received + leftover
              };
            }
          }
        },
        error: (e) => {
          this.error = e;
        },
        complete: () => {
          setTimeout(() => { // clear the loading progress bar after 2 seconds
            if (!this.loading.failed && this.loading.total === this.loading.received) {
              this.$store.commit('RESET_LOADING');
            }
          }, 2000);
        }
      });
    }
  }
};
</script>
