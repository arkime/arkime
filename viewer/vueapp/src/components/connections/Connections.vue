<template>

  <div class="connections-page">

    <!-- search navbar -->
    <moloch-search
      :start="query.start"
      :timezone="user.settings.timezone"
      @changeSearch="cancelAndLoad(true)">
    </moloch-search> <!-- /search navbar -->

    <!-- connections sub navbar -->
    <form class="connections-form">
      <div class="form-inline pr-1 pl-1 pt-1 pb-1">

        <!-- query size select -->
        <div class="input-group input-group-sm">
          <div class="input-group-prepend help-cursor"
            v-b-tooltip.hover
            title="Query Size">
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
          v-if="fields && fields.length && srcFieldTypeahead">
          <div class="input-group input-group-sm">
            <span class="input-group-prepend legend cursor-help"
              v-b-tooltip.hover
              title="Select a field for the source nodes">
              <span class="input-group-text"
                :style="{'background-color': primaryColor + '!important'}">
                Src:
              </span>
            </span>
            <moloch-field-typeahead
              :fields="fields"
              query-param="srcField"
              :initial-value="srcFieldTypeahead"
              @fieldSelected="changeSrcField"
              page="ConnectionsSrc">
            </moloch-field-typeahead>
          </div>
        </div> <!-- /src select -->

        <!-- dst select -->
        <div class="form-group ml-1"
          v-if="fields && dstFieldTypeahead">
          <div class="input-group input-group-sm">
            <span class="input-group-prepend legend cursor-help"
              v-b-tooltip.hover
              title="Select a field for the destination nodes">
              <span class="input-group-text"
                :style="{'background-color': tertiaryColor + '!important'}">
                Dst:
              </span>
            </span>
            <moloch-field-typeahead
              :fields="fields"
              query-param="dstField"
              :initial-value="dstFieldTypeahead"
              @fieldSelected="changeDstField"
              page="ConnectionsDst">
            </moloch-field-typeahead>
          </div>
        </div> <!-- /dst select -->

        <!-- src & dst color -->
        <div class="form-group ml-1">
          <div class="input-group input-group-sm">
            <span class="input-group-prepend legend cursor-help"
              v-b-tooltip.hover
              title="This is the color of a node that is both a source and destination node">
              <span class="input-group-text"
                style="border-radius: 4px"
                :style="{'background-color': secondaryColor + '!important'}">
                Src & Dst
              </span>
            </span>
          </div>
        </div> <!-- /src & dst color -->

        <!-- min connections select -->
        <div class="input-group input-group-sm ml-1">
          <div class="input-group-prepend help-cursor"
            v-b-tooltip.hover
            title="Minimum number of sessions between nodes">
            <span class="input-group-text">
              Min. Connections
            </span>
          </div>
          <select class="form-control input-sm"
            v-model="query.minConn"
            @change="changeMinConn">
            <option value="1">1</option>
            <option value="2">2</option>
            <option value="3">3</option>
            <option value="4">4</option>
            <option value="5">5</option>
          </select>
        </div> <!-- /min connections select -->

        <!-- weight select -->
        <div class="input-group input-group-sm ml-1">
          <div class="input-group-prepend help-cursor"
            v-b-tooltip.hover
            title="Change the field that calculates the radius of nodes and the width links">
            <span class="input-group-text">
              Node/Link Weight
            </span>
          </div>
          <select class="form-control input-sm"
            v-model="weight"
            @change="changeWeight">
            <option value="sessions">Sessions</option>
            <option value="totPackets">Packets</option>
            <option value="totBytes">Total Raw Bytes</option>
            <option value="totDataBytes">Total Data Bytes</option>
            <option value="">None</option>
          </select>
        </div> <!-- /weight select -->

        <!-- unlock button-->
        <button class="btn btn-default btn-sm ml-1"
          v-b-tooltip.hover
          title="Unlock any nodes that you have set into place"
          @click.stop.prevent="unlock">
          <span class="fa fa-unlock"></span>&nbsp;
          Unlock
        </button> <!-- /unlock button-->

        <!-- export button-->
        <button class="btn btn-default btn-sm ml-1"
          v-b-tooltip.hover
          title="Export this graph as a png"
          @click.stop.prevent="exportPng">
          <span class="fa fa-download"></span>&nbsp;
          Export
        </button> <!-- /export button-->

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
              v-b-tooltip.hover
              title="Toggle visible fields in the node popups">
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
              v-b-tooltip.hover
              title="Toggle visible fields in the link popups">
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

      </div>
    </form> <!-- /connections sub navbar -->

    <div class="connections-content">

      <!-- loading overlay -->
      <moloch-loading
        :can-cancel="true"
        v-if="loading && !error"
        @cancel="cancelAndLoad">
      </moloch-loading> <!-- /loading overlay -->

      <!-- page error -->
      <moloch-error
        v-if="error"
        :message="error"
        class="mt-5">
      </moloch-error> <!-- /page error -->

      <!-- no results -->
      <moloch-no-results
        v-if="!error && !loading && recordsFiltered === 0"
        class="mt-5"
        :view="query.view">
      </moloch-no-results> <!-- /no results -->

      <!-- connections graph container -->
      <svg class="connections-graph"></svg>
      <!-- /connections graph container -->

      <!-- popup area -->
      <div ref="infoPopup"
        v-on-clickaway="closePopups">
        <div class="connections-popup">
        </div>
      </div> <!-- /popup area -->

      <!-- zoom in/out -->
      <div class="btn-group-vertical zoom-btns overlay-btns">
        <span title="Zoom in"
          v-b-tooltip.hover.lefttop>
          <button type="button"
            class="btn btn-default btn-sm"
            :class="{'disabled':zoomLevel >= 4}"
            @click="zoomConnections(2)">
            <span class="fa fa-lg fa-search-plus">
            </span>
          </button>
        </span>
        <span title="Zoom out"
          v-b-tooltip.hover.lefttop>
          <button type="button"
            class="btn btn-default btn-sm"
            :class="{'disabled':zoomLevel <= 0.0625}"
            @click="zoomConnections(0.5)">
            <span class="fa fa-lg fa-search-minus">
            </span>
          </button>
        </span>
      </div> <!-- /zoom in/out -->

      <!-- text size increase/decrease -->
      <div class="btn-group-vertical text-size-btns overlay-btns">
        <span v-b-tooltip.hover.lefttop
          title="Increase text size (you might also want to update the node distance using the buttons just to the left)">
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
        <span v-b-tooltip.hover.lefttop
          title="Decrease text size (you might also want to update the node distance using the buttons just to the left)">
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

      <!-- node distance -->
      <div class="btn-group-vertical node-distance-btns overlay-btns">
        <span title="Increase node distance"
          v-b-tooltip.hover.lefttop>
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
        <span title="Decrease node distance"
          v-b-tooltip.hover.lefttop>
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

    </div>

  </div>

