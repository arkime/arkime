<!--
Copyright Yahoo Inc.
SPDX-License-Identifier: Apache-2.0
-->
<template>
  <form class="position-relative pt-1">
    <!-- viz options button -->
    <div
      class="viz-options-btn-container"
      v-if="!actionForm && (basePath === 'spigraph' || basePath === 'sessions' || basePath === 'spiview' || basePath === 'arkime')">
      <BTooltip
        target="hideViz"
        :delay="{show: 0, hide: 0}"
        noninteractive
        boundary="viewport"
        placement="left"
        v-if="basePath !== 'spigraph'">
        {{ $t(!hideViz ? 'search.speedUpTip' : 'search.showGraphTip') }}
      </BTooltip>
      <template v-if="!hideViz && disabledAggregations">
        <BTooltip
          target="fetchVizQuery"
          :delay="{show: 0, hide: 0}"
          noninteractive
          boundary="viewport"
          placement="left">
          {{ $t('search.fetchVizQueryTip') }}
        </BTooltip>
        <BTooltip
          target="fetchVizSession"
          :delay="{show: 0, hide: 0}"
          noninteractive
          boundary="viewport"
          placement="left">
          {{ $t('search.fetchVizSessionTip') }}
        </BTooltip>
        <BTooltip
          target="fetchVizBrowser"
          :delay="{show: 0, hide: 0}"
          noninteractive
          boundary="viewport"
          placement="left">
          {{ $t('search.fetchVizBrowserTip') }}
        </BTooltip>
      </template>
      <b-dropdown
        split
        right
        size="sm"
        variant="primary"
        class="viz-options-btn">
        <template #button-content>
          <div @click="overrideDisabledAggregations(1)">
            <span class="fa fa-gear fa-fw" />
            <span v-if="!hideViz && disabledAggregations">
              {{ $t('search.fetchVizData') }}
            </span>
          </div>
        </template>
        <template v-if="!hideViz && disabledAggregations">
          <b-dropdown-item
            id="fetchVizQuery"
            v-i18n-bdd="'search.'"
            @click="overrideDisabledAggregations(1)" />
          <b-dropdown-item
            id="fetchVizSession"
            v-i18n-bdd="'search.'"
            @click="overrideDisabledAggregations(0)" />
          <b-dropdown-item
            id="fetchVizBrowser"
            v-i18n-bdd="'search.'"
            @click="overrideDisabledAggregations(-1)" />
        </template>
        <template v-if="forcedAggregations">
          <b-dropdown-item
            @click="overrideDisabledAggregations(undefined)">
            {{ $t('search.disableVis') }}
          </b-dropdown-item>
        </template>
        <b-dropdown-divider v-if="!hideViz && disabledAggregations" />
        <b-dropdown-item
          @click="toggleStickyViz">
          {{ !stickyViz ? 'Pin' : 'Unpin' }}{{ basePath && basePath === 'spigraph' ? ' top' : '' }} {{ basePath && basePath === 'sessions' ? 'graph, map, and column headers' : 'graph and map' }}
        </b-dropdown-item>
        <b-dropdown-item
          id="hideViz"
          @click="toggleHideViz"
          v-if="basePath !== 'spigraph'">
          {{ $t(!hideViz ? 'search.hideGraphMap' : 'search.showGraphMap') }}
        </b-dropdown-item>
      </b-dropdown>
    </div> <!-- /viz options button -->

    <div class="pe-1 ps-1 pt-1 pb-1">
      <!-- actions dropdown menu -->
      <b-dropdown
        v-if="!hideActions && $route.name === 'Sessions'"
        size="sm"
        class="pull-right ms-1 action-menu-dropdown"
        variant="theme-primary"
        title="Actions menu">
        <b-dropdown-item
          @click="exportPCAP"
          v-has-permission="'!disablePcapDownload'"
          :title="$t('sessions.exports.exportPCAP')">
          <span class="fa fa-fw fa-file-o" />&nbsp;
          {{ $t('sessions.exports.exportPCAP') }}
        </b-dropdown-item>
        <b-dropdown-item
          @click="exportCSV"
          title="$t('sessions.exports.exportCSV')">
          <span class="fa fa-fw fa-file-excel-o" />&nbsp;
          {{ $t('sessions.exports.exportCSV') }}
        </b-dropdown-item>
        <b-dropdown-item
          @click="addTags"
          :title="$t('sessions.tag.addTags')">
          <span class="fa fa-fw fa-tags" />&nbsp;
          {{ $t('sessions.tag.addTags') }}
        </b-dropdown-item>
        <b-dropdown-item
          @click="removeTags"
          v-has-permission="'removeEnabled'"
          :title="$t('sessions.tag.removeTags')">
          <span class="fa fa-fw fa-eraser" />&nbsp;
          {{ $t('sessions.tag.removeTags') }}
        </b-dropdown-item>
        <b-dropdown-item
          @click="removeData"
          v-has-permission="'removeEnabled'"
          :title="$t('search.removeData')">
          <span class="fa fa-fw fa-trash-o" />&nbsp;
          {{ $t('search.removeData') }}
        </b-dropdown-item>
        <b-dropdown-item
          v-for="(clusterInfo, key) in arkimeClusters"
          :key="key"
          @click="sendSession(key)"
          :title="`$t('search.sendSession', { name: clusterInfo.name })`">
          <span class="fa fa-fw fa-paper-plane-o" />&nbsp;
          {{ $t('search.sendSession', { name: clusterInfo.name }) }}
        </b-dropdown-item>
        <b-dropdown-item
          @click="viewIntersection"
          :title="$t('sessions.intersection.title')">
          <span class="fa fa-fw fa-venn">
            <span class="fa fa-circle-o" />
            <span class="fa fa-circle-o" />
          </span>&nbsp;
          {{ $t('sessions.intersection.title') }}
        </b-dropdown-item>
        <b-dropdown-item
          v-if="!multiviewer"
          @click="periodicQuery"
          :title="$t('search.createPeriodicQuery')">
          <span class="fa fa-fw fa-search" />&nbsp;
          {{ $t('search.createPeriodicQuery') }}
        </b-dropdown-item>
      </b-dropdown> <!-- /actions dropdown menu -->

      <!-- views dropdown menu -->
      <b-dropdown
        right
        size="sm"
        class="pull-right ms-1 view-menu-dropdown"
        no-caret
        toggle-class="rounded"
        variant="theme-secondary">
        <template #button-content>
          <template v-if="view && views && getView(view)">
            <div id="viewMenuDropdown">
              <span class="fa fa-eye" />
              <span v-if="view">{{ getView(view).name || view }}</span>
              <span class="sr-only">{{ $t('common.views') }}</span>
              <BTooltip target="viewMenuDropdown">
                {{ getView(view).expression || '' }}
              </BTooltip>
            </div>
          </template>
          <template v-else>
            <span class="fa fa-eye" />
            <span class="sr-only">{{ $t('common.views') }}</span>
          </template>
        </template>
        <b-dropdown-item
          @click="modView()"
          title="Create a new view">
          <span class="fa fa-plus-circle" />&nbsp;
          {{ $t('search.newView') }}
        </b-dropdown-item>
        <b-dropdown-divider />
        <b-dropdown-item
          @click="setView(undefined)"
          :class="{'active':!view}">
          {{ $t('common.none') }}
        </b-dropdown-item>
        <b-dropdown-item
          v-for="(value, index) in views"
          :id="`view${value.id}`"
          :key="value.id"
          :class="{'active':view === value.id}"
          @click.self="setView(value.id)">
          <span
            v-if="value.shared"
            class="fa fa-share-square" />
          <!-- view action buttons -->
          <template v-if="canEditView(value)">
            <button
              :id="`deleteView${value.id}`"
              type="button"
              class="btn btn-xs btn-danger pull-right ms-1"
              @click.stop.prevent="deleteView(value.id, index)">
              <span class="fa fa-trash-o" />
              <BTooltip :target="`deleteView${value.id}`">
                {{ $t('search.deleteView') }}
              </BTooltip>
            </button>
            <button
              :id="`editView${value.id}`"
              type="button"
              @click.stop.prevent="modView(views[index])"
              class="btn btn-xs btn-warning pull-right ms-1">
              <span class="fa fa-edit" />
              <BTooltip :target="`editView${value.id}`">
                {{ $t('search.editView') }}
              </BTooltip>
            </button>
          </template>
          <button
            :id="`applyView${value.id}`"
            class="btn btn-xs btn-theme-secondary pull-right ms-1"
            type="button"
            @click.stop.prevent="applyView(value)">
            <span class="fa fa-share fa-flip-horizontal" />
            <BTooltip :target="`applyView${value.id}`">
              {{ $t('search.applyView') }}
            </BTooltip>
          </button>
          <button
            v-if="value.sessionsColConfig && $route.name === 'Sessions'"
            :id="`applyColumns${value.id}`"
            class="btn btn-xs btn-theme-tertiary pull-right"
            type="button"
            @click.stop.prevent="applyColumns(value)">
            <span class="fa fa-columns" />
            <BTooltip :target="`applyColumns${value.id}`">
              {{ $t('search.applyColumns') }}
            </BTooltip>
          </button>
          <!-- /view action buttons -->
          {{ value.name }}&nbsp;
          <BTooltip :target="`view${value.id}`">
            {{ value.expression }}
          </BTooltip>
        </b-dropdown-item>
      </b-dropdown> <!-- /views dropdown menu -->

      <Clusters :select-one="$route.name === 'Hunt'" /> <!-- cluster dropdown menu -->

      <!-- search button -->
      <a
        class="btn btn-sm btn-theme-tertiary pull-right ms-1 search-btn"
        @click="applyParams"
        tabindex="2">
        <span v-if="!shiftKeyHold">
          {{ $t('common.search') }}
        </span>
        <span
          v-else
          class="enter-icon">
          <span class="fa fa-long-arrow-left fa-lg" />
          <div class="enter-arm" />
        </span>
      </a> <!-- /search button -->

      <!-- search box typeahead -->
      <expression-typeahead
        @mod-view="modView"
        @apply-expression="applyParams"
        @change-expression="changeExpression" /> <!-- /search box typeahead -->

      <!-- time inputs -->
      <arkime-time
        :timezone="user.settings.timezone"
        @time-change="timeChange"
        :hide-interval="hideInterval"
        :update-time="updateTime" /> <!-- /time inputs -->

      <!-- form message -->
      <div class="small mt-1">
        <arkime-toast
          :message="message"
          :type="messageType"
          :done="messageDone" />
      </div> <!-- /form message -->

      <div v-if="actionForm">
        <hr class="action-form-separator">
        <div class="row d-flex gap-1">
          <div
            v-if="showApplyButtons"
            class="col-auto">
            <BTooltip
              target="visibleSessions"
              noninteractive>
              {{ visibleItemsTooltip }}
            </BTooltip>
            <BTooltip
              target="openSessions"
              noninteractive>
              {{ openItemsTooltip }}
            </BTooltip>
            <BTooltip
              target="matchingSessions"
              noninteractive>
              {{ matchingItemsTooltip }}
            </BTooltip>
            <BFormRadioGroup
              buttons
              size="sm"
              :model-value="actionFormItemRadio"
              @update:model-value="newVal => actionFormItemRadio = newVal"
              class="mb-0">
              <BFormRadio value="open">
                <template #default>
                  <div id="openSessions">
                    {{ $t('search.open') }}
                  </div>
                </template>
              </BFormRadio>
              <BFormRadio value="visible">
                <template #default>
                  <div id="visibleSessions">
                    {{ $t('search.visible') }}
                  </div>
                </template>
              </BFormRadio>
              <BFormRadio value="matching">
                <template #default>
                  <div id="matchingSessions">
                    {{ $t('search.matching') }}
                  </div>
                </template>
              </BFormRadio>
            </BFormRadioGroup>
          </div>
          <!-- actions menu forms -->
          <div class="col-auto flex-grow-1">
            <arkime-modify-view
              v-if="actionForm === 'modify:view'"
              @done="actionFormDone"
              :edit-view="editableView"
              :initial-expression="expression"
              @set-view="setView" />
            <arkime-tag-sessions
              v-else-if="actionForm === 'add:tags' || actionForm === 'remove:tags'"
              :add="actionForm === 'add:tags'"
              :start="start"
              @done="actionFormDone"
              :sessions="openSessions"
              :num-visible="numVisibleSessions"
              :num-matching="numMatchingSessions"
              :apply-to="actionFormItemRadio" />
            <arkime-remove-data
              v-else-if="actionForm === 'remove:data'"
              :start="start"
              @done="actionFormDone"
              :sessions="openSessions"
              :num-visible="numVisibleSessions"
              :num-matching="numMatchingSessions"
              :apply-to="actionFormItemRadio" />
            <arkime-send-sessions
              v-else-if="actionForm === 'send:session'"
              :start="start"
              :cluster="cluster"
              @done="actionFormDone"
              :sessions="openSessions"
              :num-visible="numVisibleSessions"
              :num-matching="numMatchingSessions"
              :apply-to="actionFormItemRadio" />
            <arkime-export-pcap
              v-else-if="actionForm === 'export:pcap'"
              @done="actionFormDone"
              :start="start"
              :sessions="openSessions"
              :num-visible="numVisibleSessions"
              :num-matching="numMatchingSessions"
              :apply-to="actionFormItemRadio" />
            <arkime-export-csv
              v-else-if="actionForm === 'export:csv'"
              :start="start"
              :fields="fields"
              @done="actionFormDone"
              :sessions="openSessions"
              :num-visible="numVisibleSessions"
              :num-matching="numMatchingSessions"
              :apply-to="actionFormItemRadio" />
            <arkime-intersection
              v-else-if="actionForm === 'view:intersection'"
              @done="actionFormDone"
              :fields="fields" />
          </div> <!-- /actions menu forms -->
        </div>
      </div>
    </div>
  </form>
