<template>

  <div class="sessions-page">

    <!-- search navbar -->
    <moloch-search
      :fields="headers"
      :open-sessions="stickySessions"
      :num-visible-sessions="query.length"
      :num-matching-sessions="sessions.recordsFiltered"
      :start="query.start"
      :timezone="user.settings.timezone"
      @changeSearch="cancelAndLoad(true)">
    </moloch-search> <!-- /search navbar -->

    <!-- paging navbar -->
    <form class="sessions-paging">
      <div class="form-inline">
        <moloch-paging
          class="mt-1 ml-1"
          :records-total="sessions.recordsTotal"
          :records-filtered="sessions.recordsFiltered"
          @changePaging="changePaging">
        </moloch-paging>
      </div>
    </form> <!-- /paging navbar -->

    <div class="sessions-content ml-2 mr-2">

      <!-- session visualizations -->
      <moloch-visualizations
        v-if="mapData && graphData"
        :graph-data="graphData"
        :map-data="mapData"
        :primary="true"
        :timezone="user.settings.timezone"
        @fetchMapData="cancelAndLoad(true)">
      </moloch-visualizations> <!-- /session visualizations -->

      <!-- sticky (opened) sessions -->
      <transition name="leave">
        <moloch-sticky-sessions
          class="sticky-sessions"
          v-if="stickySessions.length"
          :ms="user.settings.ms"
          :sessions="stickySessions"
          :timezone="user.settings.timezone"
          @closeSession="closeSession"
          @closeAllSessions="closeAllSessions">
        </moloch-sticky-sessions>
      </transition> <!-- /sticky (opened) sessions -->

      <!-- sessions results -->
      <table v-if="headers && headers.length"
        class="table-striped sessions-table"
        :style="tableStyle"
        id="sessionsTable">
        <thead>
          <tr ref="draggableColumns">
            <!-- table options -->
            <th class="ignore-element"
              style="white-space: nowrap; width: 85px;">
              <!-- column visibility button -->
              <b-dropdown
                size="sm"
                no-flip
                no-caret
                class="col-vis-menu"
                variant="theme-primary"
                @show="colVisMenuOpen = true"
                @hide="colVisMenuOpen = false">
                <template slot="button-content">
                  <span class="fa fa-th"
                    v-b-tooltip.hover
                    title="Toggle visible columns">
                  </span>
                </template>
                <b-dropdown-header>
                  <input type="text"
                    v-model="colQuery"
                    class="form-control form-control-sm dropdown-typeahead"
                    placeholder="Search for columns..."
                  />
                </b-dropdown-header>
                <b-dropdown-divider>
                </b-dropdown-divider>
                <template v-if="colVisMenuOpen">
                  <template v-for="(group, key) in filteredFields">
                    <b-dropdown-header
                      :key="key"
                      v-if="group.length"
                      class="group-header">
                      {{ key }}
                    </b-dropdown-header>
                    <template v-for="(field, k) in group">
                      <b-dropdown-item
                        :id="key + k + 'item'"
                        :key="key + k + 'item'"
                        :class="{'active':isColVisible(field.dbField) >= 0}"
                        @click.stop.prevent="toggleColVis(field.dbField)">
                        {{ field.friendlyName }}
                        <small>({{ field.exp }})</small>
                      </b-dropdown-item>
                      <b-tooltip v-if="field.help"
                        :key="key + k + 'tooltip'"
                        :target="key + k + 'item'"
                        placement="right"
                        boundary="window">
                        {{ field.help }}
                      </b-tooltip>
                    </template>
                  </template>
                </template>
              </b-dropdown> <!-- /column visibility button -->
              <!-- column save button -->
              <b-dropdown
                size="sm"
                no-flip
                no-caret
                class="col-config-menu"
                variant="theme-secondary">
                <template slot="button-content">
                  <span class="fa fa-columns"
                    v-b-tooltip.hover
                    title="Save or load custom column configuration">
                  </span>
                </template>
                <b-dropdown-header>
                  <div class="input-group input-group-sm">
                    <input type="text"
                      maxlength="30"
                      class="form-control"
                      v-model="newColConfigName"
                      placeholder="Enter new column configuration name"
                      @keydown.enter="saveColumnConfiguration"
                    />
                    <div class="input-group-append">
                      <button type="button"
                        class="btn btn-theme-secondary"
                        :disabled="!newColConfigName"
                        @click="saveColumnConfiguration">
                        <span class="fa fa-save">
                        </span>
                      </button>
                    </div>
                  </div>
                </b-dropdown-header>
                <b-dropdown-divider>
                </b-dropdown-divider>
                <b-dropdown-item
                  v-if="colConfigError"
                  class="text-danger">
                  {{ colConfigError }}
                </b-dropdown-item>
                <b-dropdown-item
                  v-if="colConfigSuccess"
                  class="text-success">
                  {{ colConfigSuccess }}
                </b-dropdown-item>
                <template>
                  <b-dropdown-item
                    id="coldefault"
                    @click.stop.prevent="loadColumnConfiguration(-1)">
                    Moloch Default
                  </b-dropdown-item>
                  <b-tooltip target="coldefault"
                    placement="right"
                    boundary="window">
                    Reset table to default columns
                  </b-tooltip>
                </template>
                <b-dropdown-item
                  v-for="(config, key) in colConfigs"
                  :key="key"
                  @click.self.stop.prevent="loadColumnConfiguration(key)">
                  <button class="btn btn-xs btn-danger pull-right ml-1"
                    type="button"
                    @click.stop.prevent="deleteColumnConfiguration(config.name, key)">
                    <span class="fa fa-trash-o">
                    </span>
                  </button>
                  <button class="btn btn-xs btn-warning pull-right"
                    type="button"
                    v-b-tooltip.hover
                    title="Update this column configuration with the currently visible columns"
                    @click.stop.prevent="updateColumnConfiguration(config.name, key)">
                    <span class="fa fa-save">
                    </span>
                  </button>
                  {{ config.name }}
                </b-dropdown-item>
              </b-dropdown> <!-- /column save button -->
            </th> <!-- /table options -->
            <!-- table headers -->
            <th v-for="header of headers"
              :key="header.dbField"
              class="moloch-col-header"
              :style="{'width': header.width + 'px'}"
              :class="{'active':isSorted(header.sortBy || header.dbField) >= 0, 'info-col-header': header.dbField === 'info'}">
              <!-- non-sortable column -->
              <span v-if="header.dbField === 'info'"
                class="cursor-pointer">
                {{ header.friendlyName }}
                <!-- info field visibility button -->
                <b-dropdown
                  size="sm"
                  no-flip
                  no-caret
                  right
                  class="col-vis-menu info-vis-menu pull-right"
                  variant="theme-primary"
                  @show="infoFieldVisMenuOpen = true"
                  @hide="infoFieldVisMenuOpen = false">
                  <template slot="button-content">
                    <span class="fa fa-th-list"
                      v-b-tooltip.hover
                      title="Toggle visible fields">
                    </span>
                  </template>
                  <b-dropdown-header>
                    <input type="text"
                      v-model="colQuery"
                      class="form-control form-control-sm dropdown-typeahead"
                      placeholder="Search for fields..."
                    />
                  </b-dropdown-header>
                  <b-dropdown-divider>
                  </b-dropdown-divider>
                  <template>
                    <b-dropdown-item
                      id="infodefault"
                      @click.stop.prevent="resetInfoVisibility">
                      Moloch Default
                    </b-dropdown-item>
                    <b-tooltip target="infodefault"
                      placement="left"
                      boundary="window">
                      Reset info column to default fields
                    </b-tooltip>
                  </template>
                  <b-dropdown-divider>
                  </b-dropdown-divider>
                  <template v-if="infoFieldVisMenuOpen">
                    <template v-for="(group, key) in filteredFields">
                      <b-dropdown-header
                        :key="key"
                        v-if="group.length"
                        class="group-header">
                        {{ key }}
                      </b-dropdown-header>
                      <template v-for="(field, k) in group">
                        <b-dropdown-item
                          :id="key + k + 'infoitem'"
                          :key="key + k + 'infoitem'"
                          :class="{'active':isInfoVisible(field.dbField) >= 0}"
                          @click.stop.prevent="toggleInfoVis(field.dbField)">
                          {{ field.friendlyName }}
                          <small>({{ field.exp }})</small>
                        </b-dropdown-item>
                        <b-tooltip v-if="field.help"
                          :key="key + k + 'infotooltip'"
                          :target="key + k + 'infoitem'"
                          placement="left"
                          boundary="window">
                          {{ field.help }}
                        </b-tooltip>
                      </template>
                    </template>
                  </template>
                </b-dropdown> <!-- /info field visibility button -->
              </span> <!-- /non-sortable column -->
              <!-- column dropdown menu -->
              <b-dropdown
                right
                no-flip
                size="sm"
                class="pull-right">
                <b-dropdown-item
                  @click="toggleColVis(header.dbField, header.sortBy)">
                  Hide Column
                </b-dropdown-item>
                <!-- single field column -->
                <template v-if="!header.children && header.type !== 'seconds'">
                  <b-dropdown-divider>
                  </b-dropdown-divider>
                  <b-dropdown-item
                    @click="exportUnique(header.rawField || header.exp, 0)">
                    Export Unique {{ header.friendlyName }}
                  </b-dropdown-item>
                  <b-dropdown-item
                    @click="exportUnique(header.rawField || header.exp, 1)">
                    Export Unique {{ header.friendlyName }} with counts
                  </b-dropdown-item>
                  <template v-if="header.portField">
                    <b-dropdown-item
                      @click="exportUnique(header.rawField || header.exp + ':' + header.portField, 0)">
                      Export Unique {{ header.friendlyName }}:Ports
                    </b-dropdown-item>
                    <b-dropdown-item
                      @click="exportUnique(header.rawField || header.exp + ':' + header.portField, 1)">
                      Export Unique {{ header.friendlyName }}:Ports with counts
                    </b-dropdown-item>
                  </template>
                  <b-dropdown-item
                    @click="openSpiGraph(header.dbField)">
                    Open {{ header.friendlyName }} in SPI Graph
                  </b-dropdown-item>
                  <b-dropdown-item
                    @click="fieldExists(header.exp, '==')">
                    Add {{ header.friendlyName }} EXISTS! to query
                  </b-dropdown-item>
                </template> <!-- /single field column -->
                <!-- multiple field column -->
                <template v-else-if="header.children && header.type !== 'seconds'">
                  <span v-for="child in header.children"
                    :key="child.dbField">
                    <b-dropdown-divider>
                    </b-dropdown-divider>
                    <b-dropdown-item
                      @click="exportUnique(child.rawField || child.exp, 0)">
                      Export Unique {{ child.friendlyName }}
                    </b-dropdown-item>
                    <b-dropdown-item
                      @click="exportUnique(child.rawField || child.exp, 1)">
                      Export Unique {{ child.friendlyName }} with counts
                    </b-dropdown-item>
                    <template v-if="child.portField">
                      <b-dropdown-item
                        @click="exportUnique(child.rawField || child.exp + ':' + child.portField, 0)">
                        Export Unique {{ child.friendlyName }}:Ports
                      </b-dropdown-item>
                      <b-dropdown-item
                        @click="exportUnique(child.rawField || child.exp + ':' + child.portField, 1)">
                        Export Unique {{ child.friendlyName }}:Ports with counts
                      </b-dropdown-item>
                    </template>
                    <b-dropdown-item
                      @click="openSpiGraph(child.dbField)">
                      Open {{ child.friendlyName }} in SPI Graph
                    </b-dropdown-item>
                    <b-dropdown-item
                      @click="fieldExists(child.exp, '==')">
                      Add {{ child.friendlyName }} EXISTS! to query
                    </b-dropdown-item>
                  </span>
                </template> <!-- /multiple field column -->
              </b-dropdown> <!-- /column dropdown menu -->
              <!-- sortable column -->
              <span v-if="(header.exp || header.sortBy) && !header.unsortable"
                @mousedown="mouseDown"
                @mouseup="mouseUp"
                @click="sortBy($event, header.sortBy || header.dbField)"
                class="cursor-pointer">
                <div class="header-sort">
                  <span v-if="isSorted(header.sortBy || header.dbField) < 0"
                    class="fa fa-sort text-muted-more">
                  </span>
                  <span v-if="isSorted(header.sortBy || header.dbField) >= 0 && getSortOrder(header.sortBy || header.dbField) === 'asc'"
                    class="fa fa-sort-asc">
                  </span>
                  <span v-if="isSorted(header.sortBy || header.dbField) >= 0 && getSortOrder(header.sortBy || header.dbField) === 'desc'"
                    class="fa fa-sort-desc">
                  </span>
                </div>
                <div class="header-text">
                  {{ header.friendlyName }}
                </div>
              </span> <!-- /sortable column -->
            </th> <!-- /table headers -->
            <button type="button"
              v-if="showFitButton && !loading"
              class="btn btn-xs btn-theme-quaternary fit-btn"
              @click="fitTable"
              v-b-tooltip.hover
              title="Fit the table to the current window size">
              <span class="fa fa-arrows-h">
              </span>
            </button>
          </tr>
        </thead>
        <tbody class="small">
          <!-- session + detail -->
          <template v-for="(session, index) of sessions.data">
            <tr :key="session.id"
              :id="'session'+session.id">
              <!-- toggle button and ip protocol -->
              <td style="width: 85px;">
                <toggle-btn class="mt-1"
                  :opened="session.expanded"
                  @toggle="toggleSessionDetail(session)">
                </toggle-btn>
                <moloch-session-field
                  :field="{dbField:'ipProtocol', exp:'ip.protocol', type:'lotermfield', group:'general', transform:'ipProtocolLookup'}"
                  :session="session"
                  :expr="'ip.protocol'"
                  :value="session.ipProtocol"
                  :pull-left="true"
                  :parse="true">
                </moloch-session-field>
                &nbsp;
              </td> <!-- /toggle button and ip protocol -->
              <!-- field values -->
              <td v-for="col in headers"
                :key="col.dbField"
                :style="{'width': col.width + 'px'}">
                <!-- field value is an array -->
                <span v-if="Array.isArray(session[col.dbField])">
                  <span v-for="value in session[col.dbField]"
                    :key="value + col.dbField">
                    <moloch-session-field
                      :field="col"
                      :session="session"
                      :expr="col.exp"
                      :value="value"
                      :parse="true"
                      :timezone="user.settings.timezone">
                    </moloch-session-field>
                  </span>
                </span> <!-- /field value is an array -->
                <!-- field value a single value -->
                <span v-else>
                  <moloch-session-field
                    :field="col"
                    :session="session"
                    :expr="col.exp"
                    :value="session[col.dbField]"
                    :parse="true"
                    :timezone="user.settings.timezone"
                    :info-fields="infoFields">
                  </moloch-session-field>
                </span> <!-- /field value a single value -->
              </td> <!-- /field values -->
            </tr>
            <!-- session detail -->
            <tr :key="session.id + '-detail'"
              v-if="session.expanded"
              class="session-detail-row">
              <td :colspan="headers.length + 1">
                <moloch-session-detail
                  :session="session"
                  :session-index="index"
                  @toggleColVis="toggleColVis"
                  @toggleInfoVis="toggleInfoVis">
                </moloch-session-detail>
              </td>
            </tr> <!-- /session detail -->
          </template> <!-- /session + detail -->
        </tbody>
      </table> <!-- /sessions results -->

      <!-- loading overlay -->
      <moloch-loading
        :can-cancel="true"
        v-if="loading && !error"
        @cancel="cancelAndLoad(false)">
      </moloch-loading> <!-- /loading overlay -->

      <!-- page error -->
      <moloch-error
        v-if="error"
        :message="error"
        class="mt-5 mb-5">
      </moloch-error> <!-- /page error -->

      <!-- no results -->
      <moloch-no-results
        v-if="!error && !loading && !sessions.data.length"
        class="mt-5 mb-5"
        :records-total="sessions.recordsTotal"
        :view="query.view">
      </moloch-no-results> <!-- /no results -->

    </div>

  </div>