</template>

<script>
// import components
import MolochSearch from '../search/Search';
import MolochError from '../utils/Error';
import MolochLoading from '../utils/Loading';
import MolochNoResults from '../utils/NoResults';
// import services
import MolochFieldTypeahead from '../utils/FieldTypeahead';
import FieldService from '../search/FieldService';
import UserService from '../users/UserService';
import ConnectionsService from './ConnectionsService';
import ConfigService from '../utils/ConfigService';
// import external
import Vue from 'vue';
import * as d3 from 'd3';
import saveSvgAsPng from 'save-svg-as-png';
import { mixin as clickaway } from 'vue-clickaway';
// import utils
import Utils from '../utils/utils';

// d3 force directed graph vars/functions ---------------------------------- */
let colors, foregroundColor;
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
let maxLog = Math.ceil(Math.pow(Math.E, 9));
/* eslint-disable no-useless-escape */
let idRegex = /[\[\]:. ]/g;
let pendingPromise; // save a pending promise to be able to cancel it

// drag helpers
function dragstarted (d) {
  d3.event.sourceEvent.stopPropagation();
  if (!d3.event.active) { simulation.alphaTarget(0.3).restart(); }
  draggingNode = d;
  d.fx = d.x;
  d.fy = d.y;
  focus(d);
}

function dragged (d) {
  d.fx = d3.event.x;
  d.fy = d3.event.y;
}

