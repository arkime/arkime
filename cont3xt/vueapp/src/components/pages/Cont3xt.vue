<template>
  <div class="d-flex flex-row flex-grow-1 no-overflow-x">
    <!-- view create form -->
    <create-view-modal />
    <!-- integration selection panel -->
    <IntegrationPanel />
    <!-- page content -->
    <div class="flex-grow-1 d-flex flex-column">
      <!-- search -->
      <div class="d-flex justify-content-center mt-2 mx-3">
        <div class="w-100 pb-1 d-flex justify-content-between">
          <!--    tag input      -->
          <b-input-group style="max-width: 150px" class="mr-2">
            <b-form-input
                type="text"
                tabindex="0"
                ref="tagInput"
                :placeholder="`Tags${tags.length ? ` (${tags.length})` : ''}`"
                @keydown.enter="submitTag"
                v-model="tagInput"
                v-focus="getFocusTagInput"
            />
            <template #append>
              <b-button
                  tabindex="0"
                  @click="toggleCollapseTagDisplay"
                  title="Collapse tag display"
                  id="expand-collapse-tags"
                  :disabled="!tags.length"
              >
                <template v-if="getShiftKeyHold">
                  <span class="tag-shortcut">G</span>
                </template>
                <template v-else-if="tagDisplayCollapsed">
                  <b-tooltip noninteractive target="expand-collapse-tags"
                             placement="top" boundary="viewport">
                    Expand tag display
                  </b-tooltip>
                  <span class="fa fa-chevron-down"/>
                </template>
                <template v-else>
                  <b-tooltip noninteractive target="expand-collapse-tags"
                             placement="top" boundary="viewport">
                    Collapse tag display
                  </b-tooltip>
                  <span class="fa fa-chevron-up"/>
                </template>
              </b-button>
            </template>
          </b-input-group>
          <!--    /tag input      -->
          <b-input-group class="flex-grow-1 mr-2">
            <template #prepend>
              <b-input-group-text>
              <span v-if="!getShiftKeyHold"
                    class="fa fa-search fa-fw"
              />
                <span v-else
                      class="query-shortcut">
                Q
              </span>
              </b-input-group-text>
            </template>
            <b-form-input
                tabindex="0"
                ref="search"
                v-model="searchTerm"
                @keydown.enter="search"
                placeholder="Indicators"
                v-focus="getFocusSearch"
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
          <span v-if="!getShiftKeyHold" class="no-wrap">
            Get Cont3xt
          </span>
            <span v-else
                  class="enter-icon">
            <span class="fa fa-long-arrow-left fa-lg" />
            <div class="enter-arm" />
          </span>
          </b-button>
          <ViewSelector
              :no-caret="true"
              :show-selected-view="true"
              :hot-key-enabled="true">
            <template #title>
              <span class="fa fa-eye" />
            </template>
          </ViewSelector>
          <b-dropdown
              class="ml-1"
              tabindex="-1"
              variant="info"
              ref="actionDropdown">
            <b-dropdown-item
                :active="skipCache"
                @click="skipCache = !skipCache"
                v-b-tooltip.hover.left="skipCache ? 'Ignorning cache - click to use cache (shift + c)' : 'Using cache - click to ignore cache (shift + c)'">
              <span class="fa fa-database fa-fw mr-1" />
              Skip Cache
            </b-dropdown-item>
            <b-dropdown-item
                @click="generateReport"
                :disabled="!searchComplete"
                v-b-tooltip.hover.left="'Download a report of this result (shift + r)'">
              <span class="fa fa-file-text fa-fw mr-1" />
              Download Report
            </b-dropdown-item>
            <b-dropdown-item
                @click="shareLink"
                :active="activeShareLink"
                v-b-tooltip.hover.left="'Copy share link to clipboard (shift + l)'">
              <span class="fa fa-share-alt fa-fw mr-1" />
              Copy Share Link
            </b-dropdown-item>
          </b-dropdown>

        </div>
      </div> <!-- /search -->

      <div class="flex-grow-1 d-flex overflow-hidden pt-1">
        <!-- welcome -->
        <div class="w-100 h-100 d-flex flex-column mt-1"
             v-if="!initialized && !error.length && !getIntegrationsError.length">
          <b-alert
              show
              variant="dark"
              class="text-center mx-3">
            <span class="fa fa-rocket fa-2x fa-flip-horizontal mr-1 text-muted" />
            <strong class="text-warning lead">
              <strong>Welcome to Cont3xt!</strong>
            </strong>
            <span v-if="!searchTerm"
                  class="text-success lead">
              <strong>Search for IPs, domains, URLs, emails, phone numbers, or hashes.</strong>
            </span>
            <span v-else
                  class="text-success lead">
              <strong>Hit enter to issue your search!</strong>
            </span>
            <span class="fa fa-rocket fa-2x ml-1 text-muted" />
          </b-alert>
          <div class="cont3xt-result-grid-container">
            <div class="cont3xt-result-grid cont3xt-welcome">
              <div class="indicator-tree-pane">
                <div class="well well-lg text-center p-4 alert-dark h-100 mb-3 mx-2">
                  <h1>
                    <span class="fa fa-2x fa-tree text-muted" />
                  </h1>
                  <h1 class="display-4">
                    Indicator Result Tree
                  </h1>
                  <p class="lead">
                    Top level indicators presented here
                  </p>
                  <p class="lead">
                    Integration icons will display high level result
                  </p>
                  <p class="lead">
                    Choose and configure integrations via
                    <a class="no-decoration"
                       href="settings#integrations">
                      Settings -> Integrations
                    </a>
                  </p>
                </div>
              </div>
              <div class="result-card-pane">
                <div class="well well-lg text-center p-4 alert-dark h-100 mb-3 mx-2">
                  <h1>
                    <span class="fa fa-2x fa-id-card-o text-muted" />
                  </h1>
                  <h1 class="display-4">
                    Indicator Card Detail
                  </h1>
                  <p class="lead">
                    Displays configurable subset of API results
                  </p>
                  <p class="lead">
                    Optionally, access raw results for card display tuning
                  </p>
                </div>
              </div>
              <div class="link-group-pane">
                <div class="well well-lg text-center p-4 alert-dark h-100 mb-3 mx-2">
                  <h1>
                    <span class="fa fa-2x fa-link text-muted" />
                  </h1>
                  <h1 class="display-4">
                    Link Groups
                  </h1>
                  <p class="lead">
                    Custom pivot links tailored to the top level indicator query
                  </p>
                  <p class="lead">
                    Create/Configure links and link groups in
                    <a class="no-decoration"
                       href="settings#linkgroups">
                      Settings -> Link Groups
                    </a>
                  </p>
                </div>
              </div>
            </div>
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

        <div v-if="rootIndicator" class="cont3xt-result-grid-container">
          <div class="cont3xt-result-grid">
            <div class="indicator-tree-pane">
              <!-- tags line -->
              <div v-if="!tagDisplayCollapsed" class="d-flex justify-content-start mb-1">
                <tag-display-line :tags="tags" :remove-tag="removeTag" :clear-tags="clearTags"/>
              </div>
              <!-- /tags line -->
              <div class="pane-scroll-content pb-5">
                <!-- indicator result tree -->
                <i-type-node :node="indicatorTreeRoot" />
                <!-- /indicator result tree -->
              </div>
            </div>
            <div class="result-card-pane position-relative" :class="{ 'result-card-pane-expanded': !getLinkGroupsPanelOpen }">
              <div class="d-flex justify-content-between mb-1 mx-2">
                <integration-btns
                    :indicator="activeIndicator"
                />
                <overview-selector
                    v-if="activeIndicator"
                    :i-type="activeIndicator.itype"
                    :selected-overview="currentOverviewCard"
                    @set-override-overview="setOverrideOverview" />
              </div>
              <div class="pane-scroll-content" @scroll="handleScroll" ref="resultsIntegration">
                <!-- integration results -->
                <b-overlay
                    no-center
                    rounded="sm"
                    blur="0.2rem"
                    opacity="0.9"
                    variant="transparent"
                    :show="getWaitRendering || getRendering">
                  <div class="mb-5">
                    <template v-if="showOverview">
                      <overview-card
                          v-if="currentOverviewCard"
                          :indicator="activeIndicator"
                          :card="currentOverviewCard"
                      />
                      <b-alert
                          v-else
                          show
                          variant="dark"
                          class="text-center">
                        There is no overview configured for the <strong>{{ this.activeIndicator.itype }}</strong> iType.
                        <a class="no-decoration" href="settings#overviews">Create one!</a>
                      </b-alert>
                    </template>
                    <integration-card
                        v-else-if="activeSource && activeIndicator"
                        :source="activeSource"
                        :indicator="activeIndicator"
                        @update-results="updateData"
                    />
                  </div>
                  <template #overlay>
                    <div class="overlay-loading">
                      <span class="fa fa-circle-o-notch fa-spin fa-2x" />
                      <p>Rendering data...</p>
                    </div>
                  </template>
                </b-overlay>
                <!-- /integration results -->
              </div>
              <b-button
                  v-if="scrollPx > 100"
                  size="sm"
                  @click="toTop"
                  title="Go to top"
                  class="to-top-btn"
                  variant="btn-link"
                  v-show="scrollPx > 100">
                <span class="fa fa-lg fa-arrow-circle-up" />
              </b-button>
            </div>
            <div v-if="getLinkGroupsPanelOpen" class="link-group-pane">
              <div class="flex-grow-1 d-flex flex-column link-group-panel-shadow ml-3 overflow-hidden">
                <div v-if="activeIndicator" class="mb-1 mx-2">
                  <!-- link groups error -->
                  <b-alert
                      variant="danger"
                      :show="!!getLinkGroupsError.length">
                    {{ getLinkGroupsError }}
                  </b-alert>
                  <!-- /link groups error -->

                  <!-- link search -->
                  <div class="d-flex justify-content-between mb-1">
                    <div class="flex-grow-1">
                      <b-input-group size="sm">
                        <template #prepend>
                          <b-input-group-text>
                            <span v-if="!getShiftKeyHold"
                                  class="fa fa-search fa-fw"
                            />
                            <span v-else
                                  class="lg-query-shortcut">
                              F
                            </span>
                          </b-input-group-text>
                        </template>
                        <b-form-input
                            tabindex="0"
                            debounce="400"
                            ref="linkSearch"
                            v-model="linkSearchTerm"
                            v-focus="getFocusLinkSearch"
                            placeholder="Search links below"
                        />
                      </b-input-group>
                    </div>
                    <b-button
                        size="sm"
                        class="mx-1"
                        v-b-tooltip.hover
                        variant="outline-secondary"
                        :disabled="!hasVisibleLinkGroup"
                        @click="toggleAllVisibleLinkGroupsCollapse"
                        :title="`${!allVisibleLinkGroupsCollapsed ? 'Collapse' : 'Expand'} ALL Link Groups`">
                      <span class="fa fa-fw"
                            :class="[!allVisibleLinkGroupsCollapsed ? 'fa-chevron-up' : 'fa-chevron-down']">
                      </span>
                    </b-button>
                    <!-- toggle link groups panel button -->
                    <b-button
                        size="sm"
                        tabindex="-1"
                        variant="link"
                        class="float-right"
                        @click="toggleLinkGroupsPanel"
                        v-b-tooltip.hover.top
                        title="Hide Link Groups Panel">
                      <span class="fa fa-lg fa-angle-double-right" />
                    </b-button>
                    <!-- /toggle link groups panel button -->
                  </div>
                  <!-- /link search -->

                  <!-- time range input for links -->
                  <time-range-input v-model="timeRangeInfo"
                                    :place-holder-tip="linkPlaceholderTip" />
                  <!-- /time range input for links -->
                </div>
                <div v-if="activeIndicator" class="pane-scroll-content">
                <!-- link groups -->
                <div class="d-flex flex-column align-items-start mb-5">
                  <template v-if="hasVisibleLinkGroup">
                    <template v-for="(linkGroup, index) in getLinkGroups">
                      <reorder-list
                          :index="index"
                          @update="updateList"
                          :key="linkGroup._id"
                          :list="getLinkGroups"
                          class="w-100"
                          v-if="hasVisibleLink(linkGroup)">
                        <template #handle>
                        <span
                            :id="`${linkGroup._id}-tt`"
                            class="fa fa-bars d-inline link-group-card-handle">
                        </span>
                          <b-tooltip
                              noninteractive
                              :target="`${linkGroup._id}-tt`">
                            Drag &amp; drop to reorder Link Groups
                          </b-tooltip>
                        </template>
                        <template #default>
                          <link-group-card
                              v-if="getLinkGroups.length"
                              class="w-100"
                              :indicator="activeIndicator"
                              :num-days="timeRangeInfo.numDays"
                              :num-hours="timeRangeInfo.numHours"
                              :stop-date="timeRangeInfo.stopDate"
                              :start-date="timeRangeInfo.startDate"
                              :link-group="getLinkGroups[index]"
                              :hide-links="hideLinks[linkGroup._id]"
                          />
                        </template>
                      </reorder-list>
                    </template>
                  </template>
                  <!-- no link groups message -->
                  <span v-else-if="hasLinkGroupWithItype" class="p-1">
                    There are no Link Groups that match your search.
                  </span>
                  <span v-else class="p-1">
                    There are no Link Groups for the <strong>{{ activeIndicator.itype }}</strong> iType.
                    <a class="no-decoration" href="settings#linkgroups">Create one!</a>
                  </span> <!-- /no link groups message -->
                </div> <!-- /link groups -->
              </div>
              </div>
            </div>
          </div>
        </div>
        <div v-if="rootIndicator && !getLinkGroupsPanelOpen" class="side-panel-stub link-group-panel-stub h-100 cursor-pointer d-flex flex-column"
             v-b-tooltip.hover.top="'Show Link Groups Panel'"
             @click="toggleLinkGroupsPanel"
        >
          <span
              class="fa fa-link p-1 mt-1"
          />
        </div>
      </div>
    </div> <!-- /page content -->
  </div>