</template>

<script>
// IMPORTANT: don't change the order of imports (it messes up the flot graph)
import Vue from 'vue';
// import services
import FieldService from '../search/FieldService';
import SessionsService from './SessionsService';
import UserService from '../users/UserService';
import ConfigService from '../utils/ConfigService';
import Utils from '../utils/utils';
// import components
import MolochSearch from '../search/Search';
import customCols from './customCols.json';
import MolochPaging from '../utils/Pagination';
import ToggleBtn from '../utils/ToggleBtn';
import MolochError from '../utils/Error';
import MolochLoading from '../utils/Loading';
import MolochNoResults from '../utils/NoResults';
import MolochSessionDetail from './SessionDetail';
import MolochVisualizations from '../visualizations/Visualizations';
import MolochStickySessions from './StickySessions';
// import external
import Sortable from 'sortablejs';
import '../../../../public/colResizable.js';

let defaultInfoFields = JSON.parse(JSON.stringify(customCols.info.children));

let componentInitialized = false;
let holdingClick = false;
let timeout;

let colResizeInitialized;
let colDragDropInitialized;
let draggableColumns;
let tableDestroyed;

// window/table resize variables
let resizeTimeout;
let windowResizeEvent;
const defaultColWidths = {
  'firstPacket': 100,
  'lastPacket': 100,
  'src': 140,
  'srcPort': 100,
  'dst': 140,
  'dstPort': 100,
  'totPackets': 100,
  'dbby': 120,
  'node': 100,
  'info': 250
};

