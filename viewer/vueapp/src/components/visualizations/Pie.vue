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

    <div v-show="spiGraphType === 'pie' && tableData.length">
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

    <!-- table area -->
    <div v-show="spiGraphType === 'table' && tableData.length"
      class="container mt-4">
      <table class="table table-bordered table-condensed table-sm">
        <thead>
          <tr>
            <th colspan="2">
              {{ getFieldObj(baseField).friendlyName }}
            </th>
            <th v-if="outerData && fieldTypeaheadList.length"
              colspan="2">
              {{ fieldTypeaheadList[0].friendlyName }}
            </th>
          </tr>
          <tr>
            <th class="cursor-pointer"
              @click="columnClick(0, 'value')">
              Count
              <span v-show="tableSortField === 0 && tableSortType === 'value' && !tableDesc"
                class="fa fa-sort-asc ml-2">
              </span>
              <span v-show="tableSortField === 0 && tableSortType === 'value' && tableDesc"
                class="fa fa-sort-desc ml-2">
              </span>
              <span v-show="tableSortField !== 0 || tableSortType !== 'value'"
                class="fa fa-sort ml-2">
              </span>
            </th>
            <th class="cursor-pointer"
              @click="columnClick(0, 'name')">
              Value
              <span v-show="tableSortField === 0 && tableSortType === 'name' && !tableDesc"
                class="fa fa-sort-asc ml-2">
              </span>
              <span v-show="tableSortField === 0 && tableSortType === 'name' && tableDesc"
                class="fa fa-sort-desc ml-2">
              </span>
              <span v-show="tableSortField !== 0 || tableSortType !== 'name'"
                class="fa fa-sort ml-2">
              </span>
            </th>
            <template v-if="outerData">
              <th class="cursor-pointer"
                @click="columnClick(1, 'name')">
                Value
                <span v-show="tableSortField === 1 && tableSortType === 'name' && !tableDesc"
                  class="fa fa-sort-asc ml-2">
                </span>
                <span v-show="tableSortField === 1 && tableSortType === 'name' && tableDesc"
                  class="fa fa-sort-desc ml-2">
                </span>
                <span v-show="tableSortField !== 1 || tableSortType !== 'name'"
                  class="fa fa-sort ml-2">
                </span>
              </th>
              <th class="cursor-pointer"
                @click="columnClick(1, 'value')">
                Count
                <span v-show="tableSortField === 1 && tableSortType === 'value' && !tableDesc"
                  class="fa fa-sort-asc ml-2">
                </span>
                <span v-show="tableSortField === 1 && tableSortType === 'value' && tableDesc"
                  class="fa fa-sort-desc ml-2">
                </span>
                <span v-show="tableSortField !== 1 || tableSortType !== 'value'"
                  class="fa fa-sort ml-2">
                </span>
              </th>
            </template>
          </tr>
        </thead>
        <tbody>
          <template v-if="outerData">
            <template v-for="(item, key) in tableData">
              <tr :key="key">
                <td>
                  {{ item.innerData.value }}
                </td>
                <td>
                  <moloch-session-field
                    :field="baseFieldObj"
                    :value="item.innerData.name"
                    :expr="baseFieldObj.exp"
                    :parse="true"
                    :session-btn="true">
                  </moloch-session-field>
                  <span class="color-swatch"
                    :style="{ backgroundColor: item.color }">
                  </span>
                </td>
                <td>
                  <moloch-session-field
                    :field="fieldTypeaheadList[0]"
                    :value="item.name"
                    :expr="fieldTypeaheadList[0].exp"
                    :parse="true"
                    :session-btn="true">
                  </moloch-session-field>
                </td>
                <td>
                  {{ item.value }}
                </td>
              </tr>
            </template>
          </template>
          <template v-else
            v-for="item in tableData">
            <tr :key="item.name">
              <td>
                {{ item.value }}
              </td>
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
import 'd3-interpolate';
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
let g, g2, svg, pie;
let width = getWidth();
let height = getHeight();
let radius = getRadius();
let arc = getArc(0.7, 0.4);
let arc2 = getArc(0.9, 0.7);
let outerArc = getArc(0.9, 0.9);
let outerArc2 = getArc(0.95, 0.95);

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

function getArc (innerRadiusScale, outerRadiusScale) {
  return d3.arc()
    .innerRadius(radius * innerRadiusScale)
    .outerRadius(radius * outerRadiusScale)
    .cornerRadius(6);
}

