<template>

  <div class="connections-page">

    <!-- search navbar -->
    <moloch-search
      :start="query.start"
      :timezone="settings.timezone"
      @changeSearch="loadData">
    </moloch-search> <!-- /search navbar -->

    <!-- connections sub navbar -->
    <form class="connections-form">
      <div class="form-inline pr-1 pl-1 pt-1 pb-1">

        <!-- query size select -->
        <div class="input-group input-group-sm">
          <div class="input-group-prepend help-cursor"
            v-b-tooltip.hover
            title="Query Size">
            <span class="input-group-text">
              Query Size
            </span>
          </div>
          <select class="form-control input-sm"
            v-model="query.length"
            v-on:change="changeLength()">
            <option value="100">100</option>
            <option value="500">500</option>
            <option value="1000">1,000</option>
            <option value="5000">5,000</option>
            <option value="10000">10,000</option>
            <option value="50000">50,000</option>
            <option value="100000">100,000</option>
          </select>
        </div> <!-- /query size select -->

        <!-- src select -->
        <div class="form-group ml-1"
          v-if="fields && fields.length && srcFieldTypeahead">
          <div class="input-group input-group-sm">
            <span class="input-group-prepend legend cursor-help"
              v-b-tooltip.hover
              title="Select a field for the source nodes">
              <span class="input-group-text"
                :style="{'background-color': primaryColor + '!important'}">
                Src:
              </span>
            </span>
            <moloch-field-typeahead
              :fields="fields"
              query-param="srcField"
              :initial-value="srcFieldTypeahead"
              @fieldSelected="changeSrcField">
            </moloch-field-typeahead>
          </div>
        </div> <!-- /src select -->

        <!-- dst select -->
        <div class="form-group ml-1"
          v-if="fields && dstFieldTypeahead">
          <div class="input-group input-group-sm">
            <span class="input-group-prepend legend cursor-help"
              v-b-tooltip.hover
              title="Select a field for the source nodes">
              <span class="input-group-text"
                :style="{'background-color': tertiaryColor + '!important'}">
                Dst:
              </span>
            </span>
            <moloch-field-typeahead
              :fields="fields"
              query-param="dstField"
              :initial-value="dstFieldTypeahead"
              @fieldSelected="changeDstField">
            </moloch-field-typeahead>
          </div>
        </div> <!-- /dst select -->

        <!-- src & dst color -->
        <div class="form-group ml-1">
          <div class="input-group input-group-sm">
            <span class="input-group-prepend legend cursor-help"
              v-b-tooltip.hover
              title="This is the color of a node that is both a source and destination node">
              <span class="input-group-text"
                style="border-radius: 4px"
                :style="{'background-color': secondaryColor + '!important'}">
                Src & Dst
              </span>
            </span>
          </div>
        </div> <!-- /src & dst color -->

        <!-- min connections select -->
        <div class="input-group input-group-sm ml-1">
          <div class="input-group-prepend help-cursor"
            v-b-tooltip.hover
            title="Min connections">
            <span class="input-group-text">
              Min. Connections
            </span>
          </div>
          <select class="form-control input-sm"
            v-model="query.minConn"
            v-on:change="changeMinConn()">
            <option value="1">1</option>
            <option value="2">2</option>
            <option value="3">3</option>
            <option value="4">4</option>
            <option value="5">5</option>
          </select>
        </div> <!-- /min connections select -->

        <!-- node dist select -->
        <div class="input-group input-group-sm ml-1">
          <div class="input-group-prepend help-cursor"
            v-b-tooltip.hover
            title="Node distance in pixels">
            <span class="input-group-text">
              Node distance
            </span>
          </div>
          <select class="form-control input-sm"
            v-model="query.nodeDist"
            v-on:change="changeNodeDist()">
            <option value="75">75</option>
            <option value="100">100</option>
            <option value="125">125</option>
            <option value="150">150</option>
            <option value="200">200</option>
            <option value="250">250</option>
          </select>
        </div> <!-- /node dist select -->

        <!-- unlock button-->
        <button class="btn btn-default btn-sm ml-1"
          @click.stop.prevent="unlock()">
          <span class="fa fa-unlock"></span>&nbsp;
          Unlock
        </button> <!-- /unlock button-->
      </div>
    </form>

    <div class="connections-content ml-2 mr-2">

      <!-- loading overlay -->
      <moloch-loading
        v-if="loading && !error">
      </moloch-loading> <!-- /loading overlay -->

      <!-- page error -->
      <moloch-error
        v-if="error"
        :message="error"
        class="mt-5 mb-5">
      </moloch-error> <!-- /page error -->

      <!-- no results -->
      <moloch-no-results
        v-if="!error && !loading && recordsFiltered === 0"
        class="mt-5 mb-5"
        :records-total="recordsFiltered"
        :view="query.view">
      </moloch-no-results> <!-- /no results -->

      <div class="network-container"
        v-show="!error && recordsFiltered !== 0">
        <div id="network"></div>
        <div class="node-popup connections-popup" ref="nodePopup"></div>
        <div class="link-popup connections-popup" ref="linkPopup"></div>
      </div>

    </div>

  </div>

