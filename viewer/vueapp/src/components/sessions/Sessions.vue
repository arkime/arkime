<!--
Copyright Yahoo Inc.
SPDX-License-Identifier: Apache-2.0
-->
<template>
  <div
    class="sessions-page"
    :class="{'hide-tool-bars': !showToolBars}">
    <ArkimeCollapsible>
      <span class="fixed-header">
        <!-- search navbar -->
        <arkime-search
          :fields="headers"
          :open-sessions="stickySessions"
          :num-visible-sessions="query.length"
          :num-matching-sessions="sessions.recordsFiltered"
          :start="query.start"
          @change-search="cancelAndLoad(true)"
          @set-view="loadNewView"
          @set-columns="loadColumns"
          @recalc-collapse="$emit('recalc-collapse')" /> <!-- /search navbar -->

        <!-- paging navbar -->
        <div class="d-flex justify-content-start align-items-baseline m-1">
          <arkime-paging
            style="height: 32px;"
            class="ms-2"
            :records-total="sessions.recordsTotal"
            :records-filtered="sessions.recordsFiltered"
            @change-paging="changePaging" />
        </div> <!-- /paging navbar -->
      </span>
    </ArkimeCollapsible>

    <!-- visualizations -->
    <arkime-visualizations
      v-if="graphData && showToolBars"
      :primary="true"
      :map-data="mapData"
      :graph-data="graphData"
      @fetch-map-data="fetchMapData"
      @spanning-change="loadData"
      :timeline-data-filters="timelineDataFilters" />
    <!-- /visualizations -->

    <div
      class="sessions-content ms-2"
      id="sessions-content"
      ref="sessionsContent">
      <!-- table view -->
      <div>
        <!-- sticky (opened) sessions -->
        <transition name="leave">
          <arkime-sticky-sessions
            class="sticky-sessions"
            v-if="stickySessions.length"
            :ms="user.settings.ms"
            :sessions="stickySessions"
            @close-session="closeSession"
            @close-all-sessions="closeAllSessions" />
        </transition> <!-- /sticky (opened) sessions -->

        <!-- sessions results -->
        <table
          class="table-striped sessions-table"
          :style="`width:${tableWidth}px`"
          :class="{'sticky-header':stickyHeader}"
          ref="sessionsTable"
          id="sessionsTable">
          <thead
            ref="tableHeader"
            id="sessions-table-header"
            style="overflow:scroll">
            <tr ref="draggableColumns">
              <!-- table options -->
              <th
                class="ignore-element"
                style="width:85px;">
                <!-- table fit button -->
                <div class="fit-btn-container">
                  <template v-if="sessions.data && sessions.data.length <= 50">
                    <button
                      id="openAllSessions"
                      @click="openAll"
                      class="btn btn-xs btn-theme-tertiary open-all-btn">
                      <span class="fa fa-plus-circle" />
                      <BTooltip
                        target="openAllSessions"
                        noninteractive
                        placement="right"
                        boundary="viewport"
                        teleport-to="body">
                        {{ $t('sessions.sessions.openAll') }}
                      </BTooltip>
                    </button>
                  </template>
                  <button
                    id="closeAllSessions"
                    @click="closeAll"
                    v-if="!loading && stickySessions.length > 0"
                    class="btn btn-xs btn-theme-secondary close-all-btn ms-4">
                    <span class="fa fa-times-circle" />
                    <BTooltip
                      target="closeAllSessions"
                      noninteractive
                      placement="right"
                      boundary="viewport"
                      teleport-to="body">
                      {{ $t('sessions.sessions.closeAll') }}
                    </BTooltip>
                  </button>
                  <button
                    id="fitTable"
                    @click="fitTable"
                    v-if="showFitButton && !loading"
                    class="btn btn-xs btn-theme-quaternary fit-btn"
                    :class="{'ms-4':stickySessions.length === 0, 'fit-btn-right':sessions.data && sessions.data.length <= 50 && stickySessions.length > 0}">
                    <span class="fa fa-arrows-h" />
                    <BTooltip
                      target="fitTable"
                      noninteractive
                      placement="right"
                      boundary="viewport"
                      teleport-to="body">
                      {{ $t('sessions.sessions.fitTable') }}
                    </BTooltip>
                  </button>
                </div> <!-- /table fit button -->
                <!-- column visibility button -->
                <FieldSelectDropdown
                  class="me-1"
                  :selected-fields="tableState.visibleHeaders"
                  :tooltip-text="$t('sessions.sessions.toggleColumns')"
                  :search-placeholder="$t('sessions.sessions.searchColumns')"
                  :exclude-filename="true"
                  :max-visible-fields="maxVisibleFields"
                  field-id-key="dbField"
                  @toggle="toggleColVis" />
                <!-- /column visibility button -->
                <!-- column save button -->
                <b-dropdown
                  lazy
                  no-flip
                  no-caret
                  size="sm"
                  teleport-to="body"
                  boundary="viewport"
                  menu-class="col-dropdown-menu"
                  class="col-dropdown d-inline-block"
                  variant="theme-secondary">
                  <template #button-content>
                    <span
                      class="fa fa-save"
                      id="colConfigMenu">
                      <BTooltip
                        target="colConfigMenu"
                        noninteractive
                        placement="right"
                        boundary="viewport"
                        teleport-to="body">{{ $t('sessions.sessions.customColumnMsg') }}</BTooltip>
                    </span>
                  </template>
                  <b-dropdown-header header-class="p-1">
                    <div class="input-group input-group-sm">
                      <b-input
                        autofocus
                        @click.stop
                        maxlength="30"
                        class="form-control"
                        v-model="newColConfigName"
                        :placeholder="$t('sessions.sessions.customColumnName')"
                        @keydown.enter="saveColumnConfiguration" />
                      <button
                        type="button"
                        class="btn btn-theme-secondary"
                        :disabled="!newColConfigName"
                        @click="saveColumnConfiguration">
                        <span class="fa fa-save" />
                      </button>
                    </div>
                  </b-dropdown-header>
                  <b-dropdown-divider />
                  <transition-group
                    name="list"
                    tag="span">
                    <b-dropdown-item
                      id="colConfigDefault"
                      key="col-config-default"
                      @click.stop.prevent="loadColumnConfiguration(-1)">
                      {{ $t('sessions.sessions.arkimeDefault') }}
                      <BTooltip
                        target="colConfigDefault"
                        noninteractive
                        placement="right"
                        boundary="viewport"
                        teleport-to="body">
                        {{ $t('sessions.sessions.customColumnReset') }}
                      </BTooltip>
                    </b-dropdown-item>
                    <b-dropdown-item
                      v-for="(config, key) in colConfigs"
                      :key="config.name"
                      @click.self.stop.prevent="loadColumnConfiguration(key)">
                      <button
                        class="btn btn-xs btn-danger pull-right ms-1"
                        type="button"
                        @click.stop.prevent="deleteColumnConfiguration(config.name, key)">
                        <span class="fa fa-trash-o" />
                      </button>
                      <button
                        id="updateColumnConfiguration"
                        class="btn btn-xs btn-warning pull-right"
                        type="button"
                        @click.stop.prevent="updateColumnConfiguration(config.name, key)">
                        <span class="fa fa-save" />
                        <BTooltip
                          target="updateColumnConfiguration"
                          noninteractive
                          placement="right"
                          boundary="viewport"
                          teleport-to="body">
                          {{ $t('sessions.sessions.customColumnUpdate') }}
                        </BTooltip>
                      </button>
                      {{ config.name }}
                    </b-dropdown-item>
                    <b-dropdown-item
                      key="col-config-error"
                      v-if="colConfigError">
                      <span class="text-danger">
                        <span class="fa fa-exclamation-triangle" />
                        {{ colConfigError }}
                      </span>
                    </b-dropdown-item>
                    <b-dropdown-item
                      key="col-config-success"
                      v-if="colConfigSuccess">
                      <span class="text-success">
                        <span class="fa fa-check" />
                        {{ colConfigSuccess }}
                      </span>
                    </b-dropdown-item>
                  </transition-group>
                </b-dropdown> <!-- /column save button -->
              </th> <!-- /table options -->
              <!-- table headers -->
              <template v-if="headers && headers.length">
                <th
                  v-for="header of headers"
                  :key="header.dbField"
                  class="arkime-col-header"
                  :style="{'width': header.width > 0 ? `${header.width}px` : '100px'}"
                  :class="{'active':isSorted(header.sortBy || header.dbField) >= 0, 'info-col-header': header.dbField === 'info'}">
                  <div
                    class="grip"
                    v-if="header.dbField !== 'info'">
                  &nbsp;
                  </div>
                  <!-- non-sortable column -->
                  <span
                    v-if="header.dbField === 'info'"
                    class="cursor-pointer">
                    {{ header.friendlyName }}
                    <!-- info field config button -->
                    <b-dropdown
                      lazy
                      right
                      no-flip
                      no-caret
                      size="sm"
                      teleport-to="body"
                      boundary="viewport"
                      variant="theme-secondary"
                      menu-class="col-dropdown-menu"
                      class="info-vis-menu pull-right col-dropdown">
                      <template #button-content>
                        <span
                          class="fa fa-save"
                          id="infoConfigMenuSave">
                          <BTooltip
                            target="infoConfigMenuSave"
                            noninteractive
                            placement="right"
                            boundary="viewport"
                            teleport-to="body">{{ $t('sessions.sessions.customInfoMsg') }}</BTooltip>
                        </span>
                      </template>
                      <b-dropdown-header header-class="p-1">
                        <div class="input-group input-group-sm">
                          <b-input
                            autofocus
                            @click.stop
                            maxlength="30"
                            class="form-control"
                            v-model="newInfoConfigName"
                            :placeholder="$t('sessions.sessions.customInfoName')"
                            @keydown.enter="saveInfoFieldLayout" />
                          <button
                            type="button"
                            class="btn btn-theme-secondary"
                            :disabled="!newInfoConfigName"
                            @click="saveInfoFieldLayout">
                            <span class="fa fa-save" />
                          </button>
                        </div>
                      </b-dropdown-header>
                      <b-dropdown-divider />
                      <b-dropdown-item
                        key="infodefault"
                        id="infodefault"
                        @click.stop.prevent="resetInfoVisibility">
                        {{ $t('sessions.sessions.arkimeDefault') }}
                        <BTooltip
                          target="infodefault"
                          noninteractive
                          placement="right"
                          boundary="viewport"
                          teleport-to="body">
                          {{ $t('sessions.sessions.customInfoReset') }}
                        </BTooltip>
                      </b-dropdown-item>
                      <transition-group
                        name="list"
                        tag="span">
                        <b-dropdown-divider
                          key="infodivider"
                          v-if="infoConfigs.length" />
                        <b-dropdown-item
                          v-for="(config, key) in infoConfigs"
                          :key="config.name"
                          @click.self.stop.prevent="loadInfoFieldLayout(key)">
                          <button
                            class="btn btn-xs btn-danger pull-right ms-1"
                            type="button"
                            @click.stop.prevent="deleteInfoFieldLayout(config.name, key)">
                            <span class="fa fa-trash-o" />
                          </button>
                          <button
                            id="updateInfoFieldConfiguration"
                            class="btn btn-xs btn-warning pull-right"
                            type="button"
                            @click.stop.prevent="updateInfoFieldLayout(config.name, key)">
                            <span class="fa fa-save" />
                            <BTooltip
                              target="updateInfoFieldConfiguration"
                              noninteractive
                              placement="right"
                              boundary="viewport"
                              teleport-to="body">
                              {{ $t('sessions.sessions.customInfoUpdate') }}
                            </BTooltip>
                          </button>
                          {{ config.name }}
                        </b-dropdown-item>
                        <b-dropdown-item
                          key="info-config-error"
                          v-if="infoConfigError">
                          <span class="text-danger">
                            <span class="fa fa-exclamation-triangle" />
                            {{ infoConfigError }}
                          </span>
                        </b-dropdown-item>
                        <b-dropdown-item
                          key="info-config-success"
                          v-if="infoConfigSuccess">
                          <span class="text-success">
                            <span class="fa fa-check" />
                            {{ infoConfigSuccess }}
                          </span>
                        </b-dropdown-item>
                      </transition-group>
                    </b-dropdown> <!-- /info field config button -->
                    <!-- info field visibility button -->
                    <b-dropdown
                      lazy
                      right
                      no-flip
                      no-caret
                      size="sm"
                      teleport-to="body"
                      boundary="viewport"
                      menu-class="col-dropdown-menu"
                      class="info-vis-menu pull-right col-dropdown me-1"
                      variant="theme-primary"
                      @show="infoFieldVisMenuOpen = true"
                      @hide="infoFieldVisMenuOpen = false; showAllInfoFields = false">
                      <template #button-content>
                        <span
                          class="fa fa-bars"
                          id="infoConfigMenu">
                          <BTooltip
                            target="infoConfigMenu"
                            noninteractive
                            placement="right"
                            boundary="viewport"
                            teleport-to="body">
                            {{ $t('sessions.sessions.toggleInfoFields') }}
                          </BTooltip>
                        </span>
                      </template>
                      <b-dropdown-header header-class="p-1">
                        <b-input
                          autofocus
                          v-model="colQuery"
                          @input="debounceInfoColQuery"
                          @click.stop
                          class="form-control form-control-sm dropdown-typeahead"
                          :placeholder="$t('common.searchForFields')" />
                      </b-dropdown-header>
                      <b-dropdown-divider />
                      <template v-if="infoFieldVisMenuOpen">
                        <b-dropdown-item v-if="!filteredInfoFieldsCount">
                          {{ $t('sessions.sessions.noFieldsMatch') }}
                        </b-dropdown-item>
                        <template
                          v-for="(group, key) in visibleFilteredInfoFields"
                          :key="key">
                          <b-dropdown-header
                            v-if="group.length"
                            class="group-header"
                            header-class="p-1 text-uppercase">
                            {{ key }}
                          </b-dropdown-header>
                          <template
                            v-for="(field, k) in group"
                            :key="key + k + 'infoitem'">
                            <b-dropdown-item
                              :id="key + k + 'infoitem'"
                              :class="{'active':isInfoVisible(field.dbField) >= 0}"
                              @click.prevent.stop="toggleInfoVis(field.dbField)">
                              {{ field.friendlyName }}
                              <small>({{ field.exp }})</small>
                              <BTooltip
                                v-if="field.help"
                                :target="key + k + 'infoitem'"
                                noninteractive
                                placement="right"
                                boundary="viewport"
                                teleport-to="body">{{ field.help }}</BTooltip>
                            </b-dropdown-item>
                          </template>
                        </template>
                        <button
                          v-if="hasMoreInfoFields"
                          type="button"
                          @click.stop="showAllInfoFields = true"
                          class="dropdown-item text-center cursor-pointer">
                          <strong>Show {{ $t('sessions.sessions.showMoreFields', filteredInfoFieldsCount - maxVisibleFields) }}</strong>
                        </button>
                      </template>
                    </b-dropdown> <!-- /info field visibility button -->
                  </span> <!-- /non-sortable column -->
                  <!-- column dropdown menu -->
                  <b-dropdown
                    lazy
                    right
                    no-flip
                    size="sm"
                    teleport-to="body"
                    boundary="viewport"
                    menu-class="col-dropdown-menu"
                    class="pull-right col-dropdown">
                    <b-dropdown-item
                      @click="toggleColVis(header.dbField, header.sortBy)">
                      {{ $t('sessions.hideColumn') }}
                    </b-dropdown-item>
                    <!-- single field column -->
                    <template v-if="!header.children && header.type !== 'seconds'">
                      <b-dropdown-divider />
                      <b-dropdown-item
                        @click="exportUnique(header.rawField || header.exp, 0)">
                        {{ $t('sessions.exportUnique', {name: header.friendlyName}) }}
                      </b-dropdown-item>
                      <b-dropdown-item
                        @click="exportUnique(header.rawField || header.exp, 1)">
                        {{ $t('sessions.exportUniqueCounts', {name: header.friendlyName}) }}
                      </b-dropdown-item>
                      <template v-if="header.portField">
                        <b-dropdown-item
                          @click="exportUnique(header.rawField || header.exp + ':' + header.portField, 0)">
                          {{ $t('sessions.exportUniquePort', {name: header.friendlyName}) }}
                        </b-dropdown-item>
                        <b-dropdown-item
                          @click="exportUnique(header.rawField || header.exp + ':' + header.portField, 1)">
                          {{ $t('sessions.exportUniquePortCounts', {name: header.friendlyName}) }}
                        </b-dropdown-item>
                      </template>
                      <b-dropdown-item
                        @click="openSpiGraph(header.dbField)">
                        {{ $t('sessions.openSpiGraph', {name: header.friendlyName}) }}
                      </b-dropdown-item>
                      <b-dropdown-item
                        @click="fieldExists(header.exp, '==')">
                        {{ $t('sessions.addExists', {name: header.friendlyName}) }}
                      </b-dropdown-item>
                      <b-dropdown-item
                        @click="pivot(header.dbField, header.exp)">
                        {{ $t('sessions.pivotOn', {name: header.friendlyName}) }}
                      </b-dropdown-item>
                      <!-- field actions -->
                      <field-actions
                        :separator="true"
                        :expr="header.exp" />
                    </template> <!-- /single field column -->
                    <!-- multiple field column -->
                    <template v-else-if="header.children && header.type !== 'seconds'">
                      <span
                        v-for="(child, key) in header.children"
                        :key="`child${key}`">
                        <template v-if="child">
                          <b-dropdown-divider />
                          <b-dropdown-item
                            @click="exportUnique(child.rawField || child.exp, 0)">
                            {{ $t('sessions.exportUnique', {name: child.friendlyName}) }}
                          </b-dropdown-item>
                          <b-dropdown-item
                            @click="exportUnique(child.rawField || child.exp, 1)">
                            {{ $t('sessions.exportUniqueCounts', {name: child.friendlyName}) }}
                          </b-dropdown-item>
                          <template v-if="child.portField">
                            <b-dropdown-item
                              @click="exportUnique(child.rawField || child.exp + ':' + child.portField, 0)">
                              {{ $t('sessions.exportUniquePort', {name: child.friendlyName}) }}
                            </b-dropdown-item>
                            <b-dropdown-item
                              @click="exportUnique(child.rawField || child.exp + ':' + child.portField, 1)">
                              {{ $t('sessions.exportUniquePortCounts', {name: child.friendlyName}) }}
                            </b-dropdown-item>
                          </template>
                          <b-dropdown-item
                            @click="openSpiGraph(child.dbField)">
                            {{ $t('sessions.openSpiGraph', {name: child.friendlyName}) }}
                          </b-dropdown-item>
                          <b-dropdown-item
                            @click="fieldExists(child.exp, '==')">
                            {{ $t('sessions.addExists', {name: child.friendlyName}) }}
                          </b-dropdown-item>
                          <b-dropdown-item
                            @click="pivot(child.dbField, child.exp)">
                            {{ $t('sessions.pivotOn', {name: child.friendlyName}) }}
                          </b-dropdown-item>
                          <!-- field actions -->
                          <field-actions
                            :expr="child.exp"
                            :separator="false" />
                        </template>
                      </span>
                    </template> <!-- /multiple field column -->
                  </b-dropdown> <!-- /column dropdown menu -->
                  <!-- sortable column -->
                  <span
                    v-if="(header.exp || header.sortBy) && !header.unsortable"
                    @mousedown="mouseDown"
                    @mouseup="mouseUp"
                    @click="sortBy($event, header.sortBy || header.dbField)"
                    class="cursor-pointer">
                    <div class="header-sort">
                      <span
                        v-if="isSorted(header.sortBy || header.dbField) < 0"
                        class="fa fa-sort text-muted-more" />
                      <span
                        v-if="isSorted(header.sortBy || header.dbField) >= 0 && getSortOrder(header.sortBy || header.dbField) === 'asc'"
                        class="fa fa-sort-asc" />
                      <span
                        v-if="isSorted(header.sortBy || header.dbField) >= 0 && getSortOrder(header.sortBy || header.dbField) === 'desc'"
                        class="fa fa-sort-desc" />
                    </div>
                    <div class="header-text">
                      {{ header.friendlyName }}
                    </div>
                  </span> <!-- /sortable column -->
                </th> <!-- /table headers -->
              </template>
            </tr>
          </thead>

          <tbody
            class="small"
            id="sessions-table-body">
            <!-- session + detail -->
            <template
              v-for="(session, index) of sessions.data"
              :key="session.id">
              <tr
                class="sessions-scroll-margin"
                :ref="`tableRow${index}`"
                :id="`session${session.id}`">
                <!-- toggle button and ip protocol -->
                <td class="ignore-element">
                  <toggle-btn
                    class="mt-1"
                    :opened="session.expanded"
                    @toggle="toggleSessionDetail(session)" />
                  <span v-if="session.ipProtocol === 0">
                    notip
                  </span>
                  <arkime-session-field
                    v-else
                    :field="{dbField:'ipProtocol', exp:'ip.protocol', type:'lotermfield', group:'general', transform:'ipProtocolLookup'}"
                    :session="session"
                    :expr="'ip.protocol'"
                    :value="session.ipProtocol"
                    :pull-left="true"
                    :parse="true" />
                &nbsp;
                </td> <!-- /toggle button and ip protocol -->
                <!-- field values -->
                <td
                  v-for="col in headers"
                  :key="col.dbField"
                  :style="{'width': `${col.width}px`}">
                  <!-- field value is an array -->
                  <span v-if="Array.isArray(session[col.dbField])">
                    <span
                      v-for="value in session[col.dbField]"
                      :key="value + col.dbField">
                      <arkime-session-field
                        :field="col"
                        :session="session"
                        :expr="col.exp"
                        :value="value"
                        :parse="true" />
                    </span>
                  </span> <!-- /field value is an array -->
                  <!-- field value a single value -->
                  <span v-else>
                    <arkime-session-field
                      :field="col"
                      :session="session"
                      :expr="col.exp"
                      :value="session[col.dbField]"
                      :parse="true"
                      :info-fields="infoFields" />
                  </span> <!-- /field value a single value -->
                </td> <!-- /field values -->
              </tr>
              <!-- session detail -->
              <tr
                :key="session.id + '-detail'"
                v-if="session.expanded"
                class="session-detail-row">
                <td
                  :colspan="headers.length + 1"
                  :style="tableWidthStyle">
                  <suspense>
                    <arkime-session-detail
                      :session="session"
                      @toggle-col-vis="toggleColVis"
                      @toggle-info-vis="toggleInfoVis" />
                    <template #fallback>
                      <div class="mt-1 mb-1 large">
                        <span class="fa fa-spinner fa-spin me-2" />
                        {{ $t('sessions.sessions.loadingSessionDetail') }}
                      </div>
                    </template>
                  </suspense>
                </td>
              </tr> <!-- /session detail -->
            </template> <!-- /session + detail -->
          </tbody>
        </table> <!-- /sessions results -->

        <!-- loading overlay -->
        <arkime-loading
          :can-cancel="true"
          v-if="loading && !error"
          @cancel="cancelAndLoad(false)" /> <!-- /loading overlay -->

        <!-- page error -->
        <arkime-error
          v-if="error"
          :message="error"
          class="mt-5 mb-5" /> <!-- /page error -->

        <!-- no results -->
        <arkime-no-results
          v-if="!error && !loading && !(sessions.data && sessions.data.length)"
          class="mt-5 mb-5"
          :records-total="sessions.recordsTotal"
          :view="query.view" /> <!-- /no results -->
      </div> <!-- /table view -->
    </div>
  </div>