</template>

<script>
import SettingsService from '../settings/SettingsService';
import ExpressionTypeahead from './ExpressionTypeahead.vue';
import ArkimeTime from './Time.vue';
import ArkimeToast from '../utils/Toast.vue';
import ArkimeModifyView from '../sessions/ModifyView.vue';
import ArkimeTagSessions from '../sessions/Tags.vue';
import ArkimeRemoveData from '../sessions/Remove.vue';
import ArkimeSendSessions from '../sessions/Send.vue';
import ArkimeExportPcap from '../sessions/ExportPcap.vue';
import ArkimeExportCsv from '../sessions/ExportCsv.vue';
import ArkimeIntersection from '../sessions/Intersection.vue';
import Clusters from '../utils/Clusters.vue';
import { commaString } from '@common/vueFilters.js';

export default {
  name: 'ArkimeSearch',
  components: {
    ExpressionTypeahead,
    ArkimeTime,
    ArkimeToast,
    ArkimeModifyView,
    ArkimeTagSessions,
    ArkimeRemoveData,
    ArkimeSendSessions,
    ArkimeExportPcap,
    ArkimeExportCsv,
    ArkimeIntersection,
    Clusters
  },
  props: {
    openSessions: {
      type: Array,
      default: () => []
    },
    numVisibleSessions: {
      type: Number,
      default: 0
    },
    numMatchingSessions: {
      type: Number,
      default: 0
    },
    start: {
      type: Number,
      default: 0
    },
    fields: {
      type: Array,
      default: () => []
    },
    hideActions: {
      type: Boolean,
      default: false
    },
    hideInterval: {
      type: Boolean,
      default: false
    }
  },
  emits: [
    'changeSearch',
    'recalc-collapse',
    'setView',
    'setColumns'
  ],
  data: function () {
    return {
      actionFormItemRadio: 'visible',
      actionForm: undefined,
      showApplyButtons: false,
      cluster: {},
      view: this.$route.query.view,
      message: undefined,
      messageType: undefined,
      updateTime: 'false',
      editableView: undefined, // Not necessarily active view
      multiviewer: this.$constants.MULTIVIEWER,
      basePath: undefined
    };
  },
  computed: {
    expression: {
      get: function () {
        return this.$store.state.expression;
      },
      set: function (newValue) {
        this.$store.commit('setExpression', newValue);
      }
    },
    issueSearch: function () {
      return this.$store.state.issueSearch;
    },
    shiftKeyHold: function () {
      return this.$store.state.shiftKeyHold;
    },
    views: {
      get: function () {
        return this.$store.state.views;
      },
      set: function (newValue) {
        this.$store.commit('setViews', newValue);
      }
    },
    user: function () {
      return this.$store.state.user;
    },
    arkimeClusters: function () {
      return this.$store.state.remoteclusters;
    },
    openItemsTooltip: function () {
      return this.$t('search.applyOpen', { count: commaString(this.openSessions.length) });
    },
    visibleItemsTooltip: function () {
      return this.$t('search.applyVisible', { count: commaString(Math.min(this.numVisibleSessions, this.numMatchingSessions)) });
    },
    matchingItemsTooltip: function () {
      return this.$t('search.applyMatching', { count: commaString(this.numMatchingSessions) });
    },
    stickyViz: {
      get: function () { return this.$store.state.stickyViz; },
      set: function (newValue) { this.$store.commit('toggleStickyViz', newValue); }
    },
    hideViz: {
      get: function () { return this.$store.state.hideViz; },
      set: function (newValue) { this.$store.commit('toggleHideViz', newValue); }
    },
    disabledAggregations: function () {
      return this.$store.state.disabledAggregations;
    },
    forcedAggregations: function () {
      return this.$store.state.forcedAggregations;
    }
  },
  watch: {
    // watch route update of view param
    '$route.query.view': function (newVal, oldVal) {
      this.view = newVal;
      sessionStorage['moloch-view'] = newVal;

      this.$emit('changeSearch', {
        expression: this.expression,
        view: this.view
      });
    },
    issueSearch: function (newVal, oldVal) {
      if (newVal) { this.applyParams(); }
    },
    actionForm: function () {
      this.$emit('recalc-collapse');
    }
  },
  created: function () {
    this.basePath = this.$route.path.split('/')[1];

    this.stickyViz = localStorage && localStorage[`${this.basePath}-sticky-viz`] &&
      localStorage[`${this.basePath}-sticky-viz`] !== 'false';

    this.hideViz = localStorage && localStorage[`${this.basePath}-hide-viz`] &&
      localStorage[`${this.basePath}-hide-viz`] !== 'false';
  },
  methods: {
    /* exposed page functions ------------------------------------ */
    messageDone: function () {
      this.message = undefined;
      this.messageType = undefined;
      this.$emit('recalc-collapse');
    },
    applyExpression: function (expression) {
      if (!this.expression) { this.expression = undefined; }
      this.$router.push({
        query: {
          ...this.$route.query,
          expression: this.expression
        },
        params: { nav: true }
      });
    },
    changeExpression: function () {
      this.timeUpdate();
    },
    applyParams: function () {
      if (this.$route.query.expression !== this.expression) {
        this.applyExpression();
      } else {
        this.timeUpdate();
      }
    },
    exportPCAP: function () {
      this.actionForm = 'export:pcap';
      this.showApplyButtons = true;
    },
    exportCSV: function () {
      this.actionForm = 'export:csv';
      this.showApplyButtons = true;
    },
    addTags: function () {
      this.actionForm = 'add:tags';
      this.showApplyButtons = true;
    },
    removeTags: function () {
      this.actionForm = 'remove:tags';
      this.showApplyButtons = true;
    },
    removeData: function () {
      this.actionForm = 'remove:data';
      this.showApplyButtons = true;
    },
    sendSession: function (cluster) {
      this.cluster = cluster;
      this.actionForm = 'send:session';
      this.showApplyButtons = true;
    },
    modView: function (view) {
      this.editableView = undefined;
      this.actionForm = undefined;

      this.$nextTick(() => {
        this.editableView = view;
        this.actionForm = 'modify:view';
        this.showApplyButtons = false;
      });
    },
    viewIntersection: function () {
      this.actionForm = 'view:intersection';
      this.showApplyButtons = false;
    },
    periodicQuery: function () {
      const params = new URLSearchParams();

      if (this.expression) {
        params.append('expression', this.expression);
      }

      if (this.$store.state.timeRange && this.$store.state.timeRange !== '0') {
        params.append('process', this.$store.state.timeRange);
      }

      window.open(`settings?${params.toString()}#cron`, '_blank');
    },
    actionFormDone: function (message, success) {
      // If a view was being edited, remove selection name
      this.editableView = undefined;
      this.actionForm = undefined;

      if (message) {
        this.message = message;
        this.messageType = success ? 'success' : 'warning';
      }
    },
    canEditView: function (view) {
      return this.user.roles.includes('arkimeAdmin') || (view.user && view.user === this.user.userId);
    },
    deleteView: function (viewId, index) {
      SettingsService.deleteView(viewId, this.user.userId).then((response) => {
        // check if deleting current view
        if (this.view === viewId) {
          this.setView(undefined);
        }
        // remove the view from the view list
        this.views.splice(index, 1);
        // display success message to user
        this.msg = response.text;
        this.msgType = 'success';
      }).catch((error) => {
        // display error message to user
        this.msg = error.text;
        this.msgType = 'danger';
      });
    },
    setView: function (viewId) {
      this.view = viewId;

      // update the url and session storage (to persist user's choice)
      // triggers the '$route.query.view' watcher that issues changeSearch event
      sessionStorage['moloch-view'] = viewId;
      if (this.$route.query.view !== viewId) { // view name changed
        this.$router.push({
          query: {
            ...this.$route.query,
            view: viewId
          }
        });

        this.$emit('setView');
      } else { // view name is the same but expression or columns are different
        this.$emit('changeSearch');
      }
    },
    applyView: function (view) {
      this.expression = view.expression;
      this.$store.commit('setFocusSearch', true);
      setTimeout(() => { // unfocus input for further re-focusing
        this.$store.commit('setFocusSearch', false);
      }, 1000);
    },
    getView: function (viewId) {
      return this.views.find(v => v.id === viewId || v.name === viewId);
    },
    applyColumns: function (view) {
      this.$emit('setColumns', view.sessionsColConfig);
    },
    toggleStickyViz: function () {
      this.stickyViz = !this.stickyViz;
      localStorage[`${this.basePath}-sticky-viz`] = this.stickyViz;
    },
    toggleHideViz: function () {
      this.hideViz = !this.hideViz;
      localStorage[`${this.basePath}-hide-viz`] = this.hideViz;
      if (!this.hideViz) { // watch for $store.state.fetchGraphData
        // wherever graph data needs to be updated when viz is shown
        this.$store.commit('setFetchGraphData', true);
        setTimeout(() => {
          this.$store.commit('setFetchGraphData', false);
        }, 500);
      }
    },
    /**
     * Overrides the server's disabling of aggregations on large time ranges
     * @param {number} option - How long to disable the aggregation
     *                          -1 = forever
     *                          0  = this session
     *                          1  = once
     */
    overrideDisabledAggregations: function (option) {
      if (option === undefined) {
        localStorage['force-aggregations'] = false;
        sessionStorage['force-aggregations'] = false;
        this.$store.commit('setForcedAggregations', false);
        return;
      }

      if (option === -1) {
        localStorage['force-aggregations'] = true;
        sessionStorage['force-aggregations'] = true;
        this.$store.commit('setForcedAggregations', true);
      } else {
        sessionStorage['force-aggregations'] = true;
        this.$store.commit('setForcedAggregations', true);
      }

      if (this.hideViz || this.disabledAggregations) { // data is missing
        this.$store.commit('setFetchGraphData', true); // fetch the data
      }

      this.hideViz = false;
      setTimeout(() => {
        this.$store.commit('setFetchGraphData', false); // unset for future data fetching
        this.$store.commit('setForcedAggregations', true);
        if (option === 1) { // if just override just once, unset it for future calls to disable aggs
          localStorage['force-aggregations'] = false;
          sessionStorage['force-aggregations'] = false;
        }
      }, 500);
    },
    /**
     * If the start/stop time has changed:
     * Applies the date start/stop time url parameters and removes the date url parameter
     * Updating the url parameter triggers updateParams in Time.vue
     * If just a search was issued:
     * Update the start/stop time in the time component so that the query that is
     * issued has the correct start/stop time (date only sent if -1)
     */
    timeUpdate: function () {
      let changed = false;

      if (this.$store.state.timeRange === '0' &&
        this.$store.state.time.startTime && this.$store.state.time.stopTime) {
        if (this.user.timeLimit) {
          // make sure the query doesn't exceed the user time limit
          const deltaTime = this.$store.state.time.stopTime - this.$store.state.time.startTime;

          // make sure the time range does not exceed the user setting
          const deltaTimeHrs = deltaTime / 3600;
          if (deltaTimeHrs > this.user.timeLimit) {
            return;
          }
        }

        if (parseInt(this.$store.state.time.startTime) !== parseInt(this.$route.query.startTime) ||
          parseInt(this.$store.state.time.stopTime) !== parseInt(this.$route.query.stopTime)) {
          changed = true;
        }

        this.$router.push({
          query: {
            ...this.$route.query,
            date: undefined,
            stopTime: this.$store.state.time.stopTime,
            startTime: this.$store.state.time.startTime
          }
        });
      }

      this.updateTime = !changed ? 'query' : 'true'; // issue a query if the time
      // hasn't changed, otherwise just update the time in the time component
      this.$nextTick(() => { this.updateTime = 'false'; });
    },
    /* event functions ------------------------------------------- */
    /**
     * Triggered when a time value is changed in the Time component
     * If the expression has changed, but has not been applied to the
     * url query parameters, apply it to url (this kicks off query by
     * triggering changeExpression, then timeUpdate)
     * If the expression has not changed, just tell the parent component
     * that the time has changed, so it should issue a query
     */
    timeChange: function () {
      if (this.$route.query.expression !== this.expression) {
        this.applyExpression();
      } else {
        this.$emit('changeSearch');
      }
    }
  }
};
</script>

<style>
.view-menu-dropdown .dropdown-menu {
  width: 300px;
}
</style>

<style>
.multies-menu-dropdown .dropdown-menu {
  /* max-height: 300px; */
  /* overflow: auto; */
  width: 300px
}

.multies-menu-dropdown .dropdown-header {
  padding: .25rem .5rem 0;
}

.multies-menu-dropdown .group-header {
  text-transform: uppercase;
  margin-top: 8px;
  padding: .2rem;
  /* font-size: 120%; */
  font-weight: bold;
}
</style>

<style scoped>
form {
  border: none;
  z-index: 5;
  background-color: var(--color-secondary-lightest);
  -webkit-box-shadow: 0 0 16px -2px black;
     -moz-box-shadow: 0 0 16px -2px black;
          box-shadow: 0 0 16px -2px black;
}
.action-form-separator {
  margin: 5px 0;
  border-top: 1px solid var(--color-gray-light);
}
/* make sure action menu dropdown is above all the things
 * but specifically above the sticky sessions button */
.action-menu-dropdown { z-index: 1030; }

/* viz options button position above viz in nav */
.viz-options-btn-container {
  top: 122px;
  right: 4px;
  position: fixed;
}
</style>