function midAngle (d) {
  return d.startAngle + (d.endAngle - d.startAngle) / 2;
}

function sliceTransition (d, arcFunc, _current) {
  let interpolate = d3.interpolate(_current, d);
  _current = interpolate(0);
  return function (t) {
    return arcFunc(interpolate(t));
  };
};

// put the text outside of the pie slices
function textTransform (d, arcFunc, _current) {
  const interpolate = d3.interpolate(_current, d);
  _current = interpolate(0);
  return function (t) {
    const d2 = interpolate(t);
    let pos = arcFunc.centroid(d2);
    pos[0] = radius * (midAngle(d2) < Math.PI ? 1 : -1);
    return `translate(${pos})`;
  };
};

function textTransition (d) {
  this._current = this._current || d;
  const interpolate = d3.interpolate(this._current, d);
  this._current = interpolate(0);
  return function (t) {
    var d2 = interpolate(t);
    return midAngle(d2) < Math.PI ? 'start' : 'end';
  };
}

function getLabelText (d) {
  return `${d.data.value.name} (${d.data.value.value})`;
}

function getTopLabelText (d) {
  if (d.data.value.subIndex < 2) {
    return `${d.data.value.name} (${d.data.value.value})`;
  }
}

function polylineTransform (d) {
  this._current = this._current || d;
  const interpolate = d3.interpolate(this._current, d);
  this._current = interpolate(0);
  return function (t) {
    const d2 = interpolate(t);
    let pos = outerArc.centroid(d2);
    pos[0] = radius * 0.95 * (midAngle(d2) < Math.PI ? 1 : -1);
    let arcCentroid = arc.centroid(d2);
    arcCentroid[0] = arcCentroid[0] - (arcCentroid[0] * -0.2);
    arcCentroid[1] = arcCentroid[1] - (arcCentroid[1] * -0.2);
    return [arcCentroid, outerArc.centroid(d2), pos];
  };
}

function polylineTransform2 (d) {
  if (d.data.value.subIndex > 1) {
    return; // only display the first 2
  }
  this._current = this._current || d;
  const interpolate = d3.interpolate(this._current, d);
  this._current = interpolate(0);
  return function (t) {
    const d2 = interpolate(t);
    let pos = outerArc2.centroid(d2);
    pos[0] = radius * 0.95 * (midAngle(d2) < Math.PI ? 1 : -1);
    let arcCentroid = arc2.centroid(d2);
    arcCentroid[0] = arcCentroid[0] - (arcCentroid[0] * -0.1);
    arcCentroid[1] = arcCentroid[1] - (arcCentroid[1] * -0.1);
    return [arcCentroid, outerArc2.centroid(d2), pos];
  };
}

function mouseover (d, self) {
  d3.select(self).style('opacity', 1);
}

