<!--
Copyright Yahoo Inc.
SPDX-License-Identifier: Apache-2.0
-->
<template>

  <div class="connections-page">
    <ArkimeCollapsible>
      <span class="fixed-header">
        <!-- search navbar -->
        <arkime-search
          :start="query.start"
          @changeSearch="cancelAndLoad(true)">
        </arkime-search> <!-- /search navbar -->

        <!-- connections sub navbar -->
        <form class="connections-form">
          <div class="form-inline pr-1 pl-1 pt-1 pb-1">

            <!-- query size select -->
            <div class="input-group input-group-sm">
              <div class="input-group-prepend help-cursor"
                v-b-tooltip.hover.bottom.d300="'Query Size'">
                <span class="input-group-text">
                  Query Size
                </span>
              </div>
              <select class="form-control input-sm"
                v-model="query.length"
                @change="changeLength">
                <option value="100">100</option>
                <option value="500">500</option>
                <option value="1000">1,000</option>
                <option value="5000">5,000</option>
                <option value="10000">10,000</option>
                <option value="50000">50,000</option>
                <option value="100000">100,000</option>
              </select>
            </div> <!-- /query size select -->

            <!-- src select -->
            <div class="form-group ml-1"
              v-if="fields && fields.length && srcFieldTypeahead && fieldHistoryConnectionsSrc">
              <div class="input-group input-group-sm">
                <span class="input-group-prepend legend cursor-help"
                  v-b-tooltip.hover.bottom.d300="'Select a field for the source nodes. This is the color of a source node.'">
                  <span class="input-group-text primary-legend">
                    Src:
                  </span>
                </span>
                <arkime-field-typeahead
                  :fields="fields"
                  query-param="srcField"
                  :initial-value="srcFieldTypeahead"
                  @fieldSelected="changeSrcField"
                  :history="fieldHistoryConnectionsSrc"
                  page="ConnectionsSrc">
                </arkime-field-typeahead>
              </div>
            </div> <!-- /src select -->

            <!-- dst select -->
            <div class="form-group ml-1"
              v-if="fields && dstFieldTypeahead && fieldHistoryConnectionsDst">
              <div class="input-group input-group-sm">
                <span class="input-group-prepend legend cursor-help"
                  v-b-tooltip.hover.bottom.d300="'Select a field for the destination nodes. This is the color of a destination node.'">
                  <span class="input-group-text tertiary-legend">
                    Dst:
                  </span>
                </span>
                <arkime-field-typeahead
                  :fields="fields"
                  query-param="dstField"
                  :initial-value="dstFieldTypeahead"
                  @fieldSelected="changeDstField"
                  :history="fieldHistoryConnectionsDst"
                  page="ConnectionsDst">
                </arkime-field-typeahead>
              </div>
            </div> <!-- /dst select -->

            <!-- src & dst color -->
            <div class="form-group ml-1">
              <div class="input-group input-group-sm">
                <span class="input-group-prepend legend cursor-help"
                  v-b-tooltip.hover.bottom.d300="'This is the color of a node that is both a source and destination node'">
                  <span class="input-group-text secondary-legend">
                    Src &amp; Dst
                  </span>
                </span>
              </div>
            </div> <!-- /src & dst color -->

            <!-- min connections select -->
            <div class="input-group input-group-sm ml-1">
              <div class="input-group-prepend help-cursor">
                <span class="input-group-text"
                  v-b-tooltip.hover.bottom.d300="'Minimum number of sessions between nodes'">
                  Min. Connections
                </span>
              </div>
              <b-select class="form-control input-sm"
                v-model="query.minConn"
                @change="changeMinConn"
                :options="[1,2,3,4,5]">
              </b-select>
            </div> <!-- /min connections select -->

            <!-- weight select -->
            <div class="input-group input-group-sm ml-1">
              <div class="input-group-prepend help-cursor"
                v-b-tooltip.hover.bottom.d300="'Change the field that calculates the radius of nodes and the width links'">
                <span class="input-group-text">
                  Node/Link Weight
                </span>
              </div>
              <select class="form-control input-sm"
                v-model="weight"
                @change="changeWeight">
                <option value="sessions">Sessions</option>
                <option value="network.packets">Packets</option>
                <option value="network.bytes">Total Raw Bytes</option>
                <option value="totDataBytes">Total Data Bytes</option>
                <option value="">None</option>
              </select>
            </div> <!-- /weight select -->

            <!-- node fields button -->
            <b-dropdown
              size="sm"
              no-flip
              no-caret
              toggle-class="rounded"
              class="field-vis-menu ml-1"
              variant="theme-primary"
              v-if="fields && groupedFields && nodeFields">
              <template slot="button-content">
                <span class="fa fa-circle-o"
                  v-b-tooltip.hover.bottom.d300="'Toggle visible fields in the node popups'">
                </span>
              </template>
              <b-dropdown-header>
                <input type="text"
                  v-model="fieldQuery"
                  class="form-control form-control-sm dropdown-typeahead"
                  placeholder="Search for fields..."
                />
              </b-dropdown-header>
              <b-dropdown-divider>
              </b-dropdown-divider>
              <b-dropdown-item
                @click.stop.prevent="resetNodeFieldsDefault">
                Reset to default
              </b-dropdown-item>
              <b-dropdown-divider>
              </b-dropdown-divider>
              <template v-for="(group, key) in filteredFields">
                <b-dropdown-header
                  :key="key"
                  v-if="group.length"
                  class="group-header">
                  {{ key }}
                </b-dropdown-header>
                <template v-for="(field, k) in group">
                  <b-dropdown-item
                    :id="key + k + 'itemnode'"
                    :key="key + k + 'itemnode'"
                    :class="{'active':isFieldVisible(field.dbField, nodeFields) >= 0}"
                    @click.stop.prevent="toggleFieldVisibility(field.dbField, nodeFields)">
                    {{ field.friendlyName }}
                    <small>({{ field.exp }})</small>
                  </b-dropdown-item>
                  <b-tooltip v-if="field.help"
                    :key="key + k + 'tooltipnode'"
                    :target="key + k + 'itemnode'"
                    placement="left"
                    boundary="window">
                    {{ field.help }}
                  </b-tooltip>
                </template>
              </template>
            </b-dropdown> <!-- /node fields button -->

            <!-- link fields button -->
            <b-dropdown
              size="sm"
              no-flip
              no-caret
              toggle-class="rounded"
              class="field-vis-menu ml-1"
              variant="theme-primary"
              v-if="fields && groupedFields && linkFields">
              <template slot="button-content">
                <span class="fa fa-link"
                  v-b-tooltip.hover.bottom.d300="'Toggle visible fields in the link popups'">
                </span>
              </template>
              <b-dropdown-header>
                <input type="text"
                  v-model="fieldQuery"
                  class="form-control form-control-sm dropdown-typeahead"
                  placeholder="Search for fields..."
                />
              </b-dropdown-header>
              <b-dropdown-divider>
              </b-dropdown-divider>
              <b-dropdown-item
                @click.stop.prevent="resetLinkFieldsDefault">
                Reset to default
              </b-dropdown-item>
              <b-dropdown-divider>
              </b-dropdown-divider>
              <template v-for="(group, key) in filteredFields">
                <b-dropdown-header
                  :key="key"
                  v-if="group.length"
                  class="group-header">
                  {{ key }}
                </b-dropdown-header>
                <template v-for="(field, k) in group">
                  <b-dropdown-item
                    :id="key + k + 'itemlink'"
                    :key="key + k + 'itemlink'"
                    :class="{'active':isFieldVisible(field.dbField, linkFields) >= 0}"
                    @click.stop.prevent="toggleFieldVisibility(field.dbField, linkFields)">
                    {{ field.friendlyName }}
                    <small>({{ field.exp }})</small>
                  </b-dropdown-item>
                  <b-tooltip v-if="field.help"
                    :key="key + k + 'tooltiplink'"
                    :target="key + k + 'itemlink'"
                    placement="left"
                    boundary="window">
                    {{ field.help }}
                  </b-tooltip>
                </template>
              </template>
            </b-dropdown> <!-- /link fields button -->

            <!-- network baseline time range -->
            <div class="input-group input-group-sm ml-1">
              <div class="input-group-prepend help-cursor"
                v-b-tooltip.hover.bottom.d300="'Time range for baseline (preceding query time range)'">
                <span class="input-group-text">
                  Baseline
                </span>
              </div>
              <select class="form-control input-sm"
                v-model="query.baselineDate"
                @change="changeBaselineDate">
                <option value="0">disabled</option>
                <option value="1x">1 × query range</option>
                <option value="2x">2 × query range</option>
                <option value="4x">4 × query range</option>
                <option value="6x">6 × query range</option>
                <option value="8x">8 × query range</option>
                <option value="10x">10 × query range</option>
                <option value="1">1 hour</option>
                <option value="6">6 hours</option>
                <option value="24">24 hours</option>
                <option value="48">48 hours</option>
                <option value="72">72 hours</option>
                <option value="168">1 week</option>
                <option value="336">2 weeks</option>
                <option value="720">1 month</option>
                <option value="1440">2 months</option>
                <option value="4380">6 months</option>
                <option value="8760">1 year</option>
              </select>
            </div> <!-- /network baseline time range -->

            <!-- network baseline node visibility -->
            <div class="input-group input-group-sm ml-1"
              v-show="query.baselineDate !== '0'">
              <div class="input-group-prepend help-cursor"
                v-b-tooltip.hover.bottom.d300="'Toggle node visibility based on baseline result set membership'">
                <span class="input-group-text">
                  Baseline Visibility
                </span>
              </div>
              <select class="form-control input-sm"
                v-bind:disabled="query.baselineDate === '0'"
                v-model="query.baselineVis"
                @change="changeBaselineVis">
                <option value="all">All</option>
                <option value="actual">Actual</option>
                <option value="actualold">Baseline</option>
                <option value="new">New only</option>
                <option value="old">Baseline only</option>
              </select>
            </div> <!-- /network baseline node visibility -->

          </div>
        </form> <!-- /connections sub navbar -->
      </span>
    </ArkimeCollapsible>

    <div class="connections-content">

      <!-- loading overlay -->
      <arkime-loading
        :can-cancel="true"
        v-if="loading && !error"
        @cancel="cancelAndLoad">
      </arkime-loading> <!-- /loading overlay -->

      <!-- page error -->
      <arkime-error
        v-if="error"
        :message="error"
        class="mt-5">
      </arkime-error> <!-- /page error -->

      <!-- no results -->
      <arkime-no-results
        v-if="!error && !loading && recordsFiltered === 0"
        class="mt-5"
        :view="query.view">
      </arkime-no-results> <!-- /no results -->

      <!-- connections graph container -->
      <svg class="connections-graph"></svg>
      <!-- /connections graph container -->

      <!-- popup area -->
      <div ref="infoPopup"
        v-on-clickaway="closePopups">
        <div class="connections-popup">
        </div>
      </div> <!-- /popup area -->

      <!-- Button group -->
      <span class="connections-buttons"
        :style= "[showToolBars ? {'top': '160px'} : {'top': '40px'}]">
        <div class="btn-group-vertical unlock-btn overlay-btns">
          <!-- unlock button-->
          <span class="unlock-btn">
            <button class="btn btn-default btn-sm ml-1"
              v-b-tooltip.hover.lefttop="'Unlock any nodes that you have set into place'"
              @click.stop.prevent="unlock">
              <span class="fa fa-unlock"></span>
            </button>
          </span> <!-- /unlock button-->
          <!-- export button-->
          <span class="export-btn">
            <button class="btn btn-default btn-sm ml-1"
              v-b-tooltip.hover.lefttop="'Export this graph as a PNG'"
              @click.stop.prevent="exportPng">
              <span class="fa fa-download"></span>
            </button>
          </span> <!-- /export button-->
        </div>

        <!-- node distance -->
        <div class="btn-group-vertical node-distance-btns overlay-btns">
          <span v-b-tooltip.hover.lefttop="'Increase node distance'">
            <button type="button"
              class="btn btn-default btn-sm"
              :class="{'disabled':query.nodeDist >= 200}"
              @click="changeNodeDist(10)">
              <span class="fa fa-plus">
              </span>
              <span class="fa fa-arrows-v">
              </span>
            </button>
          </span>
          <span v-b-tooltip.hover.lefttop="'Decrease node distance'">
            <button type="button"
              class="btn btn-default btn-sm"
              :class="{'disabled':query.nodeDist <= 10}"
              @click="changeNodeDist(-10)">
              <span class="fa fa-minus">
              </span>
              <span class="fa fa-arrows-v">
              </span>
            </button>
          </span>
        </div> <!-- /node distance -->

        <!-- text size increase/decrease -->
        <div class="btn-group-vertical text-size-btns overlay-btns">
          <span v-b-tooltip.hover.lefttop="'Increase text size (you might also want to update the node distance using the buttons just to the left)'">
            <button type="button"
              class="btn btn-default btn-sm"
              :class="{'disabled':fontSize >= 1}"
              @click="updateTextSize(0.1)">
              <span class="fa fa-long-arrow-up">
              </span>
              <span class="fa fa-font">
              </span>
            </button>
          </span>
          <span v-b-tooltip.hover.lefttop="'Decrease text size (you might also want to update the node distance using the buttons just to the left)'">
            <button type="button"
              class="btn btn-default btn-sm"
              :class="{'disabled':fontSize <= 0.2}"
              @click="updateTextSize(-0.1)">
              <span class="fa fa-long-arrow-down">
              </span>
              <span class="fa fa-font">
              </span>
            </button>
          </span>
        </div> <!-- /text size increase/decrease -->

        <!-- zoom in/out -->
        <div class="btn-group-vertical zoom-btns overlay-btns">
          <span v-b-tooltip.hover.lefttop="'Zoom in'">
            <button type="button"
              class="btn btn-default btn-sm"
              :class="{'disabled':zoomLevel >= 4}"
              @click="zoomConnections(2)">
              <span class="fa fa-lg fa-search-plus">
              </span>
            </button>
          </span>
          <span v-b-tooltip.hover.lefttop="'Zoom out'">
            <button type="button"
              class="btn btn-default btn-sm"
              :class="{'disabled':zoomLevel <= 0.0625}"
              @click="zoomConnections(0.5)">
              <span class="fa fa-lg fa-search-minus">
              </span>
            </button>
          </span>
        </div> <!-- /zoom in/out -->
      </span> <!-- /Button group -->
    </div>

  </div>