// save a pending promise to be able to cancel it
let pendingPromise;

export default {
  name: 'Sessions',
  components: {
    MolochSearch,
    MolochPaging,
    ToggleBtn,
    MolochError,
    MolochLoading,
    MolochNoResults,
    MolochSessionDetail,
    MolochVisualizations,
    MolochStickySessions
  },
  data: function () {
    return {
      loading: true,
      error: '',
      sessions: {}, // page data
      stickySessions: [],
      colWidths: {},
      colConfigs: [],
      colConfigError: '',
      colConfigSuccess: '',
      headers: [],
      graphData: undefined,
      mapData: undefined,
      colQuery: '', // query for columns to toggle visibility
      newColConfigName: '', // name of new custom column config
      viewChanged: false,
      infoFields: customCols.info.children,
      colVisMenuOpen: false,
      infoFieldVisMenuOpen: false
    };
  },
  created: function () {
    this.getColumnWidths();
    this.getTableState(); // IMPORTANT: kicks off the initial search query!
    this.getCustomColumnConfigurations();

    // watch for window resizing and update the info column width
    // this is only registered when the user has not set widths for any
    // columns && the info column is visible
    windowResizeEvent = () => {
      if (resizeTimeout) { clearTimeout(resizeTimeout); }
      resizeTimeout = setTimeout(() => {
        this.mapHeadersToFields();
      }, 300);
    };

    window.addEventListener('resize', windowResizeEvent, { passive: true });
  },
  computed: {
    query: function () {
      return { // query defaults
        length: parseInt(this.$route.query.length || 50), // page length
        start: 0, // first item index
        facets: 1,
        date: this.$store.state.timeRange,
        startTime: this.$store.state.time.startTime,
        stopTime: this.$store.state.time.stopTime,
        bounding: this.$route.query.bounding || 'last',
        interval: this.$route.query.interval || 'auto',
        view: this.$route.query.view || undefined,
        expression: this.$store.state.expression || undefined
      };
    },
    sorts: {
      get: function () {
        return this.$store.state.sorts || 'firstPacket:desc';
      },
      set: function (newValue) {
        this.$store.commit('setSorts', newValue);
      }
    },
    user: function () {
      return this.$store.state.user;
    },
    filteredFields: function () {
      let filteredGroupedFields = {};

      for (let group in this.groupedFields) {
        filteredGroupedFields[group] = this.$options.filters.searchFields(
          this.colQuery,
          this.groupedFields[group]
        );
      }

      return filteredGroupedFields;
    },
    views: function () {
      return this.$store.state.views;
    },
    tableStyle: function () {
      let style = { width: this.tableWidth + 'px' };
      // pad the bottom of the table so that opening a field
      // dropdowns at the bottom doesn't make the table scrolly
      if (this.sessions && this.sessions.data &&
        this.sessions.data.length) {
        style['margin-bottom'] = '300px';
      }
      return style;
    }
  },
  watch: {
    'query.view': function (newView, oldView) {
      this.viewChanged = true;
    }
  },
  methods: {
    /* exposed page functions ---------------------------------------------- */
    /* SESSIONS DATA */
    /**
     * Cancels the pending session query (if it's still pending) and runs a new
     * query if requested
     * @param {bool} runNewQuery  Whether to run a new sessions query after
     *                            canceling the request
     * @param {bool} updateTable  Whether the table needs updating
     */
    cancelAndLoad: function (runNewQuery, updateTable) {
      if (pendingPromise) {
        ConfigService.cancelEsTask(pendingPromise.cancelId)
          .then((response) => {
            pendingPromise.source.cancel();
            pendingPromise = null;

            if (!runNewQuery) {
              this.loading = false;
              if (!this.sessions.data) {
                // show a page error if there is no data on the page
                this.error = 'You canceled the search';
              }
              return;
            }

            this.loadData(updateTable);
          });
      } else if (runNewQuery) {
        this.loadData(updateTable);
      }
    },

    /* SESSION DETAIL */
    /**
     * Toggles the display of the session detail for each session
     * @param {Object} session The session to expand, collapse details
     */
    toggleSessionDetail: function (session) {
      session.expanded = !session.expanded;

      if (session.expanded) {
        this.stickySessions.push(session);
      } else {
        let index = this.stickySessions.indexOf(session);
        if (index >= 0) { this.stickySessions.splice(index, 1); }
      }
    },
    /**
     * Closes the session detail for a session
     * Triggered by the sticky session component
     * @param {string} sessionId The id of the session to close
     */
    closeSession: function (sessionId) {
      for (let i in this.stickySessions) {
        let session = this.stickySessions[i];
        if (session.id === sessionId) {
          session.expanded = false;
          this.stickySessions.splice(i, 1);
          return;
        }
      }
    },
    /**
     * Closes all the open sessions
     * Triggered by the sticky session component
     */
    closeAllSessions: function () {
      for (let session of this.stickySessions) {
        session.expanded = false;
      }

      this.stickySessions = [];
    },

    /* TABLE SORTING */
    /**
     * Determines if the table is being sorted by specified column
     * @param {string} id The id of the column
     */
    isSorted: function (id) {
      for (let i = 0; i < this.tableState.order.length; ++i) {
        if (this.tableState.order[i][0] === id) { return i; }
      }

      return -1;
    },
    /**
     * Sorts the sessions by the clicked column
     * (if the user issues a click less than 300ms long)
     * @param {Object} event  The click event that triggered the sort
     * @param {string} id     The id of the column to sort by
     */
    sortBy: function (event, id) {
      // if the column click was a click and hold/drag, don't issue new query
      if (holdingClick) { return; }

      if (this.isSorted(id) >= 0) {
        // the table is already sorted by this element
        if (!event.shiftKey) {
          let item = this.toggleSortOrder(id);
          this.tableState.order = [item];
        } else {
          // if it's a shift click - toggle the order between 3 states:
          // 'asc' -> 'desc' -> removed from sorts
          if (this.getSortOrder(id) === 'desc' && this.tableState.order.length > 1) {
            for (let i = 0, len = this.tableState.order.length; i < len; ++i) {
              if (this.tableState.order[i][0] === id) {
                this.tableState.order.splice(i, 1);
                break;
              }
            }
          } else {
            this.toggleSortOrder(id);
          }
        }
      } else { // sort by a new column
        if (!event.shiftKey) {
          // if it's a regular click - remove other sorts and add this one
          this.tableState.order = [[ id, 'asc' ]];
        } else {
          // if it's a shift click - add it to the list
          this.tableState.order.push([ id, 'asc' ]);
        }
      }

      this.sorts = this.tableState.order;

      this.saveTableState();

      this.cancelAndLoad(true, true);
    },
    /**
     * Determines the sort order of a column
     * @param {string} id     The unique id of the column
     * @return {string} order The sort order of the column
     */
    getSortOrder: function (id) {
      for (let i = 0, len = this.tableState.order.length; i < len; ++i) {
        if (this.tableState.order[i][0] === id) {
          return this.tableState.order[i][1];
        }
      }
    },
    /**
     * Toggles the sort order of a column, given its id
     * ('asc' -> 'desc' & 'desc' -> 'asc')
     * @param {string} id   The id of the column to toggle
     * @return {Array} item The sort item with updated order
     */
    toggleSortOrder: function (id) {
      for (let i = 0, len = this.tableState.order.length; i < len; ++i) {
        let item = this.tableState.order[i];
        if (item[0] === id) {
          if (item[1] === 'asc') {
            item[1] = 'desc';
          } else {
            item[1] = 'asc';
          }
          return item;
        }
      }
    },
    /**
     * Updates the sort field and order if the current sort column has
     * been removed
     * @param {string} id The id of the sort field
     * @returns {boolean} Whether the table requires a data reload
     */
    updateSort: function (id) {
      let updated = false;

      // update the sort field and order if the table was being sorted by that field
      let sortIndex = this.isSorted(id);
      if (sortIndex > -1) {
        updated = true; // requires a data reload because the sort is different
        // if we are sorting by this column, remove it
        if (this.tableState.order.length === 1) {
          // this column is the only column we are sorting by
          // so reset it to the first sortable field in the visible headers
          let newSort;
          for (let i = 0, len = this.headers.length; i < len; ++i) {
            let header = this.headers[i];
            // find the first sortable column
            if ((!header.children || (header.children && header.sortBy)) &&
               (header.dbField !== id && header.sortBy !== id)) {
              newSort = header.sortBy || header.dbField;
              break;
            }
          }

          // if there are no columns to sort by, sort by start time
          if (!newSort) { newSort = 'firstPacket'; }

          this.tableState.order = [[newSort, 'asc']];
        } else {
          // this column is one of many we are sorting by, so just remove it
          this.tableState.order.splice(sortIndex, 1);
        }

        // update the sorts
        this.sorts = this.tableState.order;
      }

      return updated;
    },
    /**
     * Sets holdingClick to true if the user holds the click for
     * 300ms or longer. If the user clicks and holds/drags, the
     * sortBy function returns immediately and does not issue query
     */
    mouseDown: function () {
      holdingClick = false;
      timeout = setTimeout(() => {
        holdingClick = true;
      }, 300);
    },
    /* Sets holdingClick to false 500ms after mouse up */
    mouseUp: function () {
      clearTimeout(timeout);
      setTimeout(() => {
        holdingClick = false;
      }, 500);
    },

    /* TABLE COLUMNS */
    /**
     * Determines a column's visibility given its id
     * @param {string} id       The id of the column
     * @return {number} number  The index of the visible header
     */
    isColVisible: function (id) {
      return this.isVisible(id, this.tableState.visibleHeaders);
    },
    /**
     * Toggles the visibility of a column given its id
     * @param {string} id   The id of the column to show/hide (toggle)
     * @param {string} sort Option sort id for columns that have sortBy
     */
    toggleColVis: function (id, sort) {
      let reloadData = false;

      let index = this.isColVisible(id);

      if (index >= 0) { // it's visible
        // remove it from the visible headers list
        this.tableState.visibleHeaders.splice(index, 1);
        reloadData = this.updateSort(sort || id);
      } else { // it's hidden
        reloadData = true; // requires a data reload
        // add it to the visible headers list
        this.tableState.visibleHeaders.push(id);
      }

      this.mapHeadersToFields();

      this.saveTableState();

      if (reloadData) { // need data from the server
        this.cancelAndLoad(true, true);
      } else { // have all the data, just need to reload the table
        this.reloadTable();
      }
    },
    /* Saves a custom column configuration */
    saveColumnConfiguration: function () {
      if (!this.newColConfigName) {
        this.colConfigError = 'You must name your new column configuration';
        return;
      }

      let data = {
        name: this.newColConfigName,
        columns: this.tableState.visibleHeaders.slice(),
        order: this.tableState.order.slice()
      };

      UserService.createColumnConfig(data)
        .then((response) => {
          data.name = response.name; // update column config name

          this.colConfigs.push(data);

          this.newColConfigName = null;
          this.colConfigError = false;
        })
        .catch((error) => {
          this.colConfigError = error.text;
        });
    },
    /**
     * Loads a previously saved custom column configuration and
     * reloads table and table data
     * If no index is given, loads the default columns
     * @param {int} index The index in the array of the column config to load
     */
    loadColumnConfiguration: function (index) {
      $('#sessionsTable').colResizable({ disable: true });
      colResizeInitialized = false;

      this.loading = true;

      if (index === -1) { // default columns
        this.tableState.visibleHeaders = Utils.getDefaultTableState().visibleHeaders.slice();
        this.tableState.order = Utils.getDefaultTableState().order.slice();
        this.colWidths = {}; // clear out column widths to load defaults
        setTimeout(() => { this.saveColumnWidths(); });
        // reset field widths
        for (let headerId of this.tableState.visibleHeaders) {
          let field = this.getField(headerId);
          if (field) { field.width = defaultColWidths[headerId] || 100; }
        }
      } else {
        this.tableState.visibleHeaders = this.colConfigs[index].columns.slice();
        this.tableState.order = this.colConfigs[index].order.slice();
      }

      this.sorts = this.tableState.order;

      this.saveTableState();

      this.cancelAndLoad(true, true);
    },
    /**
     * Deletes a previously saved custom column configuration
     * @param {string} name The name of the column config to remove
     * @param {int} index   The index in the array of the column config to remove
     */
    deleteColumnConfiguration: function (name, index) {
      UserService.deleteColumnConfig(name)
        .then((response) => {
          this.colConfigs.splice(index, 1);
          this.colConfigError = false;
        })
        .catch((error) => {
          this.colConfigError = error.text;
        });
    },
    /**
     * Updates a previously saved custom column configuration
     * @param {string} name The name of the column config to udpate
     * @param {int} index   The index in the array of the column config to udpate
     */
    updateColumnConfiguration: function (name, index) {
      let data = {
        name: name,
        columns: this.tableState.visibleHeaders.slice(),
        order: this.tableState.order.slice()
      };

      UserService.updateColumnConfig(data)
        .then((response) => {
          this.colConfigs[index] = data;
          this.colConfigError = false;
          this.colConfigSuccess = response.text;
          setTimeout(() => { this.colConfigSuccess = ''; }, 5000);
        })
        .catch((error) => {
          this.colConfigError = error.text;
        });
    },
    /**
     * Determines a field's visibility in the array provided
     * @param {string} id       The id of the column
     * @param {array} array     The array to search for the field
     * @return {number} number  The index of the visible header
     */
    isVisible: function (id, array) {
      let index = 0;

      for (let field of array) {
        if (typeof field !== 'object') {
          field = this.getField(field);
        }

        if (!field) { return -1; }

        if (field.dbField === id || field.exp === id) {
          return index;
        }

        if (field.aliases) { // check aliases too
          for (let alias of field.aliases) {
            if (id === alias) {
              return index;
            }
          }
        }

        index++;
      }

      return -1;
    },
    /**
     * Determines a field's visibility in the info column given its id
     * @param {string} id       The id of the column
     * @return {number} number  The index of the visible header
     */
    isInfoVisible: function (id) {
      return this.isVisible(id, this.infoFields);
    },
    /**
     * Toggles the visibility of a field in the info column given its id
     * @param {string} id The id of the column to show/hide (toggle)
     */
    toggleInfoVis: function (id) {
      let reloadData = false;

      let index = this.isInfoVisible(id);

      if (index >= 0) { // it's visible
        // remove it from the info fields list
        this.infoFields.splice(index, 1);
      } else { // it's hidden
        reloadData = true; // requires a data reload
        // add it to the info fields list
        let field = this.getField(id);
        if (field) { this.infoFields.push(field); }
      }

      this.saveInfoFields();

      if (reloadData) { // need data from the server
        this.cancelAndLoad(true, true);
      } else { // have all the data, just need to reload the table
        this.reloadTable();
      }
    },
    /* Resets the visible fields in the info column to the default */
    resetInfoVisibility: function () {
      this.infoFields = defaultInfoFields;
      customCols.info.children = defaultInfoFields;
      this.user.settings.infoFields = undefined;

      // make sure children of fields are field objects
      this.setupFields();
      // unset the user setting for info fields
      this.saveInfoFields();
      // load the table data (assume missing fields)
      this.cancelAndLoad(true, true);
    },
    /* Saves the info fields on the user settings */
    saveInfoFields: function () {
      let infoFields = [];
      for (let field of this.infoFields) {
        infoFields.push(field.dbField);
      }
      this.user.settings.infoFields = infoFields;
      customCols.info.children = this.infoFields;
      UserService.saveSettings(this.user.settings);
    },
    /* Fits the table to the width of the current window size */
    fitTable: function () {
      // disable resizable columns so it can be initialized after columns are resized
      $('#sessionsTable').colResizable({ disable: true });
      colResizeInitialized = false;

      let windowWidth = window.innerWidth - 34; // account for right and left margins
      let leftoverWidth = windowWidth - this.sumOfColWidths;
      let percentChange = 1 + (leftoverWidth / this.sumOfColWidths);

      for (let i = 0, len = this.headers.length; i < len; ++i) {
        let header = this.headers[i];
        let newWidth = Math.floor(header.width * percentChange);
        header.width = newWidth;
        this.colWidths[header.dbField] = newWidth;
      }

      this.tableWidth = windowWidth;
      this.showFitButton = false;

      this.saveColumnWidths();

      setTimeout(() => { this.initializeColResizable(); });
    },
    /**
     * Opens the spi graph page in a new browser tab
     * @param {string} fieldID The field id (dbField) to display spi graph data for
     */
    openSpiGraph: function (fieldID) {
      SessionsService.openSpiGraph(fieldID, this.$route.query);
    },
    /**
     * Open a page to view unique values for different fields
     * @param {string} exp    The field to get unique values for
     * @param {number} counts 1 or 0 whether to include counts of the values
     */
    exportUnique: function (exp, counts) {
      SessionsService.exportUniqueValues(exp, counts, this.$route.query);
    },
    /**
     * Adds field == EXISTS! to the search expression
     * @param {string} field  The field name
     * @param {string} op     The relational operator
     */
    fieldExists: function (field, op) {
      const fullExpression = this.$options.filters.buildExpression(field, 'EXISTS!', op);
      this.$store.commit('addToExpression', { expression: fullExpression });
    },

    /* helper functions ---------------------------------------------------- */
    reloadTable: function () {
      // disable resizable columns so it can be initialized after table reloads
      $('#sessionsTable').colResizable({ disable: true });
      colResizeInitialized = false;

      setTimeout(() => {
        this.initializeColResizable();
      });
    },
    /* Gets the column widths of the table if they exist */
    getColumnWidths: function () {
      UserService.getState('sessionsColWidths')
        .then((response) => {
          this.colWidths = response.data || {};
        });
    },
    /* Gets the state of the table (sort order and column order/visibility) */
    getTableState: function () {
      UserService.getState('sessionsNew')
        .then((response) => {
          this.tableState = response.data;
          this.$store.commit('setSessionsTableState', this.tableState);
          if (Object.keys(this.tableState).length === 0 ||
            !this.tableState.visibleHeaders || !this.tableState.order) {
            this.tableState = JSON.parse(JSON.stringify(Utils.getDefaultTableState()));
          } else if (this.tableState.visibleHeaders[0] === '') {
            this.tableState.visibleHeaders.shift();
          }

          // update the sort order for the session table query
          this.sorts = this.tableState.order;

          FieldService.get()
            .then((result) => {
              this.fields = result;
              this.setupUserSettings(); // IMPORTANT: kicks off the initial search query!
            }).catch((error) => {
              this.loading = false;
              this.error = error;
            });
        }).catch((error) => {
          this.loading = false;
          this.error = error;
        });
    },
    /* Gets the current user's custom column configurations */
    getCustomColumnConfigurations: function () {
      UserService.getColumnConfigs()
        .then((response) => {
          this.colConfigs = response;
        })
        .catch((error) => {
          this.colConfigError = error.text;
        });
    },
    setupUserSettings: function () {
      // if settings has custom sort field and the custom sort field
      // exists in the table headers, apply it
      if (this.user.settings && this.user.settings.sortColumn !== 'last' &&
         this.tableState.visibleHeaders.indexOf(this.user.settings.sortColumn) > -1) {
        this.sorts = [[this.user.settings.sortColumn, this.user.settings.sortDirection]];
        this.tableState.order = this.sorts;
      }

      // if user had infoFields set, update the info fields and custom info column
      if (this.user.settings && this.user.settings.infoFields) {
        this.infoFields = this.user.settings.infoFields;
        customCols.info.children = this.user.settings.infoFields;
      }

      // make sure children of fields are field objects
      this.setupFields();

      // IMPORTANT: kicks off the initial search query
      if (!this.user.settings.manualQuery ||
        !JSON.parse(this.user.settings.manualQuery) ||
        componentInitialized) {
        this.cancelAndLoad(true);
      } else {
        this.loading = false;
        this.error = 'Now, issue a query!';
      }

      componentInitialized = true;
    },
    /**
     * Makes a request to the Session Service to get the list of sessions
     * that match the query parameters
     * @param {bool} updateTable Whether the table needs updating
     */
    loadData: function (updateTable) {
      this.loading = true;
      this.error = '';

      // save expanded sessions
      let expandedSessions = [];
      for (let session of this.stickySessions) {
        expandedSessions.push(session.id);
      }

      this.sorts = this.tableState.order || JSON.parse(JSON.stringify(Utils.getDefaultTableState().order));

      if (this.viewChanged && this.views) {
        for (let view in this.views) {
          if (view === this.query.view && this.views[view].sessionsColConfig) {
            this.tableState = JSON.parse(JSON.stringify(this.views[view].sessionsColConfig));
            this.mapHeadersToFields();
            this.sorts = this.tableState.order;
            this.saveTableState();
          }
        }
        this.updateTable = true;
        this.viewChanged = false;
      } else {
        this.mapHeadersToFields();
      }

      this.query.fields = ['ipProtocol']; // minimum required field

      // set the fields to retrieve from the server for each session
      if (this.headers) {
        for (let field of this.headers) {
          if (field.children) {
            let children = field.children;
            // add user configurable child info fields
            if (field.exp === 'info' && this.infoFields) {
              children = JSON.parse(JSON.stringify(this.infoFields));
            }
            for (let child of children) {
              if (child) {
                this.query.fields.push(child.dbField);
              }
            }
          } else {
            this.query.fields.push(field.dbField);
          }
        }
      }

      // create unique cancel id to make canel req for corresponding es task
      const cancelId = Utils.createRandomString();
      this.query.cancelId = cancelId;

      const source = Vue.axios.CancelToken.source();
      const cancellablePromise = SessionsService.get(this.query, source.token);

      // set pending promise info so it can be cancelled
      pendingPromise = { cancellablePromise, source, cancelId };

      cancellablePromise.then((response) => {
        pendingPromise = null;
        this.stickySessions = []; // clear sticky sessions
        this.error = '';
        this.loading = false;
        this.sessions = response.data;
        this.mapData = response.data.map;
        this.graphData = response.data.graph;

        if (updateTable) { this.reloadTable(); }

        if (parseInt(this.$route.query.openAll) === 1) {
          this.openAll();
        } else { // open the previously opened sessions
          for (let sessionId of expandedSessions) {
            for (let session of this.sessions.data) {
              if (session.id === sessionId) {
                session.expanded = true;
                this.stickySessions.push(session);
              }
            }
          }
        }

        // initialize resizable columns now that there is data
        if (!colResizeInitialized) { this.initializeColResizable(); }

        // initialize sortable table
        if (!colDragDropInitialized) { this.initializeColDragDrop(); }
      }).catch((error) => {
        pendingPromise = null;
        this.sessions.data = undefined;
        this.error = error.text || error;
        this.loading = false;
      });
    },
    /**
     * Saves the table state
     * @param {bool} stopLoading Whether to stop the loading state when promise returns
     */
    saveTableState: function (stopLoading) {
      this.$store.commit('setSessionsTableState', this.tableState);
      UserService.saveState(this.tableState, 'sessionsNew')
        .then(() => {
          if (stopLoading) { this.loading = false; }
        })
        .catch((error) => { this.error = error; });
    },
    /**
     * Sets up the fields for the column visibility typeahead and column headers
     * by adding custom columns to the visible columns list and table
     */
    setupFields: function () {
      for (let key in customCols) {
        if (customCols.hasOwnProperty(key)) {
          this.fields[key] = customCols[key];
          let children = this.fields[key].children;
          // expand all the children
          for (let c in children) {
            // (replace fieldId with field object)
            if (children.hasOwnProperty(c)) {
              if (typeof children[c] !== 'object') {
                children[c] = this.getField(children[c]);
              }
            }
          }
        }
      }

      // group fields map by field group
      // and remove duplicate fields (e.g. 'host.dns' & 'dns.host')
      let existingFieldsLookup = {}; // lookup map of fields in fieldsArray
      this.groupedFields = {};
      for (let f in this.fields) {
        if (this.fields.hasOwnProperty(f)) {
          let field = this.fields[f];
          if (!existingFieldsLookup.hasOwnProperty(field.exp)) {
            existingFieldsLookup[field.exp] = field;
            if (!this.groupedFields[field.group]) {
              this.groupedFields[field.group] = [];
            }
            this.groupedFields[field.group].push(field);
          }
        }
      }
    },
    /**
     * Finds a field object given its id
     * @param {string} fieldId  The unique id of the field
     * @return {Object} field   The field object
     */
    getField: function (fieldId) {
      for (let key in this.fields) {
        if (this.fields.hasOwnProperty(key)) {
          let field = this.fields[key];
          if (field.dbField === fieldId || field.exp === fieldId) {
            return field;
          }
          if (field.aliases) {
            for (let alias of field.aliases) {
              if (alias === fieldId) {
                return field;
              }
            }
          }
        }
      }

      return undefined;
    },
    /* Maps visible column headers to their corresponding fields */
    mapHeadersToFields: function () {
      this.headers = [];
      this.sumOfColWidths = 85;

      if (!this.colWidths) { this.colWidths = {}; }

      for (let headerId of this.tableState.visibleHeaders) {
        let field = this.getField(headerId);

        if (field) {
          field.width = this.colWidths[headerId] || field.width || 100;
          if (field.dbField === 'info') { // info column is super special
            // reset info field width to default so it can always be recalculated
            // to take up all of the rest of the space that it can
            field.width = defaultColWidths['info'];
          } else { // don't account for info column's width because it changes
            this.sumOfColWidths += field.width;
          }
          this.headers.push(field);
        }
      }

      this.sumOfColWidths = Math.round(this.sumOfColWidths);

      this.calculateInfoColumnWidth(defaultColWidths['info']);
    },
    /* Opens up to 10 session details in the table */
    openAll: function () {
      // opening too many session details at once is bad!
      if (this.sessions.data.length > 10) {
        alert('You\'re trying to open too many session details at once! I\'ll only open the first 10 for you, sorry!');
      }

      let len = Math.min(this.sessions.data.length, 10);

      for (let i = 0; i < len; ++i) {
        this.toggleSessionDetail(this.sessions.data[i]);
      }

      // unset open all for future queries
      this.$router.replace({
        query: {
          ...this.$route.query,
          openAll: undefined
        }
      });
    },
    /* Initializes column drag and drop */
    initializeColDragDrop: function () {
      colDragDropInitialized = true;
      draggableColumns = Sortable.create(this.$refs.draggableColumns, {
        animation: 100,
        filter: '.ignore-element',
        preventOnFilter: false, // allow clicks within the ignored element
        onMove: (event) => { // col header is being dragged
          // don't allow a column to be dropped in the far left column
          return !event.related.classList.contains('ignore-element');
        },
        onEnd: (event) => { // dragged col header was dropped
          // nothing has changed, so don't do stuff
          if (event.oldIndex === event.newIndex) { return; }

          this.loading = true;

          // update the headers to the new order
          let oldIdx = event.oldIndex - 1;
          let newIdx = event.newIndex - 1;
          let element = this.tableState.visibleHeaders[oldIdx];
          this.tableState.visibleHeaders.splice(oldIdx, 1);
          this.tableState.visibleHeaders.splice(newIdx, 0, element);

          this.mapHeadersToFields();
          this.saveTableState();
          this.reloadTable();

          setTimeout(() => {
            this.loading = false;
          });
        }
      });
    },
    /* Initializes resizable columns */
    initializeColResizable: function () {
      if (tableDestroyed) {
        $('#sessionsTable').colResizable({ disable: true });
        colResizeInitialized = false;
        tableDestroyed = false;
      }

      setTimeout(() => {
        colResizeInitialized = true;
        $('#sessionsTable').colResizable({
          minWidth: 50,
          headerOnly: true,
          resizeMode: 'overflow',
          disabledColumns: [0],
          hoverCursor: 'col-resize',
          onResize: (event, column, colIdx) => {
            this.loading = true;

            let header = this.headers[colIdx - 1];
            if (header) {
              header.width = column.w;
              this.colWidths[header.dbField] = column.w;

              this.saveColumnWidths();
              this.mapHeadersToFields();
            }

            this.loading = false;
          }
        });
      });
    },
    /* Saves the column widths */
    saveColumnWidths: function () {
      UserService.saveState(this.colWidths, 'sessionsColWidths')
        .catch((error) => { this.error = error; });
    },
    /**
     * Calculates the info column's width based on the width of the window
     * If the info column is visible, it should take up whatever space is left
     * Also determines whether the fit to window size button should be displayed
     * @param infoColWidth
     */
    calculateInfoColumnWidth: function (infoColWidth) {
      this.showFitButton = false;
      if (!this.colWidths) { return; }
      let windowWidth = window.innerWidth - 34; // account for right and left margins
      if (this.tableState.visibleHeaders.indexOf('info') >= 0) {
        let fillWithInfoCol = windowWidth - this.sumOfColWidths;
        let newTableWidth = this.sumOfColWidths;
        for (let i = 0, len = this.headers.length; i < len; ++i) {
          if (this.headers[i].dbField === 'info') {
            let newInfoColWidth = Math.max(fillWithInfoCol, infoColWidth);
            this.headers[i].width = newInfoColWidth;
            newTableWidth += newInfoColWidth;
          }
        }
        this.tableWidth = newTableWidth;
      } else {
        this.tableWidth = this.sumOfColWidths;
        // display a button to fit the table to the width of the window
        // if the table is more than 10 pixels larger or smaller than the window
        if (Math.abs(this.tableWidth - windowWidth) > 10) {
          this.showFitButton = true;
        }
      }
    },
    /* event handlers ------------------------------------------------------ */
    /**
     * Fired when paging changes (from utils/Pagination)
     * Update start and length, then get data
     */
    changePaging: function (args) {
      this.query.start = args.start;
      this.query.length = args.length;
      this.cancelAndLoad(true);
    }
  },
  beforeDestroy: function () {
    tableDestroyed = true;
    holdingClick = false;
    componentInitialized = false;
    colResizeInitialized = false;
    colDragDropInitialized = false;

    if (pendingPromise) {
      pendingPromise.source.cancel();
      pendingPromise = null;
    }

    if (timeout) { clearTimeout(timeout); }

    $('#sessionsTable').colResizable({ disable: true });

    if (draggableColumns) { draggableColumns.destroy(); }

    window.removeEventListener('resize', windowResizeEvent);
  }
};
</script>