</template>

<script>
// IMPORTANT: don't change the order of imports (it messes up the flot graph)
// import services
import FieldService from '../search/FieldService';
import SessionsService from './SessionsService';
import UserService from '../users/UserService';
import ConfigService from '../utils/ConfigService';
import Utils from '../utils/utils';
// import components
import ArkimeSearch from '../search/Search.vue';
import customCols from './customCols.json';
import ArkimePaging from '../utils/Pagination.vue';
import ToggleBtn from '@common/ToggleBtn.vue';
import ArkimeError from '../utils/Error.vue';
import ArkimeLoading from '../utils/Loading.vue';
import ArkimeNoResults from '../utils/NoResults.vue';
import ArkimeSessionDetail from './SessionDetail.vue';
import ArkimeCollapsible from '../utils/CollapsibleWrapper.vue';
import ArkimeVisualizations from '../visualizations/Visualizations.vue';
import ArkimeStickySessions from './StickySessions.vue';
import FieldActions from './FieldActions.vue';
import FieldSelectDropdown from '../utils/FieldSelectDropdown.vue';
// import utils
import { searchFields, buildExpression } from '@common/vueFilters.js';
// import external
import Sortable from 'sortablejs';

const defaultInfoFields = JSON.parse(JSON.stringify(customCols.info.children));

