<!--
Copyright Yahoo Inc.
SPDX-License-Identifier: Apache-2.0
-->
<template>
  <!-- settings content -->
  <div class="settings-page">
    <!-- messages (success/error) displayed at bottom of page -->
    <v-snackbar
      v-model="showMessage"
      :color="vuetifyMsgType"
      location="bottom"
      timeout="-1"
      variant="flat">
      {{ msg }}
      <template #actions>
        <v-btn
          variant="text"
          icon="$close"
          @click="showMessage = false" />
      </template>
    </v-snackbar> <!-- /messages -->

    <!-- sub navbar -->
    <div class="sub-navbar">
      <span class="sub-navbar-title">
        <v-icon
          icon="mdi-cog"
          class="me-1" />
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
      no-gutters
      class="settings-content"
      v-if="!loading && !error && settings">
      <!-- navigation -->
      <v-col
        cols="12"
        xl="1"
        lg="2"
        md="2"
        sm="3"
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
            <v-icon
              icon="mdi-cog"
              class="me-1" />
            {{ $t('settings.nav.general') }}
          </v-tab>
          <v-tab value="col">
            <v-icon
              icon="mdi-view-column"
              class="me-1" />
            {{ $t('settings.nav.columnLayout') }}
          </v-tab>
          <v-tab value="info">
            <v-icon
              icon="mdi-information"
              class="me-1" />
            {{ $t('settings.nav.infoFieldLayout') }}
          </v-tab>
          <v-tab value="spiview">
            <v-icon
              icon="mdi-eyedropper"
              class="me-1" />
            {{ $t('settings.nav.spiViewLayout') }}
          </v-tab>
          <v-tab value="theme">
            <v-icon
              icon="mdi-brush"
              class="me-1" />
            {{ $t('settings.nav.themes') }}
          </v-tab>
          <v-tab
            v-if="(!multiviewer || hasUsersES) && !disablePassword"
            value="password">
            <v-icon
              icon="mdi-lock"
              class="me-1" />
            {{ $t('settings.nav.password') }}
          </v-tab>
          <v-tab
            v-if="!multiviewer || hasUsersES"
            v-has-role="{user:user,roles:'arkimeAdmin,cont3xtAdmin,wiseAdmin'}"
            value="totp">
            <v-icon
              icon="mdi-two-factor-authentication"
              class="me-1" />
            {{ $t('settings.totp.title') }}
          </v-tab>
          <v-divider class="my-1" />
          <v-tab value="views">
            <v-icon
              icon="mdi-eye"
              class="me-1" />
            {{ $t('settings.nav.views') }}
          </v-tab>
          <v-tab
            v-if="!multiviewer || hasUsersES"
            value="shortcuts">
            <v-icon
              icon="mdi-format-list-bulleted"
              class="me-1" />
            {{ $t('settings.nav.shortcuts') }}
          </v-tab>
          <v-tab
            v-if="!multiviewer"
            value="cron">
            <v-icon
              icon="mdi-magnify"
              class="me-1" />
            {{ $t('settings.nav.cron') }}
          </v-tab>
          <v-tab
            v-has-role="{user:user,roles:'arkimeAdmin'}"
            value="notifiers">
            <v-icon
              icon="mdi-bell"
              class="me-1" />
            {{ $t('settings.nav.notifiers') }}
          </v-tab>
          <v-tab
            v-has-role="{user:user,roles:'arkimeAdmin'}"
            value="banner">
            <v-icon
              icon="mdi-bullhorn"
              class="me-1" />
            {{ $t('settings.nav.banner') }}
          </v-tab>
        </v-tabs>
      </v-col> <!-- /navigation -->

      <v-col
        cols="12"
        xl="11"
        lg="10"
        md="10"
        sm="9"
        class="settings-right-panel settings-content-pane">
        <!-- general settings -->
        <form
          v-if="visibleTab === 'general'"
          id="general">
          <h3 class="d-flex align-center">
            <span class="flex-grow-1">{{ $t('settings.general.title') }}</span>
            <v-btn
              variant="flat"
              size="large"
              color="warning"
              @click="resetSettings">
              <v-icon
                icon="mdi-repeat"
                class="me-2" />
              {{ $t('settings.general.reset') }}
            </v-btn>
          </h3>

          <hr>

          <!-- timezone -->
          <v-row>
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
          <v-row>
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
          <v-row>
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
          <v-row>
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
          <v-row>
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
          <v-row>
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
            v-if="fields && settings.spiGraph">
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
            v-if="fields && settings.connSrcField">
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
            v-if="fields && settings.connDstField">
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
            v-if="integerFields">
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
                    <v-icon icon="mdi-close" />
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
                  <v-icon icon="mdi-refresh" />
                  <v-tooltip activator="#resetTimelineFilters">
                    {{ $t('settings.general.resetTimelineDataFilters') }}
                  </v-tooltip>
                </v-btn>
              </h4>
            </v-col>
          </v-row>

          <!-- hide tags field -->
          <v-row
            v-if="fields">
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
                      <v-icon
                        icon="mdi-trash-can-outline"
                        class="me-1" />
                      {{ $t('common.delete') }}
                    </v-btn>
                  </td>
                </tr>
              </template> <!-- /col configs -->
              <!-- col config list error -->
              <tr v-if="colConfigError">
                <td colspan="3">
                  <p class="text-danger mb-0">
                    <v-icon icon="mdi-alert" />&nbsp;
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
                      <v-icon
                        icon="mdi-trash-can-outline"
                        class="me-1" />
                      {{ $t('common.delete') }}
                    </v-btn>
                  </td>
                </tr>
              </template> <!-- /info field configs -->
              <!-- info field config list error -->
              <tr v-if="infoFieldLayoutError">
                <td colspan="3">
                  <p class="text-danger mb-0">
                    <v-icon icon="mdi-alert" />&nbsp;
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
                      <v-icon
                        icon="mdi-trash-can-outline"
                        class="me-1" />
                      {{ $t('common.delete') }}
                    </v-btn>
                  </td>
                </tr>
              </template> <!-- /spiview field configs -->
              <!-- spiview field config list error -->
              <tr v-if="spiviewConfigError">
                <td colspan="3">
                  <p class="text-danger mb-0">
                    <v-icon icon="mdi-alert" />&nbsp;
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
          <h1 class="mb-3">
            Themes
          </h1>
          <p class="text-medium-emphasis mb-4">
            Choose a theme or build your own. Themes are saved to your
            account and apply across all Arkime apps.
          </p>

          <!-- theme picker (incl. Custom card) -->
          <ThemePicker
            :model-value="activeThemeId"
            :themes="themes"
            :custom-theme="settings.vuetifyCustomTheme || settings.customTheme || null"
            @update:model-value="changeTheme"
            @update:custom-theme="onCustomThemeChange" />

          <!-- logo picker -->
          <hr>
          <h3>{{ $t('settings.themes.logos') }}</h3>
          <p>{{ $t('settings.themes.pickLogo') }}</p>
          <v-row class="well logo-well me-1 ms-1 mb-6">
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
        </form> <!-- /theme settings -->

        <!-- password settings -->
        <form
          v-if="visibleTab === 'password' && (!multiviewer || hasUsersES) && !disablePassword"
          @keyup.enter="changePassword"
          id="password">
          <h3>{{ $t('settings.password.title') }}</h3>

          <hr>

          <!-- current password -->
          <v-row
            v-if="!userId">
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
          <v-row>
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
          <v-row>
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
          <v-row>
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
                <v-icon icon="mdi-alert" />&nbsp;
                {{ changePasswordError }}
              </span>
            </v-col>
          </v-row> <!-- /change password button/error -->
        </form> <!-- /password settings -->

        <!-- two-factor (TOTP) settings -->
        <form
          v-if="visibleTab === 'totp' && (!multiviewer || hasUsersES)"
          v-has-role="{user:user,roles:'arkimeAdmin,cont3xtAdmin,wiseAdmin'}"
          id="totp">
          <h3 class="d-flex align-center">
            <span class="me-2">{{ $t('settings.totp.title') }}</span>
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
              <v-icon icon="mdi-check" /> {{ $t('settings.totp.enabled') }}
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
          </h3>

          <hr>

          <p class="mb-3">
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
                style="border: 1px solid rgb(var(--v-theme-foreground)); background-color: rgb(var(--v-theme-background));">
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
              <v-icon icon="mdi-alert" />&nbsp;
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
              <v-icon icon="mdi-alert" />&nbsp;
              {{ totpError }}
            </span>
          </div>
        </form> <!-- /two-factor settings -->

        <!-- notifiers settings -->
        <Notifiers
          id="notifiers"
          parent-app="arkime"
          @display-message="displayMessage"
          v-if="visibleTab === 'notifiers'"
          v-has-role="{user:user,roles:'arkimeAdmin'}"
          help-intl-id="settings.notifiers.helpViewer" />

        <!-- banner settings -->
        <BannerSettings
          id="banner"
          v-if="visibleTab === 'banner'"
          v-has-role="{user:user,roles:'arkimeAdmin'}" />

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
import ThemePicker from '@common/ThemePicker.vue';
import { THEMES } from '@common/themes/manifest.js';
import { registerVuetifyTheme } from '@common/themes/registerVuetifyTheme.js';
import Utils from '../utils/utils';
import PeriodicQueries from './PeriodicQueries.vue';
import Shortcuts from './Shortcuts.vue';
import BannerSettings from '@common/BannerSettings.vue';
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
    ThemePicker,
    PeriodicQueries,
    Shortcuts,
    Notifiers,
    BannerSettings,
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
      themes: THEMES, // from common/vueapp/themes/manifest.js
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
  computed: {
    user: function () {
      return this.$store.state.user;
    },
    // Vuetify v-alert accepts: success | info | warning | error.
    // Map Bootstrap-flavored"danger" → Vuetify"error".
    vuetifyMsgType: function () {
      return this.msgType === 'danger' ? 'error' : (this.msgType || 'success');
    },
    /* The id that ThemePicker should show as selected. Normalizes
       legacy '...-theme' suffix strings to bare manifest ids; falls
       back to the manifest default for unknown values. */
    activeThemeId: function () {
      // Prefer the v7 key; fall back to the legacy key for users who
      // haven't yet had applySavedTheme() promote their preference.
      const raw = (this.settings && (this.settings.vuetifyTheme || this.settings.theme)) || '';
      if (raw.startsWith('custom1')) { return 'custom1'; }
      return raw.replace(/-theme$/, '') || 'arkime-light';
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
        tab === 'spiview' || tab === 'notifiers' || tab === 'banner' || tab === 'shortcuts' ||
        tab === 'totp') {
        this.visibleTab = tab;
      }

      if ((tab === 'password' && this.multiviewer) || (tab === 'cron' && this.multiviewer) ||
        (tab === 'totp' && this.multiviewer && !this.hasUsersES)) {
        // multiviewer user can't change password or configure periodic queries
        this.openView('general');
      }
    }

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
      UserService.resetSettings(this.userId, this.settings.vuetifyTheme || this.settings.theme).then((response) => {
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
      // Default to the arkime-light theme if the user has not set one.
      // Prefer the v7 key; if the user only has a legacy `theme` value
      // (e.g. they've never set anything in v7), seed the v7 key from
      // it -- normalizing any legacy '...-theme' suffix. We DO NOT
      // mutate settings.theme so legacy arkime keeps reading it intact.
      if (!this.settings.vuetifyTheme) {
        const legacy = typeof this.settings.theme === 'string'
          ? this.settings.theme.replace(/-theme$/, '')
          : '';
        this.settings.vuetifyTheme = legacy || 'arkime-light';
      }
      if (!this.settings.logo) {
        this.settings.logo = 'assets/Arkime_Logo_Mark_White.png';
      }
    },
    changeTheme: function (newThemeId) {
      this.settings.vuetifyTheme = newThemeId;
      this.$vuetify.theme.change(newThemeId);
      this.update();
    },
    onCustomThemeChange: function (newCustomTheme) {
      if (!newCustomTheme || typeof newCustomTheme.colors !== 'object' || !newCustomTheme.colors) return;
      const safe = {
        dark: !!newCustomTheme.dark,
        colors: { ...newCustomTheme.colors }
      };
      registerVuetifyTheme(this.$vuetify, 'custom1', safe);
      this.settings.vuetifyCustomTheme = safe;
      if (this.settings.vuetifyTheme !== 'custom1') {
        this.settings.vuetifyTheme = 'custom1';
        this.$vuetify.theme.change('custom1');
      }
      this.update();
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
          this.settings.manualQuery = 'false';
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
  margin-left: 0;
  margin-right: 0;
  overflow-x: hidden;
}

/* Tighten the vertical tab strip. Vuetify's v-tab default sits at
   ~48px (compact density: 36px) which feels loose for a side nav. */
.settings-content .v-tab {
  min-height: 28px !important;
  height: 28px !important;
  padding: 0 12px !important;
  font-size: 0.85rem !important;
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
  background-color: rgb(var(--v-theme-neutral-lighter));
  border: 1px solid rgb(var(--v-theme-neutral-light));
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

</style>
