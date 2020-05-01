<template>
  <div class="spigraph-pie">

    <!-- field select -->
    <div class="form-inline pl-1 position-absolute">
      <div class="form-group"
        v-if="fields && fields.length">
        <div class="input-group input-group-sm mr-2">
          <span class="input-group-prepend cursor-help"
            v-b-tooltip.hover
            title="SPI Graph Field">
            <span class="input-group-text">
              Add another field:
            </span>
          </span>
          <moloch-field-typeahead
            :fields="fields"
            @fieldSelected="changeField"
            page="SpigraphSubfield">
          </moloch-field-typeahead>
        </div>
      </div>
      <template v-for="(field, index) in fieldTypeaheadList">
        <label class="badge badge-secondary mr-1 mb-1 help-cursor"
          :title="field.help"
          v-b-tooltip.hover
          v-if="field"
          :key="`${field.dbField}-${index}`">
          {{ field.friendlyName }}
          <button class="close ml-2"
            style="margin-top:-8px"
            @click="removeField(index)">
            <span>&times;</span>
          </button>
        </label>
      </template>
    </div> <!-- /field select -->

    <!-- info area -->
    <span ref="infoPopup">
      <div class="pie-popup">
      </div>
    </span> <!-- /info area -->

    <!-- pie chart area -->
    <div id="pie-area"
      v-show="spiGraphType === 'pie' && vizData && vizData.children.length">
    </div>
    <!-- /pie chart area -->

    <!-- treemap area -->
    <div id="treemap-area" class="pt-3"
      v-show="spiGraphType === 'treemap' && vizData && vizData.children.length">
    </div>
    <!-- /treemap area -->

    <!-- table area -->
    <div v-show="spiGraphType === 'table' && tableData.length"
      class="container mt-2 pt-5">
      <table class="table table-bordered table-condensed table-sm">
        <thead>
          <tr>
            <th colspan="2">
              {{ getFieldObj(baseField).friendlyName }}
            </th>
            <th v-if="fieldTypeaheadList.length > 0"
              colspan="2">
              {{ fieldTypeaheadList[0].friendlyName }}
            </th>
            <th v-if="fieldTypeaheadList.length === 2"
              colspan="2">
              {{ fieldTypeaheadList[1].friendlyName }}
            </th>
          </tr>
          <tr>
            <template v-if="!fieldTypeaheadList.length">
              <th class="cursor-pointer"
                @click="columnClick('child', 'name')">
                Value
                <span v-show="tableSortField === 'child' && tableSortType === 'name' && !tableDesc"
                  class="fa fa-sort-asc ml-2">
                </span>
                <span v-show="tableSortField === 'child' && tableSortType === 'name' && tableDesc"
                  class="fa fa-sort-desc ml-2">
                </span>
                <span v-show="tableSortField !== 'child' || tableSortType !== 'name'"
                  class="fa fa-sort ml-2">
                </span>
              </th>
              <th class="cursor-pointer"
                @click="columnClick('child', 'size')">
                Count
                <span v-show="tableSortField === 'child' && tableSortType === 'size' && !tableDesc"
                  class="fa fa-sort-asc ml-2">
                </span>
                <span v-show="tableSortField === 'child' && tableSortType === 'size' && tableDesc"
                  class="fa fa-sort-desc ml-2">
                </span>
                <span v-show="tableSortField !== 'child' || tableSortType !== 'size'"
                  class="fa fa-sort ml-2">
                </span>
              </th>
            </template>
            <template v-if="fieldTypeaheadList.length === 1">
              <th class="cursor-pointer"
                @click="columnClick('parent', 'name')">
                Value
                <span v-show="tableSortField === 'parent' && tableSortType === 'name' && !tableDesc"
                  class="fa fa-sort-asc ml-2">
                </span>
                <span v-show="tableSortField === 'parent' && tableSortType === 'name' && tableDesc"
                  class="fa fa-sort-desc ml-2">
                </span>
                <span v-show="tableSortField !== 'parent' || tableSortType !== 'name'"
                  class="fa fa-sort ml-2">
                </span>
              </th>
              <th class="cursor-pointer"
                @click="columnClick('parent', 'size')">
                Count
                <span v-show="tableSortField === 'parent' && tableSortType === 'size' && !tableDesc"
                  class="fa fa-sort-asc ml-2">
                </span>
                <span v-show="tableSortField === 'parent' && tableSortType === 'size' && tableDesc"
                  class="fa fa-sort-desc ml-2">
                </span>
                <span v-show="tableSortField !== 'parent' || tableSortType !== 'size'"
                  class="fa fa-sort ml-2">
                </span>
              </th>
              <th class="cursor-pointer"
                @click="columnClick('child', 'name')">
                Value
                <span v-show="tableSortField === 'child' && tableSortType === 'name' && !tableDesc"
                  class="fa fa-sort-asc ml-2">
                </span>
                <span v-show="tableSortField === 'child' && tableSortType === 'name' && tableDesc"
                  class="fa fa-sort-desc ml-2">
                </span>
                <span v-show="tableSortField !== 'child' || tableSortType !== 'name'"
                  class="fa fa-sort ml-2">
                </span>
              </th>
              <th class="cursor-pointer"
                @click="columnClick('child', 'size')">
                Count
                <span v-show="tableSortField === 'child' && tableSortType === 'size' && !tableDesc"
                  class="fa fa-sort-asc ml-2">
                </span>
                <span v-show="tableSortField === 'child' && tableSortType === 'size' && tableDesc"
                  class="fa fa-sort-desc ml-2">
                </span>
                <span v-show="tableSortField !== 'child' || tableSortType !== 'size'"
                  class="fa fa-sort ml-2">
                </span>
              </th>
            </template>
            <template v-if="fieldTypeaheadList.length === 2">
              <th class="cursor-pointer"
                @click="columnClick('grandparent', 'name')">
                Value
                <span v-show="tableSortField === 'grandparent' && tableSortType === 'name' && !tableDesc"
                  class="fa fa-sort-asc ml-2">
                </span>
                <span v-show="tableSortField === 'grandparent' && tableSortType === 'name' && tableDesc"
                  class="fa fa-sort-desc ml-2">
                </span>
                <span v-show="tableSortField !== 'grandparent' || tableSortType !== 'name'"
                  class="fa fa-sort ml-2">
                </span>
              </th>
              <th class="cursor-pointer"
                @click="columnClick('grandparent', 'size')">
                Count
                <span v-show="tableSortField === 'grandparent' && tableSortType === 'size' && !tableDesc"
                  class="fa fa-sort-asc ml-2">
                </span>
                <span v-show="tableSortField === 'grandparent' && tableSortType === 'size' && tableDesc"
                  class="fa fa-sort-desc ml-2">
                </span>
                <span v-show="tableSortField !== 'grandparent' || tableSortType !== 'size'"
                  class="fa fa-sort ml-2">
                </span>
              </th>
              <th class="cursor-pointer"
                @click="columnClick('parent', 'name')">
                Value
                <span v-show="tableSortField === 'parent' && tableSortType === 'name' && !tableDesc"
                  class="fa fa-sort-asc ml-2">
                </span>
                <span v-show="tableSortField === 'parent' && tableSortType === 'name' && tableDesc"
                  class="fa fa-sort-desc ml-2">
                </span>
                <span v-show="tableSortField !== 'parent' || tableSortType !== 'name'"
                  class="fa fa-sort ml-2">
                </span>
              </th>
              <th class="cursor-pointer"
                @click="columnClick('parent', 'size')">
                Count
                <span v-show="tableSortField === 'parent' && tableSortType === 'size' && !tableDesc"
                  class="fa fa-sort-asc ml-2">
                </span>
                <span v-show="tableSortField === 'parent' && tableSortType === 'size' && tableDesc"
                  class="fa fa-sort-desc ml-2">
                </span>
                <span v-show="tableSortField !== 'parent' || tableSortType !== 'size'"
                  class="fa fa-sort ml-2">
                </span>
              </th>
              <th class="cursor-pointer"
                @click="columnClick('child', 'name')">
                Value
                <span v-show="tableSortField === 'child' && tableSortType === 'name' && !tableDesc"
                  class="fa fa-sort-asc ml-2">
                </span>
                <span v-show="tableSortField === 'child' && tableSortType === 'name' && tableDesc"
                  class="fa fa-sort-desc ml-2">
                </span>
                <span v-show="tableSortField !== 'child' || tableSortType !== 'name'"
                  class="fa fa-sort ml-2">
                </span>
              </th>
              <th class="cursor-pointer"
                @click="columnClick('child', 'size')">
                Count
                <span v-show="tableSortField === 'child' && tableSortType === 'size' && !tableDesc"
                  class="fa fa-sort-asc ml-2">
                </span>
                <span v-show="tableSortField === 'child' && tableSortType === 'size' && tableDesc"
                  class="fa fa-sort-desc ml-2">
                </span>
                <span v-show="tableSortField !== 'child' || tableSortType !== 'size'"
                  class="fa fa-sort ml-2">
                </span>
              </th>
            </template>
          </tr>
        </thead>
        <tbody>
          <template v-if="fieldTypeaheadList.length">
            <template v-for="(item, key) in tableData">
              <tr :key="key" v-if="item.grandparent">
                <td>
                  <moloch-session-field
                    :field="baseFieldObj"
                    :value="item.grandparent.name"
                    :expr="baseFieldObj.exp"
                    :parse="true"
                    :session-btn="true">
                  </moloch-session-field>
                  <span class="color-swatch"
                    :style="{ backgroundColor: item.color }">
                  </span>
                </td>
                <td>
                  {{ item.grandparent.size | commaString }}
                </td>
                <td>
                  <moloch-session-field
                    :field="fieldTypeaheadList[0]"
                    :value="item.parent.name"
                    :expr="fieldTypeaheadList[0].exp"
                    :parse="true"
                    :session-btn="true">
                  </moloch-session-field>
                </td>
                <td>
                  {{ item.parent.size | commaString }}
                </td>
                <td>
                  <moloch-session-field
                    v-if="fieldTypeaheadList[1]"
                    :field="fieldTypeaheadList[1]"
                    :value="item.name"
                    :expr="fieldTypeaheadList[1].exp"
                    :parse="true"
                    :session-btn="true">
                  </moloch-session-field>
                </td>
                <td>
                  {{ item.size | commaString }}
                </td>
              </tr>
              <tr :key="key" v-else-if="item.parent">
                <td>
                  <moloch-session-field
                    :field="baseFieldObj"
                    :value="item.parent.name"
                    :expr="baseFieldObj.exp"
                    :parse="true"
                    :session-btn="true">
                  </moloch-session-field>
                  <span class="color-swatch"
                    :style="{ backgroundColor: item.color }">
                  </span>
                </td>
                <td>
                  {{ item.parent.size | commaString }}
                </td>
                <td>
                  <moloch-session-field
                    v-if="fieldTypeaheadList[0]"
                    :field="fieldTypeaheadList[0]"
                    :value="item.name"
                    :expr="fieldTypeaheadList[0].exp"
                    :parse="true"
                    :session-btn="true">
                  </moloch-session-field>
                </td>
                <td>
                  {{ item.size | commaString }}
                </td>
              </tr>
            </template>
          </template>
          <template v-else
            v-for="(item, key) in tableData">
            <tr :key="key">
              <td>
                <span class="color-swatch"
                  :style="{ backgroundColor: item.color }">
                </span>
                <moloch-session-field
                  :field="baseFieldObj"
                  :value="item.name"
                  :expr="baseFieldObj.exp"
                  :parse="true"
                  :session-btn="true">
                </moloch-session-field>
              </td>
              <td>
                {{ item.size | commaString }}
              </td>
            </tr>
          </template>
        </tbody>
      </table>
    </div>
    <!-- /table area -->

    <!-- no results -->
    <moloch-no-results
      v-if="!tableData.length"
      class="mt-5 mb-5"
      :view="query.view">
    </moloch-no-results> <!-- /no results -->

  </div>
