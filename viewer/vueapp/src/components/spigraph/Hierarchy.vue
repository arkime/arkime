<!--
Copyright Yahoo Inc.
SPDX-License-Identifier: Apache-2.0
-->
<template>
  <div class="spigraph-pie">
    <!-- field select: teleported into the spigraph sub-navbar, alongside the
         other controls (same pattern as the connections view) -->
    <teleport
      defer
      to="#spigraph-subfield-anchor">
      <div class="d-flex flex-row align-center">
        <!-- a bar is split by at most one more field -->
        <div
          class="d-inline"
          v-if="fields && fields.length && !(spiGraphType === 'bar' && fieldTypeaheadList.length >= 1)">
          <div class="arkime-input-group subfield-group">
            <span class="arkime-input-label">
              {{ $t('spigraph.addAnotherField') }}:
            </span>
            <arkime-field-typeahead
              :fields="fields"
              @field-selected="changeField"
              page="SpigraphSubfield" />
          </div>
        </div>
        <div class="d-inline ms-1">
          <drag-list
            :list="this.fieldTypeaheadList"
            @reorder="reorderFields"
            @remove="removeField" />
        </div>
      </div>
    </teleport> <!-- /field select -->

    <!-- info area -->
    <div
      class="pie-popup"
      v-show="popupInfo">
      <popup
        v-if="popupInfo"
        :popup-info="popupInfo"
        :field-list="fieldList"
        @close-info="closeInfo" />
    </div> <!-- /info area -->

    <!-- pie chart area -->
    <div
      id="pie-area"
      v-show="spiGraphType === 'pie' && vizData && vizData.children.length" />
    <!-- /pie chart area -->

    <!-- treemap area -->
    <div
      id="treemap-area"
      v-show="spiGraphType === 'treemap' && vizData && vizData.children.length" />
    <!-- /treemap area -->

    <!-- sankey area -->
    <div
      id="sankey-area"
      v-show="spiGraphType === 'sankey' && sankeyData && sankeyData.nodes && sankeyData.nodes.length" />
    <!-- /sankey area -->

    <!-- bar area: the first field's values as bars, split by the second
         field's values when one is chosen (stacked or grouped) -->
    <div
      v-show="spiGraphType === 'bar' && barData.length"
      class="bar-area">
      <summary-bar-chart
        v-if="spiGraphType === 'bar' && barData.length && baseFieldObj"
        :data="barData"
        svg-id="spigraph-bar"
        :field-config="baseFieldObj"
        :split-field-config="barSplitField"
        :bar-mode="barMode"
        :width="barWidth"
        :height="barHeight"
        metric-type="sessions"
        @show-tooltip="showBarTooltip" />
      <summary-chart-tooltip
        :visible="barTooltip.visible"
        :data="barTooltip.data"
        :field-config="baseFieldObj"
        :position="barTooltip.position"
        :percentage="barTooltip.percentage"
        metric-type="sessions"
        :split="barTooltip.split"
        @close="barTooltip.visible = false" />
    </div> <!-- /bar area -->

    <!-- table area: the fields' value combinations (intersection) -->
    <div
      v-show="spiGraphType === 'table' && !uniqueTable && tableData.length && fieldList.length"
      class="m-1 pt-1">
      <table
        class="spigraph-table"
        id="spigraphTable"
        ref="table">
        <thead>
          <tr>
            <template
              v-for="(field, index) in fieldList"
              :key="index">
              <th
                v-if="field"
                :colspan="field.hide ? 1 : 2"
                :key="index"
                class="col-header">
                <div class="grip">
&nbsp;
                </div>
                <span v-if="field">
                  {{ field.friendlyName }}
                  <a
                    v-if="index === fieldList.length - 1 && hiddenColumns"
                    class="float-right cursor-pointer ms-2"
                    @click="showHiddenColumns">
                    <v-icon icon="mdi-plus-box" />
                    <v-tooltip activator="parent">{{ $t('spigraph.showHiddenColumnsTip') }}</v-tooltip>
                  </a>
                </span>
              </th>
            </template>
          </tr>
          <tr>
            <template
              v-for="(item, index) in fieldList"
              :key="`${index}-name`">
              <th
                class="cursor-pointer"
                @click="columnClick(index, 'name')">
                {{ $t('spigraph.tableValue') }}
                <v-icon
                  icon="mdi-chevron-up"
                  class="ms-2"
                  v-show="tableSortField === index && tableSortType === 'name' && !tableDesc" />
                <v-icon
                  icon="mdi-chevron-down"
                  class="ms-2"
                  v-show="tableSortField === index && tableSortType === 'name' && tableDesc" />
              </th>
              <th
                class="cursor-pointer"
                :key="`${index}-size`"
                @click="columnClick(index, 'size')"
                v-if="item && !item.hide">
                {{ $t('spigraph.tableCount') }}
                <v-icon
                  icon="mdi-chevron-up"
                  class="ms-2"
                  v-show="tableSortField === index && tableSortType === 'size' && !tableDesc" />
                <v-icon
                  icon="mdi-chevron-down"
                  class="ms-2"
                  v-show="tableSortField === index && tableSortType === 'size' && tableDesc" />
                <a
                  @click="hideColumn(item)"
                  class="float-right ms-2"
                  v-if="index !== fieldList.length - 1">
                  <v-icon icon="mdi-minus-box" />
                  <v-tooltip activator="parent">{{ $t('spigraph.hideColumnTip') }}</v-tooltip>
                </a>
              </th>
            </template>
          </tr>
        </thead>
        <tbody>
          <template
            v-for="(item, key) in tableData"
            :key="key">
            <tr>
              <template v-if="item.parents && item.parents.length">
                <template
                  v-for="(parent, index) in item.parents"
                  :key="`${index}-${parent.name}-0`">
                  <td>
                    <span
                      class="color-swatch"
                      style="background-color:transparent;" />
                    <!-- the table stays rendered (hidden) under the other
                         types, so its rows can be from more fields than the
                         list now has (eg a bar keeps one): until the reload
                         replaces them, those levels show as plain text -->
                    <arkime-session-field
                      v-if="fieldList[index]"
                      :field="fieldList[index]"
                      :value="parent.name"
                      :expr="fieldList[index].exp"
                      :parse="true"
                      :session-btn="true"
                      :pull-left="true" />
                    <template v-else>
                      {{ parent.name }}
                    </template>
                  </td>
                  <td
                    :key="`${index}-${parent.name}-1`"
                    v-if="fieldList[index] && !fieldList[index].hide">
                    {{ commaString(parent.size) }}
                  </td>
                </template>
              </template>
              <td>
                <span
                  class="color-swatch"
                  :style="{ backgroundColor: item.color }" />
                <template v-if="item.parents && item.parents.length && fieldList[item.parents.length]">
                  <arkime-session-field
                    :field="fieldList[item.parents.length]"
                    :value="item.name"
                    :expr="fieldList[item.parents.length].exp"
                    :parse="true"
                    :session-btn="true"
                    :pull-left="true" />
                </template>
                <template v-else-if="fieldList[0]">
                  <arkime-session-field
                    :field="fieldList[0]"
                    :value="item.name"
                    :expr="fieldList[0].exp"
                    :parse="true"
                    :session-btn="true"
                    :pull-left="true" />
                </template>
                <template v-else>
                  {{ item.name }}
                </template>
              </td>
              <td>
                {{ commaString(item.size) }}
              </td>
            </tr>
          </template>
        </tbody>
      </table>
    </div>
    <!-- /table area -->

    <!-- unique table area: each field's own top values in its own column
         pair, rows lined up by rank (not combinations) -->
    <div
      v-if="uniqueTable && uniqueColumns.length"
      class="m-1 pt-1">
      <table class="spigraph-table">
        <thead>
          <tr>
            <th
              v-for="(column, index) in uniqueColumns"
              :key="index"
              colspan="2">
              {{ fieldList[index]?.friendlyName || column.exp }}
            </th>
          </tr>
          <tr>
            <template
              v-for="(column, index) in uniqueColumns"
              :key="`${index}-head`">
              <th>{{ $t('spigraph.tableValue') }}</th>
              <th>{{ $t('spigraph.tableCount') }}</th>
            </template>
          </tr>
        </thead>
        <tbody>
          <tr
            v-for="row in uniqueRowCount"
            :key="row">
            <template
              v-for="(column, index) in uniqueColumns"
              :key="`${index}-${row}`">
              <td>
                <arkime-session-field
                  v-if="column.values[row - 1] && fieldList[index]"
                  :field="fieldList[index]"
                  :value="column.values[row - 1].name"
                  :expr="fieldList[index].exp"
                  :parse="true"
                  :session-btn="true"
                  :pull-left="true" />
              </td>
              <td>
                {{ column.values[row - 1] ? commaString(column.values[row - 1].size) : '' }}
              </td>
            </template>
          </tr>
        </tbody>
      </table>
    </div>
    <!-- /unique table area -->
  </div>
