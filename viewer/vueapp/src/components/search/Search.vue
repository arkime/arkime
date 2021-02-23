<template>

  <form class="position-relative">
    <div class="pr-1 pl-1 pt-1 pb-1">

      <!-- actions dropdown menu -->
      <b-dropdown v-if="!hideActions && $route.name === 'Sessions'"
        right
        size="sm"
        class="pull-right ml-1 action-menu-dropdown"
        boundary="body"
        variant="theme-primary">
        <b-dropdown-item @click="exportPCAP"
          v-has-permission="'!disablePcapDownload'">
          <span class="fa fa-fw fa-file-o"></span>&nbsp;
          Export PCAP
        </b-dropdown-item>
        <b-dropdown-item @click="exportCSV">
          <span class="fa fa-fw fa-file-excel-o"></span>&nbsp;
          Export CSV
        </b-dropdown-item>
        <b-dropdown-item @click="addTags">
          <span class="fa fa-fw fa-tags"></span>&nbsp;
          Add Tags
        </b-dropdown-item>
        <b-dropdown-item @click="removeTags"
          v-has-permission="'removeEnabled'">
          <span class="fa fa-fw fa-eraser"></span>&nbsp;
          Remove Tags
        </b-dropdown-item>
        <b-dropdown-item @click="removeData"
          v-has-permission="'removeEnabled'">
          <span class="fa fa-fw fa-trash-o"></span>&nbsp;
          Remove Data
        </b-dropdown-item>
        <b-dropdown-item v-for="(cluster, key) in molochClusters"
          :key="key"
          @click="sendSession(key)">
          <span class="fa fa-fw fa-paper-plane-o"></span>&nbsp;
          Send Session to {{ cluster.name }}
        </b-dropdown-item>
        <b-dropdown-item @click="viewIntersection">
          <span class="fa fa-fw fa-venn">
            <span class="fa fa-circle-o">
            </span>
            <span class="fa fa-circle-o">
            </span>
          </span>&nbsp;
          Export Intersection
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
          <div v-if="view && views && views[view]"
            v-b-tooltip.hover.left
            :title="views[view].expression">
            <span class="fa fa-eye"></span>
            <span v-if="view">{{ view }}</span>
            <span class="sr-only">Views</span>
          </div>
          <div v-else>
            <span class="fa fa-eye"></span>
            <span class="sr-only">Views</span>
          </div>
        </template>
        <b-dropdown-item @click="modView()">
          <span class="fa fa-plus-circle"></span>&nbsp;
          New View
        </b-dropdown-item>
        <b-dropdown-divider></b-dropdown-divider>
        <b-dropdown-item @click="setView(undefined)"
          :class="{'active':!view}">
          None
        </b-dropdown-item>
        <b-dropdown-item v-for="(value, key) in views"
          :key="key"
          :class="{'active':view === key}"
          @click.self="setView(key)"
          v-b-tooltip.hover.left
          :title="value.expression">
          <span v-if="value.shared"
            class="fa fa-share-square">
          </span>
          <!-- view action buttons -->
          <button class="btn btn-xs btn-danger pull-right ml-1"
            type="button"
            v-b-tooltip.hover.top
            title="Delete this view."
            @click.stop.prevent="deleteView(value, key)">
            <span class="fa fa-trash-o">
            </span>
          </button>
          <button class="btn btn-xs btn-warning pull-right ml-1"
            type="button"
            v-b-tooltip.hover.top
            title="Edit this view."
            @click.stop.prevent="modView(views[key])">
            <span class="fa fa-edit">
            </span>
          </button>
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
          {{ key }}&nbsp;
        </b-dropdown-item>
      </b-dropdown> <!-- /views dropdown menu -->

      <!-- ES cluster dropdown menu -->
      <b-dropdown v-if="multiviewer"
        right
        size="sm"
        class="multies-menu-dropdown pull-right ml-1"
        no-caret
        toggle-class="rounded"
        variant="theme-secondary"
        @show="esVisMenuOpen = true"
        @hide="esVisMenuOpen = false">
        <template slot="button-content">
          <div v-b-tooltip.hover.left :title="esMenuHoverText">
            <span class="fa fa-database"> </span>
            <span> {{ selectedCluster.length }} </span>
          </div>
        </template>
        <b-dropdown-header>
          <input type="text"
            v-model="esQuery"
            class="form-control form-control-sm dropdown-typeahead"
            placeholder="Search for Clusters..."
          />
        </b-dropdown-header>
        <b-dropdown-divider>
        </b-dropdown-divider>
         <b-dropdown-item @click.native.capture.stop.prevent="selectAllCluster">
          <span class="fa fa-list"></span>&nbsp;
          Select All
        </b-dropdown-item>
        <b-dropdown-item @click.native.capture.stop.prevent="clearAllCluster">
          <span class="fa fa-eraser"></span>&nbsp;
          Clear All
        </b-dropdown-item>
        <b-dropdown-divider>
        </b-dropdown-divider>
        <template v-if="esVisMenuOpen">
          <template v-for="(clusters, group) in filteredClusteres">
            <b-dropdown-header
              :key="group"
              class="group-header">
              {{ group + ' (' + clusters.length + ')' }}
            </b-dropdown-header>
            <template v-for="cluster in clusters">
              <b-dropdown-item
                :id="group + cluster + 'item'"
                :key="group + cluster + 'item'"
                :class="{'active':isClusterVis(cluster)}"
                @click.native.capture.stop.prevent="toggleClusterSelection(cluster)">
                {{ cluster }}
              </b-dropdown-item>
            </template>
          </template>
        </template>
      </b-dropdown> <!-- /Es cluster dropdown menu -->

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
      <moloch-time :timezone="timezone"
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
                <b-radio value="open"
                  v-b-tooltip.hover
                  :title="'Apply action to ' + openSessions.length + ' opened sessions'"
                  class="btn-radio">
                  Open Items
                </b-radio>
                <b-radio value="visible"
                  v-b-tooltip.hover
                  :title="'Apply action to ' + Math.min(numVisibleSessions, numMatchingSessions) + ' visible sessions'"
                  class="btn-radio">
                  Visible Items
                </b-radio>
                <b-radio value="matching"
                  v-b-tooltip.hover
                  :title="'Apply action to ' + numMatchingSessions + ' query matching sessions'"
                  class="btn-radio">
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
import UserService from '../users/UserService';
import ConfigService from '../utils/ConfigService';
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
    MolochIntersection
  },
  props: [
    'openSessions',
    'numVisibleSessions',
    'numMatchingSessions',
    'start',
    'timezone',
    'fields',
    'hideActions',
    'hideInterval'
  ],
  data: function () {
    return {
      molochClusters: {},
      actionFormItemRadio: 'visible',
      actionFormItemRadioOptions: [
        { text: 'Open Item', value: 'open' },
        { text: 'Visible Items', value: 'visible' },
        { text: 'Matching Items', value: 'matching' }
      ],
      actionForm: undefined,
      showApplyButtons: false,
      cluster: {},
      esVisMenuOpen: false,
      esQuery: '', // query for ES to toggle visibility
      view: this.$route.query.view,
      message: undefined,
      messageType: undefined,
      updateTime: false,
      editableView: undefined, // Not necessarily active view
      multiviewer: this.$constants.MOLOCH_MULTIVIEWER
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
    esMenuHoverText: function () {
      if (this.selectedCluster.length === 0) {
        return 'No Selection';
      } else if (this.selectedCluster.length === 1) {
        return this.selectedCluster[0];
      } else {
        return this.selectedCluster.length + ' out of ' + this.availableCluster.active.length + ' selected';
      }
    },
    availableCluster: {
      get: function () {
        return this.$store.state.esCluster.availableCluster;
      },
      set: function (newValue) {
        this.$store.commit('setAvailableCluster', newValue);
      }
    },
    selectedCluster: {
      get: function () {
        return this.$store.state.esCluster.selectedCluster || [];
      },
      set: function (newValue) {
        this.$store.commit('setSelectedCluster', newValue);
      }
    },
    filteredClusteres: function () {
      const filteredGroupedClusters = {};
      for (const group in this.availableCluster) {
        filteredGroupedClusters[group] = this.$options.filters.searchCluster(
          this.esQuery,
          this.availableCluster[group]
        );
      }
      return filteredGroupedClusters;
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
    this.getViews();
    this.getMolochClusters();
    this.getClusterInformation();
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
      }
      this.timeUpdate();
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
      this.editableView = view;
      this.actionForm = 'modify:view';
      this.showApplyButtons = false;
    },
    viewIntersection: function () {
      this.actionForm = 'view:intersection';
      this.showApplyButtons = false;
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
    deleteView: function (view, name) {
      // check if deleting current view
      if (this.view === name) {
        this.setView(undefined);
      }

      UserService.deleteView(view, this.user.userId)
        .then((response) => {
          // remove the view from the view list
          this.$store.commit('deleteViews', name);
          this.getViews();
          // display success message to user
          this.msg = response.text;
          this.msgType = 'success';
        })
        .catch((error) => {
          console.log(error);
          // display error message to user
          this.msg = error.text;
          this.msgType = 'danger';
        });
    },
    setView: function (view) {
      this.view = view;

      // update the url and session storage (to persist user's choice)
      // triggers the '$route.query.view' watcher that issues changeSearch event
      sessionStorage['moloch-view'] = view;
      this.$router.push({
        query: {
          ...this.$route.query,
          view: view
        }
      });

      this.$emit('setView');
    },
    applyView: function (view) {
      this.expression = view.expression;
      this.$store.commit('setFocusSearch', true);
      setTimeout(() => { // unfocus input for further re-focusing
        this.$store.commit('setFocusSearch', false);
      }, 1000);
    },
    applyColumns: function (view) {
      this.$emit('setColumns', view.sessionsColConfig);
    },
    /* helper functions ------------------------------------------ */
    getViews: function () {
      UserService.getViews()
        .then((response) => {
          this.views = response;
        });
    },
    getMolochClusters: function () {
      ConfigService.getMolochClusters()
        .then((response) => {
          this.molochClusters = response;
        });
    },
    /* MultiES functions ------------------------------------------ */
    getClusterInformation: function () {
      if (this.availableCluster.active.length === 0 && this.availableCluster.inactive.length === 0) {
        ConfigService.getClusters()
          .then((response) => {
            this.availableCluster = response;
            const clusters = this.$route.query.cluster ? this.$route.query.cluster.split(',') : [];
            if (clusters.length === 0) {
              this.selectedCluster = response.active;
            } else {
              this.selectedCluster = [];
              for (let i = 0; i < clusters.length; i++) {
                if (response.active.includes(clusters[i])) {
                  this.selectedCluster.push(clusters[i]);
                }
              }
            }
          });
      }
    },
    isClusterVis: function (cluster) {
      if (this.availableCluster.active.includes(cluster)) { // found in active cluster list
        return this.selectedCluster.includes(cluster); // returns True if found in selected cluster list
      } else { // inactive cluster
        return false;
      }
    },
    updateRouteQueryForClusters: function (clusters) {
      const cluster = clusters.length > 0 ? clusters.join(',') : 'none';
      if (!this.$route.query.cluster || this.$route.query.cluster !== cluster) {
        this.$router.push({
          query: {
            ...this.$route.query,
            cluster: cluster
          }
        });
      }
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

        this.$router.push({
          query: {
            ...this.$route.query,
            date: undefined,
            stopTime: this.$store.state.time.stopTime,
            startTime: this.$store.state.time.startTime
          }
        });
      }

      this.updateTime = true;
      setTimeout(() => {
        this.updateTime = false;
      }, 1000);
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
    },
    selectAllCluster: function () {
      this.selectedCluster = this.availableCluster.active;
      this.updateRouteQueryForClusters(this.selectedCluster);
    },
    clearAllCluster: function () {
      this.selectedCluster = [];
      this.updateRouteQueryForClusters(this.selectedCluster);
    },
    toggleClusterSelection: function (cluster) {
      if (this.selectedCluster.includes(cluster)) { // already selected; remove from selection
        this.selectedCluster = this.selectedCluster.filter((item) => {
          return item !== cluster;
        });
      } else if (!this.availableCluster.inactive.includes(cluster)) { // not in inactive cluster
        this.selectedCluster.push(cluster); // add to selected list
      }
      this.updateRouteQueryForClusters(this.selectedCluster);
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
</style>
