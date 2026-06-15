<!--
Copyright Yahoo Inc.
SPDX-License-Identifier: Apache-2.0
-->
<template>
  <page-layout
    ref="pageLayout"
    class="sessions-page">
    <template #chrome>
      <ArkimeCollapsible>
        <div class="page-toolbar">
          <!-- search navbar -->
          <arkime-search
            :fields="headers"
            :open-sessions="stickySessions"
            :num-visible-sessions="query.length"
            :num-matching-sessions="sessions.recordsFiltered"
            :start="query.start"
            @change-search="cancelAndLoad(true)"
            @set-view="loadNewView"
            @set-columns="loadColumns" /> <!-- /search navbar -->

          <!-- paging navbar -->
          <div class="d-flex justify-start align-center paging-navbar">
            <arkime-paging
              :records-total="sessions.recordsTotal"
              :records-filtered="sessions.recordsFiltered"
              @change-paging="changePaging" />
          </div> <!-- /paging navbar -->
        </div>
      </ArkimeCollapsible>
      <!-- pinned visualizations land here (teleported from below) -->
      <div id="viz-pin-anchor" />
    </template>

    <!-- visualizations: pinned = chrome row above the scroll container,
         unpinned = scrolls away with the content -->
    <teleport
      defer
      to="#viz-pin-anchor"
      :disabled="!stickyViz">
      <arkime-visualizations
        v-if="graphData && showToolBars"
        :primary="true"
        :map-data="mapData"
        :graph-data="graphData"
        @fetch-map-data="fetchMapData"
        @spanning-change="loadData"
        :timeline-data-filters="timelineDataFilters" />
    </teleport> <!-- /visualizations -->

    <div class="sessions-content ms-2">
      <!-- table view -->
      <div>
        <!-- sessions results -->
        <table
          class="sessions-table"
          :style="`width:${tableWidth}px`"
          :class="{'sticky-header':stickyViz}"
          ref="sessionsTable"
          id="sessionsTable">
          <thead>
            <tr ref="draggableColumns">
              <!-- table options: single dropdown collapsing five separate
                   controls (open/close all, fit, toggle columns, save
                   layouts) into one menu with side-popping submenus -->
              <th
                class="ignore-element sessions-options-cell"
                style="width:85px;">
                <v-menu
                  :close-on-content-click="false"
                  location="bottom start">
                  <template #activator="{ props: activatorProps }">
                    <v-btn
                      v-bind="activatorProps"
                      size="small"
                      variant="text"
                      icon
                      class="sessions-options-btn ms-1"
                      :aria-label="$t('sessions.sessions.tableOptions')">
                      <v-icon
                        icon="mdi-table-cog"
                        size="large" />
                      <v-tooltip
                        activator="parent"
                        location="right">
                        {{ $t('sessions.sessions.tableOptions') }}
                      </v-tooltip>
                    </v-btn>
                  </template>
                  <v-list
                    density="compact"
                    min-width="220"
                    class="sessions-options-menu">
                    <!-- direct actions -->
                    <v-list-item
                      v-if="sessions.data && sessions.data.length <= 50"
                      prepend-icon="mdi-plus-circle"
                      @click="openAll">
                      {{ $t('sessions.sessions.openAll') }}
                    </v-list-item>
                    <v-list-item
                      v-if="!loading && stickySessions.length > 0"
                      prepend-icon="mdi-close-circle"
                      @click="closeAll">
                      {{ $t('sessions.sessions.closeAll') }}
                    </v-list-item>
                    <v-list-item
                      v-if="showFitButton && !loading"
                      prepend-icon="mdi-arrow-expand-horizontal"
                      @click="fitTable">
                      {{ $t('sessions.sessions.fitTable') }}
                    </v-list-item>
                    <v-divider />

                    <!-- submenu: toggle column visibility -->
                    <v-list-item
                      prepend-icon="mdi-view-column"
                      append-icon="mdi-chevron-right">
                      <v-list-item-title>{{ $t('sessions.sessions.toggleColumns') }}</v-list-item-title>
                      <v-menu
                        activator="parent"
                        :close-on-content-click="false"
                        location="end"
                        open-on-click
                        :open-on-hover="false">
                        <FieldSelectDropdown
                          body-only
                          :selected-fields="tableState.visibleHeaders"
                          :tooltip-text="$t('sessions.sessions.toggleColumns')"
                          :search-placeholder="$t('sessions.sessions.searchColumns')"
                          :exclude-filename="true"
                          :max-visible-fields="maxVisibleFields"
                          field-id-key="dbField"
                          @toggle="toggleColVis" />
                      </v-menu>
                    </v-list-item>

                    <!-- save current columns over the loaded config -->
                    <v-list-item
                      v-if="loadedColConfig"
                      prepend-icon="mdi-content-save"
                      @click="updateColumnConfiguration(loadedColConfig)">
                      {{ $t('sessions.sessions.saveConfig', { name: loadedColConfig }) }}
                      <v-tooltip
                        activator="parent"
                        location="end">
                        {{ $t('sessions.sessions.customColumnUpdate') }}
                      </v-tooltip>
                    </v-list-item>

                    <!-- submenu: save current columns as a new config -->
                    <v-list-item
                      prepend-icon="mdi-content-save-plus"
                      append-icon="mdi-chevron-right">
                      <v-list-item-title>{{ $t('sessions.sessions.saveConfigAs') }}</v-list-item-title>
                      <v-menu
                        activator="parent"
                        :close-on-content-click="false"
                        location="end"
                        open-on-click
                        :open-on-hover="false">
                        <v-list
                          density="compact"
                          class="col-config-list">
                          <div class="px-2 py-1">
                            <div class="arkime-input-group arkime-input-group--fluid">
                              <input
                                autofocus
                                @click.stop
                                maxlength="30"
                                type="text"
                                class="arkime-input-control"
                                v-model="newColConfigName"
                                :placeholder="$t('sessions.sessions.customColumnName')"
                                @keydown.enter="saveColumnConfiguration">
                              <v-btn
                                :aria-label="$t('common.save')"
                                variant="flat"
                                size="small"
                                density="comfortable"
                                icon
                                class="arkime-input-append-btn"
                                :style="secondaryBtnStyle"
                                :disabled="!newColConfigName"
                                @click="saveColumnConfiguration">
                                <v-icon icon="mdi-content-save" />
                              </v-btn>
                            </div>
                          </div>
                        </v-list>
                        <v-alert
                          v-if="colConfigError"
                          density="compact"
                          variant="tonal"
                          type="error"
                          class="ma-1">
                          {{ colConfigError }}
                        </v-alert>
                      </v-menu>
                    </v-list-item>

                    <!-- submenu: load a saved config (filterable) -->
                    <v-list-item
                      prepend-icon="mdi-folder-open"
                      append-icon="mdi-chevron-right">
                      <v-list-item-title>{{ $t('sessions.sessions.loadConfig') }}</v-list-item-title>
                      <v-menu
                        activator="parent"
                        :close-on-content-click="false"
                        location="end"
                        open-on-click
                        :open-on-hover="false">
                        <v-list
                          density="compact"
                          class="col-config-list">
                          <div
                            v-if="colConfigs.length"
                            class="px-2 py-1">
                            <div class="arkime-input-group arkime-input-group--fluid">
                              <input
                                autofocus
                                @click.stop
                                type="text"
                                class="arkime-input-control"
                                v-model="colConfigQuery"
                                :placeholder="$t('sessions.sessions.filterConfigs')">
                            </div>
                          </div>
                          <v-divider v-if="colConfigs.length" />
                          <v-list-item
                            key="col-config-default"
                            @click.stop.prevent="loadColumnConfiguration(-1)">
                            {{ $t('sessions.sessions.arkimeDefault') }}
                            <v-tooltip
                              activator="parent"
                              location="end">
                              {{ $t('sessions.sessions.customColumnReset') }}
                            </v-tooltip>
                          </v-list-item>
                          <v-list-item
                            v-if="colConfigQuery && !filteredColConfigs.length"
                            disabled>
                            {{ $t('sessions.sessions.noConfigsMatch') }}
                          </v-list-item>
                          <v-list-item
                            v-for="config in filteredColConfigs"
                            :key="config.name"
                            :active="config.name === loadedColConfig"
                            @click.self.stop.prevent="loadColumnConfiguration(config)">
                            <div
                              class="d-flex align-center w-100"
                              @click.self="loadColumnConfiguration(config)">
                              <span
                                class="flex-grow-1"
                                @click="loadColumnConfiguration(config)">
                                {{ config.name }}
                              </span>
                              <v-btn
                                :aria-label="$t('common.delete')"
                                color="error"
                                variant="flat"
                                size="small"
                                density="comfortable"
                                icon
                                class="ms-1"
                                @click.stop.prevent="deleteColumnConfiguration(config)">
                                <v-icon icon="mdi-trash-can-outline" />
                              </v-btn>
                            </div>
                          </v-list-item>
                        </v-list>
                      </v-menu>
                    </v-list-item>
                  </v-list>
                </v-menu>
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
                    class="cursor-pointer info-col-header-inner">
                    <!-- info column: single dropdown collapsing the field-
                         visibility menu and the save-layouts menu into one
                         (mirrors the table-options menu in the first cell).
                         Rendered before the label so the cog sits to the
                         left of "Info". -->
                    <v-menu
                      :close-on-content-click="false"
                      location="bottom end"
                      class="info-col-actions">
                      <template #activator="{ props: activatorProps }">
                        <v-btn
                          v-bind="activatorProps"
                          variant="text"
                          size="small"
                          density="compact"
                          icon
                          class="info-vis-menu col-dropdown"
                          :aria-label="$t('sessions.sessions.toggleInfoFields')">
                          <v-icon icon="mdi-table-cog" />
                          <v-tooltip
                            activator="parent"
                            location="right">
                            {{ $t('sessions.sessions.toggleInfoFields') }}
                          </v-tooltip>
                        </v-btn>
                      </template>
                      <v-list
                        density="compact"
                        min-width="220"
                        class="sessions-options-menu">
                        <!-- submenu: toggle info field visibility -->
                        <v-list-item
                          prepend-icon="mdi-view-list"
                          append-icon="mdi-chevron-right">
                          <v-list-item-title>{{ $t('sessions.sessions.toggleInfoFields') }}</v-list-item-title>
                          <v-menu
                            activator="parent"
                            :close-on-content-click="false"
                            location="end"
                            open-on-click
                            :open-on-hover="false"
                            @update:model-value="(open) => {
                              infoFieldVisMenuOpen = open;
                              if (!open) showAllInfoFields = false;
                            }">
                            <v-list
                              density="compact"
                              class="col-dropdown-menu">
                              <div class="px-2 py-1">
                                <div class="arkime-input-group arkime-input-group--fluid">
                                  <input
                                    autofocus
                                    v-model="colQuery"
                                    @input="debounceInfoColQuery"
                                    @click.stop
                                    type="text"
                                    class="arkime-input-control"
                                    :placeholder="$t('common.searchForFields')">
                                </div>
                              </div>
                              <v-divider />
                              <template v-if="infoFieldVisMenuOpen">
                                <v-list-item
                                  v-if="!filteredInfoFieldsCount"
                                  disabled>
                                  {{ $t('sessions.sessions.noFieldsMatch') }}
                                </v-list-item>
                                <template
                                  v-for="(group, key) in visibleFilteredInfoFields"
                                  :key="key">
                                  <v-list-subheader
                                    v-if="group.length"
                                    class="group-header text-uppercase">
                                    {{ key }}
                                  </v-list-subheader>
                                  <template
                                    v-for="(field, k) in group"
                                    :key="key + k + 'infoitem'">
                                    <v-list-item
                                      :data-tip-id="key + k + 'infoitem'"
                                      :active="isInfoVisible(field.dbField) >= 0"
                                      class="field-item"
                                      @click.prevent.stop="toggleInfoVis(field.dbField)">
                                      {{ field.friendlyName }}
                                      <small>({{ field.exp }})</small>
                                      <v-tooltip
                                        v-if="field.help"
                                        :activator="`[data-tip-id='${key + k + 'infoitem'}']`"
                                        location="end">
                                        {{ field.help }}
                                      </v-tooltip>
                                    </v-list-item>
                                  </template>
                                </template>
                                <v-list-item
                                  v-if="hasMoreInfoFields"
                                  class="text-center"
                                  @click.stop="showAllInfoFields = true">
                                  <strong>Show {{ $t('sessions.sessions.showMoreFields', filteredInfoFieldsCount - maxVisibleFields) }}</strong>
                                </v-list-item>
                              </template>
                            </v-list>
                          </v-menu>
                        </v-list-item>

                        <!-- save current info fields over the loaded config -->
                        <v-list-item
                          v-if="loadedInfoConfig"
                          prepend-icon="mdi-content-save"
                          @click="updateInfoFieldLayout(loadedInfoConfig)">
                          {{ $t('sessions.sessions.saveConfig', { name: loadedInfoConfig }) }}
                          <v-tooltip
                            activator="parent"
                            location="end">
                            {{ $t('sessions.sessions.customInfoUpdate') }}
                          </v-tooltip>
                        </v-list-item>

                        <!-- submenu: save current info fields as a new config -->
                        <v-list-item
                          prepend-icon="mdi-content-save-plus"
                          append-icon="mdi-chevron-right">
                          <v-list-item-title>{{ $t('sessions.sessions.saveConfigAs') }}</v-list-item-title>
                          <v-menu
                            activator="parent"
                            :close-on-content-click="false"
                            location="end"
                            open-on-click
                            :open-on-hover="false">
                            <v-list
                              density="compact"
                              class="col-dropdown-menu">
                              <div class="px-2 py-1">
                                <div class="arkime-input-group arkime-input-group--fluid">
                                  <input
                                    autofocus
                                    @click.stop
                                    maxlength="30"
                                    type="text"
                                    class="arkime-input-control"
                                    v-model="newInfoConfigName"
                                    :placeholder="$t('sessions.sessions.customInfoName')"
                                    @keydown.enter="saveInfoFieldLayout">
                                  <v-btn
                                    :aria-label="$t('common.save')"
                                    variant="flat"
                                    size="small"
                                    density="comfortable"
                                    icon
                                    class="arkime-input-append-btn"
                                    :style="secondaryBtnStyle"
                                    :disabled="!newInfoConfigName"
                                    @click="saveInfoFieldLayout">
                                    <v-icon icon="mdi-content-save" />
                                  </v-btn>
                                </div>
                              </div>
                            </v-list>
                            <v-alert
                              v-if="infoConfigError"
                              density="compact"
                              variant="tonal"
                              type="error"
                              class="ma-1">
                              {{ infoConfigError }}
                            </v-alert>
                          </v-menu>
                        </v-list-item>

                        <!-- submenu: load a saved info config (filterable) -->
                        <v-list-item
                          prepend-icon="mdi-folder-open"
                          append-icon="mdi-chevron-right">
                          <v-list-item-title>{{ $t('sessions.sessions.loadConfig') }}</v-list-item-title>
                          <v-menu
                            activator="parent"
                            :close-on-content-click="false"
                            location="end"
                            open-on-click
                            :open-on-hover="false">
                            <v-list
                              density="compact"
                              class="col-dropdown-menu">
                              <div
                                v-if="infoConfigs.length"
                                class="px-2 py-1">
                                <div class="arkime-input-group arkime-input-group--fluid">
                                  <input
                                    autofocus
                                    @click.stop
                                    type="text"
                                    class="arkime-input-control"
                                    v-model="infoConfigQuery"
                                    :placeholder="$t('sessions.sessions.filterConfigs')">
                                </div>
                              </div>
                              <v-divider v-if="infoConfigs.length" />
                              <v-list-item
                                key="infodefault"
                                data-tip-id="infodefault"
                                @click.stop.prevent="resetInfoVisibility">
                                {{ $t('sessions.sessions.arkimeDefault') }}
                                <v-tooltip
                                  activator="parent"
                                  location="end">
                                  {{ $t('sessions.sessions.customInfoReset') }}
                                </v-tooltip>
                              </v-list-item>
                              <v-list-item
                                v-if="infoConfigQuery && !filteredInfoConfigs.length"
                                disabled>
                                {{ $t('sessions.sessions.noConfigsMatch') }}
                              </v-list-item>
                              <v-list-item
                                v-for="config in filteredInfoConfigs"
                                :key="config.name"
                                :active="config.name === loadedInfoConfig"
                                @click.self.stop.prevent="loadInfoFieldLayout(config)">
                                <div
                                  class="d-flex align-center w-100"
                                  @click.self="loadInfoFieldLayout(config)">
                                  <span
                                    class="flex-grow-1"
                                    @click="loadInfoFieldLayout(config)">
                                    {{ config.name }}
                                  </span>
                                  <v-btn
                                    :aria-label="$t('common.delete')"
                                    color="error"
                                    variant="flat"
                                    size="small"
                                    density="comfortable"
                                    icon
                                    class="ms-1"
                                    @click.stop.prevent="deleteInfoFieldLayout(config)">
                                    <v-icon icon="mdi-trash-can-outline" />
                                  </v-btn>
                                </div>
                              </v-list-item>
                            </v-list>
                          </v-menu>
                        </v-list-item>
                      </v-list>
                    </v-menu> <!-- /info column options menu -->
                    {{ header.friendlyName }}
                  </span> <!-- /non-sortable column -->
                  <!-- column dropdown menu -->
                  <v-menu location="bottom end">
                    <template #activator="{ props: activatorProps }">
                      <v-btn
                        v-bind="activatorProps"
                        variant="tonal"
                        size="small"
                        density="comfortable"
                        icon
                        color="primary"
                        :aria-label="$t('sessions.columnActions', 'Column actions')"
                        class="float-right col-dropdown col-context-trigger">
                        <v-icon icon="mdi-menu-down" />
                      </v-btn>
                    </template>
                    <v-list
                      density="compact"
                      class="col-dropdown-menu">
                      <v-list-item
                        @click="toggleColVis(header.dbField, header.sortBy)">
                        {{ $t('sessions.hideColumn') }}
                      </v-list-item>
                      <!-- per-field action group(s): one block per target.
                           Single-field columns get [header]; multi-field
                           columns get header.children. 'seconds' columns
                           opt out entirely. -->
                      <template
                        v-for="(target, idx) in columnActionTargets(header)"
                        :key="`col-actions-${idx}`">
                        <v-divider />
                        <v-list-item
                          @click="exportUnique(target.rawField || target.exp, 0)">
                          {{ $t('sessions.exportUnique', {name: target.friendlyName}) }}
                        </v-list-item>
                        <v-list-item
                          @click="exportUnique(target.rawField || target.exp, 1)">
                          {{ $t('sessions.exportUniqueCounts', {name: target.friendlyName}) }}
                        </v-list-item>
                        <template v-if="target.portField">
                          <v-list-item
                            @click="exportUnique(target.rawField || target.exp + ':' + target.portField, 0)">
                            {{ $t('sessions.exportUniquePort', {name: target.friendlyName}) }}
                          </v-list-item>
                          <v-list-item
                            @click="exportUnique(target.rawField || target.exp + ':' + target.portField, 1)">
                            {{ $t('sessions.exportUniquePortCounts', {name: target.friendlyName}) }}
                          </v-list-item>
                        </template>
                        <v-list-item
                          @click="openSpiGraph(target.dbField)">
                          {{ $t('sessions.openSpiGraph', {name: target.friendlyName}) }}
                        </v-list-item>
                        <v-list-item
                          @click="fieldExists(target.exp, '==')">
                          {{ $t('sessions.addExists', {name: target.friendlyName}) }}
                        </v-list-item>
                        <v-list-item
                          @click="pivot(target.dbField, target.exp)">
                          {{ $t('sessions.pivotOn', {name: target.friendlyName}) }}
                        </v-list-item>
                        <!-- field actions: separator only for single-field
                             columns (multi-field already gets dividers
                             between children) -->
                        <field-actions
                          :expr="target.exp"
                          :separator="!header.children" />
                      </template>
                    </v-list>
                  </v-menu> <!-- /column dropdown menu -->
                  <!-- sortable column -->
                  <span
                    v-if="(header.exp || header.sortBy) && !header.unsortable"
                    @mousedown="mouseDown"
                    @mouseup="mouseUp"
                    @click="sortBy($event, header.sortBy || header.dbField)"
                    class="cursor-pointer">
                    <div class="header-sort">
                      <v-icon
                        icon="mdi-chevron-up"
                        size="x-small"
                        v-if="isSorted(header.sortBy || header.dbField) >= 0 && getSortOrder(header.sortBy || header.dbField) === 'asc'" />
                      <v-icon
                        icon="mdi-chevron-down"
                        size="x-small"
                        v-if="isSorted(header.sortBy || header.dbField) >= 0 && getSortOrder(header.sortBy || header.dbField) === 'desc'" />
                    </div>
                    <div class="header-text">
                      {{ header.friendlyName }}
                      <v-tooltip
                        activator="parent"
                        location="top"
                        open-delay="500">
                        {{ header.friendlyName }}
                      </v-tooltip>
                    </div>
                  </span> <!-- /sortable column -->
                </th> <!-- /table headers -->
              </template>
            </tr>
          </thead>

          <tbody class="small">
            <!-- session + detail -->
            <template
              v-for="session of sessions.data"
              :key="session.id">
              <tr
                class="sessions-scroll-margin"
                :id="`session${session.id}`">
                <!-- toggle button and ip protocol -->
                <td class="ignore-element">
                  <div class="d-flex align-center">
                    <toggle-btn
                      class="me-1"
                      :opened="session.expanded"
                      @toggle="toggleSessionDetail(session)" />
                    <span v-if="session.ipProtocol === 0">
                      not-ip
                    </span>
                    <arkime-session-field
                      v-else
                      :field="{dbField:'ipProtocol', exp:'ip.protocol', type:'lotermfield', group:'general', transform:'ipProtocolLookup'}"
                      :session="session"
                      :expr="'ip.protocol'"
                      :value="session.ipProtocol"
                      :pull-left="true"
                      :parse="true" />
                  </div>
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
                        <v-icon
                          icon="mdi-loading"
                          class="mdi-spin me-2" />
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

    <!-- save/load config feedback -->
    <v-snackbar
      v-model="configSnackbar.open"
      location="bottom"
      :color="configSnackbar.color"
      :timeout="5000">
      {{ configSnackbar.text }}
    </v-snackbar>

    <template #overlay>
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
    </template>
  </page-layout>