</template>

<script>
// import services
import SpigraphService from './SpigraphService';
import { themedColor } from '@common/themes/themedColor.js';
// import internal
import ArkimeFieldTypeahead from '../utils/FieldTypeahead.vue';
import Popup from './Popup.vue';
import DragList from '../utils/DragList.vue';
import SummaryBarChart from '../summary/SummaryBarChart.vue';
import SummaryChartTooltip from '../summary/SummaryChartTooltip.vue';
import { OTHER_COLOR } from '../summary/widgets/chartColors';
import { isUnderOther, splitPieLayout, fitLabel } from '../utils/hierarchyLayout';
// import utils
import Utils from '../utils/utils';
import ConfigService from '../utils/ConfigService';
import { hierarchyToSankey } from '../utils/sankeyData';
import { commaString } from '@common/vueFilters.js';
import { resolveMessage } from '@common/resolveI18nMessage';
import { attachTableGrips } from '@common/composables/useColumnResize.js';

let d3; // lazy load d3
let sankey, sankeyLinkHorizontal; // lazy load d3-sankey

let init = true;

// common page variables --------------------------------------------------- //
let pendingPromise; // save a pending promise to be able to cancel it

// drops the request in flight: its fetch, and the search it started (the
// server registered it under the cancelId, so it doesn't run to completion)
function abortPending (reason) {
  if (!pendingPromise) { return; }
  const { controller, cancelId } = pendingPromise;
  pendingPromise = null;
  controller.abort(reason);
  ConfigService.cancelEsTask(cancelId).catch(() => {}); // best effort
}
let popupVue; // vue component to mount when showing pie slice information
let popupTimer; // timer to debounce pie slice info popup events
let resizeTimer; // timer to debounce resizing the pie graph on window resize
let colors; // colors function to apply colors to treemap boxes and pie slices
let background; // color of app background
let foreground; // color of app foreground

// page pie variables ------------------------------------------------------ //
let g, newSlice, arc;
let width = getWindowWidth();
let height = getWindowHeight();
let radius = getRadius();

// page treemap variables -------------------------------------------------- //
let gtree, newBox;
const treemapMargin = 4;
let treemapWidth = getTreemapWidth();
let treemapHeight = getTreemapHeight();

// page sankey variables -------------------------------------------------- //
let gsankey;
const sankeyMargin = { top: 2, right: 2, bottom: 2, left: 2 };
let sankeyWidth = getSankeyWidth();
let sankeyHeight = getSankeyHeight();

const MIN_COL_WIDTH = 70;

// pie functions ----------------------------------------------------------- //
// the scroll container is the real drawing area; measure it directly
function getContentEl () {
  return document.querySelector('.spigraph-page .page-scroll');
}

function getWindowWidth () {
  const el = getContentEl();
  return (el ? el.clientWidth : window.innerWidth) - 8;
}

function getWindowHeight () {
  const el = getContentEl();
  if (!el) { return window.innerHeight - 184; }
  // measure the live scroll container, not a guessed fixed header height
  const area = el.querySelector('.spigraph-pie');
  const offsetTop = area ? (area.getBoundingClientRect().top - el.getBoundingClientRect().top) : 0;
  return el.clientHeight - offsetTop - 8;
}

function getRadius () {
  return Math.min(width, height) / 2;
}

function getArc () {
  return d3.arc() // calculate size of each arc from data
    .startAngle((d) => { d.x0s = d.x0; return d.x0; }) // radian location for start of arc
    .endAngle((d) => { d.x1s = d.x1; return d.x1; }) // radian location for end of arc
    .innerRadius((d) => { return d.y0; }) // radian location for inside arc
    .outerRadius((d) => { return d.y1; }) // radian location for outside arc
    .cornerRadius(6); // rounded corners cause they're pretty
}

function textTransform (d) {
  const x = (d.x0 + d.x1) / 2 * 180 / Math.PI;
  const y = (d.y0 + d.y1) / 2;
  return `rotate(${x - 90}) translate(${y},0) rotate(${x < 180 ? 0 : 180})`;
}

function mouseover (d, self) {
  d3.select(self).raise(); // bring slice to front for border visibility
  d3.select(self).select('path').style('stroke', foreground);
}

function mouseleave (d, self) {
  d3.select(self).select('path').style('stroke', background);
}

// treemap functions ------------------------------------------------------- //
// get a uid based on the name of the node and the parent nodes
function getUid (d) {
  let id = '';
  while (d.parent) {
    id += `-${d.data.name.toString().replace(/\s/g, '')}`;
    d = d.parent;
  }
  return id;
}

function getTreemapWidth () {
  return getWindowWidth();
}

function getTreemapHeight () {
  return getWindowHeight() + 6;
}

function mouseoverBox (d, self) {
  d3.select(self).select('rect').style('stroke', foreground);
}

function mouseleaveBox (d, self) {
  d3.select(self).select('rect').style('stroke', background);
}

// apply foreground color for outermost level and black for the rest
// to compensate for opacity changes
function fillBoxText (d) {
  return d.depth === 1 ? foreground : 'black';
}

// sankey functions ------------------------------------------------------- //
function getSankeyWidth () {
  return getWindowWidth();
}

function getSankeyHeight () {
  return getWindowHeight() + 6;
}

// common functions -------------------------------------------------------- //
// a hierarchy without its "Other" nodes
function withoutOther (node) {
  if (!node.children) { return node; }
  return { ...node, children: node.children.filter(c => !c.other).map(withoutOther) };
}

const SLICE_LABEL_PX = 7; // ~width of a label character