</template>

<script>
import Vue from 'vue';
import UserService from '../users/UserService';
import MolochSearch from '../search/Search';
import FieldService from '../search/FieldService';
import MolochPaging from '../utils/Pagination';
import ToggleBtn from '../utils/ToggleBtn';
import MolochError from '../utils/Error';
import MolochLoading from '../utils/Loading';
import MolochNoResults from '../utils/NoResults';
import MolochFieldTypeahead from '../utils/FieldTypeahead';

import d3 from '../../../../public/d3.min.js';

// save frequently accessed elements
let networkElem;
let nodePopupVue;
let linkPopupVue;

// save visualization data
let force, svgMain;

export default {
  name: 'Connections',
  components: {
    MolochSearch,
    MolochPaging,
    ToggleBtn,
    MolochError,
    MolochLoading,
    MolochNoResults,
    MolochFieldTypeahead
  },
  data: function () {
    return {
      user: null,
      loading: true,
      error: '',
      settings: {}, // user settings
      recordsFiltered: 0,
      styles: null,
      primaryColor: null,
      secondaryColor: null,
      tertiaryColor: null,
      fields: [],
      srcFieldTypeahead: undefined,
      dstFieldTypeahead: undefined
    };
  },
  computed: {
    query: function () {
      return {
        length: this.$route.query.length || 100, // page length
        start: 0, // first item index
        date: this.$store.state.timeRange,
        startTime: this.$store.state.time.startTime,
        stopTime: this.$store.state.time.stopTime,
        srcField: this.$route.query.srcField || 'srcIp',
        dstField: this.$route.query.dstField || 'dstIp',
        bounding: this.$route.query.bounding || 'last',
        interval: this.$route.query.interval || 'auto',
        minConn: this.$route.query.minConn || 1,
        nodeDist: this.$route.query.nodeDist || 125,
        view: this.$route.query.view || undefined,
        expression: this.$store.state.expression || undefined
      };
    }
  },
  watch: {
    '$route.query.length': function (newVal, oldVal) {
      this.loadData();
    },
    '$route.query.minConn': function (newVal, oldVal) {
      this.loadData();
    },
    '$route.query.nodeDist': function (newVal, oldVal) {
      force.distance(this.query.nodeDist).start();
    },
    '$route.query.srcField': function (newVal, oldVal) {
      this.loadData();
    },
    '$route.query.dstField': function (newVal, oldVal) {
      this.loadData();
    }
  },
  created: function () {
    this.loadUser();
    FieldService.get(true)
      .then((result) => {
        this.fields = result;
        for (let field of this.fields) {
          if (field.dbField === this.query.srcField) {
            this.srcFieldTypeahead = field.friendlyName;
          }
          if (field.dbField === this.query.dstField) {
            this.dstFieldTypeahead = field.friendlyName;
          }
        }
      }).catch((error) => {
        this.loading = false;
        this.error = error;
      });
  },
  mounted: function () {
    let styles = window.getComputedStyle(document.body);
    this.primaryColor = styles.getPropertyValue('--color-primary').trim();
    this.secondaryColor = styles.getPropertyValue('--color-tertiary').trim();
    this.tertiaryColor = styles.getPropertyValue('--color-quaternary').trim();
    this.colors = ['', this.primaryColor, this.tertiaryColor, this.secondaryColor];

    this.startD3();
    this.loadData();
    $('.footer').hide();
  },
  beforeDestroy: function () {
    this.endD3();
    $('.footer').show();
  },
  methods: {
    /* exposed page functions ---------------------------------------------- */
    changeLength: function () {
      this.$router.push({
        query: {
          ...this.$route.query,
          length: this.query.length
        }
      });
    },
    changeMinConn: function () {
      this.$router.push({
        query: {
          ...this.$route.query,
          minConn: this.query.minConn
        }
      });
    },
    changeNodeDist: function () {
      this.$router.push({
        query: {
          ...this.$route.query,
          nodeDist: this.query.nodeDist
        }
      });
    },
    unlock: function () {
      this.svg.selectAll('.node circle').each(function (d) { d.fixed = 0; });
      force.resume();
    },
    closePopups: function () {
      $('.connections-popup').hide();
    },
    /* event functions ----------------------------------------------------- */
    changeSrcField: function (field) {
      this.srcFieldTypeahead = field.friendlyName;
      this.query.srcField = field.dbField;
      this.$router.push({
        query: {
          ...this.$route.query,
          srcField: this.query.srcField
        }
      });
    },
    changeDstField: function (field) {
      this.dstFieldTypeahead = field.friendlyName;
      this.query.dstField = field.dbField;
      this.$router.push({
        query: {
          ...this.$route.query,
          dstField: this.query.dstField
        }
      });
    },
    /* helper functions ---------------------------------------------------- */
    loadUser: function () {
      UserService.getCurrent()
        .then((response) => {
          this.user = response;
        }, (error) => {
          this.user = { settings: { timezone: 'local' } };
        });
    },
    loadData: function () {
      this.loading = true;
      this.error = false;

      this.svg.selectAll('.link').remove();
      this.svg.selectAll('.node').remove();

      this.$http.get('connections.json', { params: this.query })
        .then((response) => {
          this.error = '';
          this.loading = false;
          this.processData(response.data);
          this.recordsFiltered = response.data.recordsFiltered;
        }, (error) => {
          this.loading = false;
          this.error = error;
        });
    },
    dbField2Type: function (dbField) {
      for (let k in this.fields) {
        if (dbField === this.fields[k].dbField ||
            dbField === this.fields[k].rawField) {
          return this.fields[k].type;
        }
      }

      return undefined;
    },
    dbField2Exp: function (dbField) {
      for (let k in this.fields) {
        if (dbField === this.fields[k].dbField ||
            dbField === this.fields[k].rawField) {
          return this.fields[k].exp;
        }
      }

      return undefined;
    },
    processData: function (json) {
      if (!json.nodes) {
        this.error = 'No nodes returned from your query.';
        return;
      }

      let self = this;
      let doConvert = 0;
      doConvert |= (self.dbField2Type(self.query.srcField) === 'seconds') ? 1 : 0;
      doConvert |= (self.dbField2Type(self.query.dstField) === 'seconds') ? 2 : 0;

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
      let highlightTrans = 0.1;
      let highlightColor = 'blue';
      let focusNode = null;
      let highlightNode = null;

      // Highlighting helpers
      let linkedByIndex = {};
      json.links.forEach(function (d) {
        linkedByIndex[d.source.index + ',' + d.target.index] = true;
      });

      function isConnected (a, b) {
        return linkedByIndex[a.index + ',' + b.index] || linkedByIndex[b.index + ',' + a.index] || a.index === b.index;
      }

      function setFocus (d) {
        if (highlightTrans < 1) {
          self.node.selectAll('circle').style('opacity', function (o) {
            return isConnected(d, o) ? 1 : highlightTrans;
          });

          self.node.selectAll('text').style('opacity', function (o) {
            return isConnected(d, o) ? 1 : highlightTrans;
          });

          self.link.style('opacity', function (o) {
            return o.source.index === d.index || o.target.index === d.index ? 1 : highlightTrans;
          });
        }
      }

      function setHighlight (d) {
        if (focusNode !== null) {
          d = focusNode;
        }
        highlightNode = d;

        if (highlightColor !== 'white') {
          self.node.selectAll('circle').style('stroke', function (o) {
            return isConnected(d, o) ? highlightColor : '#333';
          });
          self.node.selectAll('text').style('font-weight', function (o) {
            return isConnected(d, o) ? 'bold' : 'normal';
          });
          self.link.style('stroke', function (o) {
            return o.source.index === d.index || o.target.index === d.index ? highlightColor : '#ccc';
          });
        }
      }

      function exitHighlight () {
        highlightNode = null;
        if (focusNode === null) {
          if (highlightColor !== 'white') {
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
        .style('stroke-width', function (d) { return Math.min(1 + Math.log(d.value), 12); })
        .style('stroke', '#ccc')
        .on('mouseover', function (d) {
          if (self.popupTimer) {
            window.clearTimeout(self.popupTimer);
          }
          self.popupTimer = window.setTimeout(self.showLinkPopup, 600, self, d, d3.mouse(this));
        }).on('mouseout', function (d) {
          window.clearTimeout(self.popupTimer);
        });

      self.drag = d3.behavior.drag()
        .origin(function (d) { return d; })
        .on('dragstart', function (d) {
          self.closePopups();
          d3.event.sourceEvent.stopPropagation();
          d3.select(this).classed('dragging', true);
          d.fixed |= 1;
        })
        .on('drag', function (d) { d.px = d3.event.x; d.py = d3.event.y; force.resume(); })
        .on('dragend', function (d) {
          d3.select(this).classed('dragging', false);
        });

      self.node = self.svg.selectAll('.node')
        .data(json.nodes)
        .enter().append('g')
        .attr('class', 'node')
        .attr('id', function (d) { return 'id' + d.id.replace(/[:.]/g, '_'); })
        // .attr('y', function (d,i) {return i; })
        .call(self.drag)
        .on('mouseover', function (d) {
          if (d3.select(this).classed('dragging')) {
            return;
          }

          if (self.popupTimer) {
            window.clearTimeout(self.popupTimer);
          }
          self.popupTimer = window.setTimeout(self.showNodePopup, 600, self, d);
          setHighlight(d);
        })
        .on('mousedown', function (d) {
          d3.event.stopPropagation();
          focusNode = d;
          setFocus(d);
          if (highlightNode === null) {
            setHighlight(d);
          }
        })
        .on('mouseout', function (d) {
          window.clearTimeout(self.popupTimer);
          exitHighlight();
        });

      self.node.append('svg:circle')
        .attr('class', 'node')
        .attr('r', function (d) { return Math.min(3 + Math.log(d.sessions), 12); })
        .style('fill', function (d) { return self.colors[d.type]; });

      self.node.append('svg:text')
        .attr('dx', 12)
        .attr('dy', '.35em')
        .attr('class', 'connshadow')
        .text(function (d) { return d.id; });

      self.node.append('svg:text')
        .attr('dx', 12)
        .attr('dy', '.35em')
        .text(function (d) { return d.id; });

      force.on('tick', function () {
        self.link.attr('x1', function (d) { return d.source.x; })
          .attr('y1', function (d) { return d.source.y; })
          .attr('x2', function (d) { return d.target.x; })
          .attr('y2', function (d) { return d.target.y; });

        self.node.attr('transform', function (d) { return 'translate(' + d.x + ',' + d.y + ')'; });
      });

      d3.select(window).on('mouseup', function () {
        if (focusNode !== null) {
          focusNode = null;
          if (highlightTrans < 1) {
            self.node.selectAll('circle').style('opacity', 1);
            self.node.selectAll('text').style('opacity', 1);
            self.link.style('opacity', 1);
          }
        }
        if (highlightNode === null) {
          exitHighlight();
        }
      });
    },
    showNodePopup: function (that, node, mouse) {
      if (node.type === 2) {
        node.exp = that.dbField2Exp(that.query.dstField);
      } else {
        node.exp = that.dbField2Exp(that.query.srcField);
      }

      this.closePopups();
      if (!nodePopupVue) {
        nodePopupVue = new Vue({
          template: `
            <div class="node-popup connections-popup">
              <div class="margined-bottom">
                <strong>{{node.id}}</strong>
                <a class="pull-right cursor-pointer no-decoration"
                  @click="closePopup()">
                  <span class="fa fa-close"></span>
                </a>
              </div>

              <dl class="dl-horizontal">
                <dt>Type</dt>
                <dd>{{['','Source','Target','Both'][node.type]}}</dd>
                <dt>Links</dt>
                <dd>{{node.weight}}</dd>
                <dt>Sessions</dt>
                <dd>{{node.sessions}}</dd>
                <dt>Bytes</dt>
                <dd>{{node.by}}</dd>
                <dt>Databytes</dt>
                <dd>{{node.db}}</dd>
                <dt>Packets</dt>
                <dd>{{node.pa}}</dd>
                <dt>Expressions</dt>
                <dd>
                  <a class="cursor-pointer no-decoration"
                    href="#"
                    @click.stop.prevent="addExpression('&&')">
                    AND
                  </a>&nbsp;
                  <a class="cursor-pointer no-decoration"
                    href="#"
                    @click.stop.prevent="addExpression('||')">
                    OR
                  </a>
                </dd>
              </dl>

              <a class="cursor-pointer no-decoration"
                href="#"
                @click.stop.prevent="hideNode()">
                <span class="fa fa-eye-slash">
                </span>&nbsp;
                Hide Node
              </a>
            </div>
          `,
          parent: this,
          data: {
            node: node
          },
          methods: {
            hideNode: function () {
              let self = this;
              self.$parent.closePopups();
              self.$parent.svg.select('#id' + self.node.id.replace(/[:.]/g, '_')).remove();
              self.$parent.svg.selectAll('.link')
                .filter(function (d, i) {
                  return d.source.id === self.node.id || d.target.id === self.node.id;
                })
                .remove();
            },
            addExpression: function (op) {
              let fullExpression = `${this.node.exp} == ${this.node.id}`;
              this.$store.commit('addToExpression', { expression: fullExpression, op: op });
            },
            closePopup: function () {
              this.$parent.closePopups();
            }
          }
        }).$mount(this.$refs.nodePopup);
      }

      nodePopupVue.node = node;

      that.positionPopup('.node-popup', node.px, node.py);
    },
    showLinkPopup: function (that, link, mouse) {
      link.dstExp = that.dbField2Exp(that.query.dstField);
      link.srcExp = that.dbField2Exp(that.query.srcField);

      this.closePopups();
      if (!linkPopupVue) {
        linkPopupVue = new Vue({
          template: `
            <div class="link-popup connections-popup">
              <div class="margined-bottom">
                <strong>Link</strong>
                <a class="pull-right cursor-pointer no-decoration"
                   @click="closePopup()">
                  <span class="fa fa-close"></span>
                </a>
              </div>
              <div>{{link.source.id}}</div>
              <div class="margined-bottom">{{link.target.id}}</div>

              <dl class="dl-horizontal">
                <dt>Sessions</dt>
                <dd>{{link.value}}</dd>
                <dt>Bytes</dt>
                <dd>{{link.by}}</dd>
                <dt>Databytes</dt>
                <dd>{{link.db}}</dd>
                <dt>Packets</dt>
                <dd>{{link.pa}}</dd>
                <dt>Expressions</dt>
                <dd>
                  <a class="cursor-pointer no-decoration"
                    @click="addExpression('&&')">AND</a>&nbsp;
                  <a class="cursor-pointer no-decoration"
                    @click="addExpression('||')">OR</a>
                </dd>
              </dl>

              <a class="cursor-pointer no-decoration"
                 @click="hideLink()">
                <span class="fa fa-eye-slash"></span>&nbsp;
                Hide Link
              </a>

            </div>
          `,
          parent: this,
          data: {
            link: link
          },
          methods: {
            hideLink: function () {
              let self = this;
              this.$parent.closePopups();
              self.$parent.svg.selectAll('.link')
                .filter(function (d, i) {
                  return d.source.id === self.link.source.id && d.target.id === self.link.target.id;
                })
                .remove();
            },
            addExpression: function (op) {
              let fullExpression = `(${this.link.srcExp} == ${this.link.source.id} && ${this.link.dstExp} == ${this.link.target.id})`;
              this.$store.commit('addToExpression', { expression: fullExpression, op: op });
            },
            closePopup: function () {
              this.$parent.closePopups();
            }
          }
        }).$mount(this.$refs.linkPopup);
      }

      linkPopupVue.link = link;

      that.positionPopup('.link-popup', mouse[0], mouse[1]);
    },
    /**
     * Positions and displays a popup on the connections graph within the view
     * @param {num} px      The x coordinate of the mouse/node
     * @param {num} py      The y coordinate of the mouse/node
     * @param {obj} content The content to display in the popup
     */
    positionPopup: function (name, px, py) {
      let x = -180; // 180 = width of popup + 10 padding

      // if the position node is too far to the left to accommodate the popup,
      // render it on the right of the node instead of the left
      if (this.trans[0] + (px * this.scale) < 180) { x = 10; }

      $(name).css({
        left: this.trans[0] + (px * this.scale) + x,
        top: this.trans[1] + (py * this.scale) - 20
      }).show();
    },
    startD3: function () {
      let self = this;

      self.trans = [0, 0];
      self.scale = 1;
      self.width = $(window).width() - 10;
      self.height = $(window).height() - 158;
      self.popupTimer = null;
      networkElem = $('#network');

      function redraw () {
        // store the last event data
        self.trans = d3.event.translate;
        self.scale = d3.event.scale;

        // transform the vis
        self.svg.attr('transform', 'translate(' + self.trans + ')' + ' scale(' + self.scale + ')');
      }

      self.zoom = d3.behavior.zoom()
        .translate([0, 0])
        .scale(1)
        .scaleExtent([0.25, 6])
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
        .on('mousedown', () => {
          this.closePopups();
        });

      force = d3.layout.force()
        .gravity(0.05)
        .distance(self.query.nodeDist)
        .charge(-300)
        .size([self.width, self.height]);

      d3.select(window).on('resize', this.resize);
    },
    /* resizes the graph by setting new width and height */
    resize: function () {
      let width = $(window).width();
      let height = $(window).height() - 166;

      svgMain.attr('width', width).attr('height', height);
      force.size([width, height]).resume();
    },
    endD3: function () {
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

      $('.node-popup').remove();
      $('.link-popup').remove();

      svgMain.remove();
      svgMain = null;

      this.svg.remove();
      this.svg = null;
    }
  }
};
</script>

<style>
.connections-page .connections-popup {
  position: absolute;
  display: none;
  font-size: smaller;
  padding: 4px 8px;
  width: 200px;
  z-index: 9999;
  border: solid 1px var(--color-gray);
  background: var(--color-primary-lightest);
  white-space: nowrap;
  overflow: hidden;
  text-overflow: ellipsis;

  -webkit-border-radius: var(--px-sm);
     -moz-border-radius: var(--px-sm);
          border-radius: var(--px-sm);

  -webkit-box-shadow: var(--px-md) var(--px-md) var(--px-xlg) var(--px-none) var(--color-gray);
     -moz-box-shadow: var(--px-md) var(--px-md) var(--px-xlg) var(--px-none) var(--color-gray);
          box-shadow: var(--px-md) var(--px-md) var(--px-xlg) var(--px-none) var(--color-gray);
}
.connections-page .connections-popup .dl-horizontal {
  margin-bottom: var(--px-md) !important;
}
.connections-page .connections-popup .dl-horizontal dt {
  width: 80px !important;
  text-align: left;
}
.connections-page .connections-popup .dl-horizontal dd {
  margin-left: 85px !important;
}
</style>

<style scoped>
.connections-page select {
  -webkit-appearance: none;
}

.connections-page {
  margin-top: 36px;
}
.connections-page form.connections-form {
  position: relative;
  top: 79px;
  left: 0;
  right: 0;
  background-color: var(--color-quaternary-lightest);

  -webkit-box-shadow: var(--px-md) var(--px-md) var(--px-xlg) var(--px-none) var(--color-gray);
     -moz-box-shadow: var(--px-md) var(--px-md) var(--px-xlg) var(--px-none) var(--color-gray);
          box-shadow: var(--px-md) var(--px-md) var(--px-xlg) var(--px-none) var(--color-gray);
}
.connections-page form.connections-form .form-inline {
  margin-top: -3px;
}

.connections-page form.connections-form .input-group-prepend.legend > .input-group-text {
  font-weight: 700;
  color: white !important;
}

/* apply theme colors */
.connections-page rect {
  stroke: var(--color-background, #FFF);
  fill: var(--color-background, #FFF);
}
.connections-page svg {
  fill: var(--color-foreground, #333);
}

.connections-page .connections-content {
  margin-top: 80px;
}
</style>