let searchIssued = false;
let holdingClick = false;
let timeout;
let filterFieldsTimeout;

let colDragDropInitialized;

// window/table resize variables
let resizeTimeout;
let windowResizeEvent;
const defaultColWidths = {
  firstPacket: 100,
  lastPacket: 100,
  src: 140,
  srcPort: 100,
  dst: 140,
  dstPort: 100,
  totPackets: 100,
  dbby: 120,
  node: 100,
  info: 250
};

// column resize variables and functions
let colResizeInitialized = false;
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
    vueThis.loading = true;

    const newWidth = Math.max(colStartOffset + e.pageX, 70); // min col width is 70px
    selectedColElem.style.width = `${newWidth}px`;

    let hasInfo = false;
    for (let i = 0; i < cols.length; i++) { // get width of each col
      const col = cols[i];
      const colW = parseInt(col.style.width.slice(0, -2));
      if (vueThis.headers[i]) {
        const header = vueThis.headers[i];
        if (header.exp === 'info') { // ignore info col, it resizes to fit the window
          hasInfo = true;
          continue;
        }
        header.width = colW;
        vueThis.colWidths[header.dbField] = colW;
      }
    }

    vueThis.saveColumnWidths();
    vueThis.mapHeadersToFields();

    // update the width of the table. need to do this or else the table
    // cannot overflow its container
    if (!hasInfo) { // if there is no info column update the table width
      // if there is an info column, don't do anything, the info column
      // resizes to take up the rest of the window
      const diff = newWidth - colWidthBeforeResize;
      table.style.width = `${parseInt(tableWidthBeforeResize) + parseInt(diff)}px`;
    }

    selectedGripElem.style.borderLeft = 'unset';
    selectedGripElem.style.left = 'unset';

    vueThis.loading = false;
  }

  selectedGripElem = undefined;
  selectedColElem = undefined;
}

