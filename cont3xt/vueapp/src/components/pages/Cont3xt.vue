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
          v-focus="true"
          v-model="searchTerm"
          @keydown.enter="search"
          placeholder="Indicators"
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
        v-if="!initialized && !error.length && !getIntegrationsError.length">
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
        v-if="getIntegrationsError.length"
        class="mt-2 alert alert-danger">
        <span class="fa fa-exclamation-triangle" />&nbsp;
        Error fetching integrations. Viewing data for integrations will not work!
        <br>
        {{ getIntegrationsError }}
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
          <cont3xt-text
            :data="results"
            :query="searchTerm"
            v-else-if="searchItype === 'text'"
          />
          <div v-else-if="searchItype">
            <h3 class="text-warning">
              No display for {{ searchItype }}
            </h3>
            <pre class="text-info"><code>{{ results }}</code></pre>
          </div>
          <hr>
          <!-- link groups -->
          <b-alert
            variant="danger"
            :show="!!getLinkGroupsError.length">
            {{ getLinkGroupsError }}
          </b-alert>
          <b-form inline>
            <b-input-group
              size="sm"
              class="mr-2 mb-1">
              <template #prepend>
                <b-input-group-text>
                  Days
                </b-input-group-text>
              </template>
              <b-form-input
                type="number"
                debounce="400"
                :value="numDays"
                style="width:60px"
                placeholder="Number of Days"
                @input="updateVars('numDays', $event)"
              />
            </b-input-group>
            <b-input-group
              size="sm"
              class="mr-2 mb-1">
              <template #prepend>
                <b-input-group-text>
                  Hours
                </b-input-group-text>
              </template>
              <b-form-input
                type="number"
                debounce="400"
                :value="numHours"
                style="width:70px"
                placeholder="Number of Hours"
                @input="updateVars('numHours', $event)"
              />
            </b-input-group>
            <b-input-group
              size="sm"
              class="mr-2 mb-1">
              <template #prepend>
                <b-input-group-text>
                  Start Date
                </b-input-group-text>
              </template>
              <b-form-input
                type="text"
                debounce="400"
                :value="startDate"
                style="width:165px"
                placeholder="Start Date"
                @input="updateVars('startDate', $event)"
              />
            </b-input-group>
            <b-input-group
              size="sm"
              class="mr-2 mb-1">
              <template #prepend>
                <b-input-group-text>
                  Stop Date
                </b-input-group-text>
              </template>
              <b-form-input
                type="text"
                debounce="400"
                :value="stopDate"
                style="width:165px"
                placeholder="Stop Date"
                @input="updateVars('stopDate', $event)"
              />
            </b-input-group>
          </b-form>
          <!-- link groups -->
          <div v-if="searchItype"
            class="d-flex flex-wrap link-group-cards-wrapper">
            <template v-for="(linkGroup, index) in getLinkGroups">
              <reorder-list
                :index="index"
                class="w-50 p-2"
                @update="updateList"
                :key="linkGroup._id"
                :list="getLinkGroups"
                v-if="hasLinksWithItype(linkGroup)">
                <template slot="handle">
                  <span class="fa fa-bars d-inline link-group-card-handle" />
                </template>
                <template slot="default">
                  <link-group-card
                    :query="searchTerm"
                    :num-days="numDays"
                    :itype="searchItype"
                    :num-hours="numHours"
                    :stop-date="stopDate"
                    :start-date="startDate"
                    :link-group-index="index"
                    v-if="getLinkGroups.length"
                  />
                </template>
              </reorder-list>
            </template>
          </div> <!-- /link groups -->
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
            <integration-card
              @update-results="updateData"
            />
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

import Focus from '@/utils/Focus';
import ReorderList from '@/utils/ReorderList';
import Cont3xtIp from '@/components/itypes/IP';
import Cont3xtUrl from '@/components/itypes/URL';
import Cont3xtHash from '@/components/itypes/Hash';
import Cont3xtText from '@/components/itypes/Text';
import Cont3xtEmail from '@/components/itypes/Email';
import Cont3xtPhone from '@/components/itypes/Phone';
import Cont3xtDomain from '@/components/itypes/Domain';
import UserService from '@/components/services/UserService';
import LinkGroupCard from '@/components/links/LinkGroupCard';
import Cont3xtService from '@/components/services/Cont3xtService';
import IntegrationCard from '@/components/integrations/IntegrationCard';