</template>

<script>
// import external
import Vue from 'vue';
import * as d3 from 'd3';
// import services
import SpigraphService from './SpigraphService';
// import internal
import MolochNoResults from '../utils/NoResults';
import MolochFieldTypeahead from '../utils/FieldTypeahead';
// import utils
import Utils from '../utils/utils';

// common page variables --------------------------------------------------- //
let pendingPromise; // save a pending promise to be able to cancel it
let popupVue; // vue component to mount when showing pie slice information
let popupTimer; // timer to debounce pie slice info popup events
let resizeTimer; // timer to debounce resizing the pie graph on window resize
let colors; // colors function to apply colors to treemap boxes and pie slices
let background; // color of app background
let foreground; // color of app foreground

// page pie variables ------------------------------------------------------ //
let g, newSlice;
let width = getWindowWidth();
let height = getWindowHeight();
let radius = getRadius();
let arc = getArc();

// page treemap variables -------------------------------------------------- //
let gtree, newBox;
const treemapMargin = 10;
let treemapWidth = getTreemapWidth();
let treemapHeight = getTreemapHeight();

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
    id += `-${d.data.name.replace(/\s/g, '')}`;
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

// apply foreground color for outer most level and black for the rest
// to compensate for opacity changes
function fillBoxText (d) {
  return d.depth === 1 ? foreground : 'black';
}

