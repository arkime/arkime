<template>
  <!-- container -->
  <div>
    <Alert :initialAlert="alertState.text" :variant="alertState.variant" v-on:clear-initialAlert="alertState = {text: '', variant: ''}"/>

    <b-button
      class="ml-auto mr-2"
      style="display: block"
      variant="primary"
      :disabled="saveEnabled"
      @click="saveConfig()"
    >
      Save Config & Restart
    </b-button>

    <div class="d-flex flex-row">
      <!-- Sources sidebar -->
      <div class="d-flex flex-column">
        <div
          v-for="sourceKey in sidebarOptions.services"
          :key="sourceKey + '-tab'"
        >
          <button
            @click="selectedSourceKey = sourceKey"
            type="button"
            class="btn btn-light source-btn btn-outline-dark"
          >
            <h5>
              {{ sourceKey }}
            </h5>
          </button>
        </div>

        <hr class="mx-3"/>

        <div
          v-for="sourceKey in sidebarOptions.sources"
          :key="sourceKey + '-tab'"
        >
          <button
            @click="selectedSourceKey = sourceKey"
            type="button"
            class="btn btn-light source-btn btn-outline-dark"
          >
            <h5>
              {{ sourceKey }}
            </h5>
          </button>
        </div>

        <span class="px-3">
          <hr/>
          <b-button variant="success" class="text-nowrap" @click="showSourceModal = true">
            <b-icon icon="plus" scale="1"></b-icon>
            <span>Add Source</span>
          </b-button>
        </span>
      </div> <!-- /Sources sidebar -->

      <!-- Selected Source Input Fields -->
      <div class="d-flex flex-column px-5 pt-2 w-100">
        <h2 class="text-center">{{selectedSourceKey}}</h2>
        <div v-if="configDefs[selectedSourceSplit]" class="subtext text-center mt-1 mb-4 mx-5">
          <div v-if="configDefs[selectedSourceSplit].description">
            {{ configDefs[selectedSourceSplit].description }}
          </div>

          <div v-if="configDefs[selectedSourceSplit].editable">
            <b-form-radio-group
              v-model="configViewSelected"
              :options="configViews"
              class="mt-1"
              buttons
              button-variant="outline-secondary"
              size="md"
              name="radio-btn-outline"
            >
            </b-form-radio-group>
          </div>
        </div>

        <div v-if="configViewSelected === 'edit'">
          <b-form-textarea
             v-model="currFile"
             rows="18"
           >
           </b-form-textarea>

           <span class="float-right">
             <b-button
              variant="warning"
              class="mx-auto mt-4"
              :disabled="fileActionsDisabled"
              @click="loadSourceFile()">
               Reset File
             </b-button>
             <b-button
              variant="primary"
              class="mx-auto mt-4"
              :disabled="fileActionsDisabled"
              @click="saveSourceFile()">
               Save File
             </b-button>
           </span>
        </div>
        <div v-else>
          <div
            class="input-group mb-3"
            v-for="field in activeFields"
            :key="field.name + '-field'"
          >
            <div class="input-group-prepend">
              <span class="input-group-text">{{ field.name }}</span>
            </div>

            <b-form-input
              v-if="currConfig && currConfig[selectedSourceKey]"
              :state="inputState(currConfig[selectedSourceKey][field.name], field.required, field.regex)"
              class="input-box"
              :value="currConfig[selectedSourceKey][field.name]"
              @input="(val) => inputChanged(val, field.name, field.required)"
              :placeholder="field.help"
              :required="field.required"
              v-b-popover.focus.top="field.help"
            >
            </b-form-input>
          </div>

          <b-button v-if="configDefs && configDefs[selectedSourceSplit] && !configDefs[selectedSourceSplit].service" variant="danger" class="mx-auto mt-4" style="display:block" @click="deleteSource()">
            <b-icon icon="trash" scale="1"></b-icon>
            Delete Source
          </b-button>
        </div>
      </div><!-- /Selected Source Inputs Fields-->
    </div>

    <b-modal
      v-model="showSourceModal"
      title="New Source"
    >
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
            v-model="newSource"
          >
            <option value="" disabled>Select Source</option>
            <option
              v-for="(source) in Object.keys(configDefs).filter(k => !configDefs[k].service)"
              :value="source"
              :key="source + 'Option'"
              :disabled="configDefs[source].singleton && Object.keys(currConfig).map(k => k.split(':')[0]).includes(source)"
            >
              {{ source }}
            </option>
          </select>
        </div>
        <span v-if="newSource && configDefs[newSource] && !configDefs[newSource].singleton">
          <b-form-input
            :state="inputState(newSourceName, true, null)"
            class="input-box mt-2"
            v-model="newSourceName"
            placeholder="Unique name for source"
          >
          </b-form-input>
        </span>
      </b-container>

      <template v-slot:modal-footer>
        <div class="w-100">
          <b-button
            variant="primary"
            size="sm"
            class="float-right"
            @click="showSourceModal = false"
          >
            Close
          </b-button>
          <b-button
            :disabled="!!!newSource || (configDefs[newSource] && !configDefs[newSource].singleton && !!!newSourceName) || Object.keys(currConfig).includes(newSource + ':' + newSourceName)"
            variant="success"
            size="sm"
            class="float-right mr-2"
            @click="createNewSource"
          >
            Create
          </b-button>
        </div>
      </template>
    </b-modal>
  </div>
