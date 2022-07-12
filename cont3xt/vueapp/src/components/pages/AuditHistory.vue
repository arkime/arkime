<template>
  <div class="container-fluid">
    <!--  search bar form  -->
    <b-input-group class="flex-grow-1 mr-2">
      <template #prepend>
        <b-input-group-text>
              <span class="fa fa-search fa-fw"/>
        </b-input-group-text>
      </template>
      <b-form-input
          tabindex="0"
          ref="search"
          v-model="filter"
          debounce="400"
          placeholder="Search history by indicator, iType, or tags"
      />
      <template #append>
        <b-button
            tabindex="0"
            @click="clearSearchTerm"
            :disabled="!filter"
            title="Remove the search text">
          <span class="fa fa-close" />
        </b-button>
      </template>
    </b-input-group>
    <!--  search bar form  -->

    <!-- time range inputs -->
    <time-range-input class="mt-1"
        v-model="timeRangeInfo" :place-holder-tip="timePlaceHolderTip"/>
    <!-- /time range inputs -->

    <!--  history table  -->
    <b-table
        small
        hover
        striped
        show-empty
        :fields="fields"
        :items="auditLogs"
        :sort-by.sync="sortBy"
        :sort-desc.sync="sortDesc"
        empty-text="There is no history to show"
    >
      <!--   customize column sizes   -->
      <template #table-colgroup="scope">
        <col
            v-for="field in scope.fields"
            :key="field.key"
            :style="{ width: field.setWidth }"
        >
      </template>
      <!--   /customize column sizes   -->

      <!--   Button Column   -->
      <template #cell(buttons)="data">
        <b-button v-if="getUser && getUser.removeEnabled"
            @click="deleteLog(data.item._id)"
            class="btn btn-xs btn-warning"
            :id="`${data.item._id}-trash`">
          <span class="fa fa-trash"/>
        <b-tooltip :target="`${data.item._id}-trash`" noninteractive>Delete history item</b-tooltip>
        </b-button>
        <b-button
            target="_blank"
            :href="reissueSearchLink(data.item)"
            class="btn btn-xs btn-success"
            :id="`${data.item._id}-reissue`">
          <span class="fa fa-external-link"/>
        <b-tooltip :target="`${data.item._id}-reissue`" noninteractive>Repeat search</b-tooltip>
        </b-button>
      </template>
      <!--   /Button Column   -->

      <!--   Indicator Column (enforces max length)-->
      <template #cell(indicator)="data">
        <div class="indicator-limit-width">
          {{ data.item.indicator }}
        </div>
      </template>
      <!--   /Indicator Column (enforces max length)-->

      <!--   Tag Column   -->
      <template #cell(tags)="data">
        <template v-if="data.item.tags.length">
          <indicator-tag v-for="tag in data.item.tags" :key="tag" :value="tag"/>
        </template>
        <template v-else>
          -
        </template>
      </template>
      <!--   /Tag Column   -->

      <!--   Options Column   -->
      <template #cell(queryOptions)="data">
        <template v-if="Object.keys(data.item.queryOptions).length">
          <template v-for="([key, value], index) in Object.entries(data.item.queryOptions)">
            <span :key="index">{{ index === 0 ? '?' : '&' }}{{ key }}=<template>
              <span class="text-success" v-b-tooltip.hover="value" v-if="key === 'view' && viewLookup[value] != null">{{ viewLookup[value] }}</span>
              <span v-else>{{ value }}</span>
            </template></span>
          </template>
        </template>
        <template v-else>
          -
        </template>
      </template>
      <!--   /Options Column   -->
    </b-table>
    <!--  /history table  -->
  </div>
</template>

<script>
import AuditService from '@/components/services/AuditService';
import { reDateString } from '@/utils/filters';
import IndicatorTag from '@/utils/IndicatorTag';
import TimeRangeInput from '@/utils/TimeRangeInput';
import { mapGetters } from 'vuex';

