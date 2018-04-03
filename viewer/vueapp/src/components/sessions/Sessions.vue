<template>

  <div class="sessions-content">

    <moloch-search
      :open-sessions="stickySessions"
      :num-visible-sessions="query.length"
      :num-matching-sessions="sessions.recordsFiltered"
      :start="query.start"
      :timezone="settings.timezone"
      @changeSearch="changeSearch">
    </moloch-search>

    <form class="sessions-paging">
      <div class="form-inline">
        <moloch-paging
          class="mt-1 ml-1"
          :records-total="sessions.recordsTotal"
          :records-filtered="sessions.recordsFiltered"
          @changePaging="changePaging">
        </moloch-paging>
      </div>
    </form>

    <div class="container-fluid">

      <div class="sessions-vis">
        sessions visualizations go here!
      </div>

      <div class="sessions-table">
        <!-- TODO table width -->
        <table v-if="headers && headers.length"
          class="table-striped sessions-table"
          id="sessionsTable">
          <thead>
            <tr>
              <!-- table options -->
              <th>
                &nbsp;
                <!-- TODO col vis button -->
                <!-- TODO col save button -->
              </th> <!-- /table options -->
              <!-- TODO drag n drop columns -->
              <!-- TODO resize columns -->
              <!-- table headers -->
              <th v-for="header of headers"
                :key="header.dbField"
                class="moloch-col-header"
                :style="{'width':header.width+'px'}"
                :class="{'active':isSorted(header.sortBy || header.dbField) >= 0}">
                {{ header.friendlyName }}
                <!-- TODO column sort & dropdown -->
              </th> <!-- /table headers -->
            </tr>
          </thead>
          <tbody class="small">
            <!-- TODO session detail -->
            <tr v-for="session of sessions.data"
              :key="session.id">
              <td>
                <!-- TODO toggle button -->
                <toggle-btn @toggle="toggleSessionDetail(session)">
                </toggle-btn>
                <small>
                  <!-- TODO session field parser -->
                  {{ session.ipProtocol }}
                </small>
              </td>
              <td v-for="col in headers"
                :key="col.dbField">
                <!-- TODO determine if it's an array or not -->
                {{ session[col.dbField] }}
              </td>
            </tr>
          </tbody>
        </table>
      </div>

    </div>

  </div>

</template>

<script>
import UserService from '../UserService';
import MolochSearch from '../search/Search';
import FieldService from '../search/FieldService';
import SessionsService from './SessionsService';
import customCols from './customCols.json';
import MolochPaging from '../utils/Pagination';
import ToggleBtn from '../utils/ToggleBtn';

const defaultTableState = {
  order: [['firstPacket', 'asc']],
  visibleHeaders: ['firstPacket', 'lastPacket', 'src', 'srcPort', 'dst', 'dstPort', 'totPackets', 'dbby', 'node', 'info']
};

let defaultInfoColWidth = 250;
let componentInitialized = false;
let holdingClick = false;
let timeout;

