<template>

  <!-- TODO transitions for btn drawer, field toggles, field values, field config items -->
  <div class="spiview-page">

    <!-- search navbar -->
    <moloch-search
      :num-matching-sessions="filtered"
      :timezone="settings.timezone"
      @changeSearch="changeSearch">
    </moloch-search> <!-- /search navbar -->

    <!-- info navbar -->
    <form class="info-nav">
      <div v-if="!dataLoading">
        <!-- field config save button -->
        <b-dropdown
          size="sm"
          no-caret
          class="field-config-menu"
          toggle-class="rounded"
          variant="theme-secondary">
          <template slot="button-content">
            <span class="fa fa-columns"
              v-b-tooltip.hover
              title="Save or load custom visible field configurations">
            </span>
          </template>
          <b-dropdown-header>
            <div class="input-group input-group-sm">
              <input type="text"
                maxlength="30"
                class="form-control"
                v-model="newFieldConfigName"
                placeholder="Enter new field configuration name"
                @keydown.enter.stop.prevent="saveFieldConfiguration"
              />
              <div class="input-group-append">
                <button type="button"
                  class="btn btn-theme-secondary"
                  :disabled="!newFieldConfigName"
                  @click="saveFieldConfiguration"
                  v-b-tooltip.hover
                  title="Save this custom spiview field configuration">
                  <span class="fa fa-save">
                  </span>
                </button>
              </div>
            </div>
          </b-dropdown-header>
          <b-dropdown-divider>
          </b-dropdown-divider>
          <b-dropdown-item
            v-if="fieldConfigError"
            class="text-danger">
            {{ fieldConfigError }}
          </b-dropdown-item>
          <b-dropdown-item
            v-b-tooltip.hover
            @click.stop.prevent="loadFieldConfiguration()"
            title="Reset visible fields to the default fields: Dst IP, Src IP, and Protocols">
            Moloch Default
          </b-dropdown-item>
          <b-dropdown-item
            v-for="(config, key) in fieldConfigs"
            :key="key"
            @click.self.stop.prevent="loadFieldConfiguration(key)">
            <button class="btn btn-xs btn-danger pull-right"
              type="button"
              @click.stop.prevent="deleteFieldConfiguration(config.name, key)">
              <span class="fa fa-trash-o">
              </span>
            </button>
            {{ config.name }}
          </b-dropdown-item>
        </b-dropdown> <!-- /field config save button -->
        <small>
          <strong class="ml-2 text-theme-accent"
            v-if="!error && filtered !== undefined">
            Showing {{ filtered | commaString }} entries filtered from
            {{ total | commaString }} total entries
          </strong>
        </small>
      </div>
      <div v-if="dataLoading"
        class="info-nav-loading">
        <span class="fa fa-spinner fa-lg fa-spin">
        </span>&nbsp;
        <em>
          Loading SPI data
        </em>
        <button type="button"
          class="btn btn-warning btn-sm pull-right"
          @click="cancelLoading()">
          <span class="fa fa-ban">
          </span>&nbsp;
          cancel
        </button>
      </div>
    </form> <!-- /info navbar -->

    <!-- warning navbar -->
    <form v-if="staleData && !dataLoading"
      class="loading-nav">
      <div class="form-inline text-theme-accent">
        <span class="fa fa-exclamation-triangle">
        </span>&nbsp;
        <strong>Warning:</strong>
        much of the data below does not match your query
        because the request was canceled.
        <em>
          Click search to reissue your query.
        </em>
        <span class="fa fa-close pull-right cursor-pointer"
          @click="staleData = false">
        </span>
      </div>
    </form> <!-- /warning navbar -->

    <div class="spiview-content mr-1 ml-1">

    <!-- session visualizations -->
      <div class="spiview-visualizations">
        <moloch-visualizations
          v-if="mapData && graphData"
          :graph-data="graphData"
          :map-data="mapData"
          :primary="true"
          :timezone="settings.timezone">
        </moloch-visualizations>
      </div> <!-- /session visualizations -->

      <!-- page error -->
      <moloch-error
        v-if="error"
        :message="error"
        class="mt-5 mb-5">
      </moloch-error> <!-- /page error -->

      <!-- spiview panels -->
      <div role="tablist">
        <b-card no-body
          class="mb-1"
          v-for="category in categoryList"
          :key="category">
          <b-card-header
            header-tag="header"
            class="pt-1 pb-1 pl-2 pr-2 cursor-pointer"
            v-b-toggle="category"
            @click="toggleCategory(category)">
            <strong class="category-title">
              {{ category }}
            </strong>
            <span class="when-opened mt-2 fa fa-minus pull-right">
            </span>
            <span class="when-closed mt-2 fa fa-plus pull-right">
            </span>
            <span v-if="categoryObjects[category].loading"
              class="fa fa-spin fa-spinner fa-lg pull-right mt-1 mr-1">
            </span>
            <span v-if="!categoryObjects[category].loading">
              <button class="btn btn-theme-secondary btn-sm pull-right mr-1"
                title="Load all of the values in this category"
                @click.stop.prevent="toggleAllValues(category, true)">
                Load All
              </button>
              <button class="btn btn-theme-primary btn-sm pull-right mr-1"
                title="Unload all of the values in this category"
                @click.stop.prevent="toggleAllValues(category, false)">
                Unload All
              </button>
            </span>
            <span v-if="categoryObjects[category].protocols"
              class="pull-right">
              <span v-for="(value, key) in categoryObjects[category].protocols"
                :key="key"
                @click.stop
                class="protocol-value">
                <strong>
                  <moloch-session-field
                    :field="{dbField:'ipProtocol', exp:'ip.protocol', type:'lotermfield', group:'general', transform:'ipProtocolLookup'}"
                    :expr="'ip.protocol'"
                    :value="key"
                    :pull-left="true"
                    :parse="false"
                    :session-btn="true">
                  </moloch-session-field>
                </strong>
                <sup>({{ value | round(0) }})</sup>
              </span>
            </span>
          </b-card-header>
          <b-collapse :visible="categoryObjects[category].isopen"
            :id="category">
            <b-card-body>
              <!-- toggle buttons -->
              <div class="card-text btn-drawer mt-1 mr-1 ml-1"
                :ref="category + '-btn-drawer'">
                <div class="btn-container">
                  <form class="form-inline">
                    <input type="text"
                      class="form-control form-control-sm mr-1 mb-1"
                      placeholder="Search for fields in this category"
                      @input="updateFilteredFields(category, $event.target.value)"
                    />
                    <span class="small"
                      v-if="!categoryObjects[category].fields.length">
                      <span class="fa fa-fw fa-exclamation-circle">
                      </span>&nbsp;
                      No results match your query
                    </span>
                    <template v-if="categoryObjects[category].spi">
                      <span class="small"
                        v-if="categoryObjects[category].filteredFields && !categoryObjects[category].filteredFields.length">
                        <span class="fa fa-fw fa-exclamation-circle">
                        </span>&nbsp;
                        No fields match your query
                      </span>
                      <span v-for="field in categoryObjects[category].filteredFields"
                        :key="field.dbField">
                        <b-dropdown split
                          size="sm"
                          variant="default"
                          class="mr-1 mb-1 field-dropdown"
                          :text="field.friendlyName"
                          v-b-tooltip.hover
                          :title="field.help"
                          boundary="viewport"
                          @click="toggleSpiData(field, true, true)"
                          :class="{'active':categoryObjects[category].spi[field.dbField] && categoryObjects[category].spi[field.dbField].active}">
                          <b-dropdown-item
                            @click="exportUnique(field.dbField, 0)">
                            Export Unique {{ field.friendlyName }}
                          </b-dropdown-item>
                          <b-dropdown-item
                            @click="exportUnique(field.dbField, 1)">
                            Export Unique {{ field.friendlyName }} with counts
                          </b-dropdown-item>
                          <b-dropdown-item
                            @click="openSpiGraph(field.dbField)">
                            Open {{ field.friendlyName }} SPI Graph
                          </b-dropdown-item>
                        </b-dropdown>
                      </span>
                    </template>
                  </form>
                </div>
                <div class="text-center btn-drawer-toggle cursor-pointer"
                  @click="toggleBtnDrawer(category + '-btn-drawer')">
                  <span class="when-opened mt-2 fa fa-angle-double-up">
                  </span>
                  <span class="when-closed mt-2 fa fa-angle-double-down">
                  </span>
                </div>
              </div> <!-- toggle buttons -->
              <div v-if="categoryObjects[category].spi"
                class="mt-3">
                <!-- spiview field -->
                <div v-for="(value, key) in categoryObjects[category].spi"
                  :key="key"
                  class="spi-buckets pr-1 pl-1"
                  v-if="value.active">
                  <!-- spiview field label button -->
                  <b-dropdown
                    size="sm"
                    variant="default"
                    class="field-dropdown"
                    :text="value.field.friendlyName">
                    <b-dropdown-item
                      @click="toggleSpiData(value.field, true, true)">
                      Hide {{ value.field.friendlyName }}
                    </b-dropdown-item>
                    <b-dropdown-item
                      @click="exportUnique(value.field.dbField, 0)">
                      Export Unique {{ value.field.friendlyName }}
                    </b-dropdown-item>
                    <b-dropdown-item
                      @click="exportUnique(value.field.dbField, 1)">
                      Export Unique {{ value.field.friendlyName }} with counts
                    </b-dropdown-item>
                    <b-dropdown-item
                      @click="openSpiGraph(value.field.dbField)">
                      Open {{ value.field.friendlyName }} SPI Graph
                    </b-dropdown-item>
                  </b-dropdown> <!-- spiview field label button -->
                  <!-- spiview field data -->
                  <span v-if="value && value.value && value.value.buckets">
                    <span v-for="bucket in value.value.buckets"
                      :key="bucket.key">
                      <span v-if="bucket.key || bucket.key === 0"
                        class="small spi-bucket mr-1 no-wrap">
                        <moloch-session-field
                          :field="value.field"
                          :value="bucket.key"
                          :expr="value.field.exp"
                          :parse="true"
                          :pull-left="true"
                          :session-btn="true"
                          :timezone="settings.timezone">
                        </moloch-session-field>
                        <sup>({{ bucket.doc_count | round(0) }})</sup>
                      </span>
                    </span>
                  </span>
                  <!-- /spiview field data -->
                  <!-- spiview no data -->
                  <em class="small"
                    v-if="!value.loading && !value.error && (!value.value || !value.value.buckets.length)">
                    No data for this field
                    <span v-if="canceled && !value.value">
                      (request was canceled)
                    </span>
                  </em> <!-- /spiview no data -->
                  <!-- spiview field more/less values -->
                  <a v-if="value.count && value.count > 100"
                     @click="showValues(value, false)"
                     class="btn btn-link btn-xs"
                     style="text-decoration:none;">
                    ...less
                  </a>
                  <a v-if="value && value.value && value.value.doc_count_error_upper_bound < value.value.sum_other_doc_count"
                    @click="showValues(value, true)"
                    class="btn btn-link btn-xs"
                    style="text-decoration:none;">
                    more...
                  </a> <!-- /spiview field more/less values -->
                  <!-- spiview field loading -->
                  <span v-if="value.loading"
                    class="fa fa-spinner fa-spin">
                  </span> <!-- /spiview field loading -->
                  <!-- spiview field error -->
                  <span v-if="value.error"
                    class="text-danger ml-2">
                    <span class="fa fa-exclamation-triangle">
                    </span>&nbsp;
                    {{ value.error }}
                  </span> <!-- /spiview field error -->
                </div> <!-- /spiview field -->
              </div>
            </b-card-body>
          </b-collapse>
        </b-card>
      </div> <!-- /spiview panels -->

    </div>

  </div>

