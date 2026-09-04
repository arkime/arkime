<!--
Copyright Yahoo Inc.
SPDX-License-Identifier: Apache-2.0
-->
<!--
  Connections force-directed graph (the `connections` SPIGraph type).
  Teleports its controls + overlay buttons into anchors the host exposes.
-->
<template>
  <!-- sub-navbar controls -->
  <teleport
    defer
    to="#connections-controls-anchor">
    <!-- query size select -->
    <div class="arkime-input-group">
      <span
        id="querySize"
        class="arkime-input-label cursor-help">
        {{ $t('connections.querySize') }}
      </span>
      <v-tooltip
        activator="#querySize"
        :open-delay="300">
        {{ $t('connections.querySizeTip') }}
      </v-tooltip>
      <select
        class="arkime-input-control"
        :value="query.length"
        @change="changeLength(Number($event.target.value))">
        <option
          v-for="opt in [100, 500, 1000, 5000, 10000, 50000, 100000]"
          :key="opt"
          :value="opt">
          {{ opt }}
        </option>
      </select>
    </div> <!-- /query size select -->

    <!-- src select -->
    <div
      class="connections-field-row"
      v-if="fields && fields.length && srcFieldTypeahead && fieldHistoryConnectionsSrc">
      <span
        class="connections-legend-cell primary-legend cursor-help"
        id="sourceField">
        Src:
      </span>
      <v-tooltip
        activator="#sourceField"
        :open-delay="300">
        {{ $t('connections.sourceFieldTip') }}
      </v-tooltip>
      <arkime-field-typeahead
        :fields="fields"
        query-param="srcField"
        :initial-value="srcFieldTypeahead"
        @field-selected="changeSrcField"
        :history="fieldHistoryConnectionsSrc"
        page="ConnectionsSrc" />
    </div> <!-- /src select -->

    <!-- dst select -->
    <div
      class="connections-field-row"
      v-if="fields && dstFieldTypeahead && fieldHistoryConnectionsDst">
      <span
        class="connections-legend-cell secondary-legend cursor-help"
        id="dstField">
        Dst:
      </span>
      <v-tooltip
        activator="#dstField"
        :open-delay="300">
        {{ $t('connections.dstFieldTip') }}
      </v-tooltip>
      <arkime-field-typeahead
        :fields="fields"
        query-param="dstField"
        :initial-value="dstFieldTypeahead"
        @field-selected="changeDstField"
        :history="fieldHistoryConnectionsDst"
        page="ConnectionsDst" />
    </div> <!-- /dst select -->

    <!-- src & dst color -->
    <span
      class="connections-legend-cell connections-legend-standalone tertiary-legend cursor-help"
      id="srcDstColor">
      Src &amp; dst
    </span>
    <v-tooltip
      activator="#srcDstColor"
      :open-delay="300">
      {{ $t('connections.srcDstColorTip') }}
    </v-tooltip> <!-- /src & dst color -->

    <!-- min connections select -->
    <div class="arkime-input-group">
      <span
        id="minConn"
        class="arkime-input-label help-cursor">
        {{ $t('connections.minConn') }}
      </span>
      <v-tooltip
        activator="#minConn"
        :open-delay="300">
        {{ $t('connections.minConnTip') }}
      </v-tooltip>
      <select
        class="arkime-input-control"
        :value="query.minConn"
        @change="changeMinConn(Number($event.target.value))">
        <option
          v-for="opt in [1,2,3,4,5]"
          :key="opt"
          :value="opt">
          {{ opt }}
        </option>
      </select>
    </div> <!-- /min connections select -->

    <!-- weight select -->
    <div class="arkime-input-group">
      <span
        class="arkime-input-label help-cursor"
        id="weight">
        {{ $t('connections.weight') }}
      </span>
      <v-tooltip
        activator="#weight"
        :open-delay="300">
        {{ $t('connections.weightTip') }}
      </v-tooltip>
      <select
        class="arkime-input-control"
        :value="weight"
        @change="changeWeight($event.target.value)">
        <option
          value="sessions"
          v-i18n-value="'connections.weight-'" />
        <option
          value="network.packets"
          v-i18n-value="'connections.weight-'" />
        <option
          value="network.bytes"
          v-i18n-value="'connections.weight-'" />
        <option
          value="totDataBytes"
          v-i18n-value="'connections.weight-'" />
        <option
          value=""
          v-i18n-value="'connections.weight-'" />
      </select>
    </div> <!-- /weight select -->

    <div
      v-if="!loading"
      class="d-inline-flex">
      <!-- node + link field-visibility menus (same shape, kind='node'|'link') -->
      <template
        v-for="m in fieldVisMenus"
        :key="m.kind">
        <v-menu
          v-if="fields && groupedFields && fieldList(m.kind)"
          :close-on-content-click="false"
          location="bottom start">
          <template #activator="{ props: activatorProps }">
            <v-btn
              v-bind="activatorProps"
              variant="flat"
              size="large"
              color="primary"
              class="ms-1 field-vis-trigger"
              :id="`${m.kind}Fields`">
              <v-icon :icon="m.icon" />
              <v-tooltip
                activator="parent"
                :open-delay="300">
                {{ $t(m.tipKey) }}
              </v-tooltip>
            </v-btn>
          </template>
          <v-list
            density="compact"
            class="field-vis-list">
            <div class="px-2 py-1">
              <div class="arkime-input-group arkime-input-group--fluid">
                <span class="arkime-input-label arkime-input-label-fw">
                  <v-icon icon="mdi-magnify" />
                </span>
                <input
                  type="text"
                  :value="fieldQuery"
                  @input="debouncedSetFieldQuery($event.target.value)"
                  class="arkime-input-control"
                  :placeholder="$t('common.searchForFields')">
              </div>
            </div>
            <v-divider />
            <v-list-item @click.stop.prevent="resetFieldVisibility(m.kind)">
              {{ $t('connections.reset') }}
            </v-list-item>
            <v-divider />
            <template
              v-for="(group, key) in filteredFields"
              :key="key">
              <v-list-subheader
                v-if="group.length"
                class="field-vis-group-header">
                {{ key }}
              </v-list-subheader>
              <template
                v-for="(field, k) in group"
                :key="key + k + m.kind">
                <v-list-item
                  :data-tip-id="key + k + m.kind"
                  :active="isFieldVisible(field.dbField, fieldList(m.kind)) >= 0"
                  @click.stop.prevent="toggleFieldVisibility(field.dbField, fieldList(m.kind))">
                  {{ field.friendlyName }}
                  <small>({{ field.exp }})</small>
                  <v-tooltip
                    :activator="`[data-tip-id='${key + k + m.kind}']`"
                    :open-delay="300">
                    {{ field.help }}
                  </v-tooltip>
                </v-list-item>
              </template>
            </template>
          </v-list>
        </v-menu>
      </template> <!-- /field-visibility menus -->
    </div>

    <!-- network baseline time range -->
    <div class="arkime-input-group">
      <span
        class="arkime-input-label help-cursor"
        id="baselineDate">
        {{ $t('connections.baselineDate') }}
      </span>
      <v-tooltip
        activator="#baselineDate"
        :open-delay="300">
        {{ $t('connections.baselineDateTip') }}
      </v-tooltip>
      <select
        class="arkime-input-control"
        v-model="query.baselineDate"
        @change="changeBaselineDate">
        <option value="0">
          {{ $t('common.optionDisabled') }}
        </option>
        <option value="1x">
          {{ $t('connections.queryRange', 1) }}
        </option>
        <option value="2x">
          {{ $t('connections.queryRange', 2) }}
        </option>
        <option value="4x">
          {{ $t('connections.queryRange', 4) }}
        </option>
        <option value="6x">
          {{ $t('connections.queryRange', 6) }}
        </option>
        <option value="8x">
          {{ $t('connections.queryRange', 8) }}
        </option>
        <option value="10x">
          {{ $t('connections.queryRange', 10) }}
        </option>
        <option value="1">
          {{ $t('common.hourCount', 1) }}
        </option>
        <option value="6">
          {{ $t('common.hourCount', 6) }}
        </option>
        <option value="24">
          {{ $t('common.hourCount', 24) }}
        </option>
        <option value="48">
          {{ $t('common.hourCount', 48) }}
        </option>
        <option value="72">
          {{ $t('common.hourCount', 72) }}
        </option>
        <option value="168">
          {{ $t('common.weekCount', 1) }}
        </option>
        <option value="336">
          {{ $t('common.weekCount', 2) }}
        </option>
        <option value="720">
          {{ $t('common.monthCount', 1) }}
        </option>
        <option value="1440">
          {{ $t('common.monthCount', 2) }}
        </option>
        <option value="4380">
          {{ $t('common.monthCount', 6) }}
        </option>
        <option value="8760">
          {{ $t('common.yearCount', 1) }}
        </option>
      </select>
    </div> <!-- /network baseline time range -->

    <!-- network baseline node visibility -->
    <div
      class="arkime-input-group"
      v-show="query.baselineDate !== '0'">
      <span
        class="arkime-input-label help-cursor"
        id="baselineVis">
        {{ $t('connections.baselineVis') }}
      </span>
      <v-tooltip
        activator="#baselineVis"
        :open-delay="300">
        {{ $t('connections.baselineVisTip') }}
      </v-tooltip>
      <select
        class="arkime-input-control"
        :disabled="query.baselineDate === '0'"
        v-model="query.baselineVis"
        @change="changeBaselineVis">
        <option
          value="all"
          v-i18n-value="'connections.baselineVis-'" />
        <option
          value="actual"
          v-i18n-value="'connections.baselineVis-'" />
        <option
          value="actualold"
          v-i18n-value="'connections.baselineVis-'" />
        <option
          value="new"
          v-i18n-value="'connections.baselineVis-'" />
        <option
          value="old"
          v-i18n-value="'connections.baselineVis-'" />
      </select>
    </div> <!-- /network baseline node visibility -->
  </teleport> <!-- /sub-navbar controls -->

  <!-- graph + popups -->
  <div
    ref="graphContainer"
    class="connections-page connections-graph-host">
    <!-- loading overlay -->
    <arkime-loading
      :can-cancel="true"
      v-if="loading && !error"
      @cancel="cancelAndLoad" /> <!-- /loading overlay -->

    <!-- page error -->
    <arkime-error
      v-if="error"
      :message="error"
      class="mt-5" /> <!-- /page error -->

    <!-- no results -->
    <arkime-no-results
      v-if="!error && !loading && recordsFiltered === 0"
      class="mt-5"
      :view="query.view" /> <!-- /no results -->

    <!-- connections graph container -->
    <svg
      ref="svgEl"
      class="connections-graph"
      v-if="!error" /> <!-- /connections graph container -->

    <!-- popup area -->
    <div
      v-if="showPopup">
      <div class="connections-popup-host">
        <NodePopup
          v-if="dataNode"
          :data-node="dataNode"
          :fields="fieldsMap"
          :node-fields="nodeFields"
          :baseline-date="query.baselineDate"
          @close="closePopups"
          @hide-node="hideNode" />
        <LinkPopup
          v-if="dataLink"
          :data-link="dataLink"
          :fields="fieldsMap"
          :link-fields="linkFields"
          @close="closePopups"
          @hide-link="hideLink" />
      </div>
    </div> <!-- /popup area -->
  </div>

  <!-- floating overlay buttons -->
  <teleport
    defer
    to="#connections-overlay-anchor">
    <!-- Button group -->
    <div class="connections-buttons d-flex align-center ga-2">
      <!-- unlock + export -->
      <div class="d-flex ga-1">
        <v-btn
          variant="outlined"
          size="small"
          density="comfortable"
          icon
          id="unlockNodes"
          :aria-label="$t('connections.unlockNodesTip')"
          @click.stop.prevent="unlock">
          <v-icon icon="mdi-lock-open" />
          <v-tooltip
            activator="parent"
            location="bottom"
            :open-delay="300">
            {{ $t('connections.unlockNodesTip') }}
          </v-tooltip>
        </v-btn>
        <v-btn
          variant="outlined"
          size="small"
          density="comfortable"
          icon
          id="exportGraph"
          :aria-label="$t('connections.exportGraphTip')"
          @click.stop.prevent="exportPng">
          <v-icon icon="mdi-download" />
          <v-tooltip
            activator="parent"
            location="bottom"
            :open-delay="300">
            {{ $t('connections.exportGraphTip') }}
          </v-tooltip>
        </v-btn>
      </div>

      <!-- node distance -->
      <div class="d-flex ga-1">
        <v-btn
          id="nodeDistUp"
          :aria-label="$t('connections.nodeDistUpTip')"
          variant="outlined"
          size="small"
          density="comfortable"
          icon
          :disabled="query.nodeDist >= 200"
          @click="changeNodeDist(10)">
          <v-icon icon="mdi-plus-network" />
          <v-tooltip
            activator="parent"
            location="bottom"
            :open-delay="300">
            {{ $t('connections.nodeDistUpTip') }}
          </v-tooltip>
        </v-btn>
        <v-btn
          id="nodeDistDown"
          :aria-label="$t('connections.nodeDistDownTip')"
          variant="outlined"
          size="small"
          density="comfortable"
          icon
          :disabled="query.nodeDist <= 10"
          @click="changeNodeDist(-10)">
          <v-icon icon="mdi-minus-network" />
          <v-tooltip
            activator="parent"
            location="bottom"
            :open-delay="300">
            {{ $t('connections.nodeDistDownTip') }}
          </v-tooltip>
        </v-btn>
      </div>

      <!-- text size increase/decrease -->
      <div class="d-flex ga-1">
        <v-btn
          id="textSizeUp"
          :aria-label="$t('connections.textSizeUpTip')"
          variant="outlined"
          size="small"
          density="comfortable"
          icon
          :disabled="fontSize >= 1"
          @click="updateTextSize(0.1)">
          <v-icon icon="mdi-format-font-size-increase" />
          <v-tooltip
            activator="parent"
            location="bottom"
            :open-delay="300">
            {{ $t('connections.textSizeUpTip') }}
          </v-tooltip>
        </v-btn>
        <v-btn
          id="textSizeDown"
          :aria-label="$t('connections.textSizeDownTip')"
          variant="outlined"
          size="small"
          density="comfortable"
          icon
          :disabled="fontSize <= 0.2"
          @click="updateTextSize(-0.1)">
          <v-icon icon="mdi-format-font-size-decrease" />
          <v-tooltip
            activator="parent"
            location="bottom"
            :open-delay="300">
            {{ $t('connections.textSizeDownTip') }}
          </v-tooltip>
        </v-btn>
      </div>

      <!-- zoom in/out -->
      <div class="d-flex ga-1">
        <v-btn
          id="zoomIn"
          :aria-label="$t('connections.zoomInTip')"
          variant="outlined"
          size="small"
          density="comfortable"
          icon
          :disabled="zoomLevel >= 4"
          @click="zoomConnections(2)">
          <v-icon icon="mdi-magnify-plus" />
          <v-tooltip
            activator="parent"
            location="bottom"
            :open-delay="300">
            {{ $t('connections.zoomInTip') }}
          </v-tooltip>
        </v-btn>
        <v-btn
          id="zoomOut"
          :aria-label="$t('connections.zoomOutTip')"
          variant="outlined"
          size="small"
          density="comfortable"
          icon
          :disabled="zoomLevel <= 0.0625"
          @click="zoomConnections(0.5)">
          <v-icon icon="mdi-magnify-minus" />
          <v-tooltip
            activator="parent"
            location="bottom"
            :open-delay="300">
            {{ $t('connections.zoomOutTip') }}
          </v-tooltip>
        </v-btn>
      </div>
    </div> <!-- /Button group -->
  </teleport> <!-- /floating overlay buttons -->
