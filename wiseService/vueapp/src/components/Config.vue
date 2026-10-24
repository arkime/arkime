<!--
Copyright Yahoo Inc.
SPDX-License-Identifier: Apache-2.0
-->
<template>
  <!-- container -->
  <div class="settings-page">
    <!-- sub navbar -->
    <div class="sub-navbar">
      <span class="sub-navbar-title">
        <v-icon
          icon="mdi-cog"
          class="me-1" />
        <span>{{ $t('wise.config.title') }}</span>
      </span>
    </div> <!-- /sub navbar -->

    <v-snackbar
      :model-value="!!alertState.text"
      :color="alertVariantToType(alertState.variant)"
      location="bottom"
      :timeout="-1"
      @update:model-value="(v) => { if (!v) alertState.text = '' }">
      {{ alertState.text }}
      <template #actions>
        <v-btn
          variant="text"
          icon="mdi-close"
          @click="alertState.text = ''" />
      </template>
    </v-snackbar>

    <v-row
      no-gutters
      v-if="loaded">
      <!-- Sources sidebar -->
      <v-col
        cols="12"
        sm="3"
        md="2"
        lg="2"
        xl="1"
        role="tablist"
        aria-orientation="vertical">
        <v-tabs
          :model-value="selectedSourceKey"
          direction="vertical"
          density="compact"
          color="primary"
          selected-class="font-weight-bold"
          @update:model-value="selectSource">
          <v-tab
            v-for="sourceKey in sidebarOptions.services"
            :key="sourceKey + '-tab'"
            :value="sourceKey">
            {{ sourceKey }}
          </v-tab>
          <v-divider
            v-if="sidebarOptions.sources.length"
            class="my-1" />
          <v-tab
            v-for="sourceKey in sidebarOptions.sources"
            :key="sourceKey + '-tab'"
            :value="sourceKey">
            {{ sourceKey }}
          </v-tab>
        </v-tabs>

        <div
          v-if="isWiseAdmin"
          class="px-2 mt-2">
          <v-btn
            block
            color="warning"
            class="mb-2"
            @click="showImportConfigModal = true">
            <v-icon
              icon="mdi-download"
              class="me-1" />
            {{ $t('common.import') }}
            <v-tooltip activator="parent">
              {{ $t('wise.config.importTip') }}
            </v-tooltip>
          </v-btn>
          <v-btn
            block
            color="success"
            @click="showSourceModal = true">
            <v-icon
              icon="mdi-plus"
              class="me-1" />
            {{ $t('common.create') }}
            <v-tooltip activator="parent">
              {{ $t('wise.config.createTip') }}
            </v-tooltip>
          </v-btn>
        </div>
      </v-col> <!-- /Sources sidebar -->

      <!-- Selected Source Input Fields -->
      <v-col
        cols="12"
        sm="9"
        md="10"
        lg="10"
        xl="11"
        class="d-flex flex-column px-5 pt-2 source-container">
        <h2>
          <div
            v-if="configViewSelected === 'edit'"
            class="d-flex align-center float-right ms-5 ga-2">
            <v-btn
              color="warning"
              variant="flat"
              :disabled="fileResetDisabled"
              @click="loadSourceFile">
              {{ $t('wise.config.resetFile') }}
            </v-btn>
            <v-btn
              v-if="isWiseAdmin"
              color="primary"
              variant="flat"
              :disabled="fileSaveDisabled"
              @click="saveSourceFile">
              {{ $t('wise.config.saveFile') }}
            </v-btn>
          </div>
          <div
            v-else-if="configViewSelected === 'config' && isWiseAdmin"
            class="d-flex align-center float-right ms-5 ga-2">
            <v-text-field
              id="config-pin-code-top"
              v-model="configCode"
              :placeholder="$t('wise.config.configCodePlaceholder')"
              density="compact"
              style="min-width: 420px;" />
            <v-tooltip
              activator="#config-pin-code-top"
              location="left">
              {{ $t('wise.config.configCodeTip') }}
            </v-tooltip>
            <v-btn
              color="primary"
              :disabled="!saveEnabled"
              @click="saveConfig(false)">
              {{ $t('wise.config.saveRestart') }}
            </v-btn>
          </div>
          {{ selectedSourceKey }}
        </h2>
        <div
          v-if="configDefs[selectedSourceSplit]"
          class="subtext mt-1 mb-4">
          <div
            v-if="configDefs[selectedSourceSplit].description"
            class="mb-2 wrapit">
            {{ configDefs[selectedSourceSplit].description }}
            <a
              v-if="configDefs[selectedSourceSplit].link"
              :href="configDefs[selectedSourceSplit].link"
              class="no-decoration"
              target="_blank">
              {{ $t('wise.config.learnMore') }}
            </a>
          </div>

          <div v-if="configDefs[selectedSourceSplit].editable || configDefs[selectedSourceSplit].displayable">
            <v-radio-group
              v-model="configViewSelected"
              @input="configViewChanged"
              :options="configViews"
              buttons
              button-color="secondary"
              variant="outlined"
              size="md"
              name="radio-btn-outline" />
            <v-checkbox
              switch
              v-model="showPrettyJSON"
              v-if="configViewSelected === 'display' && displayJSON">
              Format JSON
            </v-checkbox>
            <template v-if="configViewSelected === 'config'">
              <v-btn
                class="ms-2"
                :pressed="rawConfig"
                color="info"
                variant="outlined"
                @click="rawConfig = !rawConfig">
                {{ $t( rawConfig ? 'wise.config.viewConfigFields' : 'wise.config.viewRawConfig' ) }}
              </v-btn>
            </template>
            <template v-if="configViewSelected === 'edit' && currCSV">
              <v-btn
                class="ms-2"
                color="info"
                variant="outlined"
                @click="toggleCSVEditor">
                {{ $t( rawCSV ? 'wise.config.useCSVEditor' : 'wise.config.useRawCSV' ) }}
              </v-btn>
            </template>
            <template v-if="configViewSelected === 'edit' && currValueActionsFile">
              <v-btn
                class="ms-2"
                color="info"
                variant="outlined"
                @click="toggleValueActionsEditor">
                {{ $t( rawValueActions ? 'wise.config.UseValueActionsEditor' : 'wise.config.useRawValueActions' ) }}
              </v-btn>
            </template>
          </div>
        </div>

        <div v-if="configViewSelected === 'edit'">
          <p class="wrapit">
            {{ $t('wise.config.format') }} {{ currFormat ?? 'unknown' }}
            <template v-if="currFormat === 'tagger'">
              -
              <a
                target="_blank"
                class="no-decoration"
                href="https://arkime.com/taggerformat">
                {{ $t('wise.config.learnMore') }}
              </a>
            </template>
          </p>
          <p
            v-if="!currFormat && currCSV"
            class="wrapit">
            <span v-html="$t('wise.config.csvHelpHtml')" />
          </p>
          <h6
            v-if="currFormat === 'valueactions'"
            class="mb-3">
            {{ $t('wise.config.pushWaitTime') }}
          </h6>
          <div
            v-if="currFormat === 'valueactions' && !rawValueActions"
            class="value-actions-editor">
            <transition-group
              tag="ul"
              name="shrink"
              class="shrink-list">
              <li
                class="shrink-item"
                :key="line.id || lineIndex"
                v-for="(line, lineIndex) in currValueActionsFile">
                <v-row dense>
                  <v-col
                    :class="field.class"
                    :cols="field.cols || 12"
                    v-for="field in valueActionsFields"
                    :key="line.id + field.name">
                    <transition name="item-shrink">
                      <v-text-field
                        v-if="!field.advanced || displayAdvancedFields[line.key]"
                        :id="`value-action-${lineIndex}-${field.name}`"
                        :label="field.name"
                        :required="field.required"
                        :model-value="line[field.name]"
                        density="compact"
                        class="mb-1"
                        @update:model-value="debounceValueActionsChange">
                        <v-tooltip :activator="`[id='value-action-${lineIndex}-${field.name}']`">
                          {{ field.help }}
                        </v-tooltip>
                      </v-text-field>
                    </transition>
                  </v-col>
                  <v-col
                    cols="12"
                    class="mt-2">
                    <v-btn
                      color="error"
                      class="me-2"
                      @click="removeValueAction(lineIndex)">
                      <v-icon icon="mdi-minus" />&nbsp;
                      {{ $t('wise.config.removeValueAction') }}
                    </v-btn>
                    <v-btn
                      color="info"
                      @click="toggleAdvancedFields(line.key)">
                      <v-icon
                        icon="mdi-eye"
                        :class="displayAdvancedFields[line.key] ? 'mdi-eye-off' : 'mdi-eye'" />&nbsp;
                      {{ $t('wise.config.toggleAdvancedOptions') }}
                    </v-btn>
                  </v-col>
                </v-row>
                <hr>
              </li>
            </transition-group>
            <v-btn
              color="success"
              variant="flat"
              @click="addValueAction">
              <v-icon icon="mdi-plus" />&nbsp;
              {{ $t('wise.config.addValueAction') }}
            </v-btn>
          </div>
          <!-- text area input for tagger or csv formats (if user is not using the csv editor) -->
          <template
            v-else-if="!currJSONFile && (currFormat === 'tagger' || rawCSV || rawValueActions)">
            <v-textarea
              v-model="currFile"
              rows="18" />
          </template>
          <!-- json editor -->
          <json-editor-vue
            v-else-if="currJSONFile"
            v-model="currJSONFile"
            :mode="'text'" />
          <!-- csv editor -->
          <div
            v-else-if="currCSV && !rawCSV"
            class="pt-3 pb-3 csv-editor">
            <v-form
              inline
              class="flex-nowrap">
              <div class="csv-cell-group">
                <input
                  type="text"
                  disabled="true"
                  style="width:65px;"
                  class="csv-cell csv-cell--disabled">
              </div>
              <template
                v-for="(cell, cellIndex) in currCSV.longestRow"
                :key="cellIndex + 'colheader'">
                <div class="csv-cell-group">
                  <input
                    type="text"
                    disabled="true"
                    class="csv-cell csv-cell--disabled"
                    :placeholder="cellIndex">
                  <v-menu>
                    <template #activator="{ props: activatorProps }">
                      <v-btn
                        v-bind="activatorProps"
                        size="small"
                        variant="outlined"
                        class="col-control">
                        <v-icon icon="mdi-menu-down" />
                      </v-btn>
                    </template>
                    <v-list density="compact">
                      <v-list-item @click="addCSVColumn(cellIndex)">
                        <v-list-item-title>{{ $t('wise.config.addColumnLeft') }}</v-list-item-title>
                      </v-list-item>
                      <v-list-item @click="addCSVColumn(cellIndex + 1)">
                        <v-list-item-title>{{ $t('wise.config.addColumnRight') }}</v-list-item-title>
                      </v-list-item>
                      <v-list-item @click="removeCSVColumn(cellIndex)">
                        <v-list-item-title>{{ $t('wise.config.removeColumn') }}</v-list-item-title>
                      </v-list-item>
                    </v-list>
                  </v-menu>
                </div>
              </template>
            </v-form>
            <v-form
              inline
              class="flex-nowrap"
              v-for="(row, rowIndex) in currCSV.rows"
              :key="rowIndex + 'csvrow'">
              <div class="csv-cell-group">
                <input
                  type="text"
                  disabled="true"
                  style="width:65px;"
                  :placeholder="rowIndex"
                  class="csv-cell csv-cell--disabled">
                <v-menu>
                  <template #activator="{ props: activatorProps }">
                    <v-btn
                      v-bind="activatorProps"
                      size="small"
                      variant="outlined"
                      class="col-control">
                      <v-icon icon="mdi-menu-down" />
                    </v-btn>
                  </template>
                  <v-list density="compact">
                    <v-list-item @click="addCSVRow(rowIndex)">
                      <v-list-item-title>{{ $t('wise.config.addRowAbove') }}</v-list-item-title>
                    </v-list-item>
                    <v-list-item @click="addCSVRow(rowIndex + 1)">
                      <v-list-item-title>{{ $t('wise.config.addRowBelow') }}</v-list-item-title>
                    </v-list-item>
                    <v-list-item @click="removeCSVRow(rowIndex)">
                      <v-list-item-title>{{ $t('wise.config.removeRow') }}</v-list-item-title>
                    </v-list-item>
                  </v-list>
                </v-menu>
              </div>
              <template
                v-for="(cell, cellIndex) in currCSV.longestRow"
                :key="cellIndex + 'csvcell'">
                <div class="csv-cell-group">
                  <input
                    type="text"
                    @input="debounceCSVChange"
                    :id="rowIndex + '-' + cellIndex"
                    v-model="currCSV.rows[rowIndex][cellIndex]"
                    @keyup.enter="cellEnterClick(rowIndex, cellIndex)"
                    class="csv-cell">
                </div>
              </template>
            </v-form>
          </div> <!-- /csv editor -->
          <p
            v-else
            class="text-error">
            <span v-html="$t('wise.config.parseErrorHtml')" />
          </p>
          <div
            v-if="configViewSelected === 'edit'"
            class="d-flex align-center float-right ms-5 mt-2 mb-3 ga-2">
            <v-btn
              color="warning"
              variant="flat"
              :disabled="fileResetDisabled"
              @click="loadSourceFile">
              {{ $t('wise.config.resetFile') }}
            </v-btn>
            <v-btn
              v-if="isWiseAdmin"
              color="primary"
              variant="flat"
              :disabled="fileSaveDisabled"
              @click="saveSourceFile">
              {{ $t('wise.config.saveFile') }}
            </v-btn>
          </div>
        </div> <!-- edit -->

        <!-- display -->
        <div v-else-if="configViewSelected === 'display'">
          <template v-if="showPrettyJSON">
            <vue-json-pretty
              :data="displayJSON"
              :show-line-number="true"
              :show-double-quotes="false" />
          </template>
          <template v-else>
            <pre>{{ displayData }}</pre>
          </template>
        </div> <!-- /display -->

        <div v-else>
          <template v-if="!rawConfig">
            <template
              v-for="field in activeFields"
              :key="field.name + '-field'">
              <v-text-field
                v-if="currConfig && currConfig[selectedSourceKey] && field.multiline === undefined"
                :label="field.name"
                :model-value="currConfig[selectedSourceKey][field.name]"
                :placeholder="field.help"
                :title="field.help"
                :required="field.required"
                density="compact"
                class="mb-3"
                @update:model-value="(val) => inputChanged(val, field)" />
              <v-textarea
                v-if="currConfig && currConfig[selectedSourceKey] && field.multiline !== undefined"
                :label="field.name"
                :model-value="(currConfig[selectedSourceKey][field.name] || '').split(field.multiline).join('\n')"
                :placeholder="field.help"
                :title="field.help"
                :required="field.required"
                density="compact"
                rows="3"
                class="mb-3"
                @update:model-value="(val) => inputChanged(val, field)" />
            </template>
          </template>
          <pre
            v-show="rawConfig"
            class="mt-4 mb-4"
            :ref="selectedSourceKey + '-pre'"
            style="white-space:break-spaces;word-break:break-all;">{{ currConfig[selectedSourceKey] }}</pre>
          <v-btn
            v-if="configDefs && configDefs[selectedSourceSplit] && !configDefs[selectedSourceSplit].service"
            color="success"
            variant="flat"
            class="mx-auto mt-4"
            @click="copySource(selectedSourceKey)">
            <v-icon
              icon="mdi-content-copy"
              class="me-1" />
            {{ $t('wise.config.copyRawSource') }}
          </v-btn>
          <v-btn
            v-if="configDefs && configDefs[selectedSourceSplit] && !configDefs[selectedSourceSplit].service"
            color="error"
            variant="flat"
            class="mt-4 float-right"
            @click="deleteSource()">
            <v-icon
              icon="mdi-delete"
              class="me-1" />
            {{ $t('wise.config.deleteSource') }}
          </v-btn>
        </div> <!-- else -->
      </v-col><!-- /Selected Source Inputs Fields-->
    </v-row>

    <!-- add source modal -->
    <v-dialog
      v-model="showSourceModal"
      max-width="600">
      <v-card>
        <v-card-title>New Source</v-card-title>
        <v-card-text>
          <v-select
            v-model="newSource"
            :label="$t('wise.config.source')"
            :items="newSourceItems"
            item-title="title"
            item-value="value"
            :item-props="item => ({ disabled: item.disabled })"
            :placeholder="$t('wise.config.selectSource')"
            density="compact" />
          <p v-if="newSource && configDefs[newSource] && configDefs[newSource].description">
            {{ configDefs[newSource].description }}
            <a
              v-if="configDefs[newSource].link"
              :href="configDefs[newSource].link"
              class="no-decoration"
              target="_blank">
              {{ $t('wise.config.learnMore') }}
            </a>
          </p>
          <span v-if="newSource && configDefs[newSource] && !configDefs[newSource].singleton">
            <v-text-field
              class="input-box mt-2"
              v-model="newSourceName"
              :placeholder="$t('wise.config.sourceNamePlaceholder')" />
          </span>
        </v-card-text>
        <v-card-actions>
          <v-btn
            color="warning"
            size="small"
            @click="showSourceModal = false">
            {{ $t('common.cancel') }}
          </v-btn>
          <v-spacer />
          <v-btn
            :disabled="!newSource || (configDefs[newSource] && !configDefs[newSource].singleton && !newSourceName) || Object.keys(currConfig).includes(newSource + ':' + newSourceName)"
            color="success"
            size="small"
            @click="createNewSource">
            Create
          </v-btn>
        </v-card-actions>
      </v-card>
    </v-dialog> <!-- /add source modal -->

    <!-- import config modal -->
    <v-dialog
      v-model="showImportConfigModal"
      max-width="900"
      @update:model-value="(v) => { if (!v) cancelImportConfig(); }">
      <v-card>
        <v-card-title>Import Config</v-card-title>
        <v-card-text>
          <span v-html="$t('wise.config.learnMoreSourceHtml')" />
          <v-alert
            v-if="!!importConfigError"
            type="error"
            class="my-2">
            {{ importConfigError }}
          </v-alert>
          <v-textarea
            v-model="importConfigText"
            :placeholder="$t('wise.config.importConfigTextPlaceholder')"
            rows="10"
            max-rows="20" />
        </v-card-text>
        <v-card-actions>
          <v-btn
            color="warning"
            size="small"
            @click="cancelImportConfig">
            {{ $t('common.cancel') }}
          </v-btn>
          <v-spacer />
          <div
            class="d-flex align-center"
            style="gap: 8px; max-width: 420px;">
            <v-text-field
              id="config-pin-code-import"
              v-model="configCode"
              :placeholder="$t('wise.config.configCodePlaceholder')"
              density="compact" />
            <v-tooltip activator="#config-pin-code-import">
              {{ $t('wise.config.configCodeTip') }}
            </v-tooltip>
            <v-btn
              color="success"
              size="small"
              :disabled="!importConfigText || !configCode"
              @click="importConfig">
              {{ $t('wise.config.saveRestart') }}
            </v-btn>
          </div>
        </v-card-actions>
      </v-card>
    </v-dialog> <!-- /import config modal -->
  </div>
