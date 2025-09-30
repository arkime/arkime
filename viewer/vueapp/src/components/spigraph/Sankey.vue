<!--
Copyright Yahoo Inc.
SPDX-License-Identifier: Apache-2.0
-->
<template>
  <div class="spigraph-sankey">

    <!-- field select -->
    <div class="d-flex flex-row ps-1"
      :class="{'position-absolute': !!tableData.length}">
      <div class="d-inline"
        v-if="fields && fields.length">
        <div class="input-group input-group-sm me-2">
          <span class="input-group-text">
            Add another field:
          </span>
          <arkime-field-typeahead
            :fields="fields"
            @fieldSelected="changeField"
            page="SpigraphSubfield">
          </arkime-field-typeahead>
        </div>
      </div>
      <div class="d-inline ms-1">
        <drag-list
          :list="this.fieldTypeaheadList"
          @reorder="reorderFields"
          @remove="removeField"
        />
      </div>
    </div> <!-- /field select -->

    <!-- info area -->
    <div class="sankey-popup"
      v-show="popupInfo">
      <popup
        v-if="popupInfo"
        :popup-info="popupInfo"
        :field-list="fieldList"
        @closeInfo="closeInfo"
      />
    </div> <!-- /info area -->

    <!-- sankey diagram area -->
    <div id="sankey-area" class="pt-3"
      v-show="sankeyData && sankeyData.nodes.length">
    </div>
    <!-- /sankey diagram area -->

    <!-- no results -->
    <arkime-no-results
      v-if="!sankeyData || !sankeyData.nodes.length"
      class="mt-5 mb-5"
      :view="query.view">
    </arkime-no-results> <!-- /no results -->

  </div>
</template>

<script>
// import services
import SpigraphService from './SpigraphService';
// import internal
import ArkimeNoResults from '../utils/NoResults.vue';
import ArkimeFieldTypeahead from '../utils/FieldTypeahead.vue';
import Popup from './Popup.vue';
import DragList from '../utils/DragList.vue';
// import utils
import Utils from '../utils/utils';
import { commaString } from '@common/vueFilters.js';

let d3; // lazy load d3

let init = true;

// common page variables
let pendingPromise;
let popupTimer;
let resizeTimer;
let colors;
let background;
let foreground;

// sankey variables
let gsankey, sankeyContainer;
const sankeyMargin = { top: 10, right: 10, bottom: 10, left: 10 };
let sankeyWidth = getSankeyWidth();
let sankeyHeight = getSankeyHeight();

// sankey functions
function getSankeyWidth() {
  return window.innerWidth - (sankeyMargin.left + sankeyMargin.right);
}

function getSankeyHeight() {
  return window.innerHeight - 184 - (sankeyMargin.top + sankeyMargin.bottom);
}