// fired when a scroll event is captured on this page
// scrolls the table header and body together if the header is sticky
function docScroll (e, vueThis) {
  if (!vueThis.stickyHeader) { return; }

  let sibling;
  if (e.target.id === 'sessions-content') {
    sibling = vueThis.$refs.tableHeader;
  } else if (e.target.id === 'sessions-table-header') {
    sibling = vueThis.$refs.sessionsContent;
  } else {
    return;
  }
  sibling.scrollLeft = e.target.scrollLeft;
}

// save a pending promise to be able to cancel it
let pendingPromise;

export default {
  name: 'Sessions',
  components: {
    ArkimeSearch,
    ArkimePaging,
    ToggleBtn,
    ArkimeError,
    ArkimeLoading,
    ArkimeNoResults,
    ArkimeSessionDetail,
    ArkimeVisualizations,
    ArkimeStickySessions,
    ArkimeCollapsible,
    FieldActions,
    FieldSelectDropdown
  },
  emits: ['recalc-collapse'],
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
      infoFieldVisMenuOpen: false,
      stickyHeader: false,
      tableHeaderOverflow: undefined,
      showFitButton: false,
      tableWidth: window.innerWidth - 20, // account for margins
      filteredFields: [],
      filteredFieldsCount: 0,
      filteredInfoFields: [],
      filteredInfoFieldsCount: 0,
      infoConfigs: [],
      newInfoConfigName: '',
      infoConfigError: '',
      infoConfigSuccess: '',
      maxVisibleFields: 50, // limit initial field rendering for performance
      showAllFields: false,
      showAllInfoFields: false,
      tableState: Utils.getDefaultTableState()
    };
  },
  created: function () {
    this.getSessionsConfig(); // IMPORTANT: kicks off the initial search query!
    ConfigService.getFieldActions();

    // watch for window resizing and update the info column width
    // this is only registered when the user has not set widths for any
    // columns && the info column is visible
    windowResizeEvent = () => {
      if (resizeTimeout) { clearTimeout(resizeTimeout); }
      resizeTimeout = setTimeout(() => {
        this.mapHeadersToFields();
      }, 500);
    };

    window.addEventListener('resize', windowResizeEvent, { passive: true });

    UserService.getState('sessionDetailDLWidth').then((response) => {
      this.$store.commit('setSessionDetailDLWidth', response.data?.width ?? 160);
    });
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
        spanning: this.$route.query.spanning === 'true' ? true : undefined,
        view: this.$route.query.view || undefined,
        expression: this.$store.state.expression || undefined,
        cluster: this.$route.query.cluster || undefined
      };
    },
    sorts: {
      get: function () {
        return this.$store.state.sorts || 'firstPacket:desc';
      },
      set: function (newValue) {
        for (const sortArr of newValue) {
          // if sorting by a custom col, need to use sortBy property
          const sortField = FieldService.getFieldProperty(sortArr[0], 'sortBy');
          if (sortField) { sortArr[0] = sortField; }
        }
        this.$store.commit('setSorts', newValue);
      }
    },
    user: function () {
      return this.$store.state.user;
    },
    timelineDataFilters: function () {
      return this.$store.state.user.settings.timelineDataFilters.map(i => FieldService.getField(i));
    },
    views: function () {
      return this.$store.state.views;
    },
    tableWidthStyle () {
      return { width: `${table.clientWidth}px` };
    },
    showToolBars: function () {
      return this.$store.state.showToolBars;
    },
    fields: function () {
      return this.$store.state.fieldsMap;
    },
    hideViz: function () {
      return this.$store.state.hideViz;
    },
    dlWidth: {
      get: function () {
        return this.$store.state.sessionDetailDLWidth || 160;
      },
      set: function (newValue) {
        this.$store.commit('setSessionDetailDLWidth', newValue);
      }
    },
    // Performance optimization: cache field visibility to avoid function calls in template
    fieldVisibilityMap: function () {
      const map = {};
      if (this.tableState && this.tableState.visibleHeaders) {
        for (const headerId of this.tableState.visibleHeaders) {
          map[headerId] = true;
        }
      }
      return map;
    },
    // Performance optimization: limit fields shown initially, expand on demand
    visibleFilteredFields: function () {
      if (!this.filteredFields || this.showAllFields) {
        return this.filteredFields;
      }

      const limited = {};
      let totalCount = 0;

      for (const [groupName, fields] of Object.entries(this.filteredFields)) {
        if (totalCount >= this.maxVisibleFields) break;

        limited[groupName] = fields.slice(0, Math.max(0, this.maxVisibleFields - totalCount));
        totalCount += limited[groupName].length;
      }

      return limited;
    },
    hasMoreFields: function () {
      return !this.showAllFields && this.filteredFieldsCount > this.maxVisibleFields;
    },
    // Performance optimization: limit info fields shown initially, expand on demand
    visibleFilteredInfoFields: function () {
      if (!this.filteredInfoFields || this.showAllInfoFields) {
        return this.filteredInfoFields;
      }

      const limited = {};
      let totalCount = 0;

      for (const [groupName, fields] of Object.entries(this.filteredInfoFields)) {
        if (totalCount >= this.maxVisibleFields) break;

        limited[groupName] = fields.slice(0, Math.max(0, this.maxVisibleFields - totalCount));
        totalCount += limited[groupName].length;
      }

      return limited;
    },
    hasMoreInfoFields: function () {
      return !this.showAllInfoFields && this.filteredInfoFieldsCount > this.maxVisibleFields;
    }
  },
  watch: {
    '$store.state.stickyViz': function () {
      this.stickyHeader = this.$store.state.stickyViz;
      this.toggleStickyHeader();
    },
    '$store.state.fetchGraphData': function (value) {
      if (value) { this.fetchGraphData(); }
    }
  },
  methods: {
    loadNewView: function () {
      this.viewChanged = true;
    },
    loadColumns: function (colConfig) {
      this.tableState = JSON.parse(JSON.stringify(colConfig));
      this.loadData(true);
    },
    /* show the overflow when a dropdown in a column header is shown. otherwise,
     * the dropdown is cut off and scrolls vertically in the column header */
    dropdownShowListener: function (bvEvent) {
      if (!this.stickyHeader) { return; }
      const target = $(bvEvent.target);
      if (!target) { return; }
      if (!target.parent().hasClass('col-dropdown')) { return; }
      $('thead').css('overflow', 'visible');
    },
    /* when the column header dropdown is hidden, go back to the default scroll
     * behavior so that the table can overflow the window width */
    dropdownHideListener: function (bvEvent) {
      if (!this.stickyHeader) { return; }
      const target = $(bvEvent.target);
      if (!target) { return; }
      if (!target.parent().hasClass('col-dropdown')) { return; }
      $('thead').css('overflow', 'scroll');
    },
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
      searchIssued = true;

      const clientCancel = () => {
        if (pendingPromise) {
          pendingPromise.controller.abort(this.$t('sessions.sessions.canceledSearch'));
          pendingPromise = null;
        }

        if (!runNewQuery) {
          this.loading = false;
          if (!this.sessions.data) {
            // show a page error if there is no data on the page
            this.error = this.$t('sessions.sessions.canceledSearch');
          }
          return;
        }

        this.loadData(updateTable);
      };

      if (pendingPromise) {
        ConfigService.cancelEsTask(pendingPromise.cancelId).then((response) => {
          clientCancel();
        }).catch((error) => {
          clientCancel();
        });
      } else if (runNewQuery) {
        this.loadData(updateTable);
      }
    },
    fetchGraphData: function () {
      this.graphData = undefined;
      if (this.shouldIssueQuery()) {
        this.cancelAndLoad(true);
      }
    },
    fetchMapData: function () {
      this.mapData = undefined;
      if (this.shouldIssueQuery()) {
        this.cancelAndLoad(true);
      }
    },
    updateVisualizationsData: function (data) {
      this.mapData = data.mapData;
      this.graphData = data.graphData;
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
        const index = this.stickySessions.indexOf(session);
        if (index >= 0) { this.stickySessions.splice(index, 1); }
      }

      this.$store.commit('setStickySessionsBtn', !!this.stickySessions.length);
    },
    expandSessionDetail: function (session) {
      if (session.expanded) { return; }
      this.toggleSessionDetail(session);
    },
    collapseSessionDetail: function (session) {
      if (!session.expanded) { return; }
      this.toggleSessionDetail(session);
    },
    /**
     * Closes the session detail for a session
     * Triggered by the sticky session component
     * @param {string} sessionId The id of the session to close
     */
    closeSession: function (sessionId) {
      for (const i in this.stickySessions) {
        const session = this.stickySessions[i];
        if (session.id === sessionId) {
          session.expanded = false;
          this.stickySessions.splice(i, 1);
          this.$store.commit('setStickySessionsBtn', !!this.stickySessions.length);
          return;
        }
      }
    },
    /**
     * Closes all the open sessions
     * Triggered by the sticky session component
     */
    closeAllSessions: function () {
      for (const session of this.stickySessions) {
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
    sortBy: function (e, id) {
      // if the column click was a click and hold/drag, don't issue new query
      if (holdingClick) { return; }

      if (this.isSorted(id) >= 0) {
        // the table is already sorted by this element
        if (!e.shiftKey) {
          const item = this.toggleSortOrder(id);
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
        if (!e.shiftKey) {
          // if it's a regular click - remove other sorts and add this one
          this.tableState.order = [[id, 'asc']];
        } else {
          // if it's a shift click - add it to the list
          this.tableState.order.push([id, 'asc']);
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
        const item = this.tableState.order[i];
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
      const sortIndex = this.isSorted(id);
      if (sortIndex > -1) {
        updated = true; // requires a data reload because the sort is different
        // if we are sorting by this column, remove it
        if (this.tableState.order.length === 1) {
          // this column is the only column we are sorting by
          // so reset it to the first sortable field in the visible headers
          let newSort;
          for (let i = 0, len = this.headers.length; i < len; ++i) {
            const header = this.headers[i];
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
     * Debounces the column search input so that it works faster
     * @param {object} e The input event to capture the value of the input
     */
    debounceColQuery: function (e) {
      if (filterFieldsTimeout) { clearTimeout(filterFieldsTimeout); }
      filterFieldsTimeout = setTimeout(() => {
        this.colQuery = e.target.value;
        this.showAllFields = false; // Reset to limited view on new search
        const filtered = this.filterFields(false, true, false);
        this.filteredFields = filtered.fields;
        this.filteredFieldsCount = filtered.count;
      }, 600);
    },
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

      const index = this.isColVisible(id);

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
        this.colConfigError = this.$t('sessions.sessions.nameColumnConfigErr');
        return;
      }

      const data = {
        name: this.newColConfigName,
        columns: this.tableState.visibleHeaders.slice(),
        order: this.tableState.order.slice()
      };

      UserService.createLayout('sessionstable', data).then((response) => {
        data.name = response.name; // update column config name

        this.colConfigs.push(data);

        this.newColConfigName = null;
        this.colConfigError = false;
      }).catch((error) => {
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
      this.loading = true;

      if (index === -1) { // default columns
        this.tableState.visibleHeaders = Utils.getDefaultTableState().visibleHeaders.slice();
        this.tableState.order = JSON.parse(JSON.stringify(Utils.getDefaultTableState().order));
        this.colWidths = {}; // clear out column widths to load defaults
        setTimeout(() => { this.saveColumnWidths(); });
        // reset field widths
        for (const headerId of this.tableState.visibleHeaders) {
          const field = FieldService.getField(headerId);
          if (field) { field.width = defaultColWidths[headerId] || 100; }
        }
      } else {
        this.tableState.visibleHeaders = this.colConfigs[index].columns.slice();
        this.tableState.order = JSON.parse(JSON.stringify(this.colConfigs[index].order));
      }

      this.sorts = this.tableState.order;

      this.saveTableState();

      this.cancelAndLoad(true, true);
    },
    /**
     * Deletes a previously saved custom column configuration
     * @param {string} colName  The name of the column config to remove
     * @param {int} index       The index in the array of the column config to remove
     */
    deleteColumnConfiguration: function (colName, index) {
      UserService.deleteLayout('sessionstable', colName).then((response) => {
        this.colConfigs.splice(index, 1);
        this.colConfigError = false;
      }).catch((error) => {
        this.colConfigError = error.text;
      });
    },
    /**
     * Updates a previously saved custom column configuration
     * @param {string} colName  The name of the column config to update
     * @param {int} index       The index in the array of the column config to update
     */
    updateColumnConfiguration: function (colName, index) {
      const data = {
        name: colName,
        columns: this.tableState.visibleHeaders.slice(),
        order: JSON.parse(JSON.stringify(this.tableState.order))
      };

      UserService.updateLayout('sessionstable', data).then((response) => {
        this.colConfigs[index] = data;
        this.colConfigError = false;
        this.colConfigSuccess = response.text;
        setTimeout(() => { this.colConfigSuccess = ''; }, 5000);
      }).catch((error) => {
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
          field = FieldService.getField(field);
        }

        if (!field) { return -1; }

        if (id === field.dbField || id === field.dbField2 ||
          id === field.exp || id === field.fieldECS) {
          return index;
        }

        index++;
      }

      return -1;
    },
    /**
     * Debounces the column search input so that it works faster
     * @param {object} e The input event to capture the value of the input
     */
    debounceInfoColQuery: function (e) {
      if (filterFieldsTimeout) { clearTimeout(filterFieldsTimeout); }
      filterFieldsTimeout = setTimeout(() => {
        this.colQuery = e.target.value;
        this.showAllInfoFields = false; // Reset to limited view on new search
        const filtered = this.filterFields(false, true, false);
        this.filteredInfoFields = filtered.fields;
        this.filteredInfoFieldsCount = filtered.count;
      }, 600);
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

      const index = this.isInfoVisible(id);

      if (index >= 0) { // it's visible
        // remove it from the info fields list
        this.infoFields.splice(index, 1);
      } else { // it's hidden
        reloadData = true; // requires a data reload
        // add it to the info fields list
        const field = FieldService.getField(id);
        if (field) { this.infoFields.push(field); }
      }
      this.saveInfoFields();

      if (reloadData) { // need data from the server
        this.cancelAndLoad(true, true);
      } else { // have all the data, just need to reload the table
        this.reloadTable();
      }
    },
    /* Saves a custom info field column configuration */
    saveInfoFieldLayout () {
      if (!this.newInfoConfigName) {
        this.infoConfigError = 'You must name your new info field configuration';
        return;
      }

      const data = {
        name: this.newInfoConfigName,
        fields: this.infoFields.map((field) => field.dbField)
      };

      UserService.createLayout('sessionsinfofields', data).then((response) => {
        data.name = response.name; // update info config name because server sanitizes it
        this.infoConfigs.push(data);
        this.newInfoConfigName = null;
        this.infoConfigError = false;
      }).catch((error) => {
        this.infoConfigError = error.text;
      });
    },
    /**
     * Loads a previously saved custom info field column configuration and updates the table
     * @param {int} index The index in the array of the info field config to load
     */
    loadInfoFieldLayout (index) {
      const fieldObjects = [];
      for (const field of this.infoConfigs[index].fields) {
        fieldObjects.push(FieldService.getField(field));
      }
      this.infoFields = fieldObjects;
      this.saveInfoFields();
    },
    /**
     * Deletes a previously saved custom info field column layout
     * @param {string} layoutName  The name of the layout to remove
     * @param {int} index          The index in the array of layouts to remove
     */
    deleteInfoFieldLayout (layoutName, index) {
      UserService.deleteLayout('sessionsinfofields', layoutName).then((response) => {
        this.infoConfigs.splice(index, 1);
        this.infoConfigError = false;
      }).catch((error) => {
        this.infoConfigError = error.text;
      });
    },
    /**
      * Updates a previously saved custom info field layout
      * @param {string} layoutName  The name of the layout to update
      * @param {int} index          The index in the array of layouts to update
      */
    updateInfoFieldLayout (layoutName, index) {
      const data = {
        name: layoutName,
        fields: this.infoFields.slice()
      };

      UserService.updateLayout('sessionsinfofields', data).then((response) => {
        this.infoConfigs[index] = data;
        this.infoConfigError = false;
        this.infoConfigSuccess = response.text;
        setTimeout(() => { this.infoConfigSuccess = ''; }, 5000);
      }).catch((error) => {
        this.infoConfigError = error.text;
      });
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
    saveInfoFields () {
      const infoDBFields = [];
      for (const field of this.infoFields) {
        infoDBFields.push(field.dbField);
      }
      this.user.settings.infoFields = infoDBFields;
      customCols.info.children = this.infoFields;
      UserService.saveSettings(this.user.settings);
    },
    /* Fits the table to the width of the current window size */
    fitTable: function () {
      const windowWidth = window.innerWidth - 20; // account for margins
      const leftoverWidth = windowWidth - this.sumOfColWidths;
      const percentChange = 1 + (leftoverWidth / this.sumOfColWidths);

      for (let i = 0, len = this.headers.length; i < len; ++i) {
        const header = this.headers[i];
        const newWidth = Math.floor(header.width * percentChange);
        header.width = newWidth;
        this.colWidths[header.dbField] = newWidth;
      }

      this.tableWidth = windowWidth;
      this.showFitButton = false;

      this.$refs.sessionsTable.style.width = `${windowWidth}px`;

      this.saveColumnWidths();
      this.toggleStickyHeader();
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
     * Opens a new sessions page with a list of values as the search expression
     * @param {string} dbField  The key to access the data from sessions
     * @param {string} exp      The field to add to the search expression
     */
    pivot: function (dbField, exp) {
      const values = [];
      const existingVals = {}; // save map of existing values for deduping
      for (const session of this.sessions.data) {
        if (session[dbField]) {
          const value = session[dbField];
          if (existingVals[value]) { continue; }
          values.push(value);
          existingVals[value] = true;
        }
      }

      const valueStr = `[${values.join(',')}]`;
      const expression = buildExpression(exp, valueStr, '==');

      const routeData = this.$router.resolve({
        path: '/sessions',
        query: {
          ...this.$route.query,
          expression
        }
      });

      window.open(routeData.href, '_blank');
    },
    /**
     * Adds field == EXISTS! to the search expression
     * @param {string} field  The field name
     * @param {string} op     The relational operator
     */
    fieldExists: function (field, op) {
      const fullExpression = buildExpression(field, 'EXISTS!', op);
      this.$store.commit('addToExpression', { expression: fullExpression });
    },
    /**
     * Filters grouped fields based on a query string
     * @param {boolean} excludeTokens   Whether to exclude token fields
     * @param {boolean} excludeFilename Whether to exclude the filename field
     * @param {boolean} excludeInfo     Whether to exclude the special info "field"
     */
    filterFields: function (excludeTokens, excludeFilename, excludeInfo) {
      const filteredGroupedFields = {};

      let count = 0;
      for (const group in this.groupedFields) {
        const filteredFields = searchFields(
          this.colQuery,
          this.groupedFields[group],
          excludeTokens,
          excludeFilename,
          excludeInfo
        );
        count += filteredFields.length;
        filteredGroupedFields[group] = filteredFields;
      }

      return { count, fields: filteredGroupedFields };
    },

    /* helper functions ---------------------------------------------------- */
    reloadTable: function () {
      // disable resizable columns so it can be initialized after table reloads
      this.destroyColResizable();
      this.$nextTick(() => { this.initializeColResizable(); });
    },
    /* gets all the information to display the table and custom col config dropdown
       widths of columns, table columns and sort order, custom col configs */
    getSessionsConfig: function () {
      UserService.getPageConfig('sessions').then((response) => {
        this.colWidths = response.colWidths;
        this.colConfigs = response.colConfigs;
        this.tableState = response.tableState;
        this.infoConfigs = response.infoConfigs;

        this.$store.commit('setSessionsTableState', this.tableState);
        if (Object.keys(this.tableState).length === 0 ||
          !this.tableState.visibleHeaders || !this.tableState.order) {
          this.tableState = JSON.parse(JSON.stringify(Utils.getDefaultTableState()));
        } else if (this.tableState.visibleHeaders[0] === '') {
          this.tableState.visibleHeaders.shift();
        }

        // update the sort order for the session table query
        this.sorts = this.tableState.order;

        this.setupUserSettings(); // IMPORTANT: kicks off the initial search query!
      }).catch((error) => {
        this.error = error;
        this.loading = false;
      });
    },
    shouldIssueQuery: function () {
      const manualQuery = this.user.settings.manualQuery && JSON.parse(this.user.settings.manualQuery);
      const hasExpression = this.query.expression && this.query.expression.length;
      return searchIssued || !manualQuery || (manualQuery && hasExpression);
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
      if (this.shouldIssueQuery()) {
        this.cancelAndLoad(true);
      } else {
        this.loading = false;
        this.error = 'Now, issue a query!';
      }
    },
    /**
     * Makes a request to the Session Service to get the list of sessions
     * that match the query parameters
     * @param {bool} updateTable Whether the table needs updating
     */
    async loadData (updateTable) {
      if (!Utils.checkClusterSelection(this.query.cluster, this.$store.state.esCluster.availableCluster.active, this).valid) {
        this.sessions.data = undefined;
        this.dataLoading = false;
        pendingPromise = null;
        return;
      }

      this.loading = true;
      this.error = '';

      // save expanded sessions
      const expandedSessions = [];
      for (const session of this.stickySessions) {
        expandedSessions.push(session.id);
      }

      this.sorts = this.tableState.order || JSON.parse(JSON.stringify(Utils.getDefaultTableState().order));

      if (this.viewChanged && this.views) {
        const view = this.views.find(v => v.id === this.query.view);
        if (view && view.sessionsColConfig) {
          this.tableState = JSON.parse(JSON.stringify(view.sessionsColConfig));
          this.saveTableState();
        }

        this.mapHeadersToFields();

        this.updateTable = true;
        this.viewChanged = false;
      } else {
        this.mapHeadersToFields();
      }

      this.query.fields = ['ipProtocol']; // minimum required field

      // set the fields to retrieve from the server for each session
      if (this.headers) {
        for (const field of this.headers) {
          if (field.children) {
            let children = field.children;
            // add user configurable child info fields
            if (field.exp === 'info' && this.infoFields) {
              children = JSON.parse(JSON.stringify(this.infoFields));
            }
            for (const child of children) {
              if (child) {
                this.query.fields.push(child.dbField);
              }
            }
          } else {
            this.query.fields.push(field.dbField);
          }
        }
      }

      // create unique cancel id to make cancel req for corresponding es task
      const cancelId = Utils.createRandomString();
      this.query.cancelId = cancelId;

      try {
        const { controller, fetcher } = SessionsService.get(this.query, true);
        pendingPromise = { controller, cancelId };

        const response = await fetcher; // do the fetch
        if (response.data.error) {
          throw new Error(response.data.error);
        }

        pendingPromise = null;
        this.stickySessions = []; // clear sticky sessions
        this.error = '';
        this.loading = false;
        this.sessions = response;
        this.mapData = response.map;
        this.graphData = response.graph;

        if (updateTable) { this.reloadTable(); }

        if (parseInt(this.$route.query.openAll) === 1) {
          this.openAll();
        } else { // open the previously opened sessions
          for (const sessionId of expandedSessions) {
            for (const session of this.sessions.data) {
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
      } catch (error) {
        pendingPromise = null;
        this.loading = false;
        this.sessions.data = undefined;
        this.error = error.text || String(error);
      }
    },
    /**
     * Saves the table state
     * @param {bool} stopLoading Whether to stop the loading state when promise returns
     */
    saveTableState: function (stopLoading) {
      this.$store.commit('setSessionsTableState', this.tableState);
      UserService.saveState(this.tableState, 'sessionsNew').then(() => {
        if (stopLoading) { this.loading = false; }
      }).catch((error) => { this.error = error; });
    },
    /**
     * Sets up the fields for the column visibility typeahead and column headers
     * by adding custom columns to the visible columns list and table
     */
    setupFields: function () {
      for (const key in customCols) {
        this.fields[key] = customCols[key];
        const children = this.fields[key].children;
        // expand all the children
        for (const c in children) {
          // (replace fieldId with field object)
          if (typeof children[c] !== 'object') {
            children[c] = FieldService.getField(children[c]);
          }
        }
      }

      // group fields map by field group
      // and remove duplicate fields (e.g. 'host.dns' & 'dns.host')
      const existingFieldsLookup = {}; // lookup map of fields in fieldsArray
      this.groupedFields = {};
      for (const f in this.fields) {
        const field = this.fields[f];
        if (!existingFieldsLookup[field.exp]) {
          existingFieldsLookup[field.exp] = field;
          if (!this.groupedFields[field.group]) {
            this.groupedFields[field.group] = [];
          }
          this.groupedFields[field.group].push(field);
        }
      }

      // set the filtered fields in the col config menu
      const filtered = this.filterFields(false, true, false);
      this.filteredFields = filtered.fields;
      this.filteredFieldsCount = filtered.count;
      this.filteredInfoFields = filtered.fields;
      this.filteredInfoFieldsCount = filtered.count;
    },
    /* Maps visible column headers to their corresponding fields */
    mapHeadersToFields: function () {
      this.headers = [];
      this.sumOfColWidths = 85;
      if (!this.colWidths) { this.colWidths = {}; }

      for (const headerId of this.tableState.visibleHeaders) {
        const field = FieldService.getField(headerId);

        if (field) {
          field.width = this.colWidths[headerId] || field.width || 100;
          if (field.dbField === 'info') { // info column is super special
            // reset info field width to default so it can always be recalculated
            // to take up all of the rest of the space that it can
            field.width = defaultColWidths.info;
          } else { // don't account for info column's width because it changes
            this.sumOfColWidths += field.width;
          }
          this.headers.push(field);
        }
      }

      this.sumOfColWidths = Math.round(this.sumOfColWidths);

      this.calculateInfoColumnWidth(defaultColWidths.info);
      this.toggleStickyHeader();
    },
    /* Opens up to 50 session details in the table */
    openAll: function () {
      // opening too many session details at once is bad!
      if (this.sessions.data.length > 50) {
        alert(this.$t('sessions.sessions.tooManySessionsErr'));
      }

      const len = Math.min(this.sessions.data.length, 50);

      for (let i = 0; i < len; ++i) {
        this.expandSessionDetail(this.sessions.data[i]);
      }

      // unset open all for future queries
      if (this.$route.query.openAll) {
        this.$router.replace({
          query: {
            ...this.$route.query,
            openAll: undefined
          }
        });
      }
    },
    closeAll: function () {
      for (const session of this.sessions.data) {
        this.collapseSessionDetail(session);
      }
    },
    /* Initializes column drag and drop */
    initializeColDragDrop: function () {
      if (!this.$refs.draggableColumns) { return; }

      colDragDropInitialized = true;
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

          this.loading = true;

          // update the headers to the new order
          const oldIdx = e.oldIndex - 1;
          const newIdx = e.newIndex - 1;
          const element = this.tableState.visibleHeaders[oldIdx];
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
    initializeColResizable () {
      colResizeInitialized = true;

      cols = document.getElementsByClassName('arkime-col-header');
      table = this.$refs.sessionsTable;

      for (const col of cols) { // listen for grip dragging
        const grip = col.getElementsByClassName('grip')[0];
        if (grip) {
          grip.addEventListener('mousedown', (e) => gripClick(e, col));
        }
      }

      document.addEventListener('mousemove', gripDrag);
      const self = this;
      document.addEventListener('mouseup', (e) => gripUnclick(e, self));

      document.addEventListener('scroll', (e) => docScroll(e, self), true);
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
      document.removeEventListener('scroll', docScroll);

      cols = undefined;
      table = undefined;
      colResizeInitialized = false;
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

      const windowWidth = window.innerWidth - 20; // account for margins

      if (this.tableState.visibleHeaders.indexOf('info') >= 0) {
        const fillWithInfoCol = windowWidth - this.sumOfColWidths;
        let newTableWidth = this.sumOfColWidths;
        for (let i = 0, len = this.headers.length; i < len; ++i) {
          if (this.headers[i].dbField === 'info') {
            const newInfoColWidth = Math.max(fillWithInfoCol, infoColWidth);
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
    /* Toggles the sticky table header */
    toggleStickyHeader: function () {
      // Guard check: ensure refs are available before proceeding
      if (!this.$refs.draggableColumns || !this.$refs.tableHeader || !this.$refs.tableRow0) {
        return;
      }

      const firstTableRow = this.$refs.tableRow0;
      if (this.stickyHeader) {
        // calculate the height of the header row
        const height = this.$refs.draggableColumns.clientHeight + 6;
        const windowWidth = window.innerWidth - 20; // account for margins
        // calculate how much the header row is under or overflowing the window
        const tableHeaderOverflow = windowWidth - this.tableWidth;
        if (tableHeaderOverflow !== 0) { // if it's overflowing the window
          this.tableHeaderOverflow = tableHeaderOverflow;
          // set the right style to the amount of the overflow
          if (tableHeaderOverflow < 0) {
            this.$refs.tableHeader.style.width = `${window.innerWidth}px`;
            this.$refs.draggableColumns.style.width = `${table.clientWidth}px`;
          }
        } else { // otherwise unset it (default = auto, see css)
          this.tableHeaderOverflow = undefined;
          this.$refs.tableHeader.style = undefined;
          this.$refs.draggableColumns.style = undefined;
        }
        if (firstTableRow && firstTableRow.length > 0) { // if there is a table row in the body
          // set the margin top to the height of the header so it renders below it
          firstTableRow[0].style.marginTop = `${height}px`;
        }
        // set the overflow to scroll so that the header can scroll horizontally
        $('thead').css('overflow', 'scroll');
      } else { // if the header is not sticky
        if (firstTableRow && firstTableRow.length > 0) { // and there is a table row in the body
          // unset the top margin because the table header won't overlay it
          firstTableRow[0].style = undefined;
        }
        // set the overflow to visible so that the dropdowns overflow the header
        $('thead').css('overflow', 'visible');
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
      if (args.issueQuery) { this.cancelAndLoad(true); }
    }
  },
  beforeUnmount () {
    holdingClick = false;
    searchIssued = false;
    colDragDropInitialized = false;

    if (pendingPromise) {
      pendingPromise.controller.abort(this.$t('sessions.sessions.closingCancelsSearchErr'));
      pendingPromise = null;
    }

    if (timeout) { clearTimeout(timeout); }

    this.destroyColResizable();

    window.removeEventListener('resize', windowResizeEvent);
  }
};
</script>

<style>
/* column visibility menu -------------------- */
.col-dropdown-menu {
  overflow: auto;
  min-width: 250px;
  max-width: 350px;
  max-height: 300px !important;
}

.sessions-page .col-dropdown > ul {
  overflow-x: hidden;
}

/* needed for grips */
.arkime-col-header {
  position: relative;
}

/* small dropdown buttons in column headers */
.arkime-col-header .dropdown button.btn {
  padding: 0 6px;
}
.arkime-col-header .dropdown-menu {
  max-height: 250px;
  overflow: auto;
}

.arkime-col-header .dropdown:not(.info-vis-menu) {
  visibility: hidden;
  margin-left: -25px;
}

/* clear the box shadow above the sticky column headers */
.sessions-page .sticky-viz .viz-container {
  box-shadow: none !important;
}
</style>

<style scoped>
.sessions-page {
  overflow: hidden;
}

.sessions-content {
  padding-top: 30px;
  margin-top: -10px;
  overflow-x: auto;
  min-height: 500px;
}

/* sessions table styles --------------------- */
table.sessions-table {
  margin-bottom: 20px;
  table-layout: fixed;
}

/* borders for header */
table.sessions-table thead tr th {
  vertical-align: top;
  border-bottom: 2px solid var(--color-gray);
  border-right: 1px dotted var(--color-gray);
}
/* remove right border for first column because it is no resizeable */
table.sessions-table thead tr th:first-child {
  border-right: none;
  padding: 0;
  vertical-align: middle;
}
/* remove scrollbar from table header */
table.sessions-table thead::-webkit-scrollbar {
  display: none;
}

/* table hover */
table.sessions-table tbody tr:not(.session-detail-row):hover,
table.sessions-table tbody tr:not(.session-detail-row):hover td.active {
  background-color: var(--color-tertiary-lightest);
}

/* detail row background color */
table.sessions-table tbody tr.session-detail-row {
  background-color: var(--color-quaternary-lightest) !important;
}

/* condense the table and put values at the top of ceslls */
table.sessions-table tbody tr td {
  padding: 0 2px;
  line-height: 1.42857143;
  vertical-align: top;
}

/* leftmost column is always the same */
table.sessions-table thead tr th.ignore-element,
table.sessions-table tbody tr td.ignore-element {
  width: 85px;
  white-space: nowrap;
}

/* sticky table header ----------------------- */
table.sessions-table.sticky-header > thead {
  left: 0;
  z-index: 2;
  /* need to unset right because sometimes the header overflows the window */
  right: auto;
  position: fixed;
  margin-top: -20px;
  padding-top: 24px;
  /* need x overflow for the table to be able to overflow the window width */
  overflow-x: scroll;
  padding-left: 8px;
  box-shadow: 0 6px 9px -6px black;
  background-color: var(--color-background, white);
}
table.sessions-table.sticky-header > thead > tr {
  display: table;
  /* need x overflow for the table to be able to overflow the window width */
  overflow-x: scroll;
  table-layout: fixed;
}
/* need this to make sure that the body cells are the correct width */
table.sessions-table.sticky-header > tbody > tr {
  display: table;
  table-layout: fixed;
}
/* need this when reloading the page with sticky headers */
table.sessions-table.sticky-header > tbody {
  display: block;
  margin-top: 53px;
}

/* table column headers -------------------- */
.arkime-col-header {
  font-size: .9rem;
}

.arkime-col-header.active {
  color: var(--color-foreground-accent);
}

.arkime-col-header:hover .dropdown {
  visibility: visible;
}

.arkime-col-header .header-text {
  display: inline-block;
  width: calc(100% - 24px);
}

.arkime-col-header .header-sort {
  display: inline-block;
  margin-right: 3px;
  vertical-align: top;
}

.info-col-header .dropdown:not(.info-vis-menu) {
  margin-right: 4px;
}
.info-vis-menu {
  margin-right: 10px;
}
.arkime-col-header:not(:last-child) .info-vis-menu {
  margin-right: 5px;
}

/* table fit button -------------------------- */
div.fit-btn-container {
  top: -18px;
  z-index: 3;
  position: relative;
}
div.fit-btn-container > button.fit-btn,
div.fit-btn-container > button.open-all-btn,
div.fit-btn-container > button.close-all-btn {
  height: 16px;
  line-height: 1;
  position: absolute;
}
div.fit-btn-container > button.fit-btn.fit-btn-right {
  left: 48px;
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
.leave-enter-from, .leave-leave-to {
  z-index: 4;
}
.leave-leave-to {
  transform: translateY(1000px);
  opacity: 0;
}

/* set scroll margin offset for scrolling to sessions */
.sessions-scroll-margin {
  scroll-margin: 115px;
}
/* if there are no toolbars, there is no offset */
.hide-tool-bars .sessions-scroll-margin {
  scroll-margin: 0px;
}
</style>