</template>

<script>
// import components
import ArkimeSearch from '../search/Search';
import ArkimeError from '../utils/Error';
import ArkimeLoading from '../utils/Loading';
import ArkimeNoResults from '../utils/NoResults';
import ArkimeCollapsible from '../utils/CollapsibleWrapper';
// import services
import ArkimeFieldTypeahead from '../utils/FieldTypeahead';
import FieldService from '../search/FieldService';
import UserService from '../users/UserService';
import ConnectionsService from './ConnectionsService';
import ConfigService from '../utils/ConfigService';
// import external
import Vue from 'vue';
import { mixin as clickaway } from 'vue-clickaway';
// import utils
import Utils from '../utils/utils';
// lazy import these
let d3, saveSvgAsPng;

// d3 force directed graph vars/functions ---------------------------------- */
let nodeFillColors;
let simulation, svg, container, zoom;
let node, nodes, link, links, nodeLabel;
let popupTimer, popupVue;
let draggingNode;
let nodeMax = 1;
let nodeMin = 1;
let linkMax = 1;
let linkMin = 1;
let linkScaleFactor = 0;
let nodeScaleFactor = 0;
const maxLog = Math.ceil(Math.pow(Math.E, 9));
/* eslint-disable no-useless-escape */
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

// simulation resize helper
function resize (toolbarDown = true) {
  const width = $(window).width() - 10;
  // 36px for navbar + 25px for footer = 61px.
  const height = $(window).height() - (toolbarDown ? 171 : 61);

  // set the width and height of the canvas
  svg.attr('width', width).attr('height', height);
}

