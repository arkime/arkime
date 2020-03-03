<template>
  <div class="spigraph-pie">

    <!-- field select -->
    <div class="form-inline pl-1">
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

    <div v-show="spiGraphType === 'pie' && pieData && pieData.children.length">
      <!-- info area -->
      <div ref="infoPopup">
        <div class="pie-popup">
        </div>
      </div> <!-- /info area -->
      <!-- pie chart area -->
      <div id="pie-area">
      </div>
      <!-- /pie chart area -->
    </div>

    <div v-show="spiGraphType === 'treemap' && pieData && pieData.children.length">
      <!-- info area -->
      <div ref="infoPopup">
        <div class="pie-popup">
        </div>
      </div> <!-- /info area -->
      <!-- pie chart area -->
      <!-- TODO class="container-fluid mt-3 mb-3" -->
      <div id="treemap-area">
      </div>
      <!-- /pie chart area -->
    </div>

    <!-- table area -->
    <div v-show="spiGraphType === 'table' && tableData.length"
      class="container mt-4">
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
                  {{ item.grandparent.size }}
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
                  {{ item.parent.size }}
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
                  {{ item.size }}
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
                  {{ item.parent.size }}
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
                  {{ item.size }}
                </td>
              </tr>
            </template>
          </template>
          <template v-else
            v-for="item in tableData">
            <tr :key="item.name">
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
                {{ item.size }}
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
import SpigraphService from '../spigraph/SpigraphService';
// import internal
import MolochNoResults from '../utils/NoResults';
import MolochFieldTypeahead from '../utils/FieldTypeahead';
// import utils
import Utils from '../utils/utils';

let pendingPromise; // save a pending promise to be able to cancel it
let popupVue; // vue component to mount when showing pie slice information
let popupTimer; // timer to debounce pie slice info popup events
let resizeTimer; // timer to debounce resizing the pie graph on window resize

// page pie variables ------------------------------------------------------ //
let g, gtree, newSlice, styles, background, foreground;
let width = getWidth();
let height = getHeight();
let radius = getRadius();
let arc = getArc();

// pie functions ----------------------------------------------------------- //
function getWidth () {
  return window.innerWidth;
}

