<template>

  <table v-if="computedColumns && computedColumns.length"
    :style="`width:${tableWidth}px`"
    class="table-striped"
    :class="tableClasses"
    ref="table"
    :id="id">
    <thead>
      <button type="button"
        v-if="showFitButton"
        class="btn btn-xs btn-theme-quaternary fit-btn"
        @click="fitTable"
        v-b-tooltip.hover
        title="Fit the table to the current window size">
        <span class="fa fa-arrows-h">
        </span>
      </button>
      <tr ref="draggableColumns">
        <th
          v-if="actionColumn"
          style="width:70px;"
          class="ignore-element text-left">
          <div class="d-flex align-items-center">
            <!-- column visibility button -->
            <b-dropdown
              size="sm"
              no-flip
              no-caret
              role="dropdown"
              class="col-vis-menu pull-left"
              variant="theme-primary">
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
              <b-dropdown-item
                @click="resetDefault">
                Reset default columns
              </b-dropdown-item>
              <b-dropdown-divider>
              </b-dropdown-divider>
              <b-dropdown-item
                v-for="column in filteredColumns"
                :key="column.id"
                v-b-tooltip.hover.top
                :title="column.help"
                :class="{'active':isVisible(column.id) >= 0}"
                @click.stop.prevent="toggleVisibility(column)">
                {{ column.name }}
              </b-dropdown-item>
            </b-dropdown> <!-- /column visibility button -->
            <!-- ESNode data node only toggle -->
            <div class="ml-3">
              <b-form-checkbox
                v-if="this.$route.query.statsTab && parseInt(this.$route.query.statsTab) === 2"
                v-b-tooltip.hover
                :title="`Only show data nodes`"
                @change="$emit('toggle-data-node-only')"
                name="only-data-nodes-checkbox"
              >
              </b-form-checkbox>
            </div><!-- ESNode data node only toggle -->
          </div>
        </th>
        <th v-for="column in computedColumns"
          :key="column.name"
          v-b-tooltip.hover
          :title="column.help"
          @click.self="sort(column)"
          :class="(column.classes ? `${column.classes} ` : '') + (column.sort ? 'cursor-pointer' : '')"
          :style="{'width': column.width > 0 ? `${column.width}px` : '100px'}"
          class="col-header">
          <div class="grip">&nbsp;</div>
          {{ column.name }}
          <span v-if="column.canClear"
            class="btn-zero">
            <b-tooltip :target="`zero-btn-${column.name}`">
              Set this column's values to 0.
              <strong v-if="zeroedAt && zeroedAt[column.id]">
                <br>
                Last cleared at
                {{ zeroedAt[column.id] | timezoneDateString(user.settings.timezone || 'local') }}
              </strong>
            </b-tooltip>
            <button :id="`zero-btn-${column.name}`"
              type="button"
              @click="zeroColValues(column)"
              class="btn btn-xs btn-secondary">
              <span class="fa fa-ban">
              </span>
            </button>
          </span>
          <span v-if="column.sort">
            <span v-show="tableSortField === column.sort && !tableDesc" class="fa fa-sort-asc"></span>
            <span v-show="tableSortField === column.sort && tableDesc" class="fa fa-sort-desc"></span>
            <span v-show="tableSortField !== column.sort" class="fa fa-sort"></span>
          </span>
        </th>
      </tr>
    </thead>
    <transition-group tag="tbody"
      :name="tableAnimation">
      <!-- avg/total top rows -->
      <template v-if="showAvgTot && data && data.length > 9">
        <tr class="border-top-bold bold average-row"
          key="averageRow">
          <td v-if="actionColumn">
            Avg
          </td>
          <td :class="column.classes"
            v-for="(column, index) in computedColumns"
            :key="column.id + index + 'avg'">
            {{ calculateFormatAvgValue(column) }}
          </td>
        </tr>
        <tr class="border-bottom-bold bold total-row"
          key="totalRow">
          <td v-if="actionColumn">
            Total
          </td>
          <td :class="column.classes"
            v-for="(column, index) in computedColumns"
            :key="column.id + index + 'total'">
            {{ calculateFormatTotValue(column) }}
          </td>
        </tr>
      </template> <!-- /avg/total top rows -->
      <!-- data rows -->
      <template v-for="(item, index) of data">
        <tr :key="item.id || index">
          <td v-if="actionColumn"
            class="text-left"
            style="overflow: visible !important;">
            <!-- toggle more info row button -->
            <toggle-btn v-if="infoRow"
              class="mr-1"
              :opened="item.opened"
              @toggle="toggleMoreInfo(item)">
            </toggle-btn> <!-- /toggle more info row button -->
            <!-- action buttons -->
            <slot name="actions"
              :item="item">
            </slot> <!-- /action buttons -->
          </td>
          <!-- cell value -->
          <td :class="column.classes"
            v-for="(column, colindex) in computedColumns"
            :key="column.id + colindex">
            {{ calculateFormatValue(column, item, index) }}
          </td> <!-- /cell value -->
        </tr>
        <!-- more info row -->
        <tr class="text-left"
          v-if="infoRow && item.opened"
          :key="item.id+'moreInfo'">
          <td :colspan="tableColspan">
            <div :id="'moreInfo-' + item.id"></div>
          </td>
        </tr> <!-- /more info row -->
      </template> <!-- /data rows -->
      <!-- no results -->
      <tr v-if="noResults && data && !data.length"
        key="noResults">
        <td :colspan="tableColspan"
          class="text-danger text-center">
          <span class="fa fa-warning">
          </span>&nbsp;
          No results match your search
        </td>
      </tr> <!-- /no results -->
    </transition-group>
    <!-- avg/total bottom rows -->
    <tfoot v-if="showAvgTot && data && data.length > 1">
      <tr class="border-top-bold bold average-row">
        <td v-if="actionColumn">
          Avg
        </td>
        <td :class="column.classes"
          v-for="(column, index) in computedColumns"
          :key="column.id + index + 'avgfoot'">
          {{ calculateFormatAvgValue(column) }}
        </td>
      </tr>
      <tr class="bold total-row">
        <td v-if="actionColumn">
          Total
        </td>
        <td :class="column.classes"
          v-for="(column, index) in computedColumns"
          :key="column.id + index + 'totalfoot'">
          {{ calculateFormatTotValue(column) }}
        </td>
      </tr>
    </tfoot> <!-- /avg/total bottom rows -->
  </table>

