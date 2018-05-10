<template>

  <div class="spiview-page">

    <!-- search navbar -->
    <moloch-search
      :num-matching-sessions="filtered"
      :timezone="settings.timezone"
      @changeSearch="changeSearch">
    </moloch-search> <!-- /search navbar -->

    <!-- info navbar -->
    <form class="info-nav">
      <div v-if="dataLoading">
        <!-- TODO config save btn -->
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

      spiview goes HERE

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
      staleData: undefined, // TODO,
      filtered: 0,
      settings: {
        timezone: 'local'
      },
      query: {
        facets: 1,
        spi: this.$route.query.spi
      },
      fieldConfigs: [],
      graphData: undefined,
      mapData: undefined
    };
  },
  created: function () {
    if (!this.query.spi) {
      // get what's saved in the db
      SessionsService.getState('spiview')
        .then((response) => {
          this.query.spi = response.data.visibleFields || defaultSpi;
          this.issueQueries();
        })
        .catch((error) => {
          this.query.spi = defaultSpi;
          this.issueQueries();
        });
    } else {
      this.issueQueries();
    }
  },
  methods: {
    /* exposed page functions ---------------------------------------------- */
    /* Cancels the loading of all server requests */
    cancelLoading: function () {
      // TODO test if it works
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
    /* event functions ----------------------------------------------------- */
    changeSearch: function () {
      newQuery = true;

      if (pendingPromise) { // if there's already a req (or series of reqs)
        this.cancelLoading(); // cancel any current requests
        timeout = setTimeout(() => { // wait for promise abort to complete
          this.getSpiData(this.query.spi);
        }, 100);
      } else {
        this.getSpiData(this.query.spi);
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
            let newField;
            let field = this.fields[i];

            field.active = false;

            if (field.noFacet || field.regex) { continue; }

            if (this.categoryObjects.hasOwnProperty(field.group)) {
              // already created, just add a new field
              this.categoryObjects[field.group].fields.push(field);
            } else { // create it
              this.categoryObjects[field.group] = { fields: [ field ] };
            }

            if (newField) {
              newField.active = false;
              this.categoryObjects[field.group].fields.push(newField);
              this.fields.push(newField);
            }
          }

          // sorted list of categories for the view
          this.categoryList = Object.keys(this.categoryObjects).sort();
          this.categoryList.splice(this.categoryList.indexOf('general'), 1);
          this.categoryList.unshift('general');

          this.getSpiData(this.query.spi); // IMPORTANT: queries for spi data!
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

          field.active = true;
          spiData.active = true;
          spiData.loading = true;
          spiData.error = false;

          // TODO if cancelled
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

      spiData.active = true;
      spiData.loading = true;
      spiData.error = false;

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
          spiData.loading = false;
          spiData.value = response.spi[field.dbField];
          spiData.count = count;

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
      console.log('serial!');

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
    /* opens categories that were opened in a previous session
       should only run once on page load */
    openCategories: function () {
      openedCategories = true;
      for (let key in this.categoryObjects) {
        if (this.categoryObjects.hasOwnProperty(key)) {
          if (localStorage && localStorage['spiview-collapsible']) {
            if (localStorage['spiview-collapsible'].contains(key)) {
              this.categoryObjects[key].isopen = true;
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
            if (key === 'tcp' || key === 'udp' || key === 'icmp' || key === 'sctp') {
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
    /* deactivate spi data that is no longer in url params */
    deactivateSpiData: function () {
      let spiParamsArray = this.query.spi.split(',');
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
        category.spi[field.dbField] = { field: field };
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

<style scoped>
/* spiview page, navbar, and content styles - */
.spiview-page {
  margin-top: 36px;
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

.spiview-page form.info-nav .info-nav-loading,
.spiview-page form.info-nav .field-config-btn {
  margin-top: -3px;
}

.spiview-page form.info-nav .field-config-menu {
  min-width: 300px;
  max-width: 400px;
}

.spiview-page form.info-nav .field-config-menu .input-group {
  width: 105%;
}

/* spiview content ----------------- */
.spiview-page > .spiview-content {
  padding-top: 120px;
}
</style>