</template>

<script>
import WiseService from './wise.service';
import Alert from './Alert';

export default {
  name: 'Config',
  components: {
    Alert
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
      currConfig: {},
      currConfigBefore: {}, // Used to determine if changes have been made
      currFile: '',
      currFileBefore: '', // Used to determine if changes have been made
      filePath: '',
      newSource: '',
      newSourceName: '',
      configViewSelected: 'config',
      configViews: [
        { text: 'Config', value: 'config' },
        { text: 'Edit', value: 'edit' }
      ]
    };
  },
  computed: {
    selectedSourceSplit: function () {
      return this.selectedSourceKey.split(':')[0];
    },
    sidebarOptions: function () {
      let options = {};
      // Note: Services are alredy added to currConfig. This assists rendering them first
      options.services = Object.keys(this.configDefs).filter(key => this.configDefs[key].service);
      options.sources = Object.keys(this.currConfig).filter(key => !options.services.includes(key));
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
      return !(JSON.stringify(this.currConfig) !== JSON.stringify(this.currConfigBefore));
    },
    fileActionsDisabled: function () {
      return this.currFile === this.currFileBefore;
    }
  },
  watch: {
    selectedSourceKey: function () {
      this.configViewSelected = 'config';
      this.currFile = '';
      this.currFileBefore = '';
    },
    configViewSelected: function () {
      if (this.configViewSelected === 'edit') {
        this.loadSourceFile();
      }
    }
  },
  methods: {
    createNewSource: function () {
      let key = (this.configDefs && this.configDefs[this.newSource] && !this.configDefs[this.newSource].singleton)
        ? this.newSource + ':' + this.newSourceName
        : this.newSource;

      this.$set(this.currConfig, key, {});
      this.selectedSourceKey = key;
      this.showSourceModal = false;
      this.newSource = '';
      this.newSourceName = '';
    },
    inputChanged: function (val, name, isReq) {
      if (val) {
        this.$set(this.currConfig[this.selectedSourceKey], name, val);
      } else if (this.currConfig[this.selectedSourceKey][name]) {
        this.$delete(this.currConfig[this.selectedSourceKey], name);
      }
    },
    deleteSource: function () {
      this.$delete(this.currConfig, this.selectedSourceKey);
      this.selectedSourceKey = 'wiseService';
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
    saveConfig: function () {
      // Iterate through user config before saving and test for missing required fields and improper regex
      for (const sourceName in this.currConfig) {
        let defSource = this.configDefs[sourceName.split(':')[0]];

        for (const item of defSource.fields) {
          if (this.currConfig[sourceName][item.name] && item.regex && !RegExp(item.regex).test(this.currConfig[sourceName][item.name])) {
            this.alertState = {
              text: "Regex error: '" + item.name + "' for '" + sourceName + "' must match " + item.regex,
              variant: 'alert-danger'
            };
            return;
          } else if (!this.currConfig[sourceName][item.name] && item.required) {
            this.alertState = {
              text: "Required error: '" + sourceName + "' requires '" + item.name + "'",
              variant: 'alert-danger'
            };
            return;
          }
        }
      }

      WiseService.saveCurrConfig(this.currConfig)
        .then((data) => {
          if (!data.success) {
            throw data;
          } else {
            this.alertState = {
              text: `Config saved`,
              variant: 'alert-success'
            };
            // Resync object that tests for changes
            this.currConfigBefore = JSON.parse(JSON.stringify(this.currConfig));
          }
        })
        .catch((err) => {
          this.alertState = {
            text: err.text || `Error saving config for wise.`,
            variant: 'alert-danger'
          };
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
            text: err.text || `Error fetching config definitions from wise.`,
            variant: 'alert-danger'
          };
        });
    },
    loadCurrConfig: function () {
      WiseService.getCurrConfig()
        .then((data) => {
          if (!data.success) {
            this.alertState = {
              text: err.text || `Error fetching config from wise.`,
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
            if (!data.config.hasOwnProperty(serviceKey)) {
              data.config[serviceKey] = {};
            }
          });

          this.currConfig = data.config;
          this.currConfigBefore = JSON.parse(JSON.stringify(data.config));
        })
        .catch((err) => {
          this.alertState = {
            text: err.text || `Error fetching current config for wise.`,
            variant: 'alert-danger'
          };
        });
    },
    loadSourceFile: function () {
      WiseService.getSourceFile(this.selectedSourceKey)
        .then((data) => {
          if (!data.success) {
            throw data;
          }

          this.currFile = data.raw;
          this.currFileBefore = data.raw;
        })
        .catch((err) => {
          this.alertState = {
            text: err.text || `Error fetching source files from wise.`,
            variant: 'alert-danger'
          };
        });
    },
    saveSourceFile: function () {
      WiseService.saveSourceFile(this.selectedSourceKey, this.currFile)
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
          }
        })
        .catch((err) => {
          this.alertState = {
            text: err.text || `Error saving wise source file for ${this.selectedSourceKey}.`,
            variant: 'alert-danger'
          };
        });
    }
  }
};
</script>

<style scoped>
/* TODO: verify all the css works for dark mode */
.source-btn {
  width: 100%;
  border-radius: 0;
  background: white;
  border-color: white;
  box-shadow: none !important; /* Covers all css states that has this defined in boostrap */
}
.input-label {
  height: 54px;
  justify-content: center;
}
</style>
