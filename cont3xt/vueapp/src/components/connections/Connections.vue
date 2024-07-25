<template>
  <div class="d-flex flex-column w-100">
    <div class="d-flex flex-row graph-config-row pb-1 px-3 gap-1">
      <b-button @click="renderGraph">
        Re-Render
      </b-button>
      <b-button @click="recenterOnNodes">
        Re-center
      </b-button>
      <b-button @click="recenterAndZoomOnNodes">
        Re-zoom
      </b-button>
      <b-button @click="unpinNodesAndRun">
        Un-pin All & Run
      </b-button>
      <!-- <i v-if="isRendering"> -->
      <!--   | Rendering... -->
      <!-- </i> -->
      <!-- Connections Settings -->
      <b-dropdown>
        <template #button-content>
          Settings
        </template>
        <div v-for="[linkTypeCategory, categoryEntries] of Object.entries(linkTypes)" :key="linkTypeCategory">
          <hr/>
          <strong>
            {{ linkTypeCategory }}
          </strong>
          <b-checkbox
            v-for="[linkType, { name: linkTypeName }] of Object.entries(categoryEntries)"
            :key = "linkType"
            v-model="enabledLinkTypes[linkType]"
          >
            {{ linkTypeName }}
          </b-checkbox>
        </div>
      </b-dropdown>
      <i v-if="!getLoading.done && getLoading.total > 0">
        Loading...
      </i>
      <!-- /Connections Settings -->
    </div>
    <div ref="graphContainer" class="graph-svg-container">
      <svg class="connections-graph"
        @mousedown.exact="mouseDown"
        @mousedown.shift="mouseDownShift"
        @mousemove="mouseMove"
        @mouseup="mouseUp"
      />
    </div>
    <div class="right-click-menu d-flex flex-column" v-if="contextMenu != null" :style="`top: ${contextMenu.y + 5}px; left: ${contextMenu.x}px; z-index: 100; background-color: white;`">
      <!-- hi {{ contextMenu.node }} -->
      <b-button>
        Expand
      </b-button>
      <b-button @click="hide">
        Hide
      </b-button>
    </div>

    <!-- TODO: right click menu (absolute - translate here!) -->
  </div>
</template>

<script>
// TODO: toby - disable all inputs until rendered first time!
import { mapGetters } from 'vuex';
import { Cont3xtIndicatorProp, getIntegrationDataMap } from '@/utils/cont3xtUtil';
import Cont3xtField from '@/utils/Field';
import { linkTypes } from '@/components/connections/ConnectionsSettings';
// TODO: toby figure out how this bundles & ideally remove this import
// eslint-disable-next-line no-unused-vars
import d3Import from 'd3'; // allows us to later do import('d3')

// lazy loaded d3 module (external because d3 is easier to type than this.d3)
let d3;

// TODO: toby - found via: https://codepen.io/sosuke/pen/Pjoqqp
const nodeImageFillColorFilters = {
  domain: 'filter: invert(17%) sepia(0%) saturate(71%) hue-rotate(193deg) brightness(95%) contrast(92%);', // #303030
  ip: 'filter: invert(66%) sepia(26%) saturate(566%) hue-rotate(94deg) brightness(92%) contrast(91%);', // #65b689
  registrar: 'filter: invert(44%) sepia(8%) saturate(4033%) hue-rotate(137deg) brightness(87%) contrast(67%);' // #2f7d86
};

// TODO: toby - table?
function getImageForType (type) {
  switch (type) {
  case 'domain': return 'cont3xt_assets/domain.svg';
  case 'ip': return 'cont3xt_assets/ip.svg';
  case 'registrar': return 'cont3xt_assets/registrar.svg';
  case 'asn': return 'cont3xt_assets/asn.png';
  }
  return 'cont3xt_assets/domain.svg'; // TODO: missing icon?
}

