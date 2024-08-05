<!--
Copyright Yahoo Inc.
SPDX-License-Identifier: Apache-2.0
-->
<template>
  <div class="d-flex flex-row flex-grow-1 no-overflow-x">
    <!-- view create form -->
    <create-view-modal v-model="viewModalOpen" />
    <!-- integration selection panel -->
    <IntegrationPanel @create-view="viewModalOpen = true" />
    <!-- page content -->
    <div class="flex-grow-1 d-flex flex-column">
      <!-- search -->
      <div class="d-flex justify-center mt-2 mx-3">
        <div class="w-100 pb-1 d-flex justify-space-between">
          <!--    tag input      -->
          <v-text-field
            class="input-connect-right"
            style="max-width: 150px; width: 150px"
            type="text"
            tabindex="0"
            ref="tagInput"
            :placeholder="`Tags${tags.length ? ` (${tags.length})` : ''}`"
            @keydown.enter="submitTag"
            v-model="tagInput"
            v-focus="getFocusTagInput"
          />
          <v-btn
              variant="flat"
              color="secondary"
              class="btn-connect-left skinny-search-row-btn mr-1"
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
              <v-tooltip activator="parent" location="top">
                Expand tag display
              </v-tooltip>
              <span class="fa fa-chevron-down"/>
            </template>
            <template v-else>
              <v-tooltip activator="parent" location="top">
                Collapse tag display
              </v-tooltip>
              <span class="fa fa-chevron-up"/>
            </template>
          </v-btn>
          <!--    /tag input      -->
          <v-text-field
            variant="outlined"
            v-model="searchTerm"
            ref="search"
            id="cont3xt-search-bar"
            class="w-100"
            @keydown.enter=search
            placeholder="Indicators"
            v-focus="getFocusSearch"
            clearable
          >
            <template #prepend-inner>
              <span v-if="getShiftKeyHold" class="search-query-shortcut text-warning">Q</span>
                <v-icon v-else icon="mdi-magnify"/>
            </template>
          </v-text-field>
          <v-btn
              tabindex="-1"
              @click="search"
              color="success"
              title="search"
              class="mx-1 search-row-btn cont3xt-search-btn">
            <span v-if="!getShiftKeyHold" class="no-wrap">
              <span class="fa fa-rocket" :class="{ ['rocket-fly']: gettingCont3xt }"></span>
              Get Cont3xt
            </span>
            <v-icon v-else icon="mdi-keyboard-return" size="large"/>
          </v-btn>
          <ViewSelector
              @open-view-modal="viewModalOpen = true"
              class="search-row-btn"
              :no-caret="true"
              :show-selected-view="true"
              :hot-key-enabled="true">
            <span class="fa fa-eye" />
          </ViewSelector>
          <!-- action dropdown -->
          <action-dropdown
            :actions="dropdownActions"
            class="ml-1 skinny-search-row-btn"
            tabindex="-1"
            color="info"
          />
          <!-- /action dropdown -->
        </div>
      </div> <!-- /search -->

      <div class="flex-grow-1 d-flex flex-row overflow-hidden pt-1">
        <!-- welcome -->
        <div class="w-100 h-100 d-flex flex-column mt-1 cont3xt-welcome"
             v-if="!initialized && !error.length && !getIntegrationsError.length">
          <div class="well text-center mx-4 mb-2 py-2 d-flex align-center justify-center">
            <span class="fa fa-rocket fa-2x fa-flip-horizontal mr-1 text-muted" />
            <strong class="text-warning lead mr-2">
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
          </div>
          <div class="cont3xt-result-grid-container">
            <div class="cont3xt-result-grid">
              <div class="indicator-tree-pane">
                <div class="well text-center pa-4 alert-dark h-100 mb-3 mx-2">
                  <h3>
                    <span class="fa fa-2x fa-tree text-muted" />
                  </h3>
                  <h3 class="display-4">
                    Indicator Result Tree
                  </h3>
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
                <div class="well text-center pa-4 alert-dark h-100 mb-3 mx-2">
                  <h3>
                    <span class="fa fa-2x fa-id-card-o text-muted" />
                  </h3>
                  <h3 class="display-4">
                    Indicator Card Detail
                  </h3>
                  <p class="lead">
                    Displays configurable subset of API results
                  </p>
                  <p class="lead">
                    Optionally, access raw results for card display tuning
                  </p>
                </div>
              </div>
              <div class="link-group-pane">
                <div class="well text-center pa-4 alert-dark h-100 mb-3 mx-2">
                  <h3>
                    <span class="fa fa-2x fa-link text-muted" />
                  </h3>
                  <h3 class="display-4">
                    Link Groups
                  </h3>
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

        <!-- errors -->
        <div v-if="error.length || getIntegrationsError.length"
             class="w-100 d-flex flex-column mt-2 mx-3">
          <!-- search error -->
          <div
              v-if="error.length"
              class="alert alert-warning">
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
              class="alert alert-danger">
            <span class="fa fa-exclamation-triangle" />&nbsp;
            Error fetching integrations. Viewing data for integrations will not work!
            <br>
            {{ getIntegrationsError }}
          </div> <!-- /integration error -->
        </div>
        <!-- /errors -->

        <div v-if="shouldDisplayResults" class="cont3xt-result-grid-container">
          <div class="cont3xt-result-grid">
            <div class="indicator-tree-pane">
              <!-- tags line -->
              <div v-if="!tagDisplayCollapsed" class="d-flex justify-start mb-1">
                <tag-display-line :tags="tags" :remove-tag="removeTag" :clear-tags="clearTags"/>
              </div>
              <!-- /tags line -->
              <div class="pane-scroll-content pb-5 d-flex flex-column gap-3">
                <!-- indicator result tree -->
                <i-type-node
                    v-for="(indicatorTreeRoot, i) in indicatorTreeRoots" :key="i"
                    :node="indicatorTreeRoot" />
                <!-- /indicator result tree -->
              </div>
            </div>
            <div class="result-card-pane position-relative" :class="{ 'result-card-pane-expanded': !getLinkGroupsPanelOpen }">
              <integration-btns
                :indicator-id="activeIndicatorId"
                :selected-overview="currentOverviewCard"
                @set-override-overview="setOverrideOverview"
              />
              <div class="pane-scroll-content position-relative" @scroll="handleScroll" ref="resultsIntegration">
                <!-- integration results -->
                <v-overlay
                  :model-value="getWaitRendering || getRendering"
                  class="align-center justify-center blur-overlay"
                  contained
                  >
                  <div class="d-flex flex-column align-center justify-center">
                    <v-progress-circular
                      color="info"
                      size="64"
                      indeterminate />
                    <p>Rendering data...</p>
                  </div>
                </v-overlay>
                <div class="mb-5">
                  <template v-if="showOverview">
                    <overview-card
                        class="overflow-auto"
                        v-if="currentOverviewCard"
                        :indicator="getActiveIndicator"
                        :card="currentOverviewCard"
                    />
                    <v-alert
                        v-else
                        color="dark"
                        class="text-center">
                      There is no overview configured for the <strong>{{ getActiveIndicator.itype }}</strong> iType.
                      <a class="no-decoration" href="settings#overviews">Create one!</a>
                    </v-alert>
                  </template>
                  <integration-card
                      class="overflow-auto"
                      v-else-if="activeSource && getActiveIndicator"
                      :source="activeSource"
                      :indicator="getActiveIndicator"
                      @update-results="updateData"
                  />
                </div>
                <!-- /integration results -->
              </div>
              <v-btn
                  v-if="scrollPx > 100"
                  size="small"
                  @click="toTop"
                  title="Go to top"
                  class="to-top-btn square-btn-sm"
                  variant="text"
                  color="btn-link"
                  v-show="scrollPx > 100">
                <span class="fa fa-lg fa-arrow-circle-up" />
              </v-btn>
            </div>
            <div v-if="getLinkGroupsPanelOpen" class="link-group-pane">
              <div class="flex-grow-1 d-flex flex-column link-group-panel-shadow ml-3 overflow-hidden">
                <div v-if="getActiveIndicator" class="mb-1 mx-2">
                  <!-- link groups error -->
                  <v-alert
                      color="error"
                      v-if="!!getLinkGroupsError.length">
                    {{ getLinkGroupsError }}
                  </v-alert>
                  <!-- /link groups error -->

                  <!-- link search -->
                  <div class="d-flex flex-row justify-space-between mb-1">
                    <div class="flex-grow-1 no-wrap d-flex flex-row mb-2">
                      <v-text-field
                        class="w-50 input-connect-right"
                        block="false"
                        tabindex="0"
                        ref="linkSearch"
                        v-debounce="val => linkSearchTerm = val"
                        v-focus="getFocusLinkSearch"
                        placeholder="Search links below"
                      >
                        <template #prepend-inner>
                          <span v-if="!getShiftKeyHold"
                            class="fa fa-search fa-fw"
                          />
                          <span v-else
                            class="lg-query-shortcut">
                            F
                          </span>
                        </template>
                      </v-text-field>
                      <v-select
                        class="input-connect-left"
                        flat
                        style="max-width: 34px"
                        v-tooltip="`Showing links for ${currentItype} iType. Click to change.`"
                        v-model="currentItype"
                        :items="iTypes"
                      >
                        <template #selection></template>
                      </v-select>
                    </div>
                    <v-btn
                      size="small"
                      class="mx-1"
                      v-tooltip="`${!allVisibleLinkGroupsCollapsed ? 'Collapse' : 'Expand'} ALL Link Groups`"
                      variant="outlined"
                      color="secondary"
                      :disabled="!hasVisibleLinkGroup"
                      @click="toggleAllVisibleLinkGroupsCollapse"
                      :title="`${!allVisibleLinkGroupsCollapsed ? 'Collapse' : 'Expand'} ALL Link Groups`">
                      <span class="fa fa-fw"
                        :class="[!allVisibleLinkGroupsCollapsed ? 'fa-chevron-up' : 'fa-chevron-down']">
                      </span>
                    </v-btn>
                    <!-- toggle link groups panel button -->
                    <v-btn
                      size="small"
                      tabindex="-1"
                      variant="text"
                      class="float-right"
                      color="primary"
                      @click="toggleLinkGroupsPanel"
                      v-tooltip:top="'Hide Link Groups Panel'"
                      title="Hide Link Groups Panel">
                      <span class="fa fa-lg fa-angle-double-right" />
                    </v-btn>
                    <!-- /toggle link groups panel button -->
                  </div>
                  <!-- /link search -->

                  <!-- time range input for links -->
                  <time-range-input v-model="timeRangeInfo"
                    :place-holder-tip="linkPlaceholderTip"
                  />
                  <!-- /time range input for links -->
                </div>
                <div v-if="getActiveIndicator" class="pane-scroll-content">
                <!-- link groups -->
                <div class="d-flex flex-column align-start mb-5">
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
                          class="fa fa-bars d-inline link-group-card-handle"
                        ></span>
                        <id-tooltip :target="`${linkGroup._id}-tt`">
                          Drag &amp; drop to reorder Link Groups
                        </id-tooltip>

                        </template>
                        <template #default>
                          <link-group-card
                            v-if="getLinkGroups.length && getActiveIndicator"
                            class="w-100"
                            :itype="currentItype"
                            :indicator="getActiveIndicator"
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
                  <span v-else-if="hasLinkGroupWithItype" class="pa-1">
                    There are no Link Groups that match your search.
                  </span>
                  <span v-else class="pa-1">
                    There are no Link Groups for the <strong>{{ getActiveIndicator.itype }}</strong> iType.
                    <a class="no-decoration" href="settings#linkgroups">Create one!</a>
                  </span> <!-- /no link groups message -->
                </div> <!-- /link groups -->
              </div>
              </div>
            </div>
          </div>
        </div>
        <div v-if="shouldDisplayResults && !getLinkGroupsPanelOpen" class="side-panel-stub link-group-panel-stub h-100 cursor-pointer d-flex flex-column"
          v-tooltip:top="'Show Link Groups Panel'"
          @click="toggleLinkGroupsPanel">
          <span
            class="fa fa-link pa-1 mt-1"
          />
        </div>
      </div>
    </div> <!-- /page content -->
  </div>
