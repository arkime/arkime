<template>
  <div class="container-fluid">
    <!-- integration selection panel -->
    <IntegrationPanel />
    <!-- view create form -->
    <create-view-modal />
    <!-- page content -->
    <div class="main-content"
      :class="{'with-sidebar': getSidebarKeepOpen}">
      <!-- search -->
      <div class="fixed-top search-nav d-flex justify-content-between">
        <b-input-group class="flex-grow-1 mr-2">
          <template #prepend>
            <b-input-group-text>
              <span class="fa fa-search" />
            </b-input-group-text>
          </template>
          <b-form-input
            tabindex="0"
            v-focus="true"
            v-model="searchTerm"
            @keydown.enter="search"
            placeholder="Indicators"
          />
          <template #append>
            <b-button
              tabindex="0"
              @click="clear"
              :disabled="!searchTerm"
              title="Remove the search text">
              <span class="fa fa-close" />
            </b-button>
          </template>
        </b-input-group>
        <b-button
          tabindex="-1"
          @click="search"
          variant="success"
          class="mr-1 search-btn">
          Get Cont3xt
        </b-button>
        <ViewSelector
          :no-caret="true"
          :show-selected-view="true">
          <template #title>
            <span class="fa fa-eye" />
          </template>
        </ViewSelector>
        <b-dropdown
          class="ml-1"
          tabindex="-1"
          variant="info">
          <b-dropdown-item
            :active="skipCache"
            @click="skipCache = !skipCache"
            v-b-tooltip.hover.left="skipCache ? 'Ignorning cache (click to use cache)' : 'Using cache (click to ignore cache)'">
            <span class="fa fa-database fa-fw mr-1" />
            Skip Cache
          </b-dropdown-item>
          <b-dropdown-item
            @click="generateReport"
            :class="{'disabled':!searchComplete}"
            v-b-tooltip.hover.left="'Download a report of this result.'">
            <span class="fa fa-file-text fa-fw mr-1" />
            Download Report
          </b-dropdown-item>
          <b-dropdown-item
            @click="shareLink"
            v-b-tooltip.hover.left="'Copy share link to clipboard'">
            <span class="fa fa-share-alt fa-fw mr-1" />
            Copy Share Link
          </b-dropdown-item>
        </b-dropdown>
      </div> <!-- /search -->

      <div class="margin-for-search cont3xt-content">
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
            <h4 v-if="!searchTerm"
              class="text-success lead">
              Search for IPs, domains, URLs, emails, phone numbers, or hashes.
            </h4>
            <h4 v-else
              class="text-success lead">
              <strong>Hit enter to issue your search!</strong>
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
            tabindex="-1"
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

        <!-- link inputs -->
        <b-form inline
          v-if="searchTerm && initialized"
          class="w-50 d-flex align-items-start link-inputs">
          <b-input-group
            size="xs"
            class="mr-2 mb-1">
            <template #prepend>
              <b-input-group-text>
                Start
              </b-input-group-text>
            </template>
            <b-form-input
              type="text"
              tabindex="0"
              v-model="startDate"
              style="width:152px"
              placeholder="Start Date"
              @change="updateStopStart('startDate')"
            />
          </b-input-group>
          <b-input-group
            size="xs"
            class="mr-2 mb-1">
            <template #prepend>
              <b-input-group-text>
                Stop
              </b-input-group-text>
            </template>
            <b-form-input
              type="text"
              tabindex="0"
              v-model="stopDate"
              style="width:152px"
              placeholder="Stop Date"
              @change="updateStopStart('stopDate')"
            />
          </b-input-group>
          <span class="fa fa-lg fa-question-circle cursor-help mt-1"
            v-b-tooltip.hover.html="linkPlaceholderTip"
          />
          <span class="pl-2">
            {{ numDays }} days | {{ numHours }} hours
          </span>
        </b-form> <!-- /link inputs -->

        <!-- results -->
        <template v-if="searchTerm">
          <!-- itype results summary -->
          <div class="results-container results-summary">
            <div>
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
              <hr v-if="searchTerm && initialized">
              <!-- link groups error -->
              <b-alert
                variant="danger"
                :show="!!getLinkGroupsError.length">
                {{ getLinkGroupsError }}
              </b-alert>
              <!-- link search -->
              <div v-if="searchItype && initialized">
                <b-input-group size="sm">
                  <template #prepend>
                    <b-input-group-text>
                      <span class="fa fa-search" />
                    </b-input-group-text>
                  </template>
                  <b-form-input
                    tabindex="0"
                    debounce="400"
                    v-model="linkSearchTerm"
                    placeholder="Search links below"
                  />
                </b-input-group> <!-- /link search -->
                <!-- link groups -->
                <div class="d-flex flex-wrap align-items-start link-group-cards-wrapper">
                  <template v-for="(linkGroup, index) in getLinkGroups">
                    <reorder-list
                      :index="index"
                      @update="updateList"
                      :key="linkGroup._id"
                      :list="getLinkGroups"
                      class="w-50 p-2 link-group"
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
                          :hide-links="hideLinks[linkGroup._id]"
                        />
                      </template>
                    </reorder-list>
                  </template>
                </div> <!-- /link groups -->
              </div>
            </div>
          </div> <!-- /itype results summary -->
          <!-- integration results -->
          <div class="results-container results-integration pull-right">
            <div
              @scroll="handleScroll"
              ref="resultsIntegration">
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
            </div>
          </div> <!-- /integration results -->
        </template> <!-- /results -->

      </div>
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
import ViewSelector from '@/components/views/ViewSelector';
import UserService from '@/components/services/UserService';
import LinkGroupCard from '@/components/links/LinkGroupCard';
import CreateViewModal from '@/components/views/CreateViewModal';
import Cont3xtService from '@/components/services/Cont3xtService';
import IntegrationCard from '@/components/integrations/IntegrationCard';
import IntegrationPanel from '@/components/integrations/IntegrationPanel';