</template>

<script>
// import services
import FieldService from '../search/FieldService';
import SessionsService from './SessionsService';
import UserService from '../users/UserService';
import ConfigService from '../utils/ConfigService';
import Utils from '../utils/utils';
// import components
import ArkimeSearch from '../search/Search.vue';
import customCols from './customCols.json';
import ArkimePaging from '@common/Pagination.vue';
import ToggleBtn from '@common/ToggleBtn.vue';
import ArkimeError from '../utils/Error.vue';
import ArkimeLoading from '../utils/Loading.vue';
import ArkimeNoResults from '../utils/NoResults.vue';
import ArkimeSessionDetail from './SessionDetail.vue';
import ArkimeCollapsible from '../utils/CollapsibleWrapper.vue';
import PageLayout from '../utils/PageLayout.vue';
import ArkimeVisualizations from '../visualizations/Visualizations.vue';
import ArkimeStickySessions from './StickySessions.vue';
import FieldActions from './FieldActions.vue';
import FieldSelectDropdown from '../utils/FieldSelectDropdown.vue';
// import utils
import { searchFields, buildExpression } from '@common/vueFilters.js';
import { resolveMessage } from '@common/resolveI18nMessage';
import { attachTableGrips } from '@common/composables/useColumnResize.js';
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

const MIN_COL_WIDTH = 70;

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
    PageLayout,
    FieldActions,
    FieldSelectDropdown
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
      colConfigQuery: '', // filter for saved column configs in the load menu
      headers: [],
      graphData: undefined,
      mapData: undefined,
      colQuery: '', // query for columns to toggle visibility
      newColConfigName: '', // name of new custom column config
      viewChanged: false,
      infoFields: customCols.info.children,
      colVisMenuOpen: false,
      infoFieldVisMenuOpen: false,
      showFitButton: false,
      tableWidth: window.innerWidth - 20, // account for margins
      filteredFields: [],
      filteredFieldsCount: 0,
      filteredInfoFields: [],
      filteredInfoFieldsCount: 0,
      infoConfigs: [],
      newInfoConfigName: '',
      infoConfigError: '',
      infoConfigQuery: '', // filter for saved info configs in the load menu
      configSnackbar: { open: false, text: '', color: 'success' },
      maxVisibleFields: 50, // limit initial field rendering for performance
      showAllFields: false,
      showAllInfoFields: false,
      tableState: Utils.getDefaultTableState(),
      // Arkime theme-color v-btn styles. Vuetify :color can't take CSS vars.
      primaryBtnStyle: {
        backgroundColor: 'rgb(var(--v-theme-primary))',
        color: 'rgb(var(--v-theme-button-fg))'
      },
      secondaryBtnStyle: {
        backgroundColor: 'rgb(var(--v-theme-secondary))',
        color: 'rgb(var(--v-theme-button-fg))'
      },
      tertiaryBtnStyle: {
        backgroundColor: 'rgb(var(--v-theme-tertiary))',
        color: 'rgb(var(--v-theme-button-fg))'
      },
      quaternaryBtnStyle: {
        backgroundColor: 'rgb(var(--v-theme-quaternary))',
        color: 'rgb(var(--v-theme-button-fg))'
      }
    };
  },
  created: function () {
    this.getSessionsConfig(); // IMPORTANT: kicks off the initial search query!
    ConfigService.getFieldActions();

    // watch for window resizing and update the info column width
    // this is only registered when the user has not set widths for any
    // columns && the info column is visible
    windowResizeEvent = () => {
      // Surface the Fit Table button when the viewport shrinks past the content.
      // Cheap; runs every resize event so the affordance appears immediately.
      if (Math.abs((this.tableWidth || 0) - this.availableTableWidth()) > 15) {
        this.showFitButton = true;
      }
      if (resizeTimeout) { clearTimeout(resizeTimeout); }
      resizeTimeout = setTimeout(() => {
        this.mapHeadersToFields();
      }, 500);
    };

    window.addEventListener('resize', windowResizeEvent, { passive: true });

    UserService.getState('sessionDetailDLWidth').then((response) => {
      this.$store.commit('setSessionDetailDLWidth', response?.width ?? 160);
    });
  },
  computed: {
    /* name of the currently loaded column config (if it still exists) */
    loadedColConfig: function () {
      const configName = this.tableState.colConfigName;
      return configName && this.colConfigs.some(c => c.name === configName) ? configName : '';
    },
    /* name of the currently loaded info field config (if it still exists) */
    loadedInfoConfig: function () {
      const configName = this.user?.settings?.infoFieldsConfigName;
      return configName && this.infoConfigs.some(c => c.name === configName) ? configName : '';
    },
    filteredColConfigs: function () {
      if (!this.colConfigQuery) { return this.colConfigs; }
      const query = this.colConfigQuery.toLowerCase();
      return this.colConfigs.filter(c => c.name.toLowerCase().includes(query));
    },
    filteredInfoConfigs: function () {
      if (!this.infoConfigQuery) { return this.infoConfigs; }
      const query = this.infoConfigQuery.toLowerCase();
      return this.infoConfigs.filter(c => c.name.toLowerCase().includes(query));
    },
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
      return { width: `${this.tableWidth}px` };
    },
    showToolBars: function () {
      return this.$store.state.showToolBars;
    },
    stickyViz: function () {
      return this.$store.state.stickyViz;
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
      // pin toggle teleports the viz between chrome and scroll content;
      // charts/map cache geometry, so nudge their resize handling
      this.$nextTick(() => window.dispatchEvent(new Event('resize')));
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
    /* Width available to the table inside the page scroll container */
    availableTableWidth: function () {
      const scrollEl = this.$refs.pageLayout?.scrollEl;
      if (scrollEl && scrollEl.clientWidth) {
        return scrollEl.clientWidth - 20; // account for margins
      }
      return window.innerWidth - 20; // pre-mount fallback
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

      // keep info column pinned as the last visible column
      this.normalizeInfoColumnLast();

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

        // the new config is now the loaded one
        this.tableState.colConfigName = data.name;
        this.saveTableState();
        this.showConfigSnackbar(resolveMessage(response, this.$t));
      }).catch((error) => {
        this.colConfigError = resolveMessage(error, this.$t);
      });
    },
    /**
     * Loads a previously saved custom column configuration and
     * reloads table and table data
     * @param {object|int} config The column config to load (-1 loads the default columns)
     */
    loadColumnConfiguration: function (config) {
      this.loading = true;

      if (config === -1) { // default columns
        this.tableState.visibleHeaders = Utils.getDefaultTableState().visibleHeaders.slice();
        this.tableState.order = JSON.parse(JSON.stringify(Utils.getDefaultTableState().order));
        delete this.tableState.colConfigName;
        this.colWidths = {}; // clear out column widths to load defaults
        setTimeout(() => { this.saveColumnWidths(); });
        // reset field widths
        for (const headerId of this.tableState.visibleHeaders) {
          const field = FieldService.getField(headerId);
          if (field) { field.width = defaultColWidths[headerId] || 100; }
        }
      } else {
        this.tableState.visibleHeaders = config.columns.slice();
        this.tableState.order = JSON.parse(JSON.stringify(config.order));
        this.tableState.colConfigName = config.name;
      }

      // keep info column pinned as the last visible column
      this.normalizeInfoColumnLast();

      this.sorts = this.tableState.order;

      this.saveTableState();

      this.cancelAndLoad(true, true);
    },
    /**
     * Deletes a previously saved custom column configuration
     * @param {object} config The column config to remove
     */
    deleteColumnConfiguration: function (config) {
      UserService.deleteLayout('sessionstable', config.name).then((response) => {
        const index = this.colConfigs.indexOf(config);
        if (index > -1) { this.colConfigs.splice(index, 1); }
        if (this.tableState.colConfigName === config.name) {
          delete this.tableState.colConfigName;
          this.saveTableState();
        }
      }).catch((error) => {
        this.showConfigSnackbar(resolveMessage(error, this.$t), 'error');
      });
    },
    /**
     * Updates a previously saved custom column configuration
     * with the currently visible columns
     * @param {string} colName The name of the column config to update
     */
    updateColumnConfiguration: function (colName) {
      const index = this.colConfigs.findIndex(c => c.name === colName);
      if (index === -1) { return; }

      const data = {
        name: colName,
        columns: this.tableState.visibleHeaders.slice(),
        order: JSON.parse(JSON.stringify(this.tableState.order))
      };

      UserService.updateLayout('sessionstable', data).then((response) => {
        this.colConfigs[index] = data;
        this.showConfigSnackbar(resolveMessage(response, this.$t));
      }).catch((error) => {
        this.showConfigSnackbar(resolveMessage(error, this.$t), 'error');
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
        this.infoConfigError = this.$t('sessions.sessions.nameInfoConfigErr');
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

        // the new config is now the loaded one
        this.user.settings.infoFieldsConfigName = data.name;
        UserService.saveSettings(this.user.settings);
        this.showConfigSnackbar(resolveMessage(response, this.$t));
      }).catch((error) => {
        this.infoConfigError = resolveMessage(error, this.$t);
      });
    },
    /**
     * Loads a previously saved custom info field column configuration and updates the table
     * @param {object} config The info field config to load
     */
    loadInfoFieldLayout (config) {
      const fieldObjects = [];
      for (const field of config.fields) {
        fieldObjects.push(FieldService.getField(field));
      }
      this.infoFields = fieldObjects;
      this.user.settings.infoFieldsConfigName = config.name;
      this.saveInfoFields();
    },
    /**
     * Deletes a previously saved custom info field column layout
     * @param {object} config The info field config to remove
     */
    deleteInfoFieldLayout (config) {
      UserService.deleteLayout('sessionsinfofields', config.name).then((response) => {
        const index = this.infoConfigs.indexOf(config);
        if (index > -1) { this.infoConfigs.splice(index, 1); }
        if (this.user.settings.infoFieldsConfigName === config.name) {
          this.user.settings.infoFieldsConfigName = undefined;
          UserService.saveSettings(this.user.settings);
        }
      }).catch((error) => {
        this.showConfigSnackbar(resolveMessage(error, this.$t), 'error');
      });
    },
    /**
      * Updates a previously saved custom info field layout
      * with the currently visible info fields
      * @param {string} layoutName The name of the layout to update
      */
    updateInfoFieldLayout (layoutName) {
      const index = this.infoConfigs.findIndex(c => c.name === layoutName);
      if (index === -1) { return; }

      const data = {
        name: layoutName,
        fields: this.infoFields.map((field) => field.dbField)
      };

      UserService.updateLayout('sessionsinfofields', data).then((response) => {
        this.infoConfigs[index] = data;
        this.showConfigSnackbar(resolveMessage(response, this.$t));
      }).catch((error) => {
        this.showConfigSnackbar(resolveMessage(error, this.$t), 'error');
      });
    },
    /* Resets the visible fields in the info column to the default */
    resetInfoVisibility: function () {
      this.infoFields = defaultInfoFields;
      customCols.info.children = defaultInfoFields;
      this.user.settings.infoFields = undefined;
      this.user.settings.infoFieldsConfigName = undefined;

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
    /* Fits the table to the width of the page scroll container */
    fitTable: function () {
      const targetWidth = this.availableTableWidth();
      const leftoverWidth = targetWidth - this.sumOfColWidths;
      const percentChange = 1 + (leftoverWidth / this.sumOfColWidths);

      for (let i = 0, len = this.headers.length; i < len; ++i) {
        const header = this.headers[i];
        // clamp to min so narrow viewports don't push columns past the grip floor
        const newWidth = Math.max(MIN_COL_WIDTH, Math.floor(header.width * percentChange));
        header.width = newWidth;
        this.colWidths[header.dbField] = newWidth;
      }

      this.tableWidth = targetWidth;
      this.showFitButton = false;

      this.$refs.sessionsTable.style.width = `${targetWidth}px`;

      this.saveColumnWidths();
    },
    /**
     * Returns the list of field targets a column-context menu should render
     * action items for. Single-field columns yield `[header]`; multi-field
     * columns yield `header.children` (filtered for falsy entries).
     * 'seconds'-typed columns return [] (no actions apply).
     */
    columnActionTargets: function (header) {
      if (header.type === 'seconds') return [];
      if (header.children) return header.children.filter(Boolean);
      return [header];
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
    /* Shows transient feedback for save/load config actions */
    showConfigSnackbar: function (text, color = 'success') {
      this.configSnackbar = { open: true, text, color };
    },
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

        // info column auto-fills slack space and has no right-edge resize
        // grip, so force it to always be the last visible column (it would
        // otherwise leave a non-resizable seam in the middle of the table)
        this.normalizeInfoColumnLast();

        // update the sort order for the session table query
        this.sorts = this.tableState.order;

        this.setupUserSettings(); // IMPORTANT: kicks off the initial search query!
      }).catch((error) => {
        this.error = error;
        this.loading = false;
      });
    },
    shouldIssueQuery: function () {
      let manualQuery = this.user.settings.manualQuery;
      if (typeof manualQuery === 'string') {
        manualQuery = manualQuery === 'true';
      }
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
        if (response.error) {
          throw new Error(response.error);
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
        if (!this._colResizeInitialized) { this.initializeColResizable(); }

        // initialize sortable table
        if (!colDragDropInitialized) { this.initializeColDragDrop(); }
      } catch (error) {
        pendingPromise = null;
        this.loading = false;
        this.sessions.data = undefined;
        this.error = resolveMessage(error, this.$t);
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
          if (e.related.classList.contains('ignore-element')) { return false; }
          // info is always pinned to the end; don't allow drops onto/past it
          if (e.related.classList.contains('info-col-header')) { return false; }
          // don't allow info itself to be dragged off the end
          if (e.dragged.classList.contains('info-col-header')) { return false; }
          return true;
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

          // info column has no right-edge resize grip and absorbs slack,
          // so keep it pinned as the last visible column
          this.normalizeInfoColumnLast();

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
      this.destroyColResizable(); // idempotent re-init
      this._colResizeInitialized = true;
      // info column absorbs slack; skip table-width update so it can reflow.
      const hasInfoColumn = () => this.headers.some(h => h.exp === 'info');
      this._gripAttachment = attachTableGrips({
        cols: document.getElementsByClassName('arkime-col-header'),
        table: this.$refs.sessionsTable,
        minWidth: MIN_COL_WIDTH,
        shouldUpdateTable: () => !hasInfoColumn(),
        onCommit: ({ cols }) => {
          this.loading = true;
          for (let i = 0; i < cols.length; i++) {
            const w = cols[i].offsetWidth || parseInt(cols[i].style.width, 10) || 0;
            const header = this.headers[i];
            if (header && header.exp !== 'info') {
              header.width = w;
              this.colWidths[header.dbField] = w;
            }
          }
          this.saveColumnWidths();
          this.mapHeadersToFields();
          this.loading = false;
        }
      });
    },
    destroyColResizable () {
      if (this._gripAttachment) {
        this._gripAttachment.detach();
        this._gripAttachment = null;
      }
      this._colResizeInitialized = false;
    },
    /* Saves the column widths */
    saveColumnWidths: function () {
      UserService.saveState(this.colWidths, 'sessionsColWidths')
        .catch((error) => { this.error = error; });
    },
    /* Ensures the info column (if visible) is pinned as the last visible
     * column. The info column auto-fills slack space and has no right-edge
     * resize grip, so placing anything to its right leaves a non-resizable
     * seam in the middle of the table. */
    normalizeInfoColumnLast: function () {
      const vh = this.tableState && this.tableState.visibleHeaders;
      if (!vh || !vh.length) { return; }
      const idx = vh.indexOf('info');
      if (idx < 0 || idx === vh.length - 1) { return; }
      vh.splice(idx, 1);
      vh.push('info');
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

      const availableWidth = this.availableTableWidth();

      if (this.tableState.visibleHeaders.indexOf('info') >= 0) {
        const fillWithInfoCol = availableWidth - this.sumOfColWidths;
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
        // display a button to fit the table to the width of the container
        // if the table is more than 10 pixels larger or smaller than it
        if (Math.abs(this.tableWidth - availableWidth) > 10) {
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
    if (resizeTimeout) { clearTimeout(resizeTimeout); }
    if (filterFieldsTimeout) { clearTimeout(filterFieldsTimeout); }

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

/* column config save menu (Vuetify migrated) */
.col-config-list {
  overflow: auto;
  min-width: 280px;
  max-width: 350px;
  max-height: 300px;
}

/* info-field visibility dropdown: same group-header → indented field
   hierarchy as FieldSelectDropdownBody. v-list-subheader applies its own
   padding so we tighten group-header here without the .field-item rule
   stomping it. */
.col-dropdown-menu .group-header {
  text-transform: uppercase;
  font-weight: bold;
  font-size: 0.75rem;
  padding: 0.4rem 0.5rem 0.2rem;
  color: rgb(var(--v-theme-foreground-accent));
  min-height: 0;
}
.col-dropdown-menu .field-item {
  padding-inline-start: 1.25rem !important;
}

/* Single dropdown that collapses the old open/close/fit/columns/save
   button cluster in the top-left table cell. */
table.sessions-table thead tr th.sessions-options-cell {
  /* override the global vertical-align: top on thead th so the menu
     button sits centered in the cell instead of pinned to the top */
  vertical-align: middle;
  text-align: center;
}
.sessions-options-btn {
  margin: 0;
  vertical-align: baseline;
}
.sessions-options-menu :deep(.v-list-item__prepend) {
  min-width: 32px;
}
.sessions-options-menu :deep(.v-list-item__append .v-icon) {
  opacity: 0.5;
}

/* needed for grips */
.arkime-col-header {
  position: relative;
}

/* small dropdown buttons in column headers */
.arkime-col-header .col-dropdown {
  padding: 0 6px;
}

.arkime-col-header .col-dropdown:not(.info-vis-menu) {
  visibility: hidden;
  margin-left: -25px;
}

/* solid background on the column header dropdown trigger so the table
   header text behind it doesn't bleed through the tonal overlay */
.arkime-col-header .v-btn.col-context-trigger {
  background-color: rgb(var(--v-theme-background)) !important;
}

/* the last column's chevron would otherwise float past the right edge of
   the table (clipped by the viewport with no scrollbar, hidden under the
   vertical scrollbar gutter when one is present). Pin it absolutely just
   inside the th's right edge so it's visible in either case. */
.arkime-col-header:last-child .col-dropdown.col-context-trigger {
  position: absolute;
  right: 0;
  top: 50%;
  transform: translate(-100%, -50%);
  float: none;
  margin-left: 0;
}

</style>

<style scoped>
/* paging sub-navbar: fixed band height, controls centered within it */
.paging-navbar {
  height: 44px;
  padding: 0 4px;
}

.sessions-content {
  min-height: 500px;
}

/* sessions table styles --------------------- */
table.sessions-table {
  margin-bottom: 20px;
  table-layout: fixed;
  /* separate borders so they travel with the sticky header cells */
  border-collapse: separate;
  border-spacing: 0;
}

/* borders for header */
table.sessions-table thead tr th {
  vertical-align: middle;
  text-align: left;
  border-bottom: 2px solid rgb(var(--v-theme-neutral));
  border-right: 1px dotted rgb(var(--v-theme-neutral));
}
/* remove right border for first column because it is no resizeable */
table.sessions-table thead tr th:first-child {
  border-right: none;
  padding: 0;
  vertical-align: middle;
}

/* alternate-row striping */
table.sessions-table tbody tr:nth-of-type(odd) td {
  background-color: rgb(var(--v-theme-neutral-lighter));
}

/* table hover -- target the tds directly because the zebra rule above
   paints each cell, which would otherwise hide a hover bg set on the
   tr for the odd rows. */
table.sessions-table tbody tr:not(.session-detail-row):hover td,
table.sessions-table tbody tr:not(.session-detail-row):hover td.active {
  background-color: rgb(var(--v-theme-tertiary-lightest));
}

/* detail row background color */
table.sessions-table tbody tr.session-detail-row {
  background-color: rgb(var(--v-theme-quaternary-lightest)) !important;
}

/* condense the table and put values at the top of ceslls */
table.sessions-table tbody tr td {
  padding: 0 2px;
  line-height: 1.42857143;
  vertical-align: middle;
}

/* leftmost column is always the same */
table.sessions-table thead tr th.ignore-element,
table.sessions-table tbody tr td.ignore-element {
  width: 85px;
  white-space: nowrap;
}

/* sticky table header ----------------------- */
/* native sticky: the page-scroll container is the one scroller for both
   axes, so the header pins vertically and stays aligned horizontally with
   the body for free */
table.sessions-table.sticky-header > thead th {
  position: sticky;
  top: 0;
  z-index: 2;
  box-shadow: 0 6px 9px -6px black;
  background-color: rgb(var(--v-theme-background));
}
/* each sticky th is its own stacking context; lift the dragged cell so
   the grip's guide line isn't painted under the following cells */
table.sessions-table.sticky-header > thead th.col-resizing {
  z-index: 3;
}
/* keep the grip fully inside its cell — the overhanging half would
   hit-test under the next (opaque) sticky th */
table.sessions-table thead .grip {
  right: 0;
}

/* table column headers -------------------- */
.arkime-col-header {
  font-size: .9rem;
  text-align: left;
}

.arkime-col-header.active {
  color: rgb(var(--v-theme-foreground-accent));
}

/* Hovering anywhere on the column header reveals the chevron with a
   button-like background tint. The tint is 60% alpha (applied to the
   background only, not via opacity on the whole button) so the caret
   itself stays crisp / fully opaque. */
.arkime-col-header:hover .col-dropdown {
  visibility: visible;
  opacity: 1;
  /* Use the base quaternary color at low alpha so it still reads as
     tinted (not washed-out gray like quaternary-lighter at 60% was). */
  background-color: rgba(var(--v-theme-quaternary), 0.6);
}

.arkime-col-header .header-text {
  display: inline-block;
  width: calc(100% - 24px);
  /* one line max so the header height stays within scroll-margin-top */
  white-space: nowrap;
  overflow: hidden;
  text-overflow: ellipsis;
}

.arkime-col-header .header-sort {
  display: inline-block;
  margin-right: 3px;
  vertical-align: top;
}

.info-col-header .col-dropdown:not(.info-vis-menu) {
  margin-right: 4px;
}

/* Info-column cog button sits to the left of the "Info" label; small
   right margin separates it from the friendly name. */
.info-col-actions {
  margin-right: 5px;
}
.info-col-header-inner {
  display: inline-flex;
  align-items: center;
  gap: 4px;
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

/* keep scrolled-to sessions clear of the sticky column headers */
.sessions-scroll-margin {
  scroll-margin-top: 40px;
}
</style>