</template>

<script>
import { mapGetters } from 'vuex';

import ActionDropdown from '@/utils/ActionDropdown.vue';
import IdTooltip from '@/utils/IdTooltip.vue';
import ReorderList from '@/utils/ReorderList.vue';
import TimeRangeInput from '@/utils/TimeRangeInput.vue';
import Focus from '@common/Focus.vue';
import ViewSelector from '@/components/views/ViewSelector.vue';
import UserService from '@/components/services/UserService';
import LinkGroupCard from '@/components/links/LinkGroupCard.vue';
import CreateViewModal from '@/components/views/CreateViewModal.vue';
import Cont3xtService from '@/components/services/Cont3xtService';
import IntegrationCard from '@/components/integrations/IntegrationCard.vue';
import OverviewCard from '@/components/overviews/OverviewCard.vue';
import IntegrationPanel from '@/components/integrations/IntegrationPanel.vue';
import TagDisplayLine from '@/utils/TagDisplayLine.vue';
import { paramStr } from '@/utils/paramStr';
import LinkService from '@/components/services/LinkService';
import OverviewService from '@/components/services/OverviewService';
import ITypeNode from '@/components/itypes/ITypeNode.vue';
import IntegrationBtns from '@/components/integrations/IntegrationBtns.vue';
import { indicatorFromId, indicatorParentId, localIndicatorId } from '@/utils/cont3xtUtil';
import { iTypes } from '@/utils/iTypes';
import { clipboardCopyText } from '@/utils/clipboardCopyText';
import { computed } from 'vue';