export default {
  name: 'Connections',
  props: {
    indicator: Cont3xtIndicatorProp // the indicator to be connected
  },
  components: {
    Cont3xtField
  },
  data () {
    // TODO: toby - name/organize!
    return {
      hiddenNodes: [],
      hiddenNodesSet: new Set(),
      // TODO: toby - todo
      contextMenu: undefined,

      linkTypes,
      enabledLinkTypes: {},
      // TODO: toby - does this work?
      isRendering: false,
      hasDataRendered: false,
      /* --- modules --- */
      // d3: undefined, // lazy-import
      saveSvgAsPng: undefined, // lazy-import
      /* --- graph sizing --- */
      resizeObserver: undefined,
      /* --- graph drawing --- */
      simulation: undefined, // init in drawGraph
      // TODO: toby dimens
      width: 500,
      height: 500,
      dataNodes: [],
      dataLinks: [],
      // TODO: toby - update comments
      nodes: [], // copied from data.nodes when drawGraph is called
      links: [], // copied from data.links when drawGraph is called
      directedLinkIndexPairSet: undefined, // TODO: toby - used?
      undirectedLinkIndexPairSet: undefined,
      // TODO: toby - rename?
      // NOTE: non-reactive?
      nodeSelection: new Set(), // Set<number>()  (node index set)
      // NOTE: reactive
      nodeSelectionArr: [],
      containerTransform: { x: 0, y: 0, k: 1 }, // un-zoomed, un-panned, default container transform
      /* - svg elements - */
      svg: undefined, // init in drawGraph
      container: undefined,
      node: undefined,
      link: undefined,
      nodeLabel: undefined,
      svgNodeSelection: undefined, // TODO: toby - bad name?
      svgSelection: undefined,
      /* --- TODO: other - label --- */
      zoom: undefined,
      /* --- settings --- */
      fontSize: 0.4,
      /* --- dragging --- */
      draggingNode: undefined,
      panningMouseDown: false,
      panningMouseDownHasMoved: false,
      panningMouseDownStartPoint: undefined, // TODO: toby - maybe not using?
      /* --- multi-select --- */
      blockSelection: false,
      shiftDragging: false,
      shiftDragStart: undefined,
      shiftDragEnd: undefined,

      cont3xtData: {
        // domain: {
        //   'hi.com': {
        //     edges: {
        //       registrar: [
        //         'reg_aaa',
        //         'reg_bbb'
        //       ],
        //       ptDnsSubdomain: [
        //         'sub1/',
        //         'sub2/',
        //         'sub3/'
        //       ],
        //       ptSubdomainRecord: [
        //         'record_e'
        //       ]
        //     }
        //   }
        // },
        // registrar: {
        //   reg_aaa: {
        //     edges: {
        //       domain: [
        //         'hi.com'
        //       ]
        //     }
        //   }
        // },
        // ip: {
        //   '1.1.1.1': {
        //     edges: {}
        //   },
        //   '8.8.8.8': {
        //     edges: {
        //       domain: [ // ptr
        //         'hi.com'
        //       ]
        //     }
        //   }
        // }
      }
    };
  },
  computed: {
    ...mapGetters(['getShiftKeyHold', 'getResults', 'getLoading', 'getIndicatorGraph']),
    // TODO: toby - name better
    selectedNode () {
      return this.nodeSelectionArr.length === 1
        ? this.selectedNodes[0]
        : undefined;
    },
    selectedNodes () {
      return this.nodeSelectionArr.map(i => this.nodeIndexToNode(i));
    },
    integrationDataMap () { // TODO: don't want this --> need per indicator
      // TODO: toby - rm?
      return this.indicator ? getIntegrationDataMap(this.getResults, this.indicator) : {};
    },
    blockSelectionTopLeft () {
      return (this.blockSelection)
        ? {
          x: Math.min(this.shiftDragStart.x, this.shiftDragEnd.x),
          y: Math.min(this.shiftDragStart.y, this.shiftDragEnd.y)
        }
        : undefined;
    },
    blockSelectionBottomRight () {
      return (this.blockSelection)
        ? {
          x: Math.max(this.shiftDragStart.x, this.shiftDragEnd.x),
          y: Math.max(this.shiftDragStart.y, this.shiftDragEnd.y)
        }
        : undefined;
    },
  },
  watch: {
    getLoading (oldVal, newVal) {
      // loaded
      if (newVal.done && !oldVal.done && !this.hasDataRendered) {
        this.renderFromData();
      }
    }
  },
  methods: {
    // sync hiddenNodes array for use in the UI
    updateHiddenNodes () {
      this.hiddenNodes = [...this.hiddenNodesSet];
    },
    updateSvgVisibility () {
      this.node.attr('visibility', d => {
        console.log('a node', d);
        return this.hiddenNodesSet.has(d.index) ? 'hidden' : 'visible';
      });
      this.nodeLabel.attr('visibility', d => {
        console.log('a node', d);
        return this.hiddenNodesSet.has(d.index) ? 'hidden' : 'visible';
      });
    },
    hide () {
      // this.svg.select('#idip\\,1\\.1\\.1\\.1').remove();
      const { node, i } = this.contextMenu;
      const { id } = node;
      const nodeId = '#id' + id;
      const nodeLabelId = '#id' + id + '-label';
      this.svg.select(nodeId).remove();
      this.svg.select(nodeLabelId).remove();

      this.contextmenu = undefined;

      // this.hiddenNodesSet.add(i);
      // this.updateHiddenNodes();
      //
      // this.updateSvgVisibility();

      // const idRegex = /[\[\]:., ]/g;
      // // console.log('toby', node, i, idRegex, node.id);
      // const id = '#id' + node.id.replace(idRegex, '_');
      //
      //
      // // console.log('toby', node, i, id);
      // this.node.selectAll(d => {
      //   console.log('toby', d);
      //   return true;
      // }).remove();
      // this.container.select(id + '-label').remove();
              // svg.selectAll('.link')
              //   .filter(function (d, i) {
              //     return d.source.id === dataNode.id || d.target.id === dataNode.id;
              //   })
              //   .remove();
    },
    nodeIndexToNode (i) {
      const dataNode = this.dataNodes[i];
      return {
        ...dataNode,
        index: i
      };
    },
    // use indicator
    extractCont3xtData (indicatorGraph) {
      this.cont3xtData = {};

      // can happen multiple times?
      const contributeNode = ({ value, type }) => {
        console.log('toby', this.cont3xtData);
        this.cont3xtData[type] ??= {};
        this.cont3xtData[type][value] ??= {
          edges: []
        };
      };

      const addEdge = (from, to) => {
        this.cont3xtData[from.type][from.value].edges[to.type] ??= [];
        this.cont3xtData[from.type][from.value].edges[to.type].push(to.value);
      };

      // implicitly contributes the to node
      const addImplicitEdge = (from, to) => {
        contributeNode(to);
        addEdge(from, to);
      };

      for (const { indicator } of Object.values(indicatorGraph)) {
        const node = { type: indicator.itype, value: indicator.query };
        contributeNode(node);
        const data = getIntegrationDataMap(this.getResults, indicator);
        console.log('toby ind', indicator, data);

        const implicitEdgeTo = (to) => {
          addImplicitEdge(node, to);
        };

        switch (indicator.itype) {
        case 'domain':
          if ((true || this.enabledLinkTypes.DOMAIN_TO_REGISTRAR) && data.Whois) {
            if (data.Whois.registrar) {
              implicitEdgeTo({ value: data.Whois.registrar, type: 'registrar' });
            }
          }
          if ((true || this.enabledLinkTypes.DOMAIN_TO_DNS_A) && data.DNS) {
            for (const record of data.DNS.A?.Answer ?? []) {
              implicitEdgeTo({ value: record.data, type: 'ip' });
            }
          }
          if ((true || this.enabledLinkTypes.DOMAIN_TO_DNS_AAAA) && data.DNS) {
            for (const record of data.DNS.AAAA?.Answer ?? []) {
              implicitEdgeTo({ value: record.data, type: 'ip' });
            }
          }

            // if (data.PTS_DNS) {}
          break;
        case 'ip':
            console.log('toby', 'hi ip!', data);
          if (data['VT IP']?.asn) {
            implicitEdgeTo({ value: data['VT IP'].asn, type: 'asn' });
          }
          break;
        }
      }

      console.log('toby DONE', this.cont3xtData);
    },
    renderFromData () {
      console.log('toby', 'calling');
      if (this.indicator == null) { return; }
      this.hasDataRendered = true;
      this.extractCont3xtData(this.getIndicatorGraph);
      this.drawGraphWrapper();
      console.log('toby', 'rendering from data', this.integrationDataMap);
    },
    findNodeBoundingBox () {
      return d3.select('#node-group').node().getBBox();
    },
    recenterOnNodes () { // use circle svg group (<g> tag) bounding box
      const boundingBox = this.findNodeBoundingBox();
      if (boundingBox == null) { return; }

      const boxCenterX = boundingBox.x + boundingBox.width / 2;
      const boxCenterY = boundingBox.y + boundingBox.height / 2;
      this.svg
        .call(this.zoom.translateTo, boxCenterX, boxCenterY);
    },
    recenterAndZoomOnNodes () {
      const boundingBox = this.findNodeBoundingBox();
      if (boundingBox == null) { return; }

      const boxCenterX = boundingBox.x + boundingBox.width / 2;
      const boxCenterY = boundingBox.y + boundingBox.height / 2;
      const scale = Math.min(
        this.width / boundingBox.width,
        this.height / boundingBox.height
      ) - 0.05;
      this.svg
        .call(this.zoom.scaleTo, scale)
        .call(this.zoom.translateTo, boxCenterX, boxCenterY);
    },
    unpinNodesAndRun () {
      this.node.each(d => {
        delete d.fx;
        delete d.fy;
      });
      // TODO: toby - what is alpha target - and should i restart?
      this.simulation.alphaTarget(0.1).restart();
    },
    syncNodeSelectionArr () { // vue 2 reactivity :(
      this.nodeSelectionArr = [...this.nodeSelection];
    },
    nodeIsInSelection (d) {
      return this.nodeSelection.has(d.index);
    },
    addToNodeSelection (d) {
      this.nodeSelection.add(d.index);
      this.syncNodeSelectionArr();

      this.updateSvgNodeSelection();
    },
    nodeSelectionAddAll (dArr) {
      for (const d of dArr) {
        this.nodeSelection.add(d.index);
      }
      this.syncNodeSelectionArr();
      this.updateSvgNodeSelection();
    },
    removeFromNodeSelection (d) {
      // TODO: toby - this
      this.nodeSelection.delete(d.index);
      this.syncNodeSelectionArr();
      this.updateSvgNodeSelection();
    },
    clearNodeSelection (d) {
      // TODO: toby - this
      this.nodeSelection.clear();
      this.syncNodeSelectionArr();
      this.updateSvgNodeSelection();
    },
    // highlighting helpers ---------
    // check if a connects --> b, or if a is b
    isConnected (a, b) {
      return (a.index === b.index) || this.undirectedLinkIndexPairSet.has(`${a.index},${b.index}`);
    },
    itemFocus (d) {
      // don't apply focus styles if dragging a node
      // TODO: toby - why?
      if (!this.draggingNode) {
        this.node.style('opacity', (o) => {
          return this.isConnected(d, o) ? 1 : 0.1;
        });
        this.nodeLabel.attr('display', (o) => {
          return this.isConnected(d, o) ? 'block' : 'none';
        });
        this.link.style('opacity', (o) => {
          return o.source.index === d.index || o.target.index === d.index ? 1 : 0.1;
        });
      }
    },
    unfocus () {
      this.nodeLabel.attr('display', 'block');
      this.node.style('opacity', 1);
      this.link.style('opacity', 1);
    },
    // drag helpers
    dragstarted (e, d) {
      this.contextMenu = undefined;
      e.sourceEvent.stopPropagation();
      if (!e.active) { this.simulation.alphaTarget(0.1).restart(); }
      // this.itemFocus(d);
      this.draggingNode = d;
      if (!this.getShiftKeyHold && !this.nodeIsInSelection(d)) {
        this.clearNodeSelection();
      }
      this.addToNodeSelection(d);
      // d.fx = d.x;
      // d.fy = d.y;
      // this.itemFocus(d);
      // TODO: toby - focus or no?
    },
    dragged (e, d) {
      const dx = e.x - d.x;
      const dy = e.y - d.y;

      const selection = this.node.filter(this.nodeIsInSelection);

      selection
        .each(n => {
          n.x += dx;
          n.y += dy;
        });

      selection // fix them while dragging to avoid unstable behavior
        .each(n => {
          n.fx = n.x;
          n.fy = n.y;
        });
    },
    dragended (e, d) {
      if (!e.active) { this.simulation.alphaTarget(0).stop(); }
      // TODO: toby - should sim stop? - maybe have toggle
      // const selection = this.node.filter(this.nodeIsInSelection);
      // selection // fix them while dragging to avoid unstable behavior
      //   .each(n => {
      //     if (n !== this.draggingNode) {
      //       n.fx = undefined;
      //       n.fy = undefined;
      //     }
      //   });

      this.draggingNode = undefined;
      // keep the node where it was dragged
      // d.fx = d.x;
      // d.fy = d.y;
      // this.itemFocus(d);

    },
    pointFromMouseEvent (e) {
      return {
        x: e?.offsetX,
        y: e?.offsetY
      };
    },
    containerPosToWorldSpace (point) {
      // TODO: toby - scale
      return {
        x: (point.x - this.containerTransform.x) / this.containerTransform.k,
        y: (point.y - this.containerTransform.y) / this.containerTransform.k
      };
    },
    worldSpacePointFromMouseEvent (e) {
      return this.containerPosToWorldSpace(this.pointFromMouseEvent(e));
    },
    positionSvgSelection () {
      this.svgSelection
        .attr('x', this.blockSelectionTopLeft.x)
        .attr('y', this.blockSelectionTopLeft.y)
        .attr('width', this.blockSelectionBottomRight.x - this.blockSelectionTopLeft.x)
        .attr('height', this.blockSelectionBottomRight.y - this.blockSelectionTopLeft.y);
    },
    removeSvgSelection () {
      if (this.svgSelection) {
        this.svgSelection.remove();
        this.svg.selectAll('.svg-selection').remove();
      }
    },
    removeSvgNodeSelection () {
      this.svgNodeSelection.exit().remove();
      this.svg.selectAll('.node-selection').remove();
    },
    updateSvgNodeSelection () {
      if (this.svgNodeSelection) {
        this.removeSvgNodeSelection();
      }

      this.svgNodeSelection = this.container.append('g')
        .selectAll('circle')
        .data(this.nodes)
        .enter()
        .filter(d => this.nodeSelection.has(d.index))
        .append('circle')
        .attr('id', d => `node-selection-${d.index}`)
        .attr('class', 'node-selection')
        .attr('r', 10)
        .attr('cx', d => d.x)
        .attr('cy', d => d.y)
        .attr('stroke', '#3333FF44')
        .attr('stroke-width', '#333333')
        .attr('fill', '#00000000')
        .attr('visibility', 'visible')
        .style('pointer-events', 'none') // to prevent mouseover/drag capture
      ;
    },
    mouseDown (e) {
      this.contextMenu = undefined;
      if (!this.getShiftKeyHold) {
        this.panningMouseDown = true;
        this.panningMouseDownHasMoved = false;
        this.panningMouseDownStartPoint = this.pointFromMouseEvent(e);
      }
    },
    mouseDownShift (e) {
      this.contextMenu = undefined;
      if (!this.svg) { return; }

      this.blockSelection = true;
      this.shiftDragging = true;

      this.shiftDragStart = this.worldSpacePointFromMouseEvent(e);
      this.shiftDragEnd = this.worldSpacePointFromMouseEvent(e);

      // TODO: toby - warning, this stops drag events on nodes! (since it is above them!)
      // TODO: toby - container or svg? (use correct coords!)
      this.svgSelection = this.container.append('g')
        .append('rect')
        .attr('class', 'svg-selection')
        .attr('id', '__svg-selection__')
        .attr('fill', '#0000FF11')
        .attr('r', 100)
        .attr('stroke', '#00000055')
        .attr('stroke-width', 0.5)
        .attr('visibility', 'visible')
      ;

      this.positionSvgSelection();
    },
    mouseMove (e) { // only consistently-triggered on shift-drag (otherwise eaten by zoom)
      if (!this.shiftDragging) { return; }

      this.shiftDragEnd = this.worldSpacePointFromMouseEvent(e);
      this.positionSvgSelection();
    },
    mouseUp (e) { // only consistently-triggered on shift-drag (otherwise eaten by zoom)
      this.shiftDragging = false;

      const selected = [];
      this.node.each(d => {
        if (this.pointInBlockSelection(d)) {
          selected.push(d);
        }
      });

      this.nodeSelectionAddAll(selected);

      this.removeSvgSelection();
    },
    pointInBlockSelection (point) {
      return point.x >= this.blockSelectionTopLeft.x && point.x <= this.blockSelectionBottomRight.x &&
        point.y >= this.blockSelectionTopLeft.y && point.y <= this.blockSelectionBottomRight.y;
    },
    drawGraphWrapper () {
      import(/* webpackChunkName: "d3" */ 'd3').then((d3Module) => {
        d3 = d3Module;
        this.drawGraph(this.cont3xtData);
      });
    },
    /** @param data {{ nodes: any[], links: any[] }} */
    drawGraph (cont3xtData) {
      if (this.svg) { // remove any existing nodes
        this.node.exit().remove();
        this.svg.selectAll('.node').remove();

        this.link.exit().remove();
        this.svg.selectAll('.link').remove();

        this.nodeLabel.exit().remove();
        this.svg.selectAll('.node-label').remove();

        this.removeSvgSelection();

        this.removeSvgNodeSelection();

        this.container.selectAll('g').remove();
      }

      // create the node & link data from cont3xtData map!
      // const idRegex = /[\[\]:., ]/g;

      this.dataNodes = [];
      this.dataLinks = [];
      const nodeStrLookup = new Map(); // [string]: number (index)
      for (const [type, valueMap] of Object.entries(cont3xtData)) {
        for (const [value, { edges }] of Object.entries(valueMap)) {
          const nodeId = `${type},${value}`;
          nodeStrLookup.set(nodeId, this.dataNodes.length);

          this.dataNodes.push({ id: nodeId, value, type });

          for (const [linkedType, linkedValues] of Object.entries(edges)) {
            for (const linkedValue of linkedValues) { // four-ply nested for loop D:
              const linkedNodeId = `${linkedType},${linkedValue}`;
              this.dataLinks.push({ source: nodeId, target: linkedNodeId });
            }
          }
        }
      }

      // get the node and link data
      this.nodes = this.dataNodes.map(d => Object.create(d));
      // TODO: toby - THIS below
      this.links = this.dataLinks
        .map(({ source, target }) => ({ source: nodeStrLookup.get(source), target: nodeStrLookup.get(target) }))
        .filter(({ source, target }) => source != null && target != null);

      // create directed/undirected edge sets (for highlighting)
      const directedEdgeStrs = this.links.map(
        d => `${d.source},${d.target}`
      );
      const directedBackEdgeStrs = this.links.map(
        d => `${d.target},${d.source}`
      );
      this.directedLinkIndexPairSet = new Set(directedEdgeStrs); // TODO: unused
      // TODO: toby - use concat?
      this.undirectedLinkIndexPairSet = new Set([...directedEdgeStrs, ...directedBackEdgeStrs]);

      // TODO: toby - rm
      // console.log('toby', this.nodes, this.links);
      // this.links = data.links.map(d => Object.create(d));

      // TODO: toby - remove below
      // this.links = data.links.map(d => Object.create(d));
      // this.nodes = data.nodes.map(d => Object.create(d));

      this.nodeSelection = new Set();
      this.nodeSelectionArr = [];

      this.hiddenNodes = [];
      this.hiddenNodesSet = new Set();

      const nodeDist = 200;

      // setup the force directed graph
      this.simulation = d3.forceSimulation(this.nodes)
        .force('link', d3.forceLink(this.links).distance(20).strength(2)) // TODO: toby
        // .force('link',
        //   d3.forceLink(this.links).id((d) => {
        //     console.log('toby node/link?:', d);
        //     return d.pos; // tell the links where to link
        //   }).distance(10) // set the link distance
        //   // }).distance(this.query.nodeDist) // set the link distance
        // )
        // simulate gravity mutually amongst all nodes
        .force('charge', d3.forceManyBody().strength(-nodeDist * 2))
        // prevent elements from overlapping
        // .force('collision', d3.forceCollide().radius(this.calculateCollisionRadius))
        .force('collision', d3.forceCollide().radius(10))
        // set the graph center
        .force('center', d3.forceCenter(this.width / 2, this.height / 2).strength(0.3))
        // positioning force along x-axis for disjoint graph
        .force('x', d3.forceX())
        // positioning force along y-axis for disjoint graph
        .force('y', d3.forceY());

      if (!this.svg) {
        // initially procure & set the width and height of the canvas
        this.svg = d3.select('svg')
          .attr('width', this.width)
          .attr('height', this.height)
          .attr('id', 'graphSvg');
      }

      if (!this.container) { // TODO: toby - zoomContainer
        // add container for zoom/pan-ability
        this.container = this.svg.append('g');
      }

      d3.select('svg').node().oncontextmenu = () => false;

      // add zoomability
      this.svg.call(
        this.zoom = d3.zoom()
          .filter(() => !this.shiftDragging)
          .scaleExtent([0.1, 4])
          .on('zoom', (e) => {
            // don't zoom when shift-dragging (block selecting)
            if (this.panningMouseDown) {
              // TODO: toby - comment
              this.panningMouseDownHasMoved = true;
            }

            if (this.shiftDragging) { return; }
            this.containerTransform = e.transform;
            this.container.attr('transform', e.transform);
            // this.container.attr('transform', `translate(${e.transform.x}, ${e.transform.y})\nscale(${e.transform.k})`);
          })
          .on('end', e => {
            // if a panning-type click did not move (or only moved about a pixel, as this was probably not intended as a pan)
            if (this.panningMouseDown && !this.panningMouseDownHasMoved) {
              this.clearNodeSelection();
              // TODO: toby - do or no?
              // this.panningMouseDownStartPoint
            }
            this.panningMouseDown = false;
          })
      );

      // TODO: toby - ?
      /* eslint-disable no-useless-escape */
      const idRegex = /[\[\]:., ]/g;

      // add links
      this.link = this.container.append('g')
        .attr('stroke', '#acacac')
        .attr('stroke-opacity', 0.4)
        .selectAll('line')
        .data(this.links)
        .enter().append('line')
        .attr('class', 'link')
        .attr('stroke-width', 3)
        .attr('visibility', 'visible');

      // TODO: toby - does selectALl('circle make sense - will that mess with selections')
      // add nodes
      this.node = this.container.append('g')
        .attr('id', 'node-group')
        .selectAll('g')
        .data(this.nodes)
        .enter()
        .append('g')
        .attr('transform', 'translate(-10, -10)')
        .append('image')
        .attr('class', 'node')
        .attr('style', d => nodeImageFillColorFilters[d.type])
        .attr('id', (d) => {
          return 'id' + d.id.replace(idRegex, '_');
        })
        // TODO: toby - dimens
        .attr('width', 20) // r * 2
        .attr('height', 20)
        .attr('href', d => getImageForType(d.type))
        .attr('fill', '#FF0000')
        // TODO: toby - cx/cy don't work .. NEED offset!
        // .attr('fill', (d) => {
        //   // return '#FF0000';
        //   return nodeFillColors[d.type];
        // })
        // .attr('r', 8)
        // .attr('stroke', '#555555')
        // .attr('stroke-width', 0.5)
        .attr('visibility', 'visible')
        // .attr('r', this.calculateNodeWeight)
        // .attr('stroke', this.foregroundColor)
        // .attr('stroke-width', 0.5)
        // .attr('visibility', this.calculateNodeBaselineVisibility)
        .on('contextmenu', (e, { index: i }) => {
          this.contextMenu = { x: e.pageX, y: e.pageY, node: this.nodeIndexToNode(i), i };
          console.log('toby', e);
          console.log('toby', this.nodeIndexToNode(i));
        })
        .call(d3.drag()
          .on('start', this.dragstarted)
          .on('drag', this.dragged)
          .on('end', this.dragended)
        );
      ;

      this.updateSvgNodeSelection();

      // add node labels
      this.nodeLabel = this.container.append('g')
        .selectAll('text')
        .data(this.nodes)
        .enter()
        .append('text')
        .attr('dx', 10) // TODO: variable to account for radius
        .attr('id', (d) => {
          return 'id' + d.id.replace(idRegex, '_') + '-label';
        })
        .attr('dy', '2px')
        .attr('class', 'node-label')
        .style('font-size', this.fontSize + 'em')
        .style('font-weight', 'normal') // TODO: ?
        .style('font-style', 'normal') // TODO: italic on something?
        .attr('visibility', 'visible')
        .style('pointer-events', 'none') // to prevent mouseover/drag capture
        .text((d) => d.value);

      // listen on each tick of the simulation's internal timer
      this.simulation.on('tick', () => {
        // position links
        this.link
          .attr('x1', d => d.source.x)
          .attr('y1', d => d.source.y)
          .attr('x2', d => d.target.x)
          .attr('y2', d => d.target.y);

        // position nodes
        this.node
          .attr('x', d => d.x)
          .attr('y', d => d.y);

        this.svgNodeSelection
          ?.attr('cx', d => d.x)
          ?.attr('cy', d => d.y);

        // position node labels
        this.nodeLabel
          .attr('transform', d => `translate(${d.x}, ${d.y})`);
      });
    },
    // TODO: toby - remove this?
    renderGraph () {
      this.drawGraphWrapper();
    },
    observeGraphResize () {
      this.$nextTick(() => {
        this.resizeObserver = new ResizeObserver(resizeEntries => {
          // some browsers return a single entry rather than an array
          const resizeEntry = Array.isArray(resizeEntries) ? resizeEntries[0] : resizeEntries;

          const rectWidth = resizeEntry?.contentRect?.width;
          const rectHeight = resizeEntry?.contentRect?.height;

          if (rectWidth != null && rectHeight != null) {
            this.setGraphSize(rectWidth, rectHeight);
          }
        });
        this.resizeObserver.observe(this.$refs.graphContainer);
      });
    },
    setGraphSize (width, height) {
      this.width = width;
      this.height = height;

      this.svg
        ?.attr('width', this.width)
        ?.attr('height', this.height);
    }
  },
  mounted () {
    this.observeGraphResize();
    setTimeout(() => {
      if (this.indicator != null && (this.getLoading.done || this.getLoading.total === 0)) {
        this.renderFromData();
      } else {
        // TODO: toby - this is debugging only (though maybe we should render an empty graph here to init elems?)
        this.renderGraph();
      }
    }, 50);
  },
  beforeDestroy () {
    this.resizeObserver.disconnect();
  }

};
</script>
<style>
.connections-graph {
  /* don't allow selecting text */
  -webkit-user-select: none;
  -moz-user-select: none;
  user-select: none;
}
</style>
<style scoped>
.right-click-menu {
  position: absolute;
}

.graph-svg-container {
  width: calc(100% - 5px); /* TODO: toby - no 5px */
  height: 100%;
  max-width: calc(100% - 5px);
  max-height: 100vh;

  min-height: 70vh; /* TODO: toby - testing rm */
  overflow: hidden;
}
.graph-config-row {
  /* box-shadow: 0px 4px 5px 0px rgba(0,0,0,0.75); */
  box-shadow: 0 6px 4px -4px rgba(0, 0, 0, 0.5);
}
.graph-sidebar {
  width: 300px;
  height: calc(100vh - calc(calc(calc(62px + 42px) + 42px) + 16px));
  /* background-color: red; */
  position: absolute;
  border-right: 1px solid gray;
  /* TODO: toby - position correctly */
  top: calc(calc(calc(62px + 42px) + 42px) + 16px);
  left: 16px;
}
</style>