// a slice's label: "Other" is translated; a label runs along the radius, so
// it is cut to what fits across its ring, and a slice too thin for a line
// of text gets none
function sliceLabel (d, t) {
  if (!d.depth || (d.y0 + d.y1) / 2 * (d.x1 - d.x0) < 15) { return ''; }
  const label = d.data.other ? t('sessions.summary.other') : d.data.name;
  return fitLabel(label, d.y1 - d.y0 - 8, SLICE_LABEL_PX);
}

// color based on largest parent
function fillColor (d) {
  // a pie's "Other" (what falls outside a level's top values) is no value of
  // the field, so it and anything under it is gray rather than a palette color
  if (isUnderOther(d)) { return OTHER_COLOR; }
  while (d.depth > 1) { d = d.parent; }
  return colors(d.data.name);
}

// Vue component ----------------------------------------------------------- //
export default {
  name: 'ArkimePie',
  emits: ['fetchedResults', 'toggleLoad', 'toggleError'],
  components: {
    ArkimeFieldTypeahead,
    Popup,
    DragList,
    SummaryBarChart,
    SummaryChartTooltip
  },
  props: {
    spiGraphType: {
      type: String,
      default: ''
    },
    // bar: 'stacked' | 'grouped' layout of the second field's values
    barMode: {
      type: String,
      default: 'stacked'
    },
    // pie: 'nested' rings, or 'split' into one pie per value of the first field
    pieMode: {
      type: String,
      default: 'nested'
    },
    // pie: add an "Other" slice per level for what falls outside its top values
    showOther: {
      type: Boolean,
      default: false
    },
    // table: the fields' value combinations ('intersection'), or each field's
    // own top values ('unique')
    tableMode: {
      type: String,
      default: 'intersection'
    },
    // split pie: how many pies (the first field's top values)
    pieCount: {
      type: Number,
      default: 5
    },
    baseField: {
      type: String,
      default: ''
    },
    graphData: {
      type: Array,
      default: () => []
    },
    fields: {
      type: Array,
      default: () => []
    },
    query: {
      type: Object,
      default: () => ({})
    }
  },
  data: function () {
    return {
      tableData: [],
      uniqueColumns: [], // unique table: one { exp, values } per field
      outerData: false,
      tableSortType: 'size',
      tableSortField: 0,
      tableDesc: true,
      fieldTypeaheadList: [],
      baseFieldObj: undefined,
      vizData: undefined,
      sankeyData: undefined,
      hiddenColumns: false,
      popupInfo: undefined,
      barWidth: 800,
      barHeight: 500,
      barTooltip: { visible: false, data: null, position: { x: 0, y: 0 }, percentage: null, split: null }
    };
  },
  async mounted () {
    // set colors to match the background
    background = themedColor('background', '#FFFFFF');
    foreground = themedColor('foreground', '#333333');

    this.baseFieldObj = this.getFieldObj(this.baseField);

    this.syncFieldsFromRoute();
    this.trimBarFields();

    d3 = await import('d3'); // lazy load d3 to avoid loading it on every page
    const { sankey: d3sankey, sankeyLinkHorizontal: d3sankeyLinkHorizontal } = await import('d3-sankey');
    sankey = d3sankey;
    sankeyLinkHorizontal = d3sankeyLinkHorizontal;
    this.loadData();

    // resize the pie with the window
    window.addEventListener('resize', this.resize);
    this.sizeBar();
    // close info popup if the user presses escape
    window.addEventListener('keyup', this.closeInfoOnEsc);
    document.addEventListener('click', this.closeBarTooltipOnOutsideClick);
  },
  watch: {
    graphData: function (newVal, oldVal) {
      this.baseFieldObj = this.getFieldObj(this.baseField);
      // if there is more data to fetch than the spigraph component can provide
      // (more fields, or a pie's "Other"), fetch it from the hierarchy endpoint
      if (this.fieldTypeaheadList.length || this.wantsOther) {
        this.loadData();
      } else {
        const data = this.formatDataFromSpigraph(newVal);
        this.applyGraphData(data);
      }
    },
    spiGraphType: function (newVal, oldVal) {
      this.closeInfo();
      this.barTooltip.visible = false;
      // a unique table fetches no hierarchy, so there may be nothing to redraw
      // yet (its watcher's refetch draws the new type)
      if (this.vizData) { this.applyGraphData(this.vizData); }
      if (newVal === 'table') {
        this.initializeColResizable();
      }
      if (newVal === 'bar') {
        this.$nextTick(this.sizeBar);
        // sankey keeps its own node/link shape; the bar needs the hierarchy
        if (!this.vizData?.children) { this.loadData(); }
      }
      // only the pie fetches "Other", and a split pie's first level is cut to
      // its pie count: switching to one, or away from a split, refetches
      if ((newVal === 'pie' && (this.showOther || this.splitPie)) || (oldVal === 'pie' && this.splitPie)) { this.loadData(); }
    },
    // trims once the url says bar: trimming on the prop change would push a
    // url built before the graph type change landed and undo it. The fields
    // come from the url first: a loaded graph changes both at once, and this
    // watcher runs before the subFields one
    '$route.query.spiGraphType': function () {
      this.syncFieldsFromRoute();
      this.trimBarFields();
    },
    '$route.query.subFields': function (newVal, oldVal) {
      this.syncFieldsFromRoute();
      this.loadData();
    },
    pieMode: function () {
      this.closeInfo();
      // a split pie's first level is cut to its pie count, so the layouts
      // need different data; with one field there is nothing to split
      if (this.fieldTypeaheadList.length) {
        this.loadData();
      } else if (this.vizData) { // nothing to lay out until the first fetch has landed
        this.applyGraphData(this.vizData);
      }
    },
    pieCount: function () {
      if (this.spiGraphType === 'pie' && this.splitPie) { this.loadData(); }
    },
    showOther: function (show) {
      this.closeInfo();
      // turning it off on a single-field pie needs no fetch: the spigraph
      // data already holds the top values (the graphData watcher's own path)
      if (!show && !this.fieldTypeaheadList.length && this.graphData && !init) {
        this.applyGraphData(this.formatDataFromSpigraph(this.graphData));
      } else {
        this.loadData();
      }
    },
    // the unique table has its own data: switching to or from it refetches
    uniqueTable: function () {
      this.loadData();
    },
    // Resize svg height after toggle is updated and mounted()
    showToolBars: function () {
      this.$nextTick(() => {
        this.resize();
      });
    }
  },
  computed: {
    // Boolean in the store will remember chosen toggle state for all pages
    showToolBars: function () {
      return this.$store.state.showToolBars;
    },
    fieldList: function () {
      const fieldList = [this.baseFieldObj];
      return fieldList.concat(this.fieldTypeaheadList);
    },
    // the hierarchy as the bar chart wants it: one item per first-field
    // value, with the second field's values (if any) as its split
    barData: function () {
      const children = (this.vizData ? withoutOther(this.vizData).children : null) || [];
      const split = this.fieldTypeaheadList.length > 0;
      // a value with children has its count in sizeValue (size is cleared)
      const count = (node) => node.sizeValue ?? node.size;
      return children.map(c => ({
        item: c.name,
        sessions: count(c),
        value: count(c),
        ...(split ? { split: (c.children || []).map(s => ({ item: s.name, sessions: count(s), value: count(s) })) } : {})
      }));
    },
    barSplitField: function () {
      return this.fieldTypeaheadList.length ? this.fieldTypeaheadList[0] : null;
    },
    // only a pie draws "Other": its parts add up to a whole
    wantsOther: function () {
      return this.spiGraphType === 'pie' && this.showOther;
    },
    // a split pie: one pie per value of the first field, of the fields after it
    splitPie: function () {
      return this.pieMode === 'split' && this.fieldTypeaheadList.length > 0;
    },
    // a unique table: with one field it's the same as the intersection
    uniqueTable: function () {
      return this.spiGraphType === 'table' && this.tableMode === 'unique' && this.fieldTypeaheadList.length > 0;
    },
    uniqueRowCount: function () {
      return Math.max(0, ...this.uniqueColumns.map(c => c.values.length));
    }
  },
  methods: {
    commaString,
    /* exposed page functions ---------------------------------------------- */
    /**
     * Adds an expression to the search expression input box
     * @param {Object} slice  The pie slice data
     * @param {String} op     The operator to apply to the search expression ('||' or '&&')
     */
    addExpression: function (slice, op) {
      const fullExpression = `${slice.field} == ${slice.name}`;
      this.$store.commit('addToExpression', {
        expression: fullExpression, op
      });
    },
    /**
     * Removes a field from the field typeahead list and loads the data
     * @param {Number} index The index of the field typeahead list to remove
     */
    removeField: function (index) {
      this.fieldTypeaheadList.splice(index, 1);
      this.applyFieldListToUrl();
    },
    /**
     * Reorders the fields from the field typeahead list and loads the data
     * @param {Array} list The reordered list
     */
    reorderFields: function (list) {
      this.fieldTypeaheadList = list;
      this.applyFieldListToUrl();
    },
    /**
     * Fired when a second level field typeahead field is selected
     * @param {Object} field The field the add to the pie graph
     */
    changeField: function (field) {
      this.fieldTypeaheadList.push(field);
      this.applyFieldListToUrl();
    },
    /**
     * Fired when a column header is clicked
     * Sets the sort field and type vars and initiates sorting the table data
     * @param {Number} sort The index of the data to sort by (0 = biggest bucket)
     * @param {String} type The type to sort by (value or name)
     */
    columnClick: function (sort, type) {
      // if the sort field and type is the same, toggle it,
      // otherwise set it to default (true)
      this.tableDesc = (this.tableSortField === sort && this.tableSortType === type)
        ? !this.tableDesc
        : true;
      this.tableSortType = type;
      this.tableSortField = sort;
      this.sortTable();
    },
    hideColumn: function (col) {
      col.hide = true;
      this.hiddenColumns = true;
      this.initializeColResizable();
    },
    showHiddenColumns: function () {
      for (const field of this.fieldList) {
        field.hide = false;
      }
      this.hiddenColumns = false;
      this.initializeColResizable();
    },
    /* event functions ----------------------------------------------------- */
    /** Fits the bar chart to the content area */
    sizeBar: function () {
      this.barWidth = Math.max(300, getWindowWidth());
      this.barHeight = Math.max(300, getWindowHeight());
    },
    /**
     * A bar is split by at most one more field, so drop any others carried
     * over from another graph type
     * @returns {Boolean} true when fields were dropped (the url change reloads)
     */
    trimBarFields: function () {
      // the url's type: on back/forward the prop catches up a tick later
      const type = this.$route.query.spiGraphType || this.spiGraphType;
      if (type !== 'bar' || this.fieldTypeaheadList.length <= 1) { return false; }
      this.fieldTypeaheadList.splice(1);
      this.applyFieldListToUrl();
      return true;
    },
    /**
     * The url is the source of truth for the extra fields: a loaded saved
     * graph (or back/forward) changes it without going through changeField.
     * A field this viewer doesn't know (eg a hand-edited url) is skipped
     */
    syncFieldsFromRoute: function () {
      const exps = String(this.$route.query.subFields || '').split(',').filter(Boolean);
      if (exps.join(',') !== this.fieldTypeaheadList.map(f => f?.exp).join(',')) {
        this.fieldTypeaheadList = exps.map(exp => this.getFieldObj(exp)).filter(Boolean);
      }
    },
    closeBarTooltipOnOutsideClick: function (e) {
      if (this.barTooltip.visible && !e.target.closest('.summary-tooltip')) {
        this.barTooltip.visible = false;
      }
    },
    showBarTooltip: function (evt) {
      this.barTooltip = {
        visible: true,
        data: evt.data,
        position: evt.position,
        percentage: evt.percentage ?? null,
        split: evt.split || null
      };
    },
    /**
     * Resizes the pie graph after the window is resized
     * Waits for the window resize to stop for .5 sec
     */
    resize: function () {
      if (resizeTimer) { clearTimeout(resizeTimer); }
      resizeTimer = setTimeout(() => {
        this.sizeBar();
        // recalculate from the live scroll container
        width = getWindowWidth();
        height = getWindowHeight();
        radius = getRadius();

        // set the new width and height of the pie
        d3.select('#pie-area svg')
          .attr('viewBox', `${-width / 2} ${-height / 2} ${width} ${height}`)
          .attr('width', width)
          .attr('height', height)
          .select('g');

        // recalculate width and height
        treemapWidth = getTreemapWidth();
        treemapHeight = getTreemapHeight();

        // set the new width and height of the treemap
        d3.select('#treemap-area svg')
          .attr('width', treemapWidth)
          .attr('height', treemapHeight)
          .attr('transform', `translate(${treemapMargin},${treemapMargin})`)
          .select('g');

        sankeyWidth = getSankeyWidth();
        sankeyHeight = getSankeyHeight();
        d3.select('#sankey-area svg')
          .attr('width', sankeyWidth)
          .attr('height', sankeyHeight)
          .attr('transform', `translate(${sankeyMargin.left},${sankeyMargin.top})`)
          .select('g');

        // just rerender the pie graph (seems like the only way)
        if (this.vizData) { this.applyGraphData(this.vizData); }
      }, 500);
    },
    /* helper functions ---------------------------------------------------- */
    /**
     * Adds the field exps of the subfields to the url
     */
    applyFieldListToUrl: function () {
      const subFieldExps = [];
      for (const field of this.fieldTypeaheadList) {
        subFieldExps.push(field.exp);
      }

      let subFieldExpsString = subFieldExps.join(',');
      if (!subFieldExpsString) { subFieldExpsString = undefined; }

      this.$router.push({
        query: {
          ...this.$route.query,
          subFields: subFieldExpsString
        }
      });
    },
    /**
     * Sorts the table data based on the existing sort field and type vars
     */
    sortTable: function () {
      this.tableData.sort((a, b) => {
        let result = -1;
        if (this.tableSortField === this.fieldList.length - 1) {
          // sort on child field
          if (!this.tableDesc) {
            result = a[this.tableSortType] > b[this.tableSortType];
          } else {
            result = b[this.tableSortType] > a[this.tableSortType];
          }
        } else {
          // sort on one of the parent fields
          if (!this.tableDesc) {
            result = a.parents[this.tableSortField][this.tableSortType] > b.parents[this.tableSortField][this.tableSortType];
          } else {
            result = b.parents[this.tableSortField][this.tableSortType] > a.parents[this.tableSortField][this.tableSortType];
          }
        }
        return result ? 1 : -1;
      });
    },
    /**
     * Turn the data array into an object and only preserve necessary info
     * This is only needed when data is coming from the spigraph loadData func
     * (key = data name, count = data value, idx = index to be added to the pie)
     * Also adds data to the tableData array
     * @param {Array} data The data array the format
     * @returns {Object} formattedData The formatted data object
     */
    formatDataFromSpigraph: function (data) {
      const formattedData = {
        name: 'Top Talkers',
        children: []
      };

      for (const item of data) {
        const dataObj = {
          name: item.name,
          size: item.count,
          field: this.baseField
        };
        formattedData.children.push(dataObj);
      }

      this.tableData = formattedData.children;
      this.applyColorsToTableData(this.tableData);
      this.sortTable();

      if (this.spiGraphType === 'sankey') {
        return this.formatDataFromSpigraphSankey(formattedData);
      }

      return formattedData;
    },
    /**
     * Turn the data array into Sankey format
     * @param {Array} data The data array to format
     * @returns {Object} formattedData The formatted data object
     */
    formatDataFromSpigraphSankey: function (hierarchicalData) {
      const { nodes, links } = hierarchyToSankey(hierarchicalData);
      for (const node of nodes) { // the popup resolves a node's field by name
        node.field = this.fieldList[node.depth] ? this.fieldList[node.depth].exp : this.baseField;
      }
      return { nodes, links };
    },
    /**
     * Generates a list of colors (RAINBOW) based on the length of the data
     * @param {Number} dataLength The length of the data to calculate colors for
     * @returns {Function} colors Function to retrieve a color per data point
     */
    generateColors: function (dataLength) {
      return d3.scaleOrdinal(
        d3.quantize(d3.interpolateRainbow, dataLength + 1)
      );
    },
    /**
     * Generates a list of opacity levels based on the levels of data
     * @param {Object} data The data to display in the treemap
     * @returns {Function} opacity Function to retrieve opacity based on data level
     */
    generateOpacity: function (data) {
      let levelCount = 0;

      while (data.children || data[0].children) {
        levelCount++;
        data = data.children || data[0].children;
      }

      const lowRange = levelCount > 1 ? 0.3 : 1;

      return d3.scaleLinear().domain([1, levelCount]).range([lowRange, 1]);
    },
    /**
     * Adds a color variable to every table data item using the outer bucket
     * @param {Object} data The data to generate the colors from
     */
    applyColorsToTableData: function (data) {
      const parentMap = {};
      for (const item of data) { // count top level parents
        if (item.parents && item.parents.length) {
          parentMap[item.parents[0].name] = true;
        } else {
          parentMap[item.name] = true;
        }
      }
      const parentCount = Object.keys(parentMap).length;
      const tableColors = this.generateColors(parentCount);
      for (const item of data) {
        let key = item.name;
        if (item.parents && item.parents.length) {
          key = item.parents[0].name;
        }
        item.color = tableColors(key);
      }
    },
    /**
     * Initializes the graph by adding the svg to the page once the component
     * has mounted and the pie area container is present.
     * @param {Object} data The data to construct the pie
     */
    initializeGraphs: function (data) {
      // size to the live scroll container before creating the svgs
      width = getWindowWidth();
      height = getWindowHeight();
      radius = getRadius();
      treemapWidth = getTreemapWidth();
      treemapHeight = getTreemapHeight();
      sankeyWidth = getSankeyWidth();
      sankeyHeight = getSankeyHeight();

      g = d3.select('#pie-area')
        .append('svg')
        .attr('viewBox', `${-width / 2} ${-height / 2} ${width} ${height}`)
        .attr('width', width)
        .attr('height', height)
        .call(d3.zoom().on('zoom', (e) => { // allow zooming of the pie graph
          g.attr('transform', e.transform);
        }))
        .append('g');

      gtree = d3.select('#treemap-area')
        .append('svg')
        .attr('width', treemapWidth)
        .attr('height', treemapHeight)
        .append('g')
        .attr('transform', `translate(${treemapMargin},${treemapMargin})`);

      gsankey = d3.select('#sankey-area')
        .append('svg')
        .attr('width', sankeyWidth)
        .attr('height', sankeyHeight)
        .append('g')
        .attr('transform', `translate(${sankeyMargin.left},${sankeyMargin.top})`);

      if (data) { this.applyGraphData(data); }
    },
    /**
     * Applies the graph data to the pie chart or tree map
     * @param {Object} data The data to add to the graph
     */
    applyGraphData: function (data) {
      // save viz data for resize and switching between viz types
      this.vizData = data;
      // a pie's "Other" was fetched for it alone: the other types never draw
      // it, and the page's csv export (handed the rows here) leaves it out,
      // as the server's table rows do
      const shown = data?.children ? withoutOther(data) : data;
      this.$emit('fetchedResults', shown.children, this.fieldTypeaheadList, this.baseFieldObj);
      if (this.spiGraphType !== 'pie') { data = shown; }
      if (this.spiGraphType === 'pie') {
        // a split needs a field to split into pies and one to draw in them
        if (this.splitPie) {
          this.applySplitPieGraphData(data);
        } else {
          this.applyPieGraphData(data);
        }
      } else if (this.spiGraphType === 'treemap') {
        this.applyTreemapGraphData(data);
      } else if (this.spiGraphType === 'sankey') {
        // Handle different data formats:
        // - If data has 'nodes' property, it's already formatted for sankey
        // - If data has 'children' property, it's hierarchical data that needs sankey formatting
        // - Otherwise, it should have been formatted in formatDataFromSpigraph
        let sankeyData = data;
        if (!data.nodes && data.children) {
          // This is hierarchical data from additional field requests
          sankeyData = this.formatDataFromSpigraphSankey(data);
        }
        this.applySankeyGraphData(sankeyData);
      }
    },
    /**
     * Applies the graph data to the pie chart by adding slices and text labels
     * It also adds the colors and transitions to the pie graph
     * (works for new a new pie as well as updating the pie)
     * @param {Object} hierarchyData The data to add to the graph
     */
    applyPieGraphData: function (hierarchyData) {
      const data = JSON.parse(JSON.stringify(hierarchyData)); // clone the data so we don't mutate it

      colors = this.generateColors(data.children.length);

      g.selectAll('g.split-pie').remove(); // a split pie drawn before

      const partition = d3.partition() // organize data into sunburst pattern
        .size([2 * Math.PI, radius]); // show sunburst in full circle

      const root = d3.hierarchy(data) // our data is hierarchical
        .sum((d) => { return d.size; }); // sum each node's children

      // combine partition var (data structure) with root node (the actual data)
      partition(root);

      // SLICES ------------------------------ //
      const slice = g.selectAll('g.node') // select all g elements with the node class
        .data(root.descendants(), (d) => { // pass in root variable with descendants
          return d.data.name;
        });

      newSlice = slice.enter() // connect the path element with our data
        .append('g') // add the g element to be fetched later when the data changes
        .attr('class', 'node') // apply the node class (again to fetch later)
        // merge the DOM elements of the new slices with the old slices
        // (because the data could have changed)
        .merge(slice);

      slice.exit().remove(); // remove any slices that are no longer needed
      slice.selectAll('path').remove(); // remove all old slice path elements
      slice.selectAll('text').remove(); // and their labels

      this.drawSlices(newSlice);
    },
    /**
     * Draws each slice of a selection of g.node elements (bound to partition
     * nodes): its path, its hover highlight and popup, and its label. The
     * nested pie and a split pie's small pies draw their slices the same way.
     * @param {Object} slices The d3 selection of g.node elements
     * @param {Function} infoNode The node the popup describes for a slice: a
     *   split pie lays its slices out on a copy, so it points back at the
     *   slice's place in the whole tree (the popup reads its field values up
     *   its parents)
     */
    drawSlices: function (slices, infoNode = (d) => d) {
      const vueSelf = this;

      slices.append('path')
        .attr('display', (d) => { // don't display the root node
          return d.depth ? null : 'none';
        })
        .attr('d', arc) // set the d attribute on the paths for drawing each slice
        .style('stroke-width', '3px')
        .style('stroke', background) // lines between the slices
        .style('fill', fillColor); // apply the colors to the slices

      slices // hover functionality
        .on('mouseover', function (e, d) {
          mouseover(d, this);
          if (popupTimer) { clearTimeout(popupTimer); }
          popupTimer = setTimeout(() => {
            vueSelf.showInfo(infoNode(d));
          }, 400);
        })
        .on('mouseleave', function (e, d) {
          mouseleave(d, this);
          if (popupTimer) { clearTimeout(popupTimer); }
        });

      slices.append('text')
        .attr('transform', textTransform)
        .attr('text-anchor', 'middle')
        .attr('dy', '.35em')
        .text((d) => sliceLabel(d, this.$t));
    },
    /**
     * Applies the graph data as a split pie: one small pie per value of the
     * first field, each the sunburst of the fields after it. A slice's value
     * reads the same color in every pie.
     * @param {Object} hierarchyData The data to add to the graph
     */
    applySplitPieGraphData: function (hierarchyData) {
      const data = JSON.parse(JSON.stringify(hierarchyData)); // clone the data so we don't mutate it

      g.selectAll('*').remove(); // the nested pie, or the pies drawn before
      newSlice = undefined;

      // the hover popup reads a slice's field values up its parents, so the
      // slices keep their place in this whole tree
      const root = d3.hierarchy(data).sum((d) => { return d.size; });
      const layout = splitPieLayout(root, width, height, 20);
      if (!layout) { return; }
      colors = this.generateColors(layout.sliceCount);
      const pieRadius = Math.max(layout.radius - 4, 1); // a gap between the pies
      const cellWidth = layout.cellW;

      layout.cells.forEach(({ pie, cx, cy }) => {
        // the svg's viewBox is centered on the origin
        const cell = g.append('g')
          .attr('class', 'split-pie')
          .attr('transform', `translate(${cx - width / 2},${cy - height / 2})`);

        cell.append('text') // the split value, above its pie: cut to its cell, whole in the tooltip
          .attr('y', -pieRadius - 6)
          .attr('text-anchor', 'middle')
          .style('fill', foreground)
          .style('font-weight', 'bold')
          .text(fitLabel(pie.data.name, cellWidth - 8, 7))
          .append('title').text(pie.data.name);

        // lay the pie out on a copy rooted at it (partition sizes rings by
        // depth from the root), each copied node pointing back at its slice
        const node = pie.copy();
        const slices = pie.descendants();
        node.descendants().forEach((d, j) => { d.slice = slices[j]; });
        d3.partition().size([2 * Math.PI, pieRadius])(node.sum((d) => { return d.size; }));

        const pieSlice = cell.selectAll('g.node')
          .data(node.descendants().filter(d => d.depth))
          .enter()
          .append('g')
          .attr('class', 'node');
        this.drawSlices(pieSlice, (d) => d.slice);
      });
    },
    /**
     * Applies the graph data to the treemap by adding boxes and text labels
     * It also adds the colors to the treemap
     * (works for new a new treemap as well as updating the treemap)
     * @param {Object} hierarchyData The data to add to the graph
     */
    applyTreemapGraphData: function (hierarchyData) {
      const data = JSON.parse(JSON.stringify(hierarchyData)); // clone the data so we don't mutate it

      const vueSelf = this;
      colors = this.generateColors(data.children.length);

      const opacity = this.generateOpacity(data);

      const treeRoot = d3.hierarchy(data) // our data is hierarchical
        .sum((d) => { return d.size; }); // sum each node's children

      const treemap = d3.treemap() // organize data into treemap
        .size([treemapWidth - (treemapMargin * 2), treemapHeight - (treemapMargin * 2)])
        // root label is hidden, so reserve no top band for it
        .paddingTop((d) => d.depth === 0 ? 2 : 18)
        .paddingLeft(2)
        .paddingRight(2)
        .paddingBottom(2)
        .paddingInner(3) // padding between children
        .round(true); // round the width/height numbers

      // combine treemap var (data structure) with the root node (the actual data)
      treemap(treeRoot);

      // BOXES ------------------------------- //
      const box = gtree.selectAll('g.box') // select all g elements with the box class
        .data(treeRoot.descendants()); // pass the root variable with descendants

      newBox = box.enter() // connect the path element with our data
        .append('g') // add the g element to be fetched later when the data changes
        .attr('class', 'box') // apply the box class (again to fetch later)
        // merge the DOM elements of the new boxes with the old boxes
        // (because the data could have changed)
        .merge(box);

      box.exit().remove(); // remove any boxes that are no longer needed
      box.selectAll('rect').remove(); // remove all old box path elements

      newBox.append('rect') // add new box path elements
        // apply an id to the box so we can reference it with our clip path element
        .attr('id', (d) => 'rect' + getUid(d))
        .attr('display', (d) => { // don't display the root node
          return d.depth ? null : 'none';
        })
        .attr('x', (d) => { return d.x0; }) // x position of the box calculated by treemap
        .attr('y', (d) => { return d.y0; }) // y position of the box calculated by treemap
        .attr('width', (d) => { return d.x1 - d.x0; }) // width of the box calculated by treemap
        .attr('height', (d) => { return d.y1 - d.y0; }) // height of the box calculated by treemap
        .attr('stroke', background) // outline each box
        .attr('stroke-width', '2px')
        .style('fill', fillColor) // apply the colors to the boxes
        .style('opacity', (d) => {
          // change the opacity of the box colors based on depth
          return opacity(d.depth);
        });

      newBox.append('clipPath') // add a clipPath element to clip the inner text
        .attr('id', (d) => 'clip' + getUid(d)) // id to reference by the text element
        .append('use') // add use element to reference the box size
        .attr('xlink:href', (d) => '#rect' + getUid(d));

      newBox // hover functionality
        .on('mouseover', function (e, d) {
          mouseoverBox(d, this);
          if (popupTimer) { clearTimeout(popupTimer); }
          popupTimer = setTimeout(() => {
            vueSelf.showInfo(d);
          }, 400);
        })
        .on('mouseleave', function (e, d) {
          mouseleaveBox(d, this);
          if (popupTimer) { clearTimeout(popupTimer); }
        });

      // TEXT -------------------------------- //
      box.selectAll('text').remove(); // remove any text that is no longer needed

      // show title (name)
      newBox.append('text') // add text element for box name
        // reference the clipPath element that references the box dimensions
        // so that the text doesn't overflow the box
        .attr('clip-path', (d) => 'url(#clip' + getUid(d) + ')')
        .attr('dx', (d) => { return d.x0 + 2; }) // adjust left position
        .attr('dy', (d) => { return d.y0 + 13; }) // adjust top position
        .attr('fill', fillBoxText) // color the test
        .style('font-size', '.85rem')
        .style('font-weight', 'bold')
        .text((d) => { return d.data.name; }); // show the box name

      // show value (size)
      newBox.append('text') // add text element for box size
        // reference the clipPath element that references the box dimensions
        // so that the text doesn't overflow the box
        .attr('clip-path', (d) => 'url(#clip' + getUid(d) + ')')
        .attr('dx', (d) => { return d.x0 + 2; }) // adjust left position
        .attr('dy', (d) => { return d.y0 + 25; }) // adjust top position
        .attr('fill', fillBoxText) // color the font
        .style('font-size', '.85rem') // make it a little smaller than the name
        .text((d) => { // show the box size
          if (d.children) { return; } // only show size for leaf nodes
          return commaString(d.data.size);
        });
    },
    /**
     * Applies the graph data to the sankey diagram
     * @param {Object} data The data to add to the graph
     */
    applySankeyGraphData: function (data) {
      // save sankey data for resize
      this.sankeyData = JSON.parse(JSON.stringify(data)); // clone the data so we don't mutate it

      // update the spigraph page with results
      this.$emit('fetchedResults', data.tableResults || data.nodes || [], this.fieldTypeaheadList, this.baseFieldObj);

      if (!data || !data.nodes || !data.nodes.length) {
        return;
      }

      const vueSelf = this;
      colors = this.generateColors(data.nodes.length);

      // Create sankey generator
      const sankeyLayout = sankey()
        .nodeId(d => d.id)
        .nodeWidth(15)
        .nodePadding(10)
        .extent([[1, 1], [sankeyWidth - sankeyMargin.left - sankeyMargin.right - 1, sankeyHeight - sankeyMargin.top - sankeyMargin.bottom - 2]]);

      // Generate the sankey layout
      const graph = sankeyLayout(data);

      // Clear existing elements
      gsankey.selectAll('*').remove();

      // Add links
      const links = gsankey.append('g')
        .attr('stroke', '#000')
        .attr('fill', 'none')
        .selectAll('path')
        .data(graph.links)
        .enter()
        .append('path')
        .attr('d', sankeyLinkHorizontal())
        .attr('stroke', d => colors(d.source.name))
        .attr('stroke-width', d => Math.max(1, d.width))
        .attr('opacity', 0.6);

      // Add nodes
      const nodes = gsankey.append('g')
        .selectAll('rect')
        .data(graph.nodes)
        .enter()
        .append('rect')
        .attr('x', d => d.x0)
        .attr('y', d => d.y0)
        .attr('height', d => d.y1 - d.y0)
        .attr('width', d => d.x1 - d.x0)
        .attr('fill', d => colors(d.name))
        .attr('stroke', foreground)
        .attr('stroke-width', 0.5);

      // Add node labels
      const labels = gsankey.append('g')
        .style('font', '10px sans-serif')
        .selectAll('text')
        .data(graph.nodes)
        .enter()
        .append('text')
        .attr('x', d => d.x0 < sankeyWidth / 2 ? d.x1 + 6 : d.x0 - 6)
        .attr('y', d => (d.y1 + d.y0) / 2)
        .attr('dy', '0.35em')
        .attr('text-anchor', d => d.x0 < sankeyWidth / 2 ? 'start' : 'end')
        .text(d => d.name)
        .style('fill', foreground);

      // add popup functionality for nodes
      nodes.on('mouseover', function (e, d) {
        if (popupTimer) { clearTimeout(popupTimer); }
        popupTimer = setTimeout(() => {
          // Transform sankey node data to match popup component expectations
          const transformedData = vueSelf.transformSankeyNodeForPopup(d);
          vueSelf.showInfo(transformedData);
        }, 400);
      }).on('mouseleave', function () {
        if (popupTimer) { clearTimeout(popupTimer); }
      });

      // add popup functionality for links
      links.on('mouseover', function (e, d) {
        if (popupTimer) { clearTimeout(popupTimer); }
        popupTimer = setTimeout(() => {
          // Transform sankey node data to match popup component expectations
          // use the source node here for the link popup
          const transformedData = vueSelf.transformSankeyNodeForPopup(d.source);
          vueSelf.showInfo(transformedData);
        }, 400);
      }).on('mouseleave', function () {
        if (popupTimer) { clearTimeout(popupTimer); }
      });

      // add popup functionality for labels
      labels.on('mouseover', function (e, d) {
        if (popupTimer) { clearTimeout(popupTimer); }
        popupTimer = setTimeout(() => {
          const transformedData = vueSelf.transformSankeyNodeForPopup(d);
          vueSelf.showInfo(transformedData);
        }, 400);
      }).on('mouseleave', function () {
        if (popupTimer) { clearTimeout(popupTimer); }
      });
    },
    /**
     * Gets a field object based on an exp
     * @param {String} exp      The exp of the field to retrieve
     * @returns {Object} field  The field that matches the exp or undefined if not found
     */
    getFieldObj: function (exp) {
      for (const field of this.fields) {
        if (field.exp === exp) {
          return field;
        }
      }
      return undefined;
    },
    async loadData () {
      this.$emit('toggleLoad', true);
      this.$emit('toggleError', '');

      // create unique cancel id to make cancel req for corresponding es task
      const cancelId = Utils.createRandomString();

      // one fetch at a time: a request still in flight (two watchers firing
      // for one change, say) is dropped for this one
      abortPending('A newer request replaced this one');

      // setup the query params
      const params = this.query;
      params.cancelId = cancelId;

      const exps = [this.baseField];

      for (const field of this.fieldTypeaheadList) {
        exps.push(field.exp);
      }

      params.exp = exps.toString(',');
      params.other = this.wantsOther || undefined;
      params.mode = this.uniqueTable ? 'unique' : undefined;
      // a split pie draws pieCount pies of size slices: a size per level
      params.sizes = this.spiGraphType === 'pie' && this.splitPie
        ? [this.pieCount, ...this.fieldTypeaheadList.map(() => params.size)].join(',')
        : undefined;

      let controller;
      try {
        let fetcher;
        ({ controller, fetcher } = SpigraphService.getHierarchy(params));
        pendingPromise = { controller, cancelId };

        const response = await fetcher; // do the fetch
        if (response.error) {
          throw new Error(response.error);
        }

        if (init) {
          init = false;
          if (!this.fieldTypeaheadList.length) {
            // just use spigraph data if there are no additional levels of fields to display
            this.initializeGraphs(this.formatDataFromSpigraph(this.graphData));
          } else { // otherwise load the data for the additional fields
            this.initializeGraphs();
          }
        }
        arc = getArc();
        pendingPromise = null;
        this.$emit('toggleLoad', false);
        if (response.columns) { // unique: no hierarchy to draw
          this.applyUniqueColumns(response.columns);
          return;
        }
        this.uniqueColumns = [];
        this.applyGraphData(response.hierarchicalResults);
        this.tableData = response.tableResults;
        this.sortTable();
        this.applyColorsToTableData(response.tableResults);
        this.showHiddenColumns(); // initializes resizable cols
        this.$emit('fetchedResults', response.tableResults, this.fieldTypeaheadList, this.baseFieldObj);
      } catch (error) {
        // aborted for a newer request or the page closing: that one reports.
        // Anything else (the fetch failing, or drawing what it returned)
        // reports here
        if (controller?.signal.aborted) { return; }
        if (pendingPromise?.cancelId === cancelId) { pendingPromise = null; }
        this.$emit('toggleLoad', false);
        this.$emit('toggleError', resolveMessage(error, this.$t));
      }
    },
    /**
     * Shows a unique table, and hands the page its rows for the csv
     * export: one per rank, in the intersection's { parents, name, size }
     * shape, with a blank for a field that has fewer values
     * @param {Array} columns One { exp, values } per field, in field order
     */
    applyUniqueColumns: function (columns) {
      this.uniqueColumns = columns;
      // the hierarchy and its rows are from before this fetch (other fields,
      // maybe): switching to a chart redraws nothing until its own fetch lands
      this.vizData = undefined;
      this.tableData = [];
      const blank = { name: '', size: '' };
      const rows = [];
      for (let r = 0; r < this.uniqueRowCount; r++) {
        const cells = columns.map(c => c.values[r] || blank);
        const last = cells.pop();
        rows.push({ parents: cells, name: last.name, size: last.size });
      }
      this.$emit('fetchedResults', rows, this.fieldTypeaheadList, this.baseFieldObj);
    },
    initializeColResizable: function () {
      this.destroyColResizable();
      this.$nextTick(() => {
        this._gripAttachment = attachTableGrips({
          cols: document.getElementsByClassName('col-header'),
          table: this.$refs.table,
          minWidth: MIN_COL_WIDTH
        });
      });
    },
    destroyColResizable () {
      if (this._gripAttachment) {
        this._gripAttachment.detach();
        this._gripAttachment = null;
      }
    },
    /**
     * Transforms sankey node data to match the popup component expectations
     * @param {Object} sankeyNode The sankey node data from d3
     * @returns {Object} Transformed data for popup component
     */
    transformSankeyNodeForPopup: function (sankeyNode) {
      // Find the path to the selected node in vizData
      const findNodePath = (node, targetName, currentPath = []) => {
        const nPath = [...currentPath, node];

        if (node.name === targetName) {
          return nPath;
        }

        if (node.children && node.children.length > 0) {
          for (const child of node.children) {
            const result = findNodePath(child, targetName, nPath);
            if (result) {
              return result;
            }
          }
        }

        return null;
      };

      // Get the path from root to the selected node
      const nodePath = findNodePath(this.vizData, sankeyNode.name);

      if (!nodePath) {
        // Fallback if node not found - create a simple d3.hierarchy-like structure
        return {
          data: {
            name: sankeyNode.name,
            size: sankeyNode.value,
            srcips: sankeyNode.sourceLinks ? sankeyNode.sourceLinks.length : 0,
            dstips: sankeyNode.targetLinks ? sankeyNode.targetLinks.length : 0,
            sizeValue: sankeyNode.value
          },
          depth: 1,
          parent: null
        };
      }

      // Create d3.hierarchy-like structure from the node path
      // This mimics what d3.hierarchy() creates for pie/treemap
      const createHierarchyNode = (pathNodes, currentIndex) => {
        const currentNodeData = pathNodes[currentIndex];

        return {
          data: currentNodeData, // The original node data
          depth: currentIndex,
          parent: currentIndex === 0 ? null : createHierarchyNode(pathNodes, currentIndex - 1)
        };
      };

      // Start from the deepest node (selected node) and build the hierarchy
      const hierarchyNode = createHierarchyNode(nodePath, nodePath.length - 1);

      return hierarchyNode;
    },
    /**
     * Displays the information about a pie slice
     * Note: must be here and not top level so that this.$refs works
     * @param {Object} d The pie slice data
     */
    showInfo: function (d) {
      this.popupInfo = d;
    },
    closeInfo () {
      this.popupInfo = undefined;
    },
    closeInfoOnEsc (e) {
      if (e.key === 'Escape') { // esc
        this.popupInfo = undefined;
      }
    }
  },
  beforeUnmount () {
    abortPending('Closing the page canceled the request');

    // remove listeners
    window.removeEventListener('resize', this.resize);
    window.removeEventListener('keyup', this.closeInfoOnEsc);
    document.removeEventListener('click', this.closeBarTooltipOnOutsideClick);

    // remove elements
    if (newSlice) {
      // d3 doesn't have .off function to remove listeners,
      // so use .on('listener', null)
      newSlice
        .on('mouseover', null)
        .on('mouseleave', null);
      newSlice.exit().remove();
      newSlice.selectAll('path').remove();
      newSlice.selectAll('text').remove();
    }
    if (newBox) {
      // d3 doesn't have .off function to remove listeners,
      // so use .on('listener', null)
      newBox
        .on('mouseover', null)
        .on('mouseleave', null);
      newBox.exit().remove();
      newBox.selectAll('rect').remove();
      newBox.selectAll('text').remove();
      newBox.selectAll('clipPath').remove();
    }

    // destroy child component
    $('.info-popup').remove();
    if (popupVue) { popupVue.unmount(); }

    // cleanup global vars
    setTimeout(() => {
      init = true;
      g = undefined;
      gtree = undefined;
      colors = undefined;
      newBox = undefined;
      newSlice = undefined;
      popupVue = undefined;
      background = undefined;
      foreground = undefined;
      popupTimer = undefined;
      resizeTimer = undefined;
    });
  }
};
</script>

