<template>
  <div class="container-fluid">

    <!-- search -->
    <div class="fixed-top pl-2 pr-2 search-nav">
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
      </b-input-group>
    </div> <!-- /search -->

    <!-- page content -->
    <div class="margin-for-search">
      <!-- welcome -->
      <div class="whole-page-info container"
        v-if="!initialized && !error.length && !integrationError.length">
        <div class="well center-area">
          <h1 class="text-muted">
            <span class="fa fa-fw fa-rocket fa-2x" />
          </h1>
          <h1 class="text-warning display-4">
            Welcome to Cont3xt!
          </h1>
          <h4 class="text-success lead">
            Search for IPs, domains, URLs, emails, phone numbers, or hashes.
          </h4>
        </div>
      </div> <!-- /welcome -->

      <!-- search error -->
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
      </div> <!-- /search error -->

      <!-- integration error -->
      <div
        v-if="integrationError.length"
        class="mt-2 alert alert-danger">
        <span class="fa fa-exclamation-triangle" />&nbsp;
        Error fetching integrations. Viewing data for integrations will not work!
        <br>
        {{ integrationError }}
        <button
          type="button"
          @click="integrationError = ''"
          class="close cursor-pointer">
          <span>&times;</span>
        </button>
      </div> <!-- /integration error -->

      <!-- results -->
      <div v-if="searchTerm"
        class="row mt-2 results-container">
        <!-- itype results summary -->
        <div class="col-6 results-summary">
          <cont3xt-domain
            :data="results"
            v-if="searchItype === 'domain'"
          />
          <cont3xt-ip
            :data="results"
            v-else-if="searchItype === 'ip'"
          />
          <cont3xt-url
            :data="results"
            :query="searchTerm"
            v-else-if="searchItype === 'url'"
          />
          <cont3xt-email
            :data="results"
            :query="searchTerm"
            v-else-if="searchItype === 'email'"
          />
          <cont3xt-hash
            :data="results"
            v-else-if="searchItype === 'hash'"
          />
          <cont3xt-phone
            :data="results"
            :query="searchTerm"
            v-else-if="searchItype === 'phone'"
          />
          <div v-else-if="searchItype">
            <h3 class="text-warning">
              No display for {{ searchItype }}
            </h3>
            <pre class="text-info"><code>{{ results }}</code></pre>
          </div>
        </div> <!-- /itype results summary -->
        <!-- integration results -->
        <div
          @scroll="handleScroll"
          ref="resultsIntegration"
          class="col-6 results-integration">
          <b-overlay
            no-center
            rounded="sm"
            blur="0.2rem"
            opacity="0.9"
            variant="transparent"
            :show="getWaitRendering || getRendering">
            <integration-card />
            <template #overlay>
              <div class="overlay-loading">
                <span class="fa fa-circle-o-notch fa-spin fa-2x" />
                <p>Rendering data...</p>
              </div>
            </template>
          </b-overlay>
          <b-button
            size="sm"
            @click="toTop"
            title="Go to top"
            class="to-top-btn"
            variant="btn-link"
            v-show="scrollPx > 100">
            <span class="fa fa-lg fa-arrow-circle-up" />
          </b-button>
        </div> <!-- /integration results -->
      </div> <!-- /results -->

    </div> <!-- /page content -->
  </div>
</template>

<script>
import { mapGetters } from 'vuex';

import Cont3xtIp from '@/components/itypes/IP';
import Cont3xtUrl from '@/components/itypes/URL';
import Cont3xtHash from '@/components/itypes/Hash';
import Cont3xtEmail from '@/components/itypes/Email';
import Cont3xtPhone from '@/components/itypes/Phone';
import Cont3xtDomain from '@/components/itypes/Domain';
import Cont3xtService from '@/components/services/Cont3xtService';
import IntegrationCard from '@/components/integrations/IntegrationCard';

