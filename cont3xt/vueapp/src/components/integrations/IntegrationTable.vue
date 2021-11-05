<template>
  <table class="table table-sm table-striped table-bordered small">
    <tr>
      <th
        v-for="field in fields"
        :key="`${field.label}-header`">
        {{ field.label }}
      </th>
    </tr>
    <tr
      :key="index"
      v-for="index in (tableLen - 1)">
      <td
        class="break-word"
        v-for="field in fields"
        :key="`${field.label}-${index}-cell`">
        <integration-value
          :field="field"
          :hide-label="true"
          :data="tableData[index]"
        />
      </td>
    </tr>
    <tr>
      <td :colspan="fields.length">
        <div class="d-flex justify-content-between">
          <a
            @click="showLess"
            class="btn btn-link btn-xs"
            :class="{'disabled':tableLen <= 100}">
            show less...
          </a>
          <a
            @click="showAll"
            class="btn btn-link btn-xs"
            :class="{'disabled':tableLen >= tableData.length}">
            show ALL
          </a>
          <a
            @click="showMore"
            class="btn btn-link btn-xs"
            :class="{'disabled':tableLen >= tableData.length}">
            show more...
          </a>
        </div>
      </td>
    </tr>
  </table>
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
      type: Array,
      required: true
    }
  },
  data () {
    return {
      tableLen: Math.min(this.tableData.length, 100)
    };
  },
  methods: {
    showMore () {
      this.tableLen = Math.min(this.tableLen + 100, this.tableData.length);
    },
    showLess () {
      this.tableLen = Math.max(this.tableLen - 100, 100);
    },
    showAll () {
      this.tableLen = this.tableData.length;
    }
  }
};
</script>