</template>

<script>
import Sortable from 'sortablejs';

import UserService from '../users/UserService';
import ToggleBtn from '../../../../../common/vueapp/ToggleBtn';

// column resize variables and functions
let selectedColElem; // store selected column to watch drag and calculate new column width
let colStartOffset; // store column offset width to calculate new column width
let colWidthBeforeResize; // sore column width before resize to calculate diff
let tableWidthBeforeResize; // store table width before column resize to add to col resize diff
let table; // store table element to update its width after column resize
let cols; // store cols to add grip event handlers and save new widths
let selectedGripElem; // store the grip to style it while resizing column

// fired when a column resize grip is clicked
// stores values for calculations when the grip is unclicked
function gripClick (e, col) {
  e.preventDefault();
  e.stopPropagation();
  selectedColElem = col;
  colWidthBeforeResize = col.style.width.slice(0, -2);
  tableWidthBeforeResize = table.style.width.slice(0, -2);
  colStartOffset = col.offsetWidth - e.pageX;
  selectedGripElem = col.getElementsByClassName('grip')[0];
};

// fired when the column resize grip is dragged
// styles the grip to show where it's being dragged
function gripDrag (e) { // move the grip where the user moves their cursor
  if (selectedColElem && selectedGripElem) {
    const newWidth = colStartOffset + e.pageX;
    selectedGripElem.style.borderLeft = '1px dotted var(--color-gray)';
    selectedGripElem.style.left = `${newWidth}px`;
  }
}

// fired when a clicked and dragged grip is dropped
// updates the column and table width and saves the values
function gripUnclick (e, vueThis) {
  if (selectedColElem && selectedGripElem) {
    const newWidth = Math.max(colStartOffset + e.pageX, 70); // min col width is 70px
    selectedColElem.style.width = `${newWidth}px`;

    for (let i = 0; i < cols.length; i++) { // get width of each col
      const col = cols[i];
      const colW = parseInt(col.style.width.slice(0, -2));
      if (vueThis.computedColumns[i]) {
        const header = vueThis.computedColumns[i];
        header.width = colW;
        vueThis.columnWidths[header.id] = colW;
      }
    }

    vueThis.saveColumnWidths();

    // update the width of the table. need to do this or else the table
    // cannot overflow its container
    const newTableWidth = parseInt(tableWidthBeforeResize) + newWidth - colWidthBeforeResize;
    table.style.width = `${newTableWidth}px`;
    vueThis.tableWidth = newTableWidth;

    if (Math.abs(vueThis.tableWidth - window.innerWidth) > 15) {
      vueThis.showFitButton = true;
    }

    selectedGripElem.style.borderLeft = 'unset';
    selectedGripElem.style.left = 'unset';
  }

  selectedGripElem = undefined;
  selectedColElem = undefined;
}

