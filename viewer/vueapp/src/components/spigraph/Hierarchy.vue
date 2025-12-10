<!--
Copyright Yahoo Inc.
SPDX-License-Identifier: Apache-2.0
-->
<template>
  <div class="spigraph-pie">
    <!-- field select -->
    <div
      class="d-flex flex-row ps-1"
      :class="{'position-absolute': !!tableData.length}">
      <div
        class="d-inline"
        v-if="fields && fields.length">
        <div class="input-group input-group-sm me-2">
          <span class="input-group-text">
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
    </div> <!-- /field select -->

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
      class="pt-3"
      v-show="spiGraphType === 'treemap' && vizData && vizData.children.length" />
    <!-- /treemap area -->

    <!-- sankey area -->
    <div
      id="sankey-area"
      class="pt-4"
      v-show="spiGraphType === 'sankey' && sankeyData && sankeyData.nodes && sankeyData.nodes.length" />
    <!-- /sankey area -->

    <!-- table area -->
    <div
      v-show="spiGraphType === 'table' && tableData.length && fieldList.length"
      class="m-1 pt-5">
      <table
        class="table-bordered table-hover spigraph-table"
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
                    class="pull-right cursor-pointer ms-2"
                    id="showHiddenColumns"
                    @click="showHiddenColumns">
                    <span class="fa fa-plus-square" />
                    <BTooltip target="showHiddenColumns"><span v-i18n-btip="'spigraph.'" /></BTooltip>
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
                <span
                  v-show="tableSortField === index && tableSortType === 'name' && !tableDesc"
                  class="fa fa-sort-asc ms-2" />
                <span
                  v-show="tableSortField === index && tableSortType === 'name' && tableDesc"
                  class="fa fa-sort-desc ms-2" />
                <span
                  v-show="tableSortField !== index || tableSortType !== 'name'"
                  class="fa fa-sort ms-2" />
              </th>
              <th
                class="cursor-pointer"
                :key="`${index}-size`"
                @click="columnClick(index, 'size')"
                v-if="item && !item.hide">
                {{ $t('spigraph.tableCount') }}
                <span
                  v-show="tableSortField === index && tableSortType === 'size' && !tableDesc"
                  class="fa fa-sort-asc ms-2" />
                <span
                  v-show="tableSortField === index && tableSortType === 'size' && tableDesc"
                  class="fa fa-sort-desc ms-2" />
                <span
                  v-show="tableSortField !== index || tableSortType !== 'size'"
                  class="fa fa-sort ms-2" />
                <a
                  @click="hideColumn(item)"
                  id="hideColumn"
                  class="pull-right ms-2"
                  v-if="index !== fieldList.length - 1">
                  <span class="fa fa-minus-square" />
                  <BTooltip target="hideColumn"><span v-i18n-btip="'spigraph.'" /></BTooltip>
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
                    <arkime-session-field
                      :field="fieldList[index]"
                      :value="parent.name"
                      :expr="fieldList[index].exp"
                      :parse="true"
                      :session-btn="true"
                      :pull-left="true" />
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
                <template v-else>
                  <arkime-session-field
                    :field="fieldList[0]"
                    :value="item.name"
                    :expr="fieldList[0].exp"
                    :parse="true"
                    :session-btn="true"
                    :pull-left="true" />
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
  </div>
</template>

<script>
// import services
import SpigraphService from './SpigraphService';
// import internal
import ArkimeFieldTypeahead from '../utils/FieldTypeahead.vue';
import Popup from './Popup.vue';
import DragList from '../utils/DragList.vue';
// import utils
import Utils from '../utils/utils';
import { commaString } from '@common/vueFilters.js';

let d3; // lazy load d3
let sankey, sankeyLinkHorizontal; // lazy load d3-sankey

let init = true;

// common page variables --------------------------------------------------- //
let pendingPromise; // save a pending promise to be able to cancel it
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
const treemapMargin = 10;
let treemapWidth = getTreemapWidth();
let treemapHeight = getTreemapHeight();