function mouseleave (d, self) {
  d3.select(self).style('opacity', 0.8);
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
      tableSortType: 'value',
      tableSortField: 0,
      tableDesc: true,
      closeInfo: closeInfo,
      fieldTypeaheadList: [],
      baseFieldObj: undefined
    };
  },
  mounted: function () {
    this.initializeGraph(this.formatDataFromSpigraph(this.graphData));

    this.baseFieldObj = this.getFieldObj(this.baseField);

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
        this.applyGraphData(data, g, arc, outerArc, polylineTransform, getLabelText);
      }
    }
  },
  methods: {
    /* exposed page functions ---------------------------------------------- */
    /**
     * Removes a field from the field typeahead list and loads the data
     * @param {Number} index The index of the field typeahead list to remove
     */
    removeField: function (index) {
      this.fieldTypeaheadList.splice(index, 1);

      if (g2) {
        // remove slices
        g2.datum(d3.entries({}))
          .selectAll('path')
          .data(pie(d3.entries({})))
          .exit().remove();
        // remove labels
        g2.datum(d3.entries({}))
          .selectAll('text')
          .data(pie(d3.entries({})))
          .exit().remove();
        // remove polylines
        g2.datum(d3.entries({}))
          .selectAll('polyline')
          .data(pie(d3.entries({})))
          .exit().remove();
      }

      this.loadData();
    },
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
     * Fired when a second level field typeahead field is selected
     * @param {Object} field The field the add to the pie graph
     */
    changeField: function (field) {
      // TODO allow 2 items in this array?
      if (this.fieldTypeaheadList.length > 0) {
        this.$set(this.fieldTypeaheadList, 0, field);
      } else {
        this.fieldTypeaheadList.push(field);
      }

      this.loadData();
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
        // recalculate width, height, radius, and arcs
        width = getWidth();
        height = getHeight();
        radius = getRadius();
        arc = getArc(0.7, 0.4);
        arc2 = getArc(0.9, 0.7);
        outerArc = getArc(0.9, 0.9);
        outerArc2 = getArc(0.95, 0.95);

        // set the new width and height of the pie
        d3.select('#pie-area svg')
          .attr('width', width)
          .attr('height', height)
          .select('g')
          .attr('transform', 'translate(' + width / 2 + ',' + height / 2 + ')');

        // redraw the inner pie data
        this.redraw(g, arc, outerArc, polylineTransform);

        if (g2) { // redraw the outer pie data if it exists
          this.redraw(g2, arc2, outerArc2, polylineTransform2);
        }
      }, 500);
    },
    /* helper functions ---------------------------------------------------- */
    /**
     * Sorts the table data based on the existing sort field and type vars
     */
    sortTable: function () {
      this.tableData.sort((a, b) => {
        let result = false;
        if (!this.tableDesc && this.tableSortField === 0 && a.innerData) {
          result = a.innerData[this.tableSortType] > b.innerData[this.tableSortType];
        } else if (this.tableDesc && this.tableSortField === 0 && a.innerData) {
          result = b.innerData[this.tableSortType] > a.innerData[this.tableSortType];
        } else if (!this.tableDesc) {
          result = a[this.tableSortType] > b[this.tableSortType];
        } else {
          result = b[this.tableSortType] > a[this.tableSortType];
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
      this.tableData = [];

      let formattedData = {};
      for (let item of data) {
        let dataObj = {
          name: item.name,
          value: item.count,
          field: this.baseField
        };
        formattedData[item.name] = dataObj;
        this.tableData.push(dataObj);
      }

      this.sortTable();
      this.applyColorsToTableData(data);

      return formattedData;
    },
    /**
     * Generates a list of colors (RAINBOW) based on the length of the data
     * @param {Object} data The data object to calculate colors for
     * @returns {Function} colors Function to retrieve a color per data point
     */
    generateColors: function (data) {
      const colorScale = d3.interpolateHslLong('red', 'purple');
      const dataLength = Object.keys(data).length;
      const intervalSize = 1 / dataLength;

      let colorArray = [];
      for (let i = 0; i < dataLength; i++) {
        let color = colorScale(i * intervalSize);
        colorArray.push(color);
      }

      // set the color scale
      return d3.scaleOrdinal()
        .domain(data)
        .range(colorArray);
    },
    /**
     * Adds a color variable to every table data item using the outer bucket
     * @param {Object} data The data to generate the colors from
     */
    applyColorsToTableData: function (data) {
      let colors = this.generateColors(data);
      for (let item of this.tableData) {
        let key = item.name;
        if (item.innerData) { key = item.innerData.name; }
        item.color = colors(key);
      }
    },
    /**
     * Initializes the graph by adding the svg to the page once the component
     * has mounted and the pie area container is present.
     * Sets the value of the pie slices and sorts the pie slices by index
     * Applies the initial data to the pie graph
     * @param {Object} data The data to construct the pie
     */
    initializeGraph: function (data) {
      // PIE SETUP --------------------------- //
      svg = d3.select('#pie-area')
        .append('svg')
        .attr('width', width)
        .attr('height', height)
        .append('g')
        .attr('transform', 'translate(' + width / 2 + ',' + height / 2 + ')');

      g = svg.append('g');

      pie = d3.pie().value((d) => {
        // show the scaled value if it exists (so buckets fit data)
        if (d && d.value && d.value.scaledValue) {
          return d.value.scaledValue;
        }
        // otherwise show the exact value
        if (d && d.value && d.value.value) {
          return d.value.value;
        }
        return d.value;
      }).sort((a, b) => {
        if (a && a.value && a.value.index) {
          return a.value.index - b.value.index;
        }
        return true;
      });

      this.applyGraphData(data, g, arc, outerArc, polylineTransform, getLabelText);
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
    /**
     * Applies the graph data to the pie chart by adding the slices, text labels,
     * and lines from the slices to the text labels
     * It also adds the colors and transitions to the pie graph
     * (works for new a new pie as well as updating the pie)
     * @param {Object} data             The data to add to the graph
     * @param {Object} gArea            The d3 g element for the data
     * @param {Function} arcFunc        The arc function to apply to the slices
     * @param {Function} outerArcFunc   The outer arc function to apply to the slices
     * @param {Function} lineTransFunc  The function to apply to draw the lines
     * @param {Function} labelTextFunc  The function to show the text labels
     */
    applyGraphData: function (data, gArea, arcFunc, outerArcFunc, lineTransFunc, labelTextFunc) {
      let colors = this.generateColors(data);
      let vueSelf = this;

      // PIE SLICES -------------------------- //
      // add any new slices
      gArea.datum(d3.entries(data))
        .selectAll('path')
        .data(pie(d3.entries(data)))
        .enter()
        .append('path')
        .attr('d', arc2)
        .attr('stroke', 'white')
        .style('stroke-width', '2px')
        .style('opacity', 0.8)
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
        })
        .on('click', (d) => {
          this.addExpression(d.data.value, '||');
        });

      // apply color to ALL of the slices (not just the new ones)
      gArea.selectAll('path')
        .attr('fill', (d) => {
          return colors(d.data.key);
        });

      // add transition to new slices
      gArea.datum(d3.entries(data))
        .selectAll('path')
        .data(pie(d3.entries(data)))
        .transition().duration(1000)
        .attrTween('d', (d) => {
          return sliceTransition(d, arcFunc, this._current || d);
        });

      // remove any slices not being used
      gArea.datum(d3.entries(data))
        .selectAll('path')
        .data(pie(d3.entries(data)))
        .exit().remove();

      // TEXT LABELS ------------------------- //
      // add any new text
      gArea.datum(d3.entries(data))
        .selectAll('text')
        .data(pie(d3.entries(data)))
        .enter()
        .append('text')
        .attr('dy', '.15rem');

      // apply text location and transition to new labels
      gArea.datum(d3.entries(data))
        .selectAll('text')
        .data(pie(d3.entries(data)))
        .transition().duration(1000)
        .attrTween('transform', (d) => {
          return textTransform(d, outerArcFunc, this._current || d);
        })
        .styleTween('text-anchor', textTransition);

      // remove any text not being used and apply text label data
      gArea.datum(d3.entries(data))
        .selectAll('text')
        .data(pie(d3.entries(data)))
        .text(labelTextFunc)
        .exit().remove();

      // LINE TO LABEL ----------------------- //
      // remove all lines (because removing them with provided data doesn't
      // work for some insane and unknown reason)
      gArea.datum(d3.entries({}))
        .selectAll('polyline')
        .data(pie(d3.entries({})))
        .exit().remove();

      // add any new lines from slices to labels
      gArea.datum(d3.entries(data))
        .selectAll('polyline')
        .data(pie(d3.entries(data)))
        .enter()
        .append('polyline');

      // apply transitions to new lines
      gArea.datum(d3.entries(data))
        .selectAll('polyline')
        .data(pie(d3.entries(data)))
        .transition().duration(1000)
        .attrTween('points', lineTransFunc);
    },
    /**
     * Redraws the pie slices, lines, and label positions
     * @param {Object} gArea            The d3 g element for the data
     * @param {Function} arcFunc        The arc function to apply to the slices
     * @param {Function} outerArcFunc   The outer arc function to apply to the slices
     * @param {Function} lineTransFunc  The function to apply to draw the lines
     */
    redraw: function (gArea, arcFunc, outerArcFunc, lineTransFunc) {
      gArea.selectAll('path')
        .transition().duration(1000)
        .attrTween('d', (d) => {
          return sliceTransition(d, arcFunc, this._current || d);
        });

      // set the new position of the text
      gArea.selectAll('text')
        .transition().duration(1000)
        .attrTween('transform', (d) => {
          return textTransform(d, outerArcFunc, this._current || d);
        })
        .styleTween('text-anchor', textTransition);

      // apply transitions to new lines
      gArea.selectAll('polyline')
        .transition().duration(1000)
        .attrTween('points', lineTransFunc);
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

        this.outerData = false;
        this.tableData = []; // clear the table data

        // format the data for the pie graph
        let innerData = {};
        let outerData = {};
        let index = 0;
        let parentIndex = 0;

        // create a data object and identify the parent index (which bucket a
        // value belongs to) and the index of the value itself (for coloring)
        for (let item in response.data) {
          let itemObj = {
            name: item,
            value: response.data[item].value,
            // save the field to add to the search expression
            field: this.baseField
          };
          innerData[item] = itemObj;

          let subIndex = 0;
          let subBucketsSum = 0;
          for (let subItem in response.data[item].subData) {
            this.outerData = true;
            let value = response.data[item].subData[subItem];
            let subItemObj = {
              value: value,
              parentIndex: parentIndex,
              index: index,
              name: subItem,
              subIndex: subIndex,
              // save the inner data to show on hover
              innerData: innerData[item],
              // save the field to add to the search expression
              field: this.fieldTypeaheadList[0].exp
            };
            outerData[`${subItem}-${parentIndex}`] = subItemObj;
            index++;
            subIndex++;
            subBucketsSum += value;
            // add the flat info to the table data
            this.tableData.push(subItemObj);
          }

          if (!this.outerData) {
            // add the info to the table data
            this.tableData.push(itemObj);
          }

          // scale the inner data so that outer data fits the bucket
          innerData[item].scaledValue = innerData[item].value * (subBucketsSum / innerData[item].value);
          parentIndex++;
        }

        this.sortTable();
        this.applyColorsToTableData(innerData);

        // add the data to the inner circle (it might have changed)
        this.applyGraphData(innerData, g, arc, outerArc, polylineTransform, getLabelText);
        if (index > 0) { // if there's outer data
          // remove the inner labels
          g.datum(d3.entries({}))
            .selectAll('text')
            .data(pie(d3.entries({})))
            .exit().remove();

          // remove lines from slices to labels from inner pie
          g.datum(d3.entries({}))
            .selectAll('polyline')
            .data(pie(d3.entries({})))
            .exit().remove();

          // add another g to add the new pie data
          if (!g2) { g2 = svg.append('g'); }
          // add data to the outer circle
          this.applyGraphData(outerData, g2, arc2, outerArc2, polylineTransform2, getTopLabelText);
        }
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
                  <tr v-if="outerFieldObj && sliceData.innerData">
                    <td>
                      {{ outerFieldObj.friendlyName }}
                    </td>
                    <td>
                      <moloch-session-field
                        :field="outerFieldObj"
                        :value="sliceData.name"
                        :expr="outerFieldObj.exp"
                        :parse="true"
                        :session-btn="true">
                      </moloch-session-field>
                    </td>
                    <td>
                      <strong>
                        {{ sliceData.value }}
                      </strong>
                    </td>
                  </tr>
                  <tr v-else-if="baseFieldObj">
                    <td>
                      {{ baseFieldObj.friendlyName }}
                    </td>
                    <td>
                      <moloch-session-field
                        :field="baseFieldObj"
                        :value="sliceData.name"
                        :expr="baseFieldObj.exp"
                        :parse="true"
                        :session-btn="true">
                      </moloch-session-field>
                    </td>
                    <td>
                      <strong>
                        {{ sliceData.value }}
                      </strong>
                    </td>
                  </tr>
                  <tr v-if="sliceData.innerData">
                    <td>
                      {{ baseFieldObj.friendlyName }}
                    </td>
                    <td>
                      <moloch-session-field
                        :field="baseFieldObj"
                        :value="sliceData.innerData.name"
                        :expr="baseFieldObj.exp"
                        :parse="true"
                        :session-btn="true">
                      </moloch-session-field>
                    </td>
                    <td>
                      <strong>
                        {{ sliceData.innerData.value }}
                      </strong>
                    </td>
                  </tr>
                </tbody>
              </table>
            </div>
          `,
          parent: this,
          data: {
            sliceData: d.data.value,
            baseFieldObj: this.getFieldObj(this.baseField),
            outerFieldObj: this.fieldTypeaheadList[0] || undefined
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
    g.selectAll('path')
      .on('click', null)
      .on('mouseover', null)
      .on('mouseleave', null);
    g2.selectAll('path')
      .on('click', null)
      .on('mouseover', null)
      .on('mouseleave', null);

    // remove svg elements
    svg.selectAll('path').exit().remove();
    svg.selectAll('text').exit().remove();
    svg.selectAll('polylines').exit().remove();

    // destroy child component
    $('.info-popup').remove();
    if (popupVue) { popupVue.$destroy(); }

    // cleanup global vars
    setTimeout(() => {
      g = undefined;
      g2 = undefined;
      svg = undefined;
      pie = undefined;
      popupVue = undefined;
      popupTimer = undefined;
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
