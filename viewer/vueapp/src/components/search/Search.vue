<template>

  <form>
    <div class="mr-1 ml-1 mt-1 mb-1">

      <!-- actions dropdown menu -->
      <b-dropdown right
        size="sm"
        class="pull-right ml-1 action-menu-dropdown"
        variant="theme-primary">
        <b-dropdown-item @click="exportPCAP()">
          <span class="fa fa-fw fa-file-o"></span>&nbsp;
          Export PCAP
        </b-dropdown-item>
        <b-dropdown-item @click="exportCSV()">
          <span class="fa fa-fw fa-file-excel-o"></span>&nbsp;
          Export CSV
        </b-dropdown-item>
        <b-dropdown-item @click="addTags()">
          <span class="fa fa-fw fa-tags"></span>&nbsp;
          Add Tags
        </b-dropdown-item>
        <b-dropdown-item @click="removeTags()"
          v-has-permission="'removeEnabled'">
          <span class="fa fa-fw fa-trash-o"></span>&nbsp;
          Remove Tags
        </b-dropdown-item>
        <b-dropdown-item @click="scrubPCAP()"
          v-has-permission="'removeEnabled'">
          <span class="fa fa-fw fa-trash-o"></span>&nbsp;
          Scrub PCAP Storage
        </b-dropdown-item>
        <b-dropdown-item @click="deleteSession()"
          v-has-permission="'removeEnabled'">
          <span class="fa fa-fw fa-trash-o"></span>&nbsp;
          Delete SPI & PCAP
        </b-dropdown-item>
        <b-dropdown-item v-for="(cluster, key) in molochClusters"
          :key="key"
          @click="sendSession(key)">
          <span class="fa fa-fw fa-paper-plane-o"></span>&nbsp;
          Send Session to {{ cluster.name }}
        </b-dropdown-item>
      </b-dropdown> <!-- /actions dropdown menu -->

      <!-- views dropdown menu -->
      <b-dropdown right
        size="sm"
        class="pull-right ml-1"
        no-caret
        toggle-class="rounded"
        variant="theme-secondary">
        <template slot="button-content">
          <span class="fa fa-eye"></span>
          <span v-if="view">{{ view }}</span>
          <span class="sr-only">Views</span>
        </template>
        <b-dropdown-item @click="createView()">
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
          @click.self="setView(key)">
          <button class="btn btn-xs btn-default pull-right"
            type="button"
            @click="deleteView(key)">
            <span class="fa fa-trash-o"></span>
          </button>
          {{ key }}
        </b-dropdown-item>
      </b-dropdown> <!-- /views dropdown menu -->

      <!-- search button -->
      <a class="btn btn-sm btn-theme-tertiary pull-right ml-1"
        @click="applyParams()"
        tabindex="2">
        Search
      </a> <!-- /search button -->

      <!-- search box typeahead -->
      <expression-typeahead
        @applyExpression="applyParams"
        @changeExpression="changeExpression">
      </expression-typeahead> <!-- /search box typeahead -->

      <!-- time inputs -->
      <div class="form-inline">
        <moloch-time :timezone="timezone"
          @timeChange="timeChange"
          :updateTime="updateTime">
        </moloch-time>
      </div> <!-- /time inputs -->

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
            <moloch-create-view v-if="actionForm === 'create:view'"
              :done="actionFormDone"
              @newView="newView">
            </moloch-create-view>
            <moloch-tag-sessions v-else-if="actionForm === 'add:tags' || actionForm === 'remove:tags'"
              :add="actionForm === 'add:tags'"
              :start="start"
              :done="actionFormDone"
              :sessions="openSessions"
              :num-visible="numVisibleSessions"
              :num-matching="numMatchingSessions"
              :apply-to="actionFormItemRadio">
            </moloch-tag-sessions>
            <moloch-delete-sessions v-else-if="actionForm === 'delete:session'"
              :start="start"
              :done="actionFormDone"
              :sessions="openSessions"
              :num-visible="numVisibleSessions"
              :num-matching="numMatchingSessions"
              :apply-to="actionFormItemRadio">
            </moloch-delete-sessions>
            <moloch-scrub-pcap v-else-if="actionForm === 'scrub:pcap'"
              :start="start"
              :done="actionFormDone"
              :sessions="openSessions"
              :num-visible="numVisibleSessions"
              :num-matching="numMatchingSessions"
              :apply-to="actionFormItemRadio">
            </moloch-scrub-pcap>
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
              :done="actionFormDone"
              :sessions="openSessions"
              :num-visible="numVisibleSessions"
              :num-matching="numMatchingSessions"
              :apply-to="actionFormItemRadio">
            </moloch-export-csv>
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
import MolochCreateView from '../sessions/CreateView';
import MolochTagSessions from '../sessions/Tags';
import MolochDeleteSessions from '../sessions/Delete';
import MolochScrubPcap from '../sessions/Scrub';
import MolochSendSessions from '../sessions/Send';
import MolochExportPcap from '../sessions/ExportPcap';
import MolochExportCsv from '../sessions/ExportCsv';

export default {
  name: 'MolochSearch',
  components: {
    ExpressionTypeahead,
    MolochTime,
    MolochToast,
    MolochCreateView,
    MolochTagSessions,
    MolochDeleteSessions,
    MolochScrubPcap,
    MolochSendSessions,
    MolochExportPcap,
    MolochExportCsv
  },
  props: [
    'openSessions',
    'numVisibleSessions',
    'numMatchingSessions',
    'start',
    'timezone',
    'fields'
  ],
  data: function () {
    return {
      views: {},
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
      view: this.$route.query.view,
      message: undefined,
      messageType: undefined,
      updateTime: false
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
    }
  },
  created: function () {
    this.getMolochClusters();
    this.getViews();
  },
  methods: {
    /* exposed page functions ------------------------------------ */
    messageDone: function () {
      this.message = undefined;
      this.messageType = undefined;
    },
    applyExpression: function () {
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
      this.applyExpression();
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
    scrubPCAP: function () {
      this.actionForm = 'scrub:pcap';
      this.showApplyButtons = true;
    },
    deleteSession: function () {
      this.actionForm = 'delete:session';
      this.showApplyButtons = true;
    },
    sendSession: function (cluster) {
      this.cluster = cluster;
      this.actionForm = 'send:session';
      this.showApplyButtons = true;
    },
    createView: function () {
      this.actionForm = 'create:view';
      this.showApplyButtons = false;
    },
    actionFormDone: function (message, success, reloadData) {
      this.actionForm = undefined;
      if (message) {
        this.message = message;
        this.messageType = success ? 'success' : 'warning';
      }
    },
    /* updates the views list with the included new view */
    newView: function (views) {
      if (views) { this.views = views; }
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
    },
    deleteView: function (view) {
      UserService.deleteView(view)
        .then((response) => {
          // display success message
          if (response.text) {
            this.message = response.text;
            this.messageType = response.success ? 'success' : 'warning';
          }

          if (response.success) {
            // remove the deleted view if it was selected
            if (this.view === view) {
              this.setView(undefined);
            }

            this.views[view] = null;
            delete this.views[view];
          }
        })
        .catch((error) => {
          this.message = error;
          this.messageType = 'danger';
        });
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
    /**
     * update the stop/start times in time component, which in turn
     * notifies this controller (using the 'changeTime' event), then
     * updates the time params and emits a 'changeSearch' event to parent
     */
    timeUpdate: function () {
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
    }
  }
};
</script>

<style scoped>
form {
  position: fixed;
  right: 0;
  left: 0;
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
