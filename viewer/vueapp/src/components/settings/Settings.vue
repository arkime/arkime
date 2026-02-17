<!--
Copyright Yahoo Inc.
SPDX-License-Identifier: Apache-2.0
-->
<template>
  <!-- settings content -->
  <div class="settings-page">
    <!-- messages (success/error) displayed at bottom of page -->
    <div
      v-if="showMessage"
      style="z-index: 2000;"
      :class="`alert-${msgType}`"
      class="alert position-fixed fixed-bottom m-0 rounded-0">
      {{ msg }}
      <button
        type="button"
        class="btn-close pull-right"
        @click="showMessage = false" />
    </div> <!-- /messages -->

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
    <div
      class="settings-content row"
      v-if="!loading && !error && settings">
      <!-- navigation -->
      <div
        class="col-xl-2 col-lg-3 col-md-3 col-sm-4 col-xs-12"
        role="tablist"
        aria-orientation="vertical">
        <div class="nav flex-column nav-pills">
          <a
            class="nav-link cursor-pointer"
            @click="openView('general')"
            :class="{'active':visibleTab === 'general'}">
            <span class="fa fa-fw fa-cog" />&nbsp;
            {{ $t('settings.nav.general') }}
          </a>
          <a
            class="nav-link cursor-pointer"
            @click="openView('col')"
            :class="{'active':visibleTab === 'col'}">
            <span class="fa fa-fw fa-columns" />&nbsp;
            {{ $t('settings.nav.columnLayout') }}
          </a>
          <a
            class="nav-link cursor-pointer"
            @click="openView('info')"
            :class="{'active':visibleTab === 'info'}">
            <span class="fa fa-fw fa-info" />&nbsp;
            {{ $t('settings.nav.infoFieldLayout') }}
          </a>
          <a
            class="nav-link cursor-pointer"
            @click="openView('spiview')"
            :class="{'active':visibleTab === 'spiview'}">
            <span class="fa fa-fw fa-eyedropper" />&nbsp;
            {{ $t('settings.nav.spiViewLayout') }}
          </a>
          <a
            class="nav-link cursor-pointer"
            @click="openView('theme')"
            :class="{'active':visibleTab === 'theme'}">
            <span class="fa fa-fw fa-paint-brush" />&nbsp;
            {{ $t('settings.nav.themes') }}
          </a>
          <a
            v-if="(!multiviewer || hasUsersES) && !disablePassword"
            class="nav-link cursor-pointer"
            @click="openView('password')"
            :class="{'active':visibleTab === 'password'}">
            <span class="fa fa-fw fa-lock" />&nbsp;
            {{ $t('settings.nav.password') }}
          </a>
          <hr class="hr-small nav-separator">
          <a
            class="nav-link cursor-pointer"
            @click="openView('views')"
            :class="{'active':visibleTab === 'views'}">
            <span class="fa fa-fw fa-eye" />&nbsp;
            {{ $t('settings.nav.views') }}
          </a>
          <a
            v-if="!multiviewer || hasUsersES"
            class="nav-link cursor-pointer"
            @click="openView('shortcuts')"
            :class="{'active':visibleTab === 'shortcuts'}">
            <span class="fa fa-fw fa-list" />&nbsp;
            {{ $t('settings.nav.shortcuts') }}
          </a>
          <a
            v-if="!multiviewer"
            class="nav-link cursor-pointer"
            @click="openView('cron')"
            :class="{'active':visibleTab === 'cron'}">
            <span class="fa fa-fw fa-search" />&nbsp;
            {{ $t('settings.nav.cron') }}
          </a>
          <a
            class="nav-link cursor-pointer"
            v-has-role="{user:user,roles:'arkimeAdmin'}"
            @click="openView('notifiers')"
            :class="{'active':visibleTab === 'notifiers'}">
            <span class="fa fa-fw fa-bell" />&nbsp;
            {{ $t('settings.nav.notifiers') }}
          </a>
        </div>
      </div> <!-- /navigation -->

      <div class="col-xl-10 col-lg-9 col-md-9 col-sm-8 col-xs-12 settings-right-panel">
        <!-- general settings -->
        <form
          class="form-horizontal"
          v-if="visibleTab === 'general'"
          id="general">
          <h3>
            {{ $t('settings.general.title') }}
            <button
              type="button"
              @click="resetSettings"
              class="btn btn-theme-quaternary btn-sm pull-right ms-1">
              <span class="fa fa-repeat me-2" />
              {{ $t('settings.general.reset') }}
            </button>
          </h3>

          <hr>

          <!-- timezone -->
          <div class="form-group row">
            <label class="col-sm-3 col-form-label text-end fw-bold">
              {{ $t('settings.general.timezoneFormat') }}
            </label>
            <div class="col-sm-9">
              <BFormRadioGroup
                buttons
                size="sm"
                class="d-inline me-2"
                button-variant="outline-secondary"
                :model-value="settings.timezone"
                @update:model-value="updateTimezone"
                :options="[
                  { text: $t('settings.general.tz-local'), value: 'local' },
                  { text: $t('settings.general.tz-localtz'), value: 'localtz' },
                  { text: $t('settings.general.tz-gmt'), value: 'gmt' }
                ]" />
              <BFormCheckbox
                button
                size="sm"
                class="d-inline"
                id="millisecondsSetting"
                :active="settings.ms"
                :model-value="settings.ms"
                @update:model-value="updateMs"
                button-variant="outline-secondary">
                {{ $t('common.milliseconds') }}
                <BTooltip target="millisecondsSetting">
                  {{ $t('settings.general.millisecondsSettingTip') }}
                </BTooltip>
              </BFormCheckbox>
              <label class="ms-2 fw-bold text-theme-primary align-bottom">
                {{ timezoneDateString(date, settings.timezone, settings.ms) }}
              </label>
            </div>
          </div> <!-- /timezone -->

          <!-- session detail format -->
          <div class="form-group row">
            <label class="col-sm-3 col-form-label text-end fw-bold">
              {{ $t('settings.general.sessionDetailFormat') }}
            </label>
            <div class="col-sm-9">
              <BFormRadioGroup
                buttons
                size="sm"
                class="d-inline"
                button-variant="outline-secondary"
                :model-value="settings.detailFormat"
                @update:model-value="updateSessionDetailFormat"
                :options="[
                  { text: $t('settings.general.lastUsed'), value: 'last' },
                  { text: $t('settings.general.detail-natural'), value: 'natural' },
                  { text: $t('settings.general.detail-ascii'), value: 'ascii' },
                  { text: $t('settings.general.detail-utf8'), value: 'utf8' },
                  { text: $t('settings.general.detail-hex'), value: 'hex' }
                ]" />
            </div>
          </div> <!-- /session detail format -->

          <!-- number of packets -->
          <div class="form-group row">
            <label class="col-sm-3 col-form-label text-end fw-bold">
              {{ $t('settings.general.numberOfPackets') }}
            </label>
            <div class="col-sm-9">
              <BFormRadioGroup
                buttons
                size="sm"
                class="d-inline"
                button-variant="outline-secondary"
                :model-value="settings.numPackets"
                @update:model-value="updateNumberOfPackets"
                :options="[
                  { text: $t('settings.general.lastUsed'), value: 'last' },
                  { text: '50', value: '50' },
                  { text: '200', value: '200' },
                  { text: '500', value: '500' },
                  { text: '1,000', value: '1000' },
                  { text: '2,000', value: '2000' }
                ]" />
            </div>
          </div> <!-- /number of packets -->

          <!-- show packet timestamp -->
          <div class="form-group row">
            <label class="col-sm-3 col-form-label text-end fw-bold">
              {{ $t('settings.general.showPacketInfo') }}
            </label>
            <div class="col-sm-9">
              <BFormRadioGroup
                buttons
                size="sm"
                class="d-inline"
                button-variant="outline-secondary"
                :model-value="settings.showTimestamps"
                @update:model-value="updateShowPacketTimestamps"
                :options="[
                  { text: $t('settings.general.lastUsed'), value: 'last' },
                  { text: $t('settings.general.info-on'), value: 'on' },
                  { text: $t('settings.general.info-off'), value: 'off' }
                ]" />
            </div>
          </div> <!-- /show packet timestamp -->

          <!-- issue query on initial page load -->
          <div class="form-group row">
            <label class="col-sm-3 col-form-label text-end fw-bold">
              {{ $t('settings.general.queryOnLoad') }}
            </label>
            <div class="col-sm-9">
              <BFormRadioGroup
                buttons
                size="sm"
                class="d-inline"
                button-variant="outline-secondary"
                :model-value="settings.manualQuery"
                @update:model-value="updateQueryOnPageLoad"
                :options="[
                  { text: $t('settings.general.lastUsed'), value: 'last' },
                  { text: $t('settings.general.query-false'), value: 'false' },
                  { text: $t('settings.general.query-true'), value: 'true' }
                ]" />
            </div>
          </div> <!-- /issue query on initial page load -->

          <!-- session sort -->
          <div class="form-group row">
            <label class="col-sm-3 col-form-label text-end fw-bold">
              {{ $t('settings.general.sortSessionsBy') }}
            </label>
            <div class="col-sm-6">
              <select
                size="sm"
                class="form-select form-select-sm"
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
            <div class="col-sm-3">
              <BFormRadioGroup
                buttons
                size="sm"
                class="d-inline"
                button-variant="outline-secondary"
                v-if="settings.sortColumn !== 'last'"
                :model-value="settings.sortDirection"
                @update:model-value="updateSortDirection"
                :options="[
                  { text: $t('settings.general.sort-asc'), value: 'asc' },
                  { text: $t('settings.general.sort-desc'), value: 'desc' }
                ]" />
            </div>
          </div> <!-- /session sort -->

          <!-- default spi graph -->
          <div
            v-if="fields && settings.spiGraph"
            class="form-group row">
            <label class="col-sm-3 col-form-label text-end fw-bold">
              {{ $t('settings.general.defaultSPIGraph') }}
            </label>
            <div class="col-sm-6">
              <arkime-field-typeahead
                :dropup="true"
                :fields="fields"
                query-param="field"
                :initial-value="spiGraphTypeahead"
                @field-selected="spiGraphFieldSelected" />
            </div>
            <div class="col-sm-3">
              <h4 v-if="spiGraphField">
                <label
                  id="spiGraphFieldSetting"
                  class="badge bg-info cursor-help">
                  {{ spiGraphTypeahead || 'unknown field' }}
                  <BTooltip target="spiGraphFieldSetting">{{ spiGraphField.help }}</BTooltip>
                </label>
              </h4>
            </div>
          </div> <!-- /default spi graph -->

          <!-- connections src field -->
          <div
            v-if="fields && settings.connSrcField"
            class="form-group row">
            <label class="col-sm-3 col-form-label text-end fw-bold">
              {{ $t('settings.general.connectionsSrc') }}
            </label>
            <div class="col-sm-6">
              <arkime-field-typeahead
                :dropup="true"
                :fields="fields"
                query-param="field"
                :initial-value="connSrcFieldTypeahead"
                @field-selected="connSrcFieldSelected" />
            </div>
            <div class="col-sm-3">
              <h4 v-if="connSrcField">
                <label
                  class="badge bg-info cursor-help"
                  id="connSrcFieldSetting">
                  {{ connSrcFieldTypeahead || 'unknown field' }}
                  <BTooltip target="connSrcFieldSetting">{{ connSrcField.help }}</BTooltip>
                </label>
              </h4>
            </div>
          </div> <!-- /connections src field -->

          <!-- connections dst field -->
          <div
            v-if="fields && settings.connDstField"
            class="form-group row">
            <label class="col-sm-3 col-form-label text-end fw-bold">
              {{ $t('settings.general.connectionsDst') }}
            </label>
            <div class="col-sm-6">
              <arkime-field-typeahead
                :dropup="true"
                :fields="fields"
                query-param="field"
                :initial-value="connDstFieldTypeahead"
                @field-selected="connDstFieldSelected" />
            </div>
            <div class="col-sm-3">
              <h4 v-if="connDstField">
                <label
                  class="badge bg-info cursor-help"
                  id="connDstFieldSetting">
                  {{ connDstFieldTypeahead || 'unknown field' }}
                  <BTooltip target="connDstFieldSetting">{{ connDstField.help }}</BTooltip>
                </label>
              </h4>
            </div>
          </div> <!-- /connections dst field -->

          <div
            v-if="integerFields"
            class="form-group row">
            <label class="col-sm-3 col-form-label text-end fw-bold">
              {{ $t('settings.general.timelineDataFilters') }}
            </label>

            <div class="col-sm-6">
              <arkime-field-typeahead
                :dropup="true"
                :fields="integerFields"
                :initial-value="filtersTypeahead"
                query-param="field"
                @field-selected="timelineFilterSelected" />
            </div>
            <div class="col-sm-3">
              <h4 v-if="timelineDataFilters.length > 0">
                <label
                  class="badge bg-info cursor-help small-badge"
                  v-for="filter in timelineDataFilters"
                  :key="filter.dbField + 'DataFilterBadge'"
                  @click="timelineFilterSelected(filter)"
                  :id="filter.dbField + 'DataFilterBadge'">
                  <span class="fa fa-times" />
                  {{ filter.friendlyName || 'unknown field' }}
                  <BTooltip :target="filter.dbField + 'DataFilterBadge'">{{ filter.help }}</BTooltip>
                </label>
              </h4>
              <b-button
                size="sm"
                variant="danger"
                @click="resetDefaultFilters">
                {{ $t('settings.general.resetTimelineDataFilters') }}
              </b-button>
            </div>
          </div>

          <!-- hide tags field -->
          <div
            v-if="fields"
            class="form-group row">
            <label class="col-sm-3 col-form-label text-end fw-bold">
              {{ $t('settings.general.hideTags') }}
            </label>
            <div class="col-sm-6">
              <input
                type="text"
                @change="update"
                v-model="settings.hideTags"
                class="form-control form-control-sm"
                :placeholder="$t('settings.general.hideTagsPlaceholder')">
            </div>
          </div> <!-- /hide tags field -->
        </form>

        <!-- col configs settings -->
        <form
          v-if="visibleTab === 'col'"
          class="form-horizontal"
          id="col">
          <h3>{{ $t('settings.ccl.title') }}</h3>

          <p>{{ $t('settings.ccl.info') }}</p>

          <table class="table table-striped table-sm">
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
                      class="badge bg-secondary me-1 help-cursor"
                      :id="`${col}DefaultColConfigSetting`"
                      v-if="fieldsMap[col]">
                      {{ fieldsMap[col].friendlyName }}
                      <BTooltip :target="`${col}DefaultColConfigSetting`">{{ fieldsMap[col].help }}</BTooltip>
                    </label>
                  </template>
                </td>
                <td>
                  <span
                    v-for="order in defaultColConfig.order"
                    :key="order[0]">
                    <label
                      class="badge bg-secondary me-1 help-cursor"
                      v-if="fieldsMap[order[0]]"
                      :id="`${order[0]}DefaultColConfigSetting`">
                      {{ fieldsMap[order[0]].friendlyName }}&nbsp;
                      ({{ order[1] }})
                      <BTooltip :target="`${order[0]}DefaultColConfigSetting`">{{ fieldsMap[order[0]].help }}</BTooltip>
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
                        class="badge bg-secondary me-1 help-cursor"
                        v-if="fieldsMap[col]"
                        :id="`${index}${col}ColConfigSetting`">
                        {{ fieldsMap[col].friendlyName }}
                        <BTooltip :target="`${index}${col}ColConfigSetting`">{{ fieldsMap[col].help }}</BTooltip>
                      </label>
                    </template>
                  </td>
                  <td>
                    <span
                      v-for="order in config.order"
                      :key="order[0]">
                      <label
                        class="badge bg-secondary me-1 help-cursor"
                        v-if="fieldsMap[order[0]]"
                        :id="`${index}-${order[0]}ColConfigSetting`">
                        {{ fieldsMap[order[0]].friendlyName }}&nbsp;
                        ({{ order[1] }})
                        <BTooltip :target="`${index}-${order[0]}ColConfigSetting`">{{ fieldsMap[order[0]].help }}</BTooltip>
                      </label>
                    </span>
                  </td>
                  <td>
                    <button
                      type="button"
                      class="btn btn-sm btn-danger pull-right"
                      @click="deleteLayout('sessionstable', config.name, 'colConfigs', index)"
                      :title="$t('settings.ccl.deleteTip')">
                      <span class="fa fa-trash-o" />&nbsp;
                      {{ $t('common.delete') }}
                    </button>
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

          <div
            v-if="!colConfigs || !colConfigs.length"
            class="alert alert-info">
            <span class="fa fa-info-circle fa-lg" />
            <strong>
              {{ $t('settings.ccl.empty') }}
            </strong>
            <br>
            <br>
            <span v-html="$t('settings.ccl.howToHtml')" />
          </div>
        </form> <!-- /col configs settings -->

        <!-- info field configs settings -->
        <form
          v-if="visibleTab === 'info'"
          class="form-horizontal"
          id="col">
          <h3>{{ $t('settings.infoLayout.title') }}</h3>

          <p>{{ $t('settings.infoLayout.info') }}</p>

          <table class="table table-striped table-sm">
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
                      class="badge bg-secondary me-1 help-cursor"
                      :id="`${field}DefaultInfoFieldLayoutSetting`"
                      v-if="fieldsMap[field]">
                      {{ fieldsMap[field].friendlyName }}
                      <BTooltip :target="`${field}DefaultInfoFieldLayoutSetting`">{{ fieldsMap[field].help }}</BTooltip>
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
                        class="badge bg-secondary me-1 help-cursor"
                        :id="`${field}InfoFieldLayoutSetting`"
                        v-if="fieldsMap[field]">
                        {{ fieldsMap[field].friendlyName }}
                        <BTooltip :target="`${field}InfoFieldLayoutSetting`">{{ fieldsMap[field].help }}</BTooltip>
                      </label>
                    </template>
                  </td>
                  <td>
                    <button
                      type="button"
                      class="btn btn-sm btn-danger pull-right"
                      @click="deleteLayout('sessionsinfofields', config.name, 'infoFieldLayouts', index)"
                      :title="$t('settings.infoLayout.deleteTip')">
                      <span class="fa fa-trash-o" />&nbsp;
                      {{ $t('common.delete') }}
                    </button>
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

          <div
            v-if="!infoFieldLayouts || !infoFieldLayouts.length"
            class="alert alert-info">
            <span class="fa fa-info-circle fa-lg" />
            <strong>
              {{ $t('settings.infoLayout.empty') }}
            </strong>
            <br>
            <br>
            <span v-html="$t('settings.infoLayout.howToHtml')" />
          </div>
        </form> <!-- /info field configs settings -->

        <!-- spiview field configs settings -->
        <form
          v-if="visibleTab === 'spiview'"
          class="form-horizontal"
          id="spiview">
          <h3>{{ $t('settings.spiview.title') }}</h3>

          <p>{{ $t('settings.spiview.info') }}</p>

          <table class="table table-striped table-sm">
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
                      class="badge bg-secondary me-1 help-cursor">
                      {{ fieldsMap[field].friendlyName }} (100)
                      <BTooltip :target="`${field}DefaultSpiviewFieldConfigSetting`">{{ fieldsMap[field].help }}</BTooltip>
                    </label>
                  </template>
                </td>
                <td>&nbsp;</td>
              </tr> <!-- /default spiview field confg -->
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
                      class="badge bg-secondary me-1 help-cursor"
                      :id="`${fieldObj.dbField}SpiviewFieldConfigSetting`"
                      v-for="fieldObj in config.fieldObjs"
                      :key="fieldObj.dbField">
                      {{ fieldObj.friendlyName }}
                      ({{ fieldObj.count }})
                      <BTooltip :target="`${fieldObj.dbField}SpiviewFieldConfigSetting`">{{ fieldObj.help }}</BTooltip>
                    </label>
                  </td>
                  <td>
                    <button
                      type="button"
                      class="btn btn-sm btn-danger pull-right"
                      @click="deleteLayout('spiview', config.name, 'spiviewConfigs', index)"
                      :title="$t('settings.spiview.deleteTip')">
                      <span class="fa fa-trash-o" />&nbsp;
                      {{ $t('common.delete') }}
                    </button>
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

          <div
            v-if="!spiviewConfigs || !spiviewConfigs.length"
            class="alert alert-info">
            <span class="fa fa-info-circle fa-lg" />
            <strong>
              {{ $t('settings.spiview.empty') }}
            </strong>
            <br>
            <br>
            <span v-html="$t('settings.spiview.howToHtml')" />
          </div>
        </form> <!-- /spiview field configs settings -->

        <!-- theme settings -->
        <form
          v-if="visibleTab === 'theme'"
          id="theme">
          <h3>{{ $t('settings.themes.title') }}</h3>

          <p>{{ $t('settings.themes.pickTheme') }}</p>

          <hr>

          <!-- theme picker -->
          <div class="row">
            <div
              class="col-lg-6 col-md-12"
              v-for="theme in themeDisplays"
              :class="theme.class"
              :key="theme.class">
              <div class="theme-display">
                <div class="row">
                  <div class="col-md-12">
                    <div class="custom-control custom-radio ms-1">
                      <input
                        type="radio"
                        class="custom-control-input cursor-pointer"
                        v-model="settings.theme"
                        @change="changeTheme(theme.class)"
                        :value="theme.class"
                        :id="theme.class">
                      <label
                        class="custom-control-label cursor-pointer ms-2"
                        :for="theme.class">
                        {{ theme.name }}
                      </label>
                    </div>
                  </div>
                </div>
                <nav class="navbar navbar-dark">
                  <a class="navbar-brand cursor-pointer">
                    <img
                      :src="settings.logo"
                      class="arkime-logo"
                      alt="hoot">
                  </a>
                  <ul class="nav">
                    <a class="nav-item cursor-pointer no-decoration active">
                      Current Page
                    </a>
                    <a class="nav-item cursor-pointer no-decoration ms-3">
                      Other Pages
                    </a>
                  </ul>
                  <ul class="navbar-nav me-2">
                    <span class="fa fa-info-circle fa-lg health-green" />
                  </ul>
                </nav>
                <div class="display-sub-navbar">
                  <div class="row">
                    <div class="col-xl-5 col-lg-4 col-md-5">
                      <div class="input-group input-group-sm ms-1">
                        <span class="input-group-text">
                          <span class="fa fa-search" />
                        </span>
                        <input
                          type="text"
                          placeholder="Search"
                          class="form-control">
                      </div>
                    </div>
                    <div class="col-xl-7 col-lg-8 col-sm-7">
                      <div class="fw-bold text-theme-accent ms-1">
                        Important text
                      </div>
                      <div class="pull-right display-sub-navbar-buttons">
                        <a class="btn btn-sm btn-default btn-theme-tertiary-display me-1">
                          Search
                        </a>
                        <a class="btn btn-sm btn-default btn-theme-quaternary-display me-1">
                          <span class="fa fa-cog fa-lg" />
                        </a>
                        <a class="btn btn-sm btn-default btn-theme-secondary-display me-1">
                          <span class="fa fa-eye fa-lg" />
                        </a>
                        <b-dropdown
                          right
                          size="sm"
                          class="pull-right action-menu-dropdown"
                          variant="theme-primary-display">
                          <b-dropdown-item>
                            Example
                          </b-dropdown-item>
                          <b-dropdown-item class="active">
                            Active Example
                          </b-dropdown-item>
                        </b-dropdown>
                      </div>
                    </div>
                  </div>
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
            </div>
          </div> <!-- /theme picker -->

          <!-- logo picker -->
          <hr>
          <h3>{{ $t('settings.themes.logos') }}</h3>
          <p>{{ $t('settings.themes.pickLogo') }}</p>
          <div class="row well logo-well me-1 ms-1">
            <div
              class="col-lg-3 col-md-4 col-sm-6 col-xs-12 mb-2 mt-2 logos"
              v-for="logo in logos"
              :key="logo.location">
              <div class="custom-control custom-radio ms-1">
                <input
                  type="radio"
                  :id="logo.location"
                  :value="logo.location"
                  v-model="settings.logo"
                  @change="changeLogo(logo.location)"
                  class="custom-control-input cursor-pointer">
                <label
                  class="custom-control-label cursor-pointer ms-2"
                  :for="logo.location">
                  {{ logo.name }}
                </label>
              </div>
              <img
                :src="logo.location"
                :alt="logo.name">
            </div>
          </div> <!-- /logo picker -->

          <div v-if="settings.shiftyEyes">
            <hr>
            <h3>
              Yahaha! You found me!
              <button
                class="btn btn-primary"
                @click="toggleShiftyEyes">
                Turn Me Off
              </button>
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
            <div class="row">
              <div class="col-md-4">
                <h3 class="mt-0 mb-3">
                  Custom Theme
                  <button
                    type="button"
                    class="btn btn-theme-tertiary pull-right"
                    @click="displayHelp = !displayHelp">
                    <span class="fa fa-question-circle" />&nbsp;
                    <span v-if="displayHelp">
                      Hide
                    </span>
                    <span v-else>
                      Show
                    </span>
                    Help
                  </button>
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
              </div>
              <div class="col-md-8">
                <div
                  class="custom-theme"
                  id="custom-theme-display">
                  <div class="theme-display">
                    <div class="navbar navbar-dark">
                      <a class="navbar-brand cursor-pointer">
                        <img
                          :src="settings.logo"
                          class="arkime-logo"
                          alt="hoot">
                      </a>
                      <ul class="nav">
                        <a class="nav-item cursor-pointer active">
                          Current Page
                        </a>
                        <a class="nav-item cursor-pointer ms-3">
                          Other Pages
                        </a>
                      </ul>
                      <ul class="navbar-nav me-2">
                        <span class="fa fa-info-circle fa-lg health-green" />
                      </ul>
                    </div>
                    <div class="display-sub-navbar">
                      <div class="row">
                        <div class="col-xl-5 col-lg-4 col-md-5">
                          <div class="input-group input-group-sm ms-1">
                            <span class="input-group-text">
                              <span class="fa fa-search" />
                            </span>
                            <input
                              type="text"
                              placeholder="Search"
                              class="form-control">
                          </div>
                        </div>
                        <div class="col-xl-7 col-lg-8 col-sm-7">
                          <div class="fw-bold text-theme-accent ms-1">
                            Important text
                          </div>
                          <div class="pull-right display-sub-navbar-buttons">
                            <a class="btn btn-sm btn-default btn-theme-tertiary-display me-1">
                              Search
                            </a>
                            <a class="btn btn-sm btn-default btn-theme-quaternary-display me-1">
                              <span class="fa fa-cog fa-lg" />
                            </a>
                            <a class="btn btn-sm btn-default btn-theme-secondary-display me-1">
                              <span class="fa fa-eye fa-lg" />
                            </a>
                            <b-dropdown
                              right
                              size="sm"
                              class="pull-right action-menu-dropdown"
                              variant="theme-primary-display">
                              <b-dropdown-item>
                                Example
                              </b-dropdown-item>
                              <b-dropdown-item class="active">
                                Active Example
                              </b-dropdown-item>
                            </b-dropdown>
                          </div>
                        </div>
                      </div>
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
                        <div class="row">
                          <div class="col-md-6 sessionsrc">
                            <small class="session-detail-ts fw-bold">
                              <em class="ts-value">
                                2013/11/18 03:06:52.831
                              </em>
                              <span class="pull-right">
                                27 bytes
                              </span>
                            </small>
                            <pre>Source packet text</pre>
                          </div>
                          <div class="col-md-6 sessiondst">
                            <small class="session-detail-ts fw-bold">
                              <em class="ts-value">
                                2013/11/18 03:06:52.841
                              </em>
                              <span class="pull-right">
                                160 bytes
                              </span>
                            </small>
                            <pre>Destination packet text</pre>
                          </div>
                        </div>
                      </div>
                    </div>
                  </div>
                </div>
              </div>
            </div> <!-- /custom theme display -->

            <br>

            <p
              v-if="displayHelp"
              class="help-block">
              Main theme colors are lightened/darkened programmatically to
              provide dark borders, active buttons, hover colors, etc.
            </p>

            <!-- main colors -->
            <div class="row form-group">
              <div class="col-md-3">
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
              </div>
              <div class="col-md-3">
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
              </div>
              <div class="col-md-3">
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
              </div>
              <div class="col-md-3">
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
              </div>
            </div> <!-- /main colors -->

            <p
              v-if="displayHelp"
              class="help-block">
              <em>Highlight colors should be similar to their parent color, above.</em>
              <br>
              For <strong>light themes</strong>, the highlight color should be <strong>lighter</strong> than the original.
              For <strong>dark themes</strong>, the highlight color should be <strong>darker</strong> than original.
            </p>

            <!-- main color highlights/backgrounds -->
            <div class="row form-group">
              <div class="col-md-3">
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
              </div>
              <div class="col-md-3">
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
              </div>
              <div class="col-md-3">
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
              </div>
              <div class="col-md-3">
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
              </div>
            </div> <!-- /main color highlights/backgrounds -->

            <br>

            <div
              v-if="displayHelp"
              class="row">
              <div class="col-6">
                <p class="help-block">
                  <em>Map colors</em>
                  <br>
                  These should be different to show contrast between land and water.
                </p>
              </div>
              <div class="col-6">
                <p class="help-block">
                  <em>Packet colors</em>
                  <br>
                  These are displayed when viewing session packets and in the
                  sessions timeline graph. They should be very different colors.
                </p>
              </div>
            </div>

            <div class="row form-group">
              <!-- visualization colors -->
              <div class="col-md-3">
                <color-picker
                  :color="water"
                  @color-selected="changeColor"
                  color-name="water"
                  field-name="Map Water" />
              </div>
              <div class="col-md-3">
                <color-picker
                  :color="land"
                  @color-selected="changeColor"
                  color-name="land"
                  field-name="Map Land" />
              </div> <!-- /visualization colors -->
              <!-- packet colors -->
              <div class="col-md-3">
                <color-picker
                  :color="src"
                  @color-selected="changeColor"
                  color-name="src"
                  field-name="Source Packets" />
              </div>
              <div class="col-md-3">
                <color-picker
                  :color="dst"
                  @color-selected="changeColor"
                  color-name="dst"
                  field-name="Destination Packets" />
              </div> <!-- /packet colors -->
            </div>

            <br>

            <div class="row mb-4">
              <div class="col-md-12">
                <label>
                  Share your theme with others:
                </label>
                <div class="input-group input-group-sm">
                  <input
                    type="text"
                    class="form-control"
                    v-model="themeString"
                    @keyup.up.down.left.right.a.b="secretStuff">
                  <button
                    class="btn btn-theme-secondary"
                    type="button"
                    @click="copyValue(themeString)">
                    <span class="fa fa-clipboard" />&nbsp;
                    {{ $t('common.copy') }}
                  </button>
                  <button
                    class="btn btn-theme-primary"
                    type="button"
                    @click="updateThemeString">
                    <span class="fa fa-check" />&nbsp;
                    {{ $t('common.apply') }}
                  </button>
                </div>
              </div>
            </div>
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
          <div
            v-if="!userId"
            class="form-group row">
            <label class="col-sm-3 col-form-label text-end fw-bold">
              {{ $t('settings.password.currentPassword') }}
            </label>
            <div class="col-sm-6">
              <input
                type="password"
                class="form-control form-control-sm"
                v-model="currentPassword"
                :placeholder="$t('settings.password.currentPasswordPlaceholder')">
            </div>
          </div>

          <!-- new password -->
          <div class="form-group row">
            <label class="col-sm-3 col-form-label text-end fw-bold">
              {{ $t('settings.password.newPassword') }}
            </label>
            <div class="col-sm-6">
              <input
                type="password"
                class="form-control form-control-sm"
                v-model="newPassword"
                :placeholder="$t('settings.password.newPasswordPlaceholder')">
            </div>
          </div>

          <!-- confirm new password -->
          <div class="form-group row">
            <label class="col-sm-3 col-form-label text-end fw-bold">
              {{ $t('settings.password.confirmPassword') }}
            </label>
            <div class="col-sm-6">
              <input
                type="password"
                class="form-control form-control-sm"
                v-model="confirmNewPassword"
                :placeholder="$t('settings.password.confirmPasswordPlaceholder')">
            </div>
          </div>

          <!-- change password button/error -->
          <div class="form-group row">
            <label class="col-sm-3 col-form-label">&nbsp;</label>
            <div class="col-sm-9">
              <button
                type="button"
                class="btn btn-theme-tertiary"
                @click="changePassword">
                {{ $t('settings.password.changePassword') }}
              </button>
              <span
                v-if="changePasswordError"
                class="small text-danger ps-4">
                <span class="fa fa-exclamation-triangle" />&nbsp;
                {{ changePasswordError }}
              </span>
            </div>
          </div> <!-- /change password button/error -->
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
      </div>
    </div> <!-- /content -->
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
      multiviewer: this.$constants.MULTIVIEWER,
      hasUsersES: this.$constants.HASUSERSES
    };
  },
  computed: {
    user: function () {
      return this.$store.state.user;
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
        if (!response.manualQuery) {
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

/* fixed tab buttons */
.settings-page div.nav-pills {
  position: fixed;
}

.settings-page .nav-separator {
  width: 100%;
  border-top: 1px solid var(--color-gray);
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

.settings-page .navbar {
  min-height: 20px;
  height: 36px;
  border-radius: 6px 6px 0 0;
  z-index: 1;
}

.settings-page .navbar .arkime-logo {
  top: 0;
  left: 20px;
  height: 36px;
  position: absolute;
}
/* icon logos (logo in circle) are wider */
.settings-page .navbar .arkime-logo[src*="Icon"] {
  left: 8px;
}

.settings-page .navbar .nav {
  position: absolute;
  left: 50px
}

.settings-page .navbar .navbar-nav {
  margin-right: -8px;
}

.settings-page .navbar .navbar-nav .health-green {
  color: #00aa00;
}

.settings-page .navbar-dark a {
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
.settings-page .display-sub-navbar .input-group {
  padding-top: 4px;
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

/* arkime light (default) */
.settings-page .arkime-light-theme .navbar {
  background-color: #212121;
  border-color: #111111;
}

.settings-page .arkime-light-theme .navbar-dark a:hover,
.settings-page .arkime-light-theme .navbar-dark a.active {
  background-color: #303030;
}

.settings-page .arkime-light-theme .input-group > .input-group-text, .input-group:not(.color) > .input-group-text {
  color: #333333 !important;
  background-color: #EEEEEE !important;
  border-color: #CCCCCC !important;
}

.settings-page .arkime-light-theme input.form-control,
.settings-page .arkime-light-theme input.form-control:focus {
  color: #000000 !important;
  background-color: #FFFFFF !important;
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

.settings-page .arkime-light-theme .dropdown-menu {
  background-color: #FFFFFF;
  border-color: #EEEEEE;
  padding: 2px 0;
  font-size: .85rem;
  min-width: 12rem;
}
.settings-page .arkime-light-theme .dropdown-menu .dropdown-item {
  color: #212529;
  padding: 2px 8px;
}
.settings-page .arkime-light-theme .dropdown-menu .dropdown-item:hover {
  background-color: #EEEEEE;
  color: #212529;
}
.settings-page .arkime-light-theme .dropdown-menu .dropdown-item:focus {
  background-color: #FFFFFF;
  color: #212529;
}
.settings-page .arkime-light-theme .dropdown-menu li.active .dropdown-item {
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
.settings-page .arkime-dark-theme .navbar {
  background-color: #9E9E9E;
  border-color: #8E8E8E;
}

.settings-page .arkime-dark-theme .navbar-dark a:hover,
.settings-page .arkime-dark-theme .navbar-dark a.active {
  background-color: #ADADAD;
}

.settings-page .arkime-dark-theme .input-group > .input-group-text, .input-group:not(.color) > .input-group-text {
  color: #FFFFFF !important;
  background-color: #303030 !important;
  border-color: #CCCCCC !important;
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

.settings-page .arkime-dark-theme .dropdown-menu {
  background-color: #303030;
  border-color: #555555;
  padding: 2px 0;
  font-size: .85rem;
  min-width: 12rem;
}
.settings-page .arkime-dark-theme .dropdown-menu .dropdown-item {
  color: #FFFFFF;
  padding: 2px 8px;
}
.settings-page .arkime-dark-theme .dropdown-menu .dropdown-item:hover {
  background-color: #555555;
  color: #FFFFFF;
}
.settings-page .arkime-dark-theme .dropdown-menu .dropdown-item:focus {
  background-color: #555555;
  color: #FFFFFF;
}
.settings-page .arkime-dark-theme .dropdown-menu li.active .dropdown-item {
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

.settings-page .arkime-dark-theme input.form-control {
  color: #FFFFFF !important;
  background-color: #222222 !important;
}
.settings-page .arkime-dark-theme input.form-control::placeholder {
  color: #CCC !important;
}

/* purp */
.settings-page .purp-theme .navbar {
  background-color: #530763;
  border-color: #360540;
}

.settings-page .purp-theme .navbar-dark a:hover,
.settings-page .purp-theme .navbar-dark a.active {
  background-color: #830b9c;
}

.settings-page .purp-theme .input-group > .input-group-text, .input-group:not(.color) > .input-group-text {
  color: #333333 !important;
  background-color: #EEEEEE !important;
  border-color: #CCCCCC !important;
}

.settings-page .purp-theme input.form-control,
.settings-page .purp-theme input.form-control:focus {
  color: #000000 !important;
  background-color: #FFFFFF !important;
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

.settings-page .purp-theme .dropdown-menu {
  background-color: #FFFFFF;
  border-color: #EEEEEE;
  padding: 2px 0;
  font-size: .85rem;
  min-width: 12rem;
}
.settings-page .purp-theme .dropdown-menu .dropdown-item {
  color: #212529;
  padding: 2px 8px;
}
.settings-page .purp-theme .dropdown-menu .dropdown-item:hover {
  background-color: #EEEEEE;
  color: #212529;
}
.settings-page .purp-theme .dropdown-menu .dropdown-item:focus {
  background-color: #FFFFFF;
  color: #212529;
}
.settings-page .purp-theme .dropdown-menu li.active .dropdown-item {
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
.settings-page .blue-theme .navbar {
  background-color: #163254;
  border-color: #000000;
}

.settings-page .blue-theme .navbar-dark a:hover,
.settings-page .blue-theme .navbar-dark a.active {
  background-color: #214b78;
}

.settings-page .blue-theme .input-group > .input-group-text, .input-group:not(.color) > .input-group-text {
  color: #333333 !important;
  background-color: #EEEEEE !important;
  border-color: #CCCCCC !important;
}

.settings-page .blue-theme input.form-control,
.settings-page .blue-theme input.form-control:focus {
  color: #000000 !important;
  background-color: #FFFFFF !important;
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

.settings-page .blue-theme .dropdown-menu {
  background-color: #FFFFFF;
  border-color: #EEEEEE;
  padding: 2px 0;
  font-size: .85rem;
  min-width: 12rem;
}
.settings-page .blue-theme .dropdown-menu .dropdown-item {
  color: #212529;
  padding: 2px 8px;
}
.settings-page .blue-theme .dropdown-menu .dropdown-item:hover {
  background-color: #EEEEEE;
  color: #212529;
}
.settings-page .blue-theme .dropdown-menu .dropdown-item:focus {
  background-color: #FFFFFF;
  color: #212529;
}
.settings-page .blue-theme .dropdown-menu li.active .dropdown-item {
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
.settings-page .green-theme .navbar {
  background-color: #2A6E3d;
  border-color: #235A32;
}

.settings-page .green-theme .navbar-dark a:hover,
.settings-page .green-theme .navbar-dark a.active {
  background-color: #2a7847;
}

.settings-page .green-theme .input-group > .input-group-text, .input-group:not(.color) > .input-group-text {
  color: #333333 !important;
  background-color: #EEEEEE !important;
  border-color: #CCCCCC !important;
}

.settings-page .green-theme input.form-control,
.settings-page .green-theme input.form-control:focus {
  color: #000000 !important;
  background-color: #FFFFFF !important;
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

.settings-page .green-theme .dropdown-menu {
  background-color: #FFFFFF;
  border-color: #EEEEEE;
  padding: 2px 0;
  font-size: .85rem;
  min-width: 12rem;
}
.settings-page .green-theme .dropdown-menu .dropdown-item {
  color: #212529;
  padding: 2px 8px;
}
.settings-page .green-theme .dropdown-menu .dropdown-item:hover {
  background-color: #EEEEEE;
  color: #212529;
}
.settings-page .green-theme .dropdown-menu .dropdown-item:focus {
  background-color: #FFFFFF;
  color: #212529;
}
.settings-page .green-theme .dropdown-menu li.active .dropdown-item {
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
.settings-page .cotton-candy-theme .navbar {
  background-color: #B0346D;
  border-color: #9B335A;
}

.settings-page .cotton-candy-theme .navbar-dark a:hover,
.settings-page .cotton-candy-theme .navbar-dark a.active {
  background-color: #c43d75;
}

.settings-page .cotton-candy-theme .input-group > .input-group-text, .input-group:not(.color) > .input-group-text {
  color: #333333 !important;
  background-color: #EEEEEE !important;
  border-color: #CCCCCC !important;
}

.settings-page .cotton-candy-theme input.form-control,
.settings-page .cotton-candy-theme input.form-control:focus {
  color: #000000 !important;
  background-color: #FFFFFF !important;
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

.settings-page .cotton-candy-theme .dropdown-menu {
  background-color: #FFFFFF;
  border-color: #EEEEEE;
  padding: 2px 0;
  font-size: .85rem;
  min-width: 12rem;
}
.settings-page .cotton-candy-theme .dropdown-menu .dropdown-item {
  color: #212529;
  padding: 2px 8px;
}
.settings-page .cotton-candy-theme .dropdown-menu .dropdown-item:hover {
  background-color: #EEEEEE;
  color: #212529;
}
.settings-page .cotton-candy-theme .dropdown-menu .dropdown-item:focus {
  background-color: #FFFFFF;
  color: #212529;
}
.settings-page .cotton-candy-theme .dropdown-menu li.active .dropdown-item {
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
.settings-page .dark-2-theme .navbar {
  background-color: #363A7D;
  border-color: #2F2F5F;
}

.settings-page .dark-2-theme .navbar-dark a:hover,
.settings-page .dark-2-theme .navbar-dark a.active {
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

.settings-page .dark-2-theme .input-group > .input-group-text, .input-group:not(.color) > .input-group-text {
  color: #C7C7C7 !important;
  background-color: #222222 !important;
  border-color: #AAAAAA !important;
}

.settings-page .dark-2-theme .dropdown-menu {
  background-color: #222222;
  border-color: #555555;
  padding: 2px 0;
  font-size: .85rem;
  min-width: 12rem;
}
.settings-page .dark-2-theme .dropdown-menu .dropdown-item {
  color: #C7C7C7;
  padding: 2px 8px;
}
.settings-page .dark-2-theme .dropdown-menu .dropdown-item:hover {
  background-color: #555555;
  color: #C7C7C7;
}
.settings-page .dark-2-theme .dropdown-menu .dropdown-item:focus {
  background-color: #222222;
  color: #C7C7C7;
}
.settings-page .dark-2-theme .dropdown-menu li.active .dropdown-item {
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

.settings-page .dark-2-theme input.form-control {
  color: #FFFFFF !important;
  background-color: #111111 !important;
}
.settings-page .dark-2-theme input.form-control::placeholder {
  color: #CCC !important;
}

/* Dark Blue */
.settings-page .dark-3-theme .navbar {
  background-color: #9F9F9F;
  border-color: #C3C3C3;
}

.settings-page .dark-3-theme .navbar-dark a:hover,
.settings-page .dark-3-theme .navbar-dark a.active {
  background-color: #8A8A8A;
}

.settings-page .dark-3-theme .navbar-dark li.active a {
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

.settings-page .dark-3-theme .input-group > .input-group-text, .input-group:not(.color) > .input-group-text {
  color: #ADC1C3 !important;
  background-color: #002833 !important;
  border-color: #AAAAAA !important;
}

.settings-page .dark-3-theme .dropdown-menu {
  background-color: #002833;
  border-color: #555555;
  padding: 2px 0;
  font-size: .85rem;
  min-width: 12rem;
}
.settings-page .dark-3-theme .dropdown-menu .dropdown-item {
  color: #ADC1C3;
  padding: 2px 8px;
}
.settings-page .dark-3-theme .dropdown-menu .dropdown-item:hover {
  background-color: #555555;
  color: #ADC1C3;
}
.settings-page .dark-3-theme .dropdown-menu .dropdown-item:focus {
  background-color: #002833;
  color: #ADC1C3;
}
.settings-page .dark-3-theme .dropdown-menu li.active .dropdown-item {
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

.settings-page .dark-3-theme input.form-control {
  color: #EEEEEE !important;
  background-color: #222222 !important;
}
.settings-page .dark-3-theme input.form-control::placeholder {
  color: #CCC !important;
}

/* Custom */
.settings-page .custom-theme .theme-display {
  background-color: var(--color-background);
  color: var(--color-foreground);
  -webkit-box-shadow: 0 0 16px -2px black;
     -moz-box-shadow: 0 0 16px -2px black;
          box-shadow: 0 0 16px -2px black;
}

.settings-page .custom-theme .navbar {
  background-color: var(--color-primary-dark);
  border-color: var(--color-primary-darker);
}
.settings-page .custom-theme .navbar a.nav-item {
  color: var(--color-button);
}
.settings-page .custom-theme .navbar a.nav-item:hover,
.settings-page .custom-theme .navbar a.nav-item.active {
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

.settings-page .custom-theme .dropdown-menu {
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
