<template>

  <form>
    <div class="mr-1 ml-1 mt-1 mb-1">

      <!-- actions dropdown menu -->
      <b-dropdown right
        size="sm"
        class="pull-right ml-1"
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
        text="Views "
        variant="theme-quaternary">
        <b-dropdown-item @click="createView()">
          <span class="fa fa-plus-circle"></span>&nbsp;
          New View
        </b-dropdown-item>
        <b-dropdown-divider></b-dropdown-divider>
        <b-dropdown-item @click="setView('')"
          :class="{'active':!view}">
          None
        </b-dropdown-item>
        <b-dropdown-item v-for="(value, key) in views"
          :key="key"
          :class="{'active':view === key}"
          @click="setView(view)">
          <button class="btn btn-xs btn-default pull-right"
            @click="deleteView(key)">
            <span class="fa fa-trash-o"></span>
          </button>
          <span @click="setView(key)">
            {{ key }}
          </span>
        </b-dropdown-item>
      </b-dropdown> <!-- /views dropdown menu -->

      <!-- search button -->
      <a class="btn btn-sm btn-theme-tertiary pull-right ml-1"
        @click="applyParams()"
        tabindex="2">
        Search
      </a> <!-- /search button -->

      <!-- TODO search box typeahead -->
      <!-- TODO enter-click="applyParams()" -->
      <expression-typeahead>
      </expression-typeahead> <!-- /search box typeahead -->

      <!-- TODO time inputs -->
      <div class="form-inline">
        <moloch-time :timezone="timezone"
          @timeChange="timeChange">
        </moloch-time>
      </div> <!-- /time inputs -->

      <!-- TODO form message -->
      <div class="small">
      </div> <!-- /form message -->

      <div v-if="actionForm"
        class="row">
        <hr class="action-form-separator">
        <div v-if="showApplyButtons"
          class="col-md-3">
          <!-- TODO fix titles -->
          <b-form-group>
            <b-form-radio-group
              size="sm"
              buttons
              v-model="actionsFormItemRadio">
              <b-radio value="open"
                v-b-tooltip.hover
                :title="'Apply action to ' + openSessions.length + ' opened sessions'">
                Open Items
              </b-radio>
              <b-radio value="visible"
                v-b-tooltip.hover
                :title="'Apply action to ' + Math.min(numVisibleSessions, numMatchingSessions) + ' visible sessions'">
                Visible Items
              </b-radio>
              <b-radio value="matching"
                v-b-tooltip.hover
                :title="'Apply action to ' + numMatchingSessions + ' query matching sessions'">
                Matching Items
              </b-radio>
            </b-form-radio-group>
          </b-form-group>
        </div>
        <div :class="{'col-md-9':showApplyButtons,'col-md-12':!showApplyButtons}">
          <!-- TODO actions menu forms -->
          FORMS GO HERE
        </div>
      </div> <!-- /actions menu forms -->

    </div>
  </form>

</template>

<script>
import UserService from '../UserService';
import ConfigService from '../utils/ConfigService';
import ExpressionTypeahead from './ExpressionTypeahead';
import MolochTime from './Time';

// TODO
// let manualChange = false;