// common functions -------------------------------------------------------- //
// close popups helper
function closeInfo () {
  if (popupVue) { popupVue.$destroy(); }
  popupVue = undefined;
  $('.pie-popup').hide();
}

// close popup on escape press
function closeInfoOnEsc (keyCode) {
  if (event.keyCode === 27) { // esc
    closeInfo();
  }
}

// color based on largest parent
function fillColor (d) {
  while (d.depth > 1) { d = d.parent; }
  return colors(d.data.name);
}

// Vue component ----------------------------------------------------------- //
export default {
  name: 'MolochPie',
  components: { MolochNoResults, MolochFieldTypeahead },
  props: {
    spiGraphType: String,
    baseField: String,
    graphData: Array,
    fields: Array,
    query: Object
  },
  data: function () {
    return {
      tableData: [],
      outerData: false,
      tableSortType: 'size',
      tableSortField: 'child',
      tableDesc: true,
      closeInfo: closeInfo,
      fieldTypeaheadList: [],
      baseFieldObj: undefined,
      vizData: undefined
    };
  },
  mounted: function () {
    // set colors to match the background
    const styles = window.getComputedStyle(document.body);
    background = styles.getPropertyValue('--color-background').trim() || '#FFFFFF';
    foreground = styles.getPropertyValue('--color-foreground').trim() || '#333333';

    this.baseFieldObj = this.getFieldObj(this.baseField);

    if (this.$route.query.subFields) {
      let subFieldExps = this.$route.query.subFields.split(',');
      for (let exp of subFieldExps) {
        this.fieldTypeaheadList.push(this.getFieldObj(exp));
      }
    }

    if (!this.fieldTypeaheadList.length) {
      // just use spigraph data if there are no additional levels of fields to display
      this.initializeGraphs(this.formatDataFromSpigraph(this.graphData));
    } else { // otherwise load the data for the additional fields
      this.initializeGraphs();
      this.loadData();
    }

    // resize the pie with the window
    window.addEventListener('resize', this.resize);
    // close info popup if the user presses escape
    window.addEventListener('keyup', closeInfoOnEsc);
  },
  watch: {
    'graphData': function (newVal, oldVal) {
      // if there is more data to fetch than the spigraph component can provide,
      // fetch it from the spigraphpie endpoint
      if (this.fieldTypeaheadList.length) {
        this.loadData();
      } else {
        let data = this.formatDataFromSpigraph(newVal);
        this.applyGraphData(data);
      }
    },
    'spiGraphType': function (newVal, oldVal) {
      this.applyGraphData(this.vizData);
    },
    '$route.query.subFields': function (newVal, oldVal) {
      this.loadData();
    },
    // Resize svg height after toggle is updated and mounted()
    'showToolBars': function () {
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
  },
  methods: {
    /* exposed page functions ---------------------------------------------- */
    /**
     * Adds an expression to the search expression input box
     * @param {Object} slice  The pie slice data
     * @param {String} op     The operator to apply to the search expression ('||' or '&&')
     */
    addExpression: function (slice, op) {
      let fullExpression = `${slice.field} == ${slice.name}`;
      this.$store.commit('addToExpression', {
        expression: fullExpression, op: op
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
     * Fired when a second level field typeahead field is selected
     * @param {Object} field The field the add to the pie graph
     */
    changeField: function (field) {
      // only allow max 2 items in this array
      if (this.fieldTypeaheadList.length > 1) {
        this.$set(this.fieldTypeaheadList, 1, field);
      } else {
        this.fieldTypeaheadList.push(field);
      }

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
        ? !this.tableDesc : true;
      this.tableSortType = type;
      this.tableSortField = sort;
      this.sortTable();
    },
    /* event functions ----------------------------------------------------- */
    /**
     * Resizes the pie graph after the window is resized
     * Waits for the window resize to stop for .5 sec
     */
    resize: function () {
      // 36px for navbar + 25px for footer = 61px.
      //const height = $(window).height() - (toolbarDown ? 171 : 61);

      if (resizeTimer) { clearTimeout(resizeTimer); }
      resizeTimer = setTimeout(() => {
        // recalculate width, height, and radius
        width = getWindowWidth();
        // re-add the header space if collapsed
        height = getWindowHeight() + (this.showToolBars? 0 : 113);
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

        // just rerender the pie graph (seems like the only way)
        this.applyGraphData(this.vizData);
      }, 500);
    },
    /* helper functions ---------------------------------------------------- */
    /**
     * Adds the field exps of the subfields to the url
     */
    applyFieldListToUrl: function () {
      let subFieldExps = [];
      for (let field of this.fieldTypeaheadList) {
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
        let result = false;
        if (this.tableSortField === 'child') {
          if (!this.tableDesc) {
            result = a[this.tableSortType] > b[this.tableSortType];
          } else {
            result = b[this.tableSortType] > a[this.tableSortType];
          }
        } else {
          if (!this.tableDesc) {
            result = a[this.tableSortField][this.tableSortType] > b[this.tableSortField][this.tableSortType];
          } else {
            result = b[this.tableSortField][this.tableSortType] > a[this.tableSortField][this.tableSortType];
          }
        }
        return result ? 1 : -1;
      });
    },
    /**
     * Turn the data array into an object and only preserve neceessary info
     * This is only needed when data is coming from the spigraph loadData func
     * (key = data name, count = data value, idx = index to be added to the pie)
     * Also adds data to the tableData array
     * @param {Array} data The data array the format
     * @returns {Object} formattedData The formatted data object
     */
    formatDataFromSpigraph: function (data) {
      let formattedData = {
        name: 'Top Talkers',
        children: []
      };

      for (let item of data) {
        let dataObj = {
          name: item.name,
          size: item.count,
          field: this.baseField
        };
        formattedData.children.push(dataObj);
      }

      this.tableData = formattedData.children;
      this.applyColorsToTableData(this.tableData);
      this.sortTable();

      return formattedData;
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

      let lowRange = levelCount > 1 ? 0.3 : 1;

      return d3.scaleLinear().domain([1, levelCount]).range([lowRange, 1]);
    },
    /**
     * Adds a color variable to every table data item using the outer bucket
     * @param {Object} data The data to generate the colors from
     */
    applyColorsToTableData: function (data) {
      let parentMap = {};
      for (let item of data) {
        if (item.grandparent && !parentMap[item.grandparent.name]) { // count grandparents
          parentMap[item.grandparent.name] = true;
        } else if (!item.grandparent && item.parent && !parentMap[item.parent.name]) { // count parents
          parentMap[item.parent.name] = true;
        } else if (!item.grandparent && !item.parent && !parentMap[item.name]) {
          parentMap[item.name] = true;
        }
      }
      let parentCount = Object.keys(parentMap).length;
      let tableColors = this.generateColors(parentCount);
      for (let item of data) {
        let key = item.name;
        if (item.grandparent) {
          key = item.grandparent.name;
        } else if (item.parent) {
          key = item.parent.name;
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
        .call(d3.zoom().on('zoom', () => { // allow zooming of the pie graph
          g.attr('transform', d3.event.transform);
        }))
        .append('g');

      gtree = d3.select('#treemap-area')
        .append('svg')
        .attr('width', treemapWidth)
        .attr('height', treemapHeight)
        .append('g')
        .attr('transform', `translate(${treemapMargin},${treemapMargin})`);

      if (data) { this.applyGraphData(data); }
    },
    /**
     * Applies the graph data to the pie chart or tree map
     * @param {Object} data The data to add to the graph
     */
    applyGraphData: function (data) {
      // save viz data for resize and switching between viz types
      this.vizData = data;
      if (this.spiGraphType === 'pie') {
        this.applyPieGraphData(data);
      } else if (this.spiGraphType === 'treemap') {
        this.applyTreemapGraphData(data);
      }
    },
    /**
     * Applies the graph data to the pie chart by adding slices and text labels
     * It also adds the colors and transitions to the pie graph
     * (works for new a new pie as well as updating the pie)
     * @param {Object} data The data to add to the graph
     */
    applyPieGraphData: function (data) {
      let vueSelf = this;
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
        .on('mouseover', function (d) {
          mouseover(d, this);
          if (popupTimer) { clearTimeout(popupTimer); }
          popupTimer = setTimeout(() => {
            vueSelf.showInfo(d);
          }, 400);
        })
        .on('mouseleave', function (d) {
          closeInfo();
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
          let name = d.parent ? d.data.name : '';
          if (name.length > 8) {
            name = name.substr(0, 8) + '...';
          }
          return name;
        });
    },
    /**
     * Applies the graph data to the treemap by adding boxes and text labels
     * It also adds the colors to the treemap
     * (works for new a new treemap as well as updating the treemap)
     * @param {Object} data The data to add to the graph
     */
    applyTreemapGraphData: function (data) {
      let vueSelf = this;
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
        .on('mouseover', function (d) {
          mouseoverBox(d, this);
          if (popupTimer) { clearTimeout(popupTimer); }
          popupTimer = setTimeout(() => {
            vueSelf.showInfo(d);
          }, 400);
        })
        .on('mouseleave', function (d) {
          closeInfo();
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
          return this.$options.filters.commaString(d.data.size);
        });
    },
    /**
     * Gets a field object based on an exp
     * @param {String} exp      The exp of the field to retrieve
     * @returns {Object} field  The field that matches the exp or undefined if not found
     */
    getFieldObj: function (exp) {
      for (let field of this.$parent.fields) {
        if (field.exp === exp) {
          return field;
        }
      }
      return undefined;
    },
    loadData: function () {
      this.$emit('toggleLoad', true);
      this.$emit('toggleError', '');

      // create unique cancel id to make canel req for corresponding es task
      const cancelId = Utils.createRandomString();
      this.query.cancelId = cancelId;

      const source = Vue.axios.CancelToken.source();
      const cancellablePromise = SpigraphService.getHierarchy(this.query, source.token);

      // setup the query params
      let params = this.query;
      let exps = [ this.baseField ];

      for (let field of this.fieldTypeaheadList) {
        exps.push(field.exp);
      }

      params.exp = exps.toString(',');

      // set pending promise info so it can be cancelled
      pendingPromise = { cancellablePromise, source, cancelId };

      cancellablePromise.then((response) => {
        pendingPromise = null;
        this.$emit('toggleLoad', false);
        this.applyGraphData(response.data.hierarchicalResults);
        this.tableData = response.data.tableResults;
        this.sortTable();
        this.applyColorsToTableData(this.tableData);
      }).catch((error) => {
        pendingPromise = null;
        this.$emit('toggleLoad', false);
        this.$emit('toggleError', error.text || error);
      });
    },
    /**
     * Displays the information about a pie slice
     * Note: must be here and not top level so that this.$refs works
     * @param {Object} d The pie slice data
     */
    showInfo: function (d) {
      closeInfo(); // close open info section
      // create the vue template
      if (!popupVue) {
        popupVue = new Vue({
          template: `
            <div class="pie-popup">
              <table class="table table-borderless table-condensed table-sm">
                <thead>
                  <tr>
                    <th>
                      Field
                    </th>
                    <th>
                      Value
                    </th>
                    <th>
                      <a class="pull-right cursor-pointer no-decoration"
                        @click="closeInfo">
                        <span class="fa fa-close"></span>
                      </a>
                    </th>
                  </tr>
                </thead>
                <tbody>
                  <template v-if="level2FieldObj && sliceData.parent.parent && sliceData.parent.parent.data && sliceData.parent.parent.data.sizeValue">
                    <tr>
                      <td>
                        {{ level2FieldObj.friendlyName }}
                      </td>
                      <td>
                        <moloch-session-field
                          :field="level2FieldObj"
                          :value="sliceData.data.name"
                          :expr="level2FieldObj.exp"
                          :parse="true"
                          :session-btn="true">
                        </moloch-session-field>
                      </td>
                      <td>
                        <strong>
                          {{ sliceData.data.size || sliceData.data.sizeValue | commaString }}
                        </strong>
                      </td>
                    </tr>
                    <tr>
                      <td>
                        {{ level1FieldObj.friendlyName }}
                      </td>
                      <td>
                        <moloch-session-field
                          :field="level1FieldObj"
                          :value="sliceData.parent.data.name"
                          :expr="level1FieldObj.exp"
                          :parse="true"
                          :session-btn="true">
                        </moloch-session-field>
                      </td>
                      <td>
                        <strong>
                          {{ sliceData.parent.data.size || sliceData.parent.data.sizeValue | commaString }}
                        </strong>
                      </td>
                    </tr>
                    <tr>
                      <td>
                        {{ baseFieldObj.friendlyName }}
                      </td>
                      <td>
                        <moloch-session-field
                          :field="baseFieldObj"
                          :value="sliceData.parent.parent.data.name"
                          :expr="baseFieldObj.exp"
                          :parse="true"
                          :session-btn="true">
                        </moloch-session-field>
                      </td>
                      <td>
                        <strong>
                          {{ sliceData.parent.parent.data.sizeValue }}
                        </strong>
                      </td>
                    </tr>
                  </template>
                  <template v-else-if="level1FieldObj && sliceData.parent.data && sliceData.parent.data.sizeValue">
                    <tr>
                      <td>
                        {{ level1FieldObj.friendlyName }}
                      </td>
                      <td>
                        <moloch-session-field
                          :field="level1FieldObj"
                          :value="sliceData.data.name"
                          :expr="level1FieldObj.exp"
                          :parse="true"
                          :session-btn="true">
                        </moloch-session-field>
                      </td>
                      <td>
                        <strong>
                          {{ sliceData.data.size || sliceData.data.sizeValue | commaString }}
                        </strong>
                      </td>
                    </tr>
                    <tr>
                      <td>
                        {{ baseFieldObj.friendlyName }}
                      </td>
                      <td>
                        <moloch-session-field
                          :field="baseFieldObj"
                          :value="sliceData.parent.data.name"
                          :expr="baseFieldObj.exp"
                          :parse="true"
                          :session-btn="true">
                        </moloch-session-field>
                      </td>
                      <td>
                        <strong>
                          {{ sliceData.parent.data.sizeValue }}
                        </strong>
                      </td>
                    </tr>
                  </template>
                  <tr v-else-if="baseFieldObj">
                    <td>
                      {{ baseFieldObj.friendlyName }}
                    </td>
                    <td>
                      <moloch-session-field
                        :field="baseFieldObj"
                        :value="sliceData.data.name"
                        :expr="baseFieldObj.exp"
                        :parse="true"
                        :session-btn="true">
                      </moloch-session-field>
                    </td>
                    <td>
                      <strong>
                        {{ sliceData.data.size || sliceData.data.sizeValue | commaString }}
                      </strong>
                    </td>
                  </tr>
                </tbody>
              </table>
            </div>
          `,
          parent: this,
          data: {
            sliceData: d,
            baseFieldObj: this.getFieldObj(this.baseField),
            level1FieldObj: this.fieldTypeaheadList.length ? this.fieldTypeaheadList[0] : undefined,
            level2FieldObj: this.fieldTypeaheadList.length > 1 ? this.fieldTypeaheadList[1] : undefined
          },
          methods: {
            addExpression: function (slice, op) {
              this.$parent.addExpression(slice, op);
            },
            closeInfo: function () {
              this.$parent.closeInfo();
            }
          }
        }).$mount($(this.$refs.infoPopup)[0].firstChild);
      }
      // display the pie popup area
      $('.pie-popup').show();
    }
  },
  beforeDestroy: function () {
    if (pendingPromise) {
      pendingPromise.source.cancel();
      pendingPromise = null;
    }

    // remove listeners
    window.removeEventListener('resize', this.resize);
    window.removeEventListener('keyup', closeInfoOnEsc);

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
    if (popupVue) { popupVue.$destroy(); }

    // cleanup global vars
    setTimeout(() => {
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
  z-index: 9;
  position: absolute;
  right: 5px;
  max-height: 500px;
  display: none;
  padding: 4px 8px;
  max-width: 400px;
  min-width: 280px;
  border-radius: 4px;
  border: solid 1px var(--color-gray);
  background: var(--color-primary-lightest);
  overflow: visible;
  text-overflow: ellipsis;
}

/* add a color swatch to the big bucket table cells */
.spigraph-pie .color-swatch {
  width: 18px;
  height: 18px;
  border-radius: 4px;
  float: right;
  margin-top: 4px;
}
</style>