export default {
  name: 'Sessions',
  components: { MolochSearch, MolochPaging, ToggleBtn },
  data: function () {
    return {
      user: null,
      loading: true,
      error: '',
      sessions: {}, // page data
      stickySessions: [],
      settings: {}, // user settings
      query: { // set query defaults:
        length: this.$route.query.length || 50, // page length
        start: 0, // first item index
        facets: 1,
        date: this.$route.query.date || 1,
        startTime: this.$route.query.startTime || undefined,
        stopTime: this.$route.query.stopTime || undefined,
        bounding: this.$route.query.bounding || 'last',
        interval: this.$route.query.interval || 'auto',
        view: this.$route.query.view || undefined
      },
      colWidths: {},
      colConfigError: '',
      headers: []
    };
  },
  created: function () {
    this.getColumnWidths();
    this.getTableState(); // IMPORTANT: kicks off the initial search query!
    this.getCustomColumnConfigurations();
    // TODO LISTEN for add to search, change time, and window resize
  },
  methods: {
    /* exposed page functions ---------------------------------------------- */
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

      this.query.sorts = this.tableState.order;

      this.saveTableState();

      this.loadData(true);
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

        // update the query
        this.query.sorts = this.tableState.order;
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
    /**
     * Determines a column's visibility given its id
     * @param {string} id       The id of the column
     * @return {number} number  The index of the visible header
     */
    isVisible: function (id) {
      return this.tableState.visibleHeaders.indexOf(id);
    },
    // TODO organize and add functions: onDropComplete, toggleVisibility, loadColumnConfiguration, saveColumnConfiguration, deleteColumnConfiguration, openSpiGraph, exportUnique
    /* helper functions ---------------------------------------------------- */
    /* Gets the column widths of the table if they exist */
    getColumnWidths: function () {
      SessionsService.getState('sessionsColWidths')
        .then((response) => {
          this.colWidths = response.data || {};
        });
    },
    /* Gets the state of the table (sort order and column order/visibility) */
    getTableState: function () {
      SessionsService.getState('sessionsNew')
        .then((response) => {
          this.tableState = response.data;
          if (Object.keys(this.tableState).length === 0 ||
            !this.tableState.visibleHeaders || !this.tableState.order) {
            this.tableState = defaultTableState;
          } else if (this.tableState.visibleHeaders[0] === '') {
            this.tableState.visibleHeaders.shift();
          }

          // update the sort order for the session table query
          this.query.sorts = this.tableState.order;

          FieldService.get()
            .then((result) => {
              this.fields = result;
              this.setupFields();
              this.getUserSettings(); // IMPORTANT: kicks off the initial search query!
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
    getUserSettings: function () {
      UserService.getCurrent()
        .then((response) => {
          this.settings = response;

          // if settings has custom sort field and the custom sort field
          // exists in the table headers, apply it
          if (this.settings && this.settings.sortColumn !== 'last' &&
             this.tableState.visibleHeaders.indexOf(this.settings.sortColumn) > -1) {
            this.query.sorts = [[this.settings.sortColumn, this.settings.sortDirection]];
            this.tableState.order = this.query.sorts;
          }

          // IMPORTANT: kicks off the initial search query
          if (!this.settings.manualQuery || componentInitialized) {
            this.loadData();
          } else {
            this.loading = false;
            this.error = 'Now, issue a query!';
          }

          componentInitialized = true;
        }, (error) => {
          this.settings = { timezone: 'local' };
          this.error = error;
        });
    },
    /**
     * Makes a request to the Session Service to get the list of sessions
     * that match the query parameters
     * @param {bool} updateTable Whether the table needs updating
     */
    loadData: function (updateTable) {
      console.log('load data', this.query); // TODO remove
      this.loading = true;
      this.error = '';

      this.stickySessions = []; // clear sticky sessions

      this.query.fields = ['ipProtocol']; // minimum required field

      this.mapHeadersToFields();

      // set the fields to retrieve from the server for each session
      if (this.headers) {
        for (let i = 0; i < this.headers.length; ++i) {
          let field = this.headers[i];
          if (field.children) {
            for (let j = 0; j < field.children.length; ++j) {
              let child = field.children[j];
              if (child) { this.query.fields.push(child.dbField); }
            }
          } else {
            this.query.fields.push(field.dbField);
          }
        }
      }

      SessionsService.get(this.query)
        .then((response) => {
          this.error = '';
          this.loading = false;
          this.sessions = response.data;
          this.mapData = response.data.map;
          this.graphData = response.data.graph;

          // TODO this is unnecessary? because there are no one time bindings?
          // if (updateTable) { this.reloadTable(); }

          if (parseInt(this.$route.query.openAll) === 1) {
            this.openAll();
          }

          // initialize resizable columns now that there is data
          // TODO
          // if (!colResizeInitialized) { this.initializeColResizable(); }
        })
        .catch((error) => {
          this.error = error;
          this.loading = false;
          // this.reloadTable(); // sets loading to false
        });
    },
    /**
     * Saves the table state
     * @param {bool} stopLoading Whether to stop the loading state when promise returns
     */
    saveTableState: function (stopLoading) {
      SessionsService.saveState(this.tableState, 'sessionsNew')
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

      // convert fields map to array (for ng-repeat with filter and group)
      // and remove duplicate fields (e.g. 'host.dns' & 'dns.host')
      let existingFieldsLookup = {}; // lookup map of fields in fieldsArray
      this.fieldsArray = [];
      for (let f in this.fields) {
        if (this.fields.hasOwnProperty(f)) {
          let field = this.fields[f];
          if (!existingFieldsLookup.hasOwnProperty(field.exp)) {
            existingFieldsLookup[field.exp] = field;
            this.fieldsArray.push(field);
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
          let item = this.fields[key];
          if (item.dbField === fieldId) {
            return item;
          }
        }
      }

      return undefined;
    },
    /* Maps visible column headers to their corresponding fields */
    mapHeadersToFields: function () {
      this.headers = [];
      this.sumOfColWidths = 80;

      if (!this.colWidths) { this.colWidths = {}; }

      for (let i = 0, len = this.tableState.visibleHeaders.length; i < len; ++i) {
        let headerId = this.tableState.visibleHeaders[i];
        let field = this.getField(headerId);

        if (field) {
          field.width = this.colWidths[headerId] || field.width || 100;
          if (field.dbField === 'info') { // info column is super special
            // reset info field width to default so it can always be recalculated
            // to take up all of the rest of the space that it can
            field.width = defaultInfoColWidth;
          } else { // don't account for info column's width because it changes
            this.sumOfColWidths += field.width;
          }
          this.headers.push(field);
        }
      }

      this.sumOfColWidths = Math.round(this.sumOfColWidths);

      // TODO
      // this.calculateInfoColumnWidth(defaultInfoColWidth);
      // this.$scope.$broadcast('$$rebind::refreshHeaders');
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
      this.$router.push({
        query: {
          ...this.$route.query,
          openAll: undefined
        }
      }).replace();
    },
    /* event handlers ------------------------------------------------------ */
    /**
     * fired when search parameters change (from search/Search)
     * update startTime, stopTime, date, bounding, interval, view, and expression
     * then load data to update the table with new results
     */
    changeSearch: function (args) {
      // either (startTime && stopTime) || date
      if (args.startTime && args.stopTime) {
        this.query.startTime = args.startTime;
        this.query.stopTime = args.stopTime;
        this.query.date = undefined;
      } else if (args.date) {
        this.query.date = args.date;
        this.query.stopTime = undefined;
        this.query.startTime = undefined;
      }

      if (args.bounding) { this.query.bounding = args.bounding; }
      if (args.interval) { this.query.interval = args.interval; }

      this.query.view = args.view;
      this.query.expression = args.expression;

      // reset to the first page, because we are issuing a new query
      // and there may only be 1 page of results
      this.query.start = 0;

      this.loadData(true);
    },
    /**
     * fired when paging changes (from utils/Pagination)
     * update start and length, then get data if one has changed
     */
    changePaging: function (args) {
      if (this.query.start !== args.start ||
        this.query.length !== args.length) {
        this.query.start = args.start;
        this.query.length = args.length;
        this.loadData(true);
      }
    }
  },
  beforeDestroy: function () {
    // TODO
  }
};
</script>

<style scoped>
.sessions-content {
  margin-top: 36px;
}

form.sessions-paging {
  position: fixed;
  top: 110px;
  left: 0;
  right: 0;
  height: 40px;
  background-color: var(--color-quaternary-lightest);
}

.container-fluid {
  padding-top: 115px;
}
</style>
