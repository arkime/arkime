<template>
  <!-- container -->
  <div>
    <div class="ml-5 mr-5">
      <Alert
        :initialAlert="alertState.text"
        :variant="alertState.variant"
        v-on:clear-initialAlert="alertState.text = ''"
      />
    </div>

    <div class="d-flex flex-row" v-if="loaded">
      <!-- Sources sidebar -->
      <div class="d-flex flex-column">
        <div
          v-for="sourceKey in sidebarOptions.services"
          :key="sourceKey + '-tab'">
          <button
            type="button"
            @click="selectedSourceKey = sourceKey"
            :active="selectedSourceKey === sourceKey"
            class="btn btn-light source-btn btn-outline-dark">
            {{ sourceKey }}
          </button>
        </div>

        <hr class="mx-3" v-if="sidebarOptions.sources.length">

        <div
          v-for="sourceKey in sidebarOptions.sources"
          :key="sourceKey + '-tab'">
          <button
            type="button"
            @click="selectedSourceKey = sourceKey"
            :active="selectedSourceKey === sourceKey"
            class="btn btn-light source-btn btn-outline-dark">
            {{ sourceKey }}
          </button>
        </div>

        <span class="px-3">
          <hr/>
          <b-button
            block
            variant="warning"
            class="text-nowrap mt-3"
            @click="showImportConfigModal = true"
            v-b-tooltip.hover.right="'Import a new Source Configuration (JSON or INI)'">
            <b-icon-download />
            <span>Import</span>
          </b-button>
          <b-button
            block
            variant="success"
            class="text-nowrap"
            @click="showSourceModal = true"
            v-b-tooltip.hover.right="'Create a new Source through the UI'">
            <b-icon-plus />
            <span>Create</span>
          </b-button>
        </span>
      </div> <!-- /Sources sidebar -->

      <!-- Selected Source Input Fields -->
      <div class="d-flex flex-column px-5 pt-2 w-100">
        <h2>
          <form v-if="configViewSelected === 'edit'"
            class="form-inline pull-right ml-5">
            <b-button
              class="mr-2"
              variant="warning"
              :disabled="fileResetDisabled"
              @click="loadSourceFile">
              Reset File
            </b-button>
            <b-button
              variant="primary"
              :disabled="fileSaveDisabled"
              @click="saveSourceFile">
              Save File
            </b-button>
          </form>
          <form v-else-if="configViewSelected === 'config'"
            class="form-inline pull-right ml-5">
            <div class="input-group">
              <input type="text"
                class="form-control"
                v-model="configCode"
                placeholder="Config pin code"
                v-b-tooltip.hover.left
                title="The config pin code can be found in the output from running the WISE UI"
              />
              <div class="input-group-append">
                <b-button
                  class="ml-auto"
                  variant="primary"
                  :disabled="!saveEnabled"
                  @click="saveConfig">
                  Save Config &amp; Restart
                </b-button>
              </div>
            </div>
          </form>
          {{ selectedSourceKey }}
        </h2>
        <div v-if="configDefs[selectedSourceSplit]" class="subtext mt-1 mb-4">
          <div v-if="configDefs[selectedSourceSplit].description">
            {{ configDefs[selectedSourceSplit].description }}
            <a v-if="configDefs[selectedSourceSplit].link"
              :href="configDefs[selectedSourceSplit].link"
              class="no-decoration"
              target="_blank">
              Learn More!
            </a>
          </div>

          <div v-if="configDefs[selectedSourceSplit].editable || configDefs[selectedSourceSplit].displayable">
            <b-form-radio-group
              v-model="configViewSelected"
              :options="configViews"
              class="mt-1"
              buttons
              button-variant="outline-secondary"
              size="md"
              name="radio-btn-outline">
            </b-form-radio-group>
            <b-form-checkbox
              switch
              v-model="showPrettyJSON"
              v-if="configViewSelected === 'display' && displayJSON">
              Format JSON
            </b-form-checkbox>
            <template v-if="configViewSelected === 'config'">
              <b-button
                class="pull-right"
                :pressed="rawConfig"
                variant="outline-info"
                @click="rawConfig = !rawConfig">
                View {{ rawConfig ? 'Config Fields' : 'Raw Config' }}
              </b-button>
            </template>
            <template v-if="configViewSelected === 'edit' && currCSV">
              <b-button
                class="pull-right"
                variant="outline-info"
                @click="toggleCSVEditor">
                Use {{ rawCSV ? 'CSV Editor' : 'CSV Text Area' }}
              </b-button>
            </template>
          </div>
        </div>

        <div v-if="configViewSelected === 'edit'">
          <p>
            This config uses {{ currConfig[selectedSourceKey].format ? currConfig[selectedSourceKey].format : (currCSV ? 'csv' : 'an unknown') }} format
            <template v-if="currConfig[selectedSourceKey].format === 'tagger'">
              -
              <a target="_blank"
                class="no-decoration"
                href="https://arkime.com/taggerformat">
                learn more here
              </a>
            </template>
          </p>
          <!-- text area input for tagger or csv formats (if user is not using the csv editor) -->
          <template
            v-if="(!currJSONFile && currConfig[selectedSourceKey].format === 'tagger') || rawCSV">
            <b-form-textarea
              v-model="currFile"
              rows="18"
            />
          </template>
          <!-- json editor -->
          <vue-json-editor
            v-else-if="currJSONFile"
            v-model="currJSONFile"
            :mode="'code'"
            :show-btns="false"
            :expandedOnStart="true"
            @json-change="onJsonChange"
          />
          <div v-else-if="currCSV && !rawCSV">
            <div class="mb-2 border-bottom pb-2">
              <b-button
                size="sm"
                variant="outline-success"
                @click="addCSVRow(-1)"
                v-b-tooltip.hover="'Add a new row at the beginning'">
                <b-icon-plus /> Add Row
              </b-button>
            </div>
            <b-form inline
              v-for="(row, rowIndex) in currCSV"
              :key="rowIndex + 'csvrow'"
              class="border-bottom mb-2">
              <b-button
                size="sm"
                class="mr-2 mb-2"
                variant="outline-success"
                @click="addCSVCell(rowIndex, -1)"
                v-b-tooltip.hover="'Add new cell at the beginning of this row'">
                <b-icon-plus />
              </b-button>
              <template v-for="(cell, cellIndex) in row">
                <b-input-group
                  class="mr-2 mb-2"
                  :key="cellIndex + 'csvcell'">
                  <b-form-input
                    size="sm"
                    v-model="currCSV[rowIndex][cellIndex]"
                    @input="debounceCSVChange"
                  />
                  <b-input-group-append>
                    <b-button
                      size="sm"
                      variant="outline-danger"
                      @click="removeCSVCell(rowIndex, cellIndex)"
                      v-b-tooltip.hover="'Remove this cell'">
                      <b-icon-dash />
                    </b-button>
                    <b-button
                      size="sm"
                      variant="outline-success"
                      @click="addCSVCell(rowIndex, cellIndex)"
                      v-b-tooltip.hover="'Add a new cell after this one'">
                      <b-icon-plus />
                    </b-button>
                  </b-input-group-append>
                </b-input-group>
              </template>
              <b-button
                size="sm"
                class="mr-2 mb-2"
                variant="outline-danger"
                @click="removeCSVRow(rowIndex)"
                v-b-tooltip.hover="'Remove this row'">
                <b-icon-dash /> Remove Row
              </b-button>
              <b-button
                size="sm"
                class="mr-2 mb-2"
                variant="outline-success"
                @click="addCSVRow(rowIndex)"
                v-b-tooltip.hover="'Add a row after this one'">
                <b-icon-plus /> Add Row
              </b-button>
            </b-form>
            <b-button
              size="sm"
              variant="outline-success"
              @click="addCSVRow(rowIndex)"
              v-b-tooltip.hover="'Add a new row at the end'">
              <b-icon-plus /> Add Row
            </b-button>
          </div>
          <p v-else
            class="text-danger">
            We couldn't parse your config file. It might be in a format we do
            not support. Please see our
            <a href="https://arkime.com/wise"
              target="_blank"
              class="no-decoration">
              WISE Documentation</a>
            for more information on WISE Source Configuration.
          </p>
        </div> <!-- edit -->

        <!-- display -->
        <div v-else-if="configViewSelected === 'display'">
          <template v-if="showPrettyJSON">
            <vue-json-pretty
              :data="displayJSON"
              :show-line="true"
              :show-double-quotes="false"
            />
          </template>
          <template v-else>
            <pre>{{ displayData }}</pre>
          </template>
       </div> <!-- /display -->

        <div v-else>
          <template v-if="!rawConfig">
            <div
              class="input-group input-group-sm mb-3"
              v-for="field in activeFields"
              :key="field.name + '-field'">
              <div class="input-group-prepend">
                <span class="input-group-text">{{ field.name }}</span>
              </div>
              <b-form-input
                v-if="currConfig && currConfig[selectedSourceKey] && field.multiline === undefined"
                :state="inputState(currConfig[selectedSourceKey][field.name], field.required, field.regex)"
                class="input-box"
                :value="currConfig[selectedSourceKey][field.name]"
                @input="(val) => inputChanged(val, field)"
                :placeholder="field.help"
                :required="field.required"
                v-b-popover.hover.top="field.help"
              />
              <b-form-textarea
                v-if="currConfig && currConfig[selectedSourceKey] && field.multiline !== undefined"
                :state="inputState(currConfig[selectedSourceKey][field.name], field.required, field.regex)"
                class="input-box"
                :value="(currConfig[selectedSourceKey][field.name] || '').split(field.multiline).join('\n')"
                @input="(val) => inputChanged(val, field)"
                :placeholder="field.help"
                :required="field.required"
                v-b-popover.hover.top="field.help"
              />
            </div>
          </template>
          <pre v-show="rawConfig"
            class="mt-4 mb-4"
            :ref="selectedSourceKey + '-pre'"
            style="white-space:break-spaces;word-break:break-all;">{{ currConfig[selectedSourceKey] }}</pre>
          <b-button v-if="configDefs && configDefs[selectedSourceSplit] && !configDefs[selectedSourceSplit].service"
            variant="success" class="mx-auto mt-4" @click="copySource(selectedSourceKey)">
            <b-icon icon="files" scale="1"></b-icon>
            Copy Raw Source
          </b-button>
          <b-button v-if="configDefs && configDefs[selectedSourceSplit] && !configDefs[selectedSourceSplit].service"
            variant="danger" class="mx-auto mt-4 pull-right" @click="deleteSource()">
            <b-icon icon="trash" scale="1"></b-icon>
            Delete Source
          </b-button>
        </div> <!-- else -->
      </div><!-- /Selected Source Inputs Fields-->
    </div>

    <!-- add source modal -->
    <b-modal
      v-model="showSourceModal"
      title="New Source"
      :header-bg-variant="getTheme"
      :header-text-variant="getTheme === 'dark' ? 'light' : 'dark'"
      :body-bg-variant="getTheme"
      :body-text-variant="getTheme === 'dark' ? 'light' : 'dark'"
      :footer-bg-variant="getTheme"
      :footer-text-variant="getTheme === 'dark' ? 'light' : 'dark'">
      <b-container fluid>
        <div class="input-group">
          <span class="input-group-prepend cursor-help"
            placement="topright"
            v-b-tooltip.hover
            title="Source selection (some are allowed only once)">
            <span class="input-group-text">
              Source
            </span>
          </span>
          <select
            class="form-control"
            v-model="newSource">
            <option value="" disabled>Select Source</option>
            <option
              v-for="(source) in Object.keys(configDefs).filter(k => !configDefs[k].service)"
              :value="source"
              :key="source + 'Option'"
              :disabled="configDefs[source].singleton && Object.keys(currConfig).map(k => k.split(':')[0]).includes(source)">
              {{ source }}
            </option>
          </select>
        </div>
        <p v-if="newSource && configDefs[newSource] && configDefs[newSource].description">
          {{ configDefs[newSource].description }}
          <a v-if="configDefs[newSource].link"
            :href="configDefs[newSource].link"
            class="no-decoration"
            target="_blank">
            Learn More!
          </a>
        </p>
        <span v-if="newSource && configDefs[newSource] && !configDefs[newSource].singleton">
          <b-form-input
            :state="inputState(newSourceName, true, null)"
            class="input-box mt-2"
            v-model="newSourceName"
            placeholder="Unique name for source">
          </b-form-input>
        </span>
      </b-container>

      <template v-slot:modal-footer>
        <div class="w-100">
          <b-button
            variant="warning"
            size="sm"
            class="float-left"
            @click="showSourceModal = false">
            Cancel
          </b-button>
          <b-button
            :disabled="!!!newSource || (configDefs[newSource] && !configDefs[newSource].singleton && !!!newSourceName) || Object.keys(currConfig).includes(newSource + ':' + newSourceName)"
            variant="success"
            size="sm"
            class="float-right mr-2"
            @click="createNewSource">
            Create
          </b-button>
        </div>
      </template>
    </b-modal> <!-- /add source modal -->

    <!-- import config modal -->
    <b-modal size="xl"
      v-model="showImportConfigModal"
      title="Import Config"
      :header-bg-variant="getTheme"
      :header-text-variant="getTheme === 'dark' ? 'light' : 'dark'"
      :body-bg-variant="getTheme"
      :body-text-variant="getTheme === 'dark' ? 'light' : 'dark'"
      :footer-bg-variant="getTheme"
      :footer-text-variant="getTheme === 'dark' ? 'light' : 'dark'">
      <b-container fluid>
        <p>
          Learn more about WISE Source configurations
          <a href="https://arkime.com/wise#common-source-settings"
            target="_blank">here</a> and view examples
          <a href="https://arkime.com/wise-configs"
            target="_blank">here</a>.
        </p>
        <b-alert variant="danger"
          :show="!!importConfigError">
          {{ importConfigError }}
        </b-alert>
        <b-form-textarea
          v-model="importConfigText"
          placeholder="Paste your JSON or INI config here..."
          rows="10"
          max-rows="20"
        />
      </b-container>
      <template v-slot:modal-footer>
        <div class="w-100">
          <b-button
            variant="warning"
            size="sm"
            class="float-left"
            @click="cancelImportConfig">
            Cancel
          </b-button>
          <form class="form-inline pull-right ml-5">
            <div class="input-group input-group-sm">
              <input type="text"
                class="form-control"
                v-model="configCode"
                placeholder="Config pin code"
                v-b-tooltip.hover.left
                title="The config pin code can be found in the output from running the WISE UI"
              />
              <div class="input-group-append">
                <b-button
                  class="ml-auto"
                  variant="success"
                  :disabled="!importConfigText || !configCode"
                  @click="importConfig">
                  Save Config &amp; Restart
                </b-button>
              </div>
            </div>
          </form>
        </div>
      </template>
    </b-modal> <!-- /import config modal -->

  </div>
