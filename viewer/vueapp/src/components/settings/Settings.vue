<!--
Copyright Yahoo Inc.
SPDX-License-Identifier: Apache-2.0
-->
<template>
  <!-- settings content -->
  <div class="settings-page">
    <!-- messages (success/error) displayed at bottom of page -->
    <v-alert
      v-if="showMessage"
      :type="vuetifyMsgType"
      variant="tonal"
      density="compact"
      closable
      style="z-index: 2000;"
      class="position-fixed fixed-bottom m-0 rounded-0"
      @click:close="showMessage = false">
      {{ msg }}
    </v-alert> <!-- /messages -->

    <!-- sub navbar -->
    <div class="sub-navbar">
      <span class="sub-navbar-title">
        <span class="fa-stack">
          <span class="fa fa-cogs fa-stack-1x" />
          <span class="fa fa-square-o fa-stack-2x" />
        </span>&nbsp;
        <span>
          {{ $t(displayName ? 'settings.settingsFor' : 'settings.settings', { user: displayName }) }}
        </span>
      </span>
    </div> <!-- /sub navbar -->

    <!-- loading overlay -->
    <arkime-loading
      v-if="loading" /> <!-- /loading overlay -->

    <!-- page error -->
    <arkime-error
      v-if="error"
      :message-html="error"
      class="settings-error" /> <!-- /page error -->

    <!-- content -->
    <v-row
      class="settings-content"
      v-if="!loading && !error && settings">
      <!-- navigation -->
      <v-col
        cols="12"
        xl="2"
        lg="3"
        md="3"
        sm="4"
        role="tablist"
        aria-orientation="vertical">
        <v-tabs
          :model-value="visibleTab"
          @update:model-value="openView($event)"
          direction="vertical"
          density="compact"
          color="primary"
          selected-class="font-weight-bold">
          <v-tab value="general">
            <span class="fa fa-fw fa-cog me-1" />
            {{ $t('settings.nav.general') }}
          </v-tab>
          <v-tab value="col">
            <span class="fa fa-fw fa-columns me-1" />
            {{ $t('settings.nav.columnLayout') }}
          </v-tab>
          <v-tab value="info">
            <span class="fa fa-fw fa-info me-1" />
            {{ $t('settings.nav.infoFieldLayout') }}
          </v-tab>
          <v-tab value="spiview">
            <span class="fa fa-fw fa-eyedropper me-1" />
            {{ $t('settings.nav.spiViewLayout') }}
          </v-tab>
          <v-tab value="theme">
            <span class="fa fa-fw fa-paint-brush me-1" />
            {{ $t('settings.nav.themes') }}
          </v-tab>
          <v-tab
            v-if="(!multiviewer || hasUsersES) && !disablePassword"
            value="password">
            <span class="fa fa-fw fa-lock me-1" />
            {{ $t('settings.nav.password') }}
          </v-tab>
          <v-divider class="my-1" />
          <v-tab value="views">
            <span class="fa fa-fw fa-eye me-1" />
            {{ $t('settings.nav.views') }}
          </v-tab>
          <v-tab
            v-if="!multiviewer || hasUsersES"
            value="shortcuts">
            <span class="fa fa-fw fa-list me-1" />
            {{ $t('settings.nav.shortcuts') }}
          </v-tab>
          <v-tab
            v-if="!multiviewer"
            value="cron">
            <span class="fa fa-fw fa-search me-1" />
            {{ $t('settings.nav.cron') }}
          </v-tab>
          <v-tab
            v-has-role="{user:user,roles:'arkimeAdmin'}"
            value="notifiers">
            <span class="fa fa-fw fa-bell me-1" />
            {{ $t('settings.nav.notifiers') }}
          </v-tab>
        </v-tabs>
      </v-col> <!-- /navigation -->

      <v-col
        cols="12"
        xl="10"
        lg="9"
        md="9"
        sm="8"
        class="settings-right-panel">
        <!-- general settings -->
        <form
          class="form-horizontal"
          v-if="visibleTab === 'general'"
          id="general">
          <h3 class="d-flex align-center">
            <span class="flex-grow-1">{{ $t('settings.general.title') }}</span>
            <v-btn
              variant="flat"
              size="large"
              color="warning"
              @click="resetSettings">
              <span class="fa fa-repeat me-2" />
              {{ $t('settings.general.reset') }}
            </v-btn>
          </h3>

          <hr>

          <!-- timezone -->
          <v-row class="form-group">
            <v-col
              tag="label"
              cols="12"
              sm="3"
              class="text-end font-weight-bold align-self-center">
              {{ $t('settings.general.timezoneFormat') }}
            </v-col>
            <v-col
              cols="12"
              sm="9">
              <div class="d-inline-flex align-center">
                <v-btn-toggle
                  density="compact"
                  divided
                  variant="outlined"
                  color="secondary"
                  class="me-2"
                  :model-value="settings.timezone"
                  @update:model-value="updateTimezone"
                  mandatory>
                  <v-btn value="local">
                    {{ $t('settings.general.tz-local') }}
                  </v-btn>
                  <v-btn value="localtz">
                    {{ $t('settings.general.tz-localtz') }}
                  </v-btn>
                  <v-btn value="gmt">
                    {{ $t('settings.general.tz-gmt') }}
                  </v-btn>
                </v-btn-toggle>
                <v-btn-toggle
                  density="compact"
                  variant="outlined"
                  color="secondary"
                  multiple
                  :model-value="settings.ms ? ['ms'] : []"
                  @update:model-value="(val) => updateMs(val.includes('ms'))">
                  <v-btn
                    value="ms"
                    id="millisecondsSetting">
                    {{ $t('common.milliseconds') }}
                    <v-tooltip
                      activator="parent"
                      location="top">
                      {{ $t('settings.general.millisecondsSettingTip') }}
                    </v-tooltip>
                  </v-btn>
                </v-btn-toggle>
                <label class="ms-2 font-weight-bold text-theme-primary">
                  {{ timezoneDateString(date, settings.timezone, settings.ms) }}
                </label>
              </div>
            </v-col>
          </v-row> <!-- /timezone -->

          <!-- session detail format -->
          <v-row class="form-group">
            <v-col
              tag="label"
              cols="12"
              sm="3"
              class="text-end font-weight-bold align-self-center">
              {{ $t('settings.general.sessionDetailFormat') }}
            </v-col>
            <v-col
              cols="12"
              sm="9">
              <v-btn-toggle
                density="compact"
                divided
                variant="outlined"
                color="secondary"
                class="d-inline-flex"
                :model-value="settings.detailFormat"
                @update:model-value="updateSessionDetailFormat"
                mandatory>
                <v-btn value="last">
                  {{ $t('settings.general.lastUsed') }}
                </v-btn>
                <v-btn value="natural">
                  {{ $t('settings.general.detail-natural') }}
                </v-btn>
                <v-btn value="ascii">
                  {{ $t('settings.general.detail-ascii') }}
                </v-btn>
                <v-btn value="utf8">
                  {{ $t('settings.general.detail-utf8') }}
                </v-btn>
                <v-btn value="hex">
                  {{ $t('settings.general.detail-hex') }}
                </v-btn>
              </v-btn-toggle>
            </v-col>
          </v-row> <!-- /session detail format -->

          <!-- number of packets -->
          <v-row class="form-group">
            <v-col
              tag="label"
              cols="12"
              sm="3"
              class="text-end font-weight-bold align-self-center">
              {{ $t('settings.general.numberOfPackets') }}
            </v-col>
            <v-col
              cols="12"
              sm="9">
              <v-btn-toggle
                density="compact"
                divided
                variant="outlined"
                color="secondary"
                class="d-inline-flex"
                :model-value="settings.numPackets"
                @update:model-value="updateNumberOfPackets"
                mandatory>
                <v-btn value="last">
                  {{ $t('settings.general.lastUsed') }}
                </v-btn>
                <v-btn value="50">
                  50
                </v-btn>
                <v-btn value="200">
                  200
                </v-btn>
                <v-btn value="500">
                  500
                </v-btn>
                <v-btn value="1000">
                  1,000
                </v-btn>
                <v-btn value="2000">
                  2,000
                </v-btn>
              </v-btn-toggle>
            </v-col>
          </v-row> <!-- /number of packets -->

          <!-- show packet timestamp -->
          <v-row class="form-group">
            <v-col
              tag="label"
              cols="12"
              sm="3"
              class="text-end font-weight-bold align-self-center">
              {{ $t('settings.general.showPacketInfo') }}
            </v-col>
            <v-col
              cols="12"
              sm="9">
              <v-btn-toggle
                density="compact"
                divided
                variant="outlined"
                color="secondary"
                class="d-inline-flex"
                :model-value="settings.showTimestamps"
                @update:model-value="updateShowPacketTimestamps"
                mandatory>
                <v-btn value="last">
                  {{ $t('settings.general.lastUsed') }}
                </v-btn>
                <v-btn value="on">
                  {{ $t('settings.general.info-on') }}
                </v-btn>
                <v-btn value="off">
                  {{ $t('settings.general.info-off') }}
                </v-btn>
              </v-btn-toggle>
            </v-col>
          </v-row> <!-- /show packet timestamp -->

          <!-- issue query on initial page load -->
          <v-row class="form-group">
            <v-col
              tag="label"
              cols="12"
              sm="3"
              class="text-end font-weight-bold align-self-center">
              {{ $t('settings.general.queryOnLoad') }}
            </v-col>
            <v-col
              cols="12"
              sm="9">
              <v-btn-toggle
                density="compact"
                divided
                variant="outlined"
                color="secondary"
                class="d-inline-flex"
                :model-value="settings.manualQuery"
                @update:model-value="updateQueryOnPageLoad"
                mandatory>
                <v-btn value="false">
                  {{ $t('settings.general.query-false') }}
                </v-btn>
                <v-btn value="true">
                  {{ $t('settings.general.query-true') }}
                </v-btn>
              </v-btn-toggle>
            </v-col>
          </v-row> <!-- /issue query on initial page load -->

          <!-- session sort -->
          <v-row class="form-group">
            <v-col
              tag="label"
              cols="12"
              sm="3"
              class="text-end font-weight-bold align-self-center">
              {{ $t('settings.general.sortSessionsBy') }}
            </v-col>
            <v-col
              cols="12"
              sm="6">
              <div class="arkime-input-group arkime-input-group--fluid">
                <select
                  class="arkime-input-control"
                  v-model="settings.sortColumn"
                  @change="update">
                  <option value="last">
                    {{ $t('settings.general.lastUsed') }}
                  </option>
                  <option
                    v-for="field in sortableColumns"
                    :key="field.dbField"
                    :value="field.dbField">
                    {{ field.friendlyName }}
                  </option>
                </select>
              </div>
            </v-col>
            <v-col
              cols="12"
              sm="3">
              <v-btn-toggle
                v-if="settings.sortColumn !== 'last'"
                density="compact"
                divided
                variant="outlined"
                color="secondary"
                class="d-inline-flex"
                :model-value="settings.sortDirection"
                @update:model-value="updateSortDirection"
                mandatory>
                <v-btn value="asc">
                  {{ $t('settings.general.sort-asc') }}
                </v-btn>
                <v-btn value="desc">
                  {{ $t('settings.general.sort-desc') }}
                </v-btn>
              </v-btn-toggle>
            </v-col>
          </v-row> <!-- /session sort -->

          <!-- default spi graph -->
          <v-row
            v-if="fields && settings.spiGraph"
            class="form-group">
            <v-col
              tag="label"
              cols="12"
              sm="3"
              class="text-end font-weight-bold align-self-center">
              {{ $t('settings.general.defaultSPIGraph') }}
            </v-col>
            <v-col
              cols="12"
              sm="6">
              <div class="arkime-input-group arkime-input-group--fluid">
                <arkime-field-typeahead
                  :dropup="true"
                  :fields="fields"
                  query-param="field"
                  :initial-value="spiGraphTypeahead"
                  @field-selected="spiGraphFieldSelected" />
              </div>
            </v-col>
            <v-col
              cols="12"
              sm="3">
              <h4 v-if="spiGraphField">
                <label
                  id="spiGraphFieldSetting"
                  class="arkime-badge arkime-badge--info cursor-help">
                  {{ spiGraphTypeahead || 'unknown field' }}
                  <v-tooltip activator="#spiGraphFieldSetting">{{ spiGraphField.help }}</v-tooltip>
                </label>
              </h4>
            </v-col>
          </v-row> <!-- /default spi graph -->

          <!-- connections src field -->
          <v-row
            v-if="fields && settings.connSrcField"
            class="form-group">
            <v-col
              tag="label"
              cols="12"
              sm="3"
              class="text-end font-weight-bold align-self-center">
              {{ $t('settings.general.connectionsSrc') }}
            </v-col>
            <v-col
              cols="12"
              sm="6">
              <div class="arkime-input-group arkime-input-group--fluid">
                <arkime-field-typeahead
                  :dropup="true"
                  :fields="fields"
                  query-param="field"
                  :initial-value="connSrcFieldTypeahead"
                  @field-selected="connSrcFieldSelected" />
              </div>
            </v-col>
            <v-col
              cols="12"
              sm="3">
              <h4 v-if="connSrcField">
                <label
                  class="arkime-badge arkime-badge--info cursor-help"
                  id="connSrcFieldSetting">
                  {{ connSrcFieldTypeahead || 'unknown field' }}
                  <v-tooltip activator="#connSrcFieldSetting">{{ connSrcField.help }}</v-tooltip>
                </label>
              </h4>
            </v-col>
          </v-row> <!-- /connections src field -->

          <!-- connections dst field -->
          <v-row
            v-if="fields && settings.connDstField"
            class="form-group">
            <v-col
              tag="label"
              cols="12"
              sm="3"
              class="text-end font-weight-bold align-self-center">
              {{ $t('settings.general.connectionsDst') }}
            </v-col>
            <v-col
              cols="12"
              sm="6">
              <div class="arkime-input-group arkime-input-group--fluid">
                <arkime-field-typeahead
                  :dropup="true"
                  :fields="fields"
                  query-param="field"
                  :initial-value="connDstFieldTypeahead"
                  @field-selected="connDstFieldSelected" />
              </div>
            </v-col>
            <v-col
              cols="12"
              sm="3">
              <h4 v-if="connDstField">
                <label
                  class="arkime-badge arkime-badge--info cursor-help"
                  id="connDstFieldSetting">
                  {{ connDstFieldTypeahead || 'unknown field' }}
                  <v-tooltip activator="#connDstFieldSetting">{{ connDstField.help }}</v-tooltip>
                </label>
              </h4>
            </v-col>
          </v-row> <!-- /connections dst field -->

          <v-row
            v-if="integerFields"
            class="form-group">
            <v-col
              tag="label"
              cols="12"
              sm="3"
              class="text-end font-weight-bold align-self-center">
              {{ $t('settings.general.timelineDataFilters') }}
            </v-col>

            <v-col
              cols="12"
              sm="6">
              <div class="arkime-input-group arkime-input-group--fluid">
                <arkime-field-typeahead
                  :dropup="true"
                  :fields="integerFields"
                  :initial-value="filtersTypeahead"
                  query-param="field"
                  @field-selected="timelineFilterSelected" />
              </div>
            </v-col>
            <v-col
              cols="12"
              sm="3">
              <h4 class="d-flex align-center gap-1">
                <template v-if="timelineDataFilters.length > 0">
                  <label
                    class="arkime-badge arkime-badge--info cursor-help arkime-badge--sm"
                    v-for="filter in timelineDataFilters"
                    :key="filter.dbField + 'DataFilterBadge'"
                    @click="timelineFilterSelected(filter)"
                    :id="filter.dbField + 'DataFilterBadge'">
                    <span class="fa fa-times" />
                    {{ filter.friendlyName || 'unknown field' }}
                    <v-tooltip :activator="`[id='${filter.dbField}DataFilterBadge']`">{{ filter.help }}</v-tooltip>
                  </label>
                </template>
                <v-btn
                  id="resetTimelineFilters"
                  color="error"
                  variant="flat"
                  size="default"
                  icon
                  :aria-label="$t('settings.general.resetTimelineDataFilters')"
                  @click="resetDefaultFilters">
                  <span class="fa fa-refresh" />
                  <v-tooltip activator="#resetTimelineFilters">
                    {{ $t('settings.general.resetTimelineDataFilters') }}
                  </v-tooltip>
                </v-btn>
              </h4>
            </v-col>
          </v-row>

          <!-- hide tags field -->
          <v-row
            v-if="fields"
            class="form-group">
            <v-col
              tag="label"
              cols="12"
              sm="3"
              class="text-end font-weight-bold align-self-center">
              {{ $t('settings.general.hideTags') }}
            </v-col>
            <v-col
              cols="12"
              sm="6">
              <div class="arkime-input-group arkime-input-group--fluid">
                <input
                  type="text"
                  @change="update"
                  v-model="settings.hideTags"
                  class="arkime-input-control"
                  :placeholder="$t('settings.general.hideTagsPlaceholder')">
              </div>
            </v-col>
          </v-row> <!-- /hide tags field -->
        </form>

        <!-- col configs settings -->
        <form
          v-if="visibleTab === 'col'"
          class="form-horizontal"
          id="col">
          <h3>{{ $t('settings.ccl.title') }}</h3>

          <p>{{ $t('settings.ccl.info') }}</p>

          <table class="arkime-table">
            <thead>
              <tr>
                <th>{{ $t('settings.ccl.table-name') }}</th>
                <th>{{ $t('settings.ccl.table-columns') }}</th>
                <th>{{ $t('settings.ccl.table-order') }}</th>
                <th>&nbsp;</th>
              </tr>
            </thead>
            <tbody>
              <!-- default col config -->
              <tr v-if="defaultColConfig && fieldsMap">
                <td>
                  {{ $t('settings.arkimeDefault') }}
                </td>
                <td>
                  <template
                    v-for="col in defaultColConfig.visibleHeaders"
                    :key="col">
                    <label
                      class="arkime-badge arkime-badge--grey me-1 cursor-help"
                      :id="`${col}DefaultColConfigSetting`"
                      v-if="fieldsMap[col]">
                      {{ fieldsMap[col].friendlyName }}
                      <v-tooltip :activator="`[id='${col}DefaultColConfigSetting']`">{{ fieldsMap[col].help }}</v-tooltip>
                    </label>
                  </template>
                </td>
                <td>
                  <span
                    v-for="order in defaultColConfig.order"
                    :key="order[0]">
                    <label
                      class="arkime-badge arkime-badge--grey me-1 cursor-help"
                      v-if="fieldsMap[order[0]]"
                      :id="`${order[0]}DefaultColConfigSettingOrder`">
                      {{ fieldsMap[order[0]].friendlyName }}&nbsp;
                      ({{ order[1] }})
                      <v-tooltip :activator="`[id='${order[0]}DefaultColConfigSettingOrder']`">{{ fieldsMap[order[0]].help }}</v-tooltip>
                    </label>
                  </span>
                </td>
                <td>&nbsp;</td>
              </tr> <!-- /default col configs -->
              <!-- col configs -->
              <template v-if="fieldsMap">
                <tr
                  v-for="(config, index) in colConfigs"
                  :key="config.name">
                  <td>
                    {{ config.name }}
                  </td>
                  <td>
                    <template
                      v-for="col in config.columns"
                      :key="col">
                      <label
                        class="arkime-badge arkime-badge--grey me-1 cursor-help"
                        v-if="fieldsMap[col]"
                        :id="`${index}${col}ColConfigSetting`">
                        {{ fieldsMap[col].friendlyName }}
                        <v-tooltip :activator="`[id='${index}${col}ColConfigSetting']`">{{ fieldsMap[col].help }}</v-tooltip>
                      </label>
                    </template>
                  </td>
                  <td>
                    <span
                      v-for="order in config.order"
                      :key="order[0]">
                      <label
                        class="arkime-badge arkime-badge--grey me-1 cursor-help"
                        v-if="fieldsMap[order[0]]"
                        :id="`${index}-${order[0]}ColConfigSetting`">
                        {{ fieldsMap[order[0]].friendlyName }}&nbsp;
                        ({{ order[1] }})
                        <v-tooltip :activator="`[id='${index}-${order[0]}ColConfigSetting']`">{{ fieldsMap[order[0]].help }}</v-tooltip>
                      </label>
                    </span>
                  </td>
                  <td>
                    <v-btn
                      color="error"
                      variant="flat"
                      size="small"
                      density="comfortable"
                      class="float-right"
                      @click="deleteLayout('sessionstable', config.name, 'colConfigs', index)"
                      :title="$t('settings.ccl.deleteTip')">
                      <span class="fa fa-trash-o me-1" />
                      {{ $t('common.delete') }}
                    </v-btn>
                  </td>
                </tr>
              </template> <!-- /col configs -->
              <!-- col config list error -->
              <tr v-if="colConfigError">
                <td colspan="3">
                  <p class="text-danger mb-0">
                    <span class="fa fa-exclamation-triangle" />&nbsp;
                    {{ colConfigError }}
                  </p>
                </td>
              </tr> <!-- /col config list error -->
            </tbody>
          </table>

          <v-alert
            v-if="!colConfigs || !colConfigs.length"
            type="info"
            variant="tonal"
            density="compact">
            <strong>
              {{ $t('settings.ccl.empty') }}
            </strong>
            <br>
            <br>
            <span v-html="$t('settings.ccl.howToHtml')" />
          </v-alert>
        </form> <!-- /col configs settings -->

        <!-- info field configs settings -->
        <form
          v-if="visibleTab === 'info'"
          class="form-horizontal"
          id="col">
          <h3>{{ $t('settings.infoLayout.title') }}</h3>

          <p>{{ $t('settings.infoLayout.info') }}</p>

          <table class="arkime-table">
            <thead>
              <tr>
                <th>{{ $t('settings.infoLayout.table-name') }}</th>
                <th>{{ $t('settings.infoLayout.table-fields') }}</th>
                <th>&nbsp;</th>
              </tr>
            </thead>
            <tbody>
              <!-- default info field config -->
              <tr v-if="defaultInfoFieldLayout && fieldsMap">
                <td>
                  {{ $t('settings.arkimeDefault') }}
                </td>
                <td>
                  <template
                    v-for="field in defaultInfoFieldLayout"
                    :key="field">
                    <label
                      class="arkime-badge arkime-badge--grey me-1 cursor-help"
                      :id="`${field}DefaultInfoFieldLayoutSetting`"
                      v-if="fieldsMap[field]">
                      {{ fieldsMap[field].friendlyName }}
                      <v-tooltip :activator="`[id='${field}DefaultInfoFieldLayoutSetting']`">{{ fieldsMap[field].help }}</v-tooltip>
                    </label>
                  </template>
                </td>
                <td>&nbsp;</td>
              </tr> <!-- /default info field configs -->
              <!-- info field configs -->
              <template v-if="fieldsMap">
                <tr
                  v-for="(config, index) in infoFieldLayouts"
                  :key="config.name">
                  <td>
                    {{ config.name }}
                  </td>
                  <td>
                    <template
                      v-for="field in config.fields"
                      :key="field">
                      <label
                        class="arkime-badge arkime-badge--grey me-1 cursor-help"
                        :id="`${field}InfoFieldLayoutSetting`"
                        v-if="fieldsMap[field]">
                        {{ fieldsMap[field].friendlyName }}
                        <v-tooltip :activator="`[id='${field}InfoFieldLayoutSetting']`">{{ fieldsMap[field].help }}</v-tooltip>
                      </label>
                    </template>
                  </td>
                  <td>
                    <v-btn
                      color="error"
                      variant="flat"
                      size="small"
                      density="comfortable"
                      class="float-right"
                      @click="deleteLayout('sessionsinfofields', config.name, 'infoFieldLayouts', index)"
                      :title="$t('settings.infoLayout.deleteTip')">
                      <span class="fa fa-trash-o me-1" />
                      {{ $t('common.delete') }}
                    </v-btn>
                  </td>
                </tr>
              </template> <!-- /info field configs -->
              <!-- info field config list error -->
              <tr v-if="infoFieldLayoutError">
                <td colspan="3">
                  <p class="text-danger mb-0">
                    <span class="fa fa-exclamation-triangle" />&nbsp;
                    {{ infoFieldLayoutError }}
                  </p>
                </td>
              </tr> <!-- /info field config list error -->
            </tbody>
          </table>

          <v-alert
            v-if="!infoFieldLayouts || !infoFieldLayouts.length"
            type="info"
            variant="tonal"
            density="compact">
            <strong>
              {{ $t('settings.infoLayout.empty') }}
            </strong>
            <br>
            <br>
            <span v-html="$t('settings.infoLayout.howToHtml')" />
          </v-alert>
        </form> <!-- /info field configs settings -->

        <!-- spiview field configs settings -->
        <form
          v-if="visibleTab === 'spiview'"
          class="form-horizontal"
          id="spiview">
          <h3>{{ $t('settings.spiview.title') }}</h3>

          <p>{{ $t('settings.spiview.info') }}</p>

          <table class="arkime-table">
            <thead>
              <tr>
                <th>{{ $t('settings.spiview.table-name') }}</th>
                <th>{{ $t('settings.spiview.table-fields') }}</th>
                <th>&nbsp;</th>
              </tr>
            </thead>
            <tbody>
              <!-- default spiview field config -->
              <tr v-if="defaultSpiviewConfig && fieldsMap">
                <td>
                  {{ $t('settings.arkimeDefault') }}
                </td>
                <td>
                  <template
                    v-for="field in defaultSpiviewConfig.fields"
                    :key="field">
                    <label
                      :id="`${field}DefaultSpiviewFieldConfigSetting`"
                      v-if="fieldsMap[field]"
                      class="arkime-badge arkime-badge--grey me-1 cursor-help">
                      {{ fieldsMap[field].friendlyName }} (100)
                      <v-tooltip :activator="`[id='${field}DefaultSpiviewFieldConfigSetting']`">{{ fieldsMap[field].help }}</v-tooltip>
                    </label>
                  </template>
                </td>
                <td>&nbsp;</td>
              </tr> <!-- /default spiview field config -->
              <!-- spiview field configs -->
              <template v-if="fieldsMap">
                <tr
                  v-for="(config, index) in spiviewConfigs"
                  :key="config.name">
                  <td>
                    {{ config.name }}
                  </td>
                  <td>
                    <label
                      class="arkime-badge arkime-badge--grey me-1 cursor-help"
                      :id="`${fieldObj.dbField}SpiviewFieldConfigSetting`"
                      v-for="fieldObj in config.fieldObjs"
                      :key="fieldObj.dbField">
                      {{ fieldObj.friendlyName }}
                      ({{ fieldObj.count }})
                      <v-tooltip :activator="`[id='${fieldObj.dbField}SpiviewFieldConfigSetting']`">{{ fieldObj.help }}</v-tooltip>
                    </label>
                  </td>
                  <td>
                    <v-btn
                      color="error"
                      variant="flat"
                      size="small"
                      density="comfortable"
                      class="float-right"
                      @click="deleteLayout('spiview', config.name, 'spiviewConfigs', index)"
                      :title="$t('settings.spiview.deleteTip')">
                      <span class="fa fa-trash-o me-1" />
                      {{ $t('common.delete') }}
                    </v-btn>
                  </td>
                </tr>
              </template> <!-- /spiview field configs -->
              <!-- spiview field config list error -->
              <tr v-if="spiviewConfigError">
                <td colspan="3">
                  <p class="text-danger mb-0">
                    <span class="fa fa-exclamation-triangle" />&nbsp;
                    {{ spiviewConfigError }}
                  </p>
                </td>
              </tr> <!-- /spiview field config list error -->
            </tbody>
          </table>

          <v-alert
            v-if="!spiviewConfigs || !spiviewConfigs.length"
            type="info"
            variant="tonal"
            density="compact">
            <strong>
              {{ $t('settings.spiview.empty') }}
            </strong>
            <br>
            <br>
            <span v-html="$t('settings.spiview.howToHtml')" />
          </v-alert>
        </form> <!-- /spiview field configs settings -->

        <!-- theme settings -->
        <form
          v-if="visibleTab === 'theme'"
          id="theme">
          <h3>{{ $t('settings.themes.title') }}</h3>

          <p>{{ $t('settings.themes.pickTheme') }}</p>

          <hr>

          <!-- theme picker -->
          <v-row>
            <v-col
              cols="12"
              lg="6"
              md="12"
              v-for="theme in themeDisplays"
              :class="theme.class"
              :key="theme.class">
              <div class="theme-display">
                <v-row>
                  <v-col
                    cols="12"
                    md="12">
                    <div class="d-flex align-center ms-1">
                      <input
                        type="radio"
                        class="cursor-pointer"
                        v-model="settings.theme"
                        @change="changeTheme(theme.class)"
                        :value="theme.class"
                        :id="theme.class">
                      <label
                        class="cursor-pointer ms-2"
                        :for="theme.class">
                        {{ theme.name }}
                      </label>
                    </div>
                  </v-col>
                </v-row>
                <nav class="preview-navbar preview-navbar-dark">
                  <a class="preview-navbar-brand cursor-pointer">
                    <img
                      :src="settings.logo"
                      class="arkime-logo"
                      alt="hoot">
                  </a>
                  <ul class="preview-nav">
                    <a class="preview-nav-item cursor-pointer no-decoration active">
                      Current Page
                    </a>
                    <a class="preview-nav-item cursor-pointer no-decoration ms-3">
                      Other Pages
                    </a>
                  </ul>
                  <ul class="preview-navbar-status me-2">
                    <span class="fa fa-info-circle fa-lg health-green" />
                  </ul>
                </nav>
                <div class="display-sub-navbar">
                  <v-row>
                    <v-col
                      cols="12"
                      xl="5"
                      lg="4"
                      md="5">
                      <div class="arkime-input-group ms-1">
                        <span class="arkime-input-label arkime-input-label-fw">
                          <span class="fa fa-search fa-fw" />
                        </span>
                        <input
                          type="text"
                          placeholder="Search"
                          class="arkime-input-control">
                      </div>
                    </v-col>
                    <v-col
                      cols="12"
                      xl="7"
                      lg="8"
                      sm="7">
                      <div class="font-weight-bold text-theme-accent ms-1">
                        Important text
                      </div>
                      <div class="float-right display-sub-navbar-buttons">
                        <a class="theme-display-btn btn-theme-tertiary-display me-1">
                          Search
                        </a>
                        <a class="theme-display-btn btn-theme-quaternary-display me-1">
                          <span class="fa fa-cog fa-lg" />
                        </a>
                        <a class="theme-display-btn btn-theme-secondary-display me-1">
                          <span class="fa fa-eye fa-lg" />
                        </a>
                        <v-menu location="bottom end">
                          <template #activator="{ props: activatorProps }">
                            <a
                              v-bind="activatorProps"
                              class="theme-display-btn btn-theme-primary-display float-right action-menu-dropdown">
                              <span class="fa fa-caret-down" />
                            </a>
                          </template>
                          <v-list density="compact">
                            <v-list-item>
                              Example
                            </v-list-item>
                            <v-list-item active>
                              Active Example
                            </v-list-item>
                          </v-list>
                        </v-menu>
                      </div>
                    </v-col>
                  </v-row>
                </div>
                <div class="display-sub-sub-navbar">
                  <div class="ms-1 mt-2 pb-2">
                    <span class="field cursor-pointer">
                      example field value
                      <span class="fa fa-caret-down" />
                    </span>
                  </div>
                </div>
              </div>
            </v-col>
          </v-row> <!-- /theme picker -->

          <!-- logo picker -->
          <hr>
          <h3>{{ $t('settings.themes.logos') }}</h3>
          <p>{{ $t('settings.themes.pickLogo') }}</p>
          <v-row class="well logo-well me-1 ms-1">
            <v-col
              cols="12"
              lg="3"
              md="4"
              sm="6"
              class="mb-2 mt-2 logos"
              v-for="logo in logos"
              :key="logo.location">
              <div class="d-flex align-center ms-1">
                <input
                  type="radio"
                  :id="logo.location"
                  :value="logo.location"
                  v-model="settings.logo"
                  @change="changeLogo(logo.location)"
                  class="cursor-pointer">
                <label
                  class="cursor-pointer ms-2"
                  :for="logo.location">
                  {{ logo.name }}
                </label>
              </div>
              <img
                :src="logo.location"
                :alt="logo.name">
            </v-col>
          </v-row> <!-- /logo picker -->

          <div v-if="settings.shiftyEyes">
            <hr>
            <h3>
              Yahaha! You found me!
              <v-btn
                color="primary"
                variant="flat"
                size="large"
                class="ms-2"
                @click="toggleShiftyEyes">
                Turn Me Off
              </v-btn>
            </h3>
            <p>
              I am now watching you while data loads
            </p>
            <img :src="watching">
          </div>

          <hr>

          <!-- custom theme -->
          <p v-if="!creatingCustom">
            <span v-html="$t('settings.themes.moreControlHtml')" />
            <a
              href="javascript:void(0)"
              class="cursor-pointer"
              @click="creatingCustom = true">
              {{ $t('settings.themes.createCustom') }}
            </a>
            <br><br>
          </p>

          <div v-if="creatingCustom">
            <!-- custom theme display -->
            <v-row>
              <v-col
                cols="12"
                md="4">
                <h3 class="mt-0 mb-3 d-flex align-center">
                  <span class="flex-grow-1">Custom Theme</span>
                  <v-btn
                    variant="flat"
                    size="large"
                    color="tertiary"
                    @click="displayHelp = !displayHelp">
                    <span class="fa fa-question-circle me-1" />
                    <span v-if="displayHelp">
                      Hide
                    </span>
                    <span v-else>
                      Show
                    </span>
                    Help
                  </v-btn>
                </h3>
                <color-picker
                  :color="background"
                  @color-selected="changeColor"
                  color-name="background"
                  field-name="Background"
                  :class="{'mb-2':!displayHelp}" />
                <p
                  class="help-block small"
                  v-if="displayHelp">
                  This color should either be very light or very dark.
                </p>
                <color-picker
                  :color="foreground"
                  @color-selected="changeColor"
                  color-name="foreground"
                  field-name="Foreground"
                  :class="{'mb-2':!displayHelp}" />
                <p
                  class="help-block small"
                  v-if="displayHelp">
                  This color should be visible on the background.
                </p>
                <color-picker
                  :color="foregroundAccent"
                  @color-selected="changeColor"
                  color-name="foregroundAccent"
                  field-name="Foreground Accent" />
                <p
                  class="help-block small"
                  v-if="displayHelp">
                  This color should stand out.
                  It displays session field values and important text in navbars.
                </p>
              </v-col>
              <v-col
                cols="12"
                md="8">
                <div
                  class="custom-theme"
                  id="custom-theme-display">
                  <div class="theme-display">
                    <div class="preview-navbar preview-navbar-dark">
                      <a class="preview-navbar-brand cursor-pointer">
                        <img
                          :src="settings.logo"
                          class="arkime-logo"
                          alt="hoot">
                      </a>
                      <ul class="preview-nav">
                        <a class="preview-nav-item cursor-pointer active">
                          Current Page
                        </a>
                        <a class="preview-nav-item cursor-pointer ms-3">
                          Other Pages
                        </a>
                      </ul>
                      <ul class="preview-navbar-status me-2">
                        <span class="fa fa-info-circle fa-lg health-green" />
                      </ul>
                    </div>
                    <div class="display-sub-navbar">
                      <v-row>
                        <v-col
                          cols="12"
                          xl="5"
                          lg="4"
                          md="5">
                          <div class="arkime-input-group ms-1">
                            <span class="arkime-input-label arkime-input-label-fw">
                              <span class="fa fa-search fa-fw" />
                            </span>
                            <input
                              type="text"
                              placeholder="Search"
                              class="arkime-input-control">
                          </div>
                        </v-col>
                        <v-col
                          cols="12"
                          xl="7"
                          lg="8"
                          sm="7">
                          <div class="font-weight-bold text-theme-accent ms-1">
                            Important text
                          </div>
                          <div class="float-right display-sub-navbar-buttons">
                            <a class="theme-display-btn btn-theme-tertiary-display me-1">
                              Search
                            </a>
                            <a class="theme-display-btn btn-theme-quaternary-display me-1">
                              <span class="fa fa-cog fa-lg" />
                            </a>
                            <a class="theme-display-btn btn-theme-secondary-display me-1">
                              <span class="fa fa-eye fa-lg" />
                            </a>
                            <v-menu location="bottom end">
                              <template #activator="{ props: activatorProps }">
                                <a
                                  v-bind="activatorProps"
                                  class="theme-display-btn btn-theme-primary-display float-right action-menu-dropdown">
                                  <span class="fa fa-caret-down" />
                                </a>
                              </template>
                              <v-list density="compact">
                                <v-list-item>
                                  Example
                                </v-list-item>
                                <v-list-item active>
                                  Active Example
                                </v-list-item>
                              </v-list>
                            </v-menu>
                          </div>
                        </v-col>
                      </v-row>
                    </div>
                    <div class="display-sub-sub-navbar">
                      <arkime-paging
                        class="mt-1 ms-1"
                        :records-total="200"
                        :records-filtered="100" />
                    </div>
                    <div>
                      <div class="ms-1 me-1 mt-2 pb-2">
                        <span class="field cursor-pointer">
                          example field value
                          <span class="fa fa-caret-down" />
                        </span>
                        <br><br>
                        <v-row>
                          <v-col
                            cols="12"
                            md="6"
                            class="sessionsrc">
                            <small class="session-detail-ts font-weight-bold">
                              <em class="ts-value">
                                2013/11/18 03:06:52.831
                              </em>
                              <span class="float-right">
                                27 bytes
                              </span>
                            </small>
                            <pre>Source packet text</pre>
                          </v-col>
                          <v-col
                            cols="12"
                            md="6"
                            class="sessiondst">
                            <small class="session-detail-ts font-weight-bold">
                              <em class="ts-value">
                                2013/11/18 03:06:52.841
                              </em>
                              <span class="float-right">
                                160 bytes
                              </span>
                            </small>
                            <pre>Destination packet text</pre>
                          </v-col>
                        </v-row>
                      </div>
                    </div>
                  </div>
                </div>
              </v-col>
            </v-row> <!-- /custom theme display -->

            <br>

            <p
              v-if="displayHelp"
              class="help-block">
              Main theme colors are lightened/darkened programmatically to
              provide dark borders, active buttons, hover colors, etc.
            </p>

            <!-- main colors -->
            <v-row class="form-group">
              <v-col
                cols="12"
                md="3">
                <color-picker
                  :color="primary"
                  @color-selected="changeColor"
                  color-name="primary"
                  field-name="Primary" />
                <p
                  v-if="displayHelp"
                  class="help-block small">
                  Primary navbar, buttons, active item(s) in lists
                </p>
              </v-col>
              <v-col
                cols="12"
                md="3">
                <color-picker
                  :color="secondary"
                  @color-selected="changeColor"
                  color-name="secondary"
                  field-name="Secondary" />
                <p
                  v-if="displayHelp"
                  class="help-block small">
                  Buttons
                </p>
              </v-col>
              <v-col
                cols="12"
                md="3">
                <color-picker
                  :color="tertiary"
                  @color-selected="changeColor"
                  color-name="tertiary"
                  field-name="Tertiary" />
                <p
                  v-if="displayHelp"
                  class="help-block small">
                  Action buttons (search, apply, open, etc)
                </p>
              </v-col>
              <v-col
                cols="12"
                md="3">
                <color-picker
                  :color="quaternary"
                  @color-selected="changeColor"
                  color-name="quaternary"
                  field-name="Quaternary" />
                <p
                  v-if="displayHelp"
                  class="help-block small">
                  Accent and all other buttons
                </p>
              </v-col>
            </v-row> <!-- /main colors -->

            <p
              v-if="displayHelp"
              class="help-block">
              <em>Highlight colors should be similar to their parent color, above.</em>
              <br>
              For <strong>light themes</strong>, the highlight color should be <strong>lighter</strong> than the original.
              For <strong>dark themes</strong>, the highlight color should be <strong>darker</strong> than original.
            </p>

            <!-- main color highlights/backgrounds -->
            <v-row class="form-group">
              <v-col
                cols="12"
                md="3">
                <color-picker
                  :color="primaryLightest"
                  @color-selected="changeColor"
                  color-name="primaryLightest"
                  field-name="Highlight 1" />
                <p
                  v-if="displayHelp"
                  class="help-block small">
                  Backgrounds
                </p>
              </v-col>
              <v-col
                cols="12"
                md="3">
                <color-picker
                  :color="secondaryLightest"
                  @color-selected="changeColor"
                  color-name="secondaryLightest"
                  field-name="Highlight 2" />
                <p
                  v-if="displayHelp"
                  class="help-block small">
                  Search/Secondary navbar
                </p>
              </v-col>
              <v-col
                cols="12"
                md="3">
                <color-picker
                  :color="tertiaryLightest"
                  @color-selected="changeColor"
                  color-name="tertiaryLightest"
                  field-name="Highlight 3" />
                <p
                  v-if="displayHelp"
                  class="help-block small">
                  Tertiary navbar, table hover
                </p>
              </v-col>
              <v-col
                cols="12"
                md="3">
                <color-picker
                  :color="quaternaryLightest"
                  @color-selected="changeColor"
                  color-name="quaternaryLightest"
                  field-name="Highlight 4" />
                <p
                  v-if="displayHelp"
                  class="help-block small">
                  Session detail background
                </p>
              </v-col>
            </v-row> <!-- /main color highlights/backgrounds -->

            <br>

            <v-row
              v-if="displayHelp">
              <v-col cols="6">
                <p class="help-block">
                  <em>Map colors</em>
                  <br>
                  These should be different to show contrast between land and water.
                </p>
              </v-col>
              <v-col cols="6">
                <p class="help-block">
                  <em>Packet colors</em>
                  <br>
                  These are displayed when viewing session packets and in the
                  sessions timeline graph. They should be very different colors.
                </p>
              </v-col>
            </v-row>

            <v-row class="form-group">
              <!-- visualization colors -->
              <v-col
                cols="12"
                md="3">
                <color-picker
                  :color="water"
                  @color-selected="changeColor"
                  color-name="water"
                  field-name="Map Water" />
              </v-col>
              <v-col
                cols="12"
                md="3">
                <color-picker
                  :color="land"
                  @color-selected="changeColor"
                  color-name="land"
                  field-name="Map Land" />
              </v-col> <!-- /visualization colors -->
              <!-- packet colors -->
              <v-col
                cols="12"
                md="3">
                <color-picker
                  :color="src"
                  @color-selected="changeColor"
                  color-name="src"
                  field-name="Source Packets" />
              </v-col>
              <v-col
                cols="12"
                md="3">
                <color-picker
                  :color="dst"
                  @color-selected="changeColor"
                  color-name="dst"
                  field-name="Destination Packets" />
              </v-col> <!-- /packet colors -->
            </v-row>

            <br>

            <v-row class="mb-4">
              <v-col
                cols="12"
                md="12">
                <label>
                  Share your theme with others:
                </label>
                <div class="arkime-input-group arkime-input-group--fluid">
                  <input
                    type="text"
                    class="arkime-input-control"
                    v-model="themeString"
                    @keyup.up.down.left.right.a.b="secretStuff">
                  <v-btn
                    variant="flat"
                    size="small"
                    density="comfortable"
                    :style="secondaryBtnStyle"
                    class="me-1"
                    @click="copyValue(themeString)">
                    <span class="fa fa-clipboard me-1" />
                    {{ $t('common.copy') }}
                  </v-btn>
                  <v-btn
                    variant="flat"
                    size="small"
                    density="comfortable"
                    :style="primaryBtnStyle"
                    class="me-1"
                    @click="updateThemeString">
                    <span class="fa fa-check me-1" />
                    {{ $t('common.apply') }}
                  </v-btn>
                </div>
              </v-col>
            </v-row>
          </div> <!-- /custom theme -->
        </form> <!-- /theme settings -->

        <!-- password settings -->
        <form
          v-if="visibleTab === 'password' && (!multiviewer || hasUsersES) && !disablePassword"
          class="form-horizontal"
          @keyup.enter="changePassword"
          id="password">
          <h3>{{ $t('settings.password.title') }}</h3>

          <hr>

          <!-- current password -->
          <v-row
            v-if="!userId"
            class="mb-2">
            <v-col
              tag="label"
              cols="12"
              sm="3"
              class="text-end font-weight-bold align-self-center">
              {{ $t('settings.password.currentPassword') }}
            </v-col>
            <v-col
              cols="12"
              sm="6">
              <div class="arkime-input-group arkime-input-group--fluid">
                <input
                  type="password"
                  class="arkime-input-control"
                  v-model="currentPassword"
                  :placeholder="$t('settings.password.currentPasswordPlaceholder')">
              </div>
            </v-col>
          </v-row>

          <!-- new password -->
          <v-row class="mb-2">
            <v-col
              tag="label"
              cols="12"
              sm="3"
              class="text-end font-weight-bold align-self-center">
              {{ $t('settings.password.newPassword') }}
            </v-col>
            <v-col
              cols="12"
              sm="6">
              <div class="arkime-input-group arkime-input-group--fluid">
                <input
                  type="password"
                  class="arkime-input-control"
                  v-model="newPassword"
                  :placeholder="$t('settings.password.newPasswordPlaceholder')">
              </div>
            </v-col>
          </v-row>

          <!-- confirm new password -->
          <v-row class="mb-2">
            <v-col
              tag="label"
              cols="12"
              sm="3"
              class="text-end font-weight-bold align-self-center">
              {{ $t('settings.password.confirmPassword') }}
            </v-col>
            <v-col
              cols="12"
              sm="6">
              <div class="arkime-input-group arkime-input-group--fluid">
                <input
                  type="password"
                  class="arkime-input-control"
                  v-model="confirmNewPassword"
                  :placeholder="$t('settings.password.confirmPasswordPlaceholder')">
              </div>
            </v-col>
          </v-row>

          <!-- change password button/error -->
          <v-row class="mb-2">
            <v-col
              tag="label"
              cols="12"
              sm="3"
              class="align-self-center">
