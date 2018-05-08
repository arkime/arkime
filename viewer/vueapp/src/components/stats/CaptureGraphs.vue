<template>

  <div class="ml-1 mr-1">

    <moloch-loading v-if="loading && !error">
    </moloch-loading>

    <moloch-error v-if="error"
      :message="error">
    </moloch-error>

    <div v-show="!error">

      <div class="input-group input-group-sm node-search pull-right mt-1">
        <div class="input-group-prepend">
          <span class="input-group-text">
            <span class="fa fa-search"></span>
          </span>
        </div>
        <input type="text"
          class="form-control"
          v-model="query.filter"
          @keyup="searchForNodes()"
          placeholder="Begin typing to search for nodes by name">
      </div>

      <moloch-paging v-if="stats"
        class="mt-1"
        :records-total="stats.recordsTotal"
        :records-filtered="stats.recordsFiltered"
        v-on:changePaging="changePaging">
      </moloch-paging>

      <div id="statsGraph" style="width:1440px;"></div>

    </div>

  </div>

</template>

<script>
import d3 from '../../../../public/d3.min.js';
import cubism from '../../../../public/cubism.v1.js';
import '../../../../public/highlight.min.js';

import '../../cubismoverrides.css';
import ToggleBtn from '../utils/ToggleBtn';
import MolochPaging from '../utils/Pagination';
import MolochError from '../utils/Error';
import MolochLoading from '../utils/Loading';

let reqPromise; // promise returned from setInterval for recurring requests
let initialized; // whether the graph has been initialized
let searchInputTimeout; // timeout to debounce the search input

