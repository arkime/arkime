<template>
  <!-- container -->
  <div>
    <Error :initialError="error" v-on:clear-initialError="error = ''"/>

    <b-button
      class="ml-auto mr-2"
      style="display: block"
      variant="primary"
      :disabled="saveEnabled"
      @click="saveConfig()"
    >
      Save Config
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

        <div
          class="input-group mb-3"
          v-if="activeFields.length"
          v-for="field in activeFields"
          :key="field.name + '-field'"
        >
          <div class="input-group-prepend">
            <span class="input-group-text">{{ field.name }}</span>
          </div>

          <b-form-input
            v-if="currConfig && currConfig[selectedSourceKey]"
            :state="inputState(!!currConfig[selectedSourceKey][field.name], field.required)"
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
            :state="inputState(!!newSourceName, true)"
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
import Error from './Error';

export default {
  name: 'Config',
  components: {
    Error
  },
  mounted: function () {
    this.loadConfigDefs();
    this.loadCurrConfig();
  },
  data: function () {
    return {
      error: '',
      showSourceModal: false,
      selectedSourceKey: 'wiseService',
      configDefs: {},
      currConfig: {},
      currConfigBefore: {}, // Used to determine if changes have been made
      emptyAndRequired: [], // TODO will break for creating new sources. Redo logic
      filePath: '',
      newSource: '',
      newSourceName: ''
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
          return field.ifField === undefined || this.currConfig[this.selectedSourceKey][field.ifField] === field.ifValue;
        });
      } else {
        return [];
      }
    },
    saveEnabled: function () {
      return JSON.stringify(this.currConfig) === JSON.stringify(this.currConfigBefore);
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
      let uniqueKey = this.selectedSourceKey + '::' + name;

      if (val) {
        // If user inputs required field, clear it from emptyAndRequired if it is there
        if (this.emptyAndRequired.includes(uniqueKey)) {
          this.emptyAndRequired.splice(this.emptyAndRequired.indexOf(uniqueKey), 1);
        }

        this.$set(this.currConfig[this.selectedSourceKey], name, val);
      } else if (this.currConfig[this.selectedSourceKey][name]) {
        if (isReq && !this.emptyAndRequired.includes(uniqueKey)) {
          this.emptyAndRequired.push(uniqueKey);
        }

        this.$delete(this.currConfig[this.selectedSourceKey], name);
      }
    },
    deleteSource: function () {
      this.$delete(this.currConfig, this.selectedSourceKey);
      this.selectedSourceKey = 'wiseService';
    },
    inputState: function (hasVal, isReq) {
      if (isReq && hasVal) {
        return true;
      } else if (isReq) {
        return false;
      } else {
        return null;
      }
    },
    saveConfig: function () {
      if (this.emptyAndRequired.length > 0) {
        let missing = this.emptyAndRequired[0].split('::');
        this.error = missing[1] + ' is required for ' + missing[0];
      } else {
        WiseService.saveCurrConfig(this.currConfig)
          .then((data) => {
            if (!data.success) {
              throw data;
            } else {
              // Resync object that tests for changes
              this.currConfigBefore = JSON.parse(JSON.stringify(this.currConfig));
            }
          })
          .catch((err) => {
            this.error = err.text || `Error saving config for wise.`;
          });
      }
    },
    loadConfigDefs: function () {
      WiseService.getConfigDefs()
        .then((data) => {
          this.error = '';
          this.configDefs = data;
        })
        .catch((error) => {
          this.error = error.text ||
            `Error fetching config definitions from wise.`;
        });
    },
    loadCurrConfig: function () {
      WiseService.getCurrConfig()
        .then((data) => {
          this.error = '';

          if (data.filePath) {
            this.filePath = data.filePath;
          }

          // Always include services even if omitted from config file
          Object.keys(this.configDefs).filter(key => this.configDefs[key].service).forEach((serviceKey) => {
            if (!data.currConfig.hasOwnProperty(serviceKey)) {
              data.currConfig[serviceKey] = {};
            }
          });

          this.currConfig = data.currConfig;
          this.currConfigBefore = JSON.parse(JSON.stringify(data.currConfig));
        })
        .catch((error) => {
          this.error = error.text ||
            `Error fetching current config for wise.`;
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