</template>

<script>
import { mapGetters } from 'vuex';
import JsonEditorVue from 'json-editor-vue';
import VueJsonPretty from 'vue-json-pretty';
import 'vue-json-pretty/lib/styles.css';

import WiseService from './wise.service.js';

let jsonTimeout;
let csvTimeout;
let vaTimeout;

export default {
  name: 'Config',
  components: {
    JsonEditorVue,
    VueJsonPretty
  },
  mounted: function () {
    this.loadConfigDefs();
    this.loadCurrConfig();
    this.loadSourceData();
    this.loadCurrentUser();
  },
  data: function () {
    return {
      alertState: { text: '', variant: '' },
      showSourceModal: false,
      selectedSourceKey: this.$route.query.source ? decodeURI(this.$route.query.source) : 'wiseService',
      configDefs: {},
      loaded: false,
      currConfig: {},
      currConfigBefore: {}, // Used to determine if changes have been made
      currFile: '',
      currFileBefore: '', // Used to determine if changes have been made
      currJSONFile: null,
      currCSV: null,
      displayData: '',
      displayJSON: null,
      showPrettyJSON: false,
      filePath: '',
      newSource: '',
      newSourceName: '',
      configViewSelected: this.$route.query.view || 'config',
      configCode: '',
      showImportConfigModal: false,
      importConfigText: '',
      importConfigError: '',
      rawConfig: false,
      rawCSV: true,
      displayAdvancedFields: {},
      rawValueActions: false,
      currValueActionsFile: [],
      isWiseAdmin: false,
      valueActionsFields: [
        { name: 'key', required: true, class: 'col-md-6', help: 'The unique ID of the value action' },
        { name: 'name', required: true, depends: 'key', class: 'col-md-6', help: 'The name of the value action to show the user' },
        { name: 'category', required: true, depends: 'fields', class: 'col-md-6', help: 'Which category of fields should the value action be shown for, must set fields or category' },
        { name: 'fields', required: true, depends: 'category', class: 'col-md-6', help: 'Which fields to show the value action for, must set fields or category' },
        { name: 'url', required: true, depends: 'func', help: 'The url to send the user, supports special substitutions, must set url or func' },
        { name: 'func', required: true, advanced: true, depends: 'url', help: 'A javascript function body to call, will be passed the name and value and must return the value, must set url or func' },
        { name: 'regex', required: false, advanced: true, help: 'When set, replaces %REGEX% in the url with the match' },
        { name: 'actionType', required: false, advanced: true, help: 'Needs a url. Supported actionTypes: "fetch" (information will be fetched and displayed in the value actions menu for 5 seconds after click), "" (empty, nothing is done on value action click)' },
        { name: 'users', required: false, advanced: true, help: 'A comma separated list of user names that can see the right click item. If not set then all users can see the right click item.' },
        { name: 'notUsers', required: false, advanced: true, help: 'A comma separated list of user names that can NOT see the right click item. This setting is applied before the users setting above.' }
      ]
    };
  },
  computed: {
    ...mapGetters(['getTheme']),
    selectedSourceSplit: function () {
      return this.selectedSourceKey.split(':')[0];
    },
    newSourceItems () {
      const usedKeys = Object.keys(this.currConfig).map(k => k.split(':')[0]);
      return Object.keys(this.configDefs)
        .filter(k => !this.configDefs[k].service)
        .map(source => ({
          value: source,
          title: source,
          disabled: this.configDefs[source].singleton && usedKeys.includes(source)
        }));
    },
    sidebarOptions: function () {
      const options = {};
      // Note: Services are already added to currConfig. This assists rendering them first
      options.services = Object.keys(this.configDefs).filter(key => this.configDefs[key].service);
      options.sources = Object.keys(this.currConfig).filter(key => !options.services.includes(key)).sort();
      return options;
    },
    activeFields: function () {
      if (this.configDefs && this.configDefs[this.selectedSourceSplit] && this.configDefs[this.selectedSourceSplit].fields) {
        return this.configDefs[this.selectedSourceSplit].fields.filter((field) => {
          if (field.ifField === undefined) { return true; }
          if (this.currConfig[this.selectedSourceKey] === undefined) { return false; }
          if (Array.isArray(field.ifValue)) {
            return field.ifValue.includes(this.currConfig[this.selectedSourceKey][field.ifField]);
          } else {
            return this.currConfig[this.selectedSourceKey][field.ifField] === field.ifValue;
          }
        });
      } else {
        return [];
      }
    },
    saveEnabled: function () {
      return JSON.stringify(this.currConfig) !== JSON.stringify(this.currConfigBefore) && this.configCode.length > 0;
    },
    fileResetDisabled: function () {
      return this.currFile === this.currFileBefore;
    },
    fileSaveDisabled: function () {
      return this.currFile === this.currFileBefore;
    },
    configViews: function () {
      const views = [{ text: 'Config', value: 'config' }];

      if (this.configDefs[this.selectedSourceSplit].editable) {
        views.push({ text: 'Edit', value: 'edit' });
      }

      if (this.configDefs[this.selectedSourceSplit].displayable) {
        views.push({ text: 'Display', value: 'display' });
      }

      return views;
    },
    currFormat: function () {
      if (this.configDefs[this.selectedSourceSplit].format) {
        return this.configDefs[this.selectedSourceSplit].format;
      }
      return this.currConfig[this.selectedSourceKey].format
        ? this.currConfig[this.selectedSourceKey].format
        : (this.currCSV ? 'CSV' : undefined);
    }
  },
  watch: {
    selectedSourceKey: function () {
      this.configViewSelected = 'config';
      this.currFile = '';
      this.currFileBefore = '';
      this.displayData = '';
      this.currJSONFile = null;
      this.currCSV = null;
    },
    configViewSelected: function () {
      this.loadSourceData();
    },
    currJSONFile () {
      if (jsonTimeout) { clearTimeout(jsonTimeout); }
      jsonTimeout = setTimeout(() => {
        this.currFile = JSON.stringify(this.currJSONFile, null, 4);
      }, 1000);
    }
  },
  methods: {
    /* Map Bootstrap-era alert variants (danger/warning/info/success) to
       Vuetify v-alert types. */
    alertVariantToType (variant) {
      if (variant === 'danger') return 'error';
      if (variant === 'warning') return 'warning';
      if (variant === 'success') return 'success';
      return 'info';
    },
    loadCurrentUser: function () {
      WiseService.getCurrentUser().then((user) => {
        this.isWiseAdmin = user && Array.isArray(user.roles) && user.roles.includes('wiseAdmin');
      }).catch(() => {
        this.isWiseAdmin = false;
      });
    },
    selectSource: function (sourceKey) {
      this.selectedSourceKey = sourceKey;
      this.$router.push({
        query: { source: sourceKey }
      });
    },
    configViewChanged: function () {
      this.$router.push({
        query: {
          ...this.$route.query,
          view: this.configViewSelected
        }
      });
    },
    loadSourceData: function () {
      if (this.configViewSelected === 'edit') {
        this.loadSourceFile();
      }
      if (this.configViewSelected === 'display') {
        this.loadSourceDisplay();
      }
    },
    createNewSource: function () {
      const key = (this.configDefs && this.configDefs[this.newSource] && !this.configDefs[this.newSource].singleton)
        ? this.newSource + ':' + this.newSourceName
        : this.newSource;

      this.currConfig[key] = {};
      this.selectedSourceKey = key;
      this.showSourceModal = false;
      this.newSource = '';
      this.newSourceName = '';
    },
    parseINI: function (data) {
      // This code is from node-iniparser, MIT license
      const regex = {
        section: /^\s*\[\s*([^\]]*)\s*\]\s*$/,
        param: /^\s*([\w.\-_]+)\s*=\s*(.*?)\s*$/,
        comment: /^\s*[;#].*$/
      };
      const value = {};
      const lines = data.split(/\r\n|\r|\n/);
      let section = null;
      lines.forEach(function (line) {
        if (regex.comment.test(line)) {
          return;
        } else if (regex.param.test(line)) {
          const match = line.match(regex.param);
          if (section) {
            value[section][match[1]] = match[2];
          } else {
            value[match[1]] = match[2];
          }
        } else if (regex.section.test(line)) {
          const match = line.match(regex.section);
          value[match[1]] = {};
          section = match[1];
        }
      });
      return value;
    },
    importConfig: function () {
      // save current config in case of error
      const currConfigBefore = JSON.parse(JSON.stringify(this.currConfig));
      const text = this.importConfigText.trim();
      if (text[0] === '"' || text[0] === '{') {
        // JSON input
        try {
          let json;
          if (text[0] === '"') {
            json = JSON.parse('{' + text + '}');
          } else {
            json = JSON.parse(text);
          }
          this.currConfig = { ...this.currConfig, ...json }; // Shallow merge, with new overriding old
        } catch (e) {
          this.importConfigError = this.$t('wise.config.notJSON');
          return; // Don't clear
        }
      } else if (text.startsWith('[')) {
        // INI Input
        const json = this.parseINI(text);
        this.currConfig = { ...this.currConfig, ...json }; // Shallow merge, with new overriding old
      } else {
        this.importConfigError = this.$t('wise.config.notJSONorINI');
        return; // Don't clear
      }

      this.saveConfig(true).then(() => {
        this.showImportConfigModal = false;
        this.importConfigError = '';
        this.importConfigText = '';
      }).catch((err) => {
        this.importConfigError = err;
        // just set the config back to what is was before import
        this.currConfig = currConfigBefore;
      });
    },
    cancelImportConfig: function () {
      this.showImportConfigModal = false;
      this.importConfigError = '';
      this.importConfigText = '';
    },
    inputChanged: function (val, field) {
      if (val) {
        if (field.multiline) {
          this.currConfig[this.selectedSourceKey][field.name] = val.replace(/\n/g, field.multiline);
        } else {
          this.currConfig[this.selectedSourceKey][field.name] = val;
        }
      } else if (this.currConfig[this.selectedSourceKey][field.name]) {
        delete this.currConfig[this.selectedSourceKey][field.name];
      }
    },
    onJsonChange: function (value) {
      if (jsonTimeout) { clearTimeout(jsonTimeout); }
      jsonTimeout = setTimeout(() => {
        this.currFile = JSON.stringify(value, null, 4);
      }, 1000);
    },
    deleteSource: function () {
      delete this.currConfig[this.selectedSourceKey];
      this.selectedSourceKey = 'wiseService';
    },
    copySource: function (source) {
      const copyContent = this.$refs[`${source}-pre`].innerHTML;
      // create an input to copy from
      const input = document.createElement('textarea');
      document.body.appendChild(input);
      input.value = copyContent;
      input.select();
      document.execCommand('copy', false);
      input.remove();
    },
    inputState: function (inputVal, isReq, regex) {
      if (inputVal && regex && !RegExp(regex).test(inputVal)) {
        return false;
      }

      if (isReq && inputVal) {
        return true;
      } else if (isReq) {
        return false;
      } else {
        return null;
      }
    },
    saveConfig: function (noError) {
      return new Promise((resolve, reject) => {
        // Iterate through user config before saving and test for missing required fields and improper regex
        for (const sourceName in this.currConfig) {
          const defSource = this.configDefs[sourceName.split(':')[0]];

          for (const item of defSource.fields) {
            if (this.currConfig[sourceName][item.name] && item.regex && !RegExp(item.regex).test(this.currConfig[sourceName][item.name])) {
              const errorMsg = this.$t('wise.config.regexErr', { item: item.name, source: sourceName, regex: item.regex });
              if (!noError) { this.alertState = { text: errorMsg, variant: 'danger' }; }
              reject(errorMsg);
            } else if (!this.currConfig[sourceName][item.name] && item.required) {
              const errorMsg = this.$t('wise.config.requiredErr', { item: item.name, source: sourceName });
              if (!noError) { this.alertState = { text: errorMsg, variant: 'danger' }; }
              reject(errorMsg);
            }
          }
        }

        WiseService.saveCurrConfig(this.currConfig, this.configCode).then((data) => {
          if (!data.success) {
            if (!noError) { this.alertState = { text: data.text || 'Config save failed', variant: 'danger' }; }
            reject(data.text || 'Config save failed');
          } else {
            this.alertState = { text: 'Config saved', variant: 'success' };
            // Resync object that tests for changes
            this.currConfigBefore = JSON.parse(JSON.stringify(this.currConfig));
            this.configCode = '';
            resolve();
          }
        }).catch((err) => {
          const errorMsg = err.text || 'Error saving config.';
          if (!noError) { this.alertState = { text: errorMsg, variant: 'danger' }; }
          reject(errorMsg);
        });
      });
    },
    loadConfigDefs: function () {
      WiseService.getConfigDefs()
        .then((data) => {
          this.configDefs = data;
        })
        .catch((err) => {
          this.alertState = {
            text: err.text || 'Error fetching config definitions from wise.',
            variant: 'danger'
          };
        });
    },
    loadCurrConfig: function () {
      WiseService.getCurrConfig()
        .then((data) => {
          if (!data.success) {
            this.alertState = {
              text: data.text || 'Error fetching config from wise.',
              variant: 'danger'
            };
            return;
          }

          if (data.filePath) {
            this.filePath = data.filePath;
          }

          // Always include services even if omitted from config file
          Object.keys(this.configDefs).filter(key => this.configDefs[key].service).forEach((serviceKey) => {
            if (data.config[serviceKey] === undefined) {
              data.config[serviceKey] = {};
            }
          });

          this.currConfig = data.config;
          this.currConfigBefore = JSON.parse(JSON.stringify(data.config));
          this.loaded = true;
        })
        .catch((err) => {
          this.alertState = {
            text: err.text || 'Error fetching current config for wise.',
            variant: 'danger'
          };
        });
    },
    loadSourceFile: function () {
      this.currJSONFile = null;
      this.currValueActionsFile = null;

      WiseService.getSourceFile(this.selectedSourceKey)
        .then((data) => {
          if (!data.success) {
            throw data;
          }

          this.currFile = data.raw;
          this.currFileBefore = data.raw;

          if (this.currFormat === 'valueactions') {
            this.parseValueActions();
            return;
          }

          try { // if it's json, allow it to be
            this.currJSONFile = JSON.parse(data.raw);
          } catch (err) {
            if (this.currFormat !== 'tagger') {
              // it might be a csv file
              this.parseCSV();
            }
          }
        })
        .catch((err) => {
          this.alertState = {
            text: err.text || 'Error fetching source files from wise.',
            variant: 'danger'
          };
        });
    },
    saveSourceFile: function () {
      if (this.currConfigBefore[this.selectedSourceKey] === undefined) {
        this.alertState = {
          text: 'Wise config does not exist. Make sure to save config before the file!',
          variant: 'danger'
        };
        return;
      }

      WiseService.saveSourceFile(this.selectedSourceKey, this.currFile, this.configCode)
        .then((data) => {
          if (!data.success) {
            throw data;
          } else {
            this.alertState = {
              text: `${this.selectedSourceKey} file saved`,
              variant: 'success'
            };
            // Resync file that tests for changes
            this.currFileBefore = this.currFile;
            this.configCode = '';
          }
        })
        .catch((err) => {
          this.alertState = {
            text: err.text || `Error saving wise source file for ${this.selectedSourceKey}.`,
            variant: 'danger'
          };
        });
    },
    loadSourceDisplay: function () {
      WiseService.getSourceDisplay(this.selectedSourceKey)
        .then((data) => {
          this.displayData = data;
          try {
            this.displayJSON = JSON.parse(this.displayData);
          } catch (err) {
            this.displayJSON = null;
          }
        })
        .catch((err) => {
          this.alertState = {
            text: err.text || 'Error fetching source display from wise.',
            variant: 'danger'
          };
        });
    },
    /* VALUE ACTIONS EDITOR ------------------------------------------------ */
    /* Toggles the display of the text area or the value actions editor */
    toggleValueActionsEditor () {
      this.rawValueActions = !this.rawValueActions;
      this.rawValueActions ? this.debounceValueActionsChange() : this.parseValueActions();
    },
    valueActionsInputState (line, value, isReq, depends) {
      if (isReq && value) {
        return true; // required and has a value
      } else if (isReq && depends && line[depends]) {
        return null; // required and depends on a value that's filled out
      } else if (isReq && depends && !line[depends]) {
        return false; // required and depends on a value that's empty
      } else if (isReq) {
        return false; // required and doesn't have a value
      } else {
        return null; // not required
      }
    },
    /* Adds a new empty object to the value actions config for the user to input field values */
    addValueAction () {
      this.currValueActionsFile.push({});
    },
    /**
     * Removes a value actions object and updates the currFile for saving/canceling
     * @param {number} index - The index of the object to remove
     */
    removeValueAction (index) {
      this.currValueActionsFile.splice(index, 1);
      this.debounceValueActionsChange();
    },
    /**
     * Toggles the display of the advanced fields for a value actions object.
     * @param {string} lineKey - The unique key of the value action
     */
    toggleAdvancedFields (lineKey) {
      this.displayAdvancedFields[lineKey] = !this.displayAdvancedFields[lineKey];
    },
    /* Debounces any changes to the value actions config. After 1 second the array
     * of value actions is parsed back into the currFile to be saved/canceled */
    debounceValueActionsChange () {
      if (vaTimeout) { clearTimeout(vaTimeout); }
      vaTimeout = setTimeout(() => {
        vaTimeout = null;
        let fileStr = '';
        for (const line of this.currValueActionsFile) {
          fileStr += `${line.key}=`;
          for (const key in line) {
            if (key !== 'key') {
              fileStr += `${key}:${line[key]};`;
            }
          }
          fileStr = fileStr.slice(0, -1); // remove last ; (don't need it at end of each line)
          fileStr += '\n';
        }
        this.currFile = fileStr;
      }, 1000);
    },
    /* Parses the value actions from the currFile string to an array of
     * value action objects to be edited using the valueActionsFields */
    parseValueActions () {
      const result = [];
      const lines = this.currFile.split('\n');
      for (const line of lines) {
        if (!line) { continue; }
        const keyValArr = line.split(/=(.+)/); // splits on first '='
        const key = keyValArr[0];
        const val = keyValArr[1];
        const values = val.split(';');
        const valuesObj = { key };
        valuesObj.id = Math.floor(Math.random() * 99999); // need id for v-for key
        for (const value of values) {
          const keyVal = value.split(/:(.+)/); // splits on first ':'
          valuesObj[keyVal[0]] = keyVal[1];
        }
        result.push(valuesObj);
      }
      this.currValueActionsFile = result;
    },
    /* CSV EDITOR ---------------------------------------------------------- */
    parseCSV () {
      this.currCSV = {
        rows: [],
        longestRow: [''] // rows have at least one column (for creating new csv)
      };

      if (!this.currFile || this.currFile === '\n') {
        // there must be at least one row (for creating new csv)
        this.currCSV.rows.push(['']);
        return;
      }

      const rows = this.currFile.split('\n');

      for (const row of rows) { // parse rows
        if (!row.length) { continue; } // empty row
        if (row.startsWith('#')) { // comment row
          this.currCSV.rows.push([row]);
          continue;
        }
        // if we support double quotes in csv field values, we should
        // split string by commas but ignore commas between double quotes
        // https://stackoverflow.com/questions/11456850/split-a-string-by-commas-but-ignore-commas-within-double-quotes-using-javascript
        // const cells = row.match(/(".*?"|[^",\s]+)(?=\s*,|\s*$)/g);
        const cells = row.split(',');
        this.currCSV.rows.push(cells);
        this.currCSV.longestRow = cells.length > this.currCSV.longestRow.length ? cells : this.currCSV.longestRow;
      }
    },
    debounceCSVChange () {
      if (csvTimeout) { clearTimeout(csvTimeout); }
      csvTimeout = setTimeout(() => {
        let csvStr = '';
        for (const row of this.currCSV.rows) {
          csvStr += `${row.filter(Boolean).join(',')}\n`; // filter out empty values
        }
        this.currFile = csvStr;
      }, 1000);
    },
    cellEnterClick (rowIndex, colIndex) {
      if (this.currCSV.rows.length - 1 === rowIndex) {
        // need to add a new row
        this.addCSVRow(rowIndex + 1);
      }
      this.$nextTick(() => {
        // otherwise set cursor to the same column in the next row
        const ref = `${rowIndex + 1}-${colIndex}`;
        const cell = document.getElementById(ref);
        if (cell) { cell.focus(); }
      });
    },
    toggleCSVEditor () {
      this.rawCSV = !this.rawCSV;
      this.rawCSV ? this.debounceCSVChange() : this.parseCSV();
    },
    addCSVColumn (colIndex) {
      for (const row of this.currCSV.rows) {
        row.splice(colIndex, 0, '');
        // need to recalculate longest row
        this.currCSV.longestRow = row.length > this.currCSV.longestRow.length ? row : this.currCSV.longestRow;
      }
    },
    removeCSVColumn (colIndex) {
      for (const row of this.currCSV.rows) {
        row.splice(colIndex, 1);
        // need to recalculate longest row
        this.currCSV.longestRow = row.length > this.currCSV.longestRow.length ? row : this.currCSV.longestRow;
      }
    },
    addCSVRow (rowIndex) {
      const newArr = [];
      for (let i = 0; i < this.currCSV.longestRow.length; i++) {
        newArr.push(''); // add empty values
      }

      this.currCSV.rows.splice(rowIndex, 0, newArr);
    },
    removeCSVRow (rowIndex) {
      this.currCSV.rows.splice(rowIndex, 1);
    }
  }
};
</script>

