<template>
  <div class="container-fluid">
    <div class="d-flex align-items-center mb-2">
      <div class="flex-grow-1">
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
              placeholder="Search history by indicator, iType, or tags (case-sensitive)"
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
      </div>

      <div>
        <!-- time range inputs -->
        <time-range-input
            class="ml-2" input-group-size="s" input-width="12rem"
            v-model="timeRangeInfo" :place-holder-tip="timePlaceHolderTip"/>
        <!-- /time range inputs -->
      </div>

      <b-form-checkbox
          button
          size="sm"
          class="ml-2"
          v-model="seeAll"
          v-b-tooltip.hover
          @input="seeAllChanged"
          v-if="roles.includes('cont3xtAdmin')"
          :title="seeAll ? 'Just show the audit logs created from your activity' : 'See all the audit logs that exist for all users (you can because you are an ADMIN!)'">
        <span class="fa fa-user-circle mr-1" />
        See {{ seeAll ? ' MY ' : ' ALL ' }} History
      </b-form-checkbox>
    </div>

    <!--  history table  -->
    <b-table
        small
        hover
        striped
        show-empty
        :dark="getDarkThemeEnabled"
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
            v-b-tooltip.hover.noninteractive
            title="Delete history item">
          <span class="fa fa-trash"/>
        </b-button>
        <b-button
            target="_blank"
            :href="reissueSearchLink(data.item)"
            class="btn btn-xs btn-success"
            v-b-tooltip.hover.noninteractive
            title="Repeat search">
          <span class="fa fa-external-link"/>
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
          <indicator-tag v-for="(tag, index) of data.item.tags" :key="index" :value="tag"/>
        </template>
        <template v-else>
          -
        </template>
      </template>
      <!--   /Tag Column   -->

      <!--   View Column   -->
      <template #cell(viewId)="data">
        <template v-if="data.item.viewId != null">
          <span v-if="viewLookup[data.item.viewId] != null" v-b-tooltip.hover="data.item.viewId" class="text-success">
            {{viewLookup[data.item.viewId]}}
          </span>
          <span v-else>
            {{data.item.viewId}}
          </span>
        </template>
        <template v-else>
          -
        </template>
      </template>
      <!--   /View Column   -->
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
import { paramStr } from '@/utils/paramStr';

export default {
  name: 'AuditHistory',
  components: { IndicatorTag, TimeRangeInput },
  computed: {
    ...mapGetters(['getViews', 'getUser', 'getDarkThemeEnabled']),
    viewLookup () {
      return Object.fromEntries(this.getViews.map(view => [view._id, view.name]));
    },
    roles () {
      return this.getUser?.roles ?? [];
    },
    fields () {
      const showUserIds = this.roles.includes('cont3xtAdmin');

      return [
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
        showUserIds
          ? {
            label: 'User ID',
            key: 'userId',
            sortable: true,
            setWidth: '5rem'
          }
          : undefined,
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
          label: 'View',
          key: 'viewId',
          sortable: true,
          setWidth: '8rem'
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
      ];
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
      sortBy: 'issuedAt',
      sortDesc: true,
      filter: '',
      seeAll: false
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
      return typeof msNum === 'number' ? `${msNum}ms` : '?';
    },
    reissueSearchLink (log) {
      const resubmittedTags = log.tags?.length ? log.tags.join(',') : undefined;
      const allQueryParams = { b: window.btoa(log.indicator), view: log.viewId, tags: resubmittedTags, submit: 'y' };
      return `/${paramStr(allQueryParams)}`;
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
        searchTerm: this.filter === '' ? undefined : this.filter,
        seeAll: this.seeAll
      }).then(audits => {
        this.auditLogs = audits;
      });
    },
    seeAllChanged () {
      this.$router.push({ query: { ...this.$route.query, seeAll: this.seeAll ? 'true' : undefined } });
      this.loadAuditsFromSearch();
    }
  },
  mounted () {
    if (this.$route.query.seeAll === 'true') {
      this.seeAll = true;
    }

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