</template>

<script>
// import components
import ArkimeError from '../utils/Error.vue';
import ArkimeLoading from '../utils/Loading.vue';
import ArkimeNoResults from '../utils/NoResults.vue';
import NodePopup from './NodePopup.vue';
import LinkPopup from './LinkPopup.vue';
// import services
import ArkimeFieldTypeahead from '../utils/FieldTypeahead.vue';
import FieldService from '../search/FieldService';
import UserService from '../users/UserService';
import ConnectionsService from './ConnectionsService';
import ConfigService from '../utils/ConfigService';
// import utils
import store from '@/store';
import Utils from '../utils/utils';
import { timezoneDateString, searchFields } from '@common/vueFilters.js';
import { resolveMessage } from '@common/resolveI18nMessage';
// lazy import these
let d3, saveSvgAsPng;

// d3 force directed graph vars/functions ---------------------------------- */
let nodeFillColors;
let simulation, svg, container, zoom;
let node, nodes, link, links, nodeLabel;
let popupTimer;
let draggingNode;
let nodeMax = 1;
let nodeMin = 1;
let linkMax = 1;
let linkMin = 1;
let linkScaleFactor = 0;
let nodeScaleFactor = 0;
const maxLog = Math.ceil(Math.pow(Math.E, 9));

const idRegex = /[\[\]:. ]/g;
let pendingPromise; // save a pending promise to be able to cancel it