</template>

<script>
import { mapGetters } from 'vuex';
import vueJsonEditor from 'vue-json-editor';
import VueJsonPretty from 'vue-json-pretty';
import 'vue-json-pretty/lib/styles.css';

import WiseService from './wise.service';
import Alert from './Alert';

let jsonTimeout;
let csvTimeout;

export default {
  name: 'Config',
  components: {
    Alert,
    vueJsonEditor,
    VueJsonPretty
  },
  mounted: function () {
    this.loadConfigDefs();
    this.loadCurrConfig();
  },
  data: function () {
    return {
      alertState: { text: '', variant: '' },
      showSourceModal: false,
      selectedSourceKey: 'wiseService',
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
      configViewSelected: 'config',
      configCode: '',
      showImportConfigModal: false,
      importConfigText: '',
      importConfigError: '',
      rawConfig: false,
      rawCSV: false
    };
  },
  computed: {
    ...mapGetters(['getTheme']),
    selectedSourceSplit: function () {
      return this.selectedSourceKey.split(':')[0];
    },
    sidebarOptions: function () {
      const options = {};
      // Note: Services are alredy added to currConfig. This assists rendering them first
      options.services = Object.keys(this.configDefs).filter(key => this.configDefs[key].service);
      options.sources = Object.keys(this.currConfig).filter(key => !options.services.includes(key)).sort();
      return options;
    },
    activeFields: function () {
      if (this.configDefs && this.configDefs[this.selectedSourceSplit] && this.configDefs[this.selectedSourceSplit].fields) {
        return this.configDefs[this.selectedSourceSplit].fields.filter((field) => {
          if (field.ifField === undefined) { return true; }
          if (this.currConfig[this.selectedSourceKey] === undefined) { return false; }
          return this.currConfig[this.selectedSourceKey][field.ifField] === field.ifValue;
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
      if (this.configViewSelected === 'edit') {
        this.loadSourceFile();
      }
      if (this.configViewSelected === 'display') {
        this.loadSourceDisplay();
      }
    }
  },
  methods: {
    createNewSource: function () {
      const key = (this.configDefs && this.configDefs[this.newSource] && !this.configDefs[this.newSource].singleton)
        ? this.newSource + ':' + this.newSourceName
        : this.newSource;

      this.$set(this.currConfig, key, {});
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
        };
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
          this.importConfigError = 'Not valid JSON';
          return; // Don't clear
        }
      } else if (text.startsWith('[')) {
        // INI Input
        const json = this.parseINI(text);
        this.currConfig = { ...this.currConfig, ...json }; // Shallow merge, with new overriding old
      } else {
        this.importConfigError = 'Doesn\'t look like JSON or INI';
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
          this.$set(this.currConfig[this.selectedSourceKey], field.name, val.replace(/\n/g, field.multiline));
        } else {
          this.$set(this.currConfig[this.selectedSourceKey], field.name, val);
        }
      } else if (this.currConfig[this.selectedSourceKey][field.name]) {
        this.$delete(this.currConfig[this.selectedSourceKey], field.name);
      }
    },
    onJsonChange: function (value) {
      if (jsonTimeout) { clearTimeout(jsonTimeout); }
      jsonTimeout = setTimeout(() => {
        this.currFile = JSON.stringify(value, null, 4);
      }, 1000);
    },
    deleteSource: function () {
      this.$delete(this.currConfig, this.selectedSourceKey);
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
              const errorMsg = `Regex error: "${item.name}" for "${sourceName}" must match ${item.regex}`;
              if (!noError) { this.alertState = { text: errorMsg, variant: 'alert-danger' }; }
              reject(errorMsg);
            } else if (!this.currConfig[sourceName][item.name] && item.required) {
              const errorMsg = `Required error: "${sourceName}" requires "${item.name}"`;
              if (!noError) { this.alertState = { text: errorMsg, variant: 'alert-danger' }; }
              reject(errorMsg);
            }
          }
        }

        WiseService.saveCurrConfig(this.currConfig, this.configCode).then((data) => {
          if (!data.success) {
            reject(data.text || 'Config save failed');
          } else {
            this.alertState = { text: 'Config saved', variant: 'alert-success' };
            // Resync object that tests for changes
            this.currConfigBefore = JSON.parse(JSON.stringify(this.currConfig));
            this.configCode = '';
            resolve();
          }
        }).catch((err) => {
          const errorMsg = err.text || 'Error savign config.';
          if (!noError) { this.alertState = { text: errorMsg, variant: 'alert-danger' }; }
          reject(errorMsg);
        });
      });
    },
    loadConfigDefs: function () {
      WiseService.getConfigDefs()
        .then((data) => {
          this.alertState = { text: '', variant: '' };
          this.configDefs = data;
        })
        .catch((err) => {
          this.alertState = {
            text: err.text || 'Error fetching config definitions from wise.',
            variant: 'alert-danger'
          };
        });
    },
    loadCurrConfig: function () {
      WiseService.getCurrConfig()
        .then((data) => {
          if (!data.success) {
            this.alertState = {
              text: data.text || 'Error fetching config from wise.',
              variant: 'alert-danger'
            };
            return;
          }

          this.alertState = { text: '', variant: '' };

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
            variant: 'alert-danger'
          };
        });
    },
    loadSourceFile: function () {
      this.currJSONFile = null;

      WiseService.getSourceFile(this.selectedSourceKey)
        .then((data) => {
          if (!data.success) {
            throw data;
          }

          this.currFile = data.raw;
          this.currFileBefore = data.raw;

          try { // if it's json, allow it to be
            this.currJSONFile = JSON.parse(data.raw);
          } catch (err) {
            this.currJSONFile = null;
            if (this.currConfig[this.selectedSourceKey].format !== 'tagger') {
              // it might be a csv file
              this.parseCSV();
            }
          }
        })
        .catch((err) => {
          this.alertState = {
            text: err.text || 'Error fetching source files from wise.',
            variant: 'alert-danger'
          };
        });
    },
    parseCSV () {
      this.currCSV = [];
      const rows = this.currFile.split('\n');
      for (const row of rows) {
        if (!row.length) { continue; } // emtpy row
        if (row.startsWith('#')) { // comment row
          this.currCSV.push([row]);
          continue;
        }
        const cells = row.split(',');
        this.currCSV.push(cells);
      }
    },
    debounceCSVChange () {
      if (csvTimeout) { clearTimeout(csvTimeout); }
      csvTimeout = setTimeout(() => {
        let csvStr = '';
        for (const row of this.currCSV) {
          csvStr += `${row.join(',')}\n`;
        }
        this.currFile = csvStr;
      }, 1000);
    },
    toggleCSVEditor () {
      this.rawCSV = !this.rawCSV;
      this.rawCSV ? this.debounceCSVChange() : this.parseCSV();
    },
    addCSVCell (rowIndex, cellIndex) {
      this.currCSV[rowIndex].splice(cellIndex + 1, 0, '');
      this.debounceCSVChange();
    },
    removeCSVCell (rowIndex, cellIndex) {
      this.currCSV[rowIndex].splice(cellIndex, 1);
      this.debounceCSVChange();
    },
    addCSVRow (rowIndex) {
      this.currCSV.splice(rowIndex + 1, 0, ['']);
      this.debounceCSVChange();
    },
    removeCSVRow (rowIndex) {
      this.currCSV.splice(rowIndex, 1);
      this.debounceCSVChange();
    },
    saveSourceFile: function () {
      if (this.currConfigBefore[this.selectedSourceKey] === undefined) {
        this.alertState = {
          text: 'Wise config does not exist. Make sure to save config before the file!',
          variant: 'alert-danger'
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
              variant: 'alert-success'
            };
            // Resync file that tests for changes
            this.currFileBefore = this.currFile;
            this.configCode = '';
          }
        })
        .catch((err) => {
          this.alertState = {
            text: err.text || `Error saving wise source file for ${this.selectedSourceKey}.`,
            variant: 'alert-danger'
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
            variant: 'alert-danger'
          };
        });
    }
  }
};
</script>

<style scoped>
.source-btn {
  width: 100%;
  font-size: .9rem;
  border-radius: 0;
  padding: .2rem .3rem;
  border-color: transparent;
  background-color: transparent;
  box-shadow: none !important; /* Covers all css states that has this defined in boostrap */
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
</style>