// page sankey variables -------------------------------------------------- //
let gsankey;
const sankeyMargin = { top: 10, right: 10, bottom: 10, left: 10 };
let sankeyWidth = getSankeyWidth();
let sankeyHeight = getSankeyHeight();

// column resize variables ------------------------------------------------- //
let selectedColElem; // store selected column to watch drag and calculate new column width
let colStartOffset; // store column offset width to calculate new column width
let colWidthBeforeResize; // sore column width before resize to calculate diff
let tableWidthBeforeResize; // store table width before column resize to add to col resize diff
let table; // store table element to update its width after column resize
let cols; // store cols to add grip event handlers and save new widths
let selectedGripElem; // store the grip to style it while resizing column

// column resize functions ------------------------------------------------- //
// fired when a column resize grip is clicked
// stores values for calculations when the grip is unclicked
function gripClick (e, col) {
  e.preventDefault();
  e.stopPropagation();
  selectedColElem = col;
  colWidthBeforeResize = col.style.width.slice(0, -2);
  tableWidthBeforeResize = table.style.width.slice(0, -2);
  colStartOffset = col.offsetWidth - e.pageX;
  selectedGripElem = col.getElementsByClassName('grip')[0];
};

// fired when the column resize grip is dragged
// styles the grip to show where it's being dragged
function gripDrag (e) { // move the grip where the user moves their cursor
  if (selectedColElem && selectedGripElem) {
    const newWidth = colStartOffset + e.pageX;
    selectedGripElem.style.borderLeft = '1px dotted var(--color-gray)';
    selectedGripElem.style.left = `${newWidth}px`;
  }
}

// fired when a clicked and dragged grip is dropped
// updates the column and table width and saves the values
function gripUnclick (e, vueThis) {
  if (selectedColElem && selectedGripElem) {
    const newWidth = Math.max(colStartOffset + e.pageX, 70); // min col width is 70px
    selectedColElem.style.width = `${newWidth}px`;

    // update the width of the table. need to do this or else the table
    // cannot overflow its container
    const diff = newWidth - colWidthBeforeResize;
    table.style.width = `${parseInt(tableWidthBeforeResize) + parseInt(diff)}px`;

    selectedGripElem.style.borderLeft = 'unset';
    selectedGripElem.style.left = 'unset';
  }

  selectedGripElem = undefined;
  selectedColElem = undefined;
}

// pie functions ----------------------------------------------------------- //
function getWindowWidth () {
  return window.innerWidth;
}

function getWindowHeight (toolbarDown = true) {
  return window.innerHeight - 184; // height - (footer + headers + padding)
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
  self.parentNode.appendChild(self);
  self.parentNode.parentNode.appendChild(self.parentNode);
  d3.select(self).select('path').style('stroke', foreground);
}

function mouseleave (d, self) {
  d3.select(self).select('path').style('stroke', background);
};

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
  return getWindowWidth() - (treemapMargin * 2);
}

function getTreemapHeight () {
  return getWindowHeight() - (treemapMargin * 2);
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
  return window.innerWidth - (sankeyMargin.left + sankeyMargin.right);
}

function getSankeyHeight () {
  return window.innerHeight - 200 - (sankeyMargin.top + sankeyMargin.bottom);
}

