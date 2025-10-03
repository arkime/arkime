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
          @changeSearch="cancelAndLoad(true)"
          @recalc-collapse="$emit('recalc-collapse')">
        </arkime-search> <!-- /search navbar -->

        <!-- connections sub navbar -->
        <div class="connections-form m-1">
          <BRow gutter-x="1" align-h="start">

            <!-- query size select -->
            <BCol cols="auto">
              <BInputGroup size="sm">
                <BInputGroupText id="querySize" class="cursor-help">
                  {{ $t('connections.querySize') }}
                  <BTooltip target="querySize" :delay="{show: 300, hide: 0}" noninteractive><span v-i18n-btip="'connections.'" /></BTooltip>
                </BInputGroupText>
                <BFormSelect class="form-control input-sm"
                  :model-value="query.length"
                  @update:model-value="(val) => changeLength(val)"
                  :options="[100, 500, 1000, 5000, 10000, 50000, 100000]">
                </BFormSelect>
              </BInputGroup>
            </BCol> <!-- /query size select -->

            <!-- src select -->
            <BCol cols="auto" v-if="fields && fields.length && srcFieldTypeahead && fieldHistoryConnectionsSrc">
              <BInputGroup size="sm">
                <BInputGroupText class="legend cursor-help primary-legend" id="sourceField">
                  Src:
                  <BTooltip target="sourceField" :delay="{show: 300, hide: 0}" noninteractive><span v-i18n-btip="'connections.'" /></BTooltip>
                </BInputGroupText>
                <arkime-field-typeahead
                  :fields="fields"
                  query-param="srcField"
                  :initial-value="srcFieldTypeahead"
                  @fieldSelected="changeSrcField"
                  :history="fieldHistoryConnectionsSrc"
                  page="ConnectionsSrc">
                </arkime-field-typeahead>
              </BInputGroup>
            </BCol> <!-- /src select -->

            <!-- dst select -->
            <BCol cols="auto" v-if="fields && dstFieldTypeahead && fieldHistoryConnectionsDst">
              <BInputGroup size="sm">
                <BInputGroupText class="legend cursor-help secondary-legend" id="dstField">
                  Dst:
                  <BTooltip target="dstField" :delay="{show: 300, hide: 0}" noninteractive><span v-i18n-btip="'connections.'" /></BTooltip>
                </BInputGroupText>
                <arkime-field-typeahead
                  :fields="fields"
                  query-param="dstField"
                  :initial-value="dstFieldTypeahead"
                  @fieldSelected="changeDstField"
                  :history="fieldHistoryConnectionsDst"
                  page="ConnectionsDst">
                </arkime-field-typeahead>
              </BInputGroup>
            </BCol> <!-- /dst select -->

            <!-- src & dst color -->
            <BCol cols="auto">
              <BInputGroup size="sm">
                <BInputGroupText class="legend cursor-help tertiary-legend" id="srcDstColor">
                    Src &amp; dst
                  <BTooltip target="srcDstColor" :delay="{show: 300, hide: 0}" noninteractive><span v-i18n-btip="'connections.'" /></BTooltip>
                </BInputGroupText>
              </BInputGroup>
            </BCol> <!-- /src & dst color -->

            <!-- min connections select -->
            <BCol cols="auto">
              <BInputGroup size="sm">
                <BInputGroupText id="minConn" class="help-cursor">
                  {{ $t('connections.minConn') }}
                  <BTooltip target="minConn" :delay="{show: 300, hide: 0}" noninteractive><span v-i18n-btip="'connections.'" /></BTooltip>
                </BInputGroupText>
                <BFormSelect
                  size="sm"
                  :model-value="query.minConn"
                  @update:model-value="(val) => changeMinConn(val)"
                  :options="[1,2,3,4,5]">
                </BFormSelect>
              </BInputGroup>
            </BCol> <!-- /min connections select -->

            <!-- visualization type select -->
            <BCol cols="auto">
              <BInputGroup size="sm">
                <BInputGroupText class="help-cursor" id="vizType">
                  Visualization
                  <BTooltip target="vizType" :delay="{show: 300, hide: 0}" noninteractive>
                    Choose between network graph and Sankey diagram visualization
                  </BTooltip>
                </BInputGroupText>
                <BFormSelect
                  size="sm"
                  :model-value="vizType"
                  @update:model-value="(val) => changeVizType(val)">
                  <option value="network">Network Graph</option>
                  <option value="sankey">Sankey Diagram</option>
                </BFormSelect>
              </BInputGroup>
            </BCol> <!-- /visualization type select -->

            <!-- weight select -->
            <BCol cols="auto">
              <BInputGroup size="sm">
                <BInputGroupText class="help-cursor" id="weight">
                  {{ $t('connections.weight') }}
                  <BTooltip target="weight" :delay="{show: 300, hide: 0}" noninteractive><span v-i18n-btip="'connections.'" /></BTooltip>
                </BInputGroupText>
                <BFormSelect
                  size="sm"
                  :model-value="weight"
                  @update:model-value="(val) => changeWeight(val)">
                  <option value="sessions" v-i18n-value="'connections.weight-'" />
                  <option value="network.packets" v-i18n-value="'connections.weight-'" />
                  <option value="network.bytes" v-i18n-value="'connections.weight-'" />
                  <option value="totDataBytes" v-i18n-value="'connections.weight-'" />
                  <option value="" v-i18n-value="'connections.weight-'" />
                </BFormSelect>
              </BInputGroup>
            </BCol> <!-- /weight select -->

            <BCol cols="auto" v-if="!loading">
              <!-- node fields button -->
              <b-dropdown
                size="sm"
                no-flip
                no-caret
                toggle-class="rounded"
                class="field-vis-menu ms-1 display-inline"
                variant="theme-primary"
                v-if="fields && groupedFields && nodeFields">
                <template #button-content>
                  <div id="nodeFields">
                    <span class="fa fa-circle-o"></span>
                    <BTooltip target="nodeFields" :delay="{show: 300, hide: 0}" noninteractive><span v-i18n-btip="'connections.'" /></BTooltip>
                  </div>
                </template>
                <b-dropdown-header>
                  <input type="text"
                    v-model="fieldQuery"
                    class="form-control form-control-sm dropdown-typeahead"
                    :placeholder="$t('common.searchForFields')"
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
                <template v-for="(group, key) in filteredFields" :key="key">
                  <b-dropdown-header
                    v-if="group.length"
                    class="group-header">
                    {{ key }}
                  </b-dropdown-header>
                  <template v-for="(field, k) in group" :key="key + k + 'itemnode'">
                    <b-dropdown-item
                      :id="key + k + 'itemnode'"
                      :class="{'active':isFieldVisible(field.dbField, nodeFields) >= 0}"
                      @click.stop.prevent="toggleFieldVisibility(field.dbField, nodeFields)">
                      {{ field.friendlyName }}
                      <small>({{ field.exp }})</small>
                      <BTooltip v-if="field.help" :delay="{show: 300, hide: 0}" noninteractive :target="key + k + 'itemnode'">{{ field.help }}</BTooltip>
                    </b-dropdown-item>
                  </template>
                </template>
              </b-dropdown> <!-- /node fields button -->

              <!-- link fields button -->
              <b-dropdown
                size="sm"
                no-flip
                no-caret
                toggle-class="rounded"
                class="field-vis-menu ms-1 display-inline"
                variant="theme-primary"
                v-if="fields && groupedFields && linkFields">
                <template #button-content>
                  <div id="linkFields">
                    <span class="fa fa-link"></span>
                    <BTooltip target="linkFields" :delay="{show: 300, hide: 0}" noninteractive><span v-i18n-btip="'connections.'" /></BTooltip>
                  </div>
                </template>
                <b-dropdown-header>
                  <input type="text"
                    v-model="fieldQuery"
                    class="form-control form-control-sm dropdown-typeahead"
                    :placeholder="$t('common.searchForFields')"
                  />
                </b-dropdown-header>
                <b-dropdown-divider>
                </b-dropdown-divider>
                <b-dropdown-item
                  @click.stop.prevent="resetLinkFieldsDefault">
                  {{ $t('connections.reset') }}
                </b-dropdown-item>
                <b-dropdown-divider>
                </b-dropdown-divider>
                <template v-for="(group, key) in filteredFields" :key="key">
                  <b-dropdown-header
                    v-if="group.length"
                    class="group-header">
                    {{ key }}
                  </b-dropdown-header>
                  <template v-for="(field, k) in group" :key="key + k + 'itemlink'">
                    <b-dropdown-item
                      :id="key + k + 'itemlink'"
                      :class="{'active':isFieldVisible(field.dbField, linkFields) >= 0}"
                      @click.stop.prevent="toggleFieldVisibility(field.dbField, linkFields)">
                      {{ field.friendlyName }}
                      <small>({{ field.exp }})</small>
                      <BTooltip v-if="field.help" :delay="{show: 300, hide: 0}" noninteractive :target="key + k + 'itemlink'">{{ field.help }}</BTooltip>
                    </b-dropdown-item>
                  </template>
                </template>
              </b-dropdown> <!-- /link fields button -->
            </BCol>

            <!-- network baseline time range -->
            <BCol cols="auto">
              <BInputGroup size="sm">
                <BInputGroupText class="help-cursor" id="baselineDate">
                  {{ $t('connections.baselineDate') }}
                  <BTooltip target="baselineDate" :delay="{show: 300, hide: 0}" noninteractive><span v-i18n-btip="'connections.'" /></BTooltip>
                </BInputGroupText>
                <select class="form-control input-sm"
                  v-model="query.baselineDate"
                  @change="changeBaselineDate">
                  <option value="0">{{ $t('common.optionDisabled') }}</option>
                  <option value="1x">{{ $t('connections.queryRange', 1) }}</option>
                  <option value="2x">{{ $t('connections.queryRange', 2) }}</option>
                  <option value="4x">{{ $t('connections.queryRange', 4) }}</option>
                  <option value="6x">{{ $t('connections.queryRange', 6) }}</option>
                  <option value="8x">{{ $t('connections.queryRange', 8) }}</option>
                  <option value="10x">{{ $t('connections.queryRange', 10) }}</option>
                  <option value="1">{{ $t('common.hourCount', 1) }}</option>
                  <option value="6">{{ $t('common.hourCount', 6) }}</option>
                  <option value="24">{{ $t('common.hourCount', 24) }}</option>
                  <option value="48">{{ $t('common.hourCount', 48) }}</option>
                  <option value="72">{{ $t('common.hourCount', 72) }}</option>
                  <option value="168">{{ $t('common.weekCount', 1) }}</option>
                  <option value="336">{{ $t('common.weekCount', 2) }}</option>
                  <option value="720">{{ $t('common.monthCount', 1) }}</option>
                  <option value="1440">{{ $t('common.monthCount', 2) }}</option>
                  <option value="4380">{{ $t('common.monthCount', 6) }}</option>
                  <option value="8760">{{ $t('common.yearCount', 1) }}</option>
                </select>
              </BInputGroup>
            </BCol> <!-- /network baseline time range -->

            <!-- network baseline node visibility -->
            <BCol cols="auto" v-show="query.baselineDate !== '0'">
              <BInputGroup size="sm">
                <BInputGroupText class="help-cursor" id="baselineVis">
                  {{ $t('connections.baselineVis') }}
                  <BTooltip target="baselineVis" :delay="{show: 300, hide: 0}" noninteractive><span v-i18n-btip="'connections.'" /></BTooltip>
                </BInputGroupText>
                <select class="form-control input-sm"
                  v-bind:disabled="query.baselineDate === '0'"
                  v-model="query.baselineVis"
                  @change="changeBaselineVis">
                  <option value="all" v-i18n-value="'connections.baselineVis-'" />
                  <option value="actual" v-i18n-value="'connections.baselineVis-'" />
                  <option value="actualold" v-i18n-value="'connections.baselineVis-'" />
                  <option value="new" v-i18n-value="'connections.baselineVis-'" />
                  <option value="old" v-i18n-value="'connections.baselineVis-'" />
                </select>
              </BInputGroup>
            </BCol> <!-- /network baseline node visibility -->

          </BRow>
        </div> <!-- /connections sub navbar -->
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
      <div class="connections-viz-container">
        <svg class="connections-graph" :class="{'sankey-graph': vizType === 'sankey'}"></svg>
      </div>
      <!-- /connections graph container -->

      <!-- popup area -->
      <div ref="infoPopup" v-if="showPopup">
        <div class="connections-popup">
          <NodePopup v-if="dataNode"
            :dataNode="dataNode"
            :fields="fieldsMap"
            :nodeFields="nodeFields"
            :baselineDate="query.baselineDate"
            @close="closePopups"
            @hideNode="hideNode"
          />
          <LinkPopup v-if="dataLink"
            :dataLink="dataLink"
            :fields="fieldsMap"
            :linkFields="linkFields"
            @close="closePopups"
            @hideLink="hideLink"
          />
        </div>
      </div> <!-- /popup area -->

      <!-- Button group -->
      <span class="connections-buttons"
        :style= "[showToolBars ? {'top': '160px'} : {'top': '40px'}]">
        <div class="btn-group-vertical unlock-btn overlay-btns">
          <!-- unlock button-->
          <span class="unlock-btn">
            <button class="btn btn-default btn-sm ms-1" id="unlockNodes"
              @click.stop.prevent="unlock">
              <span class="fa fa-unlock"></span>
              <BTooltip
                target="unlockNodes"
                placement="bottom"
                triggers="hover"
                :delay="{show: 300, hide: 0}" noninteractive><span v-i18n-btip="'connections.'" /></BTooltip>
            </button>
          </span> <!-- /unlock button-->
          <!-- export button-->
          <span class="export-btn">
            <button class="btn btn-default btn-sm ms-1" id="exportGraph"
              @click.stop.prevent="exportPng">
              <span class="fa fa-download"></span>
              <BTooltip
                target="exportGraph"
                placement="bottom"
                triggers="hover"
                :delay="{show: 300, hide: 0}" noninteractive><span v-i18n-btip="'connections.'" /></BTooltip>
            </button>
          </span> <!-- /export button-->
        </div>

        <!-- node distance -->
        <div class="btn-group-vertical node-distance-btns overlay-btns">
          <button
            id="nodeDistUp"
            type="button"
            class="btn btn-default btn-sm"
            :class="{'disabled':query.nodeDist >= 200}"
            @click="changeNodeDist(10)">
            <span class="fa fa-plus">
            </span>
            <span class="fa fa-arrows-v">
            </span>
            <BTooltip
              target="nodeDistUp"
              placement="bottom"
              triggers="hover"
              :delay="{show: 300, hide: 0}" noninteractive><span v-i18n-btip="'connections.'" /></BTooltip>
          </button>
          <button
            id="nodeDistDown"
            type="button"
            class="btn btn-default btn-sm"
            :class="{'disabled':query.nodeDist <= 10}"
            @click="changeNodeDist(-10)">
            <span class="fa fa-minus">
            </span>
            <span class="fa fa-arrows-v">
            </span>
            <BTooltip
              target="nodeDistDown"
              placement="bottom"
              triggers="hover"
              :delay="{show: 300, hide: 0}" noninteractive><span v-i18n-btip="'connections.'" /></BTooltip>
          </button>
        </div> <!-- /node distance -->

        <!-- text size increase/decrease -->
        <div class="btn-group-vertical text-size-btns overlay-btns">
          <button
            id="textSizeUp"
            type="button"
            class="btn btn-default btn-sm"
            :class="{'disabled':fontSize >= 1}"
            @click="updateTextSize(0.1)">
            <span class="fa fa-long-arrow-up">
            </span>
            <span class="fa fa-font">
            </span>
            <BTooltip
              target="textSizeUp"
              placement="bottom"
              triggers="hover"
              :delay="{show: 300, hide: 0}" noninteractive><span v-i18n-btip="'connections.'" /></BTooltip>
          </button>
          <button
            id="textSizeDown"
            type="button"
            class="btn btn-default btn-sm"
            :class="{'disabled':fontSize <= 0.2}"
            @click="updateTextSize(-0.1)">
            <span class="fa fa-long-arrow-down">
            </span>
            <span class="fa fa-font">
            </span>
            <BTooltip
              target="textSizeDown"
              placement="bottom"
              triggers="hover"
              :delay="{show: 300, hide: 0}" noninteractive><span v-i18n-btip="'connections.'" /></BTooltip>
          </button>
        </div> <!-- /text size increase/decrease -->

        <!-- zoom in/out -->
        <div class="btn-group-vertical zoom-btns overlay-btns">
          <button
            id="zoomIn"
            type="button"
            class="btn btn-default btn-sm"
            :class="{'disabled':zoomLevel >= 4}"
            @click="zoomConnections(2)">
            <span class="fa fa-lg fa-search-plus">
            </span>
            <BTooltip
              target="zoomIn"
              placement="bottom"
              triggers="hover"
              :delay="{show: 300, hide: 0}" noninteractive><span v-i18n-btip="'connections.'" /></BTooltip>
          </button>
          <button
            id="zoomOut"
            type="button"
            class="btn btn-default btn-sm"
            :class="{'disabled':zoomLevel <= 0.0625}"
            @click="zoomConnections(0.5)">
            <span class="fa fa-lg fa-search-minus">
            </span>
            <BTooltip
              target="zoomOut"
              placement="bottom"
              triggers="hover"
              :delay="{show: 300, hide: 0}" noninteractive><span v-i18n-btip="'connections.'" /></BTooltip>
          </button>
        </div> <!-- /zoom in/out -->
      </span> <!-- /Button group -->
    </div>

  </div>