<style>
.col-config-menu .dropdown-menu {
  min-width: 250px;
  max-width: 350px;
}
.col-config-menu > button.btn {
  border-top-right-radius: 4px !important;
  border-bottom-right-radius: 4px !important;;
}
.col-vis-menu > button.btn {
  border-top-right-radius: 4px !important;
  border-bottom-right-radius: 4px !important;;
}
.col-vis-menu .dropdown-menu {
  max-height: 300px;
  overflow: auto;
}

.info-vis-menu .dropdown-menu {
  width: 360px;
}

/* small dropdown buttons in column headers */
.moloch-col-header .btn-group button.btn {
  padding: 0 6px;
}
.moloch-col-header .dropdown-menu {
  max-height: 250px;
  overflow: auto;
}

.moloch-col-header .btn-group:not(.info-vis-menu) {
  visibility: hidden;
  margin-left: -25px;
}
</style>

<style scoped>
/* sessions page, navbar, and content styles - */
.sessions-page {
  margin-top: 36px;
}

form.sessions-paging {
  z-index: 4;
  position: fixed;
  top: 110px;
  left: 0;
  right: 0;
  height: 40px;
  background-color: var(--color-quaternary-lightest);

  -webkit-box-shadow: 0 0 16px -2px black;
     -moz-box-shadow: 0 0 16px -2px black;
          box-shadow: 0 0 16px -2px black;
}