// Vue component
export default {
  name: 'ArkimeSankey',
  components: {
    ArkimeNoResults,
    ArkimeFieldTypeahead,
    Popup,
    DragList
  },
  props: {
    baseField: String,
    graphData: Array,
    fields: Array,
    query: Object
  },
  data: function () {
    return {
      sankeyData: null,
      fieldTypeaheadList: [],
      baseFieldObj: undefined,
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

    d3 = await import('d3'); // lazy load d3
    this.loadData();

    // resize the sankey with the window
    window.addEventListener('resize', this.resize);
    // close info popup if the user presses escape
    window.addEventListener('keyup', this.closeInfoOnEsc);
  },
  watch: {
    graphData: async function (newVal, oldVal) {
      this.baseFieldObj = this.getFieldObj(this.baseField);
      if (this.fieldTypeaheadList.length) {
        this.loadData();
      } else {
        const data = this.formatDataFromSpigraph(newVal);
        await this.applySankeyData(data);
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
    },
    tableData: function () {
      return this.sankeyData && this.sankeyData.tableResults ? this.sankeyData.tableResults : [];
    }
  },
  methods: {
    commaString,
    /* exposed page functions */
    /**
     * Adds an expression to the search expression input box
     * @param {Object} node  The sankey node data
     * @param {String} op     The operator to apply to the search expression ('||' or '&&')
     */
    addExpression: function (node, op) {
      const fullExpression = `${node.field} == ${node.name}`;
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
     * @param {Object} field The field the add to the sankey graph
     */
    changeField: function (field) {
      this.fieldTypeaheadList.push(field);
      this.applyFieldListToUrl();
    },
    /* event functions */
    /**
     * Resizes the sankey graph after the window is resized
     * Waits for the window resize to stop for .5 sec
     */
    resize: function () {
      if (resizeTimer) { clearTimeout(resizeTimer); }
      resizeTimer = setTimeout(async () => {
        // recalculate width and height
        sankeyWidth = getSankeyWidth();
        sankeyHeight = getSankeyHeight();

        // set the new width and height of the sankey
        d3.select('#sankey-area svg')
          .attr('width', sankeyWidth)
          .attr('height', sankeyHeight)
          .select('g');

        // just rerender the sankey graph
        await this.applySankeyData(this.sankeyData);
      }, 500);
    },
    /* helper functions */
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
     * Turn the data array into Sankey format
     * @param {Array} data The data array to format
     * @returns {Object} formattedData The formatted data object
     */
    formatDataFromSpigraph: function (data) {
      const nodes = [];
      const links = [];
      const nodeMap = new Map();

      // Create nodes and track them
      data.forEach((item, index) => {
        if (!nodeMap.has(item.name)) {
          nodeMap.set(item.name, {
            id: item.name,
            name: item.name,
            value: item.count || item.sessions || 0,
            field: this.baseField,
            category: index < 10 ? 'top' : 'others' // categorize for better visualization
          });
          nodes.push(nodeMap.get(item.name));
        }
      });

      // For simple single-field data, create a flow from sources to a destination
      if (nodes.length > 1) {
        const totalValue = nodes.reduce((sum, node) => sum + node.value, 0);

        // Create a destination node
        const destNode = {
          id: 'total',
          name: 'Total',
          value: totalValue,
          field: this.baseField,
          category: 'destination'
        };
        nodes.push(destNode);

        // Create links from each source to destination
        nodes.slice(0, -1).forEach(node => {
          links.push({
            source: node.id,
            target: destNode.id,
            value: node.value
          });
        });
      }

      return { nodes, links };
    },
    /**
     * Generates a list of colors based on the length of the data
     * @param {Number} dataLength The length of the data to calculate colors for
     * @returns {Function} colors Function to retrieve a color per data point
     */
    generateColors: function (dataLength) {
      return d3.scaleOrdinal(
        d3.quantize(d3.interpolateRainbow, dataLength + 1)
      );
    },
    /**
     * Initializes the sankey graph
     * @param {Object} data The data to construct the sankey
     */
    async initializeSankey (data) {
      gsankey = d3.select('#sankey-area')
        .append('svg')
        .attr('width', sankeyWidth)
        .attr('height', sankeyHeight)
        .append('g')
        .attr('transform', `translate(${sankeyMargin.left},${sankeyMargin.top})`);

      if (data) { await this.applySankeyData(data); }
    },
    /**
     * Applies the graph data to the sankey diagram
     * @param {Object} data The data to add to the graph
     */
    async applySankeyData (data) {
      // save sankey data for resize
      this.sankeyData = data;

      // update the spigraph page with results
      this.$emit('fetchedResults', data.tableResults || data.nodes || [], this.fieldTypeaheadList, this.baseFieldObj);

      if (!data || !data.nodes || !data.nodes.length) {
        return;
      }

      const vueSelf = this;
      colors = this.generateColors(data.nodes.length);

      // Import d3-sankey
      const { sankey, sankeyLinkHorizontal } = await import('d3-sankey');

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
        .attr('opacity', 0.6)
        .on('mouseover', function (e, d) {
          if (popupTimer) { clearTimeout(popupTimer); }
          popupTimer = setTimeout(() => {
            vueSelf.showInfo(d);
          }, 400);
        })
        .on('mouseleave', function (e, d) {
          if (popupTimer) { clearTimeout(popupTimer); }
        });

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
        .attr('stroke-width', 0.5)
        .on('mouseover', function (e, d) {
          if (popupTimer) { clearTimeout(popupTimer); }
          popupTimer = setTimeout(() => {
            vueSelf.showInfo(d);
          }, 400);
        })
        .on('mouseleave', function (e, d) {
          if (popupTimer) { clearTimeout(popupTimer); }
        });

      // Add node labels
      gsankey.append('g')
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
            await this.initializeSankey(this.formatDataFromSpigraph(this.graphData));
          } else { // otherwise load the data for the additional fields
            await this.initializeSankey();
          }
        }

        pendingPromise = null;
        this.$emit('toggleLoad', false);

        // Transform hierarchical results to sankey format
        const sankeyData = this.transformHierarchicalToSankey(response.hierarchicalResults);
        sankeyData.tableResults = response.tableResults;

        await this.applySankeyData(sankeyData);
        this.$emit('fetchedResults', response.tableResults, this.fieldTypeaheadList, this.baseFieldObj);
      } catch (error) {
        pendingPromise = null;
        this.$emit('toggleLoad', false);
        this.$emit('toggleError', error.text || error);
      }
    },
    /**
     * Transform hierarchical data to sankey format
     * @param {Object} hierarchicalData The hierarchical data from the API
     * @returns {Object} Sankey formatted data
     */
    transformHierarchicalToSankey: function (hierarchicalData) {
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

      traverse(hierarchicalData);

      return { nodes, links };
    },
    /**
     * Displays the information about a sankey element
     * @param {Object} d The sankey element data
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

    // cleanup global vars
    setTimeout(() => {
      init = true;
      gsankey = undefined;
      colors = undefined;
      background = undefined;
      foreground = undefined;
      popupTimer = undefined;
      resizeTimer = undefined;
    });
  }
};
</script>

<style scoped>
.spigraph-sankey {
  position: relative;
}

/* this needs to not be scoped because it's a child component */
.spigraph-sankey div.sankey-popup {
  right: 5px;
  z-index: 9;
  min-width: 220px;
  position: absolute;
  overflow: visible;
  white-space: nowrap;
}

.spigraph-sankey #sankey-area {
  overflow: visible;
}

.spigraph-sankey #sankey-area path {
  cursor: pointer;
}

.spigraph-sankey #sankey-area rect {
  cursor: pointer;
}

.spigraph-sankey #sankey-area text {
  pointer-events: none;
}
</style>
