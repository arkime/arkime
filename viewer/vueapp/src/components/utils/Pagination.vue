<template>

  <div>
    <div v-if="!infoOnly">
      <!-- page size -->
      <b-select
        role="listbox"
        v-model="length"
        :options="lengthOptions"
        class="form-control page-select"
      /> <!-- /page size -->
      <!-- paging -->
      <b-pagination
        size="sm"
        :limit="5"
        hide-ellipsis
        :per-page="length"
        v-model="currentPage"
        :total-rows="recordsFiltered"
        @input="notifyParent(true)">
      </b-pagination> <!-- paging -->
      <!-- page info -->
      <div
        :title="pagingInfoTitle"
        class="pagination-info cursor-help"
        v-b-tooltip.hover.right="pagingInfoTitle">
        Showing
        <span v-if="recordsFiltered">
          {{ start + 1 | commaString }}
        </span>
        <span v-else>
          {{ start | commaString }}
        </span>
        <span v-if="recordsFiltered">
          - {{ Math.min((start + length), recordsFiltered) | commaString }}
        </span>
        of {{ recordsFiltered | commaString }} entries
      </div> <!-- /page info -->
    </div>
    <div v-else
      class="pagination-info info-only">
      Showing {{ recordsFiltered | commaString }} entries,
      filtered from {{ recordsTotal | commaString }} total entries
    </div>
  </div>

</template>

<script>
export default {
  name: 'MolochPaging',
  props: [
    'recordsTotal',
    'recordsFiltered',
    'lengthDefault',
    'infoOnly'
  ],
  data: function () {
    return {
      start: 0,
      currentPage: 1
    };
  },
  computed: {
    length: {
      get: function () {
        // only allow a maximum of 1000
        return Math.min(parseInt(this.$route.query.length || this.lengthDefault || 50), 1000);
      },
      set: function (newValue) {
        if (newValue !== this.length) {
          const newQuery = {
            ...this.$route.query,
            length: newValue
          };

          const exprChanged = this.$store.state.expression !== this.$route.query.expression;
          if (exprChanged) {
            newQuery.expression = this.$store.state.expression;
          }

          this.$router.push({ query: newQuery });

          // only issue a new query if the expression hasn't changed. if it
          // has changed, a query will be issued by ExpressionTypeahead.vue
          this.notifyParent(!exprChanged);
        }
      }
    },
    lengthOptions: function () {
      const options = [
        { value: 10, text: '10 per page' },
        { value: 50, text: '50 per page' },
        { value: 100, text: '100 per page' },
        { value: 200, text: '200 per page' },
        { value: 500, text: '500 per page' },
        { value: 1000, text: '1000 per page' }
      ];

      let exists = false;
      for (const option of options) {
        if (this.length === option.value) {
          exists = true;
          break;
        }
      }

      if (!exists) {
        options.push({
          value: this.length,
          label: `${this.length} per page`
        });
      }

      options.sort(function (a, b) {
        return a.value - b.value;
      });

      return options;
    },
    pagingInfoTitle: function () {
      const total = this.$options.filters.commaString(this.recordsTotal);
      return `filtered from ${total} total entries`;
    }
  },
  methods: {
    notifyParent: function (issueQuery) {
      this.start = (this.currentPage - 1) * this.length;

      const pagingParams = {
        start: this.start,
        length: this.length,
        issueQuery
      };

      this.$emit('changePaging', pagingParams);
    }
  }
};
</script>

<style scoped>
.pagination {
  display: inline-flex;
}

select.page-select {
  width: 120px;
  font-size: .8rem;
  display: inline-flex;
  height: 31px !important;
  margin-top: 1px;
  margin-right: -5px;
  margin-bottom: var(--px-xs);
  padding-top: var(--px-xs);
  padding-bottom: var(--px-xs);
  border-right: none;
  border-radius: var(--px-sm) 0 0 var(--px-sm);
  border-color: var(--color-gray-light);
  -webkit-appearance: none;
}

.pagination-info {
  display: inline-block;
  font-size: .8rem;
  color: var(--color-gray-dark);
  border: 1px solid var(--color-gray-light);
  padding: 5px 10px;
  margin-left: -6px;
  border-radius: 0 var(--px-sm) var(--px-sm) 0;
  background-color: var(--color-white);
}

.pagination-info.info-only {
  border-radius: var(--px-sm);
}
</style>