// close popups helpers
function closePopups () {
  if (popupVue) { popupVue.$destroy(); }
  popupVue = undefined;
  $('.connections-popup').hide();
}

// close popup on escape press
function closePopupsOnEsc (keyCode) {
  if (event.keyCode === 27) { // esc
    closePopups();
  }
}

// other necessary vars ---------------------------------------------------- */
// default fields to display in the node/link popups
const defaultLinkFields = ['network.bytes', 'totDataBytes', 'network.packets', 'node'];
const defaultNodeFields = ['network.bytes', 'totDataBytes', 'network.packets', 'node'];

// vue definition ---------------------------------------------------------- */
export default {
  name: 'Connections',
  mixins: [clickaway],
  components: {
    ArkimeSearch,
    ArkimeError,
    ArkimeLoading,
    ArkimeNoResults,
    ArkimeCollapsible,
    ArkimeFieldTypeahead
  },
  data: function () {
    return {
      error: '',
      loading: true,
      settings: {}, // user settings
      recordsFiltered: 0,
      fieldsMap: {},
      fieldQuery: '',
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
      closePopups,
      fontSize: 0.4,
      zoomLevel: 1,
      weight: 'sessions',
      fieldHistoryConnectionsSrc: undefined,
      fieldHistoryConnectionsDst: undefined
    };
  },
  computed: {
    query: function () {
      return {
        start: 0, // first item index
        length: this.$route.query.length || 100, // page length
        date: this.$store.state.timeRange,
        startTime: this.$store.state.time.startTime,
        stopTime: this.$store.state.time.stopTime,
        srcField: this.$route.query.srcField || this.$store.state.user.settings.connSrcField || 'source.ip',
        dstField: this.$route.query.dstField || this.$store.state.user.settings.connDstField || 'destination.ip',
        bounding: this.$route.query.bounding || 'last',
        interval: this.$route.query.interval || 'auto',
        minConn: this.$route.query.minConn || 1,
        baselineDate: this.$route.query.baselineDate || '0',
        baselineVis: this.$route.query.baselineVis || 'all',
        nodeDist: this.$route.query.nodeDist || 40,
        view: this.$route.query.view || undefined,
        expression: this.$store.state.expression || undefined,
        cluster: this.$route.query.cluster || undefined
      };
    },
    user: function () {
      return this.$store.state.user;
    },
    // Boolean in the store will remember chosen toggle state for all pages
    showToolBars: function () {
      return this.$store.state.showToolBars;
    },
    filteredFields: function () {
      const filteredGroupedFields = {};

      for (const group in this.groupedFields) {
        filteredGroupedFields[group] = this.$options.filters.searchFields(
          this.fieldQuery,
          this.groupedFields[group]
        );
      }

      return filteredGroupedFields;
    },
    nodeFields: {
      get: function () {
        return this.$store.state.user.settings.connNodeFields || defaultNodeFields;
      },
      set: function (newValue) {
        const settings = this.$store.state.user.settings;
        settings.connNodeFields = newValue;
        this.$store.commit('setUserSettings', settings);
      }
    },
    linkFields: {
      get: function () {
        return this.$store.state.user.settings.connLinkFields || defaultLinkFields;
      },
      set: function (newValue) {
        const settings = this.$store.state.user.settings;
        settings.connLinkFields = newValue;
        this.$store.commit('setUserSettings', settings);
      }
    },
    fields () {
      return FieldService.addIpDstPortField(this.$store.state.fieldsArr);
    }
  },
  watch: {
    '$route.query.length': function (newVal, oldVal) {
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
    },

    // Resize svg height after toggle is updated and mounted()
    showToolBars: function () {
      this.$nextTick(() => {
        resize(this.showToolBars);
      });
    }
  },
  mounted: function () {
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
    window.addEventListener('keyup', closePopupsOnEsc);
    // resize the simulation with the window
    window.addEventListener('resize', resize);
  },
  methods: {
    /* exposed page functions ---------------------------------------------- */
    /**
     * Cancels the pending session query (if it's still pending) and runs a new
     * query if requested
     * @param {bool} runNewQuery  Whether to run a new connections query after
     *                            canceling the request
     */
    cancelAndLoad: function (runNewQuery) {
      const clientCancel = () => {
        if (pendingPromise) {
          pendingPromise.source.cancel();
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
    changeLength: function () {
      this.$router.push({
        query: {
          ...this.$route.query,
          length: this.query.length
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
    changeMinConn: function () {
      this.$router.push({
        query: {
          ...this.$route.query,
          minConn: this.query.minConn
        }
      });
    },
    changeWeight: function () {
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
    resetNodeFieldsDefault: function () {
      this.nodeFields = defaultNodeFields;
      this.closePopups();
      this.cancelAndLoad(true);
      this.saveVisibleFields();
    },
    resetLinkFieldsDefault: function () {
      this.linkFields = defaultLinkFields;
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

      import(
        /* webpackChunkName: "saveSvgAsPng" */ 'save-svg-as-png'
      ).then((saveSvgAsPngModule) => {
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
                // saveSvgAsPng cannot resolve var(--color-foreground)
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
    /* helper functions ---------------------------------------------------- */
    loadData: function () {
      if (!Utils.checkClusterSelection(this.query.cluster, this.$store.state.esCluster.availableCluster.active, this).valid) {
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

      // create unique cancel id to make canel req for corresponding es task
      const cancelId = Utils.createRandomString();
      this.query.cancelId = cancelId;

      const source = Vue.axios.CancelToken.source();
      const cancellablePromise = ConnectionsService.get(this.query, source.token);

      // set pending promise info so it can be cancelled
      pendingPromise = { cancellablePromise, source, cancelId };

      cancellablePromise.then((response) => {
        pendingPromise = null;
        this.error = '';
        this.loading = false;
        this.recordsFiltered = response.data.recordsFiltered;
        this.drawGraphWrapper(response.data);
      }).catch((error) => {
        pendingPromise = null;
        this.loading = false;
        this.error = error.text || error;
      });
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
      import(/* webpackChunkName: "d3" */ 'd3').then((d3Module) => {
        d3 = d3Module;
        this.drawGraph(data);
      });
    },
    drawGraph: function (data) {
      if (!nodeFillColors) {
        const styles = window.getComputedStyle(document.body);
        this.backgroundColor = styles.getPropertyValue('--color-background').trim() || '#FFFFFF';
        this.foregroundColor = styles.getPropertyValue('--color-foreground').trim() || '#212529';
        this.primaryColor = styles.getPropertyValue('--color-primary').trim();
        this.secondaryColor = styles.getPropertyValue('--color-tertiary').trim();
        this.tertiaryColor = styles.getPropertyValue('--color-quaternary').trim();
        this.highlightPrimaryColor = styles.getPropertyValue('--color-primary-lighter').trim();
        this.highlightSecondaryColor = styles.getPropertyValue('--color-secondary-lighter').trim();
        this.highlightTertiaryColor = styles.getPropertyValue('--color-tertiary-lighter').trim();
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
            dataNode.id = this.$options.filters.timezoneDateString(
              dataNode.id,
              this.settings.timezone ||
                this.$store.state.user.settings.timezone,
              this.settings.ms ||
                this.$store.state.user.settings.ms
            );
          }
        }
      }

      // map which nodes are linked (for highlighting)
      linkedByIndex = {};
      data.links.forEach((d) => {
        linkedByIndex[d.source + ',' + d.target] = true;
      });

      // calculate the width and height of the canvas
      const width = $(window).width() - 10;
      // 36px for navbar + 25px for footer = 61px.
      const height = $(window).height() - (this.toolbarDown ? 171 : 61);

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
        // set the width and height of the canvas
        svg = d3.select('svg')
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
          val = ' 🚫';
          break;
        case 1:
          // "new" (in actual, not in baseline result set)
          val = ' ✨';
          break;
        }
      }
      return val;
    },
    calculateNodeBaselineVisibility: function (n) {
      let val = 'visible';

      if (this.query.baselineDate !== '0') {
        const inActualSet = ((n.inresult & 0x1) !== 0);
        const inBaselineSet = ((n.inresult & 0x2) !== 0);
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
    showNodePopup: function (dataNode) {
      if (dataNode.type === 2) {
        dataNode.dbField = FieldService.getFieldProperty(this.query.dstField, 'dbField');
        dataNode.exp = FieldService.getFieldProperty(this.query.dstField, 'exp');
      } else {
        dataNode.dbField = FieldService.getFieldProperty(this.query.srcField, 'dbField');
        dataNode.exp = FieldService.getFieldProperty(this.query.srcField, 'exp');
      }

      closePopups();
      if (!popupVue) {
        popupVue = new Vue({
          template: `
            <div class="connections-popup">
              <div class="mb-2 mt-2">
                <strong>
                  <arkime-session-field
                    :value="dataNode.id"
                    :session="dataNode"
                    :expr="dataNode.exp"
                    :field="fields[dataNode.dbField]"
                    :pull-left="true">
                  </arkime-session-field>
                </strong>
                <a class="pull-right cursor-pointer no-decoration"
                  @click="closePopup">
                  <span class="fa fa-close"></span>
                </a>
              </div>

              <dl class="dl-horizontal">
                <dt>Type</dt>
                <dd>{{['','Source','Target','Both'][dataNode.type]}}</dd>
                <dt>Links</dt>
                <dd>{{dataNode.weight || dataNode.cnt}}&nbsp;</dd>
                <dt>Sessions</dt>
                <dd>{{dataNode.sessions}}&nbsp;</dd>

                <span v-for="field in nodeFields"
                  :key="field">
                  <template v-if="fields[field]">
                    <dt>
                      {{ fields[field].friendlyName }}
                    </dt>
                    <dd>
                      <span v-if="!Array.isArray(dataNode[field])">
                        <arkime-session-field
                          :value="dataNode[field]"
                          :session="dataNode"
                          :expr="fields[field].exp"
                          :field="fields[field]"
                          :pull-left="true">
                        </arkime-session-field>
                      </span>
                      <span v-else
                        v-for="value in dataNode[field]">
                        <arkime-session-field
                          :value="value"
                          :session="dataNode"
                          :expr="fields[field].exp"
                          :field="fields[field]"
                          :pull-left="true">
                        </arkime-session-field>
                      </span>&nbsp;
                    </dd>
                    </template>
                </span>

                <div v-if="baselineDate !== '0'">
                  <dt>Result Set</dt>
                  <dd>{{['','✨Actual','🚫 Baseline','Both'][dataNode.inresult]}}</dd>
                </div>
              </dl>

              <a class="cursor-pointer no-decoration"
                href="javascript:void(0)"
                @click.stop.prevent="hideNode">
                <span class="fa fa-eye-slash">
                </span>&nbsp;
                Hide Node
              </a>
            </div>
          `,
          parent: this,
          data: {
            dataNode,
            nodeFields: this.nodeFields,
            fields: this.fieldsMap,
            baselineDate: this.query.baselineDate
          },
          methods: {
            hideNode: function () {
              this.$parent.closePopups();
              const id = '#id' + dataNode.id.replace(idRegex, '_');
              svg.select(id).remove();
              svg.select(id + '-label').remove();
              svg.selectAll('.link')
                .filter(function (d, i) {
                  return d.source.id === dataNode.id || d.target.id === dataNode.id;
                })
                .remove();
            },
            addExpression: function (op) {
              const fullExpression = `${this.dataNode.exp} == ${this.dataNode.id}`;
              this.$store.commit('addToExpression', { expression: fullExpression, op });
            },
            closePopup: function () {
              this.$parent.closePopups();
            }
          }
        }).$mount($(this.$refs.infoPopup)[0].firstChild);
      }

      popupVue.dataNode = dataNode;

      $('.connections-popup').show();
    },
    showLinkPopup: function (linkData) {
      linkData.dstDbField = FieldService.getFieldProperty(this.query.dstField, 'dbField');
      linkData.srcDbField = FieldService.getFieldProperty(this.query.srcField, 'dbField');
      linkData.dstExp = FieldService.getFieldProperty(this.query.dstField, 'exp');
      linkData.srcExp = FieldService.getFieldProperty(this.query.srcField, 'exp');

      closePopups();
      if (!popupVue) {
        popupVue = new Vue({
          template: `
            <div class="connections-popup">
              <div class="mb-2 mt-2">
                <strong>Link</strong>
                <a class="pull-right cursor-pointer no-decoration"
                   @click="closePopup">
                  <span class="fa fa-close"></span>
                </a>
              </div>
              <div>
                <arkime-session-field
                  :value="linkData.source.id"
                  :session="linkData"
                  :expr="linkData.srcExp"
                  :field="fields[linkData.srcDbField]"
                  :pull-left="true">
                </arkime-session-field>
              </div>
              <div class="mb-2">
                <arkime-session-field
                  :value="linkData.target.id"
                  :session="linkData"
                  :expr="linkData.dstExp"
                  :field="fields[linkData.dstDbField]"
                  :pull-left="true">
                </arkime-session-field>
              </div>

              <dl class="dl-horizontal">
                <dt>Sessions</dt>
                <dd>{{linkData.value}}&nbsp;</dd>

                <span v-for="field in linkFields"
                  :key="field">
                  <template v-if="fields[field]">
                    <dt>
                      {{ fields[field].friendlyName }}
                    </dt>
                    <dd>
                      <span v-if="!Array.isArray(linkData[field])">
                        <arkime-session-field
                          :value="linkData[field]"
                          :session="linkData"
                          :expr="fields[field].exp"
                          :field="fields[field]"
                          :pull-left="true">
                        </arkime-session-field>
                      </span>
                      <span v-else
                        v-for="value in linkData[field]">
                        <arkime-session-field
                          :value="value"
                          :session="linkData"
                          :expr="fields[field].exp"
                          :field="fields[field]"
                          :pull-left="true">
                        </arkime-session-field>
                      </span>&nbsp;
                    </dd>
                  </template>
                </span>
              </dl>

              <a class="cursor-pointer no-decoration"
                href="javascript:void(0)"
                @click="hideLink">
                <span class="fa fa-eye-slash"></span>&nbsp;
                Hide Link
              </a>

            </div>
          `,
          parent: this,
          data: {
            linkData,
            linkFields: this.linkFields,
            fields: this.fieldsMap
          },
          methods: {
            hideLink: function () {
              this.$parent.closePopups();
              svg.selectAll('.link')
                .filter((d, i) => {
                  return d.source.id === linkData.source.id && d.target.id === linkData.target.id;
                })
                .remove();
            },
            addExpression: function (op) {
              const fullExpression = `(${linkData.srcExp} == ${linkData.source.id} && ${linkData.dstExp} == ${linkData.target.id})`;
              this.$store.commit('addToExpression', { expression: fullExpression, op });
            },
            closePopup: function () {
              this.$parent.closePopups();
            }
          }
        }).$mount($(this.$refs.infoPopup)[0].firstChild);
      }

      popupVue.linkData = linkData;

      $('.connections-popup').show();
    }
  },
  beforeDestroy: function () {
    if (pendingPromise) {
      pendingPromise.source.cancel();
      pendingPromise = null;
    }

    // remove listeners
    window.removeEventListener('resize', resize);
    window.removeEventListener('keyup', closePopupsOnEsc);
    // d3 doesn't have .off function to remove listeners,
    // so use .on('listener', null)
    d3.zoom().on('zoom', null);
    if (simulation) { simulation.on('tick', null); }
    d3.drag()
      .on('start', null)
      .on('drag', null)
      .on('end', null);

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

    // destroy child component
    $('.connections-popup').remove();
    if (popupVue) { popupVue.$destroy(); }

    setTimeout(() => {
      // clean up global vars
      svg = undefined;
      zoom = undefined;
      node = undefined;
      link = undefined;
      nodeFillColors = undefined;
      popupVue = undefined;
      container = undefined;
      nodeLabel = undefined;
      popupTimer = undefined;
      simulation = undefined;
      draggingNode = undefined;
    });
  }
};
</script>

<style>
.connections-page text {
  fill: var(--color-foreground, #333);
}
</style>

<style scoped>
.connections-graph {
  /* don't allow selecting text */
  -webkit-user-select: none;
  -moz-user-select: none;
  user-select: none;
}

/* position the subnavbar */
.connections-page form.connections-form {
  z-index: 4;
  background-color: var(--color-quaternary-lightest);

  -webkit-box-shadow: 0 0 16px -2px black;
     -moz-box-shadow: 0 0 16px -2px black;
          box-shadow: 0 0 16px -2px black;
}

/* remove select box styles */
.connections-page form.connections-form select {
  -webkit-appearance: none;
}

/* make the color for legend areas white */
.connections-page form.connections-form .input-group-prepend.legend > .input-group-text {
  font-weight: 700;
  color: var(--color-button, #FFF) !important;
}
.connections-page form.connections-form .input-group-prepend.legend > .primary-legend {
  background-color: var(--color-primary) !important;
}
.connections-page form.connections-form .input-group-prepend.legend > .tertiary-legend {
  background-color: var(--color-tertiary) !important;
}
.connections-page form.connections-form .input-group-prepend.legend > .secondary-legend {
  border-radius: 4px;
  background-color: var(--color-secondary) !important;
}

/* apply foreground theme color */
.connections-page svg {
  fill: var(--color-foreground, #333);
}

/* buttons overlaying the graph */
.connections-content .overlay-btns > span:first-child > button {
  border-bottom: none;
  border-radius: 4px 4px 0 0;
}
.connections-content .overlay-btns > span:last-child > button {
  border-radius: 0 0 4px 4px;
}
.connections-buttons {
  position: fixed;
  right: 10px;
}
</style>

<style>
/* this needs to not be scoped because it's a child component */
/* node/link data popup */
.connections-page div.connections-popup {
  position: absolute;
  left: 0;
  top: 148px;
  bottom: 24px;
  display: none;
  font-size: smaller;
  padding: 4px 8px;
  max-width: 400px;
  min-width: 280px;
  border: solid 1px var(--color-gray);
  background: var(--color-primary-lightest);
  white-space: nowrap;
  overflow-x: visible;
  overflow-y: auto;
  text-overflow: ellipsis;
}
.connections-page div.connections-popup .dl-horizontal {
  margin-bottom: var(--px-md) !important;
}
.connections-page div.connections-popup .dl-horizontal dt {
  width: 100px !important;
  text-align: left;
}
.connections-page div.connections-popup .dl-horizontal dd {
  margin-left: 105px !important;
  white-space: normal;
}

.field-vis-menu > button.btn {
  border-top-right-radius: 4px !important;
  border-bottom-right-radius: 4px !important;
}
.field-vis-menu .dropdown-menu input {
  width: 100%;
}
.field-vis-menu .dropdown-menu {
  max-height: 300px;
  overflow: auto;
}
.field-vis-menu .dropdown-header {
  padding: .25rem .5rem 0;
}
.field-vis-menu .dropdown-header.group-header {
  text-transform: uppercase;
  margin-top: 8px;
  padding: .2rem;
  font-size: 120%;
  font-weight: bold;
}
</style>