&nbsp;
            </v-col>
            <v-col
              cols="12"
              sm="9">
              <v-btn
                variant="flat"
                size="large"
                color="tertiary"
                @click="changePassword">
                {{ $t('settings.password.changePassword') }}
              </v-btn>
              <span
                v-if="changePasswordError"
                class="small text-danger ps-4">
                <span class="fa fa-exclamation-triangle" />&nbsp;
                {{ changePasswordError }}
              </span>
            </v-col>
          </v-row> <!-- /change password button/error -->

          <!-- TOTP Two-Factor Authentication (only for admins) -->
          <div v-has-role="{user:user,roles:'arkimeAdmin,cont3xtAdmin,wiseAdmin'}">
            <hr class="my-4">
            <div class="d-flex align-center mb-1">
              <h4 class="mb-0 me-2">
                {{ $t('settings.totp.title') }}
              </h4>
              <!-- Enroll button (when not enrolled) -->
              <v-btn
                v-if="!totpEnabled && !totpSetupMode"
                variant="flat"
                size="large"
                color="primary"
                @click="startTotpSetup">
                {{ $t('settings.totp.enroll') }}
              </v-btn>
              <!-- Enabled status + Unenroll button (when enrolled) -->
              <span
                v-if="totpEnabled && !totpSetupMode"
                class="text-success me-2">
                <span class="fa fa-check" /> {{ $t('settings.totp.enabled') }}
              </span>
              <v-btn
                v-if="totpEnabled && !totpSetupMode"
                color="error"
                variant="flat"
                size="large"
                @click="showTotpDisable = true">
                {{ $t('settings.totp.unenroll') }}
              </v-btn>
              <!-- Cancel button (when in setup mode) -->
              <v-btn
                v-if="totpSetupMode"
                color="warning"
                variant="flat"
                size="large"
                @click="cancelTotpSetup">
                {{ $t('common.cancel') }}
              </v-btn>
            </div>
            <p class="small mb-3">
              {{ $t('settings.totp.description') }}
            </p>

            <!-- TOTP Setup (QR Code + Verification) -->
            <div
              v-if="totpSetupMode"
              class="mb-3">
              <div class="my-2">
                <img
                  v-if="totpQRDataUrl"
                  :src="totpQRDataUrl"
                  alt="TOTP QR Code"
                  class="rounded p-2"
                  style="border: 1px solid var(--color-foreground, #333); background-color: var(--color-background, #fff);">
                <span
                  v-else>{{ $t('settings.totp.generatingQR') }}</span>
              </div>
              <div class="small mb-2">
                {{ $t('settings.totp.manualEntry') }}: <code>{{ totpSecret }}</code>
              </div>
              <div
                class="arkime-input-group"
                style="width: 400px;">
                <span class="arkime-input-label">{{ $t('settings.totp.verifyCode') }}</span>
                <input
                  type="text"
                  maxlength="6"
                  class="arkime-input-control"
                  v-model="totpVerifyCode"
                  :placeholder="$t('settings.totp.codePlaceholder')"
                  @keyup.enter="confirmTotpSetup">
                <v-btn
                  color="success"
                  variant="flat"
                  size="small"
                  density="comfortable"
                  class="me-1"
                  :disabled="!totpVerifyCode || totpVerifyCode.length !== 6"
                  @click="confirmTotpSetup">
                  {{ $t('settings.totp.verify') }}
                </v-btn>
              </div>
              <span
                v-if="totpError"
                class="small text-danger d-block mt-2">
                <span class="fa fa-exclamation-triangle" />&nbsp;
                {{ totpError }}
              </span>
            </div>

            <!-- TOTP Disable Confirmation -->
            <div
              v-if="showTotpDisable"
              class="mb-3">
              <div
                class="arkime-input-group"
                style="width: 400px;">
                <span class="arkime-input-label">{{ $t('settings.totp.confirmDisable') }}</span>
                <input
                  type="text"
                  maxlength="6"
                  class="arkime-input-control"
                  v-model="totpDisableCode"
                  :placeholder="$t('settings.totp.codePlaceholder')"
                  @keyup.enter="disableTotp">
                <v-btn
                  color="error"
                  variant="flat"
                  size="small"
                  density="comfortable"
                  class="me-1"
                  :disabled="!totpDisableCode || totpDisableCode.length !== 6"
                  @click="disableTotp">
                  {{ $t('settings.totp.confirmUnenroll') }}
                </v-btn>
              </div>
              <v-btn
                color="grey"
                variant="flat"
                size="large"
                class="mt-2"
                @click="showTotpDisable = false; totpDisableCode = ''">
                {{ $t('common.cancel') }}
              </v-btn>
              <span
                v-if="totpError"
                class="small text-danger d-block mt-2">
                <span class="fa fa-exclamation-triangle" />&nbsp;
                {{ totpError }}
              </span>
            </div>
          </div> <!-- /TOTP Two-Factor Authentication (only for admins) -->
        </form> <!-- /password settings -->

        <!-- notifiers settings -->
        <Notifiers
          id="notifiers"
          parent-app="arkime"
          @display-message="displayMessage"
          v-if="visibleTab === 'notifiers'"
          v-has-role="{user:user,roles:'arkimeAdmin'}"
          help-intl-id="settings.notifiers.helpViewer" />

        <!-- shortcut settings -->
        <Shortcuts
          id="shortcuts"
          @copy-value="copyValue"
          v-if="visibleTab === 'shortcuts'"
          @display-message="displayMessage" />

        <!-- view settings -->
        <Views
          id="views"
          :user-id="userId"
          :fields-map="fieldsMap"
          @copy-value="copyValue"
          v-if="visibleTab === 'views'"
          @display-message="displayMessage" />

        <!-- cron query settings -->
        <PeriodicQueries
          id="cron"
          :user-id="userId"
          @display-message="displayMessage"
          v-if="visibleTab === 'cron' && !multiviewer" />
      </v-col>
    </v-row> <!-- /content -->
  </div> <!-- /settings content -->