</template>

<script>
// import components
import ArkimeSearch from '../search/Search.vue';
import ArkimeError from '../utils/Error.vue';
import ArkimeLoading from '../utils/Loading.vue';
import ArkimeNoResults from '../utils/NoResults.vue';
import ArkimeCollapsible from '../utils/CollapsibleWrapper.vue';
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

// simulation resize helper
function resize (toolbarDown = true) {
  const width = $(window).width() - 10;
  // 36px for navbar + 25px for footer = 61px.
  const height = $(window).height() - (toolbarDown ? 171 : 61);

  // set the width and height of the canvas
  svg.attr('width', width).attr('height', height);
}

// other necessary vars ---------------------------------------------------- */
// default fields to display in the node/link popups
const defaultLinkFields = ['network.bytes', 'totDataBytes', 'network.packets', 'node'];
const defaultNodeFields = ['network.bytes', 'totDataBytes', 'network.packets', 'node'];

// vue definition ---------------------------------------------------------- */
export default {
  name: 'Connections',
  components: {
    ArkimeSearch,
    ArkimeError,
    ArkimeLoading,
    ArkimeNoResults,
    ArkimeCollapsible,
    ArkimeFieldTypeahead,
    NodePopup,
    LinkPopup
  },
  emits: ['recalc-collapse'],
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
      fontSize: 0.4,
      zoomLevel: 1,
      weight: 'sessions',
      vizType: this.$route.query.vizType || 'network',
      lastGraphData: null, // store last graph data for redrawing
      fieldHistoryConnectionsSrc: undefined,
      fieldHistoryConnectionsDst: undefined,
      showPopup: false, // whether to show the node/link data popup
      dataNode: undefined, // data for the node popup
      dataLink: undefined // data for the link popup
    };
  },
  computed: {
    query: function () {
      return {
        start: 0, // first item index
        length: this.$route.query.length || 100, // page length
        date: store.state.timeRange,
        startTime: store.state.time.startTime,
        stopTime: store.state.time.stopTime,
        srcField: this.$route.query.srcField || store.state.user.settings.connSrcField || 'source.ip',
        dstField: this.$route.query.dstField || store.state.user.settings.connDstField || 'destination.ip',
        bounding: this.$route.query.bounding || 'last',
        interval: this.$route.query.interval || 'auto',
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
    // Boolean in the store will remember chosen toggle state for all pages
    showToolBars: function () {
      return store.state.showToolBars;
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
    '$route.query.vizType': function (newVal, oldVal) {
      this.vizType = newVal || 'network';
      if (this.lastGraphData) {
        this.drawGraphWrapper(this.lastGraphData);
      }
    },

    // Resize svg height after toggle is updated and mounted()
    showToolBars: function () {
      this.$nextTick(() => {
        resize(this.showToolBars);
      });
    }
  },
  mounted () {
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
          length: len
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
    changeVizType: function (vizType) {
      this.vizType = vizType;
      this.$router.push({
        query: {
          ...this.$route.query,
          vizType: this.vizType
        }
      });
    },
    changeWeight: function (weight) {
      this.weight = weight;
      if (this.weight) { this.getMinMaxForScale(); }

      if (this.vizType === 'network') {
        svg.selectAll('.node')
          .attr('r', this.calculateNodeWeight);

        svg.selectAll('.link')
          .attr('stroke-width', this.calculateLinkWeight);

        svg.selectAll('.node-label')
          .attr('dx', this.calculateNodeLabelOffset);
      } else if (this.vizType === 'sankey' && this.lastGraphData) {
        // For Sankey, weight changes trigger a redraw
        this.drawGraphWrapper(this.lastGraphData);
      }
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
      svg.selectAll('.link').filter(function (d, i) {
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
        this.error = error.text || error;
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
      this.lastGraphData = data; // Store data for redrawing
      import('d3').then(async (d3Module) => {
        d3 = d3Module;
        if (this.vizType === 'sankey') {
          await this.drawSankeyGraph(data);
        } else {
          this.drawGraph(data);
        }
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
    async drawSankeyGraph (data) {
      // Initialize colors if not already done
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

      // Clear existing visualizations
      if (svg) {
        svg.selectAll('*').remove();
      }

      // Don't do anything if there's no data to process
      if (!data.nodes.length) { return; }

      // Calculate the width and height of the canvas
      const width = $(window).width() - 10;
      const height = $(window).height() - (this.showToolBars ? 171 : 61);

      // Transform data for Sankey format
      const sankeyData = this.transformDataForSankey(data);

      console.log('Original data:', data);
      console.log('Transformed Sankey data:', sankeyData);

      // Validate Sankey data
      if (!sankeyData.nodes.length || !sankeyData.links.length) {
        console.warn('No valid Sankey data to display');
        return;
      }

      // Validate node and link values
      sankeyData.nodes.forEach(node => {
        if (!node.id || typeof node.id !== 'string') {
          console.warn('Invalid node id:', node);
          node.id = `node_${Math.random()}`;
        }
        if (isNaN(node.value) || node.value <= 0) {
          console.warn('Invalid node value:', node);
          node.value = 1; // Set default value
        }
      });

      sankeyData.links.forEach(link => {
        if (!link.source || !link.target) {
          console.warn('Invalid link source/target:', link);
        } else if (typeof link.source === 'number' || typeof link.target === 'number') {
          console.warn('Link still has numeric references:', link);
        } else if (isNaN(link.value) || link.value <= 0) {
          console.warn('Invalid link value, setting to 1:', link);
          link.value = 1; // Set default value
        }
      });

      if (!svg) {
        svg = d3.select('svg')
          .attr('width', width)
          .attr('height', height)
          .attr('id', 'graphSvg');
      } else {
        svg.attr('width', width).attr('height', height);
      }

      // Import d3-sankey
      const { sankey, sankeyLinkHorizontal } = await import('d3-sankey');

      // Create Sankey generator
      const sankeyLayout = sankey()
        .nodeId(d => d.id)
        .nodeWidth(15)
        .nodePadding(10)
        .extent([[1, 1], [width - 1, height - 6]]);

      // Generate the Sankey layout
      const graph = sankeyLayout(sankeyData);

      // Validate the generated graph
      if (!graph.nodes || !graph.links) {
        console.error('Sankey layout failed to generate valid graph');
        return;
      }

      // Add links
      const links = svg.append('g')
        .attr('stroke', '#000')
        .attr('fill', 'none')
        .selectAll('path')
        .data(graph.links)
        .enter()
        .append('path')
        .attr('d', sankeyLinkHorizontal())
        .attr('stroke', d => this.getSankeyLinkColor(d))
        .attr('stroke-width', d => Math.max(1, d.width))
        .attr('opacity', 0.6)
        .on('mouseover', (e, d) => {
          if (popupTimer) { clearTimeout(popupTimer); }
          popupTimer = setTimeout(() => {
            this.showSankeyLinkPopup(d, e);
          }, 600);
        })
        .on('mouseout', () => {
          if (popupTimer) { clearTimeout(popupTimer); }
        });

      // Add nodes
      const nodes = svg.append('g')
        .selectAll('rect')
        .data(graph.nodes)
        .enter()
        .append('rect')
        .attr('x', d => d.x0)
        .attr('y', d => d.y0)
        .attr('height', d => d.y1 - d.y0)
        .attr('width', d => d.x1 - d.x0)
        .attr('fill', d => this.getSankeyNodeColor(d))
        .attr('stroke', this.foregroundColor)
        .attr('stroke-width', 0.5)
        .on('mouseover', (e, d) => {
          if (popupTimer) { clearTimeout(popupTimer); }
          popupTimer = setTimeout(() => {
            this.showSankeyNodePopup(d, e);
          }, 600);
        })
        .on('mouseout', () => {
          if (popupTimer) { clearTimeout(popupTimer); }
        });

      // Add node labels
      svg.append('g')
        .style('font', '10px sans-serif')
        .selectAll('text')
        .data(graph.nodes)
        .enter()
        .append('text')
        .attr('x', d => d.x0 < width / 2 ? d.x1 + 6 : d.x0 - 6)
        .attr('y', d => (d.y1 + d.y0) / 2)
        .attr('dy', '0.35em')
        .attr('text-anchor', d => d.x0 < width / 2 ? 'start' : 'end')
        .text(d => d.id)
        .style('fill', this.foregroundColor);
    },
    transformDataForSankey: function (data) {
      // Transform the connections data into Sankey format
      const nodes = [];
      const links = [];
      const nodeMap = new Map();
      const nodeIndexMap = new Map();

      // Create unique nodes from source and destination
      data.nodes.forEach((node, index) => {
        // Ensure node.id is a string and exists
        const nodeId = String(node.id || `node_${index}`);

        if (!nodeMap.has(nodeId)) {
          const sankeyNode = {
            id: nodeId,
            name: nodeId,
            type: node.type,
            value: this.getSankeyNodeValue(node),
            ...node
          };
          nodeMap.set(nodeId, sankeyNode);
          nodeIndexMap.set(index, sankeyNode);
          nodes.push(sankeyNode);
        }
      });

      // Create links from connections data
      data.links.forEach(link => {
        // Handle both index-based and id-based references
        let sourceNode, targetNode;

        if (typeof link.source === 'number') {
          // Get the original node at this index and find its corresponding Sankey node
          const originalSourceNode = data.nodes[link.source];
          if (originalSourceNode) {
            const sourceNodeId = String(originalSourceNode.id || `node_${link.source}`);
            sourceNode = nodeMap.get(sourceNodeId);
          }
        } else if (typeof link.source === 'object' && link.source.id) {
          sourceNode = nodeMap.get(String(link.source.id));
        } else {
          sourceNode = nodeMap.get(String(link.source));
        }

        if (typeof link.target === 'number') {
          // Get the original node at this index and find its corresponding Sankey node
          const originalTargetNode = data.nodes[link.target];
          if (originalTargetNode) {
            const targetNodeId = String(originalTargetNode.id || `node_${link.target}`);
            targetNode = nodeMap.get(targetNodeId);
          }
        } else if (typeof link.target === 'object' && link.target.id) {
          targetNode = nodeMap.get(String(link.target.id));
        } else {
          targetNode = nodeMap.get(String(link.target));
        }

        if (sourceNode && targetNode) {
          const linkValue = this.getSankeyLinkValue(link);
          // Create new link object with corrected source/target, avoiding spread that would overwrite
          const newLink = {
            source: sourceNode.id,
            target: targetNode.id,
            value: linkValue,
            // Copy other properties except source and target
            ...(({ source, target, ...rest }) => rest)(link)
          };
          links.push(newLink);
        } else {
          console.warn('Could not resolve link nodes:', {
            link,
            sourceNode,
            targetNode,
            originalSource: typeof link.source === 'number' ? data.nodes[link.source] : link.source,
            originalTarget: typeof link.target === 'number' ? data.nodes[link.target] : link.target
          });
        }
      });

      // Remove circular links by combining bidirectional connections
      const combinedLinks = this.combineReverseLinks(links);

      // Break any remaining cycles
      const acyclicLinks = this.breakCycles(nodes, combinedLinks);

      return { nodes, links: acyclicLinks };
    },
    combineReverseLinks: function (links) {
      const linkMap = new Map();
      const combinedLinks = [];

      links.forEach(link => {
        // Create a key that's the same for both directions
        const key1 = `${link.source}-${link.target}`;
        const key2 = `${link.target}-${link.source}`;

        if (linkMap.has(key2)) {
          // We found the reverse link, combine them
          const existingLink = linkMap.get(key2);
          existingLink.value += link.value;
          // Keep additional properties from both links
          existingLink.reverseValue = link.value;
          existingLink.bidirectional = true;
        } else {
          // No reverse link found yet, store this one
          linkMap.set(key1, { ...link });
          combinedLinks.push(linkMap.get(key1));
        }
      });

      console.log(`Combined ${links.length} links into ${combinedLinks.length} links`);
      return combinedLinks;
    },
    breakCycles: function (nodes, links) {
      // Build adjacency list and link lookup
      const graph = new Map();
      const linkLookup = new Map();

      // Initialize graph with all nodes
      nodes.forEach(node => {
        graph.set(node.id, []);
      });

      // Build adjacency list and link lookup
      links.forEach((link, index) => {
        if (graph.has(link.source) && graph.has(link.target)) {
          graph.get(link.source).push(link.target);
          linkLookup.set(`${link.source}-${link.target}`, { ...link, index });
        }
      });

      const toRemove = new Set();
      const visited = new Set();
      const recStack = new Set();
      const path = [];

      // DFS to find cycles
      const hasCycleDFS = (node) => {
        if (recStack.has(node)) {
          // Found a cycle, find the weakest link in the cycle
          const cycleStart = path.indexOf(node);
          const cyclePath = path.slice(cycleStart);
          cyclePath.push(node); // Complete the cycle

          let weakestLink = null;
          let weakestValue = Infinity;

          for (let i = 0; i < cyclePath.length - 1; i++) {
            const linkKey = `${cyclePath[i]}-${cyclePath[i + 1]}`;
            const link = linkLookup.get(linkKey);
            if (link && link.value < weakestValue) {
              weakestValue = link.value;
              weakestLink = linkKey;
            }
          }

          if (weakestLink) {
            console.log(`Breaking cycle by removing link: ${weakestLink} (value: ${weakestValue})`);
            toRemove.add(weakestLink);
          }

          return true;
        }

        if (visited.has(node)) {
          return false;
        }

        visited.add(node);
        recStack.add(node);
        path.push(node);

        const neighbors = graph.get(node) || [];
        for (const neighbor of neighbors) {
          const linkKey = `${node}-${neighbor}`;
          if (!toRemove.has(linkKey) && hasCycleDFS(neighbor)) {
            path.pop();
            recStack.delete(node);
            return true;
          }
        }

        path.pop();
        recStack.delete(node);
        return false;
      };

      // Keep checking for cycles until none are found
      let foundCycle = true;
      let iterations = 0;
      const maxIterations = 100; // Prevent infinite loops

      while (foundCycle && iterations < maxIterations) {
        foundCycle = false;
        visited.clear();
        recStack.clear();
        path.length = 0;

        for (const node of nodes.map(n => n.id)) {
          if (!visited.has(node) && hasCycleDFS(node)) {
            foundCycle = true;
            break;
          }
        }
        iterations++;
      }

      // Filter out the links marked for removal
      const acyclicLinks = links.filter((link, index) => {
        const linkKey = `${link.source}-${link.target}`;
        return !toRemove.has(linkKey);
      });

      console.log(`Broke ${toRemove.size} links to eliminate cycles. Final link count: ${acyclicLinks.length}`);
      return acyclicLinks;
    },
    getSankeyLinkValue: function (link) {
      // Get the value for the link width based on the selected weight
      let value = 1;
      if (this.weight && this.weight !== '') {
        if (this.weight === 'sessions') {
          value = parseInt(link.value) || 1;
        } else {
          value = parseInt(link[this.weight]) || 1;
        }
      }
      // Ensure we return a valid positive number
      return isNaN(value) || value <= 0 ? 1 : value;
    },
    getSankeyNodeValue: function (node) {
      // Get the value for the node based on the selected weight
      let value = 1;
      if (this.weight && this.weight !== '') {
        if (this.weight === 'sessions') {
          value = parseInt(node.sessions) || 1;
        } else {
          value = parseInt(node[this.weight]) || 1;
        }
      }
      // Ensure we return a valid positive number
      return isNaN(value) || value <= 0 ? 1 : value;
    },
    getSankeyNodeColor: function (node) {
      // Color nodes based on their type (similar to network graph)
      if (node.type === 1) {
        return this.primaryColor; // Source nodes
      } else if (node.type === 2) {
        return this.secondaryColor; // Destination nodes
      } else {
        return this.tertiaryColor; // Both source and destination
      }
    },
    getSankeyLinkColor: function (link) {
      // Color links with a blend or use a neutral color
      return this.foregroundColor;
    },
    showSankeyNodePopup: function (node, event) {
      // Show popup for Sankey node (similar to network node popup)
      this.dataLink = undefined;
      this.dataNode = {
        ...node,
        dbField: node.type === 2
          ? FieldService.getFieldProperty(this.query.dstField, 'dbField')
          : FieldService.getFieldProperty(this.query.srcField, 'dbField'),
        exp: node.type === 2
          ? FieldService.getFieldProperty(this.query.dstField, 'exp')
          : FieldService.getFieldProperty(this.query.srcField, 'exp')
      };
      this.showPopup = true;
    },
    showSankeyLinkPopup: function (link, event) {
      // Show popup for Sankey link (similar to network link popup)
      this.dataNode = undefined;
      this.dataLink = {
        source: { id: link.source.id },
        target: { id: link.target.id },
        value: link.value,
        dstDbField: FieldService.getFieldProperty(this.query.dstField, 'dbField'),
        srcDbField: FieldService.getFieldProperty(this.query.srcField, 'dbField'),
        dstExp: FieldService.getFieldProperty(this.query.dstField, 'exp'),
        srcExp: FieldService.getFieldProperty(this.query.srcField, 'exp'),
        ...link
      };
      this.showPopup = true;
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
    window.removeEventListener('resize', resize);
    window.removeEventListener('keyup', this.closePopupsOnEsc);
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

    setTimeout(() => {
      // clean up global vars
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
.connections-page .connections-form {
  z-index: 4;
  background-color: var(--color-quaternary-lightest);

  -webkit-box-shadow: 0 0 16px -2px black;
     -moz-box-shadow: 0 0 16px -2px black;
          box-shadow: 0 0 16px -2px black;
}

/* remove select box styles */
.connections-page .connections-form select {
  -webkit-appearance: none;
}

/* make the color for legend areas white */
.connections-page .connections-form .legend.input-group-text {
  font-weight: 700;
  color: var(--color-button, #FFF) !important;
}
.connections-page .connections-form .legend.primary-legend {
  background-color: var(--color-primary) !important;
}
.connections-page .connections-form .legend.tertiary-legend {
  background-color: var(--color-tertiary) !important;
}
.connections-page .connections-form .legend.secondary-legend {
  border-radius: 4px;
  background-color: var(--color-secondary) !important;
}

/* apply foreground theme color */
.connections-page svg {
  fill: var(--color-foreground, #333);
}

/* buttons overlaying the graph */
.connections-content .overlay-btns {
  margin-right: 4px;
}
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

/* Sankey diagram specific styles */
.connections-viz-container .sankey-graph {
  overflow: visible;
}

.connections-viz-container .sankey-graph path {
  cursor: pointer;
}

.connections-viz-container .sankey-graph rect {
  cursor: pointer;
}

.connections-viz-container .sankey-graph text {
  pointer-events: none;
}
</style>

<style>
/* this needs to not be scoped because it's a child component */
/* node/link data popup */
.connections-page div.connections-popup {
  position: absolute;
  left: 0;
  top: 75px;
  bottom: 24px;
  font-size: 0.85rem;
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
