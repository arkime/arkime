<template>

  <div>
    <!-- page size -->
    <select class="form-control page-select"
      v-model="length">
      <option v-for="option of lengthOptions"
        :key="option.value"
        :value="option.value">
        {{ option.label }}
      </option>
    </select> <!-- /page size -->
    <!-- paging -->
    <b-pagination size="sm"
      v-model="currentPage"
      :limit="5"
      :per-page="length"
      :total-rows="recordsTotal"
      @input="notifyParent()">
    </b-pagination> <!-- paging -->
    <!-- page info -->
    <span class="pagination-info cursor-help"
      v-b-tooltip.hover
      :title="pagingInfoTitle">
      Showing {{ start + 1 }} - {{ start + length }}
      of {{ recordsFiltered | commaString }} entries
    </span> <!-- /page info -->
  </div>

</template>

<script>
export default {
  name: 'MolochPaging',
  props: [ 'recordsTotal', 'recordsFiltered' ],
  watch: {
    length: function (newVal, oldVal) {
      if (newVal !== oldVal) {
        this.notifyParent();
      }
    }
  },
  data: function () {
    return {
      start: 0,
      currentPage: 1
    };
  },
  computed: {
    length: {
      get: function () {
        let lengthUrlParam = this.$route.query.length;
        // only allow a maximum of 1000
        lengthUrlParam = (lengthUrlParam && lengthUrlParam <= 1000) ? parseInt(lengthUrlParam) : 50;
        return lengthUrlParam;
      },
      set: function (newValue) {
        if (newValue !== this.length) {
          this.$router.push({ query: { ...this.$route.query, length: newValue } });
        }
      }
    },
    lengthOptions: function () {
      let options = [
        { value: 10, label: '10 per page' },
        { value: 50, label: '50 per page' },
        { value: 100, label: '100 per page' },
        { value: 200, label: '200 per page' },
        { value: 500, label: '500 per page' }
      ];

      let exists = false;
      for (let option of options) {
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
      let total = this.$options.filters.commaString(this.recordsTotal);
      return `filtered from ${total} total entries`;
    }
  },
  methods: {
    notifyParent: function () {
      this.start = (this.currentPage - 1) * this.length;

      let pagingParams = {
        start: this.start,
        length: this.length
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
  font-size: .9rem;
  color: var(--color-foreground);
  background-color: var(--color-background);
  display: inline-flex;
  height: 32px !important;
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
  font-size: .8rem;
  color: var(--color-gray-dark);
  border: 1px solid var(--color-gray-light);
  padding: 8px 10px 6px;
  margin-left: -6px;
  border-radius: 0 var(--px-sm) var(--px-sm) 0;
  background-color: var(--color-white);
}
</style>
