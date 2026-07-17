<!--
Copyright Yahoo Inc.
SPDX-License-Identifier: Apache-2.0
-->
<template>
  <form class="position-relative pt-1">
    <!-- viz options button -->
    <div
      class="viz-options-btn-container"
      v-if="!actionForm && (basePath === 'sessions' || basePath === 'spiview' || basePath === 'arkime')">
      <!-- Split-button vs single-dropdown depending on whether the gear
           has a meaningful primary action. When viz is hidden or there
           are no disabled aggregations to fetch, the gear has nothing
           to do on its own, so we collapse to a single v-menu trigger. -->
      <v-btn-group
        v-if="!hideViz && disabledAggregations"
        density="compact"
        divided
        class="viz-options-btn">
        <v-btn
          color="primary"
          variant="flat"
          size="small"
          @click="overrideDisabledAggregations(1)">
          <v-icon icon="mdi-cog" />
          <span class="ms-1">
            {{ $t('search.fetchVizData') }}
          </span>
        </v-btn>
        <v-menu location="bottom end">
          <template #activator="{ props: activatorProps }">
            <v-btn
              v-bind="activatorProps"
              color="primary"
              variant="flat"
              size="small">
              <v-icon icon="mdi-menu-down" />
            </v-btn>
          </template>
          <v-list density="compact">
            <v-list-item
              id="fetchVizQuery"
              @click="overrideDisabledAggregations(1)">
              {{ $t('search.fetchVizQuery') }}
              <v-tooltip
                activator="#fetchVizQuery"
                location="left">
                {{ $t('search.fetchVizQueryTip') }}
              </v-tooltip>
            </v-list-item>
            <v-list-item
              id="fetchVizSession"
              @click="overrideDisabledAggregations(0)">
              {{ $t('search.fetchVizSession') }}
              <v-tooltip
                activator="#fetchVizSession"
                location="left">
                {{ $t('search.fetchVizSessionTip') }}
              </v-tooltip>
            </v-list-item>
            <v-list-item
              id="fetchVizBrowser"
              @click="overrideDisabledAggregations(-1)">
              {{ $t('search.fetchVizBrowser') }}
              <v-tooltip
                activator="#fetchVizBrowser"
                location="left">
                {{ $t('search.fetchVizBrowserTip') }}
              </v-tooltip>
            </v-list-item>
            <v-list-item
              v-if="forcedAggregations"
              @click="overrideDisabledAggregations(undefined)">
              {{ $t('search.disableVis') }}
            </v-list-item>
            <v-divider />
            <v-list-item @click="toggleStickyViz">
              {{ !stickyViz ? 'Pin' : 'Unpin' }} {{ basePath && basePath === 'sessions' ? 'graph, map, and column headers' : 'graph and map' }}
            </v-list-item>
            <v-list-item
              id="hideViz"
              @click="toggleHideViz"
              v-if="basePath !== 'spigraph'">
              {{ $t(!hideViz ? 'search.hideGraphMap' : 'search.showGraphMap') }}
              <v-tooltip
                activator="#hideViz"
                location="left">
                {{ $t(!hideViz ? 'search.speedUpTip' : 'search.showGraphTip') }}
              </v-tooltip>
            </v-list-item>
          </v-list>
        </v-menu>
      </v-btn-group>
      <v-menu
        v-else
        location="bottom end">
        <template #activator="{ props: activatorProps }">
          <v-btn
            v-bind="activatorProps"
            color="primary"
            variant="flat"
            size="small"
            class="viz-options-btn">
            <v-icon icon="mdi-cog" />
            <v-icon
              icon="mdi-menu-down"
              class="ms-1" />
          </v-btn>
        </template>
        <v-list density="compact">
          <v-list-item
            v-if="forcedAggregations"
            @click="overrideDisabledAggregations(undefined)">
            {{ $t('search.disableVis') }}
          </v-list-item>
          <v-list-item @click="toggleStickyViz">
            {{ !stickyViz ? 'Pin' : 'Unpin' }} {{ basePath && basePath === 'sessions' ? 'graph, map, and column headers' : 'graph and map' }}
          </v-list-item>
          <v-list-item
            id="hideViz"
            @click="toggleHideViz"
            v-if="basePath !== 'spigraph'">
            {{ $t(!hideViz ? 'search.hideGraphMap' : 'search.showGraphMap') }}
            <v-tooltip
              activator="#hideViz"
              location="left">
              {{ $t(!hideViz ? 'search.speedUpTip' : 'search.showGraphTip') }}
            </v-tooltip>
          </v-list-item>
        </v-list>
      </v-menu>
    </div> <!-- /viz options button -->

    <div class="pe-1 ps-1 pt-1 pb-1">
      <!-- search row: expression input + submit + actions all on one
           flex line. Expression input grows to fill the remaining
           space (via .arkime-input-group--fluid in ExpressionTypeahead). -->
      <div class="d-flex align-start gap-1 mb-1 search-row">
        <!-- search box typeahead -->
        <expression-typeahead
          class="flex-grow-1"
          @mod-view="modView"
          @apply-expression="applyParams"
          @change-expression="changeExpression" /> <!-- /search box typeahead -->

        <!-- time inputs -->
        <arkime-time
          :timezone="user.settings.timezone"
          @time-change="timeChange"
          :hide-interval="hideInterval"
          :update-time="updateTime" /> <!-- /time inputs -->

        <!-- search button -->
        <v-btn
          variant="flat"
          size="small"
          density="comfortable"
          class="search-btn"
          :style="tertiaryBtnStyle"
          tabindex="2"
          @click="applyParams">
          <span v-if="!shiftKeyHold">
            {{ $t('common.search') }}
          </span>
          <span
            v-else
            class="enter-icon">
            <v-icon
              icon="mdi-arrow-left"
              size="small" />
            <div class="enter-arm" />
          </span>
        </v-btn> <!-- /search button -->

        <Clusters
          v-if="multiviewer"
          :select-one="$route.name === 'Hunt'" /> <!-- cluster dropdown menu -->

        <!-- views dropdown menu -->
        <v-menu location="bottom end">
          <template #activator="{ props: activatorProps }">
            <v-btn
              v-bind="activatorProps"
              variant="flat"
              size="small"
              density="comfortable"
              :style="secondaryBtnStyle">
              <template v-if="view && views && getView(view)">
                <span id="viewMenuDropdown">
                  <v-icon
                    icon="mdi-eye"
                    class="me-1" />
                  <span v-if="view">{{ getView(view).name || view }}</span>
                  <span class="sr-only">{{ $t('common.views') }}</span>
                </span>
                <v-tooltip activator="#viewMenuDropdown">
                  {{ getView(view).expression || '' }}
                </v-tooltip>
              </template>
              <template v-else>
                <v-icon icon="mdi-eye" />
                <span class="sr-only">{{ $t('common.views') }}</span>
              </template>
            </v-btn>
          </template>
          <v-list density="compact">
            <v-list-item
              @click="modView()">
              <v-icon icon="mdi-plus-circle" />&nbsp;
              {{ $t('search.newView') }}
              <v-tooltip
                activator="parent"
                location="left">
                Create a new view
              </v-tooltip>
            </v-list-item>
            <v-divider />
            <v-list-item
              @click="setView(undefined)"
              :active="!view">
              {{ $t('common.none') }}
            </v-list-item>
            <v-list-item
              v-for="(value, index) in views"
              :id="`view${value.id}`"
              :key="value.id"
              :active="view === value.id"
              @click.self="setView(value.id)">
              <div
                class="d-flex align-center w-100"
                @click.self="setView(value.id)">
                <v-icon
                  icon="mdi-share-all"
                  class="me-1"
                  v-if="value.shared" />
                <span
                  class="flex-grow-1"
                  @click="setView(value.id)">
                  {{ value.name }}
                </span>
                <!-- view action buttons -->
                <v-btn
                  v-if="value.sessionsColConfig && $route.name === 'Sessions'"
                  :id="`applyColumns${value.id}`"
                  :aria-label="$t('search.applyColumns')"
                  variant="flat"
                  size="small"
                  density="comfortable"
                  icon
                  class="ms-1"
                  :style="tertiaryBtnStyle"
                  @click.stop.prevent="applyColumns(value)">
                  <v-icon icon="mdi-view-column" />
                  <v-tooltip :activator="`[id='applyColumns${value.id}']`">
                    {{ $t('search.applyColumns') }}
                  </v-tooltip>
                </v-btn>
                <v-btn
                  :id="`applyView${value.id}`"
                  :aria-label="$t('search.applyView')"
                  variant="flat"
                  size="small"
                  density="comfortable"
                  icon
                  class="ms-1"
                  :style="secondaryBtnStyle"
                  @click.stop.prevent="applyView(value)">
                  <v-icon
                    icon="mdi-share"
                    class="mdi-flip-h" />
                  <v-tooltip :activator="`[id='applyView${value.id}']`">
                    {{ $t('search.applyView') }}
                  </v-tooltip>
                </v-btn>
                <template v-if="canEditView(value)">
                  <v-btn
                    :id="`editView${value.id}`"
                    :aria-label="$t('search.editView')"
                    color="warning"
                    variant="flat"
                    size="small"
                    density="comfortable"
                    icon
                    class="ms-1"
                    @click.stop.prevent="modView(views[index])">
                    <v-icon icon="mdi-pencil-box" />
                    <v-tooltip :activator="`[id='editView${value.id}']`">
                      {{ $t('search.editView') }}
                    </v-tooltip>
                  </v-btn>
                  <v-btn
                    :id="`deleteView${value.id}`"
                    :aria-label="$t('search.deleteView')"
                    color="error"
                    variant="flat"
                    size="small"
                    density="comfortable"
                    icon
                    class="ms-1"
                    @click.stop.prevent="deleteView(value.id, index)">
                    <v-icon icon="mdi-trash-can-outline" />
                    <v-tooltip :activator="`[id='deleteView${value.id}']`">
                      {{ $t('search.deleteView') }}
                    </v-tooltip>
                  </v-btn>
                </template>
                <!-- /view action buttons -->
              </div>
              <v-tooltip
                :activator="`[id='view${value.id}']`"
                location="left">
                {{ value.expression }}
              </v-tooltip>
            </v-list-item>
          </v-list>
        </v-menu> <!-- /views dropdown menu -->

        <!-- actions dropdown menu -->
        <v-menu
          v-if="!hideActions && $route.name === 'Sessions'"
          location="bottom end">
          <template #activator="{ props: activatorProps }">
            <v-btn
              v-bind="activatorProps"
              variant="flat"
              size="small"
              density="comfortable"
              class="action-menu-dropdown"
              :style="primaryBtnStyle"
              title="Actions menu">
              <v-icon icon="mdi-menu-down" />
            </v-btn>
          </template>
          <v-list density="compact">
            <v-list-item
              @click="exportPCAP"
              v-has-permission="'!disablePcapDownload'">
              <v-icon icon="mdi-file-outline" />&nbsp;
              {{ $t('sessions.exports.exportPCAP') }}
              <v-tooltip
                activator="parent"
                location="left">
                {{ $t('sessions.exports.exportPCAP') }}
              </v-tooltip>
            </v-list-item>
            <v-list-item
              @click="exportCSV">
              <v-icon icon="mdi-file-excel-outline" />&nbsp;
              {{ $t('sessions.exports.exportCSV') }}
              <v-tooltip
                activator="parent"
                location="left">
                {{ $t('sessions.exports.exportCSV') }}
              </v-tooltip>
            </v-list-item>
            <v-list-item
              @click="addTags">
              <v-icon icon="mdi-tag-multiple" />&nbsp;
              {{ $t('sessions.tag.addTags') }}
              <v-tooltip
                activator="parent"
                location="left">
                {{ $t('sessions.tag.addTags') }}
              </v-tooltip>
            </v-list-item>
            <v-list-item
              @click="removeTags"
              v-has-permission="'removeEnabled'">
              <v-icon icon="mdi-eraser" />&nbsp;
              {{ $t('sessions.tag.removeTags') }}
              <v-tooltip
                activator="parent"
                location="left">
                {{ $t('sessions.tag.removeTags') }}
              </v-tooltip>
            </v-list-item>
            <v-list-item
              @click="removeData"
              v-has-permission="'removeEnabled'">
              <v-icon icon="mdi-trash-can-outline" />&nbsp;
              {{ $t('search.removeData') }}
              <v-tooltip
                activator="parent"
                location="left">
                {{ $t('search.removeData') }}
              </v-tooltip>
            </v-list-item>
            <v-list-item
              v-for="(clusterInfo, key) in arkimeClusters"
              :key="key"
              @click="sendSession(key)">
              <v-icon icon="mdi-send-outline" />&nbsp;
              {{ $t('search.sendSession', { name: clusterInfo.name }) }}
              <v-tooltip
                activator="parent"
                location="left">
                {{ $t('search.sendSession', { name: clusterInfo.name }) }}
              </v-tooltip>
            </v-list-item>
            <v-list-item
              @click="viewIntersection">
              <span class="arkime-venn me-2">
                <v-icon icon="mdi-circle-outline" />
                <v-icon icon="mdi-circle-outline" />
              </span>
              {{ $t('sessions.intersection.title') }}
              <v-tooltip
                activator="parent"
                location="left">
                {{ $t('sessions.intersection.title') }}
              </v-tooltip>
            </v-list-item>
            <v-list-item
              v-if="!multiviewer"
              @click="periodicQuery">
              <v-icon icon="mdi-magnify" />&nbsp;
              {{ $t('search.createPeriodicQuery') }}
              <v-tooltip
                activator="parent"
                location="left">
                {{ $t('search.createPeriodicQuery') }}
              </v-tooltip>
            </v-list-item>
          </v-list>
        </v-menu> <!-- /actions dropdown menu -->
      </div> <!-- /search row -->

      <!-- form message -->
      <div class="small mt-1">
        <arkime-toast
          :message="message"
          :type="messageType"
          :done="messageDone" />
      </div> <!-- /form message -->

      <div v-if="actionForm">
        <hr class="action-form-separator">
        <v-row class="d-flex gap-1">
          <v-col
            cols="auto"
            v-if="showApplyButtons">
            <v-tooltip activator="#openSessions">
              {{ openItemsTooltip }}
            </v-tooltip>
            <v-tooltip activator="#visibleSessions">
              {{ visibleItemsTooltip }}
            </v-tooltip>
            <v-tooltip activator="#matchingSessions">
              {{ matchingItemsTooltip }}
            </v-tooltip>
            <v-btn-toggle
              density="compact"
              divided
              variant="outlined"
              color="secondary"
              class="d-inline-flex mb-0"
              :model-value="actionFormItemRadio"
              @update:model-value="newVal => actionFormItemRadio = newVal"
              mandatory>
              <v-btn
                size="small"
                value="open"
                id="openSessions">
                {{ $t('search.open') }}
              </v-btn>
              <v-btn
                size="small"
                value="visible"
                id="visibleSessions">
                {{ $t('search.visible') }}
              </v-btn>
              <v-btn
                size="small"
                value="matching"
                id="matchingSessions">
                {{ $t('search.matching') }}
              </v-btn>
            </v-btn-toggle>
          </v-col>
          <!-- actions menu forms -->
          <v-col
            cols="auto"
            class="flex-grow-1">
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
          </v-col> <!-- /actions menu forms -->
        </v-row>
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
import { resolveMessage } from '@common/resolveI18nMessage';

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
      basePath: undefined,
      // Arkime theme-color v-btn styles. Vuetify :color can't take CSS vars.
      primaryBtnStyle: {
        backgroundColor: 'rgb(var(--v-theme-primary))',
        color: 'rgb(var(--v-theme-button-fg))'
      },
      secondaryBtnStyle: {
        backgroundColor: 'rgb(var(--v-theme-secondary))',
        color: 'rgb(var(--v-theme-button-fg))'
      },
      tertiaryBtnStyle: {
        backgroundColor: 'rgb(var(--v-theme-tertiary))',
        color: 'rgb(var(--v-theme-button-fg))'
      }
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
        this.msg = resolveMessage(response, this.$t);
        this.msgType = 'success';
      }).catch((error) => {
        // display error message to user
        this.msg = resolveMessage(error, this.$t);
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

<style scoped>
form {
  border: none;
  z-index: 5;
  background-color: rgb(var(--v-theme-secondary-lightest));
  -webkit-box-shadow: 0 0 16px -2px black;
     -moz-box-shadow: 0 0 16px -2px black;
          box-shadow: 0 0 16px -2px black;
}
.action-form-separator {
  margin: 5px 0;
  border-top: 1px solid rgb(var(--v-theme-neutral-light));
}
/* make sure action menu dropdown is above all the things
 * but specifically above the sticky sessions button */
.action-menu-dropdown { z-index: 1030; }

/* In the search row, match all v-btns to the 32px height of the
   .arkime-input-group expression input next to them. v-btn size=small
   defaults to 28px, leaving them visibly shorter than the input. */
.search-row :deep(.v-btn) {
  height: 32px;
}

/* drop v-btn's built-in min-width: 42px on the SEARCH button so it
   sizes to its content (the "Search" label or the enter-icon glyph)
   instead of staying a fixed minimum. */
.search-row :deep(.search-btn) {
  min-width: 0;
}

/* viz options gear: docked under the right end of the search form (its
   position-relative parent) so it follows the toolbar chrome in flow */
.viz-options-btn-container {
  position: absolute;
  top: 108%; /* a nudge past the form bottom centers it in the paging bar */
  right: 4px;
  z-index: 5;
}
</style>
