<template>

  <div class="connections-page">
    <svg></svg>
  </div>

</template>

<script>
import * as d3 from 'd3';

let colors, foregroundColor;
let simulation, svg, container;
let node, link, nodeLabel;
let draggingNode;

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
  return linkedByIndex[a.index + ',' + b.index] || linkedByIndex[b.index + ',' + a.index] || a.index === b.index;
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

// vue def
export default {
  name: 'Connections',
  mounted: function () {
    let styles = window.getComputedStyle(document.body);
    let primaryColor = styles.getPropertyValue('--color-primary').trim();
    let secondaryColor = styles.getPropertyValue('--color-tertiary').trim();
    let tertiaryColor = styles.getPropertyValue('--color-quaternary').trim();
    foregroundColor = styles.getPropertyValue('--color-foreground').trim() || '#212529';
    colors = ['', primaryColor, tertiaryColor, secondaryColor];

    this.loadData();
  },
  methods: {
    loadData: function () {
      let query = {
        length: 1000,
        start: 0,
        date: -1,
        srcField: 'srcIp',
        dstField: 'ip.dst:port',
        bounding: 'last',
        interval: 'auto',
        minConn: 1,
        nodeDist: 125,
        fields: 'totBytes,totDataBytes,totPackets,node'
      };

      this.$http.get('connections.json', { params: query })
        .then((response) => {
          this.processData(response.data);
          this.recordsFiltered = response.data.recordsFiltered;
        }, (error) => {
          // TODO
        });
    },
    processData: function (data) {
      // map which nodes are linked (for highlighting)
      data.links.forEach((d) => {
        linkedByIndex[d.source + ',' + d.target] = true;
      });

      // calculate the width and height of the canvas
      const width = $(window).width();
      const height = $(window).height() - 158;

      // get the node and link data
      const links = data.links.map(d => Object.create(d));
      const nodes = data.nodes.map(d => Object.create(d));

      // setup the force directed graph
      simulation = d3.forceSimulation(nodes)
        .force('link',
          d3.forceLink(links).id((d) => {
            return d.pos; // tell the links where to link
          }).distance(40) // TODO configurable this.query.nodeDist
        )
        // TODO make this configurable?
        .force('charge', d3.forceManyBody().strength(-75)) // simulate gravity mutually amongst all nodes
        .force('center', d3.forceCenter(width / 2, height / 2)) // set the center
        .force('x', d3.forceX()) // positioning force along x-axis for disjoint graph
        .force('y', d3.forceY()); // positioning force along y-axis for disjoint graph

      // set the width and height of the canvas
      svg = d3.select('svg')
        .attr('width', width)
        .attr('height', height);

      // add container for zoomability
      container = svg.append('g');

      // add zoomability
      svg.call(
        d3.zoom()
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
        .attr('stroke-width', (d) => {
          return Math.min(1 + Math.log(d.value), 12);
        });

      // add nodes
      node = container.append('g')
        .attr('stroke', foregroundColor)
        .attr('stroke-width', 0.5)
        .selectAll('circle')
        .data(nodes)
        .enter()
        .append('circle')
        // TODO drag
        .attr('id', (d) => {
          /* eslint-disable no-useless-escape */
          return 'id' + d.id.replace(/[\[\]:.]/g, '_');
        })
        .attr('fill', (d) => {
          return colors[d.type];
        })
        .attr('r', (d) => {
          return Math.min(3 + Math.log(d.sessions), 12);
        })
        .call(d3.drag()
          .on('start', dragstarted)
          .on('drag', dragged)
          .on('end', dragended)
        );

      // highlight connected nodes
      node.on('mouseover', focus)
        .on('mouseout', unfocus);

      // add node labels
      nodeLabel = container.append('g')
        .selectAll('text')
        .data(nodes)
        .enter()
        .append('text')
        .attr('dx', 6)
        .attr('dy', '.35em')
        .style('font-size', '0.35em')
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
    }
  }
};
</script>

<style scoped>
/* don't allow selecting text */
.connections-page {
  -webkit-user-select: none;
  -moz-user-select: none;
  user-select: none;
}

/* accommodate top navbars */
.connections-page svg {
  margin-top: 130px;
}

/* apply foreground theme color */
.connections-page svg {
  fill: var(--color-foreground, #333);
}
</style>