</template>

<script>
import { mapGetters } from 'vuex';

import ReorderList from '@/utils/ReorderList';
import TimeRangeInput from '@/utils/TimeRangeInput';
import Focus from '@/../../../common/vueapp/Focus';
import ViewSelector from '@/components/views/ViewSelector';
import UserService from '@/components/services/UserService';
import LinkGroupCard from '@/components/links/LinkGroupCard';
import CreateViewModal from '@/components/views/CreateViewModal';
import Cont3xtService from '@/components/services/Cont3xtService';
import IntegrationCard from '@/components/integrations/IntegrationCard';
import OverviewCard from '@/components/overviews/OverviewCard';
import IntegrationPanel from '@/components/integrations/IntegrationPanel';
import TagDisplayLine from '@/utils/TagDisplayLine';
import { paramStr } from '@/utils/paramStr';
import LinkService from '@/components/services/LinkService';
import OverviewService from '@/components/services/OverviewService';
import OverviewSelector from '../overviews/OverviewSelector.vue';
import ITypeNode from '@/components/itypes/ITypeNode.vue';
import IntegrationBtns from '@/components/integrations/IntegrationBtns.vue';

export default {
  name: 'Cont3xt',
  components: {
    IntegrationBtns,
    ITypeNode,
    OverviewSelector,
    ReorderList,
    ViewSelector,
    LinkGroupCard,
    CreateViewModal,
    IntegrationCard,
    OverviewCard,
    IntegrationPanel,
    TimeRangeInput,
    TagDisplayLine
  },
  directives: { Focus },
  data () {
    return {
      error: '',
      scrollPx: 0,
      initialized: false,
      searchTerm: this.$route.query.q ? this.$route.query.q : (this.$route.query.b ? window.atob(this.$route.query.b) : ''),
      overrideOverviewId: undefined,
      skipCache: false,
      searchComplete: false,
      linkSearchTerm: this.$route.query.linkSearch || '',
      hideLinks: {},
      linkPlaceholderTip: {
        title: 'These values are used to fill in <a href="help#linkgroups" class="no-decoration">link placeholders</a>.<br>' +
            'Try using <a href="help#general" class="no-decoration">relative times</a> like -5d or -1h.'
      },
      activeShareLink: false,
      timeRangeInfo: {
        numDays: 7, // 1 week
        numHours: 7 * 24, // 1 week
        startDate: new Date(new Date().getTime() - (3600000 * 24 * 7)).toISOString().slice(0, -5) + 'Z', // 1 week ago
        stopDate: new Date().toISOString().slice(0, -5) + 'Z' // now
      },
      tagInput: ''
    };
  },
  mounted () {
    // if the user was seeing all views or link groups (admin), toggle off and use regular
    if (this.getSeeAllViews) {
      this.$store.commit('SET_SEE_ALL_VIEWS', false);
      UserService.getIntegrationViews();
    }
    if (this.getSeeAllLinkGroups) {
      this.$store.commit('SET_SEE_ALL_LINK_GROUPS', false);
      LinkService.getLinkGroups();
    }
    if (this.getSeeAllOverviews) {
      this.$store.commit('SET_SEE_ALL_OVERVIEWS', false);
      OverviewService.getOverviews();
    }

    // no need to parse start/stopDate query params here -- that is handled by TimeRangeInput
    // submit, view, and tags query params are handled in watcher

    // needs to be unfocused to focus again later with hotkey (subsequent focuses are unfocused in store)
    this.$store.commit('SET_FOCUS_SEARCH', false);
  },
  computed: {
    ...mapGetters([
      'getRendering', 'getWaitRendering', 'getQueuedIntegration',
      'getIntegrationsError', 'getLinkGroupsError', 'getLinkGroups',
      'getSidebarKeepOpen', 'getShiftKeyHold', 'getFocusSearch',
      'getIssueSearch', 'getFocusLinkSearch', 'getFocusTagInput',
      'getToggleCache', 'getDownloadReport', 'getCopyShareLink',
      'getAllViews', 'getImmediateSubmissionReady', 'getSelectedView',
      'getTags', 'getTagDisplayCollapsed', 'getSeeAllViews', 'getSeeAllLinkGroups',
      'getSeeAllOverviews', 'getSelectedOverviewMap', 'getOverviewMap', 'getResults',
      'getIndicatorGraph', 'getLinkGroupsPanelOpen'
    ]),
    tags: {
      get () { return this.getTags; },
      set (val) { this.$store.commit('SET_TAGS', val); }
    },
    tagDisplayCollapsed: {
      get () { return this.getTagDisplayCollapsed; },
      set (val) { this.$store.commit('SET_TAG_DISPLAY_COLLAPSED', val); }
    },
    loading: {
      get () { return this.$store.state.loading; },
      set (val) { this.$store.commit('SET_LOADING', val); }
    },
    activeIndicator: {
      get () { return this.$store.state.activeIndicator; },
      set (val) { this.$store.commit('SET_ACTIVE_INDICATOR', val); }
    },
    activeSource: {
      get () { return this.$store.state.activeSource; },
      set (val) { this.$store.commit('SET_ACTIVE_SOURCE', val); }
    },
    results () {
      return this.$store.state.results;
    },
    collapsedLinkGroups () {
      return this.$store.state.collapsedLinkGroups;
    },
    hasLinkGroupWithItype () {
      return this.getLinkGroups?.some(this.hasLinkWithItype);
    },
    visibleLinkGroups () {
      return (this.getLinkGroups ?? []).filter(this.hasVisibleLink);
    },
    hasVisibleLinkGroup () {
      return this.visibleLinkGroups.length > 0;
    },
    allVisibleLinkGroupsCollapsed () {
      return this.visibleLinkGroups.every(lg => this.collapsedLinkGroups[lg._id]);
    },
    currentOverviewCard () {
      if (this.overrideOverviewId) {
        return this.getOverviewMap[this.overrideOverviewId];
      }
      return this.getSelectedOverviewMap[this.activeIndicator.itype];
    },
    /** @returns {Cont3xtIndicatorNode[]} */
    indicatorTreeRoots () {
      return Object.values(this.getIndicatorGraph).filter(node => node.parentIds.has(undefined));
    },
    /** @returns {Cont3xtIndicatorNode | undefined} */
    indicatorTreeRoot () {
      // since we don't yet have bulk, there can only be one root, so we grab it here
      // TODO: this should be removed when bulk is added
      return this.indicatorTreeRoots?.[0];
    },
    rootIndicator () {
      return this.indicatorTreeRoot?.indicator;
    },
    showOverview () {
      return this.activeSource == null && !(this.getWaitRendering || this.getRendering);
    }
  },
  watch: {
    getQueuedIntegration (newQueuedIntegration) {
      this.activeSource = undefined;
      this.$store.commit('SET_RENDERING_CARD', true);
      // need wait rendering to tell the card that we aren't rendering yet
      // or else the data will be stale when it updates the integration type
      this.$store.commit('SET_WAIT_RENDERING', true);
      setTimeout(() => { // need timeout for SET_RENDERING_CARD to take effect
        this.activeIndicator = newQueuedIntegration.indicator;
        this.activeSource = newQueuedIntegration.source;
        this.$store.commit('SET_WAIT_RENDERING', false);
      }, 100);
    },
    activeIndicator (newIndicator, oldIndicator) {
      if (newIndicator?.query !== oldIndicator?.query || newIndicator?.itype !== oldIndicator?.itype) {
        this.overrideOverviewId = undefined;
      }
    },
    linkSearchTerm (searchTerm) {
      this.hideLinks = {};

      // removes linkSearch query parameter when empty string
      const queryParamLinkSearch = searchTerm === '' ? undefined : searchTerm;
      if (this.$route.query.linkSearch !== queryParamLinkSearch) {
        this.$router.push({
          query: {
            ...this.$route.query,
            linkSearch: queryParamLinkSearch
          }
        });
      }

      this.filterLinks(searchTerm);
    },
    getIssueSearch (val) {
      if (val) { this.search(); }
    },
    getToggleCache (val) {
      if (val) {
        this.$refs.actionDropdown.show();
        setTimeout(() => { this.skipCache = !this.skipCache; }, 100);
        setTimeout(() => { this.$refs.actionDropdown.hide(); }, 1000);
      }
    },
    getDownloadReport (val) {
      if (val) { this.generateReport(); }
    },
    getCopyShareLink (val) {
      if (val) {
        this.$refs.actionDropdown.show();
        setTimeout(() => { this.activeShareLink = true; }, 100);
        setTimeout(() => {
          this.shareLink();
          this.activeShareLink = false;
        }, 500);
      }
    },
    getFocusSearch (val) {
      if (val) { this.$refs.search.select(); }
    },
    getFocusLinkSearch (val) {
      if (val) { this.$refs.linkSearch.select(); }
    },
    getFocusTagInput (val) {
      if (val) { this.$refs.tagInput.select(); }
    },
    // handle page-load query params -- fires once both integrations and views are loaded in from backend
    getImmediateSubmissionReady () {
      // only proceed when ready
      if (!this.getImmediateSubmissionReady) { return; }

      // parse tags from query parameter (used by history to reissue search with tags), then remove it
      if (this.$route.query.tags) {
        this.tags = this.$route.query.tags.split(',').map(tag => tag.trim());
        this.tagDisplayCollapsed = !this.tags.length;
      }

      // apply 'view' query param
      if (this.$route.query.view !== undefined) {
        this.setViewByQueryParam(this.$route.query.view);
      }

      // search now depending on 'submit' query parameter
      if (this.shouldSubmitImmediately()) {
        this.search();
      }

      // remove 'submit' and 'tags' query parameters
      if (this.$route.query.submit !== undefined || this.$route.query.tags !== undefined) {
        this.$router.push({ query: { ...this.$route.query, submit: undefined, tags: undefined } });
      }
    }
  },
  methods: {
    /* page functions ------------------------------------------------------ */
    toggleLinkGroupsPanel () {
      this.$store.commit('TOGGLE_LINK_GROUPS_PANEL');
    },
    toggleCollapseTagDisplay () {
      this.tagDisplayCollapsed = !this.tagDisplayCollapsed;
    },
    clearTags () {
      this.tags = [];
      this.tagDisplayCollapsed = true;
    },
    submitTag () {
      if (this.tagInput !== '') {
        const tagsCreated = this.tagInput.split(',')
          .map(str => str.trim())
          .filter(str => str !== '');
        if (tagsCreated.length > 0) {
          this.tagDisplayCollapsed = false;
        }
        this.tags = [...this.tags, ...tagsCreated];
        this.tagInput = '';
      }
    },
    removeTag (index) {
      const newTags = [...this.tags];
      newTags.splice(index, 1);
      if (newTags.length === 0) {
        this.tagDisplayCollapsed = true;
      }
      this.tags = newTags;
    },
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
    handleIntegrationChunk (chunk) {
      switch (chunk.purpose) {
      case 'init':
        // determine the search type and save the search term
        this.activeIndicator = chunk.indicator;
        this.filterLinks(this.linkSearchTerm);
        break;
      case 'error':
        this.error = `ERROR: ${chunk.text}`;
        break;
      case 'data':
        if (chunk.name && chunk.indicator) {
          this.$store.commit('SET_INTEGRATION_RESULT', {
            indicator: chunk.indicator,
            source: chunk.name,
            result: chunk.data
          });
        }
        break;
      case 'fail':
        // TODO: in the future, visually show result for integration as a failure
        if (chunk.sent && chunk.total) { // add failure to the progress bar
          this.loading.failed++;
          this.loading.failure = chunk.name;
        }
        break;
      case 'link':
        this.$store.commit('UPDATE_INDICATOR_GRAPH', {
          indicator: chunk.indicator,
          parentIndicator: chunk.parentIndicator
        });
        break;
      case 'enhance':
        this.$store.commit('ADD_ENHANCE_INFO', {
          indicator: chunk.indicator,
          enhanceInfo: chunk.enhanceInfo
        });
        break;
      case 'finish': {
        const leftover = this.loading.total - this.loading.failed - this.loading.received;
        if (leftover) {
          this.loading = { // complete the progress bar
            received: this.loading.received + leftover
          };
        }
        break;
      }
      default:
        this.error = `ERROR: Unknown purpose '${chunk.purpose}' in data chunk`;
        break;
      }

      if (chunk.sent && chunk.total) { // update the progress bar
        this.loading.total = chunk.total;
        this.loading.received = chunk.sent;
      }
    },
    search () {
      if (this.searchTerm == null || this.searchTerm === '') {
        return; // do NOT search if the query is empty
      }

      this.error = '';
      this.$store.commit('CLEAR_CONT3XT_RESULTS');
      this.activeIndicator = undefined;
      this.activeSource = undefined;
      this.initialized = true;
      this.searchComplete = false;
      this.$store.commit('RESET_LOADING');
      this.overrideOverviewId = undefined;

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
      const viewId = this.getSelectedView?._id;
      Cont3xtService.search({ searchTerm: this.searchTerm, skipCache: this.skipCache, tags: this.tags, viewId }).subscribe({
        next: this.handleIntegrationChunk,
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
    setOverrideOverview (id) {
      this.overrideOverviewId = id;
    },
    hasLinkWithItype (linkGroup) {
      return linkGroup.links.some(link =>
        link.url !== '----------' && link.itypes.includes(this.activeIndicator.itype)
      );
    },
    hasVisibleLink (linkGroup) {
      return linkGroup.links.some((link, i) =>
        link.url !== '----------' && link.itypes.includes(this.activeIndicator.itype) && !this.hideLinks[linkGroup._id]?.[i]
      );
    },
    shareLink () {
      let shareLink = window.location.href;
      if (this.$route.query.b !== undefined || this.$route.query.q !== undefined) {
        // share link is given submit=y query param (only if some search parameter exists)
        const allSharedQueryParams = { ...this.$route.query, submit: 'y' };
        shareLink = `${window.location.origin}/${paramStr(allSharedQueryParams)}`;
      }
      this.$copyText(shareLink);
    },
    toggleAllVisibleLinkGroupsCollapse () {
      // if all are collapsed, open them all
      // if even one is open, close them all
      const allCollapsed = this.allVisibleLinkGroupsCollapsed;
      for (const lg of this.visibleLinkGroups) {
        this.$set(this.collapsedLinkGroups, lg._id, !allCollapsed);
      }
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
    updateData (chunk) {
      if (chunk.purpose !== 'data') {
        // error/fail, so we stop buffering, else it would continue forever!
        this.$store.commit('SET_RENDERING_CARD', false);
      }

      if (chunk.purpose === 'fail') {
        // we don't want to overwrite good data with a failure
        this.error = 'ERROR: Failed to refresh data';
        return;
      }
      // handle purpose:data and purpose:error
      this.handleIntegrationChunk(chunk);
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
    },
    setViewByQueryParam (viewParam) {
      const removeViewParam = () => {
        this.$router.push({ query: { ...this.$route.query, view: undefined } });
      };

      if (typeof viewParam !== 'string' || viewParam === '') {
        console.log(`WARNING -- Invalid view '${viewParam}' from query parameter... defaulting to current integrations.`);
        removeViewParam();
        return;
      }
      // first check if the parameter is an existing view id
      for (const view of this.getAllViews) {
        if (viewParam === view._id) {
          this.$store.commit('SET_SELECTED_VIEW', view);
          this.$store.commit('SET_SELECTED_INTEGRATIONS', view.integrations);
          return;
        }
      }
      // if not an id, check if the parameter is an exact match for an existing view name
      for (const view of this.getAllViews) {
        if (viewParam === view.name) {
          this.$store.commit('SET_SELECTED_VIEW', view);
          this.$store.commit('SET_SELECTED_INTEGRATIONS', view.integrations);
          return;
        }
      }
      console.log(`WARNING -- View '${viewParam}' specified by query parameter was not found... defaulting to current integrations.`);
      removeViewParam();
    },
    shouldSubmitImmediately () {
      const submitParam = this.$route.query.submit;
      return (submitParam === 'y' || submitParam === 'yes' ||
          submitParam === 't' || submitParam === 'true');
    }
  },
  beforeDestroy () {
    this.$store.commit('RESET_LOADING');

    // clear results/selections from the store (so the current search is not presented when the user returns)
    this.$store.commit('CLEAR_CONT3XT_RESULTS');
    this.activeSource = undefined;
    this.activeIndicator = undefined;
  }
};
</script>

<style scoped>
.search-btn {
  width: 148px;
}

.search-nav {
  padding: 16px 16px 0 16px;
  margin-top: 50px !important;
}
body.dark {
  background-color: #222;
}

.cont3xt-welcome .well {
  border-radius: 6px;
}
/* better text-wrapping on the welcome screen for browsers that support it */
/*noinspection CssInvalidPropertyValue*/
.cont3xt-welcome .well h1, .cont3xt-welcome .well p {
  text-wrap: balance;
}

/* scroll to top btn for integration results */
.to-top-btn, .to-top-btn:hover {
  z-index: 99;
  right: 8px;
  bottom: 0;
  position: absolute;
  color: var(--info);
}

.link-group-card-handle {
  top: 2rem;
  z-index: 2;
  float: right;
  right: 1rem;
  position: relative;
}

/* enter icon for search/refresh button to be displayed on shift hold */
.enter-icon > .fa-long-arrow-left {
  top: 2px;
  position: relative;
}
.enter-icon > .enter-arm {
  top: -2px;
  right: 6px;
  width: 3px;
  height: 9px;
  position: relative;
  display: inline-block;
  background-color: #FFF;
}

.cont3xt-result-grid-container {
  flex-grow: 1;
  overflow: hidden;
  padding-inline: 0.5rem;
}

.cont3xt-result-grid {
  display: grid;
  grid-template-columns: 25% 1fr 33%;
  width: 100%;
  height: 100%;
  overflow: hidden;
}

.indicator-tree-pane, .result-card-pane, .result-card-pane-expanded, .link-group-pane {
  display: flex;
  flex-direction: column;
  overflow: hidden;
  height: 100%;
}

.indicator-tree-pane {
  grid-column: 1;
}

.result-card-pane {
  grid-column: 2;
}

.result-card-pane-expanded {
  grid-column: 2 / span 2;
}

.link-group-pane {
  grid-column: 3;
}

.pane-scroll-content {
  flex-grow: 1;
  overflow-y: auto;
  width: 100%;
  padding-inline: 0.5rem;
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

.link-group-panel-stub {
  border-top-left-radius: 5px;
}
.link-group-panel-shadow {
  -webkit-box-shadow: -2px 0 1rem 0 rgba(0, 0, 0, 0.175) !important;
  box-shadow: -2px 0 1rem 0 rgba(0, 0, 0, 0.175) !important;
}
</style>