export default {
  name: 'Cont3xt',
  components: {
    Cont3xtIp,
    Cont3xtUrl,
    Cont3xtHash,
    Cont3xtEmail,
    Cont3xtPhone,
    Cont3xtDomain,
    IntegrationCard
  },
  data () {
    return {
      error: '',
      results: {},
      scrollPx: 0,
      searchItype: '',
      initialized: false,
      integrationError: '',
      searchTerm: this.$route.query.q || ''
    };
  },
  computed: {
    ...mapGetters(['getRendering', 'getWaitRendering', 'getIntegrationData']),
    loading: {
      get () { return this.$store.state.loading; },
      set (val) { this.$store.commit('SET_LOADING', val); }
    },
    displayIntegration () {
      return this.$store.state.displayIntegration;
    }
  },
  watch: {
    displayIntegration (newIntegration) {
      this.$store.commit('SET_INTEGRATION_DATA', {});
      this.$store.commit('SET_RENDERING_CARD', true);
      // need wait rendering to tell the card that we aren't rendering yet
      // or else the data will be stale when it updates the integration type
      this.$store.commit('SET_WAIT_RENDERING', true);
      setTimeout(() => { // need timeout for SET_RENDERING_CARD to take effect
        const { itype, source, value } = newIntegration;
        for (const data of this.results[itype][source]) {
          if (data._query === value) {
            this.$store.commit('SET_INTEGRATION_DATA', data);
          }
        }
        this.$store.commit('SET_WAIT_RENDERING', false);
      }, 100);
    }
  },
  mounted () {
    this.getIntegrations();
    if (this.searchTerm) {
      this.search();
    }
  },
  methods: {
    /* page functions ------------------------------------------------------ */
    handleScroll (e) {
      this.scrollPx = e.target.scrollTop;
    },
    toTop () {
      this.$refs.resultsIntegration.scrollTo({
        top: 0,
        behavior: 'smooth'
      });
    },
    search () {
      this.error = '';
      this.results = {};
      this.searchItype = '';
      this.initialized = true;
      this.$store.commit('RESET_LOADING');
      this.$store.commit('SET_INTEGRATION_DATA', {});

      let failed = 0;

      if (this.$route.query.q !== this.searchTerm) {
        this.$router.push({
          query: {
            ...this.$route.query,
            q: this.searchTerm
          }
        });
      }

      Cont3xtService.search(this.searchTerm).subscribe({
        next: (data) => {
          if (data.itype && !this.searchItype) {
            // determine the search type and save the search term
            // based of the first itype seen
            this.searchItype = data.itype;
          }

          if (data.itype && data.name) { // add the data to the page per itype
            if (!this.results[data.itype]) {
              this.$set(this.results, data.itype, {});
            }
            if (!this.results[data.itype][data.name]) {
              this.$set(this.results[data.itype], data.name, []);
            }
            if (!this.results[data.itype]._query) {
              this.$set(this.results[data.itype], '_query', data.query);
            }
            this.results[data.itype][data.name].push({
              data: data.data,
              _query: data.query
            });
          }

          if (data.sent && data.total) { // update the progress bar
            failed = data.failed ? ++failed : failed;
            this.loading = {
              failed: failed,
              total: data.total,
              received: data.sent,
              failure: data.failed && data.name ? data.name : null
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
    },
    /* helpers ------------------------------------------------------------- */
    getIntegrations () {
      // NOTE: don't need to do anything with the data (the store does it)
      Cont3xtService.getIntegrations().catch((err) => {
        this.integrationError = err;
      });
    }
  },
  beforeDestroy () {
    this.$store.commit('RESET_LOADING');
    this.$store.commit('SET_INTEGRATION_DATA', {});
  }
};
</script>

<style scoped>
.search-nav {
  margin-top: 60px !important;
  padding-top: 16px;
  background-color: #FFF;
}
body.dark .search-nav {
  background-color: #222;
}

.margin-for-search {
  margin-top: 136px;
}

/* side by side scrolling results containers */
.results-container {
  height: 100%;
  overflow: hidden;
}
.results-summary {
  height: calc(100vh - 136px);
  overflow: scroll;
}
.results-integration {
  height: calc(100vh - 136px);
  overflow: scroll;
}

/* scroll to top btn for integration results */
.to-top-btn, .to-top-btn:hover {
  z-index: 99;
  right: 2px;
  bottom: 0px;
  position: fixed;
  color: var(--info);
}
</style>
