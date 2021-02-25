<template>

  <div class="container-fluid">

    <moloch-loading v-if="initialLoading && !error">
    </moloch-loading>

    <moloch-error v-if="error"
      :message="error">
    </moloch-error>

    <div v-show="!error">

      <span v-b-tooltip.hover.left
        class="fa fa-lg fa-question-circle-o cursor-help mt-2 pull-right"
        title="HINT: These graphs are 1440 pixels wide. Expand your browser window to at least 1500 pixels wide for best viewing.">
      </span>

      <moloch-paging v-if="stats"
        class="mt-1"
        :records-total="stats.recordsTotal"
        :records-filtered="stats.recordsFiltered"
        v-on:changePaging="changePaging"
        length-default=200>
      </moloch-paging>

      <div id="statsGraph" style="width:1440px;"></div>

    </div>

  </div>

</template>

<script>
import d3 from '../../../../public/d3.min.js';
import cubism from '../../../../public/cubism.v1.min.js';
import '../../../../public/highlight.min.js';

import '../../cubismoverrides.css';
import MolochPaging from '../utils/Pagination';
import MolochError from '../utils/Error';
import MolochLoading from '../utils/Loading';

let reqPromise; // promise returned from setInterval for recurring requests
let initialized; // whether the graph has been initialized

export default {
  name: 'NodeStats',
  props: [
    'user',
    'graphType',
    'graphInterval',
    'graphHide',
    'graphSort',
    'searchTerm',
    'refreshData'
  ],
  components: { MolochPaging, MolochError, MolochLoading },
  data: function () {
    return {
      error: '',
      initialLoading: true,
      context: null,
      stats: null,
      query: {
        length: parseInt(this.$route.query.length) || 200,
        start: 0,
        filter: this.searchTerm || undefined,
        desc: this.graphSort === 'desc',
        hide: this.graphHide || 'none'
      }
    };
  },
  computed: {
    colors: function () {
      // build colors array from css variables
      const styles = window.getComputedStyle(document.body);
      const primaryLighter = styles.getPropertyValue('--color-primary-light').trim();
      const primaryLight = styles.getPropertyValue('--color-primary').trim();
      const primary = styles.getPropertyValue('--color-primary-dark').trim();
      const primaryDark = styles.getPropertyValue('--color-primary-darker').trim();
      const secondaryLighter = styles.getPropertyValue('--color-tertiary-light').trim();
      const secondaryLight = styles.getPropertyValue('--color-tertiary').trim();
      const secondary = styles.getPropertyValue('--color-tertiary-dark').trim();
      const secondaryDark = styles.getPropertyValue('--color-tertiary-darker').trim();
      return [primaryDark, primary, primaryLight, primaryLighter, secondaryLighter, secondaryLight, secondary, secondaryDark];
    },
    loading: {
      get: function () {
        return this.$store.state.loadingData;
      },
      set: function (newValue) {
        this.$store.commit('setLoadingData', newValue);
      }
    }
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
    },
    refreshData: function () {
      if (this.refreshData) {
        this.loadData();
      }
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
    /* helper functions ---------------------------------------------------- */
    loadData: function () {
      this.loading = true;
      initialized = false;

      this.query.filter = this.searchTerm;

      this.$http.get('api/stats', { params: this.query })
        .then((response) => {
          this.error = '';
          this.loading = false;
          this.initialLoading = false;
          this.stats = response.data;

          if (!this.stats.data) { return; }

          if (this.stats.data && !initialized) {
            initialized = true; // only make the graph when page loads or tab switched to 0
            this.makeStatsGraph(this.graphType, parseInt(this.graphInterval, 10));
          }
        }, (error) => {
          this.loading = false;
          this.initialLoading = false;
          this.error = error.text || error;
        });
    },
    makeStatsGraph: function (metricName, interval) {
      const self = this;
      if (self.context) { self.context.stop(); } // Stop old context

      self.context = cubism.cubism.context()
        .step(interval * 1000)
        .size(1440);

      const context = self.context;
      const nodes = self.stats.data.map((item) => {
        return item.nodeName;
      });

      function metric (name) {
        return context.metric((startV, stopV, stepV, callback) => {
          const config = {
            method: 'GET',
            url: 'api/dstats',
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

      const wrap = document.getElementById('statsGraph');
      if (wrap) {
        while (wrap.firstChild) {
          wrap.removeChild(wrap.firstChild);
        }
      }

      d3.select('#statsGraph').call((div) => {
        const metrics = [];
        for (let i = 0, ilen = nodes.length; i < ilen; i++) {
          metrics.push(metric(nodes[i]));
        }

        if (div[0][0]) {
          const axis = context.axis();
          const axisBottom = context.axis();

          const timeStr = self.graphInterval >= 600 ? '%m/%d %H:%M:%S' : '%H:%M:%S';

          const timeFormat = self.user.settings.timezone === 'gmt' ? d3.time.format.utc(timeStr + 'Z') : d3.time.format(timeStr);

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

          div.append('div')
            .attr('class', 'axis')
            .attr('height', 28)
            .call(
              axisBottom.orient('bottom')
                .tickFormat(timeFormat)
                .focusFormat(timeFormat)
            );
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

    const wrap = document.getElementById('statsGraph');
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
