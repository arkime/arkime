<template>
  <div>
    <!-- search -->
    <div class="mb-1"
      v-if="data.length > 1">
      <b-input-group size="sm">
        <template #prepend>
          <b-input-group-text>
            <span class="fa fa-search" />
          </b-input-group-text>
        </template>
        <b-form-input
          debounce="400"
          v-model="searchTerm"
          placeholder="Search table values"
        />
      </b-input-group>
    </div> <!-- /search -->
    <!-- data -->
    <table class="table table-sm table-striped table-bordered small">
      <tr>
        <th
          @click="sortBy(field)"
          v-for="field in fields"
          :key="`${field.label}-header`"
          :class="{'cursor-pointer':isSortable(field)}">
          {{ field.label }}
          <template v-if="isSortable(field)">
            <span
              class="fa fa-sort"
              v-if="sortField !== field.label"
            />
            <span
              class="fa fa-sort-desc"
              v-else-if="sortField === field.label && desc"
            />
            <span
              class="fa fa-sort-asc"
              v-else-if="sortField === field.label && !desc"
            />
          </template>
        </th>
      </tr>
      <tr
        :key="index"
        v-for="index in (Math.max(tableLen, 0))">
        <td class="break-all"
          v-for="field in fields"
          :key="`${field.label}-${index}-cell`">
          <integration-value
            :field="field"
            :truncate="true"
            :hide-label="true"
            v-if="filteredData[index - 1]"
            :data="filteredData[index - 1]"
          />
        </td>
      </tr>
      <tr v-if="filteredData.length > tableLen || tableLen > size">
        <td :colspan="fields.length">
          <div class="d-flex justify-content-between">
            <a
              @click="showLess"
              class="btn btn-link btn-xs"
              :class="{'disabled':tableLen <= size}">
              show less...
            </a>
            <a
              @click="showAll"
              class="btn btn-link btn-xs"
              :class="{'disabled':tableLen >= filteredData.length}">
              show ALL
              <span v-if="filteredData.length > 2000">
                (careful)
              </span>
            </a>
            <a
              @click="showMore"
              class="btn btn-link btn-xs"
              :class="{'disabled':tableLen >= filteredData.length}">
              show more...
            </a>
          </div>
        </td>
      </tr>
    </table> <!-- /data -->
  </div>
</template>

<script>
export default {
  name: 'IntegrationCardTable',
  components: {
    // NOTE: need async import here because there's a circular dependency
    // between IntegrationValue and IntegrationTable
    // see: vuejs.org/v2/guide/components.html#Circular-References-Between-Components
    IntegrationValue: () => import('@/components/integrations/IntegrationValue')
  },
  props: {
    fields: { // the list of fields to display in the table (populates the
      type: Array, // column headers and determines how to access the data)
      required: true
    },
    tableData: { // the data to display in the table
      type: [Array, Object], // if object, turns the object into an array of length 1
      required: true
    },
    size: { // the rows of data to display initially and increment or
      type: Number, // decrement thereafter (by clicking more/less)
      default: 50
    },
    defaultSortField: { // the default field to sort the table by
      type: String // if undefined, the table is not sorted
    },
    defaultSortDirection: { // the default sort direction (asc or desc)
      type: String,
      default: 'desc'
    }
  },
  data () {
    return {
      searchTerm: '',
      sortField: this.defaultSortField || undefined,
      tableLen: Math.min(this.tableData.length || 1, this.size),
      desc: this.defaultSortDirection && this.defaultSortDirection === 'desc',
      data: Array.isArray(this.tableData) ? this.tableData : [this.tableData],
      filteredData: Array.isArray(this.tableData) ? this.tableData : [this.tableData]
    };
  },
  mounted () {
    if (this.sortField) {
      for (const field of this.fields) {
        if (field.path.includes(this.sortField)) {
          this.sortBy(field);
          break;
        }
      }
    }
  },
  watch: {
    searchTerm (newValue, oldValue) {
      if (!newValue) {
        this.filteredData = this.data;
        this.setTableLen();
        return;
      }

      this.filteredData = this.data.filter((row) => {
        let match = false;
        const query = newValue.toLowerCase();

        for (const c in row) {
          if (!row[c]) { continue; }
          match = row[c].toString().toLowerCase().match(query)?.length > 0;
          if (match) { break; }
        }

        return match;
      });

      this.setTableLen();
    }
  },
  methods: {
    /* exposed page functions ---------------------------------------------- */
    showMore () {
      this.tableLen = Math.min(this.tableLen + this.size, this.filteredData.length);
    },
    showLess () {
      this.tableLen = Math.max(this.tableLen - this.size, this.size);
    },
    showAll () {
      this.$store.commit('SET_RENDERING_TABLE', true);
      setTimeout(() => { // need settimeout for rendering to take effect
        this.tableLen = this.filteredData.length;
      }, 100);
    },
    isSortable (field) {
      return field.type === 'string' || field.type === 'date';
    },
    sortBy (field) {
      if (!this.isSortable(field)) {
        return;
      }

      if (this.sortField === field.label) {
        this.desc = !this.desc;
      } else {
        this.desc = true;
      }

      this.sortField = field.label;

      this.filteredData.sort((a, b) => {
        let valueA = JSON.parse(JSON.stringify(a));
        let valueB = JSON.parse(JSON.stringify(b));

        for (const p of field.path) {
          valueA = valueA[p];
          valueB = valueB[p];
        }

        if (!valueA) { valueA = ''; }
        if (!valueB) { valueB = ''; }

        if (field.type === 'string') {
          if (this.desc) {
            return valueA.toString().localeCompare(valueB);
          } else {
            return valueB.toString().localeCompare(valueA);
          }
        } else {
          valueA = new Date(valueA);
          valueB = new Date(valueB);
          if (this.desc) {
            return valueA.getTime() < valueB.getTime() ? 1 : -1;
          } else {
            return valueB.getTime() < valueA.getTime() ? 1 : -1;
          }
        }
      });
    },
    // helpers ------------------------------------------------------------- */
    setTableLen () {
      this.tableLen = Math.min(this.filteredData.length, this.size);
    }
  },
  updated () { // data is rendered
    this.$nextTick(() => {
      this.$store.commit('SET_RENDERING_TABLE', false);
    });
  }
};
</script>