/**
 * IMPORTANT! This component kicks off the loading of the
 * data in the table once the table state has been loaded.
 * There is no need to load data in the parent.
 */
export default {
  name: 'MolochTable',
  components: { ToggleBtn },
  props: {
    loadData: { // event to fire when the table needs to load data
      type: Function,
      required: true
    },
    id: { // unique id of the table
      type: String,
      required: false
    },
    tableClasses: { // table classes to be applied to the table
      type: String,
      require: false
    },
    /* IMPORTANT:
     * All columns must have a width.
     * Columns that should be shown by default (no table state saved) must have default flag */
    columns: { // columns to be displayed in the table
      type: Array,
      required: true
    },
    actionColumn: { // whether to display an action column on the left
      type: Boolean,
      default: false
    },
    infoRow: { // whether to display a row underneath each data row with additional information
      // IMPORTANT! actionColumn must also be set to true for it to be visible
      type: Boolean,
      default: false
    },
    infoRowFunction: { // function to call to render content for more info row
      type: Function
    },
    data: { // table data
      type: Array
    },
    noResults: { // whether or not to display a no results row if data array is empty
      type: Boolean,
      default: false
    },
    sortField: { // the field that the query is sorting by
      type: String,
      required: true
    },
    desc: { // the direction the query is sorting by
      type: Boolean,
      required: true
    },
    page: { // api endpoint to fetch the table configuration info (api/user/config/:page)
      type: String,
      required: true
    },
    tableStateName: { // api endpoint to save table state (api/user/state/:tableStateName)
      type: String,
      required: true
    },
    tableWidthsStateName: { // api endpoint to save table state (api/user/state/:tableWidthsStateName)
      type: String,
      required: true
    },
    /* IMPORTANT:
     * doStat property must be set to true on the columns that should be computed
     * and these columns must be numbers */
    showAvgTot: { // whether to display the average and total rows
      type: Boolean,
      default: false
    },
    /* IMPORTANT! 'list' is the only table animation currently available */
    tableAnimation: { // table animation name
      type: String
    }
  },
  data: function () {
    return {
      error: '',
      tableDiv: undefined,
      tableDesc: undefined,
      tableSortField: undefined,
      computedColumns: [], // columns in the order computed from the saved table state
      columnWidths: {}, // width of each column that has been modified
      showFitButton: false, // whether to show the table fit button (if the table is >||< 15px of the inner window width)
      colQuery: '', // the search string for columns to add/remove from the table
      openedRows: {}, // save the opened rows so they don't get unopened when the table data refreshes
      averageValues: {}, // list of total values
      totalValues: {}, // list of total values
      zeroMap: {}, // list of values that have been cleared
      zeroedAt: {}, // list of times each column was cleared
      tableWidth: $(this.tableDiv).width()
    };
  },
  computed: {
    user: function () {
      return this.$store.state.user;
    },
    filteredColumns: function () {
      // let filteredColumns = [];
      return this.columns.filter((column) => {
        return column.name.toLowerCase().includes(this.colQuery.toLowerCase());
      });
    },
    tableColspan: function () {
      let colspan = this.computedColumns.length;
      if (this.actionColumn) { colspan++; }
      return colspan;
    }
  },
  // watch for data to change to set opened rows
  // and to recalculate the average and total rows
  watch: {
    data: function () {
      if (Object.keys(this.openedRows).length) {
        // there are opened rows
        for (const item of this.data) {
          if (this.openedRows[item.id]) {
            this.$set(item, 'opened', true);
          }
        }
      }
      if (this.showAvgTot) { // calculate avg/tot values
        for (const column of this.computedColumns) {
          if (column.doStats) {
            let totalValue = 0;
            for (const item of this.data) {
              if (!item.[column.id] && !item[column.sort]) {
                continue;
              }
              totalValue += parseInt(item[column.id || column.sort]);
            }
            this.totalValues[column.id] = totalValue;
            this.averageValues[column.id] = totalValue / this.data.length;
          }
        }
      }
    }
  },
  mounted: function () {
    this.tableDiv = `#${this.id}`;
    // IMPORTANT! this loads the data for the table after we fetch sort field and order
    UserService.getPageConfig(this.page).then((response) => {
      if (response.tableState && response.tableState.order && response.tableState.visibleHeaders) {
        // there is a saved table state for this table
        // so apply it to sortField, desc, and column order
        this.tableSortField = response.tableState.order[0][0];
        this.tableDesc = response.tableState.order[0][1] === 'desc';
        for (const c of response.tableState.visibleHeaders) {
          for (const column of this.columns) {
            if (column.id === c) {
              const newCol = this.cloneColumn(column);
              this.computedColumns.push(newCol);
            }
          }
        }
      } else {
        // this table has not been saved, so use the defaults
        this.displayDefaultColumns();
      }

      if (response.columnWidths) {
        this.columnWidths = response.columnWidths || {};
        let tableWidth = 0;
        for (const column of this.computedColumns) {
          for (const c in this.columnWidths) {
            if (column.id === c) {
              column.width = JSON.parse(JSON.stringify(this.columnWidths[c]));
            }
          }
          tableWidth += column.width;
        }

        this.tableWidth = tableWidth;
        if (Math.abs(this.tableWidth - window.innerWidth) > 15) {
          this.showFitButton = true;
        }

        if (!this.tableWidth) {
          this.tableWidth = $(this.tableDiv).width();
        }
      }

      this.initializeColResizable();

      this.loadData(this.tableSortField, this.tableDesc);
      this.initializeColDragDrop();
    }).catch((error) => {
      // if there's an error getting the table state,
      // just use the default columns and fetch the data
      this.displayDefaultColumns();
      this.initializeColDragDrop();
      this.initializeColResizable();
      this.loadData(this.tableSortField, this.tableDesc);
    });
  },
  methods: {
    /* exposed page functions ------------------------------------ */
    sort: function (column) {
      if (!column.sort) { return; }
      // if the sort field is the same, toggle it, otherwise set it to default (true)
      this.tableDesc = this.tableSortField === column.sort ? !this.tableDesc : true;
      this.tableSortField = column.sort;
      this.loadData(this.tableSortField, this.tableDesc);
      this.saveTableState();
    },
    /* fits the table to the width of the current window size */
    fitTable: function () {
      const windowWidth = window.innerWidth - 30; // account for margins
      const leftoverWidth = windowWidth - this.tableWidth;
      const percentChange = 1 + (leftoverWidth / this.tableWidth);

      for (const column of this.computedColumns) {
        if (!column.width || isNaN(column.width)) {
          // column has no width so use the default
          for (const col of this.columns) {
            column.width = parseInt(JSON.parse(JSON.stringify(col.width)));
          }
        }
        const newWidth = Math.floor(column.width * percentChange);
        column.width = newWidth;
        this.columnWidths[column.id] = newWidth;
      }

      this.tableWidth = windowWidth;
      this.showFitButton = false;

      this.$refs.table.style.width = `${windowWidth}px`;

      this.saveColumnWidths();
    },
    toggleMoreInfo: function (item) {
      this.$set(item, 'opened', !item.opened);
      this.openedRows[item.id] = !this.openedRows[item.id];
      if (this.infoRowFunction) {
        setTimeout(() => { // wait for row to expand
          this.infoRowFunction(item);
        });
      }
    },
    isVisible: function (id) {
      let index = 0;
      for (const column of this.computedColumns) {
        if (column.id === id) {
          return index;
        }
        index++;
      }
      return -1;
    },
    toggleVisibility: function (column) {
      this.destroyColResizable();

      const index = this.isVisible(column.id);
      if (index >= 0) { // it's visible
        this.computedColumns.splice(index, 1);
      } else { // it's hidden
        this.computedColumns.push(column);
      }

      // calculate table width and set showFitButton accordingly
      let tableWidth = 0;
      for (const computedCol of this.computedColumns) {
        tableWidth += computedCol.width;
      }
      this.tableWidth = tableWidth;
      this.$refs.table.style.width = `${tableWidth}px`;

      this.showFitButton = Math.abs(this.tableWidth - window.innerWidth) > 15;

      this.saveTableState();
      this.initializeColResizable();
    },
    resetDefault: function () {
      this.destroyColResizable();
      this.displayDefaultColumns();

      this.columnWidths = {};
      let tableWidth = 0;
      for (const column of this.columns) {
        for (const col of this.computedColumns) {
          if (col.id === column.id) {
            col.width = JSON.parse(JSON.stringify(column.width));
            tableWidth += col.width;
          }
        }
      }

      this.tableWidth = tableWidth;
      this.$refs.table.style.width = `${tableWidth}px`;
      if (Math.abs(this.tableWidth - window.innerWidth) > 15) {
        this.showFitButton = true;
      }

      // space out these calls so saving the column widths table state
      // doesn't overwrite the column order table state or vice versa
      this.saveColumnWidths();
      setTimeout(() => { this.saveTableState(); }, 1000);

      this.initializeColResizable();
    },
    zeroColValues: function (column) {
      this.$set(this.zeroedAt, column.id, new Date().getTime());
      this.$set(this.zeroMap, column.id, []);
      for (let i = 0; i < this.data.length; i++) {
        const data = this.data[i];
        this.$set(this.zeroMap[column.id], i, data[column.id]);
      }
    },
    calculateFormatValue: function (column, item, index) {
      // if it's not a computed field value return it immediately
      if (!column.doStats && !column.dataFunction) { return item[column.id]; }

      const itemClone = JSON.parse(JSON.stringify(item));
      let value = itemClone[column.id];

      if (value === null || value === undefined) { return; }

      if (this.zeroMap[column.id] !== undefined) {
        value = value - this.zeroMap[column.id][index];
      }

      if (value < 0) { // server reset, so update zeroMap
        this.$set(this.zeroMap[column.id], index, item[column.id]);
        value = 0;
      }

      itemClone[column.id] = value;

      return column.dataFunction ? column.dataFunction(itemClone) : value;
    },
    calculateFormatTotValue: function (column) {
      // if it's not a computed field or there's no data return empty immediately
      if (!column.doStats || !this.data || !this.data.length) { return ' '; }

      let value = this.totalValues[column.id];
      // need to recalculate the value if this column has been zeroed
      if (this.zeroMap[column.id] !== undefined) {
        // subtract all zeroed values for this column
        for (const zeroVal of this.zeroMap[column.id]) {
          value = value - zeroVal;
        }
      }

      const mock = {};
      mock[column.id] = value;

      if (column.avgTotFunction) {
        return column.avgTotFunction(mock);
      } else if (column.dataFunction) {
        return column.dataFunction(mock);
      }

      return value;
    },
    calculateFormatAvgValue: function (column) {
      // if it's not a computed field or there's no data return empty immediately
      if (!column.doStats || !this.data || !this.data.length) { return ' '; }

      let sum = 0;
      let value = this.averageValues[column.id];
      // need to recalculate the value if this column has been zeroed
      if (this.zeroMap[column.id] !== undefined) {
        for (let v = 0; v < this.data.length; v++) {
          const realValue = this.data[v];
          let val = realValue[column.id];
          val = val - this.zeroMap[column.id][v];
          sum += val;
        }
        value = sum / this.data.length;
      }

      const mock = {};
      mock[column.id] = value;

      if (column.avgTotFunction) {
        return column.avgTotFunction(mock);
      } else if (column.dataFunction) {
        return column.dataFunction(mock);
      }

      return value;
    },
    /* helper functions ------------------------------------------ */
    initializeColDragDrop: function () {
      setTimeout(() => { // wait for columns to render
        Sortable.create(this.$refs.draggableColumns, {
          animation: 100,
          filter: '.ignore-element',
          preventOnFilter: false, // allow clicks within the ignored element
          onMove: (e) => { // col header is being dragged
            // don't allow a column to be dropped in the far left column
            return !e.related.classList.contains('ignore-element');
          },
          onEnd: (e) => { // dragged col header was dropped
            // nothing has changed, so don't do stuff
            if (e.oldIndex === e.newIndex) { return; }

            // update the headers to the new order
            let oldIdx = e.oldIndex;
            let newIdx = e.newIndex;

            // account for the index of the action column
            if (this.actionColumn) {
              oldIdx--;
              newIdx--;
            }

            const element = this.computedColumns[oldIdx];
            this.computedColumns.splice(oldIdx, 1);
            this.computedColumns.splice(newIdx, 0, element);

            this.saveTableState();
          }
        });
      });
    },
    initializeColResizable: function () {
      this.$nextTick(() => {
        cols = document.getElementsByClassName('col-header');
        table = this.$refs.table;

        for (const col of cols) { // listen for grip dragging
          const grip = col.getElementsByClassName('grip')[0];
          if (grip) {
            grip.addEventListener('mousedown', (e) => gripClick(e, col));
          }
        }

        document.addEventListener('mousemove', gripDrag);
        const self = this;
        document.addEventListener('mouseup', (e) => gripUnclick(e, self));
      });
    },
    destroyColResizable () {
      if (!cols) return;

      for (const col of cols) { // remove all grip dragging listeners
        const grip = col.getElementsByClassName('grip')[0];
        if (grip) {
          grip.removeEventListener('mousedown', gripClick);
        }
      }

      // remove document listeners
      document.removeEventListener('mousemove', gripDrag);
      document.removeEventListener('mouseup', gripUnclick);

      cols = undefined;
      table = undefined;
    },
    saveTableState: function () {
      const tableState = {
        order: [[this.tableSortField, this.tableDesc === true ? 'desc' : 'asc']],
        visibleHeaders: []
      };

      for (const column of this.computedColumns) {
        tableState.visibleHeaders.push(column.id);
      }

      UserService.saveState(tableState, this.tableStateName);
    },
    saveColumnWidths: function () {
      UserService.saveState(this.columnWidths, this.tableWidthsStateName);
    },
    displayDefaultColumns: function () {
      this.computedColumns = [];
      this.tableDesc = this.desc;
      this.tableSortField = this.sortField;
      // display only the default columns
      for (const column of this.columns) {
        if (column.default) {
          const newCol = this.cloneColumn(column);
          this.computedColumns.push(newCol);
        }
      }
    },
    cloneColumn: function (column) {
      const newCol = JSON.parse(JSON.stringify(column));
      if (column.dataFunction) {
        newCol.dataFunction = column.dataFunction;
      }
      if (column.avgTotFunction) {
        newCol.avgTotFunction = column.avgTotFunction;
      }
      return newCol;
    }
  },
  beforeDestroy: function () {
    this.destroyColResizable();
  }
};
</script>