export default {
  name: 'Cont3xt',
  components: {
    Cont3xtIp,
    Cont3xtUrl,
    Cont3xtHash,
    Cont3xtText,
    Cont3xtEmail,
    Cont3xtPhone,
    Cont3xtDomain,
    LinkGroupCard,
    IntegrationCard,
    ReorderList
  },
  directives: { Focus },
  data () {
    return {
      error: '',
      numDays: 7, // 1 week
      startDate: new Date(new Date().getTime() - (3600000 * 24 * 7)).toISOString().slice(0, -5) + 'Z', // 7 days ago
      stopDate: new Date().toISOString().slice(0, -5) + 'Z', // now
      numHours: 7 * 24, // 1 week
      results: {},
      scrollPx: 0,
      searchItype: '',
      initialized: false,
      searchTerm: this.$route.query.q || ''
    };
  },
  computed: {
    ...mapGetters([
      'getRendering', 'getWaitRendering', 'getIntegrationData',
      'getIntegrationsError', 'getLinkGroupsError', 'getLinkGroups'
    ]),
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
    if (this.searchTerm) {
      this.search();
    }
  },
  methods: {
    /* page functions ------------------------------------------------------ */
    updateList ({ list }) {
      const ids = [];
      for (const group of list) {
        ids.push(group._id);
      }

      UserService.setUserSettings({ linkGroup: { order: ids } }).then((response) => {
        this.$store.commit('SET_LINK_GROUPS', list); // update list order
      }).catch((err) => {
        this.$store.commit('SET_LINK_GROUPS_ERROR', err);
      });
    },
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
    updateVars (updated, newVal) {
      this[updated] = newVal;

      const stopMs = new Date(this.stopDate).getTime();
      const startMs = new Date(this.startDate).getTime();

      if (isNaN(stopMs) || isNaN(stopMs)) {
        return;
      }

      switch (updated) {
      case 'numDays':
        this.numHours = this.numDays * 24;
        this.stopDate = new Date().toISOString().slice(0, -5) + 'Z';
        this.startDate = new Date(new Date().getTime() - (3600000 * 24 * this.numDays)).toISOString().slice(0, -5) + 'Z';
        break;
      case 'numHours':
        this.numDays = this.numHours / 24;
        this.stopDate = new Date().toISOString();
        this.startDate = new Date(new Date().getTime() - (3600000 * 24 * this.numDays)).toISOString().slice(0, -5) + 'Z';
        break;
      case 'stopDate':
        this.numDays = (stopMs - startMs) / (3600000 * 24);
        this.numHours = this.numDays * 24;
        this.startDate = new Date(new Date(this.stopDate).getTime() - (3600000 * 24 * this.numDays)).toISOString().slice(0, -5) + 'Z';
        break;
      case 'startDate':
        this.numDays = Math.floor((stopMs - startMs) / (3600000 * 24));
        this.numHours = this.numDays * 24;
        break;
      }
    },
    hasLinksWithItype (linkGroup) {
      for (const link of linkGroup.links) {
        if (link.itypes.indexOf(this.searchItype) > -1) {
          return true;
        }
      }
      return false;
    },
    /* helpers ------------------------------------------------------------- */
    updateData ({ itype, source, value, data }) {
      if (this.results[itype] && this.results[itype][source]) {
        for (const item of this.results[itype][source]) {
          if (item._query === value) {
            item.data = data.data;
          }
        }
      }
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

.link-group-cards-wrapper {
  margin-left: -0.5rem !important;
  margin-right: -0.5rem !important;
}

.link-group-card-handle {
  top: 1rem;
  z-index: 10;
  float: right;
  right: 1.5rem;
  position: relative;
}
</style>