</template>

<script>
import Vue from 'vue';

import SessionsService from '../sessions/SessionsService';
import FieldService from '../search/FieldService';
import UserService from '../users/UserService';

import MolochError from '../utils/Error';
import MolochSearch from '../search/Search';
import MolochVisualizations from '../visualizations/Visualizations';

const defaultSpi = 'dstIp:100,protocol:100,srcIp:100';

let newQuery = true;
let openedCategories = false;

// object to store loading categories and how many fields are loading within
let categoryLoadingCounts = {};

// save currently executing promise
let pendingPromise;

let timeout;
let inputTimeout;

export default {
  name: 'Spiview',
  components: {
    MolochError,
    MolochSearch,
    MolochVisualizations
  },
  data: function () {
    return {
      error: '',
      canceled: false,
      loading: true,
      dataLoading: true,
      loadingVisualizations: true,
      staleData: undefined,
      filtered: 0,
      settings: {
        timezone: 'local'
      },
      fieldConfigs: [],
      graphData: undefined,
      mapData: undefined,
      categoryList: [],
      categoryObjects: {},
      spiQuery: this.$route.query.spi,
      // field config vars
      newFieldConfigName: '',
      fieldConfigError: ''
    };
  },
  computed: {
    query: function () {
      return {
        facets: 1,
        date: this.$store.state.timeRange,
        startTime: this.$store.state.time.startTime,
        stopTime: this.$store.state.time.stopTime,
        bounding: this.$route.query.bounding || 'last',
        interval: this.$route.query.interval || 'auto',
        view: this.$route.query.view || undefined,
        expression: this.$store.state.expression || undefined
      };
    }
  },
  mounted: function () {
    if (!this.spiQuery) {
      // get what's saved in the db
      SessionsService.getState('spiview')
        .then((response) => {
          this.spiQuery = response.data.visibleFields || defaultSpi;
          this.issueQueries();
        })
        .catch((error) => {
          this.spiQuery = defaultSpi;
          this.issueQueries();
        });
    } else {
      this.issueQueries();
    }
  },
  methods: {
    /* exposed page functions ---------------------------------------------- */
    /**
     * Filters field butttons by the search filter
     * @param {string} categoryName The name (key) of the category to filter fields
     * @param {string} searchFilter The string to search for in fields
     */
    updateFilteredFields: function (categoryName, searchFilter) {
      if (inputTimeout) { clearTimeout(inputTimeout); }

      inputTimeout = setTimeout(() => {
        let category = this.categoryObjects[categoryName];
        let fields = category.fields;

        fields = fields.filter((field) => {
          return field.friendlyName.toLowerCase().includes(
            searchFilter.toLowerCase()
          );
        });

        fields = this.sortFields(fields);

        Vue.set(this.categoryObjects[categoryName], 'filteredFields', fields);
      }, 400);
    },
    /**
     * Toggles the view of field spi data by updating the active state
     * or fetching new spi data as necessary
     * Also updates the spi query parameter in the url
     * @param {object} field      The field to get spi data for
     * @param {bool} issueQuery   Whether to issue query for the data
     * @param {bool} saveFields   Whether to save the visible fields
     * @returns {string} spiQuery The query string for the toggled on fields
     *                            e.g. 'lp:200,fp:100'
     */
    toggleSpiData: function (field, issueQuery, saveFields) {
      Vue.set(field, 'active', !field.active);

      let spiData;
      if (this.categoryObjects[field.group].spi) {
        spiData = this.categoryObjects[field.group].spi[field.dbField];
      }

      let addToQuery = false;
      let spiQuery = '';

      if (spiData) { // spi data exists, so we need to toggle active state
        Vue.set(spiData, 'active', !spiData.active);
        addToQuery = spiData.active;
        // if spiData was not populated with a value and it's now active
        // we need to show get the spi data from the server
        if (!spiData.value && spiData.active) { this.getSingleSpiData(field); }
      } else { // spi data doesn't exist, so fetch it
        addToQuery = true;
        if (issueQuery) { this.getSingleSpiData(field); }
      }

      // update spi query parameter by adding or removing field id
      if (addToQuery) {
        if (this.spiQuery && this.spiQuery !== '') {
          this.spiQuery += ',';
        }
        this.spiQuery += spiQuery += `${field.dbField}:100`;
      } else {
        let spiParamsArray = this.spiQuery.split(',');
        for (let i = 0, len = spiParamsArray.length; i < len; ++i) {
          if (spiParamsArray[i].includes(field.dbField)) {
            spiParamsArray.splice(i, 1);
            break;
          }
        }
        this.spiQuery = spiParamsArray.join(',');
      }

      // save field state if method was invoked from field button click
      if (saveFields) { this.saveFieldState(); }

      return spiQuery;
    },
    /* Cancels the loading of all server requests */
    cancelLoading: function () {
      if (pendingPromise) {
        pendingPromise.source.cancel();
        pendingPromise = null;
      }

      this.canceled = true; // indicate cancellation for future requests
      this.dataLoading = false;
      this.staleData = newQuery;

      // set loading to false for all categories and fields
      for (let key in this.categoryObjects) {
        if (this.categoryObjects.hasOwnProperty(key)) {
          let cat = this.categoryObjects[key];
          cat.loading = false;
          if (cat.spi) {
            for (let field in cat.spi) {
              if (cat.spi.hasOwnProperty(field)) {
                cat.spi[field].loading = false;
              }
            }
          }
        }
      }
    },
    /**
     * Saves the open categories to spiview-collapsible localStorage
     * @param {string} name The name of the category
     */
    toggleCategory: function (name) {
      this.categoryObjects[name].isopen = !this.categoryObjects[name].isopen;

      if (localStorage) {
        if (localStorage['spiview-collapsible']) {
          let visiblePanels = localStorage['spiview-collapsible'];
          if (!this.categoryObjects[name].isopen) {
            let split = visiblePanels.split(',');
            for (let i = 0, len = split.length; i < len; ++i) {
              if (split[i].includes(name)) {
                split.splice(i, 1);
                break;
              }
            }
            visiblePanels = split.join(',');
          } else if (!visiblePanels.includes(name)) {
            if (visiblePanels !== '') { visiblePanels += ','; }
            visiblePanels += `${name}`;
          }

          localStorage['spiview-collapsible'] = visiblePanels;
        } else {
          localStorage['spiview-collapsible'] = name;
        }
      }
    },
    /**
     * Shows more values for a specific field
     * @param {object} field  The field to get more spi data for
     * @param {bool} more     Whether to display more or less values
     */
    showValues: function (value, more) {
      let count;
      let field = value.field;
      if (this.spiQuery.includes(field.dbField)) {
        // make sure field is in the spi query parameter
        let spiParamsArray = this.spiQuery.split(',');
        for (let i = 0, len = spiParamsArray.length; i < len; ++i) {
          if (spiParamsArray[i].includes(field.dbField)) {
            let spiParam = spiParamsArray[i].split(':');
            if (more) {
              count = spiParam[1] = parseInt(spiParam[1]) + 100;
            } else {
              count = spiParam[1] = parseInt(spiParam[1]) - 100;
              count = Math.max(count, 0);
            }
            spiParamsArray[i] = spiParam.join(':');
            break;
          }
        }
        this.spiQuery = spiParamsArray.join(',');
      }

      value.count = count;

      this.saveFieldState();

      this.getSingleSpiData(field, count);
    },
    /**
     * Show/hide all values for a category
     * @param {string} categoryName The name of the category to toggle values for
     * @param {bool} load           Whether to load (or unload) all values
     */
    toggleAllValues: function (categoryName, load) {
      let query = '';
      let category = this.categoryObjects[categoryName];

      for (let i = 0, len = category.fields.length; i < len; ++i) {
        let field = category.fields[i];
        if (category.spi && category.spi[field.dbField]) {
          let spiData = category.spi[field.dbField];
          if ((spiData.active && !load) ||
             (!spiData.active && load)) {
            // the spi data for this field is already visible and we don't want
            // it to be, or it's NOT visible and we want it to be
            this.toggleSpiData(field);
          }
        } else if (load) { // spi data doesn't exist in the category
          if (query) { query += ','; }
          query += this.toggleSpiData(field);
        }
      }

      if (load && query) { this.getSpiData(query); }

      this.saveFieldState();
    },
    toggleBtnDrawer: function (ref) {
      $(this.$refs[ref]).toggleClass('expanded');
    },
    /**
     * Opens a new browser tab containing all the unique values for a given field
     * @param {string} exp  The field id to display unique values for
     * @param {int} counts  Whether to display the unique values with counts (1 or 0)
     */
    exportUnique: function (exp, counts) {
      SessionsService.exportUniqueValues(exp, counts, this.$route.query);
    },
    /**
     * Opens the spi graph page in a new browser tab
     * @param {string} fieldID The field id (dbField) to display spi graph data for
     */
    openSpiGraph: function (fieldID) {
      SessionsService.openSpiGraph(fieldID, this.$route.query);
    },
    /* Saves a custom spiview fields configuration */
    saveFieldConfiguration: function () {
      if (!this.newFieldConfigName) {
        this.fieldConfigError = 'You must name your new spiview field configuration';
        return;
      }

      let data = {
        name: this.newFieldConfigName,
        fields: this.spiQuery
      };

      UserService.createSpiviewFieldConfig(data)
        .then((response) => {
          data.name = response.name; // update column config name

          this.fieldConfigs.push(data);

          this.newFieldConfigName = null;
          this.fieldConfigsOpen = false;
          this.fieldConfigError = false;
        })
        .catch((error) => {
          this.fieldConfigError = error.text;
        });
    },
    /**
     * Loads a previously saved custom spiview fields configuration and
     * reloads the fields
     * If no index is given, loads the default spiview fields
     * @param {int} index The index in the array of the spiview fields configs to load
     */
    loadFieldConfiguration: function (index) {
      if (!index && index !== 0) {
        this.spiQuery = defaultSpi;
      } else {
        this.spiQuery = this.fieldConfigs[index].fields;
      }

      this.saveFieldState();
      this.restart();
    },
    /**
     * Deletes a previously saved custom spiview fields configuration
     * @param {string} name The name of the spiview fields config to remove
     * @param {int} index   The index in the array of the spiview fields config to remove
     */
    deleteFieldConfiguration: function (name, index) {
      UserService.deleteSpiviewFieldConfig(name)
        .then(() => {
          this.fieldConfigs.splice(index, 1);
          this.fieldConfigError = false;
        })
        .catch((error) => {
          this.fieldConfigError = error.text;
        });
    },
    /* event functions ----------------------------------------------------- */
    changeSearch: function () {
      newQuery = true;

      if (pendingPromise) { // if there's already a req (or series of reqs)
        this.cancelLoading(); // cancel any current requests
        timeout = setTimeout(() => { // wait for promise abort to complete
          this.getSpiData(this.spiQuery);
        }, 100);
      } else {
        this.getSpiData(this.spiQuery);
      }
    },
    /* helper functions ---------------------------------------------------- */
    get: function (query) {
      let source = Vue.axios.CancelToken.source();

      let promise = new Promise((resolve, reject) => {
        let options = {
          method: 'GET',
          params: query,
          cancelToken: source.token,
          url: 'spiview.json'
        };

        Vue.axios(options)
          .then((response) => {
            resolve(response.data);
          })
          .catch((error) => {
            if (!Vue.axios.isCancel(error)) {
              reject(error);
            }
          });
      });

      return { promise, source };
    },
    issueQueries: function () {
      this.getFields(); // IMPORTANT: kicks off initial query for spi data!
      this.getUserSettings();
      this.getSpiviewFieldConfigs();
    },
    getFields: function () {
      FieldService.get(true)
        .then((response) => {
          this.loading = false;
          this.error = false;
          this.fields = response;
          this.categoryObjects = {};

          for (let i = 0, len = this.fields.length; i < len; ++i) {
            let field = this.fields[i];

            field.active = false;

            if (field.noFacet || field.regex) { continue; }

            if (this.categoryObjects.hasOwnProperty(field.group)) {
              // already created, just add a new field
              this.categoryObjects[field.group].fields.push(field);
            } else { // create it
              Vue.set(this.categoryObjects, field.group, {
                fields: [ field ],
                spi: {}
              });
            }
          }

          // sorted list of categories for the view
          this.categoryList = Object.keys(this.categoryObjects).sort();
          this.categoryList.splice(this.categoryList.indexOf('general'), 1);
          this.categoryList.unshift('general');

          this.getSpiData(this.spiQuery); // IMPORTANT: queries for spi data!
        })
        .catch((error) => {
          this.loading = false;
          this.error = error.text;
        });
    },
    getSpiData: function (spiQuery) {
      if (!spiQuery) { return; }

      // reset loading counts for categories
      categoryLoadingCounts = {};

      this.dataLoading = true;
      this.staleData = false;
      this.canceled = false;
      this.error = false;

      let spiParamsArray = spiQuery.split(',');

      let tasks = [];
      let category;

      // get each field from the spi query parameter and issue
      // a query for one field at a time
      for (let i = 0, len = spiParamsArray.length; i < len; ++i) {
        let param = spiParamsArray[i];
        let split = param.split(':');
        let fieldID = split[0];
        let count = split[1];

        let field;

        for (let key in this.fields) {
          if (this.fields[key].dbField === fieldID) {
            field = this.fields[key];
            break;
          }
        }

        if (field) {
          category = this.setupCategory(this.categoryObjects, field);

          category.isopen = true; // open the category to display the field
          category.loading = true; // loading is set to false in getSingleSpiData

          // count the number of fields fetched for each category
          this.countCategoryFieldsLoading(category, true);

          let spiData = category.spi[field.dbField];

          Vue.set(field, 'active', true);
          Vue.set(spiData, 'active', true);
          Vue.set(spiData, 'error', false);
          Vue.set(spiData, 'loading', true);

          let promise = () => {
            return new Promise((resolve, reject) => {
              resolve(this.getSingleSpiData(field, count).promise);
            });
          };

          tasks.push(promise);
        }
      }

      if (!openedCategories) { this.openCategories(); }

      if (tasks.length) {
        // start processing tasks serially
        this.serial(tasks)
          .then((response) => { // returns the last result in the series
            if (response && response.bsqErr) {
              this.error = response.bsqErr;
            }
            this.dataLoading = false;
            pendingPromise = null;
          })
          .catch((error) => {
            this.error = error;
            this.dataLoading = false;
            pendingPromise = null;
          });
      } else if (this.fields) {
        // if we couldn't figure out the fields to request,
        // request the default ones
        this.getSpiData(defaultSpi);
      }
    },
    /**
     * Gets spi data for the specified field and adds it to the category object
     * @param {object} field  The field to get spi data for
     * @param {int} count     The amount of spi data to query for
     */
    getSingleSpiData: function (field, count) {
      let category = this.setupCategory(this.categoryObjects, field);
      let spiData = category.spi[field.dbField];

      // don't continue if the active flag is defined and false
      if (spiData.active !== undefined && !spiData.active) { return; }

      if (!count) { count = 100; } // default amount of spi data to retrieve

      Vue.set(spiData, 'active', true);
      Vue.set(spiData, 'loading', true);
      Vue.set(spiData, 'error', false);

      let query = {
        facets: 1,
        spi: `${field.dbField}:${count}`,
        date: this.query.date,
        startTime: this.query.startTime,
        stopTime: this.query.stopTime,
        expression: this.query.expression,
        bounding: this.query.bounding,
        interval: this.query.interval,
        view: this.query.view
      };

      pendingPromise = this.get(query);

      pendingPromise.promise
        .then((response) => {
          this.countCategoryFieldsLoading(category, false);

          if (response.bsqErr) { spiData.error = response.bsqErr; }

          // only update the requested spi data
          Vue.set(spiData, 'loading', false);
          Vue.set(spiData, 'value', response.spi[field.dbField]);
          Vue.set(spiData, 'count', count);

          if (newQuery) { // this data comes back with every request
            // we should show it in the view ASAP (on first request)
            newQuery = false;
            this.mapData = response.map;
            this.graphData = response.graph;
            this.protocols = response.protocols;
            this.total = response.recordsTotal;
            this.filtered = response.recordsFiltered;
            this.loadingVisualizations = false;

            this.updateProtocols();
          }
        })
        .catch((error) => {
          this.countCategoryFieldsLoading(category, false);

          // display error for the requested spi data
          spiData.loading = false;
          spiData.error = error.text;
          this.loadingVisualizations = false;
        });

      return pendingPromise;
    },
    /* Retrieves the current user's settings (specifically for timezone) */
    getUserSettings: function () {
      UserService.getSettings()
        .then((settings) => {
          this.settings = settings;
        })
        .catch((error) => {
          this.error = error;
        });
    },
    /* Gets the current user's custom spiview fields configurations */
    getSpiviewFieldConfigs: function () {
      UserService.getSpiviewFields()
        .then((response) => {
          this.fieldConfigs = response;
        })
        .catch((error) => {
          this.fieldConfigError = error.text;
        });
    },
    /**
     * Chains sequential promises together
     * @param {object} tasks          List or map of tasks to complete
     * @returns {promise} prevPromise The previously executed promise
     */
    serial: function (tasks) {
      let prevPromise;

      for (let task of tasks) {
        if (!prevPromise) { // first task
          prevPromise = task();
        } else { // subsequent tasks
          prevPromise = prevPromise.then(task);
        }
      }

      return prevPromise;
    },
    /**
     * Adds and sorts filtered fields for a category
     * Sorts them by active state then friendlyName
     * @param {array} fields The array of fields to sort
     */
    sortFields: function (fields) {
      return fields.sort((a, b) => {
        let bool = (a.active === b.active) ? 0 : a.active ? -1 : 1;
        let str = a.friendlyName.localeCompare(b.friendlyName);

        return bool || str;
      });
    },
    /* opens categories that were opened in a previous session
       should only run once on page load */
    openCategories: function () {
      openedCategories = true;
      for (let key in this.categoryObjects) {
        if (this.categoryObjects.hasOwnProperty(key)) {
          let category = this.categoryObjects[key];

          let fields = category.fields;
          fields = this.sortFields(category.fields);
          Vue.set(category, 'filteredFields', fields);

          if (localStorage && localStorage['spiview-collapsible']) {
            if (localStorage['spiview-collapsible'].includes(key)) {
              category.isopen = true;
            } else {
              continue;
            }
          }
        }
      }
    },
    /* updates protocols and protocol counts for categories
       should only run when issuing a new query */
    updateProtocols: function () {
      // clean up any old protocols
      for (let c in this.categoryObjects) {
        if (this.categoryObjects.hasOwnProperty(c)) {
          this.categoryObjects[c].protocols = {};
        }
      }

      for (let key in this.protocols) {
        if (this.protocols.hasOwnProperty(key)) {
          let category;

          // find the category that the protocol belongs to
          if (this.categoryObjects.hasOwnProperty(key)) {
            category = this.categoryObjects[key];
          } else { // categorize special protocols that don't match category
            if (key === 'tcp' || key === 'udp' || key === 'icmp' || key === 'sctp' || key === 'esp') {
              category = this.categoryObjects.general;
            } else if (key === 'smtp' || key === 'lmtp') {
              category = this.categoryObjects.email;
            }
          }

          if (category) {
            category.protocols[key] = this.protocols[key];
          }
        }
      }
    },
    /* restarts the page with a new query by deactivating unnecessary spi data,
       canceling any pending promises/timeouts, and issuing a new query */
    restart: function () {
      newQuery = true;
      openedCategories = false;
      categoryLoadingCounts = {};

      this.deactivateSpiData(); // hide any removed fields from spi url param

      if (pendingPromise) { // if there's already a req (or series of reqs)
        this.cancelLoading(); // cancel any current requests
        timeout = setTimeout(() => { // wait for promise abort to complete
          this.getSpiData(this.spiQuery);
        }, 100);
      } else {
        this.getSpiData(this.spiQuery);
      }
    },
    /* deactivate spi data that is no longer in url params */
    deactivateSpiData: function () {
      let spiParamsArray = this.spiQuery.split(',');
      for (let key in this.categoryObjects) {
        if (this.categoryObjects.hasOwnProperty(key)) {
          let category = this.categoryObjects[key];
          for (let k in category.spi) {
            if (category.spi.hasOwnProperty(k)) {
              let spiData = category.spi[k];
              if (spiData.active) {
                let inactive = true;
                for (let i = 0, len = spiParamsArray.length; i < len; ++i) {
                  // if it exists in spi url param, it's still active
                  if (spiParamsArray[i] === k) { inactive = false; }
                }
                // it's no longer in the spi url param, so it's not active
                if (inactive) { spiData.active = false; }
              }
            }
          }
        }
      }
    },
    /**
     * Finds the category that contains the given field and (if necessary) sets
     * it up to display spi data
     * @param {object} catMap     Map of spiview field categories
     * @param {object} field      The field to setup the category for
     * @returns {object} category The updated category that the field belongs to
     */
    setupCategory: function (catMap, field) {
      let category = catMap[field.group];

      category.name = field.group;

      if (!category.spi) { category.spi = {}; }

      if (!category.spi[field.dbField]) {
        Vue.set(category.spi, field.dbField, { field: field });
      }

      return category;
    },
    /**
     * Counts category fields that are being loaded so the user can know
     * when there are fields in the category that are loading
     * @param {object} category The category to count
     * @param {bool} increment  Whether to increment or decrement the count
     */
    countCategoryFieldsLoading: function (category, increment) {
      if (increment) {
        if (categoryLoadingCounts[category.name]) {
          ++categoryLoadingCounts[category.name];
        } else {
          categoryLoadingCounts[category.name] = 1;
        }
      } else {
        if (categoryLoadingCounts[category.name] &&
            categoryLoadingCounts[category.name] > 1) {
          --categoryLoadingCounts[category.name];
        } else {
          category.loading = false;
        }
      }
    },
    /* saves the visible fields */
    saveFieldState: function () {
      SessionsService.saveState({ visibleFields: this.spiQuery }, 'spiview');
    }
  },
  beforeDestroy: function () {
    // reset state variables
    newQuery = true;
    openedCategories = false;
    categoryLoadingCounts = {};

    if (timeout) { clearTimeout(timeout); }

    if (pendingPromise) { // if there's  a req (or series of reqs)
      pendingPromise.source.cancel();
      pendingPromise = null;
    }
  }
};
</script>