export default {
  name: 'AuditHistory',
  components: { IndicatorTag, TimeRangeInput },
  computed: {
    ...mapGetters(['getViews', 'getUser']),
    viewLookup () {
      return Object.fromEntries(this.getViews.map(view => [view._id, view.name]));
    }
  },
  data () {
    return {
      auditLogs: [],
      filteredLogs: [],
      timeRangeInfo: {
        numDays: 7, // 1 week
        numHours: 7 * 24, // 1 week
        startDate: new Date(new Date().getTime() - (3600000 * 24 * 7)).toISOString().slice(0, -5) + 'Z', // 1 week ago
        stopDate: new Date().toISOString().slice(0, -5) + 'Z', // now
        startMs: Date.now() - (3600000 * 24 * 7), // by default, looks back one week
        stopMs: Date.now() // now
      },
      lastTimeRangeInfoSearched: null,
      timePlaceHolderTip: {
        title: 'These values specify the date range searched.<br>' +
            'Try using <a href="help#general" class="no-decoration">relative times</a> like -5d or -1h.'
      },
      fields: [
        { // virtual button field
          label: '',
          key: 'buttons',
          setWidth: '5rem'
        },
        {
          label: 'Time',
          key: 'issuedAt',
          formatter: reDateString,
          sortable: true,
          setWidth: '10rem'
        },
        {
          label: 'User ID',
          key: 'userId',
          sortable: true,
          setWidth: '5rem'
        },
        {
          label: 'iType',
          key: 'iType',
          sortable: true,
          setWidth: '5rem'
        },
        {
          label: 'Indicator',
          key: 'indicator',
          sortable: true,
          setWidth: '30rem'
        },
        {
          label: 'Tags',
          key: 'tags',
          sortable: true
        },
        {
          label: 'Options',
          key: 'queryOptions',
          sortable: true,
          setWidth: '15rem'
        },
        {
          label: 'Results',
          key: 'resultCount',
          sortable: true,
          setWidth: '4rem',
          tdClass: 'text-right',
          formatter: this.orQuestionMark
        },
        {
          label: 'Took',
          key: 'took',
          sortable: true,
          setWidth: '4rem',
          tdClass: 'text-right',
          formatter: this.millisecondStr
        }
      ],
      sortBy: 'issuedAt',
      sortDesc: true,
      filter: ''
    };
  },
  watch: {
    timeRangeInfo (newVal) {
      // only re-search audits if there has already been a search, and the time-range has actually changed
      if (this.lastTimeRangeInfoSearched != null &&
          (newVal.startMs !== this.lastTimeRangeInfoSearched.startMs || newVal.stopMs !== this.lastTimeRangeInfoSearched.stopMs)) {
        this.loadAuditsFromSearch();
      }
    },
    filter () {
      this.loadAuditsFromSearch();
    }
  },
  methods: { /* page methods ---------------------------------------- */
    clearSearchTerm () {
      this.filter = '';
    },
    orQuestionMark (obj) {
      return obj ?? '?';
    },
    millisecondStr (msNum) {
      return msNum ? `${msNum}ms` : '?';
    },
    reissueSearchLink (log) {
      // encodeURIComponent ensures use of the url-safe equivalents of special characters
      const baseQuery = `?b=${encodeURIComponent(window.btoa(log.indicator))}`;
      const allOtherQueryParamsList = { ...log.queryOptions, submit: 'y' };
      const otherQueryParams = Object.entries(allOtherQueryParamsList).map(([key, value]) => `&${key}=${encodeURIComponent(value)}`).join('');
      return `${baseQuery}${otherQueryParams}`;
    },
    deleteLog (id) {
      AuditService.deleteAudit(id).then(() => {
        this.auditLogs = this.auditLogs.filter(log => log._id !== id);
      }).catch((err) => console.log('ERROR - ', err));
    },
    customFilter (data, filterBy) {
      const lowerFilter = filterBy.toLowerCase();
      const simpleFilter = (value) => {
        return value.toLowerCase().includes(lowerFilter);
      };
      const arrayFilter = (arr) => {
        return arr.some(el => simpleFilter(el));
      };
      return simpleFilter(data.iType) || simpleFilter(data.indicator) || arrayFilter(data.tags);
    },
    loadAuditsFromSearch () {
      this.lastTimeRangeInfoSearched = JSON.parse(JSON.stringify(this.timeRangeInfo));
      AuditService.getAudits({
        startMs: this.timeRangeInfo.startMs,
        stopMs: this.timeRangeInfo.stopMs,
        searchTerm: this.filter === '' ? undefined : this.filter.toLowerCase()
      }).then(audits => {
        this.auditLogs = audits;
      });
    }
  },
  mounted () {
    this.loadAuditsFromSearch();
  }
};
</script>

<style scoped>
.indicator-limit-width {
  max-width: 30rem;
  overflow-wrap: break-word;
}
</style>