export default {
  name: 'MolochSearch',
  components: { ExpressionTypeahead, MolochTime },
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
      actionsFormItemRadio: 'visible',
      actionsFormItemRadioOptions: [
        { text: 'Open Item', value: 'open' },
        { text: 'Visible Items', value: 'visible' },
        { text: 'Matching Items', value: 'matching' }
      ],
      actionForm: '',
      showApplyButtons: false,
      cluster: {},
      view: '',
      message: '',
      messageType: ''
    };
  },
  created: function () {
    this.getMolochClusters();
    this.getViews();
  },
  methods: {
    /* exposed page functions ------------------------------------ */
    messageDone () {
      // TODO
      this.message = '';
      this.messageType = '';
    },
    applyExpression () {
      // TODO
      // if (this.$rootScope.expression && this.$rootScope.expression !== '') {
      //   this.$location.search('expression', this.$rootScope.expression);
      // } else {
      //   this.$location.search('expression', null);
      // }
    },
    applyParams () {
      // TODO
      // manualChange = true;

      this.applyExpression();

      // update the stop/start times in time.component, which in turn
      // notifies this controller (usin the 'change:time:input' event), which
      // then updates the time params and calls this.change()
      // this.$scope.$broadcast('update:time');
    },
    exportPCAP: function () {
      // TODO
      console.log('export pcap');
      this.actionForm = 'export:pcap';
      this.showApplyButtons = true;
    },
    exportCSV: function () {
      // TODO
      console.log('export csv');
      this.actionForm = 'export:csv';
      this.showApplyButtons = true;
    },
    addTags: function () {
      // TODO
      console.log('add tags');
      this.actionForm = 'add:tags';
      this.showApplyButtons = true;
    },
    removeTags: function () {
      // TODO
      console.log('remove tags');
      this.actionForm = 'remove:tags';
      this.showApplyButtons = true;
    },
    scrubPCAP: function () {
      // TODO
      console.log('scrub pcap');
      this.actionForm = 'scrub:pcap';
      this.showApplyButtons = true;
    },
    deleteSession: function () {
      // TODO
      console.log('delete session');
      this.actionForm = 'delete:session';
      this.showApplyButtons = true;
    },
    sendSession: function (cluster) {
      // TODO
      console.log('send session to', cluster);
      this.cluster = cluster;
      this.actionForm = 'send:session';
      this.showApplyButtons = true;
    },
    createView: function () {
      // TODO
      console.log('create view');
      this.actionForm = 'create:view';
      this.showApplyButtons = false;
    },
    setView: function (view) {
      this.view = view;
      // TODO
      // update url and session storage (to persist user's choice)
      // if (!view) {
      //   delete sessionStorage['moloch-view'];
      //   this.$location.search('view', null);
      // } else {
      //   sessionStorage['moloch-view'] = view;
      //   this.$location.search('view', view);
      // }

      // this.$scope.$emit('change:search', {
      //   expression : this.$rootScope.expression,
      //   view       : this.view
      // });
      //
      // this.$rootScope.$broadcast('issue:search', {
      //   expression : this.$rootScope.expression,
      //   view       : this.view
      // });
    },
    deleteView: function (view) {
      // TODO
      UserService.deleteView(view)
        .then((response) => {
          // let args = {};
          //
          // if (response.text) {
          //   args.message = response.text;
          //   args.success = response.success;
          // }
          //
          // // notify parent to close form and display message
          // this.$scope.$emit('close:form:container', args);
          //
          // if (response.success) {
          //   if (this.view === view) {
          //     this.setView(undefined);
          //   }
          //
          //   this.views[view] = null;
          //   delete this.views[view];
          // }
        })
        .catch((err) => {
          // notify parent to close form and display message
          // this.$scope.$emit('close:form:container', {
          //   message: err, success: false
          // });
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
    /* event functions ------------------------------------------- */
    timeChange: function (args) {
      console.log('time change event caught', args);
      // TODO
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
  z-index : 4;
  background-color: var(--color-secondary-lightest);

  -webkit-box-shadow: var(--px-none) var(--px-none) var(--px-xlg) var(--px-sm) #333;
     -moz-box-shadow: var(--px-none) var(--px-none) var(--px-xlg) var(--px-sm) #333;
          box-shadow: var(--px-none) var(--px-none) var(--px-xlg) var(--px-sm) #333;
}

.action-form-separator {
  margin: 7px 15px;
  border-top: 1px solid var(--color-gray-light);
}

/* dropdown menus in search bar */
.dropdown-menu {
  min-width: 200px;
  max-height: 320px;
  overflow-y: auto;
}

.dropdown-menu > li > div {
  display: block;
  padding: 2px 8px;
  white-space: nowrap;
  margin-left: 0;
  margin-right: 0;
}

.dropdown-menu > li > div > div {
  padding-left: 0;
  padding-right: 0;
}

.dropdown-menu > li > div a {
  color: var(--color-black);
  text-decoration: none;
}

/* make sure action menu dropdown is above all the things
 * but specifically above the sticky sessions button */
.action-menu-dropdown { z-index: 1030; }
</style>