<style scoped>
/* Tighten the vertical tab strip (sources sidebar) to match
   viewer's settings page. Vuetify's compact-density v-tab is ~36px,
   which feels loose for a side nav of dozens of sources. */
.v-tab {
  min-height: 28px !important;
  height: 28px !important;
  padding: 0 12px !important;
  font-size: 0.85rem !important;
  justify-content: flex-start !important;
}
.source-btn {
  width: 100%;
  font-size: .9rem;
  border-radius: 0;
  padding: .2rem .3rem;
  border-color: transparent;
  background-color: transparent;
  box-shadow: none !important; /* Covers all css states that has this defined in bootstrap */
}
.source-btn[active] {
  color: #222;
  background-color: #C3C3C3;
}
.source-btn:hover {
  color: #222;
  border-color: transparent;
  background-color: #DEDEDE;
}

.input-label {
  height: 54px;
  justify-content: center;
}

input.br-0 {
  border-radius: 0;
}

.wrapit {
  white-space: normal;
}

/* csv editor styles */
.csv-editor {
  overflow-x: scroll;
  overflow-y: hidden;
}

input.csv-cell {
  margin-top: -1px;
  width: 130px !important;
  padding: 4px 8px;
  font-size: 0.85rem;
  border: 1px solid rgb(var(--v-theme-outline));
  background: rgb(var(--v-theme-input-bg));
  color: rgb(var(--v-theme-foreground));
}
input.csv-cell:focus {
  outline: none;
  border-color: rgb(var(--v-theme-primary));
}
input.csv-cell.csv-cell--disabled {
  background: rgb(var(--v-theme-input-bg-disabled));
  color: rgb(var(--v-theme-foreground));
  opacity: 0.7;
}
.csv-cell-group {
  display: inline-flex;
  align-items: center;
}
.wise-help-links a {
  color: rgb(var(--v-theme-primary));
  text-decoration: none;
  margin-right: 6px;
}
.wise-help-links a:hover {
  text-decoration: underline;
}

.col-control {
  position: relative;
  right: 30px;
  top: 5px;
  width: 20px;
  height: 20px;
  margin-right: -20px;
}

/* line up add value actions button with the reset/save file buttons */
.value-actions-editor {
  margin-bottom: -47px !important;
}
</style>

<style>
.jsoneditor-poweredBy {
  visibility: hidden !important;
}

.jsoneditor-vue {
  height: 500px;
}
.editor {
  height: 500px;
}

/* move up the dropdown toggle caret in the csv-editor */
.col-control .dropdown-toggle:after {
  vertical-align: 0.55em !important;
}
</style>
