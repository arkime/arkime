<!--
Copyright Yahoo Inc.
SPDX-License-Identifier: Apache-2.0
-->
<template>
  <table
    v-if="computedColumns && computedColumns.length"
    :style="`width:${tableWidth}px`"
    class="arkime-table arkime-table--xs"
    :class="tableClasses"
    ref="table"
    :id="id">
    <thead>
      <v-btn
        v-if="showFitButton"
        variant="flat"
        size="small"
        density="comfortable"
        icon
        :style="quaternaryBtnStyle"
        class="fit-btn"
        :aria-label="$t('utils.fitBtnTip')"
        @click="fitTable">
        <v-icon icon="mdi-arrow-expand-horizontal" />
        <v-tooltip activator="parent">
          {{ $t('utils.fitBtnTip') }}
        </v-tooltip>
      </v-btn>
      <tr ref="draggableColumns">
        <th
          v-if="actionColumn"
          style="width:70px;"
          class="ignore-element text-start">
          <div class="d-flex align-center">
            <!-- column visibility menu -->
            <v-menu
              :close-on-content-click="false"
              location="bottom start">
              <template #activator="{ props: activatorProps }">
                <v-btn
                  v-bind="activatorProps"
                  variant="flat"
                  size="small"
                  density="comfortable"
                  :style="primaryBtnStyle"
                  class="col-vis-trigger">
                  <v-icon icon="mdi-view-grid" />
                  <v-tooltip activator="parent">
                    {{ $t('utils.colVisBtnTip') }}
                  </v-tooltip>
                </v-btn>
              </template>
              <v-list
                density="compact"
                class="col-vis-list">
                <div class="px-2 py-1">
                  <div class="arkime-input-group arkime-input-group--fluid">
                    <input
                      type="text"
                      v-model="colQuery"
                      class="arkime-input-control"
                      :placeholder="$t('utils.colQueryPlaceholder')">
                  </div>
                </div>
                <v-divider />
                <v-list-item @click="resetDefault">
                  {{ $t('utils.resultDefaultColumns') }}
                </v-list-item>
                <v-divider />
                <v-list-item
                  v-for="column in filteredColumns"
                  :key="column.id"
                  :id="`colVis-${column.id}`"
                  :active="isVisible(column.id) >= 0"
                  @click.stop.prevent="toggleVisibility(column)">
                  {{ column.name }}
                  <v-tooltip
                    v-if="column.help"
                    activator="parent"
                    location="end">
                    {{ column.help }}
                  </v-tooltip>
                </v-list-item>
              </v-list>
            </v-menu> <!-- /column visibility menu -->
            <!-- ESNode data node only toggle -->
            <span
              v-if="showDataNodesToggle"
              class="ms-2 d-inline-block">
              <v-checkbox
                density="compact"
                hide-details
                inline
                :id="`only-data-nodes-checkbox-${id}`"
                @update:model-value="$emit('toggle-data-node-only')" />
              <v-tooltip :activator="`#only-data-nodes-checkbox-${id}`">
                {{ $t('utils.onlyShowDataNodesTip') }}
              </v-tooltip>
            </span><!-- /ESNode data node only toggle -->
          </div>
        </th>
        <th
          v-for="column in computedColumns"
          :key="column.id"
          :id="`col-${column.id}`"
          @click.self="sort(column)"
          :class="(column.classes ? `${column.classes} ` : '') + (column.sort ? 'cursor-pointer' : '')"
          :style="{'width': column.width > 0 ? `${column.width}px` : '100px'}"
          class="col-header">
          <div class="grip">