function getHeight () {
  return window.innerHeight - 225; // height - (footer + headers + padding)
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
      pieData: undefined
    };
  },
  mounted: function () {
    // set colors to match the background
    styles = window.getComputedStyle(document.body);
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
      this.initializeGraph(this.formatDataFromSpigraph(this.graphData));
    } else { // otherwise load the data for the additional fields
      this.initializeGraph();
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
    '$route.query.subFields': function (newVal, oldVal) {
      this.loadData();
    }
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
      if (resizeTimer) { clearTimeout(resizeTimer); }
      resizeTimer = setTimeout(() => {
        // recalculate width, height, and radius
        width = getWidth();
        height = getHeight();
        radius = getRadius();

        // set the new width and height of the pie
        d3.select('#pie-area svg')
          .attr('viewBox', `${-width / 2} ${-height / 2} ${width} ${height}`)
          .attr('width', width)
          .attr('height', height)
          .select('g');

        // just rerender the pie graph (seems like the only way)
        this.applyGraphData(this.pieData);
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
      let colors = this.generateColors(parentCount);
      for (let item of data) {
        let key = item.name;
        if (item.grandparent) {
          key = item.grandparent.name;
        } else if (item.parent) {
          key = item.parent.name;
        }
        item.color = colors(key);
      }
    },
    /**
     * Initializes the graph by adding the svg to the page once the component
     * has mounted and the pie area container is present.
     * @param {Object} data The data to construct the pie
     */
    initializeGraph: function (data) {
      g = d3.select('#pie-area')
        .append('svg')
        .attr('viewBox', `${-width / 2} ${-height / 2} ${width} ${height}`)
        .attr('width', width)
        .attr('height', height)
        .call(d3.zoom().on('zoom', () => { // allow zooming of the pie graph
          g.attr('transform', d3.event.transform);
        }))
        .append('g');

      // TODO ECR
      // gtree = d3.select('#treemap-area') // .append('g')
      //   // .style('position', 'relative')
      //   .style('width', width)
      //   .style('height', height);
      gtree = d3.select('#treemap-area')
        .append('svg')
        .attr('width', width)
        .attr('height', height)
        .append('g');

      if (data) { this.applyGraphData(data); }
    },
    /**
     * Applies the graph data to the pie chart by adding slices and text labels
     * It also adds the colors and transitions to the pie graph
     * (works for new a new pie as well as updating the pie)
     * @param {Object} data The data to add to the graph
     */
    applyGraphData: function (data) {
      let vueSelf = this;
      this.pieData = data; // save pie data for resize
      let colors = this.generateColors(data.children.length);

      let partition = d3.partition() // organize data into sunburst pattern
        .size([2 * Math.PI, radius]); // show sunburst in full circle

      let root = d3.hierarchy(data) // our data is hierarchical
        .sum((d) => { return d.size; }); // sum each node's children

      // combine partition var (data structure) with root node (the actual data)
      partition(root);

      // SLICES ------------------------------ //
      let slice = g.selectAll('g.node') // select all g elements with the node class
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
        .style('fill', (d) => { // apply the colors to the slices
          while (d.depth > 1) { d = d.parent; }
          return colors(d.data.name);
        });

      newSlice // hover functionality
        .on('mouseover', function (d) {
          mouseover(d, this);
          if (popupTimer) { clearTimeout(popupTimer); }
          popupTimer = setTimeout(() => {
            vueSelf.showInfo(d);
          }, 400);
        })
        .on('mouseleave', function (d) {
          mouseleave(d, this);
          if (popupTimer) { clearTimeout(popupTimer); }
        });

      // TEXT -------------------------------- //
      slice.selectAll('text').remove();
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

      // TODO ECR ---------------------------- */
      const treeRoot = d3.hierarchy(data)
        .sum((d) => { return d.size; }); // sum each node's children

      const treemap = d3.treemap() // organize data into treemap
        .size([width, height])
        .padding(4);

      treemap(treeRoot);

      const box = gtree.selectAll('g.box')
        .data(treeRoot.leaves()); // TODO use leaves or descendants?
        // (leaves only show the most nested children, descendants show all children nested)
        // if we use descendants, we need to calculate colors differently
        // if we use descendants, we need to remove the root

      let newBox = box.enter()
        .append('g')
        .attr('class', 'box')
        .merge(box);

      box.exit().remove();
      box.selectAll('rect').remove();

      newBox.append('rect')
        // TODO don't need this with leaves
        .attr('display', (d) => { // don't display the root node
          return d.depth ? null : 'none';
        })
        .attr('x', (d) => { return d.x0; })
        .attr('y', (d) => { return d.y0; })
        .attr('width', (d) => { return d.x1 - d.x0; })
        .attr('height', (d) => { return d.y1 - d.y0; })
        .style('overflow', 'hidden')
        .style('fill', (d) => {
          // TODO make this reusable function
          while (d.depth > 1) { d = d.parent; }
          return colors(d.data.name);
        });

      // TODO hover functionality

      // TEXT -------------------------------- //
      box.selectAll('text').remove();
      newBox.append('text')
        .attr('dx', (d) => { return d.x0 + 2; })
        .attr('dy', (d) => { return d.y0 + 16; })
        // .attr('dx', 4)
        // .attr('dy', '.35em')
        // .attr('fill', 'red')
        .text((d) => {
          // if (!d.depth) { return ''; }
          return d.data.name;
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
      const cancellablePromise = SpigraphService.getPie(this.query, source.token);

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
        this.applyGraphData(response.data.pieResults);
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
                          {{ sliceData.data.size || sliceData.data.sizeValue }}
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
                          {{ sliceData.parent.data.size || sliceData.parent.data.sizeValue }}
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
                          {{ sliceData.data.size || sliceData.data.sizeValue }}
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
                        {{ sliceData.data.size || sliceData.data.sizeValue }}
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
    // d3 doesn't have .off function to remove listeners,
    // so use .on('listener', null)
    newSlice
      .on('mouseover', null)
      .on('mouseleave', null);

    // remove elements
    newSlice.exit().remove();
    newSlice.selectAll('path').remove();
    newSlice.selectAll('text').remove();

    // destroy child component
    $('.info-popup').remove();
    if (popupVue) { popupVue.$destroy(); }

    // cleanup global vars
    setTimeout(() => {
      g = undefined;
      styles = undefined;
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
  position: absolute;
  right: 5px;
  top: 160px;
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