.sessions-content {
  padding-top: 115px;
  overflow-y: auto;
}

/* sessions table -------------------------- */
table.sessions-table {
  table-layout: fixed;
  border-top: var(--color-gray-light) solid 1px;
  margin-bottom: 20px;
  border-spacing: 0;
  border-collapse: collapse;
}

table.sessions-table thead tr th {
  vertical-align: top;
  border-bottom: 2px solid var(--color-gray);
  border-right: 1px dotted var(--color-gray);
}
table.sessions-table thead tr th:last-child:not(.info-col-header) {
  padding-right: 35px;
}
table.sessions-table thead tr th:first-child {
  border-right: none;
}

table.sessions-table thead tr th:first-child{
  padding: 0;
  vertical-align: middle;
}

table.sessions-table tbody tr:not(.session-detail-row):hover {
  background-color: var(--color-tertiary-lightest);
}

table.sessions-table tbody tr:not(.session-detail-row):hover td.active {
  background-color: var(--color-tertiary-light);
}

table.sessions-table tbody tr.session-detail-row {
  background-color: var(--color-quaternary-lightest) !important;
}

table.sessions-table tbody tr td {
  padding: 0 2px;
  line-height: 1.42857143;
  vertical-align: top;
}

/*!* table.sessions-table column reorder *!*/
.JColResizer > tbody > tr > td, .JColResizer > tbody > tr > th {
  overflow: visible !important; /* show overflow for clickable fields */
}

