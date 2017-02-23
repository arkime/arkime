(function() {

  'use strict';

  /**
   * @class ConnectionsController
   * @classdesc Interacts with moloch stats page
   * @example
   * '<moloch-connections></moloch-connections>'
   */
  class ConnectionsController {

    /**
     * Initialize global variables for this controller
     * @param ConnectionsService  Transacts stats with the server
     * @param FieldService        Retrieves available fields from the server
     * @param UserService         Transacts users and user data with the server
     *
     * @ngInject
     */
    constructor($scope, $http, $routeParams, $compile, $filter, ConnectionsService, FieldService, UserService) {
      this.$scope             = $scope;
      this.$http              = $http;
      this.$routeParams       = $routeParams;
      this.$compile           = $compile;
      this.$filter            = $filter;
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
        .catch((error)   => {this.settings = {timezone: 'local'}; });

      this.FieldService.get(true)
        .then((response) => {
          this.fields = response.concat([{dbField: "ip.dst:port", exp: "ip.dst:port"}])
                                .filter(function(a) {return a.dbField !== undefined;})
                                .sort(function(a,b) {return (a.exp > b.exp?1:-1);});
        })
        .catch((error)   => {this.settings = {timezone: 'local'}; });


      this.querySize = 100;
      this.srcField = "a1";
      this.dstField = "a2";
      this.nodeDist = parseInt(this.$routeParams.nodeDist || "125");
      this.minConn = 1;

      this.startD3();

      this.$scope.$on('change:search', (event, args) => {
        this.loadData(args);
      });
    }

    startD3() {
      var self = this;

      self.trans=[0,0];
      self.scale=1;
      self.width = $(window).width();
      self.height = $(window).height() - 85;
      self.popupTimer = null;
      self.colors = ["", "green", "red", "purple"];

      function redraw() {
        //store the last event data
        self.trans = d3.event.translate;
        self.scale = d3.event.scale;

        //transform the vis
         self.svg.attr("transform", "translate(" + self.trans + ")" + " scale(" + self.scale + ")");
      }

      self.zoom = d3.behavior.zoom()
        .translate([0,0])
        .scale(1)
        .scaleExtent([0.25,6])
        .on("zoom", redraw);

      self.svgMain = d3.select("#network").append("svg:svg")
        .attr("width", self.width)
        .attr("height", self.height);

      self.svg = self.svgMain.append('svg:g')
        .call(self.zoom)
        .append('svg:g')
        ;

      self.svg.append('svg:rect')
        .attr('width', $("#network").width() + 1000)
        .attr('height', $("#network").height() + 1000)
        .attr('fill', 'white')
        .attr('id', 'zoomCanvas')
        .on("mousedown", function(){
          $('#networkLabel').hide();
        })
        ;

      self.force = d3.layout.force()
          .gravity(0.05)
          .distance(self.nodeDist)
          .charge(-300)
          .size([self.width, self.height])
          ;
    }

    loadData(params) {
      $('#networkLabel').hide();

      this.svg.selectAll(".link")
              .remove();

      this.svg.selectAll(".node")
              .remove();

      params.length   = this.graphSize;
      params.srcField = this.srcField;
      params.dstField = this.dstField;
      params.minConn  = this.minConn;
      params.nodeDist = this.nodeDist;

      this.$http({ url:'connections.json', method:'GET', cache:false, params: params })
        .then((response) => {
          this.processData(response.data);
        }, (error) => {
          console.log("ERROR", error);
        });
    }


    dbField2Type(dbField) {
      for (var k = 0; k < this.fields.length; k++) {
        if (dbField === this.fields[k].dbField ||
            dbField === this.fields[k].rawField) {
          return this.fields[k].type;
        }
      }
      return undefined;
    }

    dbField2Exp(dbField) {
      for (var k = 0; k < this.fields.length; k++) {
        if (dbField === this.fields[k].dbField ||
            dbField === this.fields[k].rawField) {
          return this.fields[k].exp;
        }
      }
      return undefined;
    }

    processData(json) {
      var self = this;
      var doConvert = 0;
      doConvert |= (self.dbField2Type(self.srcField) === "seconds")?1:0;
      doConvert |= (self.dbField2Type(self.dstField) === "seconds")?2:0;

      if (doConvert) {
        var dateFilter = this.$filter('date');
        for (var i = 0; i < json.nodes.length; i++) {
          let dataNode = json.nodes[i];
          if (doConvert & dataNode.type) {
            dataNode.id = dateFilter(dataNode.id);
          }
        }
      }

      $("#actionsForm").data("moloch-visible", +$("#graphSize").val())
                       .data("moloch-all", json.recordsFiltered);

      //updateHealth(json.health);
      //updateString("#bsqErr", json.bsqErr);

      self.force
          .nodes(json.nodes)
          .links(json.links)
          .start();

      //Highlighting
      var highlight_trans = 0.1,
          highlight_color = "blue",
          focus_node = null,
          highlight_node = null;

      var node, link;

      //Highlighting helpers
      var linkedByIndex = {};
      json.links.forEach(function(d) {
        linkedByIndex[d.source.index + "," + d.target.index] = true;
      });

      function isConnected(a, b) {
        return linkedByIndex[a.index + "," + b.index] || linkedByIndex[b.index + "," + a.index] || a.index === b.index;
      }

      function set_focus(d) {
        if (highlight_trans<1)  {
          node.selectAll("circle").style("opacity", function(o) {
            return isConnected(d, o) ? 1 : highlight_trans;
          });

          node.selectAll("text").style("opacity", function(o) {
            return isConnected(d, o) ? 1 : highlight_trans;
          });

          link.style("opacity", function(o) {
            return o.source.index === d.index || o.target.index === d.index ? 1 : highlight_trans;
          });
        }
      }

      function set_highlight(d) {
        if (focus_node !== null) {
          d = focus_node;
        }
        highlight_node = d;

        if (highlight_color !== "white") {
          node.selectAll("circle").style("stroke", function(o) {
            return isConnected(d, o) ? highlight_color : "#333";
          });
          node.selectAll("text").style("font-weight", function(o) {
            return isConnected(d, o) ? "bold" : "normal";
          });
          link.style("stroke", function(o) {
            return o.source.index === d.index || o.target.index === d.index ? highlight_color : "#ccc";
          });
        }
      }
      function exit_highlight() {
        highlight_node = null;
        if (focus_node === null) {
          if (highlight_color !== "white") {
            node.selectAll("circle").style("stroke", "#333");
            node.selectAll("text").style("font-weight", "normal");
            link.style("stroke", "#ccc");
          }
        }
      }

      link = self.svg.selectAll(".link")
          .data(json.links)
          .enter().append("line")
          .attr("class", "link")
          .style("stroke-width", function(d) { return Math.min(1 + Math.log(d.value), 12);})
          .on("mouseover",function(d) {
            if (self.popupTimer) {
              window.clearTimeout(self.popupTimer);
            }
            self.popupTimer = window.setTimeout(self.showLinkPopup, 800, self, d, d3.mouse(this));
          }).on("mouseout",function(d){
            window.clearTimeout(self.popupTimer);
          })
          ;
      var drag = d3.behavior.drag()
        .origin(function(d) { return d; })
        .on("dragstart", function (d) {
          $('#networkLabel').hide();
          d3.event.sourceEvent.stopPropagation();
          d3.select(this).classed("dragging", true);
          d.fixed |= 1;
        })
        .on("drag", function (d) {d.px = d3.event.x; d.py = d3.event.y; self.force.resume();})
        .on("dragend", function (d) {
          d3.select(this).classed("dragging", false);
        });

      node = self.svg.selectAll(".node")
          .data(json.nodes)
          .enter().append("g")
          .attr("class", "node")
          .attr("id", function(d) {return "id" + d.id.replace(/[:\.]/g,"_");})
          //.attr("y", function (d,i) {return i;})
          .call(drag)
          .on("mouseover",function(d) {
            if (d3.select(this).classed("dragging")) {
              return;
            }

            if (self.popupTimer) {
              window.clearTimeout(self.popupTimer);
            }
            self.popupTimer = window.setTimeout(self.showNodePopup, 800, self, d);
            set_highlight(d);
          })
          .on("mousedown", function(d) {
            d3.event.stopPropagation();
            focus_node = d;
            set_focus(d);
            if (highlight_node === null) {
              set_highlight(d);
            }
          })
          .on("mouseout",function(d){
            window.clearTimeout(self.popupTimer);
            exit_highlight();
          })
          ;

      node.append("svg:circle")
        .attr("class", "node")
        .attr("r", function(d) { return Math.min(3+Math.log(d.sessions), 12); })
        .style("fill", function(d) { return self.colors[d.type];})
        ;



      node.append("svg:text")
          .attr("dx", 12)
          .attr("dy", ".35em")
          .attr("class", "connshadow")
          .text(function(d) { return d.id;});

      node.append("svg:text")
          .attr("dx", 12)
          .attr("dy", ".35em")
          .text(function(d) { return d.id;});

      self.force.on("tick", function() {
        link.attr("x1", function(d) { return d.source.x; })
            .attr("y1", function(d) { return d.source.y; })
            .attr("x2", function(d) { return d.target.x; })
            .attr("y2", function(d) { return d.target.y; });

        node.attr("transform", function(d) { return "translate(" + d.x + "," + d.y + ")"; });
      });


      d3.select(window).on("mouseup", function() {
        if(focus_node !== null) {
          focus_node = null;
          if (highlight_trans < 1) {
            node.selectAll("circle").style("opacity", 1);
            node.selectAll("text").style("opacity", 1);
            link.style("opacity", 1);
          }
        }
        if (highlight_node === null) {
          exit_highlight();
        }
      });
    }

    safeStr(str) {
      if (str === undefined) {
        return "";
      }

      if (Array.isArray(str)) {
        return str.map(this.safeStr);
      }

      return str.replace(/&/g,'&amp;').replace(/</g,'&lt;').replace(/>/g,'&gt;').replace(/\"/g,'&quot;').replace(/\'/g, '&#39;').replace(/\//g, '&#47;');
    }
// From http://stackoverflow.com/a/2901298
    numberWithCommas(x) {
      if (x === undefined) {
        return "0";
      }
      return x.toString().replace(/\B(?=(\d{3})+(?!\d))/g, ",");
    }

    showNodePopup(that, node) {
      let template = `<connections-node-popup node="node" svg="svg"></connections-node-popup>`;

      if (node.type === 2) {
        node.exp = that.dbField2Exp(that.dstField);
      } else {
        node.exp = that.dbField2Exp(that.srcField);
      }
      that.$scope.node = node;
      that.$scope.svg  = that.svg;
      let content = that.$compile(template)(that.$scope);
      that.$scope.$apply();

      $('#networkLabel').html(content)
                        .css({ left: that.trans[0] + (node.px*that.scale),
                                top: that.trans[1] + (node.py*that.scale) + $('#networkLabel').height()/2
                            })
                        .show()
                        ;
    }

    showLinkPopup(that, link, mouse) {
      let template = `<connections-link-popup link="link" svg="svg"></connections-link-popup>`;

      link.dstExp = that.dbField2Exp(that.dstField);
      link.srcExp = that.dbField2Exp(that.srcField);

      that.$scope.link = link;
      that.$scope.svg  = that.svg;
      let content = that.$compile(template)(that.$scope);
      that.$scope.$apply();

      $('#networkLabel').html(content)
                        .css({ left: that.trans[0] + (mouse[0]*that.scale) + 25,
                                top: that.trans[1] + (mouse[1]*that.scale) + $('#networkLabel').height()/2 + 25
                            })
                        .show()
                        ;
    }
  }

ConnectionsController.$inject = ['$scope', '$http', '$routeParams', '$compile', '$filter', 'ConnectionsService', 'FieldService', 'UserService'];

  /**
   * Moloch Connections Directive
   */
  angular.module('moloch')
     .component('molochConnections', {
       template  : require('html!./connections.html'),
       controller: ConnectionsController
     });

})();