function dragended (d) {
  if (!d3.event.active) { simulation.alphaTarget(0); }
  draggingNode = undefined;
  // keep the node where it was dragged
  d.fx = d.x;
  d.fy = d.y;
  focus(d);
}

// highlighting helpers
let linkedByIndex = {};
function isConnected (a, b) {
  return linkedByIndex[a.index + ',' + b.index] ||
    linkedByIndex[b.index + ',' + a.index] ||
    a.index === b.index;
}

function focus (d) {
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
function resize () {
  const width = $(window).width() - 10;
  const height = $(window).height() - 171;

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
const defaultLinkFields = [ 'totBytes', 'totDataBytes', 'totPackets', 'node' ];
const defaultNodeFields = [ 'totBytes', 'totDataBytes', 'totPackets', 'node' ];

// vue definition ---------------------------------------------------------- */
export default {
  name: 'Connections',
  mixins: [ clickaway ],
  components: {
    MolochSearch,
    MolochError,
    MolochLoading,
    MolochNoResults,
    MolochFieldTypeahead
  },
  data: function () {
    return {
      error: '',
      loading: true,
      settings: {}, // user settings
      recordsFiltered: 0,
      fields: [],
      fieldsMap: {},
      fieldQuery: '',
      srcFieldTypeahead: undefined,
      dstFieldTypeahead: undefined,
      groupedFields: undefined,
      primaryColor: undefined,
      secondaryColor: undefined,
      tertiaryColor: undefined,
      closePopups: closePopups,
      fontSize: 0.4,
      zoomLevel: 1,
      weight: 'sessions'
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
        srcField: this.$route.query.srcField || 'srcIp',
        dstField: this.$route.query.dstField || 'dstIp',
        bounding: this.$route.query.bounding || 'last',
        interval: this.$route.query.interval || 'auto',
        minConn: this.$route.query.minConn || 1,
        nodeDist: this.$route.query.nodeDist || 40,
        view: this.$route.query.view || undefined,
        expression: this.$store.state.expression || undefined
      };
    },
    user: function () {
      return this.$store.state.user;
    },
    filteredFields: function () {
      let filteredGroupedFields = {};

      for (let group in this.groupedFields) {
        filteredGroupedFields[group] = this.$options.filters.searchFields(
          this.fieldQuery,
          this.groupedFields[group]
        );
      }

      return filteredGroupedFields;
    },
    nodeFields: function () {
      return this.$store.state.user.settings.connNodeFields || defaultNodeFields;
    },
    linkFields: function () {
      return this.$store.state.user.settings.connLinkFields || defaultLinkFields;
    }
  },
  watch: {
    '$route.query.length': function (newVal, oldVal) {
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
  mounted: function () {
    let styles = window.getComputedStyle(document.body);
    this.primaryColor = styles.getPropertyValue('--color-primary').trim();
    this.secondaryColor = styles.getPropertyValue('--color-tertiary').trim();
    this.tertiaryColor = styles.getPropertyValue('--color-quaternary').trim();
    foregroundColor = styles.getPropertyValue('--color-foreground').trim() || '#212529';
    colors = ['', this.primaryColor, this.tertiaryColor, this.secondaryColor];

    this.cancelAndLoad(true);

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
      if (pendingPromise) {
        ConfigService.cancelEsTask(pendingPromise.cancelId)
          .then((response) => {
            pendingPromise.source.cancel();
            pendingPromise = null;

            if (!runNewQuery) {
              this.loading = false;
              if (!this.fields.length) {
                // show a page error if there is no data on the page
                this.error = 'You canceled the search';
              }
              return;
            }

            this.loadData();
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
      this.query.nodeDist = direction > 0 ? Math.min(this.query.nodeDist + direction, 200)
        : Math.max(this.query.nodeDist + direction, 10);

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
    toggleFieldVisibility: function (id, list) {
      let index = this.isFieldVisible(id, list);

      if (index >= 0) { // it's visible
        // remove it from the visible node fields list
        list.splice(index, 1);
      } else { // it's hidden
        // add it to the visible headers list
        list.push(id);
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
      this.zoomLevel = direction > 0.5 ? Math.min(this.zoomLevel * direction, 4)
        : Math.max(this.zoomLevel * direction, 0.0625);

      svg.transition().duration(500).call(zoom.scaleBy, direction);
    },
    exportPng: function () {
      saveSvgAsPng.saveSvgAsPng(
        document.getElementById('graphSvg'),
        'connections.png',
        { backgroundColor: '#FFFFFF' }
      );
    },
    updateTextSize: function (direction) {
      this.fontSize = direction > 0 ? Math.min(this.fontSize + direction, 1)
        : Math.max(this.fontSize + direction, 0.2);

      svg.selectAll('.node-label')
        .style('font-size', this.fontSize + 'em');
    },
    /* helper functions ---------------------------------------------------- */
    loadData: function () {
      this.error = '';
      this.loading = true;

      if (!this.$route.query.srcField) {
        this.query.srcField = this.user.settings.connSrcField;
      }
      if (!this.$route.query.dstField) {
        this.query.dstField = this.user.settings.connDstField;
      }

      // send the requested fields with the query
      let fields = this.nodeFields;
      // combine fields from nodes and links
      for (let f in this.linkFields) {
        let id = this.linkFields[f];
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
        // IMPORTANT: this kicks off drawing the connections graph
        this.getFields(response.data);
        this.recordsFiltered = response.data.recordsFiltered;
      }).catch((error) => {
        pendingPromise = null;
        this.loading = false;
        this.error = error.text || error;
      });
    },
    getFields: function (connectionsData) {
      FieldService.get(true)
        .then((result) => {
          this.fields = result;

          this.setupFields();

          for (let field of this.fields) {
            if (field.dbField === this.query.srcField) {
              this.srcFieldTypeahead = field.friendlyName;
            }
            if (field.dbField === this.query.dstField) {
              this.dstFieldTypeahead = field.friendlyName;
            }
          }

          this.drawGraph(connectionsData);
        }).catch((error) => {
          this.error = error.text || error;
        });
    },
    setupFields: function () {
      // group fields map by field group
      // and remove duplicate fields (e.g. 'host.dns' & 'dns.host')
      let existingFieldsLookup = {}; // lookup map of fields in fieldsArray
      this.groupedFields = {};
      let ipDstPortFieldExists = false;
      for (let field of this.fields) {
        // found the ip.dst:port field
        if (field.dbField === 'ip.dst:port') { ipDstPortFieldExists = true; }
        // don't include fields with regex
        if (field.hasOwnProperty('regex')) { continue; }
        if (!existingFieldsLookup.hasOwnProperty(field.exp)) {
          this.fieldsMap[field.dbField] = field;
          existingFieldsLookup[field.exp] = field;
          if (!this.groupedFields[field.group]) {
            this.groupedFields[field.group] = [];
          }
          this.groupedFields[field.group].push(field);
        }
      }

      // only add this field if it doesn't already exist
      if (!ipDstPortFieldExists) {
        const ipDstPortField = {
          dbField: 'ip.dst:port',
          exp: 'ip.dst:port',
          help: 'Destination IP:Destination Port',
          group: 'general',
          friendlyName: 'Dst IP:Dst Port'
        };
        this.fields.push(ipDstPortField);
        this.groupedFields.general.push(ipDstPortField);
      }
    },
    saveVisibleFields: function () {
      this.user.settings.connNodeFields = this.nodeFields;
      this.user.settings.connLinkFields = this.linkFields;

      UserService.saveSettings(this.user.settings, this.user.userId);
    },
    drawGraph: function (data) {
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
      const srcFieldIsTime = this.dbField2Type(this.query.srcField) === 'seconds';
      const dstFieldIsTime = this.dbField2Type(this.query.dstField) === 'seconds';

      if (srcFieldIsTime || dstFieldIsTime) {
        for (let dataNode of data.nodes) {
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
      const height = $(window).height() - 171;

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
          .on('zoom', () => {
            container.attr('transform', d3.event.transform);
          })
      );

      // add links
      link = container.append('g')
        .attr('stroke', foregroundColor)
        .attr('stroke-opacity', 0.4)
        .selectAll('line')
        .data(links)
        .enter().append('line')
        .attr('class', 'link')
        .attr('stroke-width', this.calculateLinkWeight);

      // add link mouse listeners for showing popups
      link.on('mouseover', (l) => {
        if (draggingNode) { return; }
        if (popupTimer) { clearTimeout(popupTimer); }
        popupTimer = setTimeout(() => {
          this.showLinkPopup(l);
        }, 600);
      }).on('mouseout', (l) => {
        if (popupTimer) { clearTimeout(popupTimer); }
      });

      // add nodes
      node = container.append('g')
        .attr('stroke', foregroundColor)
        .attr('stroke-width', 0.5)
        .selectAll('circle')
        .data(nodes)
        .enter()
        .append('circle')
        .attr('class', 'node')
        .attr('id', (d) => {
          return 'id' + d.id.replace(idRegex, '_');
        })
        .attr('fill', (d) => {
          return colors[d.type];
        })
        .attr('r', this.calculateNodeWeight)
        .call(d3.drag()
          .on('start', dragstarted)
          .on('drag', dragged)
          .on('end', dragended)
        );

      // add node mouse listeners for showing focus and popups
      node.on('mouseover', (d) => {
        if (draggingNode) { return; }
        if (popupTimer) { clearTimeout(popupTimer); }
        popupTimer = setTimeout(() => {
          this.showNodePopup(d);
        }, 600);
        focus(d);
      }).on('mouseout', (d) => {
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
        .style('pointer-events', 'none') // to prevent mouseover/drag capture
        .text((d) => { return d.id; });

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
      for (let n of nodes) {
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
      for (let l of links) {
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
      let val = this.calculateNodeWeight(nl);
      return 2 + val;
    },
    calculateCollisionRadius: function (n) {
      let val = this.calculateNodeWeight(n);
      return 2 * val;
    },
    dbField2Type: function (dbField) {
      for (let k in this.fields) {
        if (dbField === this.fields[k].dbField ||
            dbField === this.fields[k].rawField) {
          return this.fields[k].type;
        }
      }

      return undefined;
    },
    dbField2Exp: function (dbField) {
      for (let k in this.fields) {
        if (dbField === this.fields[k].dbField ||
            dbField === this.fields[k].rawField) {
          return this.fields[k].exp;
        }
      }

      return undefined;
    },
    showNodePopup: function (dataNode) {
      if (dataNode.type === 2) {
        dataNode.exp = this.dbField2Exp(this.query.dstField);
      } else {
        dataNode.exp = this.dbField2Exp(this.query.srcField);
      }

      closePopups();
      if (!popupVue) {
        popupVue = new Vue({
          template: `
            <div class="connections-popup">
              <div class="mb-2">
                <strong>{{dataNode.id}}</strong>
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
                  <dt :title="fields[field].friendlyName">
                    {{ fields[field].exp }}
                  </dt>
                  <dd>
                    <span v-if="!Array.isArray(dataNode[field])">
                      <moloch-session-field
                        :value="dataNode[field]"
                        :session="dataNode"
                        :expr="fields[field].exp"
                        :field="fields[field]"
                        :pull-left="true">
                      </moloch-session-field>
                    </span>
                    <span v-else
                      v-for="value in dataNode[field]">
                      <moloch-session-field
                        :value="value"
                        :session="dataNode"
                        :expr="fields[field].exp"
                        :field="fields[field]"
                        :pull-left="true">
                      </moloch-session-field>
                    </span>&nbsp;
                  </dd>
                </span>

                <dt>Expressions</dt>
                <dd>
                  <a class="cursor-pointer no-decoration"
                    href="javascript:void(0)"
                    @click.stop.prevent="addExpression('&&')">
                    AND
                  </a>&nbsp;
                  <a class="cursor-pointer no-decoration"
                    href="javascript:void(0)"
                    @click.stop.prevent="addExpression('||')">
                    OR
                  </a>
                </dd>
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
            dataNode: dataNode,
            nodeFields: this.nodeFields,
            fields: this.fieldsMap
          },
          methods: {
            hideNode: function () {
              this.$parent.closePopups();
              let id = '#id' + dataNode.id.replace(idRegex, '_');
              svg.select(id).remove();
              svg.select(id + '-label').remove();
              svg.selectAll('.link')
                .filter(function (d, i) {
                  return d.source.id === dataNode.id || d.target.id === dataNode.id;
                })
                .remove();
            },
            addExpression: function (op) {
              let fullExpression = `${this.dataNode.exp} == ${this.dataNode.id}`;
              this.$store.commit('addToExpression', { expression: fullExpression, op: op });
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
      linkData.dstExp = this.dbField2Exp(this.query.dstField);
      linkData.srcExp = this.dbField2Exp(this.query.srcField);

      closePopups();
      if (!popupVue) {
        popupVue = new Vue({
          template: `
            <div class="connections-popup">
              <div class="mb-2">
                <strong>Link</strong>
                <a class="pull-right cursor-pointer no-decoration"
                   @click="closePopup">
                  <span class="fa fa-close"></span>
                </a>
              </div>
              <div>{{linkData.source.id}}</div>
              <div class="mb-2">
                {{linkData.target.id}}
              </div>

              <dl class="dl-horizontal">
                <dt>Sessions</dt>
                <dd>{{linkData.value}}&nbsp;</dd>

                <span v-for="field in linkFields"
                  :key="field">
                  <dt :title="fields[field].friendlyName">
                    {{ fields[field].exp }}
                  </dt>
                  <dd>
                    <span v-if="!Array.isArray(linkData[field])">
                      <moloch-session-field
                        :value="linkData[field]"
                        :session="linkData"
                        :expr="fields[field].exp"
                        :field="fields[field]"
                        :pull-left="true">
                      </moloch-session-field>
                    </span>
                    <span v-else
                      v-for="value in linkData[field]">
                      <moloch-session-field
                        :value="value"
                        :session="linkData"
                        :expr="fields[field].exp"
                        :field="fields[field]"
                        :pull-left="true">
                      </moloch-session-field>
                    </span>&nbsp;
                  </dd>
                </span>

                <dt>Expressions</dt>
                <dd>
                  <a class="cursor-pointer no-decoration"
                    href="javascript:void(0)"
                    @click="addExpression('&&')">AND</a>&nbsp;
                  <a class="cursor-pointer no-decoration"
                    href="javascript:void(0)"
                    @click="addExpression('||')">OR</a>
                </dd>
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
            linkData: linkData,
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
              let fullExpression = `(${linkData.srcExp} == ${linkData.source.id} && ${linkData.dstExp} == ${linkData.target.id})`;
              this.$store.commit('addToExpression', { expression: fullExpression, op: op });
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
    simulation.on('tick', null);
    d3.drag()
      .on('start', null)
      .on('drag', null)
      .on('end', null);
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

    // destroy child component
    $('.connections-popup').remove();
    if (popupVue) { popupVue.$destroy(); }

    // clean up global vars
    svg = undefined;
    zoom = undefined;
    node = undefined;
    link = undefined;
    colors = undefined;
    popupVue = undefined;
    container = undefined;
    nodeLabel = undefined;
    popupTimer = undefined;
    simulation = undefined;
    draggingNode = undefined;
    foregroundColor = undefined;
  }
};
</script>

<style scoped>
.connections-page {
  /* account for main navbar height */
  margin-top: 36px;
}

.connections-graph {
  /* don't allow selecting text */
  -webkit-user-select: none;
  -moz-user-select: none;
  user-select: none;
}

/* position the subnavbar */
.connections-page form.connections-form {
  position: fixed;
  top: 110px;
  left: 0;
  right: 0;
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

/* accommodate top navbars */
.connections-page .connections-content {
  padding-top: 110px;
}

/* make the color for legend areas white */
.connections-page form.connections-form .input-group-prepend.legend > .input-group-text {
  font-weight: 700;
  color: white !important;
}

/* apply foreground theme color */
.connections-page svg {
  fill: var(--color-foreground, #333);
}

/* zoom/font size/node distance buttons overlaying the graph */
.connections-content .zoom-btns {
  position: fixed;
  top: 160px;
  right: 10px;
}
.connections-content .text-size-btns {
  position: fixed;
  top: 160px;
  right: 47px;
}
.connections-content .node-distance-btns {
  position: fixed;
  top: 160px;
  right: 90px;
}

.connections-content .overlay-btns > span:first-child > button {
  border-bottom: none;
  border-radius: 4px 4px 0 0;
}
.connections-content .overlay-btns > span:last-child > button {
  border-radius: 0 0 4px 4px;
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
  border-bottom-right-radius: 4px !important;;
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