/* table column headers -------------------- */
.moloch-col-header {
  font-size: .9rem;
}

.moloch-col-header.active {
  color: var(--color-foreground-accent);
}

.moloch-col-header:hover .btn-group {
  visibility: visible;
}

.moloch-col-header .header-text {
  display: inline-block;
  width: calc(100% - 24px);
}

.moloch-col-header .header-sort {
  display: inline-block;
  width: 8px;
  vertical-align: top;
}

.info-col-header .btn-group:not(.info-vis-menu) {
  margin-right: 4px;
}
.info-vis-menu {
  margin-right: 10px;
}
.moloch-col-header:not(:last-child) .info-vis-menu {
  margin-right: 5px;
}

/* column visibility menu -------------------- */
.col-vis-menu .dropdown-header {
  padding: .25rem .5rem 0;
}
.col-vis-menu .dropdown-header.group-header {
  text-transform: uppercase;
  margin-top: 8px;
  padding: .2rem;
  font-size: 120%;
  font-weight: bold;
}

/* custom column configurations menu --------- */
.col-config-menu .dropdown-header {
  padding: .25rem .5rem 0;
}

/* table fit button -------------------------- */
button.fit-btn {
  position: absolute;
  top: 290px;
  left: 10px;
}

/* animate sticky sessions enter/leave */
.sticky-sessions {
  position: absolute;
  top: 0;
  right: 0;
  bottom: 0;
}
.leave-enter-active, .leave-leave-active {
  transition: all 0.5s ease;
}
.leave-enter, .leave-leave-to {
  z-index: 4;
}
.leave-leave-to {
  transform: translateY(1000px);
  opacity: 0;
}
</style>