</template>

<script>
import { timezoneDateString } from '@common/vueFilters.js';
import CommonUserService from '@common/UserService';
import UserService from '../users/UserService';
import Notifiers from '@common/Notifiers.vue';
import FieldService from '../search/FieldService';
import SettingsService from './SettingsService';
import customCols from '../sessions/customCols.json';
import ArkimeError from '../utils/Error.vue';
import ArkimeLoading from '../utils/Loading.vue';
import ArkimeFieldTypeahead from '../utils/FieldTypeahead.vue';
import ColorPicker from '../utils/ColorPicker.vue';
import ArkimePaging from '../utils/Pagination.vue';
import Utils from '../utils/utils';
import PeriodicQueries from './PeriodicQueries.vue';
import Shortcuts from './Shortcuts.vue';
import Views from './Views.vue';

let clockInterval;

const defaultSpiviewConfig = { fields: ['destination.ip', 'protocol', 'source.ip'] };
const defaultInfoFields = JSON.parse(JSON.stringify(customCols.info.children));

const secretMatch = ['ArrowUp', 'ArrowUp', 'ArrowDown', 'ArrowDown', 'ArrowLeft', 'ArrowRight', 'ArrowLeft', 'ArrowRight', 'b', 'a'];
let secrets = [];

export default {
  name: 'Settings',
  components: {
    ArkimeError,
    ArkimeLoading,
    ArkimeFieldTypeahead,
    ColorPicker,
    ArkimePaging,
    PeriodicQueries,
    Shortcuts,
    Notifiers,
    Views
  },
  data: function () {
    return {
      // page vars
      userId: undefined,
      error: '',
      loading: true,
      msg: '',
      showMessage: false,
      msgType: undefined,
      displayName: undefined,
      visibleTab: 'general', // default tab
      settings: {},
      integerFields: undefined,
      columns: [],
      // general settings vars
      date: undefined,
      spiGraphField: undefined,
      spiGraphTypeahead: undefined,
      connSrcField: undefined,
      connSrcFieldTypeahead: undefined,
      connDstField: undefined,
      connDstFieldTypeahead: undefined,
      timelineDataFilters: [],
      filtersTypeahead: '',
      // column config settings vars
      colConfigs: undefined,
      colConfigError: '',
      defaultColConfig: Utils.getDefaultTableState(),
      // info field config settings vars
      infoFieldLayouts: undefined,
      infoFieldLayoutError: '',
      defaultInfoFieldLayout: defaultInfoFields,
      // spiview field config settings vars
      spiviewConfigs: undefined,
      spiviewConfigError: '',
      defaultSpiviewConfig,
      // theme settings vars
      watching: 'assets/watching.gif',
      themeDisplays: [
        { name: 'Arkime Light', class: 'arkime-light-theme' },
        { name: 'Arkime Dark', class: 'arkime-dark-theme' },
        { name: 'Purp-purp', class: 'purp-theme' },
        { name: 'Blue', class: 'blue-theme' },
        { name: 'Green', class: 'green-theme' },
        { name: 'Cotton Candy', class: 'cotton-candy-theme' },
        { name: 'Green on Black', class: 'dark-2-theme' },
        { name: 'Dark Blue', class: 'dark-3-theme' }
      ],
      logos: [
        { name: 'Arkime Light', location: 'assets/Arkime_Logo_Mark_White.png' },
        { name: 'Arkime Dark', location: 'assets/Arkime_Logo_Mark_Black.png' },
        { name: 'Arkime Color', location: 'assets/Arkime_Logo_Mark_Full.png' },
        { name: 'Arkime Gradient', location: 'assets/Arkime_Logo_Mark_FullGradient.png' },
        { name: 'Arkime Circle Light', location: 'assets/Arkime_Icon_White.png' },
        { name: 'Arkime Circle Dark', location: 'assets/Arkime_Icon_Black.png' },
        { name: 'Arkime Circle Mint', location: 'assets/Arkime_Icon_ColorMint.png' },
        { name: 'Arkime Circle Blue', location: 'assets/Arkime_Icon_ColorBlue.png' }
      ],
      creatingCustom: false,
      displayHelp: true,
      // password settings vars
      currentPassword: '',
      newPassword: '',
      confirmNewPassword: '',
      changePasswordError: '',
      // TOTP settings vars
      totpEnabled: false,
      totpSetupMode: false,
      totpSecret: '',
      totpQRDataUrl: '',
      totpVerifyCode: '',
      totpDisableCode: '',
      totpError: '',
      showTotpDisable: false,
      multiviewer: this.$constants.MULTIVIEWER,
      hasUsersES: this.$constants.HASUSERSES,
      // Arkime theme-color v-btn styles. Vuetify :color can't take CSS vars.
      primaryBtnStyle: {
        backgroundColor: 'var(--color-primary)',
        color: 'var(--color-button, #FFF)'
      },
      secondaryBtnStyle: {
        backgroundColor: 'var(--color-secondary)',
        color: 'var(--color-button, #FFF)'
      },
      tertiaryBtnStyle: {
        backgroundColor: 'var(--color-tertiary)',
        color: 'var(--color-button, #FFF)'
      },
      quaternaryBtnStyle: {
        backgroundColor: 'var(--color-quaternary)',
        color: 'var(--color-button, #FFF)'
      }
    };
  },
  computed: {
    user: function () {
      return this.$store.state.user;
    },
    // Vuetify v-alert accepts: success | info | warning | error.
    // Map Bootstrap-flavored "danger" → Vuetify "error".
    vuetifyMsgType: function () {
      return this.msgType === 'danger' ? 'error' : (this.msgType || 'success');
    },
    sortableColumns: function () {
      return this.columns.filter(column => !column.unsortable);
    },
    fields: function () {
      return FieldService.addIpDstPortField(this.$store.state.fieldsArr);
    },
    fieldsMap: function () {
      const fieldsMap = JSON.parse(JSON.stringify(this.$store.state.fieldsMap));
      for (const key in customCols) {
        fieldsMap[key] = customCols[key];
      }
      return FieldService.addIpDstPortField(this.$store.state.fieldsMap);
    },
    notifiers () {
      return this.$store.state.notifiers;
    },
    disablePassword () {
      return !!this.$constants.DISABLE_USER_PASSWORD_UI && !!this.user.headerAuthEnabled;
    }
  },
  created: function () {
    // does the url specify a tab in hash
    let tab = window.location.hash;
    if (tab) { // if there is a tab specified and it's a valid tab
      tab = tab.replace(/^#/, '');
      if (tab === 'general' || tab === 'views' || tab === 'cron' ||
        tab === 'col' || tab === 'info' || tab === 'theme' || tab === 'password' ||
        tab === 'spiview' || tab === 'notifiers' || tab === 'shortcuts') {
        this.visibleTab = tab;
      }

      if ((tab === 'password' && this.multiviewer) || (tab === 'cron' && this.multiviewer)) {
        // multiviewer user can't change password or configure periodic queries
        this.openView('general');
      }
    }

    this.getThemeColors();

    UserService.getCurrent().then((response) => {
      this.displayName = response.userId;
      // only admins can edit other users' settings
      if (response.roles.includes('arkimeAdmin') && this.$route.query.userId) {
        if (response.userId === this.$route.query.userId) {
          // admin editing their own user so the routeParam is unnecessary
          this.$router.push({
            hash: this.$route.hash,
            query: {
              ...this.$route.query,
              userId: undefined
            }
          });
        } else { // admin editing another user
          this.userId = this.$route.query.userId;
          this.displayName = this.$route.query.userId;
        }
      } else if (this.$route.query.userId) {
        // normal user has no permission, so remove the routeParam
        // (even if it's their own userId because it's unnecessary)
        this.$router.push({
          hash: this.$route.hash,
          query: {
            ...this.$route.query,
            userId: undefined
          }
        });
      }

      // always get the user's settings because current user is cached
      // so response.settings might be stale
      // NOTE: this kicks of fetching all the other data
      this.getSettings(true);

      // Load TOTP status
      this.loadTotpStatus();
    }).catch((error) => {
      this.error = error.text;
      this.loading = false;
    });
  },
  methods: {
    timezoneDateString,
    copyValue (val) {
      if (!navigator.clipboard) {
        alert(this.$t('common.clipboardNotSupported', { value: val }));
        return;
      }
      navigator.clipboard.writeText(val);
    },
    /* exposed page functions ---------------------------------------------- */
    /* opens a specific settings tab */
    openView: function (tabName) {
      this.visibleTab = tabName;
      this.$router.push({
        hash: `#${tabName}`,
        query: this.$route.query
      });
    },
    /* displays a message in the navbar */
    displayMessage ({ msg, type }) {
      this.msg = msg;
      this.showMessage = true;
      this.msgType = type || 'success';
    },
    /* GENERAL ------------------------------- */
    /**
     * saves the user's settings and displays a message
     * @param updateTheme whether to update the UI theme
     */
    update: function (updateTheme) {
      UserService.saveSettings(this.settings, this.userId).then((response) => {
        // display success message to user
        this.displayMessage({ msg: response.text });

        if (updateTheme) {
          const now = Date.now();
          if ($('link[href^="api/user/css"]').length) {
            $('link[href^="api/user/css"]').remove();
          }
          $('head').append(`<link rel="stylesheet"
                            href="api/user/css?v${now}"
                            type="text/css" />`);
        }
      }).catch((error) => {
        // display error message to user
        this.displayMessage({ msg: error.text, type: 'danger' });
      });
    },
    resetSettings: function () {
      // Choosing to skip reset of theme. UserService will save state to store
      UserService.resetSettings(this.userId, this.settings.theme).then((response) => {
        // display success message to user
        this.displayMessage({ msg: response.text });
        this.getSettings(false);
      }).catch((error) => {
        // display error message to user
        this.displayMessage({ msg: error.text, type: 'danger' });
      });
    },
    // attach the full field object to the component's timelineDataFilters from array of dbField
    setTimelineDataFilterFields: function () {
      this.timelineDataFilters = [];
      for (let i = 0, len = this.settings.timelineDataFilters.length; i < len; i++) {
        const filter = this.settings.timelineDataFilters[i];
        const fieldOBJ = FieldService.getField(filter);
        if (fieldOBJ) {
          this.timelineDataFilters.push(fieldOBJ);
        }
      }
    },
    resetDefaultFilters: function () {
      this.settings.timelineDataFilters = UserService.getDefaultSettings().timelineDataFilters;
      this.setTimelineDataFilterFields();
      this.update();
    },
    updateTimezone (newTimezone) {
      this.settings.timezone = newTimezone;
      this.updateTime();
    },
    updateMs (newMs) {
      this.settings.ms = newMs;
      this.updateTime();
    },
    /* updates the displayed date for the timezone setting
     * triggered by the user changing the timezone/ms settings */
    updateTime: function () {
      this.tick();
      this.update();
    },
    updateSessionDetailFormat: function (newDetailFormat) {
      this.settings.detailFormat = newDetailFormat;
      this.update();
    },
    updateNumberOfPackets: function (newNumPackets) {
      this.settings.numPackets = newNumPackets;
      this.update();
    },
    updateShowPacketTimestamps: function (newShowTimestamps) {
      this.settings.showTimestamps = newShowTimestamps;
      this.update();
    },
    updateQueryOnPageLoad: function (newManualQuery) {
      this.settings.manualQuery = newManualQuery;
      this.update();
    },
    updateSortDirection: function (newSortDirection) {
      this.settings.sortDirection = newSortDirection;
      this.update();
    },
    spiGraphFieldSelected: function (field) {
      this.spiGraphField = field;
      this.settings.spiGraph = field.dbField;
      this.spiGraphTypeahead = field.friendlyName;
      this.update();
    },
    connSrcFieldSelected: function (field) {
      this.connSrcField = field;
      this.settings.connSrcField = field.dbField;
      this.connSrcFieldTypeahead = field.friendlyName;
      this.update();
    },
    connDstFieldSelected: function (field) {
      this.connDstField = field;
      this.settings.connDstField = field.dbField;
      this.connDstFieldTypeahead = field.friendlyName;
      this.update();
    },
    timelineFilterSelected: function (field) {
      let index;
      for (index = 0; index < this.settings.timelineDataFilters.length; index++) {
        const filter = this.settings.timelineDataFilters[index];
        const filterDbField = FieldService.getFieldProperty(filter, 'dbField');
        if (filterDbField && filterDbField === field.dbField) {
          break;
        }
      }
      if (index === this.settings.timelineDataFilters.length) { index = -1; }

      if (index > -1) {
        this.timelineDataFilters.splice(index, 1);
        this.settings.timelineDataFilters.splice(index, 1);
        this.update();
      } else if (this.timelineDataFilters.length < 4) {
        this.timelineDataFilters.push(field);
        this.settings.timelineDataFilters.push(field.dbField);
        this.update();
      }
    },
    /* starts the clock for the timezone setting */
    startClock: function () {
      this.tick();
      clockInterval = setInterval(() => {
        this.tick();
      }, 1000);
    },
    /* updates the date and format for the timezone setting */
    tick: function () {
      this.date = new Date();
      if (this.settings.timezone === 'gmt') {
        this.dateFormat = 'yyyy/MM/dd HH:mm:ss\'Z\'';
      } else {
        this.dateFormat = 'yyyy/MM/dd HH:mm:ss';
      }
    },
    /**
     * Displays the field.exp instead of field.dbField in the
     * field typeahead inputs
     * @param {string} value The dbField of the field
     */
    formatField: function (value) {
      for (let i = 0, len = this.fields.length; i < len; i++) {
        if (value === this.fields[i].dbField) {
          return this.fields[i].friendlyName;
        }
      }
    },
    /* LAYOUTS ------------------------------------------ */
    /**
      * Saves a custom layout to the user's settings
      * @param {string} layoutType  The type of layout to save
      * @param {string} layoutName  The name of the layout to save
      * @param {array} layoutArray  The array to save the layout to
      * @param {int} index          The index in the array of the layout to save
      */
    deleteLayout (layoutType, layoutName, layoutArray, index) {
      UserService.deleteLayout(layoutType, layoutName, this.userId).then((response) => {
        this[layoutArray].splice(index, 1);
        // display success message to user
        this.displayMessage({ msg: response.text });
      }).catch((error) => {
        // display error message to user
        this.displayMessage({ msg: error.text, type: 'danger' });
      });
    },
    /* THEMES ------------------------------------------ */
    setTheme: function () {
      // default to default theme if the user has not set a theme
      if (!this.settings.theme) { this.settings.theme = 'arkime-light-theme'; }
      if (this.settings.theme.startsWith('custom')) {
        this.creatingCustom = true;
      }
      if (!this.settings.logo) {
        this.settings.logo = 'assets/Arkime_Logo_Mark_White.png';
      }
    },
    /* changes the ui theme (picked from existing themes) */
    changeTheme: function (newTheme) {
      document.body.className = newTheme;
      this.getThemeColors();
      this.update();
    },
    /* changes a color value of a custom theme and applies the theme */
    changeColor: function (newColor) {
      if (newColor) {
        this[newColor.name] = newColor.value;
      }

      document.body.className = 'custom-theme';

      this.setThemeString();

      this.settings.theme = `custom1:${this.themeString}`;

      this.update(true);
    },
    updateThemeString: function () {
      const colors = this.themeString.split(',');

      this.background = colors[0];
      this.foreground = colors[1];
      this.foregroundAccent = colors[2];

      this.primary = colors[3];
      this.primaryLightest = colors[4];

      this.secondary = colors[5];
      this.secondaryLightest = colors[6];

      this.tertiary = colors[7];
      this.tertiaryLightest = colors[8];

      this.quaternary = colors[9];
      this.quaternaryLightest = colors[10];

      this.water = colors[11];
      this.land = colors[12];

      this.src = colors[13];
      this.dst = colors[14];

      this.changeColor();
    },
    changeLogo: function (newLogoLocation) {
      this.settings.logo = newLogoLocation;
      this.update();
    },
    secretStuff: function (e) {
      secrets.push(e.key);
      for (let i = 0; i < secrets.length; i++) {
        if (secrets[i] !== secretMatch[i]) {
          secrets = [];
          break;
        }
      }

      if (secrets.length === secretMatch.length) {
        this.toggleShiftyEyes();
      }
    },
    toggleShiftyEyes: function () {
      this.settings.shiftyEyes = !this.settings.shiftyEyes;
      UserService.saveSettings(this.settings, this.userId).then((response) => {
        // display success message to user
        this.displayMessage({ msg: 'SUPER SECRET THING HAPPENED!' });
      }).catch((error) => {
        // display error message to user
        this.displayMessage({ msg: error.text, type: 'danger' });
      });
    },
    /* PASSWORD ---------------------------------------- */
    /* changes the user's password given the current password, the new password,
     * and confirmation of the new password */
    changePassword: function () {
      if (!this.userId && (!this.currentPassword || this.currentPassword === '')) {
        this.changePasswordError = this.$t('settings.password.currentPasswordRequired');
        return;
      }

      if (!this.newPassword || this.newPassword === '') {
        this.changePasswordError = this.$t('settings.password.newPasswordRequired');
        return;
      }

      if (!this.confirmNewPassword || this.confirmNewPassword === '') {
        this.changePasswordError = this.$t('settings.password.confirmPasswordRequired');
        return;
      }

      if (this.newPassword !== this.confirmNewPassword) {
        this.changePasswordError = this.$t('settings.password.mismatchedPassword');
        return;
      }

      const data = {
        newPassword: this.newPassword,
        currentPassword: this.currentPassword
      };

      CommonUserService.changePassword(data, this.userId).then((response) => {
        this.changePasswordError = false;
        this.currentPassword = null;
        this.newPassword = null;
        this.confirmNewPassword = null;
        // display success message to user
        this.displayMessage({ msg: response.text });
      }).catch((error) => {
        // display error message to user
        this.displayMessage({ msg: error.text, type: 'danger' });
      });
    },

    /* TOTP ---------------------------------------- */
    /* loads the current TOTP status */
    loadTotpStatus: function () {
      CommonUserService.getTotpStatus().then((response) => {
        this.totpEnabled = response.enabled;
      }).catch((error) => {
        console.error('Failed to get TOTP status:', error);
      });
    },
    /* starts the TOTP enrollment process */
    startTotpSetup: function () {
      this.totpError = '';
      this.totpVerifyCode = '';
      CommonUserService.setupTotp().then((response) => {
        this.totpSetupMode = true;
        this.totpSecret = response.secret;
        this.totpQRDataUrl = response.qrCodeDataUrl;
      }).catch((error) => {
        this.totpError = error;
      });
    },
    /* confirms TOTP enrollment with a verification code */
    confirmTotpSetup: function () {
      if (!this.totpVerifyCode || this.totpVerifyCode.length !== 6) {
        this.totpError = this.$t('settings.totp.invalidCode');
        return;
      }
      CommonUserService.confirmTotp(this.totpVerifyCode).then((response) => {
        this.totpSetupMode = false;
        this.totpEnabled = true;
        this.totpSecret = '';
        this.totpQRDataUrl = '';
        this.totpVerifyCode = '';
        this.totpError = '';
        this.displayMessage({ msg: response.text });
      }).catch((error) => {
        this.totpError = error;
      });
    },
    /* cancels TOTP setup */
    cancelTotpSetup: function () {
      this.totpSetupMode = false;
      this.totpSecret = '';
      this.totpQRDataUrl = '';
      this.totpVerifyCode = '';
      this.totpError = '';
    },
    /* disables TOTP with a verification code */
    disableTotp: function () {
      if (!this.totpDisableCode || this.totpDisableCode.length !== 6) {
        this.totpError = this.$t('settings.totp.invalidCode');
        return;
      }
      CommonUserService.disableTotp(this.totpDisableCode).then((response) => {
        this.totpEnabled = false;
        this.showTotpDisable = false;
        this.totpDisableCode = '';
        this.totpError = '';
        this.displayMessage({ msg: response.text });
      }).catch((error) => {
        this.totpError = error;
      });
    },

    /* helper functions ---------------------------------------------------- */
    /* retrieves the theme colors from the document body's property values */
    getThemeColors: function () {
      const styles = window.getComputedStyle(document.body);

      this.background = styles.getPropertyValue('--color-background').trim() || '#FFFFFF';
      this.foreground = styles.getPropertyValue('--color-foreground').trim() || '#333333';
      this.foregroundAccent = styles.getPropertyValue('--color-foreground-accent').trim();

      this.primary = styles.getPropertyValue('--color-primary').trim();
      this.primaryLightest = styles.getPropertyValue('--color-primary-lightest').trim();

      this.secondary = styles.getPropertyValue('--color-secondary').trim();
      this.secondaryLightest = styles.getPropertyValue('--color-secondary-lightest').trim();

      this.tertiary = styles.getPropertyValue('--color-tertiary').trim();
      this.tertiaryLightest = styles.getPropertyValue('--color-tertiary-lightest').trim();

      this.quaternary = styles.getPropertyValue('--color-quaternary').trim();
      this.quaternaryLightest = styles.getPropertyValue('--color-quaternary-lightest').trim();

      this.water = styles.getPropertyValue('--color-water').trim();
      this.land = styles.getPropertyValue('--color-land').trim() || this.primary;

      this.src = styles.getPropertyValue('--color-src').trim() || '#CA0404';
      this.dst = styles.getPropertyValue('--color-dst').trim() || '#0000FF';

      this.setThemeString();
    },
    setThemeString: function () {
      this.themeString = `${this.background},${this.foreground},${this.foregroundAccent},${this.primary},${this.primaryLightest},${this.secondary},${this.secondaryLightest},${this.tertiary},${this.tertiaryLightest},${this.quaternary},${this.quaternaryLightest},${this.water},${this.land},${this.src},${this.dst}`;
    },
    /* retrieves the specified user's settings */
    getSettings: function (initLoad) {
      UserService.getSettings(this.userId).then((response) => {
        // set the user settings individually
        for (const key in response) {
          this.settings[key] = response[key];
        }

        // set defaults if a user setting doesn't exist
        // so that radio buttons show the default value
        if (!response.timezone) {
          this.settings.timezone = 'local';
        }
        if (!response.detailFormat) {
          this.settings.detailFormat = 'last';
        }
        if (!response.numPackets) {
          this.settings.numPackets = 'last';
        }
        if (!response.showTimestamps) {
          this.settings.showTimestamps = 'last';
        }
        if (!response.manualQuery || response.manualQuery === 'last') {
          this.settings.manualQuery = false;
        }

        this.setupFields().then(() => {
          this.loading = false;

          if (initLoad) {
            // get all the other things!
            this.getColConfigs();
            this.getSpiviewConfigs();
            this.getInfoFieldLayout();
            SettingsService.getNotifiers(); // sets notifiers in the store
          }

          this.setTheme();
          this.startClock();
        }).catch((error) => {
          console.log('ERROR getting fields to populate page', error);
        });
      }).catch((error) => {
        this.loading = false;
        if (error.text === 'User not found') {
          this.error = `<div class="text-center">
                          ${error.text}
                          <small><a href="settings">View your own settings?</a></small>
                        </div>`;
        } else {
          this.error = error.text;
        }
        this.displayName = '';
      });
    },
    /* retrieves arkime fields and visible column headers for sessions table
     * adds custom columns to fields
     * sets user settings for spigraph field & connections src/dst fields
     * creates fields map for quick lookups
     */
    setupFields: function () {
      return new Promise((resolve, reject) => {
        this.integerFields = this.fields.filter(i => i.type === 'integer');

        this.setTimelineDataFilterFields();

        // update the user settings for spigraph field & connections src/dst fields
        // NOTE: dbField is saved in settings, but show the field's friendlyName
        const spigraphField = FieldService.getField(this.settings.spiGraph);
        if (spigraphField) {
          this.spiGraphField = spigraphField;
          this.spiGraphTypeahead = spigraphField.friendlyName;
        }
        const connSrcField = FieldService.getField(this.settings.connSrcField);
        if (connSrcField) {
          this.connSrcField = connSrcField;
          this.connSrcFieldTypeahead = connSrcField.friendlyName;
        }
        const connDstField = FieldService.getField(this.settings.connDstField);
        if (connDstField) {
          this.connDstField = connDstField;
          this.connDstFieldTypeahead = connDstField.friendlyName;
        }

        this.filtersTypeahead = '';

        // get the visible headers for the sessions table layout
        UserService.getState('sessionsNew').then((sessionsTableRes) => {
          const headers = sessionsTableRes.data.visibleHeaders || this.defaultColConfig.visibleHeaders;
          this.setupColumns(headers);
          // if the sort column setting does not match any of the visible
          // headers, set the sort column setting to last
          if (headers.indexOf(this.settings.sortColumn) === -1) {
            this.settings.sortColumn = 'last';
          }
          resolve();
        }).catch((error) => {
          this.setupColumns(this.defaultColConfig.visibleHeaders);
          resolve();
        });
      });
    },
    /* retrieves the specified user's custom column layouts */
    getColConfigs: function () {
      UserService.getLayout('sessionstable', this.userId).then((response) => {
        this.colConfigs = response;
      }).catch((error) => {
        this.colConfigError = error.text;
      });
    },
    /* retrieves the specified user's custom info field layouts */
    getInfoFieldLayout: function () {
      UserService.getLayout('sessionsinfofields', this.userId).then((response) => {
        this.infoFieldLayouts = response;
      }).catch((error) => {
        this.infoFieldError = error.text;
      });
    },
    /* retrieves the specified user's custom spiview fields layouts.
     * dissects the visible spiview fields for view consumption */
    getSpiviewConfigs: function () {
      UserService.getLayout('spiview', this.userId).then((response) => {
        this.spiviewConfigs = response;

        for (let x = 0, xlen = this.spiviewConfigs.length; x < xlen; ++x) {
          const config = this.spiviewConfigs[x];
          const spiParamsArray = config.fields.split(',');

          // get each field from the spi query parameter and issue
          // a query for one field at a time
          for (let i = 0, len = spiParamsArray.length; i < len; ++i) {
            const param = spiParamsArray[i];
            const split = param.split(':');
            const fieldID = split[0];
            const count = split[1];

            const field = FieldService.getField(fieldID);

            if (field) {
              if (!config.fieldObjs) { config.fieldObjs = []; }

              field.count = count;
              config.fieldObjs.push(field);
            }
          }
        }
      }).catch((error) => {
        this.spiviewConfigError = error.text;
      });
    },
    /**
     * Setup this.columns with a list of field objects
     * @param {array} colIdArray The array of column ids
     */
    setupColumns: function (colIdArray) {
      this.columns = [];
      for (let i = 0, len = colIdArray.length; i < len; ++i) {
        const field = FieldService.getField(colIdArray[i], true);
        if (field !== undefined) {
          this.columns.push(FieldService.getField(colIdArray[i], true));
        }
      }
    }
  },
  beforeUnmount () {
    if (clockInterval) { clearInterval(clockInterval); }

    // remove userId route query parameter so that when a user
    // comes back to this page, they are on their own settings
    if (!this.$route.query.userId) { return; }
    this.$router.replace({
      query: {
        ...this.$route.query,
        userId: undefined
      }
    });
  }
};
</script>

<style>
.settings-content {
  margin-top: 90px;
  margin-left: 0;
  margin-right: 0;
  overflow-x: hidden;
}
.settings-content .settings-right-panel {
  overflow-x: auto;
}

.settings-page .sub-navbar {
  z-index: 4;
}

.settings-page .small-badge {
  font-size: 60%;
  margin-right: 5px;
}

.settings-page .small-badge:hover {
  background-color: #dc3545;
  cursor: pointer;
}

/* make sure the form is taller than the nav pills */
.settings-page form:not(.b-dropdown-form) {
  min-height: 280px;
}

.settings-page .settings-error {
  margin-top: 6rem;
  margin-bottom: 1rem;
}

/* apply theme color to notifier cards */
.card {
  box-shadow: inset 0 1px 1px rgba(0, 0, 0, .05);
  background-color: var(--color-gray-lighter);
  border: 1px solid var(--color-gray-light);
}

/* theme displays ----------------- */
.logo-well .logos {
  text-align: center;
}
.logo-well .logos img {
  height: 100px;
}

.field {
  cursor: pointer;
  padding: 0 1px;
  border-radius: 3px;
  border: 1px solid transparent;
}
.field .fa {
  opacity: 0;
  visibility: hidden;
}
.field:hover .fa {
  opacity: 1;
  visibility: visible;
}

.settings-page .control-label {
  font-weight: bolder;
}

.settings-page .theme-display {
  overflow: hidden;
  border-radius: 6px;
  padding-bottom: 20px;
}

.settings-page .preview-navbar {
  min-height: 20px;
  height: 36px;
  border-radius: 6px 6px 0 0;
  z-index: 1;
}

.settings-page .preview-navbar .arkime-logo {
  top: 0;
  left: 20px;
  height: 36px;
  position: absolute;
}
/* icon logos (logo in circle) are wider */
.settings-page .preview-navbar .arkime-logo[src*="Icon"] {
  left: 8px;
}

.settings-page .preview-navbar .preview-nav {
  position: absolute;
  left: 50px
}

.settings-page .preview-navbar .preview-navbar-status {
  margin-right: -8px;
}

.settings-page .preview-navbar .preview-navbar-status .health-green {
  color: #00aa00;
}

.settings-page .preview-navbar-dark a {
  padding: 6px;
  color: #FFFFFF;
}

.settings-page .display-sub-navbar {
  height: 40px;
  position: relative;
  -webkit-box-shadow: 0 0 16px -2px black;
     -moz-box-shadow: 0 0 16px -2px black;
          box-shadow: 0 0 16px -2px black;
}
.settings-page .display-sub-navbar .display-sub-navbar-buttons {
  margin-top: 4px;
  margin-right: 4px;
  margin-left: -10px;
}

.settings-page .display-sub-navbar .text-theme-accent {
  display: inline-block;
  margin-left: -20px;
  padding-top: 11px;
  font-size: 12px;
}

.settings-page .display-sub-sub-navbar {
  border-radius: 0 0 6px 6px;
  margin-top: -6px;
  padding-top: 6px;
}

/* theme-display button (preview swatch). Replaces Bootstrap's .btn base
   so the theme picker preview survives the .btn class going away. The
   per-theme variants below override color/background. */
.theme-display-btn {
  display: inline-block;
  padding: 0.25rem 0.5rem;
  font-size: 0.875rem;
  line-height: 1.5;
  border: 1px solid transparent;
  border-radius: 0.25rem;
  cursor: pointer;
  user-select: none;
  text-decoration: none;
}

/* arkime light (default) */
.settings-page .arkime-light-theme .preview-navbar {
  background-color: #212121;
  border-color: #111111;
}

.settings-page .arkime-light-theme .preview-navbar-dark a:hover,
.settings-page .arkime-light-theme .preview-navbar-dark a.active {
  background-color: #303030;
}

.settings-page .arkime-light-theme .display-sub-navbar {
  background-color: #A4C2D6;
}

.settings-page .arkime-light-theme .display-sub-sub-navbar {
  background-color: #E6F3EB;
}

.settings-page .arkime-light-theme .display-sub-navbar .text-theme-accent {
  color: #004C83;
}

.settings-page .arkime-light-theme .display-sub-navbar .btn-theme-primary-display {
  color: #FFFFFF;
  background-color: #303030;
  border-color: #212121;
}
.settings-page .arkime-light-theme .display-sub-navbar .btn-theme-secondary-display {
  color: #FFFFFF;
  background-color: #004C83;
  border-color: #003A64;
}
.settings-page .arkime-light-theme .display-sub-navbar .btn-theme-tertiary-display {
  color: #FFFFFF;
  background-color: #66B689;
  border-color: #52AC79;
}
.settings-page .arkime-light-theme .display-sub-navbar .btn-theme-quaternary-display {
  color: #FFFFFF;
  background-color: #2F7D86;
  border-color: #27686F;
}

.settings-page .arkime-light-theme .arkime-dropdown-menu {
  background-color: #FFFFFF;
  border-color: #EEEEEE;
  padding: 2px 0;
  font-size: .85rem;
  min-width: 12rem;
}
.settings-page .arkime-light-theme .arkime-dropdown-menu .arkime-dropdown-item {
  color: #212529;
  padding: 2px 8px;
}
.settings-page .arkime-light-theme .arkime-dropdown-menu .arkime-dropdown-item:hover {
  background-color: #EEEEEE;
  color: #212529;
}
.settings-page .arkime-light-theme .arkime-dropdown-menu .arkime-dropdown-item:focus {
  background-color: #FFFFFF;
  color: #212529;
}
.settings-page .arkime-light-theme .arkime-dropdown-menu li.active .arkime-dropdown-item {
  background-color: #303030;
  color: #FFFFFF;
}

.settings-page .arkime-light-theme .field {
  color: #004C83;
}
.settings-page .arkime-light-theme .field:hover {
  background-color: #FFFFFF;
  border-color: #EEEEEE;
}

.settings-page .arkime-light-theme .btn {
  color: #FFFFFF !important;
}

/* arkime dark */
.settings-page .arkime-dark-theme .preview-navbar {
  background-color: #9E9E9E;
  border-color: #8E8E8E;
}

.settings-page .arkime-dark-theme .preview-navbar-dark a:hover,
.settings-page .arkime-dark-theme .preview-navbar-dark a.active {
  background-color: #ADADAD;
}

.settings-page .arkime-dark-theme .display-sub-navbar {
  background-color: #003B66;
}

.settings-page .arkime-dark-theme .display-sub-sub-navbar {
  background-color: #237543;
}

.settings-page .arkime-dark-theme .display-sub-navbar .text-theme-accent {
  color: #D1E9DC;
}

.settings-page .arkime-dark-theme .display-sub-navbar .btn-theme-primary-display {
  color: #FFFFFF;
  background-color: #ADADAD;
  border-color: #9E9E9E;
}
.settings-page .arkime-dark-theme .display-sub-navbar .btn-theme-secondary-display {
  color: #FFFFFF;
  background-color: #80A9C7;
  border-color: #6B9BBE;
}
.settings-page .arkime-dark-theme .display-sub-navbar .btn-theme-tertiary-display {
  color: #FFFFFF;
  background-color: #079B72;
  border-color: #077D5C;
}
.settings-page .arkime-dark-theme .display-sub-navbar .btn-theme-quaternary-display {
  color: #FFFFFF;
  background-color: #66B689;
  border-color: #52AC79;
}

.settings-page .arkime-dark-theme .arkime-dropdown-menu {
  background-color: #303030;
  border-color: #555555;
  padding: 2px 0;
  font-size: .85rem;
  min-width: 12rem;
}
.settings-page .arkime-dark-theme .arkime-dropdown-menu .arkime-dropdown-item {
  color: #FFFFFF;
  padding: 2px 8px;
}
.settings-page .arkime-dark-theme .arkime-dropdown-menu .arkime-dropdown-item:hover {
  background-color: #555555;
  color: #FFFFFF;
}
.settings-page .arkime-dark-theme .arkime-dropdown-menu .arkime-dropdown-item:focus {
  background-color: #555555;
  color: #FFFFFF;
}
.settings-page .arkime-dark-theme .arkime-dropdown-menu li.active .arkime-dropdown-item {
  background-color: #ADADAD;
  color: #303030;
}

.settings-page .arkime-dark-theme .field {
  color: #D1E9DC;
}
.settings-page .arkime-dark-theme .field:hover {
  background-color: #303030;
  border-color: #555555;
}

.settings-page .arkime-dark-theme .btn {
  color: #FFFFFF !important;
}

/* purp */
.settings-page .purp-theme .preview-navbar {
  background-color: #530763;
  border-color: #360540;
}

.settings-page .purp-theme .preview-navbar-dark a:hover,
.settings-page .purp-theme .preview-navbar-dark a.active {
  background-color: #830b9c;
}

.settings-page .purp-theme .display-sub-navbar {
  background-color: #EDFCFF;
}

.settings-page .purp-theme .display-sub-sub-navbar {
  background-color: #FFF7E5;
}

.settings-page .purp-theme .display-sub-navbar .text-theme-accent {
  color: #76207d;
}

.settings-page .purp-theme .display-sub-navbar .btn-theme-primary-display {
  color: #FFFFFF;
  background-color: #830B9C;
  border-color: #530763;
}
.settings-page .purp-theme .display-sub-navbar .btn-theme-secondary-display {
  color: #FFFFFF;
  background-color: #1F1FA5;
  border-color: #1A1A87;
}
.settings-page .purp-theme .display-sub-navbar .btn-theme-tertiary-display {
  color: #FFFFFF;
  background-color: #079B72;
  border-color: #077D5C;
}
.settings-page .purp-theme .display-sub-navbar .btn-theme-quaternary-display {
  color: #FFFFFF;
  background-color: #ECB30A;
  border-color: #CD9A09;
}

.settings-page .purp-theme .arkime-dropdown-menu {
  background-color: #FFFFFF;
  border-color: #EEEEEE;
  padding: 2px 0;
  font-size: .85rem;
  min-width: 12rem;
}
.settings-page .purp-theme .arkime-dropdown-menu .arkime-dropdown-item {
  color: #212529;
  padding: 2px 8px;
}
.settings-page .purp-theme .arkime-dropdown-menu .arkime-dropdown-item:hover {
  background-color: #EEEEEE;
  color: #212529;
}
.settings-page .purp-theme .arkime-dropdown-menu .arkime-dropdown-item:focus {
  background-color: #FFFFFF;
  color: #212529;
}
.settings-page .purp-theme .arkime-dropdown-menu li.active .arkime-dropdown-item {
  background-color: #830B9C;
  color: #FFFFFF;
}

.settings-page .purp-theme .field {
  color: #76207d;
}
.settings-page .purp-theme .field:hover {
  background-color: #FFFFFF;
  border-color: #EEEEEE;
}

.settings-page .purp-theme .btn {
  color: #FFFFFF !important;
}

/* blue */
.settings-page .blue-theme .preview-navbar {
  background-color: #163254;
  border-color: #000000;
}

.settings-page .blue-theme .preview-navbar-dark a:hover,
.settings-page .blue-theme .preview-navbar-dark a.active {
  background-color: #214b78;
}

.settings-page .blue-theme .display-sub-navbar {
  background-color: #DBECE7;
}

.settings-page .blue-theme .display-sub-sub-navbar {
  background-color: #FFF7E5;
}

.settings-page .blue-theme .display-sub-navbar .text-theme-accent {
  color: #9A4E93;
}

.settings-page .blue-theme .display-sub-navbar .btn-theme-primary-display {
  color: #FFFFFF;
  background-color: #214B78;
  border-color: #530763;
}
.settings-page .blue-theme .display-sub-navbar .btn-theme-secondary-display {
  color: #FFFFFF;
  background-color: #3D7B7E;
  border-color: #306264;
}
.settings-page .blue-theme .display-sub-navbar .btn-theme-tertiary-display {
  color: #FFFFFF;
  background-color: #42B7C5;
  border-color: #33919b;
}
.settings-page .blue-theme .display-sub-navbar .btn-theme-quaternary-display {
  color: #FFFFFF;
  background-color: #ECB30A;
  border-color: #CD9A09;
}

.settings-page .blue-theme .arkime-dropdown-menu {
  background-color: #FFFFFF;
  border-color: #EEEEEE;
  padding: 2px 0;
  font-size: .85rem;
  min-width: 12rem;
}
.settings-page .blue-theme .arkime-dropdown-menu .arkime-dropdown-item {
  color: #212529;
  padding: 2px 8px;
}
.settings-page .blue-theme .arkime-dropdown-menu .arkime-dropdown-item:hover {
  background-color: #EEEEEE;
  color: #212529;
}
.settings-page .blue-theme .arkime-dropdown-menu .arkime-dropdown-item:focus {
  background-color: #FFFFFF;
  color: #212529;
}
.settings-page .blue-theme .arkime-dropdown-menu li.active .arkime-dropdown-item {
  background-color: #214B78;
  color: #FFFFFF;
}

.settings-page .green-theme .field {
  color: #9A4E93;
}
.settings-page .green-theme .field:hover {
  background-color: #FFFFFF;
  border-color: #EEEEEE;
}

.settings-page .blue-theme .btn {
  color: #FFFFFF !important;
}

/* green */
.settings-page .green-theme .preview-navbar {
  background-color: #2A6E3d;
  border-color: #235A32;
}

.settings-page .green-theme .preview-navbar-dark a:hover,
.settings-page .green-theme .preview-navbar-dark a.active {
  background-color: #2a7847;
}

.settings-page .green-theme .display-sub-navbar {
  background-color: #DBECE7;
}

.settings-page .green-theme .display-sub-sub-navbar {
  background-color: #FDFFE2;
}

.settings-page .green-theme .display-sub-navbar .text-theme-accent {
  color: #38738d;
}

.settings-page .green-theme .display-sub-navbar .btn-theme-primary-display {
  color: #FFFFFF;
  background-color: #2A7847;
  border-color: #2A6E3d;
}
.settings-page .green-theme .display-sub-navbar .btn-theme-secondary-display {
  color: #FFFFFF;
  background-color: #3D7B7E;
  border-color: #306264;
}
.settings-page .green-theme .display-sub-navbar .btn-theme-tertiary-display {
  color: #FFFFFF;
  background-color: #91C563;
  border-color: #7EAA57;
}
.settings-page .green-theme .display-sub-navbar .btn-theme-quaternary-display {
  color: #FFFFFF;
  background-color: #BECF14;
  border-color: #ADBC12;
}

.settings-page .green-theme .arkime-dropdown-menu {
  background-color: #FFFFFF;
  border-color: #EEEEEE;
  padding: 2px 0;
  font-size: .85rem;
  min-width: 12rem;
}
.settings-page .green-theme .arkime-dropdown-menu .arkime-dropdown-item {
  color: #212529;
  padding: 2px 8px;
}
.settings-page .green-theme .arkime-dropdown-menu .arkime-dropdown-item:hover {
  background-color: #EEEEEE;
  color: #212529;
}
.settings-page .green-theme .arkime-dropdown-menu .arkime-dropdown-item:focus {
  background-color: #FFFFFF;
  color: #212529;
}
.settings-page .green-theme .arkime-dropdown-menu li.active .arkime-dropdown-item {
  background-color: #2A7847;
  color: #FFFFFF;
}

.settings-page .blue-theme .field {
  color: #38738d;
}
.settings-page .blue-theme .field:hover {
  background-color: #FFFFFF;
  border-color: #EEEEEE;
}

.settings-page .green-theme .btn {
  color: #FFFFFF !important;
}

/* cotton candy */
.settings-page .cotton-candy-theme .preview-navbar {
  background-color: #B0346D;
  border-color: #9B335A;
}

.settings-page .cotton-candy-theme .preview-navbar-dark a:hover,
.settings-page .cotton-candy-theme .preview-navbar-dark a.active {
  background-color: #c43d75;
}

.settings-page .cotton-candy-theme .display-sub-navbar {
  background-color: #D7F1FF;
}

.settings-page .cotton-candy-theme .display-sub-sub-navbar {
  background-color: #FFF8DD;
}

.settings-page .cotton-candy-theme .display-sub-navbar .text-theme-accent {
  color: #9A4E93;
}

.settings-page .cotton-candy-theme .display-sub-navbar .btn-theme-primary-display {
  color: #FFFFFF;
  background-color: #C43D75;
  border-color: #B0346D;
}
.settings-page .cotton-candy-theme .display-sub-navbar .btn-theme-secondary-display {
  color: #FFFFFF;
  background-color: #3CAED2;
  border-color: #389BBE;
}
.settings-page .cotton-candy-theme .display-sub-navbar .btn-theme-tertiary-display {
  color: #FFFFFF;
  background-color: #079B72;
  border-color: #077D5C;
}
.settings-page .cotton-candy-theme .display-sub-navbar .btn-theme-quaternary-display {
  color: #FFFFFF;
  background-color: #F39C12;
  border-color: #D78A10;
}

.settings-page .cotton-candy-theme .arkime-dropdown-menu {
  background-color: #FFFFFF;
  border-color: #EEEEEE;
  padding: 2px 0;
  font-size: .85rem;
  min-width: 12rem;
}
.settings-page .cotton-candy-theme .arkime-dropdown-menu .arkime-dropdown-item {
  color: #212529;
  padding: 2px 8px;
}
.settings-page .cotton-candy-theme .arkime-dropdown-menu .arkime-dropdown-item:hover {
  background-color: #EEEEEE;
  color: #212529;
}
.settings-page .cotton-candy-theme .arkime-dropdown-menu .arkime-dropdown-item:focus {
  background-color: #FFFFFF;
  color: #212529;
}
.settings-page .cotton-candy-theme .arkime-dropdown-menu li.active .arkime-dropdown-item {
  background-color: #C43D75;
  color: #FFFFFF;
}

.settings-page .cotton-candy-theme .field {
  color: #9A4E93;
}
.settings-page .cotton-candy-theme .field:hover {
  background-color: #FFFFFF;
  border-color: #EEEEEE;
}

.settings-page .cotton-candy-theme .btn {
  color: #FFFFFF !important;
}

/* green on black */
.settings-page .dark-2-theme .preview-navbar {
  background-color: #363A7D;
  border-color: #2F2F5F;
}

.settings-page .dark-2-theme .preview-navbar-dark a:hover,
.settings-page .dark-2-theme .preview-navbar-dark a.active {
  background-color: #444a9b;
}

.settings-page .dark-2-theme .display-sub-navbar {
  background-color: #0F2237;
}

.settings-page .dark-2-theme .display-sub-sub-navbar {
  background-color: #191919;
}

.settings-page .dark-2-theme .display-sub-navbar .text-theme-accent {
  color: #00CA16;
}

.settings-page .dark-2-theme .display-sub-navbar .btn-theme-primary-display {
  color: #FFFFFF;
  background-color: #444A9B;
  border-color: #363A7D;
}
.settings-page .dark-2-theme .display-sub-navbar .btn-theme-secondary-display {
  color: #FFFFFF;
  background-color: #2E5D9B;
  border-color: #264B7E;
}
.settings-page .dark-2-theme .display-sub-navbar .btn-theme-tertiary-display {
  color: #FFFFFF;
  background-color: #00BB20;
  border-color: #009D1D;
}
.settings-page .dark-2-theme .display-sub-navbar .btn-theme-quaternary-display {
  color: #FFFFFF;
  background-color: #686868;
  border-color: #4B4B4B;
}

.settings-page .dark-2-theme .arkime-dropdown-menu {
  background-color: #222222;
  border-color: #555555;
  padding: 2px 0;
  font-size: .85rem;
  min-width: 12rem;
}
.settings-page .dark-2-theme .arkime-dropdown-menu .arkime-dropdown-item {
  color: #C7C7C7;
  padding: 2px 8px;
}
.settings-page .dark-2-theme .arkime-dropdown-menu .arkime-dropdown-item:hover {
  background-color: #555555;
  color: #C7C7C7;
}
.settings-page .dark-2-theme .arkime-dropdown-menu .arkime-dropdown-item:focus {
  background-color: #222222;
  color: #C7C7C7;
}
.settings-page .dark-2-theme .arkime-dropdown-menu li.active .arkime-dropdown-item {
  background-color: #444A9B;
  color: #333333;
}

.settings-page .dark-2-theme .field {
  color: #00CA16;
}
.settings-page .dark-2-theme .field:hover {
  background-color: #222222;
  border-color: #555555;
}

.settings-page .dark-2-theme .btn {
  color: #FFFFFF !important;
}

/* Dark Blue */
.settings-page .dark-3-theme .preview-navbar {
  background-color: #9F9F9F;
  border-color: #C3C3C3;
}

.settings-page .dark-3-theme .preview-navbar-dark a:hover,
.settings-page .dark-3-theme .preview-navbar-dark a.active {
  background-color: #8A8A8A;
}

.settings-page .dark-3-theme .preview-navbar-dark li.active a {
  color: #FFFFFF;
  background-color: #C3C3C3 !important;
}

.settings-page .dark-3-theme .display-sub-navbar {
  background-color: #124B47;
}

.settings-page .dark-3-theme .display-sub-sub-navbar {
  background-color: #460C3A;
}
.settings-page .dark-3-theme .display-sub-navbar .text-theme-accent{
  color: #A6A8E2;
}

.settings-page .dark-3-theme .display-sub-navbar .btn-theme-primary-display {
  color: #FFFFFF;
  background-color: #8A8A8A;
  border-color: #9F9F9F;
}
.settings-page .dark-3-theme .display-sub-navbar .btn-theme-secondary-display {
  color: #FFFFFF;
  background-color: #2AA198;
  border-color: #23837B;
}
.settings-page .dark-3-theme .display-sub-navbar .btn-theme-tertiary-display {
  color: #FFFFFF;
  background-color: #268BD2;
  border-color: #1F76B4;
}
.settings-page .dark-3-theme .display-sub-navbar .btn-theme-quaternary-display {
  color: #FFFFFF;
  background-color: #D33682;
  border-color: #B42C72;
}

.settings-page .dark-3-theme .arkime-dropdown-menu {
  background-color: #002833;
  border-color: #555555;
  padding: 2px 0;
  font-size: .85rem;
  min-width: 12rem;
}
.settings-page .dark-3-theme .arkime-dropdown-menu .arkime-dropdown-item {
  color: #ADC1C3;
  padding: 2px 8px;
}
.settings-page .dark-3-theme .arkime-dropdown-menu .arkime-dropdown-item:hover {
  background-color: #555555;
  color: #ADC1C3;
}
.settings-page .dark-3-theme .arkime-dropdown-menu .arkime-dropdown-item:focus {
  background-color: #002833;
  color: #ADC1C3;
}
.settings-page .dark-3-theme .arkime-dropdown-menu li.active .arkime-dropdown-item {
  background-color: #8A8A8A;
  color: #333333;
}

.settings-page .dark-3-theme .field {
  color: #A6A8E2;
}
.settings-page .dark-3-theme .field:hover {
  background-color: #002833;
  border-color: #555555;
}

.settings-page .dark-3-theme .btn {
  color: #FFFFFF !important;
}

/* Custom */
.settings-page .custom-theme .theme-display {
  background-color: var(--color-background);
  color: var(--color-foreground);
  -webkit-box-shadow: 0 0 16px -2px black;
     -moz-box-shadow: 0 0 16px -2px black;
          box-shadow: 0 0 16px -2px black;
}

.settings-page .custom-theme .preview-navbar {
  background-color: var(--color-primary-dark);
  border-color: var(--color-primary-darker);
}
.settings-page .custom-theme .preview-navbar a.preview-nav-item {
  color: var(--color-button);
}
.settings-page .custom-theme .preview-navbar a.preview-nav-item:hover,
.settings-page .custom-theme .preview-navbar a.preview-nav-item.active {
  background-color: var(--color-primary);
}

.settings-page .custom-theme .display-sub-navbar {
  background-color: var(--color-secondary-lightest);
}

.settings-page .custom-theme .display-sub-sub-navbar {
  border-radius: 0;
  height: 46px;
  background-color: var(--color-quaternary-lightest);
  -webkit-box-shadow: 0 0 16px -2px black;
     -moz-box-shadow: 0 0 16px -2px black;
          box-shadow: 0 0 16px -2px black;
}

.settings-page .custom-theme .display-sub-navbar .btn-theme-primary-display {
  color: #FFFFFF;
  background-color: var(--color-primary);
  border-color: var(--color-primary-dark);
}
.settings-page .custom-theme .display-sub-navbar .btn-theme-secondary-display {
  color: #FFFFFF;
  background-color: var(--color-secondary);
  border-color: var(--color-secondary-dark);
}
.settings-page .custom-theme .display-sub-navbar .btn-theme-tertiary-display {
  color: #FFFFFF;
  background-color: var(--color-tertiary);
  border-color: var(--color-tertiary-dark);
}
.settings-page .custom-theme .display-sub-navbar .btn-theme-quaternary-display {
  color: #FFFFFF;
  background-color: var(--color-quaternary);
  border-color: var(--color-quaternary-dark);
}

.settings-page .custom-theme .arkime-dropdown-menu {
  color: var(--color-foreground);
  background-color: var(--color-background, #FFFFFF);
  border-color: var(--color-gray-light);
}

.settings-page .custom-theme .field {
  color: var(--color-foreground-accent);
}
.settings-page .custom-theme .field:hover {
  z-index: 4;
  background-color: var(--color-white);
  border: 1px solid var(--color-gray-light);
}

.settings-page .custom-theme .session-detail-ts {
  color: var(--color-foreground-accent);
  padding: 0 4px;
}
.settings-page .custom-theme .sessionsrc > pre {
  color: var(--color-src, #CA0404);
  padding: 2px 4px;
}
.settings-page .custom-theme .sessiondst > pre {
  color: var(--color-dst, #0000FF);
  padding: 2px 4px;
}
</style>
