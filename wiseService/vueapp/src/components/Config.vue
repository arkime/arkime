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
          v-for="sourceKey in sidebarOptions"
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

          <hr class="mx-3" v-if="sourceKey === 'wiseService'"/>
        </div>

        <span class="px-3">
          <hr/>
          <b-button variant="success" class="text-nowrap" @click="showSourceModal = true">
            <b-icon icon="plus" scale="1"></b-icon>
            <span>Add Source</span>
          </b-button>
        </span>
      </div> <!-- /Sources sidebar -->

      <!-- Selected Source Inputs -->
      <div class="d-flex flex-column px-5 pt-2 w-100">
        <h2 class="text-center">{{selectedSourceKey}}</h2>

        <div class="d-flex flex-row" v-if="configDefs && configDefs[selectedSourceSplit] && configDefs[selectedSourceSplit].fields">
          <div class="d-flex flex-column">
            <div
              v-for="field in configDefs[selectedSourceSplit].fields"
              :key="field.name + '-label'"
              class="d-flex flex-column text-nowrap py-2 align-items-left mr-2 input-label"
            >
              {{ field.name }} :
            </div>
          </div>

          <div class="d-flex flex-column w-100">
            <div
              v-for="field in configDefs[selectedSourceSplit].fields"
              :key="field.name + '-input'"
              class="d-flex flex-column py-2"
            >
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
          </div>
        </div>

        <b-button v-if="selectedSourceKey !== 'wiseService'" variant="danger" class="mx-auto mt-4" style="display:block" @click="deleteSource()">
          <b-icon icon="trash" scale="1"></b-icon>
          Delete Source
        </b-button>
      </div><!-- /Selected Source Inputs -->
    </div>

    <b-modal
      v-model="showSourceModal"
      title="New Source"
    >
      <b-container fluid>
        <!-- <select
          <option v-for="source in Object.keys(configDefs.sources)" :value="type" :key="source + 'Option'">
            {{ type }}
          </option>
        </select> -->
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
            variant="success"
            size="sm"
            class="float-right mr-2"
            @click=""
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
      currConfigBefore: {}, //Used to determine if changes have been made
      emptyAndRequired: [],
      filePath: ""
    };
  },
  computed: {
    selectedSourceSplit: function () {
      return this.selectedSourceKey.split(':')[0];
    },
    sidebarOptions: function () {
      // Sort wiseService to first option in sidebar
      let opts = Object.keys(this.currConfig);
      let wiseIndex = opts.indexOf('wiseService');

      if (wiseIndex >= 0) {
        opts.splice(wiseIndex, 1);
      }

      opts.unshift('wiseService');
      return opts;
    },
    saveEnabled: function () {
      return JSON.stringify(this.currConfig) === JSON.stringify(this.currConfigBefore);
    }
  },
  methods: {
    inputChanged: function (val, name, isReq) {
      let uniqueKey = this.selectedSourceKey + '::' + name;

      if (val) {
        // If user inputs required field, clear it from emptyAndRequired if it is there
        if (this.emptyAndRequired.includes(uniqueKey)) {
          this.emptyAndRequired.splice(this.emptyAndRequired.indexOf(uniqueKey), 1);
        }

        this.$set(this.currConfig[this.selectedSourceKey], name, val)
      } else if (this.currConfig[this.selectedSourceKey][name]) {
        if (isReq && !this.emptyAndRequired.includes(uniqueKey)) {
          this.emptyAndRequired.push(uniqueKey)
        }

        this.$delete(this.currConfig[this.selectedSourceKey], name);
      }
    },
    deleteSource: function () {
      this.$delete(this.currConfig, this.selectedSourceKey)
      this.selectedSourceKey = 'wiseService';
    },
    inputState: function(hasVal, isReq) {
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
        let missing = this.emptyAndRequired[0].split("::");
        this.error = missing[1] + ' is required for ' + missing[0];
      } else {
        // TODO
        // this.currConfigBefore = JSON.parse(JSON.stringify(this.currConfig));
        // post this.currConfig
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
            this.filePath = data.filePath
          }

          // Add wiseService regardless if its in the ini.
          if (!data.currConfig.hasOwnProperty('wiseService')) {
            data.currConfig['wiseService'] = {}
          }

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