&nbsp;
          </div>
          {{ column.name }}
          <v-tooltip
            v-if="column.help"
            :activator="`#col-${column.id}`">
            {{ column.help }}
          </v-tooltip>
          <span
            v-if="column.canClear"
            class="btn-zero">
            <v-btn
              color="grey"
              variant="flat"
              size="x-small"
              density="comfortable"
              icon
              :aria-label="$t('common.clear')"
              @click="zeroColValues(column)">
              <v-icon icon="mdi-cancel" />
              <v-tooltip activator="parent">
                Set this column's values to 0.
                <strong v-if="zeroedAt && zeroedAt[column.id]">
                  <br>
                  {{ $t('utils.lastClearedAt') }}
                  {{ timezoneDateString(zeroedAt[column.id], user.settings.timezone || 'local') }}
                </strong>
              </v-tooltip>
            </v-btn>
          </span>
          <span v-if="column.sort && tableSortField === column.sort">
            <v-icon
              :icon="tableDesc ? 'mdi-chevron-down' : 'mdi-chevron-up'" />
          </span>
        </th>
      </tr>
    </thead>
    <transition-group
      tag="tbody"
      :name="tableAnimation">
      <!-- avg/total top rows -->
      <template v-if="showAvgTot && data && data.length > 9">
        <tr
          class="border-top-bold bold average-row"
          key="averageRow">
          <td v-if="actionColumn">
            Avg
          </td>
          <td
            :class="column.classes"
            v-for="(column, index) in computedColumns"
            :key="column.id + index + 'avg'">
            {{ calculateFormatAvgValue(column) }}
          </td>
        </tr>
        <tr
          class="border-bottom-bold bold total-row"
          key="totalRow">
          <td v-if="actionColumn">
            Total
          </td>
          <td
            :class="column.classes"
            v-for="(column, index) in computedColumns"
            :key="column.id + index + 'total'">
            {{ calculateFormatTotValue(column) }}
          </td>
        </tr>
      </template> <!-- /avg/total top rows -->
      <!-- data rows -->
      <template
        v-for="(item, index) of data"
        :key="item.id || index">
        <tr>
          <td
            v-if="actionColumn"
            class="text-start"
            style="overflow: visible !important;">
            <!-- toggle more info row button -->
            <toggle-btn
              v-if="infoRow"
              class="me-1"
              :opened="item.opened"
              @toggle="toggleMoreInfo(item)" /> <!-- /toggle more info row button -->
            <!-- action buttons -->
            <slot
              name="actions"
              :item="item" /> <!-- /action buttons -->
          </td>
          <!-- cell value -->
          <td
            :class="column.classes"
            v-for="(column, colindex) in computedColumns"
            :key="column.id + colindex">
            {{ calculateFormatValue(column, item, index) }}
          </td> <!-- /cell value -->
        </tr>
        <!-- more info row -->
        <tr
          class="text-start"
          v-if="infoRow && item.opened"
          :key="item.id + 'moreInfo'">
          <td :colspan="tableColspan">
            <div :id="'moreInfo-' + item.id" />
          </td>
        </tr> <!-- /more info row -->
      </template> <!-- /data rows -->
      <!-- no results -->
      <tr
        v-if="noResults && data && !data.length"
        key="noResults">
        <td
          :colspan="tableColspan"
          class="text-danger text-center">
          <v-icon icon="mdi-alert" />&nbsp;
          {{ noResultsMsg }}
        </td>
      </tr> <!-- /no results -->
    </transition-group>
    <!-- avg/total bottom rows -->
    <tfoot v-if="showAvgTot && data && data.length > 1">
      <tr class="border-top-bold bold average-row">
        <td v-if="actionColumn">
          Avg
        </td>
        <td
          :class="column.classes"
          v-for="(column, index) in computedColumns"
          :key="column.id + index + 'avgfoot'">
          {{ calculateFormatAvgValue(column) }}
        </td>
      </tr>
      <tr class="bold total-row">
        <td v-if="actionColumn">
          Total
        </td>
        <td
          :class="column.classes"
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
import ToggleBtn from '@common/ToggleBtn.vue';
import { timezoneDateString } from '@common/vueFilters.js';
import { attachTableGrips } from '@common/composables/useColumnResize.js';

const MIN_COL_WIDTH = 70;