// drag helpers
function dragstarted (e, d) {
  e.sourceEvent.stopPropagation();
  if (!e.active) { simulation.alphaTarget(0.1).restart(); }
  draggingNode = d;
  d.fx = d.x;
  d.fy = d.y;
  itemFocus(d);
}

function dragged (e, d) {
  d.fx = e.x;
  d.fy = e.y;
}

function dragended (e, d) {
  if (!e.active) { simulation.alphaTarget(0).stop(); }
  draggingNode = undefined;
  // keep the node where it was dragged
  d.fx = d.x;
  d.fy = d.y;
  itemFocus(d);
}

// highlighting helpers
let linkedByIndex = {};
function isConnected (a, b) {
  return linkedByIndex[a.index + ',' + b.index] ||
    linkedByIndex[b.index + ',' + a.index] ||
    a.index === b.index;
}

function itemFocus (d) {
  // don't apply focus styles if dragging a node
  if (!draggingNode) {
    node.style('opacity', (o) => {
      return isConnected(d, o) ? 1 : 0.1;
    });
    nodeLabel.attr('display', (o) => {
      return isConnected(d, o) ? 'block' : 'none';
    });
    link.style('opacity', (o) => {
      return o.source.index === d.index || o.target.index === d.index ? 1 : 0.1;
    });
  }
}

function unfocus () {
  nodeLabel.attr('display', 'block');
  node.style('opacity', 1);
  link.style('opacity', 1);
}

// simulation resize helper: size the canvas to the page scroll container
// (tracks window resizes AND toolbar collapse via the ResizeObserver below)
function resize (scrollEl) {
  if (!svg) { return; }

  const width = (scrollEl ? scrollEl.clientWidth : $(window).width()) - 10;
  const height = (scrollEl ? scrollEl.clientHeight : $(window).height() - 61) - 4;

  // set the width and height of the canvas
  svg.attr('width', width).attr('height', height);
}

// other necessary vars ---------------------------------------------------- */
// default fields to display in the node/link popups
const defaultLinkFields = ['network.bytes', 'totDataBytes', 'network.packets', 'node'];
const defaultNodeFields = ['network.bytes', 'totDataBytes', 'network.packets', 'node'];