export default {
  name: 'Cont3xt',
  components: {
    Cont3xtIp,
    Cont3xtUrl,
    ReorderList,
    Cont3xtHash,
    Cont3xtText,
    Cont3xtEmail,
    Cont3xtPhone,
    ViewSelector,
    Cont3xtDomain,
    LinkGroupCard,
    CreateViewModal,
    IntegrationCard,
    IntegrationPanel
  },
  directives: { Focus },
  data () {
    return {
      error: '',
      numDays: 7, // 1 week
      numHours: 7 * 24, // 1 week
      startDate: new Date(new Date().getTime() - (3600000 * 24 * 7)).toISOString().slice(0, -5) + 'Z', // 1 week ago
      stopDate: new Date().toISOString().slice(0, -5) + 'Z', // now
      results: {},
      scrollPx: 0,
      searchItype: '',
      initialized: false,
      searchTerm: this.$route.query.q ? this.$route.query.q : (this.$route.query.b ? window.atob(this.$route.query.b) : ''),
      skipCache: false,
      searchComplete: false,
      linkSearchTerm: this.$route.query.linkSearch || '',
      hideLinks: {},
      linkPlaceholderTip: {
        title: 'These values are used to fill in <a href="help#linkgroups" class="no-decoration">link placeholders</a>.<br>' +
          'Try using <a href="help#general" class="no-decoration">relative times</a> like -5d or -1h.'
      }
    };
  },
  mounted () {
    // set the stop/start date to the query parameters
    if (this.$route.query.stopDate) {
      this.stopDate = this.$route.query.stopDate;
      this.updateStopStart('stopDate');
    }
    if (this.$route.query.startDate) {
      this.startDate = this.$route.query.startDate;
      this.updateStopStart('startDate');
    }
  },
  computed: {
    ...mapGetters([
      'getRendering', 'getWaitRendering', 'getIntegrationData',
      'getIntegrationsError', 'getLinkGroupsError', 'getLinkGroups',
      'getSidebarKeepOpen'
    ]),
    loading: {
      get () { return this.$store.state.loading; },
      set (val) { this.$store.commit('SET_LOADING', val); }
    },
    displayIntegration () {
      return this.$store.state.displayIntegration;
    },
    collapsedLinkGroups () {
      return this.$store.state.collapsedLinkGroups;
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
    },
    linkSearchTerm (searchTerm) {
      this.hideLinks = {};

      if (this.$route.query.linkSearch !== searchTerm) {
        this.$router.push({
          query: {
            ...this.$route.query,
            linkSearch: searchTerm
          }
        });
      }

      this.filterLinks(searchTerm);
    },
    collapsedLinkGroups: {
      deep: true,
      handler () {
        this.arrangeLinkGroups();
      }
    }
  },
  methods: {
    /* page functions ------------------------------------------------------ */
    clear () {
      this.searchTerm = '';
    },
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
      this.searchComplete = false;
      this.$store.commit('RESET_LOADING');
      this.$store.commit('SET_INTEGRATION_DATA', {});

      let failed = 0;

      // only match on b because we remove the q param
      if (!this.$route.query.b ||
        (this.$route.query.b && window.atob(this.$route.query.b) !== this.searchTerm)
      ) {
        this.$router.push({
          query: {
            ...this.$route.query,
            b: window.btoa(this.searchTerm),
            q: undefined // remove the q param
          }
        });
      }

      Cont3xtService.search({ searchTerm: this.searchTerm, skipCache: this.skipCache }).subscribe({
        next: (data) => {
          if (data.itype && !this.searchItype) {
            // determine the search type and save the search term
            // based of the first itype seen
            this.searchItype = data.itype;
            this.filterLinks(this.linkSearchTerm);
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
          this.searchComplete = true;
          setTimeout(() => { // clear the loading progress bar after 2 seconds
            if (!this.loading.failed && this.loading.total === this.loading.received) {
              this.$store.commit('RESET_LOADING');
            }
          }, 2000);
        }
      });
    },
    updateStopStart (updated) {
      let stopMs = new Date(this.stopDate).getTime();
      let startMs = new Date(this.startDate).getTime();

      // test for relative times
      if (isNaN(stopMs)) {
        stopMs = this.$options.filters.parseSeconds(this.stopDate) * 1000;
      }
      if (isNaN(startMs)) {
        startMs = this.$options.filters.parseSeconds(this.startDate) * 1000;
      }

      // can't do anyting if we can't calculate the date ms
      if (isNaN(stopMs) || isNaN(startMs)) { return; }

      // update the query params with the updated value
      if (this.$route.query[updated] !== this[updated]) {
        const query = { ...this.$route.query };
        query[updated] = this[updated];
        this.$router.push({ query });
      }

      const days = (stopMs - startMs) / (3600000 * 24);

      switch (updated) {
      case 'stopDate':
        this.numDays = Math.round(days);
        this.numHours = Math.round(days * 24);
        this.startDate = new Date(stopMs - (3600000 * 24 * days)).toISOString().slice(0, -5) + 'Z';
        this.stopDate = new Date(stopMs).toISOString().slice(0, -5) + 'Z';
        break;
      case 'startDate':
        this.numDays = Math.round(days);
        this.numHours = Math.round(days * 24);
        this.startDate = new Date(startMs).toISOString().slice(0, -5) + 'Z';
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
    shareLink () {
      this.$copyText(window.location.href);
    },
    generateReport () {
      if (!this.searchComplete) { return; }

      const a = document.createElement('a');
      const file = new Blob([JSON.stringify(this.results, false, 2)], { type: 'application/json' });
      a.href = URL.createObjectURL(file);
      a.download = `${new Date().toISOString()}_${this.searchTerm}.json`;
      a.click();
      URL.revokeObjectURL(a.href);
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
    },
    filterLinks (searchTerm) {
      if (!searchTerm) { return; }

      const query = searchTerm.toLowerCase();

      for (const group of this.getLinkGroups) {
        this.hideLinks[group._id] = {};
        for (let i = 0; i < group.links.length; i++) {
          const match = group.links[i].name.toString().toLowerCase().match(query);
          if (!match || match.length <= 0) {
            this.hideLinks[group._id][i] = true;
          }
        }
      }

      this.arrangeLinkGroups();
    },
    arrangeLinkGroups () {
      this.$nextTick(() => { // wait for render
        const lgElements = document.getElementsByClassName('link-group');

        for (let i = 0, len = lgElements.length; i < len; i++) {
          let delta = 0;
          const even = i % 2 === 0;

          if (i < 2) { continue; }

          const target = lgElements[i];

          if (!even) { // right side
            let x = i;
            let leftHeight = 0;
            let rightHeight = 0;
            while (x > -1) { // sum heights of upper sibling elements
              if (lgElements[x - 3]) {
                leftHeight += lgElements[x - 3].clientHeight;
                rightHeight += lgElements[x - 2].clientHeight;
              } else {
                break;
              }
              x = x - 2;
            }
            if (leftHeight > rightHeight) {
              delta = Math.abs(leftHeight - rightHeight);
            }
          } else { // left side
            let x = i;
            let leftHeight = 0;
            let rightHeight = 0;
            while (x > -1) {
              if (lgElements[x - 2]) { // sum heights of upper sibling elements
                leftHeight += lgElements[x - 2].clientHeight;
                rightHeight += lgElements[x - 1].clientHeight;
              } else {
                break;
              }
              x = x - 2;
            }
            if (rightHeight > leftHeight) {
              delta = Math.abs(rightHeight - leftHeight);
            }
          }

          target.style = `margin-top: -${delta}px`;
        }
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
.search-btn {
  width: 148px;
}

.search-nav {
  padding: 16px 16px 0 16px;
  background-color: #FFF;
  margin-top: 60px !important;
}
body.dark .search-nav {
  background-color: #222;
}

.margin-for-search {
  margin-top: 126px;
}
.cont3xt-content {
  overflow: hidden;
  margin-left: -7px;
  margin-right: -7px;
  height: calc(100vh - 126px); /* total height - margin for search */
}

.link-inputs {
  padding-left: 10px;
}

/* side by side scrolling results containers */
.results-container {
  width: 50%;
  height: 100%;
  overflow: hidden;
  display: inline-block;
}
.results-container.results-integration {
  margin-top: -28px;
}
.results-container.results-summary > div {
  /* total height - margin for search - height of link inputs */
  height: calc(100vh - 153px);
  overflow-y: auto;
  overflow-x: hidden;
  padding-left: 8px;
  padding-right: 0.5rem;
}
.results-container.results-integration > div {
  height: calc(100vh - 126px); /* total height - margin for search */
  overflow-y: auto;
  overflow-x: hidden;
  padding-right: 8px;
  padding-left: 0.5rem;
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
  margin-right: -1.25rem !important;
  margin-left: -0.5rem !important;
}

.link-group-card-handle {
  top: 1rem;
  z-index: 10;
  float: right;
  right: 1.5rem;
  position: relative;
}

.link-group {
  transition: margin-top 0.5s ease-out;
}
</style>

<style>
/* scroll the integration select dropdown */
.integration-select > ul {
  width: 200px;
  overflow: scroll;
  max-height: 300px;
}
/* condense the dropdown items */
.integration-select .dropdown-item,
.integration-select .custom-control {
  font-size: 0.8rem;
  padding: 0.1rem 0.5rem;
}
</style>