// common functions -------------------------------------------------------- //
// color based on largest parent
function fillColor (d) {
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
    DragList
  },
  props: {
    spiGraphType: {
      type: String,
      default: ''
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
      outerData: false,
      tableSortType: 'size',
      tableSortField: 0,
      tableDesc: true,
      fieldTypeaheadList: [],
      baseFieldObj: undefined,
      vizData: undefined,
      sankeyData: undefined,
      hiddenColumns: false,
      popupInfo: undefined
    };
  },
  async mounted () {
    // set colors to match the background
    const styles = window.getComputedStyle(document.body);
    background = styles.getPropertyValue('--color-background').trim() || '#FFFFFF';
    foreground = styles.getPropertyValue('--color-foreground').trim() || '#333333';

    this.baseFieldObj = this.getFieldObj(this.baseField);

    if (this.$route.query.subFields) {
      const subFieldExps = this.$route.query.subFields.split(',');
      for (const exp of subFieldExps) {
        this.fieldTypeaheadList.push(this.getFieldObj(exp));
      }
    }

    d3 = await import('d3'); // lazy load d3 to avoid loading it on every page
    const { sankey: d3sankey, sankeyLinkHorizontal: d3sankeyLinkHorizontal } = await import('d3-sankey');
    sankey = d3sankey;
    sankeyLinkHorizontal = d3sankeyLinkHorizontal;
    this.loadData();

    // resize the pie with the window
    window.addEventListener('resize', this.resize);
    // close info popup if the user presses escape
    window.addEventListener('keyup', this.closeInfoOnEsc);
  },
  watch: {
    graphData: function (newVal, oldVal) {
      this.baseFieldObj = this.getFieldObj(this.baseField);
      // if there is more data to fetch than the spigraph component can provide,
      // fetch it from the spigraphpie endpoint
      if (this.fieldTypeaheadList.length) {
        this.loadData();
      } else {
        const data = this.formatDataFromSpigraph(newVal);
        this.applyGraphData(data);
      }
    },
    spiGraphType: function (newVal, oldVal) {
      this.closeInfo();
      this.applyGraphData(this.vizData);
      if (newVal === 'table') {
        this.initializeColResizable();
      }
    },
    '$route.query.subFields': function (newVal, oldVal) {
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
    /**
     * Resizes the pie graph after the window is resized
     * Waits for the window resize to stop for .5 sec
     */
    resize: function () {
      if (resizeTimer) { clearTimeout(resizeTimer); }
      resizeTimer = setTimeout(() => {
        // recalculate width, height, and radius
        width = getWindowWidth();
        // re-add the header space if collapsed
        height = getWindowHeight() + (this.showToolBars ? 0 : 113);
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
        this.applyGraphData(this.vizData);
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
      const nodes = [];
      const links = [];
      const nodeMap = new Map();

      if (!hierarchicalData || !hierarchicalData.children) {
        return { nodes, links };
      }

      // First pass: calculate cumulative values for each node
      const calculateCumulativeValue = (node) => {
        if (!node.children || node.children.length === 0) {
          return node.size || 0;
        }

        const childSum = node.children.reduce((sum, child) => {
          return sum + calculateCumulativeValue(child);
        }, 0);

        // Use the larger of the node's own size or the sum of children
        return Math.max(node.size || 0, childSum);
      };

      // Traverse the hierarchical data and create nodes and links
      const traverse = (node, depth = 0, parentId = null, parentCumulativeValue = 0) => {
        const nodeId = `${node.name}_${depth}`;
        const cumulativeValue = calculateCumulativeValue(node);

        if (!nodeMap.has(nodeId)) {
          nodeMap.set(nodeId, {
            id: nodeId,
            name: node.name,
            value: cumulativeValue,
            depth: depth,
            field: this.fieldList[depth] ? this.fieldList[depth].exp : this.baseField
          });
          nodes.push(nodeMap.get(nodeId));
        }

        if (parentId && parentId !== nodeId) {
          // For the link value, use the cumulative value of this node
          // This ensures first-level links show full height
          links.push({
            source: parentId,
            target: nodeId,
            value: cumulativeValue
          });
        }

        if (node.children && node.children.length > 0) {
          node.children.forEach(child => {
            traverse(child, depth + 1, nodeId, cumulativeValue);
          });
        }
      };

      // Check if we have multiple levels (any child has children)
      const hasMultipleLevels = hierarchicalData.children.some(child =>
        child.children && child.children.length > 0
      );

      if (hasMultipleLevels) {
        // Skip the top level and start with children
        hierarchicalData.children.forEach(child => {
          traverse(child, 0);
        });
      } else {
        // Keep the top level if there's only one level
        traverse(hierarchicalData, 0);
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
      // update the spigraph page with results so export can find them
      this.$emit('fetchedResults', data.children, this.fieldTypeaheadList, this.baseFieldObj);
      if (this.spiGraphType === 'pie') {
        this.applyPieGraphData(data);
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

      const vueSelf = this;
      colors = this.generateColors(data.children.length);

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

      newSlice.append('path') // add new slice path elements
        .attr('display', (d) => { // don't display the root node
          return d.depth ? null : 'none';
        })
        .attr('d', arc) // set the d attribute on the paths for drawing each slice
        .style('stroke-width', '3px')
        .style('stroke', background) // lines between the slices
        .style('fill', fillColor); // apply the colors to the slices

      newSlice // hover functionality
        .on('mouseover', function (e, d) {
          mouseover(d, this);
          if (popupTimer) { clearTimeout(popupTimer); }
          popupTimer = setTimeout(() => {
            vueSelf.showInfo(d);
          }, 400);
        })
        .on('mouseleave', function (e, d) {
          mouseleave(d, this);
          if (popupTimer) { clearTimeout(popupTimer); }
        });

      // TEXT -------------------------------- //
      slice.selectAll('text').remove(); // remove any text that is no longer needed

      newSlice.append('text')
        .attr('transform', textTransform)
        .attr('dx', '-35')
        .attr('dy', '.35em')
        .text((d) => {
          // don't show text for tiny slices or the root node text
          if (d.depth && (d.y0 + d.y1) / 2 * (d.x1 - d.x0) < 15) { return ''; }
          // truncate long values
          let sliceName = d.parent ? d.data.name : '';
          if (sliceName.length > 8) {
            sliceName = sliceName.substr(0, 8) + '...';
          }
          return sliceName;
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
        .size([treemapWidth - 12, treemapHeight - 12])
        .paddingTop(18) // padding between children and parent
        .paddingLeft(8)
        .paddingRight(8)
        .paddingBottom(8)
        .paddingInner(4) // padding between children
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
        .extent([[1, 1], [sankeyWidth - sankeyMargin.left - sankeyMargin.right - 1, sankeyHeight - sankeyMargin.top - sankeyMargin.bottom - 6]]);

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

      // setup the query params
      const params = this.query;
      params.cancelId = cancelId;

      const exps = [this.baseField];

      for (const field of this.fieldTypeaheadList) {
        exps.push(field.exp);
      }

      params.exp = exps.toString(',');

      try {
        const { controller, fetcher } = SpigraphService.getHierarchy(params);
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
        this.applyGraphData(response.hierarchicalResults);
        this.tableData = response.tableResults;
        this.sortTable();
        this.applyColorsToTableData(response.tableResults);
        this.showHiddenColumns(); // initializes resizable cols
        this.$emit('fetchedResults', response.tableResults, this.fieldTypeaheadList, this.baseFieldObj);
      } catch (error) {
        pendingPromise = null;
        this.$emit('toggleLoad', false);
        this.$emit('toggleError', error.text || String(error));
      }
    },
    initializeColResizable: function () {
      this.destroyColResizable();

      this.$nextTick(() => {
        cols = document.getElementsByClassName('col-header');
        table = this.$refs.table;

        for (const col of cols) { // listen for grip dragging
          const grip = col.getElementsByClassName('grip')[0];
          if (grip) {
            grip.addEventListener('mousedown', (e) => gripClick(e, col));
          }
        }

        document.addEventListener('mousemove', gripDrag);
        const self = this;
        document.addEventListener('mouseup', (e) => gripUnclick(e, self));
      });
    },
    destroyColResizable () {
      if (!cols) return;

      for (const col of cols) { // remove all grip dragging listeners
        const grip = col.getElementsByClassName('grip')[0];
        if (grip) {
          grip.removeEventListener('mousedown', gripClick);
        }
      }

      // remove document listeners
      document.removeEventListener('mousemove', gripDrag);
      document.removeEventListener('mouseup', gripUnclick);

      cols = undefined;
      table = undefined;
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
    if (pendingPromise) {
      pendingPromise.controller.abort('Closing the page canceled the request');
      pendingPromise = null;
    }

    // remove listeners
    window.removeEventListener('resize', this.resize);
    window.removeEventListener('keyup', this.closeInfoOnEsc);

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

/* make sure field dropdowns are visible in the table */
.spigraph-table > tbody > tr > td,
.spigraph-table > tbody > tr > th {
  overflow: visible;
}
</style>