<style>
/* force border radius on col vis menu btn */
.col-vis-menu > button.btn {
  border-top-right-radius: 4px !important;
  border-bottom-right-radius: 4px !important;
}

/* don't let col vis menu overflow the page */
.col-vis-menu .dropdown-menu {
  max-height: 300px;
  overflow: auto;
}
</style>

<style scoped>
/* table fit button -------------------------- */
/* make fit button pos relative to table */
table {
  position: relative;
}
button.fit-btn {
  top: 0;
  right: 0;
  z-index: 9;
  position: absolute;
  visibility: hidden;
}
table > thead:hover button.fit-btn {
  visibility: visible;
}

/* column visibility menu styles ------------- */
.col-vis-menu .dropdown-header {
  padding: .25rem .5rem 0;
}

/* average/total delimiters ------------------ */
tr.bold {
  font-weight: bold;
}
table tr.border-bottom-bold > td {
  border-bottom: 2px solid #dee2e6;
}
table tr.border-top-bold > td {
  border-top: 2px solid #dee2e6;
}

/* slider grips indicator -------------------- */
table thead tr th {
  border-right: 1px dotted var(--color-gray);
}
table thead tr th.ignore-element {
  border-right: none;
}

/* remove terrible padding applied by the column resize lib */
.JPadding > tbody > tr > td, .JPadding > tbody > tr > th {
  padding: 0.75rem;
}
.table-sm.JPadding > tbody > tr > td, .table-sm.JPadding > tbody > tr > th {
  padding: 0.1rem 0.5rem !important;
}

/* column clear button ----------------------- */
table thead th {
  position: relative;
}
table thead th .btn-zero {
  top: 0;
  left: 2px;
  position: absolute;
  visibility: hidden;
}
table thead th:hover .btn-zero {
  visibility: visible;
}

/* break words for long values in cells ------ */
table tr td.break-all {
  word-break: break-all;
}
</style>