/**
 * IMPORTANT! This component kicks off the loading of the
 * data in the table once the table state has been loaded.
 * There is no need to load data in the parent.
 */
export default {
  name: 'ArkimeTable',
  components: { ToggleBtn },
  props: {
    loadData: { // event to fire when the table needs to load data
      type: Function,
      required: true
    },
    id: { // unique id of the table
      type: String,
      required: false,
      default: ''
    },
    tableClasses: { // table classes to be applied to the table
      type: String,
      required: false,
      default: ''
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
      type: Function,
      default: () => {}
    },
    data: { // table data
      type: Array,
      default: () => []
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
      type: String,
      default: ''
    },
    noResultsMsg: { // message to display when there are no results
      type: String,
      default: 'No results match your search'
    },
    showDataNodesToggle: { // whether to show the "only data nodes" toggle (for ES Nodes stats)
      type: Boolean,
      default: false
    }
  },
  emits: ['toggle-data-node-only'],
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
      tableWidth: $(this.tableDiv).width(),
      // Arkime theme-color v-btn styles. Vuetify :color can't take CSS vars.
      primaryBtnStyle: {
        backgroundColor: 'rgb(var(--v-theme-primary))',
        color: 'rgb(var(--v-theme-button-fg))'
      },
      quaternaryBtnStyle: {
        backgroundColor: 'rgb(var(--v-theme-quaternary))',
        color: 'rgb(var(--v-theme-button-fg))'
      }
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
    data: {
      deep: true,
      handler (newVal, oldVal) {
        if (Object.keys(this.openedRows).length) {
          // there are opened rows
          for (const item of this.data) {
            if (this.openedRows[item.id]) {
              item.opened = true;
            }
          }
        }
        if (this.showAvgTot) { // calculate avg/tot values
          for (const column of this.computedColumns) {
            if (column.doStats) {
              let totalValue = 0;
              for (const item of this.data) {
                if (!item[column.id] && !item[column.sort]) {
                  continue;
                }
                totalValue += parseInt(item[column.sort || column.id]);
              }
              this.totalValues[column.sort || column.id] = totalValue;
              this.averageValues[column.sort || column.id] = totalValue / this.data.length;
            }
          }
        }
      }
    },
    // watch for columns prop changes (e.g. i18n locale change)
    // and update computedColumns in place to preserve order, visibility, and width
    columns: {
      handler (newColumns) {
        const columnMap = {};
        for (const column of newColumns) {
          columnMap[column.id] = column;
        }
        for (const computedCol of this.computedColumns) {
          const sourceCol = columnMap[computedCol.id];
          if (sourceCol) {
            computedCol.name = sourceCol.name;
            computedCol.help = sourceCol.help;
            computedCol.dataFunction = sourceCol.dataFunction;
            computedCol.avgTotFunction = sourceCol.avgTotFunction;
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

    // Surface the Fit Table button when the viewport shrinks past the content,
    // without auto-fitting (which would clobber explicit user widths).
    this._windowResizeHandler = () => {
      if (Math.abs((this.tableWidth || 0) - window.innerWidth) > 15) {
        this.showFitButton = true;
      }
    };
    window.addEventListener('resize', this._windowResizeHandler);
  },
  methods: {
    timezoneDateString,
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
        // clamp to min so narrow viewports don't push columns past the grip floor
        const newWidth = Math.max(MIN_COL_WIDTH, Math.floor(column.width * percentChange));
        column.width = newWidth;
        this.columnWidths[column.id] = newWidth;
      }

      this.tableWidth = windowWidth;
      this.showFitButton = false;

      this.$refs.table.style.width = `${windowWidth}px`;

      this.saveColumnWidths();
    },
    toggleMoreInfo: function (item) {
      item.opened = !item.opened;
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
      this.zeroedAt[column.id] = new Date().getTime();
      this.zeroMap[column.id] = [];
      for (let i = 0; i < this.data.length; i++) {
        this.zeroMap[column.id][i] = this.data[i][column.id];
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
        this.zeroMap[column.id][index] = item[column.id];
        value = 0;
      }

      itemClone[column.id] = value;

      return column.dataFunction ? column.dataFunction(itemClone) : value;
    },
    calculateFormatTotValue: function (column) {
      // if it's not a computed field or there's no data return empty immediately
      if (!column.doStats || !this.data || !this.data.length) { return ' '; }

      let value = this.totalValues[column.sort || column.id];
      // need to recalculate the value if this column has been zeroed
      if (this.zeroMap[column.id] !== undefined) {
        // subtract all zeroed values for this column
        for (const zeroVal of this.zeroMap[column.id]) {
          value = value - zeroVal;
        }
      }

      const mock = {};
      mock[column.sort || column.id] = value;

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
      let value = this.averageValues[column.sort || column.id];
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
      mock[column.sort || column.id] = value;

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
            // Grip handlers capture colIndex in a closure at attach time,
            // so they go stale after a reorder. Rebuild them against the
            // new column order.
            this.initializeColResizable();
          }
        });
      });
    },
    initializeColResizable: function () {
      this.$nextTick(() => {
        this.destroyColResizable(); // idempotent re-init
        this._gripAttachment = attachTableGrips({
          cols: document.getElementsByClassName('col-header'),
          table: this.$refs.table,
          minWidth: MIN_COL_WIDTH,
          onResetAll: () => {
            // Ctrl+Shift+click any grip → reset column widths/order to the
            // in-code defaults (handy for testing layout defaults).
            this.resetDefault();
          },
          onCommit: ({ colIndex, newWidth, newTableWidth }) => {
            // Only update the dragged column. Reading offsetWidth from every
            // column would clobber state with whatever widths the browser
            // happened to render (table-layout:auto redistributes width based
            // on content), so e.g. dragging the name|locked grip could end up
            // growing the num column.
            const header = this.computedColumns[colIndex];
            if (header) {
              header.width = newWidth;
              this.columnWidths[header.id] = newWidth;
            }
            this.saveColumnWidths();
            this.tableWidth = newTableWidth;
            if (Math.abs(newTableWidth - window.innerWidth) > 15) {
              this.showFitButton = true;
            }
          }
        });
      });
    },
    destroyColResizable () {
      if (this._gripAttachment) {
        this._gripAttachment.detach();
        this._gripAttachment = null;
      }
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
  beforeUnmount () {
    this.destroyColResizable();
    if (this._windowResizeHandler) {
      window.removeEventListener('resize', this._windowResizeHandler);
      this._windowResizeHandler = null;
    }
  }
};
</script>