// vue definition ---------------------------------------------------------- */
export default {
  name: 'ConnectionsGraph',
  components: {
    ArkimeError,
    ArkimeLoading,
    ArkimeNoResults,
    ArkimeFieldTypeahead,
    NodePopup,
    LinkPopup
  },
  data: function () {
    return {
      error: '',
      loading: true,
      settings: {}, // user settings
      recordsFiltered: 0,
      fieldsMap: {},
      fieldQuery: '',
      scrollEl: undefined, // scroll container (sizes the svg)
      srcFieldTypeahead: undefined,
      dstFieldTypeahead: undefined,
      groupedFields: undefined,
      foregroundColor: undefined,
      primaryColor: undefined,
      secondaryColor: undefined,
      tertiaryColor: undefined,
      highlightPrimaryColor: undefined,
      highlightSecondaryColor: undefined,
      highlightTertiaryColor: undefined,
      fontSize: 0.4,
      zoomLevel: 1,
      weight: 'sessions',
      fieldHistoryConnectionsSrc: undefined,
      fieldHistoryConnectionsDst: undefined,
      showPopup: false, // whether to show the node/link data popup
      dataNode: undefined, // data for the node popup
      dataLink: undefined, // data for the link popup
      // node + link field-visibility menus share one v-menu template;
      // kind drives which underlying list (nodeFields / linkFields)
      // and reset-default array (defaultNodeFields / defaultLinkFields)
      // the menu mutates.
      fieldVisMenus: [
        { kind: 'node', icon: 'mdi-circle-outline', tipKey: 'connections.nodeFieldsTip' },
        { kind: 'link', icon: 'mdi-link', tipKey: 'connections.linkFieldsTip' }
      ]
    };
  },
  computed: {
    query: function () {
      return {
        start: 0, // first item index
        length: this.$route.query.connectionsLength || 100, // page length
        date: store.state.timeRange,
        startTime: store.state.time.startTime,
        stopTime: store.state.time.stopTime,
        srcField: this.$route.query.srcField || store.state.user.settings.connSrcField || 'source.ip',
        dstField: this.$route.query.dstField || store.state.user.settings.connDstField || 'destination.ip',
        bounding: this.$route.query.bounding || 'last',
        interval: this.$route.query.interval || 'auto',
        spanning: this.$route.query.spanning === 'true' ? true : undefined,
        minConn: this.$route.query.minConn || 1,
        baselineDate: this.$route.query.baselineDate || '0',
        baselineVis: this.$route.query.baselineVis || 'all',
        nodeDist: this.$route.query.nodeDist || 40,
        view: this.$route.query.view || undefined,
        expression: store.state.expression || undefined,
        cluster: this.$route.query.cluster || undefined
      };
    },
    user: function () {
      return store.state.user;
    },
    filteredFields: function () {
      const filteredGroupedFields = {};

      for (const group in this.groupedFields) {
        filteredGroupedFields[group] = searchFields(
          this.fieldQuery,
          this.groupedFields[group]
        );
      }

      return filteredGroupedFields;
    },
    nodeFields: {
      get: function () {
        return store.state.user.settings.connNodeFields || defaultNodeFields;
      },
      set: function (newValue) {
        const settings = store.state.user.settings;
        settings.connNodeFields = newValue;
        store.commit('setUserSettings', settings);
      }
    },
    linkFields: {
      get: function () {
        return store.state.user.settings.connLinkFields || defaultLinkFields;
      },
      set: function (newValue) {
        const settings = store.state.user.settings;
        settings.connLinkFields = newValue;
        store.commit('setUserSettings', settings);
      }
    },
    fields () {
      return FieldService.addIpDstPortField(store.state.fieldsArr);
    }
  },
  watch: {
    '$route.query.connectionsLength': function (newVal, oldVal) {
      this.cancelAndLoad(true);
    },
    '$route.query.baselineDate': function (newVal, oldVal) {
      this.cancelAndLoad(true);
    },
    '$route.query.minConn': function (newVal, oldVal) {
      this.cancelAndLoad(true);
    },
    '$route.query.nodeDist': function (newVal, oldVal) {
      simulation.force('link',
        d3.forceLink(links).id((d) => {
          return d.pos; // tell the links where to link
        }).distance(parseInt(this.query.nodeDist)) // set the link distance
      ).force(
        'charge', // recalculate the gravity mutually amongst all nodes
        d3.forceManyBody().strength(-parseInt(this.query.nodeDist * 2))
      );

      // restart the graph simulation
      simulation.alphaTarget(0.3).restart();
    },
    '$route.query.srcField': function (newVal, oldVal) {
      this.cancelAndLoad(true);
    },
    '$route.query.dstField': function (newVal, oldVal) {
      this.cancelAndLoad(true);
    }
  },
  mounted () {
    // the host PageLayout wraps us in a .page-scroll container; size the
    // graph svg against it (covers window resizes and toolbar collapse)
    this.scrollEl = this.$refs.graphContainer?.closest('.page-scroll');

    // IMPORTANT: this kicks off loading data and drawing the graph
    this.cancelAndLoad(true);

    UserService.getPageConfig('connections').then((response) => {
      this.fieldHistoryConnectionsSrc = response.fieldHistoryConnectionsSrc.fields || [];
      this.fieldHistoryConnectionsDst = response.fieldHistoryConnectionsDst.fields || [];
    });
    this.setupFields();
    this.srcFieldTypeahead = FieldService.getFieldProperty(this.query.srcField, 'friendlyName');
    this.dstFieldTypeahead = FieldService.getFieldProperty(this.query.dstField, 'friendlyName');

    // close any node/link popups if the user presses escape
    window.addEventListener('keyup', this.closePopupsOnEsc);
    // resize the simulation with its container (covers window resizes and
    // the animated toolbar collapse)
    if (this.scrollEl) {
      this._svgResizeObserver = new ResizeObserver(() => {
        resize(this.scrollEl);
      });
      this._svgResizeObserver.observe(this.scrollEl);
    }
  },
  methods: {
    /* exposed page functions ---------------------------------------------- */
    /**
     * Debounces updates to the field-visibility search filter so filtering
     * (which can run over 1000s of fields) doesn't run on every keystroke
     * @param {string} value The raw input value
     */
    debouncedSetFieldQuery: function (value) {
      if (this._fieldQueryTimer) { clearTimeout(this._fieldQueryTimer); }
      this._fieldQueryTimer = setTimeout(() => {
        this.fieldQuery = value;
      }, 300);
    },
    /**
     * Cancels the pending session query (if it's still pending) and runs a new
     * query if requested
     * @param {bool} runNewQuery  Whether to run a new connections query after
     *                            canceling the request
     */
    cancelAndLoad: function (runNewQuery) {
      const clientCancel = () => {
        if (pendingPromise) {
          pendingPromise.controller.cancel();
          pendingPromise = null;
        }

        if (!runNewQuery) {
          this.loading = false;
          if (!this.fields.length) {
            // show a page error if there is no data on the page
            this.error = 'You canceled the search';
          }
          return;
        }

        this.loadData();
      };

      if (pendingPromise) {
        ConfigService.cancelEsTask(pendingPromise.cancelId).then((response) => {
          clientCancel();
        }).catch((error) => {
          clientCancel();
        });
      } else if (runNewQuery) {
        this.loadData();
      }
    },
    changeLength: function (len) {
      this.query.length = len;
      this.$router.push({
        query: {
          ...this.$route.query,
          connectionsLength: len
        }
      });
    },
    changeBaselineDate: function () {
      this.$router.push({
        query: {
          ...this.$route.query,
          baselineDate: this.query.baselineDate
        }
      });
    },
    changeBaselineVis: function () {
      svg.selectAll('.node')
        .attr('visibility', this.calculateNodeBaselineVisibility);

      // TODO: is there a way to get each label's/link's associated node(s)
      // and just get its visibility rather than re-running
      // calculateNodeBaselineVisibility/calculateLinkBaselineVisibility
      // for all of them?

      svg.selectAll('.node-label')
        .attr('visibility', this.calculateNodeBaselineVisibility);

      svg.selectAll('.link')
        .attr('visibility', this.calculateLinkBaselineVisibility);

      this.$router.push({
        query: {
          ...this.$route.query,
          baselineVis: this.query.baselineVis
        }
      });
    },
    changeMinConn: function (minConn) {
      this.query.minConn = minConn;
      this.$router.push({
        query: {
          ...this.$route.query,
          minConn
        }
      });
    },
    changeWeight: function (weight) {
      this.weight = weight;
      if (this.weight) { this.getMinMaxForScale(); }

      svg.selectAll('.node')
        .attr('r', this.calculateNodeWeight);

      svg.selectAll('.link')
        .attr('stroke-width', this.calculateLinkWeight);

      svg.selectAll('.node-label')
        .attr('dx', this.calculateNodeLabelOffset);
    },
    changeNodeDist: function (direction) {
      this.query.nodeDist = direction > 0
        ? Math.min(+this.query.nodeDist + direction, 200)
        : Math.max(+this.query.nodeDist + direction, 10);

      if (this.query.nodeDist === +this.$route.query.nodeDist) {
        return;
      }

      this.$router.push({
        query: {
          ...this.$route.query,
          nodeDist: this.query.nodeDist
        }
      });
    },
    changeSrcField: function (field) {
      this.srcFieldTypeahead = field.friendlyName;
      this.query.srcField = field.dbField;
      this.$router.push({
        query: {
          ...this.$route.query,
          srcField: this.query.srcField
        }
      });
    },
    changeDstField: function (field) {
      this.dstFieldTypeahead = field.friendlyName;
      this.query.dstField = field.dbField;
      this.$router.push({
        query: {
          ...this.$route.query,
          dstField: this.query.dstField
        }
      });
    },
    isFieldVisible: function (id, list) {
      return list.indexOf(id);
    },
    /* Returns the visible-fields ref array for a given menu kind
       ('node' | 'link'). Used to drive the shared v-menu template. */
    fieldList: function (kind) {
      return kind === 'node' ? this.nodeFields : this.linkFields;
    },
    /* Reset the visible fields for the given menu kind back to its
       default array. Replaces resetNodeFieldsDefault /
       resetLinkFieldsDefault. */
    resetFieldVisibility: function (kind) {
      if (kind === 'node') {
        this.nodeFields = defaultNodeFields;
      } else {
        this.linkFields = defaultLinkFields;
      }
      this.closePopups();
      this.cancelAndLoad(true);
      this.saveVisibleFields();
    },
    toggleFieldVisibility: function (id, list) {
      const index = this.isFieldVisible(id, list);

      if (index >= 0) { // it's visible
        // remove it from the visible node fields list
        list.splice(index, 1);
      } else { // it's hidden
        // add it to the visible headers list
        list.push(id);
        this.closePopups();
        this.cancelAndLoad(true);
      }

      this.saveVisibleFields();
    },
    unlock: function () {
      svg.selectAll('.node').each((d) => {
        d.fx = undefined;
        d.fy = undefined;
      });
      simulation.alphaTarget(0.3).restart();
    },
    zoomConnections: function (direction) {
      this.zoomLevel = direction > 0.5
        ? Math.min(this.zoomLevel * direction, 4)
        : Math.max(this.zoomLevel * direction, 0.0625);

      svg.transition().duration(500).call(zoom.scaleBy, direction);
    },
    exportPng: function () {
      const foregroundColor = this.foregroundColor;

      import('save-svg-as-png').then((saveSvgAsPngModule) => {
        saveSvgAsPng = saveSvgAsPngModule;
        saveSvgAsPng.saveSvgAsPng(
          document.getElementById('graphSvg'),
          'connections.png',
          {
            backgroundColor: this.backgroundColor,
            modifyCss: function (selector, properties) {
              if (selector === '.connections-page text') {
                // remove .connections-page from selector since element is
                // rendered outside connections page for saving as png
                selector = 'text';
                // make sure that the text uses the foreground property
                // saveSvgAsPng cannot resolve rgb(var(--v-theme-foreground))
                properties = `fill: ${foregroundColor}`;
              }
              return selector + '{' + properties + '}';
            }
          }
        );
      });
    },
    updateTextSize: function (direction) {
      this.fontSize = direction > 0
        ? Math.min(this.fontSize + direction, 1)
        : Math.max(this.fontSize + direction, 0.2);

      svg.selectAll('.node-label')
        .style('font-size', this.fontSize + 'em');
    },
    closePopups () {
      this.showPopup = false;
      this.dataNode = undefined;
      this.dataLink = undefined;
      if (popupTimer) { clearTimeout(popupTimer); }
    },
    closePopupsOnEsc (e) {
      if (e.key === 'Escape') { // esc
        this.closePopups();
      }
    },
    hideNode () {
      const id = '#id' + this.dataNode.id.replace(idRegex, '_');
      svg.select(id).remove();
      svg.select(id + '-label').remove();
      svg.selectAll('.link').filter((d) => {
        return d.source.id === this.dataNode.id || d.target.id === this.dataNode.id;
      }).remove();
      this.closePopups();
    },
    hideLink () {
      svg.selectAll('.link').filter((d, i) => {
        return d.source.id === this.dataLink.source.id && d.target.id === this.dataLink.target.id;
      }).remove();
      this.closePopups();
    },
    /* helper functions ---------------------------------------------------- */
    async loadData () {
      if (!Utils.checkClusterSelection(this.query.cluster, store.state.esCluster.availableCluster.active, this).valid) {
        this.drawGraphWrapper({ nodes: [], links: [] }); // draw empty graph
        this.recordsFiltered = 0;
        pendingPromise = null;
        return;
      }

      this.error = '';
      this.loading = true;

      if (!this.$route.query.srcField) {
        this.query.srcField = FieldService.getFieldProperty(this.user.settings.connSrcField, 'dbField');
      }
      if (!this.$route.query.dstField) {
        this.query.dstField = FieldService.getFieldProperty(this.user.settings.connDstField, 'dbField');
      }

      // send the requested fields with the query
      const fields = this.nodeFields;
      // combine fields from nodes and links
      for (const f in this.linkFields) {
        const id = this.linkFields[f];
        if (fields.indexOf(id) > -1) { continue; }
        fields.push(id);
      }
      this.query.fields = fields.join(',');

      // create unique cancel id to make cancel req for corresponding es task
      const cancelId = Utils.createRandomString();
      this.query.cancelId = cancelId;

      try {
        const { controller, fetcher } = await ConnectionsService.get(this.query);
        pendingPromise = { controller, cancelId };

        const response = await fetcher; // do the fetch
        pendingPromise = null;
        this.error = '';
        this.loading = false;
        this.recordsFiltered = response.recordsFiltered;
        this.drawGraphWrapper(response);
      } catch (error) {
        pendingPromise = null;
        this.loading = false;
        this.error = resolveMessage(error, this.$t);
      }
    },
    setupFields: function () {
      // group fields map by field group
      // and remove duplicate fields (e.g. 'host.dns' & 'dns.host')
      const existingFieldsLookup = {}; // lookup map of fields in fieldsArray
      this.groupedFields = {};
      for (const field of this.fields) {
        // don't include fields with regex
        if (field.regex) { continue; }
        if (!existingFieldsLookup[field.exp]) {
          this.fieldsMap[field.dbField] = field;
          existingFieldsLookup[field.exp] = field;
          if (!this.groupedFields[field.group]) {
            this.groupedFields[field.group] = [];
          }
          this.groupedFields[field.group].push(field);
        }
      }
    },
    saveVisibleFields: function () {
      this.user.settings.connNodeFields = this.nodeFields;
      this.user.settings.connLinkFields = this.linkFields;

      UserService.saveSettings(this.user.settings, this.user.userId);
    },
    drawGraphWrapper: function (data) {
      import('d3').then((d3Module) => {
        d3 = d3Module;
        this.drawGraph(data);
      });
    },
    drawGraph: function (data) {
      if (!nodeFillColors) {
        // D3 fills need real color values, not CSS var() refs. Read the
        // active Vuetify theme's tokens (Vuetify emits each as `r,g,b`)
        // and wrap in rgb() for the SVG fill attribute.
        const styles = window.getComputedStyle(document.documentElement);
        const themed = (key, fallback) => {
          const v = styles.getPropertyValue(`--v-theme-${key}`).trim();
          return v ? `rgb(${v})` : fallback;
        };
        this.backgroundColor = themed('background', '#FFFFFF');
        this.foregroundColor = themed('foreground', '#212529');
        this.primaryColor = themed('primary', '#000000');
        this.secondaryColor = themed('quaternary', '#000000');
        this.tertiaryColor = themed('tertiary', '#000000');
        this.highlightPrimaryColor = themed('primary-lighter', this.primaryColor);
        this.highlightSecondaryColor = themed('secondary-lighter', this.secondaryColor);
        this.highlightTertiaryColor = themed('tertiary-lighter', this.tertiaryColor);
        nodeFillColors = ['', this.primaryColor, this.secondaryColor, this.tertiaryColor];
      }

      if (svg) { // remove any existing nodes
        node.exit().remove();
        link.exit().remove();
        nodeLabel.exit().remove();
        svg.selectAll('.link').remove();
        svg.selectAll('.node').remove();
        svg.selectAll('.node-label').remove();
      }

      // don't do anything if there's no data to process
      if (!data.nodes.length) { return; }

      // convert time in ms to timezone date string
      const srcFieldIsTime = FieldService.getFieldProperty(this.query.srcField, 'type') === 'seconds';
      const dstFieldIsTime = FieldService.getFieldProperty(this.query.dstField, 'type') === 'seconds';

      if (srcFieldIsTime || dstFieldIsTime) {
        for (const dataNode of data.nodes) {
          // only parse date values if the source field is of type seconds
          // and the node is a source node OR if the destination field is
          // of type seconds and the node is a destination (target) node
          if ((srcFieldIsTime && dataNode.type === 1) ||
            (dstFieldIsTime && dataNode.type === 2)) {
            dataNode.id = timezoneDateString(
              dataNode.id,
              this.settings.timezone ||
                store.state.user.settings.timezone,
              this.settings.ms ||
                store.state.user.settings.ms
            );
          }
        }
      }

      // map which nodes are linked (for highlighting)
      linkedByIndex = {};
      data.links.forEach((d) => {
        linkedByIndex[d.source + ',' + d.target] = true;
      });

      // calculate the width and height of the canvas from the scroll container
      const scrollEl = this.scrollEl;
      const width = (scrollEl ? scrollEl.clientWidth : $(window).width()) - 10;
      const height = (scrollEl ? scrollEl.clientHeight : $(window).height() - 61) - 4;

      // get the node and link data
      links = data.links.map(d => Object.create(d));
      nodes = data.nodes.map(d => Object.create(d));

      // get the min/max values of links and nodes to scale the weight
      this.getMinMaxForScale();

      // setup the force directed graph
      simulation = d3.forceSimulation(nodes)
        .force('link',
          d3.forceLink(links).id((d) => {
            return d.pos; // tell the links where to link
          }).distance(this.query.nodeDist) // set the link distance
        )
        // simulate gravity mutually amongst all nodes
        .force('charge', d3.forceManyBody().strength(-parseInt(this.query.nodeDist * 2)))
        // prevent elements from overlapping
        .force('collision', d3.forceCollide().radius(this.calculateCollisionRadius))
        // set the graph center
        .force('center', d3.forceCenter(width / 2, height / 2))
        // positioning force along x-axis for disjoint graph
        .force('x', d3.forceX())
        // positioning force along y-axis for disjoint graph
        .force('y', d3.forceY());

      if (!svg) {
        // scope to our own svg ref, never another viz's svg
        svg = d3.select(this.$refs.svgEl)
          .attr('width', width)
          .attr('height', height)
          .attr('id', 'graphSvg');
      }

      if (!container) {
        // add container for zoomability
        container = svg.append('g');
      }

      // add zoomability
      svg.call(
        zoom = d3.zoom()
          .scaleExtent([0.1, 4])
          .on('zoom', (e) => {
            container.attr('transform', e.transform);
          })
      );

      // add links
      link = container.append('g')
        .attr('stroke', this.foregroundColor)
        .attr('stroke-opacity', 0.4)
        .selectAll('line')
        .data(links)
        .enter().append('line')
        .attr('class', 'link')
        .attr('stroke-width', this.calculateLinkWeight)
        .attr('visibility', this.calculateLinkBaselineVisibility);

      // add link mouse listeners for showing popups
      link.on('mouseover', (e, l) => {
        if (draggingNode) { return; }
        if (popupTimer) { clearTimeout(popupTimer); }
        popupTimer = setTimeout(() => {
          this.showLinkPopup(l);
        }, 600);
      }).on('mouseout', () => {
        if (popupTimer) { clearTimeout(popupTimer); }
      });

      // add nodes
      node = container.append('g')
        .selectAll('circle')
        .data(nodes)
        .enter()
        .append('circle')
        .attr('class', 'node')
        .attr('id', (d) => {
          return 'id' + d.id.replace(idRegex, '_');
        })
        .attr('fill', (d) => {
          return nodeFillColors[d.type];
        })
        .attr('r', this.calculateNodeWeight)
        .attr('stroke', this.foregroundColor)
        .attr('stroke-width', 0.5)
        .attr('visibility', this.calculateNodeBaselineVisibility)
        .call(d3.drag()
          .on('start', dragstarted)
          .on('drag', dragged)
          .on('end', dragended)
        );

      // add node mouse listeners for showing focus and popups
      node.on('mouseover', (e, d) => {
        if (draggingNode) { return; }
        if (popupTimer) { clearTimeout(popupTimer); }
        popupTimer = setTimeout(() => {
          this.showNodePopup(d);
        }, 600);
        itemFocus(d);
      }).on('mouseout', (e, d) => {
        if (popupTimer) { clearTimeout(popupTimer); }
        unfocus(d);
      });

      // add node labels
      nodeLabel = container.append('g')
        .selectAll('text')
        .data(nodes)
        .enter()
        .append('text')
        .attr('dx', this.calculateNodeLabelOffset)
        .attr('id', (d) => {
          return 'id' + d.id.replace(idRegex, '_') + '-label';
        })
        .attr('dy', '2px')
        .attr('class', 'node-label')
        .style('font-size', this.fontSize + 'em')
        .style('font-weight', this.calculateNodeLabelWeight)
        .style('font-style', this.calculateNodeLabelStyle)
        .attr('visibility', this.calculateNodeBaselineVisibility)
        .style('pointer-events', 'none') // to prevent mouseover/drag capture
        .text((d) => { return d.id + this.calculateNodeLabelSuffix(d); });

      // listen on each tick of the simulation's internal timer
      simulation.on('tick', () => {
        // position links
        link.attr('x1', d => d.source.x)
          .attr('y1', d => d.source.y)
          .attr('x2', d => d.target.x)
          .attr('y2', d => d.target.y);

        // position nodes
        node.attr('cx', d => d.x)
          .attr('cy', d => d.y);

        // position node labels
        nodeLabel.attr('transform', function (d) {
          return 'translate(' + d.x + ',' + d.y + ')';
        });
      });
    },
    getMinMaxForScale: function () {
      let weightField = this.weight;

      nodeMax = 1;
      nodeMin = 1;
      for (const n of nodes) {
        if (n[weightField] !== undefined) {
          if (n[weightField] > nodeMax) {
            nodeMax = n[weightField];
          }
          if (n[weightField] < nodeMin) {
            nodeMin = n[weightField];
          }
        }
      }

      linkMax = 1;
      linkMin = 1;
      if (weightField === 'sessions') {
        weightField = 'value';
      }
      for (const l of links) {
        if (l[weightField] !== undefined) {
          if (l[weightField] > linkMax) {
            linkMax = l[weightField];
          }
          if (l[weightField] < linkMin) {
            linkMin = l[weightField];
          }
        }
      }

      nodeScaleFactor = (nodeMax - nodeMin) / maxLog;
      if (nodeScaleFactor < 1) { nodeScaleFactor = 1; }
      linkScaleFactor = (linkMax - linkMin) / maxLog;
      if (linkScaleFactor < 1) { linkScaleFactor = 1; }
    },
    calculateLinkWeight: function (l) {
      let val = this.weight ? l[this.weight] || l.value : 1;
      if (this.weight) {
        val = Math.max(Math.log((val - linkMin) / linkScaleFactor), 0);
      }
      return 1 + val;
    },
    calculateNodeWeight: function (n) {
      let val = this.weight ? n[this.weight] || n.sessions : 1;
      if (this.weight) {
        val = Math.max(Math.log((val - nodeMin) / nodeScaleFactor), 0);
      }
      return 3 + val;
    },
    calculateNodeLabelOffset: function (nl) {
      const val = this.calculateNodeWeight(nl);
      return 2 + val;
    },
    calculateNodeLabelWeight: function (n) {
      let val = 'normal';
      if (this.query.baselineDate !== '0') {
        switch (n.inresult) {
        case 2:
          // "old" (in baseline, not in actual result set)
          val = 'lighter';
          break;
        case 1:
          // "new" (in actual, not in baseline result set)
          val = 'bold';
          break;
        }
      }
      return val;
    },
    calculateNodeLabelStyle: function (n) {
      // italicize "old" nodes (in baseline, not in actual result set)
      return ((this.query.baselineDate !== '0') && (n.inresult === 2)) ? 'italic' : 'normal';
    },
    calculateNodeLabelSuffix: function (n) {
      let val = '';
      if (this.query.baselineDate !== '0') {
        switch (n.inresult) {
        case 2:
          // "old" (in baseline, not in actual result set)
          val = ' 🚫';
          break;
        case 1:
          // "new" (in actual, not in baseline result set)
          val = ' ✨';
          break;
        }
      }
      return val;
    },
    calculateNodeBaselineVisibility: function (n) {
      let val = 'visible';

      if (this.query.baselineDate !== '0') {
        /* eslint-disable no-bitwise */
        const inActualSet = ((n.inresult & 0x1) !== 0);
        const inBaselineSet = ((n.inresult & 0x2) !== 0);
        /* eslint-enable no-bitwise */
        switch (this.query.baselineVis) {
        case 'actual':
          val = inActualSet ? 'visible' : 'hidden';
          break;
        case 'actualold':
          val = inBaselineSet ? 'visible' : 'hidden';
          break;
        case 'new':
          val = (inActualSet && !inBaselineSet) ? 'visible' : 'hidden';
          break;
        case 'old':
          val = (!inActualSet && inBaselineSet) ? 'visible' : 'hidden';
          break;
        }
      }

      return val;
    },
    calculateLinkBaselineVisibility: function (l) {
      let val = 'visible';

      if (this.query.baselineDate !== '0') {
        const nodesVisibilities = [this.calculateNodeBaselineVisibility(l.source), this.calculateNodeBaselineVisibility(l.target)];
        val = (nodesVisibilities.includes('hidden')) ? 'hidden' : 'visible';
      }

      return val;
    },
    calculateCollisionRadius: function (n) {
      const val = this.calculateNodeWeight(n);
      return 2 * val;
    },
    showNodePopup (dataNode) {
      this.dataLink = undefined;
      this.dataNode = dataNode;
      if (dataNode.type === 2) {
        this.dataNode.dbField = FieldService.getFieldProperty(this.query.dstField, 'dbField');
        this.dataNode.exp = FieldService.getFieldProperty(this.query.dstField, 'exp');
      } else {
        this.dataNode.dbField = FieldService.getFieldProperty(this.query.srcField, 'dbField');
        this.dataNode.exp = FieldService.getFieldProperty(this.query.srcField, 'exp');
      }
      this.showPopup = true; // show the popup
    },
    showLinkPopup (dataLink) {
      this.dataNode = undefined;
      this.dataLink = dataLink;
      this.dataLink.dstDbField = FieldService.getFieldProperty(this.query.dstField, 'dbField');
      this.dataLink.srcDbField = FieldService.getFieldProperty(this.query.srcField, 'dbField');
      this.dataLink.dstExp = FieldService.getFieldProperty(this.query.dstField, 'exp');
      this.dataLink.srcExp = FieldService.getFieldProperty(this.query.srcField, 'exp');
      this.showPopup = true;
    }
  },
  beforeUnmount () {
    if (pendingPromise) {
      pendingPromise.controller.abort('Closing Connections page canceled the search');
      pendingPromise = null;
    }

    // remove listeners
    this._svgResizeObserver?.disconnect();
    window.removeEventListener('keyup', this.closePopupsOnEsc);

    if (d3) {
      // d3 doesn't have .off function to remove listeners,
      // so use .on('listener', null)
      d3.zoom().on('zoom', null);
      if (simulation) { simulation.on('tick', null); }
      d3.drag()
        .on('start', null)
        .on('drag', null)
        .on('end', null);
    }

    if (svg) {
      node.on('mouseover', null)
        .on('mouseout', null);
      link.on('mouseover', null)
        .on('mouseout', null);

      // remove svg elements
      node.exit().remove();
      link.exit().remove();
      nodeLabel.exit().remove();
      svg.selectAll('.link').remove();
      svg.selectAll('.node').remove();
      svg.selectAll('.node-label').remove();
      container.remove();
      svg.remove();
    }

    // listeners/elements are detached above, so reset the module-level d3
    // state synchronously -- a remount then always rebuilds cleanly
    if (simulation) { simulation.stop(); }
    svg = undefined;
    zoom = undefined;
    node = undefined;
    link = undefined;
    nodeFillColors = undefined;
    container = undefined;
    nodeLabel = undefined;
    popupTimer = undefined;
    simulation = undefined;
    draggingNode = undefined;
  }
};
</script>

