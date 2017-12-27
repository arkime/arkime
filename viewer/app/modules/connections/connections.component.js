(function() {

  'use strict';

  // save frequently accessed elements
  let networkLabelElem, networkElem;

  // save visualization data
  let force, svgMain;

  /**
   * @class ConnectionsController
   * @classdesc Interacts with moloch stats page
   * @example
   * '<moloch-connections></moloch-connections>'
   */
  class ConnectionsController {

    /**
     * Initialize global variables for this controller
     * @param $scope              Angular application model object
     * @param $filter             Filters format the value of an expression
     * @param $compile            Compiles template and links scope and the template together
     * @param $location           Exposes browser address bar URL (based on the window.location)
     * @param $routeParams        Retrieve the current set of route parameters
     * @param ConnectionsService  Transacts connection data with the server
     * @param FieldService        Retrieves available fields from the server
     * @param UserService         Transacts users and user data with the server
     *
     * @ngInject
     */
    constructor($scope, $filter, $compile, $location, $routeParams,
      ConnectionsService, FieldService, UserService) {
      this.$scope             = $scope;
      this.$filter            = $filter;
      this.$compile           = $compile;
      this.$location          = $location;
      this.$routeParams       = $routeParams;
      this.ConnectionsService = ConnectionsService;
      this.FieldService       = FieldService;
      this.UserService        = UserService;
    }

    /* Callback when component is mounted and ready */
    $onInit() {
      this.UserService.getSettings()
        .then((response) => {
          this.settings = response;
          if (this.settings.timezone === undefined) {
            this.settings.timezone = 'local';
          }
        })
        .catch((error) => { this.settings = { timezone: 'local' }; });

      this.FieldService.get(true)
        .then((response) => {
          this.fields = response.concat([{dbField: 'ip.dst:port', exp: 'ip.dst:port'}])
                                .filter(function(a) {return a.dbField !== undefined;})
                                .sort(function(a,b) {return (a.exp > b.exp?1:-1);});
        })
        .catch((error) => { this.settings = { timezone: 'local' }; });

      // load route params
      this.query = {};
      this.query.length   = this.$routeParams.connLength || 100;
      this.query.srcField = this.$routeParams.srcField || 'srcIp';
      this.query.dstField = this.$routeParams.dstField || 'dstIp';
      this.query.nodeDist = parseInt(this.$routeParams.nodeDist || '125');
      this.query.minConn  = parseInt(this.$routeParams.minConn || '1');

      let styles = window.getComputedStyle(document.body);
      this.primaryColor   = styles.getPropertyValue('--color-primary').trim();
      this.secondaryColor = styles.getPropertyValue('--color-tertiary').trim();
      this.tertiaryColor  = styles.getPropertyValue('--color-quaternary').trim();
      this.colors = ['', this.primaryColor, this.tertiaryColor, this.secondaryColor];

      this.startD3();

      networkLabelElem = $('#networkLabel');

      // hide the footer so that there is more space for the graph
      $('footer').hide();

      /* LISTEN! */
      this.$scope.$on('change:search', (event, args) => {
        if (args.startTime && args.stopTime) {
          this.query.startTime  = args.startTime;
          this.query.stopTime   = args.stopTime;
          this.query.date       = null;
        } else if (args.date) {
          this.query.date      = args.date;
          this.query.startTime = null;
          this.query.stopTime  = null;
        }

        this.query.expression = args.expression;
        if (args.bounding) {this.query.bounding = args.bounding;}
        if (args.interval) {this.query.interval = args.interval;}

        // don't load data if it's already loading
        if (!this.loading) { this.loadData(); }
      });

      // watch for the url parameters to change and update the page
      // connLength, srcField, dstField, minConn, and nodeDist parameters
      // are managed by the connections component
      this.$scope.$on('$routeUpdate', (event, current) => {
        let change = false;

        let size = current.params.connLength || 100;
        if (size !== this.query.length) {
          change = true;
          this.query.length = size;
        }

        let srcField = current.params.srcField || 'srcIp';
        if (srcField !== this.query.srcField) {
          change = true;
          this.query.srcField = srcField;
        }

        let dstField = current.params.dstField || 'dstIp';
        if (dstField !== this.query.dstField) {
          change = true;
          this.query.dstField = dstField;
        }

        let nodeDist = current.params.nodeDist || 125;
        if (nodeDist !== this.query.nodeDist) {
          this.query.nodeDist = nodeDist;
          force.distance(this.query.nodeDist).start();
        }

        let minConn = current.params.minConn || 1;
        if (minConn !== this.query.minConn) {
          change = true;
          this.query.minConn = minConn;
        }

        if (change) { this.loadData(); }
      });
    }

    /* fired when controller's containing scope is destroyed */
    $onDestroy() { // cleanup all the d3 stuffs!
      // d3 doesn't have .off function to remove listeners, so use .on('listener', null)
      // http://stackoverflow.com/questions/20269384/how-do-you-remove-a-handler-using-a-d3-js-selector
      d3.behavior.zoom().on('zoom', null);
      d3.select(window).on('resize', null);
      d3.select(window).on('mouseup', null);

      this.svgRect.on('mousedown', null);
      this.svgRect.remove();

      this.node.on('mouseover', null);
      this.node.on('mousedown', null);
      this.node.on('mouseout', null);
      this.node = null;

      this.link.on('mouseover', null);
      this.link.on('mouseout', null);
      this.link = null;

      this.drag.on('drag', null);
      this.drag = null;

      force.on('tick', null);
      force = null;

      this.svg.selectAll('.link').remove();
      this.svg.selectAll('.node').remove();

      networkLabelElem.remove();
      networkLabelElem = null;

      svgMain.remove();
      svgMain = null;

      this.svg.remove();
      this.svg = null;

      // show the footer for all other pages
      $('footer').show();
    }

    /* sets up d3 graph and saves variables for use in other functions */
    startD3() {
      let self = this;

      self.trans      = [0,0];
      self.scale      = 1;
      self.width      = $(window).width();
      self.height     = $(window).height() - 166;
      self.popupTimer = null;
      networkElem     = $('#network');

      function redraw() {
        // store the last event data
        self.trans = d3.event.translate;
        self.scale = d3.event.scale;

        // transform the vis
        self.svg.attr('transform', 'translate(' + self.trans + ')' + ' scale(' + self.scale + ')');
      }

      self.zoom = d3.behavior.zoom()
        .translate([0,0])
        .scale(1)
        .scaleExtent([0.25,6])
        .on('zoom', redraw);

      svgMain = d3.select('#network').append('svg:svg')
        .attr('width', self.width)
        .attr('height', self.height);

      self.svg = svgMain.append('svg:g')
        .call(self.zoom)
        .append('svg:g');

      self.svgRect = self.svg.append('svg:rect')
        .attr('width', networkElem.width() + 1000)
        .attr('height', networkElem.height() + 1000)
        .attr('fill', 'white')
        .attr('id', 'zoomCanvas')
        .on('mousedown', function(){
          networkLabelElem.hide();
        });

      force = d3.layout.force()
        .gravity(0.05)
        .distance(self.query.nodeDist)
        .charge(-300)
        .size([self.width, self.height]);

      d3.select(window).on('resize', this.resize);
    }

    /* resizes the graph by setting new width and height */
    resize() {
      let width  = $(window).width();
      let height = $(window).height() - 166;

      svgMain.attr('width', width).attr('height', height);
      force.size([width, height]).resume();
    }

    /* removes existing nodes, update url parameters and retrieves data from
     * the connections service */
    loadData() {
      this.loading  = true;
      this.error    = false;

      networkLabelElem.hide();

      this.svg.selectAll('.link').remove();
      this.svg.selectAll('.node').remove();

      this.ConnectionsService.get(this.query)
        .then((response) => {
          this.loading = false;
          this.processData(response);
          this.recordsFiltered = response.recordsFiltered;
        })
        .catch((error) => {
          this.loading  = false;
          this.error    = error.text;
        });
    }

    changeLength() {
      this.$location.search('connLength', this.query.length);
      this.loadData();
    }

    changeSrcField() {
      this.$location.search('srcField', this.query.srcField);
      this.loadData();
    }

    changeDstField() {
      this.$location.search('dstField', this.query.dstField);
      this.loadData();
    }

    changeMinConn() {
      this.$location.search('minConn', this.query.minConn);
      this.loadData();
    }

    /* changes distance between the nodes and saves it in a url parameter */
    changeNodeDist() {
      force.distance(this.query.nodeDist).start();
      this.$location.search('nodeDist', this.query.nodeDist);
    }

    /* unlocks nodes in the graph */
    unlock() {
      this.svg.selectAll('.node circle').each(function(d) { d.fixed = 0; });
      force.resume();
    }

    dbField2Type(dbField) {
      for (let k = 0; k < this.fields.length; k++) {
        if (dbField === this.fields[k].dbField ||
            dbField === this.fields[k].rawField) {
          return this.fields[k].type;
        }
      }

      return undefined;
    }

    dbField2Exp(dbField) {
      for (let k = 0; k < this.fields.length; k++) {
        if (dbField === this.fields[k].dbField ||
            dbField === this.fields[k].rawField) {
          return this.fields[k].exp;
        }
      }

      return undefined;
    }

    processData(json) {
      if (!json.nodes) {
        this.error = 'No nodes returned from your query.';
        return;
      }

      let self = this;
      let doConvert = 0;
      doConvert |= (self.dbField2Type(self.query.srcField) === 'seconds')?1:0;
      doConvert |= (self.dbField2Type(self.query.dstField) === 'seconds')?2:0;

      if (doConvert) {
        let dateFilter = this.$filter('date');
        for (let i = 0; i < json.nodes.length; i++) {
          let dataNode = json.nodes[i];
          if (dataNode.type) {
            dataNode.id = dateFilter(dataNode.id);
          }
        }
      }

      force.nodes(json.nodes)
           .links(json.links)
           .start();

      // Highlighting
      let highlight_trans = 0.1,
          highlight_color = 'blue',
          focus_node = null,
          highlight_node = null;

      // Highlighting helpers
      let linkedByIndex = {};
      json.links.forEach(function(d) {
        linkedByIndex[d.source.index + ',' + d.target.index] = true;
      });

      function isConnected(a, b) {
        return linkedByIndex[a.index + ',' + b.index] || linkedByIndex[b.index + ',' + a.index] || a.index === b.index;
      }

      function set_focus(d) {
        if (highlight_trans<1)  {
          self.node.selectAll('circle').style('opacity', function(o) {
            return isConnected(d, o) ? 1 : highlight_trans;
          });

          self.node.selectAll('text').style('opacity', function(o) {
            return isConnected(d, o) ? 1 : highlight_trans;
          });

          self.link.style('opacity', function(o) {
            return o.source.index === d.index || o.target.index === d.index ? 1 : highlight_trans;
          });
        }
      }

      function set_highlight(d) {
        if (focus_node !== null) {
          d = focus_node;
        }
        highlight_node = d;

        if (highlight_color !== 'white') {
          self.node.selectAll('circle').style('stroke', function(o) {
            return isConnected(d, o) ? highlight_color : '#333';
          });
          self.node.selectAll('text').style('font-weight', function(o) {
            return isConnected(d, o) ? 'bold' : 'normal';
          });
          self.link.style('stroke', function(o) {
            return o.source.index === d.index || o.target.index === d.index ? highlight_color : '#ccc';
          });
        }
      }

      function exit_highlight() {
        highlight_node = null;
        if (focus_node === null) {
          if (highlight_color !== 'white') {
            self.node.selectAll('circle').style('stroke', '#333');
            self.node.selectAll('text').style('font-weight', 'normal');
            self.link.style('stroke', '#ccc');
          }
        }
      }

      self.link = self.svg.selectAll('.link')
        .data(json.links)
        .enter().append('line')
        .attr('class', 'link')
        .style('stroke-width', function(d) { return Math.min(1 + Math.log(d.value), 12);})
        .style('stroke', '#ccc')
        .on('mouseover',function(d) {
          if (self.popupTimer) {
            window.clearTimeout(self.popupTimer);
          }
          self.popupTimer = window.setTimeout(self.showLinkPopup, 800, self, d, d3.mouse(this));
        }).on('mouseout',function(d){
          window.clearTimeout(self.popupTimer);
        });

      self.drag = d3.behavior.drag()
        .origin(function(d) { return d; })
        .on('dragstart', function (d) {
          networkLabelElem.hide();
          d3.event.sourceEvent.stopPropagation();
          d3.select(this).classed('dragging', true);
          d.fixed |= 1;
        })
        .on('drag', function (d) {d.px = d3.event.x; d.py = d3.event.y; force.resume();})
        .on('dragend', function (d) {
          d3.select(this).classed('dragging', false);
        });

      self.node = self.svg.selectAll('.node')
        .data(json.nodes)
        .enter().append('g')
        .attr('class', 'node')
        .attr('id', function(d) {return 'id' + d.id.replace(/[:\.]/g,'_');})
        //.attr('y', function (d,i) {return i;})
        .call(self.drag)
        .on('mouseover',function(d) {
          if (d3.select(this).classed('dragging')) {
            return;
          }

          if (self.popupTimer) {
            window.clearTimeout(self.popupTimer);
          }
          self.popupTimer = window.setTimeout(self.showNodePopup, 800, self, d);
          set_highlight(d);
        })
        .on('mousedown', function(d) {
          d3.event.stopPropagation();
          focus_node = d;
          set_focus(d);
          if (highlight_node === null) {
            set_highlight(d);
          }
        })
        .on('mouseout',function(d){
          window.clearTimeout(self.popupTimer);
          exit_highlight();
        });

      self.node.append('svg:circle')
        .attr('class', 'node')
        .attr('r', function(d) { return Math.min(3+Math.log(d.sessions), 12); })
        .style('fill', function(d) { return self.colors[d.type];});

      self.node.append('svg:text')
        .attr('dx', 12)
        .attr('dy', '.35em')
        .attr('class', 'connshadow')
        .text(function(d) { return d.id;});

      self.node.append('svg:text')
        .attr('dx', 12)
        .attr('dy', '.35em')
        .text(function(d) { return d.id;});

      force.on('tick', function() {
        self.link.attr('x1', function(d) { return d.source.x; })
            .attr('y1', function(d) { return d.source.y; })
            .attr('x2', function(d) { return d.target.x; })
            .attr('y2', function(d) { return d.target.y; });

        self.node.attr('transform', function(d) { return 'translate(' + d.x + ',' + d.y + ')'; });
      });

      d3.select(window).on('mouseup', function() {
        if(focus_node !== null) {
          focus_node = null;
          if (highlight_trans < 1) {
            self.node.selectAll('circle').style('opacity', 1);
            self.node.selectAll('text').style('opacity', 1);
            self.link.style('opacity', 1);
          }
        }
        if (highlight_node === null) {
          exit_highlight();
        }
      });
    }

    showNodePopup(that, node) {
      let template = `<connections-node-popup node="node" svg="svg"></connections-node-popup>`;

      if (node.type === 2) {
        node.exp = that.dbField2Exp(that.query.dstField);
      } else {
        node.exp = that.dbField2Exp(that.query.srcField);
      }
      that.$scope.node = node;
      that.$scope.svg  = that.svg;
      let content = that.$compile(template)(that.$scope);
      that.$scope.$apply();

      that.positionPopup(node.px, node.py, content);
    }

    showLinkPopup(that, link, mouse) {
      let template = `<connections-link-popup link="link" svg="svg"></connections-link-popup>`;

      link.dstExp = that.dbField2Exp(that.query.dstField);
      link.srcExp = that.dbField2Exp(that.query.srcField);

      that.$scope.link = link;
      that.$scope.svg  = that.svg;
      let content = that.$compile(template)(that.$scope);
      that.$scope.$apply();

      that.positionPopup(mouse[0], mouse[1], content);
    }

    /**
     * Positions and displays a popup on the connections graph within the view
     * @param {num} px      The x coordinate of the mouse/node
     * @param {num} py      The y coordinate of the mouse/node
     * @param {obj} content The content to display in the popup
     */
    positionPopup(px, py, content) {
      let x = -180; // 180 = width of popup + 10 padding

      // if the position node is too far to the left to accommodate the popup,
      // render it on the right of the node instead of the left
      if (this.trans[0] + (px * this.scale) < 180) { x = 10; }

      networkLabelElem.html(content)
        .css({
          left: this.trans[0] + (px * this.scale) + x,
          top : this.trans[1] + (py * this.scale) -20
        }).show();
    }

    /**
     * Displays the field.exp instead of field.dbField in the field typeahead
     * @param {string} value The dbField of the field
     */
    formatField(value) {
      for (let i = 0, len = this.fields.length; i < len; i++) {
        if (value === this.fields[i].dbField) {
          return this.fields[i].exp;
        }
      }
    }
  }

  ConnectionsController.$inject = ['$scope','$filter','$compile','$location','$routeParams',
    'ConnectionsService','FieldService','UserService'];

  /**
   * Moloch Connections Directive
   */
  angular.module('moloch')
     .component('molochConnections', {
       template  : require('./connections.html'),
       controller: ConnectionsController
     });

})();