<style>
/* not scoped: teleported into the spigraph sub-navbar */
/* widen the "Add another field" typeahead so its placeholder isn't clipped */
.subfield-group {
  flex: 0 1 auto;
}
.subfield-group input {
  min-width: 280px;
}

/* styling for the lines connecting the labels to the slices
   make sure they are lines, not triangles (no fill) */
.spigraph-pie polyline {
  fill: none;
  opacity: .3;
  stroke: black;
  stroke-width: 2px;
}

/* this needs to not be scoped because it's a child component */
/* pie slice data popup */
.spigraph-pie div.pie-popup {
  right: 5px;
  z-index: 9;
  min-width: 220px;
  position: absolute;
  overflow: visible;
  white-space: nowrap;
}

/* add a color swatch to the big bucket table cells */
.spigraph-pie .color-swatch {
  width: 18px;
  height: 18px;
  border-radius: 4px;
  float: right;
  margin-top: 4px;
}

.col-header {
  position: relative;
}

/* cell borders + hover */
.spigraph-table {
  border-collapse: collapse;
  width: 100%;
  color: rgb(var(--v-theme-foreground));
}
.spigraph-table > thead > tr > th,
.spigraph-table > tbody > tr > td,
.spigraph-table > tbody > tr > th {
  border: 1px solid rgb(var(--v-theme-neutral-light));
  padding: 0.25rem;
}
.spigraph-table > tbody > tr:hover > td,
.spigraph-table > tbody > tr:hover > th {
  background-color: rgb(var(--v-theme-neutral-lighter));
}

/* make sure field dropdowns are visible in the table */
.spigraph-table > tbody > tr > td,
.spigraph-table > tbody > tr > th {
  overflow: visible;
}
</style>
