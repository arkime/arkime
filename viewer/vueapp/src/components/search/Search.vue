<template>

  <form class="position-relative">

    <!-- viz options button -->
    <div class="viz-options-btn-container"
      v-if="!actionForm && (basePath === 'spigraph' || basePath === 'sessions' || basePath === 'spiview')">
      <b-dropdown
        split
        right
        size="sm"
        variant="primary"
        class="viz-options-btn"
        @click="overrideDisabledAggregations(1)">
        <template #button-content>
          <span class="fa fa-gear fa-fw" />
          <span v-if="!hideViz && disabledAggregations">
            Fetch Viz Data
          </span>
        </template>
        <template v-if="!hideViz && disabledAggregations">
          <b-dropdown-item
            @click="overrideDisabledAggregations(1)"
            v-b-tooltip.hover.left="'might take a while'">
            Fetch vizualizations for this query
          </b-dropdown-item>
          <b-dropdown-item
            @click="overrideDisabledAggregations(0)"
            v-b-tooltip.hover.left="'slows down future searches until you close this tab'">
            Fetch vizualizations for this browser session
          </b-dropdown-item>
          <b-dropdown-item
            @click="overrideDisabledAggregations(-1)"
            v-b-tooltip.hover.left="'slows down future searches until you turn it off'">
            Always fetch vizualizations for this browser
          </b-dropdown-item>
        </template>
        <template v-if="forcedAggregations">
          <b-dropdown-item
            @click="overrideDisabledAggregations(undefined)">
            Disable forced vizualizations
          </b-dropdown-item>
        </template>
        <b-dropdown-divider v-if="!hideViz && disabledAggregations" />
        <b-dropdown-item
          @click="toggleStickyViz">
          {{ !stickyViz ? 'Pin' : 'Unpin' }}{{ basePath && basePath === 'spigraph' ? ' top' : '' }} {{ basePath && basePath === 'sessions' ? 'graph, map, and column headers' : 'graph and map' }}
        </b-dropdown-item>
        <b-dropdown-item
          @click="toggleHideViz"
          v-if="basePath !== 'spigraph'"
          v-b-tooltip.hover.left="!hideViz ? 'Speeds up large queries!' : 'Show graph & map'">
          {{ !hideViz ? 'Hide' : 'Show' }} graph and map
        </b-dropdown-item>
      </b-dropdown>
    </div> <!-- /viz options button -->

    <div class="pr-1 pl-1 pt-1 pb-1">

      <!-- actions dropdown menu -->
      <b-dropdown v-if="!hideActions && $route.name === 'Sessions'"
        right
        size="sm"
        class="pull-right ml-1 action-menu-dropdown"
        boundary="body"
        variant="theme-primary"
        title="Actions menu">
        <b-dropdown-item @click="exportPCAP"
          v-has-permission="'!disablePcapDownload'"
          title="Export PCAP">
          <span class="fa fa-fw fa-file-o"></span>&nbsp;
          Export PCAP
        </b-dropdown-item>
        <b-dropdown-item @click="exportCSV"
          title="Export CSV">
          <span class="fa fa-fw fa-file-excel-o"></span>&nbsp;
          Export CSV
        </b-dropdown-item>
        <b-dropdown-item @click="addTags"
          title="Add Tags">
          <span class="fa fa-fw fa-tags"></span>&nbsp;
          Add Tags
        </b-dropdown-item>
        <b-dropdown-item @click="removeTags"
          v-has-permission="'removeEnabled'"
          title="Remove Tags">
          <span class="fa fa-fw fa-eraser"></span>&nbsp;
          Remove Tags
        </b-dropdown-item>
        <b-dropdown-item @click="removeData"
          v-has-permission="'removeEnabled'"
          title="Remove Data">
          <span class="fa fa-fw fa-trash-o"></span>&nbsp;
          Remove Data
        </b-dropdown-item>
        <b-dropdown-item v-for="(cluster, key) in molochClusters"
          :key="key"
          @click="sendSession(key)"
          :title="`Send to ${cluster.name}`">
          <span class="fa fa-fw fa-paper-plane-o"></span>&nbsp;
          Send to {{ cluster.name }}
        </b-dropdown-item>
        <b-dropdown-item @click="viewIntersection"
          title="Export Intersection">
          <span class="fa fa-fw fa-venn">
            <span class="fa fa-circle-o">
            </span>
            <span class="fa fa-circle-o">
            </span>
          </span>&nbsp;
          Export Intersection
        </b-dropdown-item>
        <b-dropdown-item
          v-if="!multiviewer"
          @click="periodicQuery"
          title="Create Periodic Query">
          <span class="fa fa-fw fa-search" />&nbsp;
          Create Periodic Query
        </b-dropdown-item>
      </b-dropdown> <!-- /actions dropdown menu -->

      <!-- views dropdown menu -->
      <b-dropdown right
        size="sm"
        class="pull-right ml-1 view-menu-dropdown"
        no-caret
        toggle-class="rounded"
        variant="theme-secondary">
        <template slot="button-content">
          <div v-if="view && views && getView(view)"
            v-b-tooltip.hover.left
            :title="getView(view).expression || ''">
            <span class="fa fa-eye"></span>
            <span v-if="view">{{ getView(view).name || view }}</span>
            <span class="sr-only">Views</span>
          </div>
          <div v-else>
            <span class="fa fa-eye"></span>
            <span class="sr-only">Views</span>
          </div>
        </template>
        <b-dropdown-item @click="modView()"
          title="Create a new view">
          <span class="fa fa-plus-circle"></span>&nbsp;
          New View
        </b-dropdown-item>
        <b-dropdown-divider></b-dropdown-divider>
        <b-dropdown-item @click="setView(undefined)"
          :class="{'active':!view}">
          None
        </b-dropdown-item>
        <b-dropdown-item v-for="(value, index) in views"
          :key="value.id"
          :class="{'active':view === value.id}"
          @click.self="setView(value.id)"
          v-b-tooltip.hover.left
          :title="value.expression">
          <span v-if="value.shared"
            class="fa fa-share-square">
          </span>
          <!-- view action buttons -->
          <template v-if="canEditView(value)">
            <button
              type="button"
              v-b-tooltip.hover.top
              title="Delete this view."
              class="btn btn-xs btn-danger pull-right ml-1"
              @click.stop.prevent="deleteView(value.id, index)">
              <span class="fa fa-trash-o">
              </span>
            </button>
            <button
              type="button"
              v-b-tooltip.hover.top
              title="Edit this view."
              @click.stop.prevent="modView(views[index])"
              class="btn btn-xs btn-warning pull-right ml-1">
              <span class="fa fa-edit">
              </span>
            </button>
          </template>
          <button class="btn btn-xs btn-theme-secondary pull-right ml-1"
            type="button"
            v-b-tooltip.hover.top
            title="Put this view's search expression into the search input. Note: this does not issue a search."
            @click.stop.prevent="applyView(value)">
            <span class="fa fa-share fa-flip-horizontal">
            </span>
          </button>
          <button v-if="value.sessionsColConfig && $route.name === 'Sessions'"
            class="btn btn-xs btn-theme-tertiary pull-right"
            type="button"
            v-b-tooltip.hover.top
            title="Apply this view's column configuration to the sessions table. Note: this will issue a search and update the sessions table columns"
            @click.stop.prevent="applyColumns(value)">
            <span class="fa fa-columns">
            </span>
          </button> <!-- /view action buttons -->
          {{ value.name }}&nbsp;
        </b-dropdown-item>
      </b-dropdown> <!-- /views dropdown menu -->

      <Clusters /> <!-- cluster dropdown menu -->

      <!-- search button -->
      <a class="btn btn-sm btn-theme-tertiary pull-right ml-1 search-btn"
        @click="applyParams"
        tabindex="2">
        <span v-if="!shiftKeyHold">
          Search
        </span>
        <span v-else
          class="enter-icon">
          <span class="fa fa-long-arrow-left fa-lg">
          </span>
          <div class="enter-arm">
          </div>
        </span>
      </a> <!-- /search button -->

      <!-- search box typeahead -->
      <expression-typeahead
        @modView="modView"
        @applyExpression="applyParams"
        @changeExpression="changeExpression">
      </expression-typeahead> <!-- /search box typeahead -->

      <!-- time inputs -->
      <moloch-time
        :timezone="user.settings.timezone"
        @timeChange="timeChange"
        :hide-interval="hideInterval"
        :updateTime="updateTime">
      </moloch-time> <!-- /time inputs -->

      <!-- form message -->
      <div class="small mt-1">
        <moloch-toast :message="message"
          :type="messageType"
          :done="messageDone">
        </moloch-toast>
      </div> <!-- /form message -->

      <div v-if="actionForm">
        <hr class="action-form-separator">
        <div class="row">
          <div v-if="showApplyButtons"
            class="col-md-3">
            <b-form-group>
              <b-form-radio-group
                size="sm"
                buttons
                v-model="actionFormItemRadio">
                <b-radio
                  value="open"
                  class="btn-radio"
                  v-b-tooltip.hover
                  :title="openItemsTooltip">
                  Open Items
                </b-radio>
                <b-radio
                  value="visible"
                  class="btn-radio"
                  v-b-tooltip.hover
                  :title="visibleItemsTooltip">
                  Visible Items
                </b-radio>
                <b-radio
                  value="matching"
                  class="btn-radio"
                  v-b-tooltip.hover
                  :title="matchingItemsTooltip">
                  Matching Items
                </b-radio>
              </b-form-radio-group>
            </b-form-group>
          </div>
          <!-- actions menu forms -->
          <div :class="{'col-md-9':showApplyButtons,'col-md-12':!showApplyButtons}">
            <moloch-modify-view v-if="actionForm === 'modify:view'"
              :done="actionFormDone"
              :editView="editableView"
              :initialExpression="expression"
              @setView="setView">
            </moloch-modify-view>
            <moloch-tag-sessions v-else-if="actionForm === 'add:tags' || actionForm === 'remove:tags'"
              :add="actionForm === 'add:tags'"
              :start="start"
              :done="actionFormDone"
              :sessions="openSessions"
              :num-visible="numVisibleSessions"
              :num-matching="numMatchingSessions"
              :apply-to="actionFormItemRadio">
            </moloch-tag-sessions>
            <moloch-remove-data v-else-if="actionForm === 'remove:data'"
              :start="start"
              :done="actionFormDone"
              :sessions="openSessions"
              :num-visible="numVisibleSessions"
              :num-matching="numMatchingSessions"
              :apply-to="actionFormItemRadio">
            </moloch-remove-data>
            <moloch-send-sessions v-else-if="actionForm === 'send:session'"
              :start="start"
              :cluster="cluster"
              :done="actionFormDone"
              :sessions="openSessions"
              :num-visible="numVisibleSessions"
              :num-matching="numMatchingSessions"
              :apply-to="actionFormItemRadio">
            </moloch-send-sessions>
            <moloch-export-pcap v-else-if="actionForm === 'export:pcap'"
              :start="start"
              :done="actionFormDone"
              :sessions="openSessions"
              :num-visible="numVisibleSessions"
              :num-matching="numMatchingSessions"
              :apply-to="actionFormItemRadio">
            </moloch-export-pcap>
            <moloch-export-csv v-else-if="actionForm === 'export:csv'"
              :start="start"
              :fields="fields"
              :done="actionFormDone"
              :sessions="openSessions"
              :num-visible="numVisibleSessions"
              :num-matching="numMatchingSessions"
              :apply-to="actionFormItemRadio">
            </moloch-export-csv>
            <moloch-intersection v-else-if="actionForm === 'view:intersection'"
              :done="actionFormDone"
              :fields="fields">
            </moloch-intersection>
          </div> <!-- /actions menu forms -->
        </div>
      </div>

    </div>
  </form>