export default {
  name: 'Cont3xt',
  components: {
    ActionDropdown,
    IdTooltip,
    IntegrationBtns,
    ITypeNode,
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
      gettingCont3xt: false,
      viewModalOpen: false,
      dropdownActions: [
        {
          icon: 'fa-database',
          text: 'Skip Cache',
          tooltip: computed(() => this.skipCache ? 'Ignorning cache - click to use cache (shift + c)' : 'Using cache - click to ignore cache (shift + c)'),
          active: computed(() => this.skipCache),
          action: () => { this.skipCache = !this.skipCache; }
        },
        {
          icon: 'fa-child',
          text: 'Skip Children',
          tooltip: computed(() => this.skipChildren ? 'Ignorning child queries - select to enable child queries' : 'Including child queries - select to disable child queries'),
          active: computed(() => this.skipChildren),
          action: () => { this.skipChildren = !this.skipChildren; }
        },
        {
          icon: 'fa-file-text',
          text: 'Download Report',
          tooltip: computed(() => 'Download a report of this result (shift + r)'),
          disabled: computed(() => !this.searchComplete),
          action: this.generateReport
        },
        {
          icon: 'fa-share-alt',
          text: 'Copy Share Link',
          tooltip: computed(() => 'Copy share link to clipboard (shift + l)'),
          active: computed(() => this.activeShareLink),
          action: this.shareLink
        }
      ],
      iTypes,
      error: '',
      scrollPx: 0,
      initialized: false,
      searchTerm: this.$route.query.q ? this.$route.query.q : (this.$route.query.b && this.$route.query.b.length > 1 ? window.atob(this.$route.query.b) : ''),
      overrideOverviewId: undefined,
      skipCache: false,
      skipChildren: false,
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
      tagInput: '',
      overrideItype: undefined
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
      'getToggleCache', 'getToggleChildren', 'getDownloadReport', 'getCopyShareLink',
      'getAllViews', 'getImmediateSubmissionReady', 'getSelectedView',
      'getTags', 'getTagDisplayCollapsed', 'getSeeAllViews', 'getSeeAllLinkGroups',
      'getSeeAllOverviews', 'getSelectedOverviewMap', 'getOverviewMap', 'getResults',
      'getIndicatorGraph', 'getLinkGroupsPanelOpen', 'getActiveIndicator',
      'getResultTreeNavigationDirection', 'getCollapsedIndicatorNodeMap',
      'getCollapseOrExpandIndicatorRoots'
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
    activeIndicatorId: {
      get () { return this.$store.state.activeIndicatorId; },
      set (val) { this.$store.commit('SET_ACTIVE_INDICATOR_ID', val); }
    },
    activeSource: {
      get () { return this.$store.state.activeSource; },
      set (val) { this.$store.commit('SET_ACTIVE_SOURCE', val); }
    },
    shouldDisplayResults () {
      return this.getActiveIndicator != null;
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
      return this.getSelectedOverviewMap[this.getActiveIndicator.itype];
    },
    currentItype: {
      get () {
        if (this.overrideItype) { return this.overrideItype; }
        return this.getActiveIndicator?.itype;
      },
      set (val) { this.overrideItype = val; }
    },
    /** @returns {Cont3xtIndicatorNode[]} */
    indicatorTreeRoots () {
      return Object.values(this.getIndicatorGraph).filter(node => node.parentIds.has(undefined));
    },
    showOverview () {
      return this.activeSource == null && !(this.getWaitRendering || this.getRendering);
    },
    preOrderVisibleIndicators () {
      const ids = [];
      const traverseNode = (node, parentId) => {
        const id = `${(parentId == null) ? '' : `${parentId},`}${localIndicatorId(node.indicator)}`;
        ids.push(id);
        if (!this.getCollapsedIndicatorNodeMap[id]) {
          for (const childNode of node.children) {
            traverseNode(childNode, id);
          }
        }
      };

      for (const rootNode of this.indicatorTreeRoots) {
        traverseNode(rootNode, undefined);
      }

      return ids;
    }
  },
  watch: {
    getResultTreeNavigationDirection (direction) {
      if (direction == null || !this.shouldDisplayResults) { return; }

      switch (direction) {
      case 'up':
        this.navigateUpResultTree();
        break;
      case 'down':
        this.navigateDownResultTree();
        break;
      case 'left':
        this.navigateLeftResultTree();
        break;
      case 'right':
        this.navigateRightResultTree();
        break;
      }
    },
    getCollapseOrExpandIndicatorRoots (val) {
      if (val == null) { return; }

      for (const rootNode of this.indicatorTreeRoots) {
        const rootIndicatorId = localIndicatorId(rootNode.indicator);
        if (this.nodeIsCollapsable(rootIndicatorId) &&
            val.setRootsOpen === !!this.getCollapsedIndicatorNodeMap[rootIndicatorId]) {
          this.$store.commit('TOGGLE_INDICATOR_NODE_COLLAPSE', rootIndicatorId);
        }
      }
    },
    getQueuedIntegration (newQueuedIntegration) {
      this.activeSource = undefined;
      this.$store.commit('SET_RENDERING_CARD', true);
      // need wait rendering to tell the card that we aren't rendering yet
      // or else the data will be stale when it updates the integration type
      this.$store.commit('SET_WAIT_RENDERING', true);
      setTimeout(() => { // need timeout for SET_RENDERING_CARD to take effect
        this.activeIndicatorId = newQueuedIntegration.indicatorId;
        this.activeSource = newQueuedIntegration.source;
        this.$store.commit('SET_WAIT_RENDERING', false);
      }, 100);
    },
    activeIndicatorId (newIndicatorId, oldIndicatorId) {
      if (newIndicatorId !== oldIndicatorId) {
        this.overrideOverviewId = undefined;
        this.changeItype(this.getActiveIndicator?.itype);
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
      if (val) { // TODO: toby check refs
        this.$refs.actionDropdown.show();
        setTimeout(() => { this.skipCache = !this.skipCache; }, 100);
        setTimeout(() => { this.$refs.actionDropdown.hide(); }, 1000);
      }
    },
    getToggleChildren (val) {
      if (val) {
        this.$refs.actionDropdown.show();
        setTimeout(() => { this.skipChildren = !this.skipChildren; }, 100);
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
    changeItype (itype) {
      this.currentItype = itype;
    },
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
        this.activeIndicatorId = localIndicatorId(chunk.indicators[0]);
        this.filterLinks(this.linkSearchTerm);

        for (const indicator of chunk.indicators) {
          this.$store.commit('UPDATE_INDICATOR_GRAPH', {
            indicator, parentIndicator: undefined
          });
        }
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
        if (chunk.sent && chunk.total) { // add failure to the progress bar
          this.loading.failed++;
          this.loading.failures.push(`${chunk.name} (${chunk.indicator.query})`);
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
        this.loading.done = true;
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
      this.gettingCont3xt = true;
      setTimeout(() => {
        this.gettingCont3xt = false;
      }, 1000);
      if (this.searchTerm == null || this.searchTerm === '') {
        return; // do NOT search if the query is empty
      }

      this.error = '';
      this.$store.commit('CLEAR_CONT3XT_RESULTS');
      this.activeIndicatorId = undefined;
      this.activeSource = undefined;
      this.initialized = true;
      this.searchComplete = false;
      this.$store.commit('RESET_LOADING');
      this.overrideOverviewId = undefined;

      // only match on b because we remove the q param
      if (!this.$route.query.b || this.$route.query.b.length === 1 ||
          (this.$route.query.b && this.$route.query.b.length > 1 && window.atob(this.$route.query.b) !== this.searchTerm)
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
      Cont3xtService.search({ searchTerm: this.searchTerm, skipCache: this.skipCache, skipChildren: this.skipChildren, tags: this.tags, viewId }).subscribe({
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
        link.url !== '----------' && link.itypes.includes(this.getActiveIndicator.itype)
      );
    },
    hasVisibleLink (linkGroup) {
      return linkGroup.links.some((link, i) =>
        link.url !== '----------' && link.itypes.includes(this.getActiveIndicator.itype) && !this.hideLinks[linkGroup._id]?.[i]
      );
    },
    shareLink () {
      let shareLink = window.location.href;
      if (this.$route.query.b !== undefined || this.$route.query.q !== undefined) {
        // share link is given submit=y query param (only if some search parameter exists)
        const allSharedQueryParams = { ...this.$route.query, submit: 'y' };
        shareLink = `${window.location.origin}/${paramStr(allSharedQueryParams)}`;
      }
      clipboardCopyText(shareLink);
    },
    toggleAllVisibleLinkGroupsCollapse () {
      // if all are collapsed, open them all
      // if even one is open, close them all
      const allCollapsed = this.allVisibleLinkGroupsCollapsed;
      for (const lg of this.visibleLinkGroups) {
        this.collapsedLinkGroups[lg._id] = !allCollapsed;
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
    /* result tree navigation/manipulation --------------------------------- */
    navigateToResultNode (indicatorId) {
      // scroll and set active to the indicator node with this id
      this.activeIndicatorId = indicatorId;
      this.activeSource = undefined;
      // handled in BaseIType component with the corresponding indicatorId
      this.$store.commit('SET_INDICATOR_ID_TO_FOCUS', indicatorId);
    },
    navigateUpResultTree () {
      // proceed to the node that is visually above our current selection
      const resultDisplayIndex = this.preOrderVisibleIndicators.indexOf(this.activeIndicatorId);
      if (resultDisplayIndex > 0) {
        this.navigateToResultNode(this.preOrderVisibleIndicators[resultDisplayIndex - 1]);
      }
    },
    navigateDownResultTree () {
      // proceed to the node that is visually below our current selection
      const resultDisplayIndex = this.preOrderVisibleIndicators.indexOf(this.activeIndicatorId);
      if (resultDisplayIndex >= 0 && resultDisplayIndex < this.preOrderVisibleIndicators.length - 1) {
        this.navigateToResultNode(this.preOrderVisibleIndicators[resultDisplayIndex + 1]);
      }
    },
    navigateLeftResultTree () {
      // if the node is collapsable and not collapsed, collapse it--otherwise, go to the parent
      if (this.nodeIsCollapsable(this.activeIndicatorId) && !this.getCollapsedIndicatorNodeMap[this.activeIndicatorId]) {
        this.$store.commit('TOGGLE_INDICATOR_NODE_COLLAPSE', this.activeIndicatorId);
      } else {
        const parentId = indicatorParentId(this.activeIndicatorId);
        if (parentId) { this.navigateToResultNode(parentId); }
      }
    },
    navigateRightResultTree () {
      // if the node is collapsed, expand it--otherwise, go to first child, if there is one
      if (!this.nodeIsCollapsable(this.activeIndicatorId)) { return; }

      if (this.getCollapsedIndicatorNodeMap[this.activeIndicatorId]) {
        this.$store.commit('TOGGLE_INDICATOR_NODE_COLLAPSE', this.activeIndicatorId);
      } else {
        this.navigateDownResultTree();
      }
    },
    nodeIsCollapsable (indicatorId) {
      return this.nodeForIndicatorId(indicatorId).children.length > 0;
    },
    nodeForIndicatorId (indicatorId) {
      const localId = localIndicatorId(indicatorFromId(indicatorId));
      return this.getIndicatorGraph[localId];
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
  beforeUnmount () {
    this.$store.commit('RESET_LOADING');

    // clear results/selections from the store (so the current search is not presented when the user returns)
    this.$store.commit('CLEAR_CONT3XT_RESULTS');
    this.activeSource = undefined;
    this.activeIndicatorId = undefined;
  }
};
</script>

<style scoped>
.rocket-fly {
  animation: rocket-fly 0.82s cubic-bezier(0.36, 0.07, 0.19, 0.97) both;
  transform: translate(0, 0);
}

@keyframes rocket-fly {
  50% {
    transform: translate(30px, -30px);
  }
  51% {
    transform: translate(-30px, 30px);
  }
  100% {
    transform: translate(0, 0);
  }
}

.cont3xt-search-btn {
  width: 148px;
  overflow: hidden;
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
  background-color: rgb(var(--v-theme-well));
  border: 1px solid rgb(var(--v-theme-well-border));
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
  color: rgb(var(--v-theme-info));
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
  overflow-x: auto;
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
.search-query-shortcut { /* exactly fits the space of magnify icon */
  font-size: 20px;
  width: 24px !important;
  text-align: center;
}
</style>