<style scoped>
/* col visibility menu list */
.col-vis-list {
  max-height: 300px;
  overflow: auto;
  width: 280px;
}

/* table fit button -------------------------- */
/* make fit button pos relative to table */
table {
  position: relative;
}
.fit-btn {
  top: 0;
  right: 0;
  z-index: 9;
  position: absolute;
  visibility: hidden;
}
table > thead:hover .fit-btn {
  visibility: visible;
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
  border-right: 1px dotted rgb(var(--v-theme-neutral));
  vertical-align: middle;
  text-align: left;
}
table tbody tr td {
  vertical-align: middle;
}
table thead tr th.ignore-element {
  border-right: none;
}

/* remove terrible padding applied by the column resize lib */
.JPadding > tbody > tr > td, .JPadding > tbody > tr > th {
  padding: 0.1rem 0.5rem !important;
}

/* column clear button ----------------------- */
table thead th {
  position: relative;
}
table thead th .btn-zero {
  top: 1px;
  right: 4px;
  position: absolute;
  visibility: hidden;
}
table thead th .btn-zero .btn {
  padding: 0 4px;
  font-size: 0.7rem;
  line-height: 1.2;
}
table thead th:hover .btn-zero {
  visibility: visible;
}

/* break words for long values in cells ------ */
table tr td.break-all {
  word-break: break-all;
}
</style>