</template>

<script>
import SettingsService from '../settings/SettingsService';
import ExpressionTypeahead from './ExpressionTypeahead';
import MolochTime from './Time';
import MolochToast from '../utils/Toast';
import MolochModifyView from '../sessions/ModifyView';
import MolochTagSessions from '../sessions/Tags';
import MolochRemoveData from '../sessions/Remove';
import MolochSendSessions from '../sessions/Send';
import MolochExportPcap from '../sessions/ExportPcap';
import MolochExportCsv from '../sessions/ExportCsv';
import MolochIntersection from '../sessions/Intersection';
import Clusters from '../utils/Clusters';

export default {
  name: 'MolochSearch',
  components: {
    ExpressionTypeahead,
    MolochTime,
    MolochToast,
    MolochModifyView,
    MolochTagSessions,
    MolochRemoveData,
    MolochSendSessions,
    MolochExportPcap,
    MolochExportCsv,
    MolochIntersection,
    Clusters
  },
  props: [
    'openSessions',
    'numVisibleSessions',
    'numMatchingSessions',
    'start',
    'fields',
    'hideActions',
    'hideInterval'
  ],
  data: function () {
    return {
      actionFormItemRadio: 'visible',
      actionFormItemRadioOptions: [
        { text: 'Open Item', value: 'open' },
        { text: 'Visible Items', value: 'visible' },
        { text: 'Matching Items', value: 'matching' }
      ],
      actionForm: undefined,
      showApplyButtons: false,
      cluster: {},
      view: this.$route.query.view,
      message: undefined,
      messageType: undefined,
      updateTime: false,
      editableView: undefined, // Not necessarily active view
      multiviewer: this.$constants.MOLOCH_MULTIVIEWER,
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
    molochClusters: function () {
      return this.$store.state.remoteclusters;
    },
    openItemsTooltip: function () {
      return 'Apply action to ' + this.$options.filters.commaString(this.openSessions.length) + ' opened sessions';
    },
    visibleItemsTooltip: function () {
      return 'Apply action to ' + this.$options.filters.commaString(Math.min(this.numVisibleSessions, this.numMatchingSessions)) + ' visible sessions';
    },
    matchingItemsTooltip: function () {
      return 'Apply action to ' + this.$options.filters.commaString(this.numMatchingSessions) + ' query matching sessions';
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
      this.$parent.$emit('recalc-collapse');
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
      this.$parent.$emit('recalc-collapse');
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
     * Overrides the server's diabling of aggregations on large time ranges
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
          sessionStorage['force-aggregations'] = false;
          this.$store.commit('setForcedAggregations', false);
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

      this.updateTime = !changed ? 'query' : true; // issue a query if the time
      // hasn't changed, otherwise just update the time in the time component
      this.$nextTick(() => { this.updateTime = false; });
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
.form-group {
  margin-bottom: 0;
}

/* viz options button position above viz in nav */
.viz-options-btn-container {
  top: 118px;
  right: 4px;
  position: fixed;
}
</style>