<style>
.connections-page text {
  fill: rgb(var(--v-theme-foreground));
}
</style>

<style scoped>
/* graph host: positioning context for the absolutely-placed popup */
.connections-graph-host {
  position: relative;
}

.connections-graph {
  /* don't allow selecting text */
  -webkit-user-select: none;
  -moz-user-select: none;
  user-select: none;
}

/* connections-field-row: label-cell + FieldTypeahead pair. No outer
   container border (FieldTypeahead's own input border is the visible
   one); a flush colored legend cell on the left and the typeahead
   input flush against it. */
.connections-field-row {
  display: inline-flex;
  align-items: stretch;
  height: 32px;
}
.connections-legend-cell {
  display: inline-flex;
  align-items: center;
  padding: 0 8px;
  font-weight: 700;
  font-size: 0.875rem;
  color: rgb(var(--v-theme-button-fg));
  border-radius: 4px 0 0 4px;
  white-space: nowrap;
}
/* the standalone "Src & dst" legend is its own self-contained chip,
   not a label-cell glued to an input. */
.connections-legend-standalone {
  height: 32px;
  border-radius: 4px;
}
.connections-field-row .primary-legend { background-color: rgb(var(--v-theme-primary)); }
.connections-field-row .secondary-legend { background-color: rgb(var(--v-theme-secondary)); }
.connections-legend-standalone.primary-legend { background-color: rgb(var(--v-theme-primary)); }
.connections-legend-standalone.secondary-legend { background-color: rgb(var(--v-theme-secondary)); }
.connections-legend-standalone.tertiary-legend { background-color: rgb(var(--v-theme-tertiary)); }