<style>
/* make field buttons tiny */
.spiview-page .btn-group.dropdown.field-dropdown > button {
  padding: 0 5px;
  font-size: .75rem;
}
/* show active button */
.spiview-page .btn-group.dropdown.active > button:not(.dropdown-toggle) {
  color: var(--color-foreground, #333);
  background-color: var(--color-gray-light);
  box-shadow: inset 0 3px 5px rgba(0, 0, 0, .25);
}
/* bold field label dropdown buttons */
.spiview-page .spi-buckets > div.btn-group.dropdown > button {
  font-weight: 600;
}
/* wide field config dropdown */
.spiview-page form.info-nav .field-config-menu .dropdown-menu {
  min-width: 300px;
  max-width: 400px;
}
</style>

<style scoped>
/* spiview page, navbar, and content styles - */
.spiview-page {
  margin-top: 36px;
  overflow-x: hidden;
}

/* info navbar --------------------- */
.spiview-page form.info-nav {
  z-index: 4;
  position: fixed;
  top: 110px;
  left: 0;
  right: 0;
  height: 42px;
  padding: var(--px-md);
  background-color: var(--color-quaternary-lightest);

  -webkit-box-shadow: 0 0 16px -2px black;
     -moz-box-shadow: 0 0 16px -2px black;
          box-shadow: 0 0 16px -2px black;
}

.spiview-page form.info-nav button,
.spiview-page form.info-nav .field-config-menu {
  margin-top: -2px;
}
.spiview-page form.info-nav .field-config-menu .dropdown-header {
  padding: 0 2px;
}

/* spiview content ----------------- */
.spiview-page > .spiview-content {
  padding-top: 120px;
}

/* panels -------------------------- */
.spiview-page .card-body {
  padding: 0;
}

/* +/- button on panel header */
.collapsed > .when-opened,
:not(.collapsed) > .when-closed {
  display: none;
}

.spiview-page .protocol-value {
  margin-right: .75rem;
  font-size: .9rem;
}

/* value counts */
.spiview-page sup {
  margin-left: -8px;
}

/* larger titles */
.spiview-page strong.category-title {
  font-size: 1.3rem;
}

/* btn drawer for toggling values -- */
.spiview-page .btn-drawer-toggle {
  background-color: var(--color-gray-lighter);
  margin-right: -15px;
  margin-left: -15px;
}
.spiview-page .btn-drawer .form-control-sm {
  height: 22px;
  padding: 2px 6px;
  font-size: 12px;
  line-height: 1.5;
  border-radius: 3px;
  vertical-align: top;
  width: 300px;
}
.spiview-page .btn-drawer .btn-container {
  max-height: 22px;
  overflow: hidden;
}

.spiview-page .btn-drawer.expanded .btn-container {
  max-height: 999px;
  overflow: visible;
}
/* expand drawer button icon */
.spiview-page .btn-drawer.expanded .fa-angle-double-up {
  display: block;
}
.spiview-page .btn-drawer:not(.expanded) .fa-angle-double-up {
  display: none;
}
.spiview-page .btn-drawer.expanded .fa-angle-double-down {
  display: none;
}
.spiview-page .btn-drawer:not(.expanded) .fa-angle-double-down {
  display: block;
}

/* stripes! */
.spiview-page .spi-buckets:nth-child(odd) {
  background-color: var(--color-quaternary-lightest);
}

/* force wrapping */
.spiview-page .spi-bucket {
  display: inline-block;
}
</style>