export default {
  name: 'NodeStats',
  props: ['user', 'graphType', 'graphInterval', 'graphHide', 'graphSort'],
  components: { ToggleBtn, MolochPaging, MolochError, MolochLoading },
  data: function () {
    return {
      error: '',
      loading: true,
      context: null,
      stats: null,
      query: {
        length: parseInt(this.$route.query.length) || 50,
        start: 0,
        filter: null,
        desc: this.graphSort === 'desc',
        hide: this.graphHide || 'none'
      }
    };
  },
  watch: {
    graphType: function () {
      initialized = false;
      this.loadData();
    },
    graphInterval: function () {
      initialized = false;
      this.loadData();
    },
    graphHide: function () {
      initialized = false;
      this.query.hide = this.graphHide;
      this.loadData();
    },
    graphSort: function () {
      initialized = false;
      this.query.desc = this.graphSort === 'desc';
      this.loadData();
    }
  },
  created: function () {
    this.loadData();

    // watch for the user to leave or return to the page
    // Don't load graph data if the user is not focused on the page!
    // if data is loaded in an inactive (background) tab,
    // the user will experience gaps in their cubism graph data
    // cubism uses setTimeout to delay requests
    // inactive tabs' timeouts are clamped and can fire late;
    // cubism requires little error in the timing of requests
    // for more info, view the "reasons for delays longer than specified" section of:
    // https://developer.mozilla.org/en-US/docs/Web/API/WindowTimers/setTimeout#Inactive_tabs
    if (document.addEventListener) {
      document.addEventListener('visibilitychange', this.handleVisibilityChange);
    }
  },
  computed: {
    colors: function () {
      // build colors array from css variables
      let styles = window.getComputedStyle(document.body);
      let primaryLighter = styles.getPropertyValue('--color-primary-light').trim();
      let primaryLight = styles.getPropertyValue('--color-primary').trim();
      let primary = styles.getPropertyValue('--color-primary-dark').trim();
      let primaryDark = styles.getPropertyValue('--color-primary-darker').trim();
      let secondaryLighter = styles.getPropertyValue('--color-tertiary-light').trim();
      let secondaryLight = styles.getPropertyValue('--color-tertiary').trim();
      let secondary = styles.getPropertyValue('--color-tertiary-dark').trim();
      let secondaryDark = styles.getPropertyValue('--color-tertiary-darker').trim();
      return [primaryDark, primary, primaryLight, primaryLighter, secondaryLighter, secondaryLight, secondary, secondaryDark];
    }
  },
  methods: {
    /* exposed page functions ------------------------------------ */
    toggleSection: function () {
      if (!this.context) { return; }

      this.context.start();
    },
    changePaging (pagingValues) {
      this.query.length = pagingValues.length;
      this.query.start = pagingValues.start;

      initialized = false;
      this.loadData();
    },
    searchForNodes () {
      if (searchInputTimeout) { clearTimeout(searchInputTimeout); }
      // debounce the input so it only issues a request after keyups cease for 400ms
      searchInputTimeout = setTimeout(() => {
        searchInputTimeout = null;
        initialized = false;
        this.loadData();
      }, 400);
    },
    loadData: function () {
      this.$http.get('stats.json', { params: this.query })
        .then((response) => {
          this.error = '';
          this.loading = false;
          this.stats = response.data;

          if (!this.stats.data) { return; }

          if (this.stats.data && !initialized) {
            initialized = true; // only make the graph when page loads or tab switched to 0
            this.makeStatsGraph(this.graphType, parseInt(this.graphInterval, 10));
          }
        }, (error) => {
          this.loading = false;
          this.error = error;
        });
    },
    makeStatsGraph: function (metricName, interval) {
      var self = this;
      if (self.context) { self.context.stop(); } // Stop old context

      self.context = cubism.cubism.context()
        .step(interval * 1000)
        .size(1440);

      var context = self.context;
      var nodes = self.stats.data.map(function (item) {
        return item.nodeName;
      });

      function metric (name) {
        return context.metric(function (startV, stopV, stepV, callback) {
          let config = {
            method: 'GET',
            url: 'dstats.json',
            params: {
              nodeName: name,
              start: startV / 1000,
              stop: stopV / 1000,
              step: stepV / 1000,
              interval: interval,
              name: metricName
            }
          };
          self.$http(config)
            .then((response) => {
              return callback(null, response.data);
            }, (error) => {
              return callback(new Error('Unable to load data'));
            });
        }, name);
      }

      let wrap = document.getElementById('statsGraph');
      if (wrap) {
        while (wrap.firstChild) {
          wrap.removeChild(wrap.firstChild);
        }
      }

      d3.select('#statsGraph').call(function (div) {
        var metrics = [];
        for (var i = 0, ilen = nodes.length; i < ilen; i++) {
          metrics.push(metric(nodes[i]));
        }

        if (div[0][0]) {
          let axis = context.axis();

          let timeStr = self.graphInterval >= 600 ? '%m/%d %H:%M:%S' : '%H:%M:%S';

          let timeFormat = self.user.settings.timezone === 'gmt' ? d3.time.format.utc(timeStr + 'Z') : d3.time.format(timeStr);

          div.append('div')
            .attr('class', 'axis')
            .attr('height', 28)
            .call(
              axis.orient('top')
                .tickFormat(timeFormat)
                .focusFormat(timeFormat)
            );

          div.selectAll('.horizon')
            .data(metrics)
            .enter().append('div')
            .attr('class', 'horizon')
            .call(context.horizon().colors(self.colors));

          div.append('div')
            .attr('class', 'rule')
            .call(context.rule());
        }
      });
    },
    // stop the requests if the user is not looking at the stats page,
    // otherwise start the requests
    handleVisibilityChange () {
      if (!this.context) { return; }
      if (document.hidden) {
        this.context.stop();
      } else {
        this.context.start();
      }
    }
  },
  beforeDestroy: function () {
    initialized = false;

    if (this.context) {
      this.context.stop(); // stop cubism context from continuing to issue reqs
    }

    let wrap = document.getElementById('statsGraph');
    if (wrap) {
      while (wrap.firstChild) {
        wrap.removeChild(wrap.firstChild);
      }
    }

    if (reqPromise) {
      clearInterval(reqPromise);
      reqPromise = null;
    }

    if (document.removeEventListener) {
      document.removeEventListener('visibilitychange', this.handleVisibilityChange);
    }
  }
};
</script>

<style scoped>
.collapsed > .when-opened,
:not(.collapsed) > .when-closed {
  display: none;
}

.node-search {
  max-width: 50%;
}

td {
  white-space: nowrap;
}
tr.bold {
  font-weight: bold;
}
table.table tr.border-bottom-bold > td {
  border-bottom: 2px solid #dee2e6;
}
table.table tr.border-top-bold > td {
  border-top: 2px solid #dee2e6;
}

#graphContent, #nodeStatsContent {
  overflow-x: auto;
}
</style>