/* Flush the inner FieldTypeahead's input against the legend cell:
   straighten its left edge, keep its right side rounded. */
.connections-field-row :deep(input) {
  border-radius: 0 4px 4px 0;
  border-left: none;
  height: 100%;
}

/* apply foreground theme color */
.connections-page svg {
  fill: rgb(var(--v-theme-foreground));
}

/* buttons overlaying the graph (in the page shell's overlay slot, which
   tracks the content area — no toolbar offset math) */
.connections-buttons {
  position: absolute;
  top: 8px;
  right: 10px;
}
</style>

<style>
/* not scoped: targets child popup components */
/* popup host: positions the card; no overflow clip so its shadow can spill */
.connections-page div.connections-popup-host {
  position: absolute;
  left: 0;
  top: 8px;
  z-index: 5;
  min-width: 280px;
  max-width: 400px;
}
/* the card caps its height, scrolls, and uses a louder shadow than Vuetify's */
.connections-page .connections-popup.v-card {
  max-height: calc(100vh - 200px);
  overflow-y: auto;
  box-shadow: 0 8px 30px -4px rgba(0, 0, 0, 0.55) !important;
}

/* label / value grid */
.connections-page .connections-popup-grid {
  display: grid;
  grid-template-columns: auto 1fr;
  column-gap: 16px;
  row-gap: 6px;
  margin: 0;
  font-size: 0.8125rem;
  align-items: baseline;
}
.connections-page .connections-popup-grid dt {
  font-weight: 500;
  color: rgba(var(--v-theme-on-surface), 0.6);
  white-space: nowrap;
}
.connections-page .connections-popup-grid dd {
  margin: 0;
  white-space: normal;
  word-break: break-word;
}

/* Field-vis menu trigger -- the round node/link icon buttons that open
   the field-visibility dropdowns. Vuetify v-menu teleports the v-list
   to body, so .field-vis-list is targeted globally (not under
   .connections-page) to reach the portal. */
.field-vis-list {
  max-height: 300px;
  min-width: 260px;
}
.field-vis-list .field-vis-group-header {
  text-transform: uppercase;
  margin-top: 8px;
  padding: .2rem .5rem;
  font-size: 110%;
  font-weight: bold;
  opacity: .75;
}
</style>
