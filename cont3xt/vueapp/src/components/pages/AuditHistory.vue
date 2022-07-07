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

    <!--  primary page content  -->
    <!--  history table  -->
    <b-table
        small
        hover
        striped
        show-empty
        :filter="filter"
        :filter-function="customFilter"
        :fields="fields"
        :items="auditLogs"
        :sort-by.sync="sortBy"
        :sort-desc.sync="sortDesc"
        empty-text="There is no history to show"
        :empty-filtered-text="`There are now searches that contain: ${filter}`"
    >
      <!--   customize column sizes   -->
      <template #table-colgroup="scope">
        <col
            v-for="field in scope.fields"
            :key="field.key"
            :style="{ width: field.setWidth }"
        >
      </template>
      <!--   customize column sizes   -->

      <template #cell(buttons)="data">
        <b-button
            @click="deleteLog(data.item._id)"
            class="btn btn-xs btn-warning"
            v-b-tooltip.hover="'Delete log'">
          <span class="fa fa-trash"/>
        </b-button>
        <b-button
            target="_blank"
            :href="reissueSearchLink(data.item)"
            class="btn btn-xs btn-success"
            v-b-tooltip.hover="'Repeat search'">
          <span class="fa fa-external-link"/>
        </b-button>
      </template>

      <template #cell(indicator)="data">
        <div class="indicator-limit-width">
          {{ data.item.indicator }}
        </div>
      </template>

      <template #cell(tags)="data">
        <template v-if="data.item.tags.length">
          <indicator-tag v-for="tag in data.item.tags" :key="tag" :value="tag"/>
        </template>
        <template v-else>
          -
        </template>
      </template>
      <template #cell(queryOptions)="data">
        <template v-if="Object.keys(data.item.queryOptions).length">
          {{ JSON.stringify(data.item.queryOptions) }}
        </template>
        <template v-else>
          -
        </template>
      </template>
    </b-table>
    <!--  history table  -->

  </div>
</template>

<script>
import AuditService from '@/components/services/AuditService';
import { reDateString } from '@/utils/filters';
import IndicatorTag from '@/utils/IndicatorTag';

export default {
  name: 'AuditHistory',
  components: { IndicatorTag },
  data () {
    return {
      auditLogs: [],
      filteredLogs: [],
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
          setWidth: '35rem'
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
        }
      ],
      sortBy: 'name',
      sortDesc: false,
      filter: ''
    };
  },
  methods: {
    clearSearchTerm () {
      this.filter = '';
    },
    reissueSearchLink (log) {
      const otherQueryParams = Object.entries(log.queryOptions).map(([key, value]) => `&${key}=${value}`).join('');
      return `?b=${window.btoa(log.indicator)}${otherQueryParams}`;
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
    }
  },
  mounted () {
    AuditService.getAudits().then(audits => {
      this.auditLogs = audits;
    });
  }
};
</script>

<style scoped>
.indicator-limit-width {
  max-width: 35rem;
  overflow-wrap: break-word;
}
</style>
