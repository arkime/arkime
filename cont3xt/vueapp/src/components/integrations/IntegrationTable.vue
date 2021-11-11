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
      v-for="index in (Math.max(tableLen, 0))">
      <td class="break-all"
        v-for="field in fields"
        :key="`${field.label}-${index}-cell`">
        <integration-value
          :field="field"
          :truncate="true"
          :hide-label="true"
          v-if="tableData[index - 1]"
          :data="tableData[index - 1]"
        />
      </td>
    </tr>
    <tr v-if="tableData.length > tableLen || tableLen > size">
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
            :class="{'disabled':tableLen >= tableData.length}">
            show ALL
            <span v-if="tableData.length > 2000">
              (careful)
            </span>
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
    },
    size: { // the rows of data to display initially and increment or
      type: Number, // decrement thereafter (by clicking more/less)
      default: 50
    }
  },
  data () {
    return {
      tableLen: Math.min(this.tableData.length, this.size)
    };
  },
  methods: {
    showMore () {
      this.tableLen = Math.min(this.tableLen + this.size, this.tableData.length);
    },
    showLess () {
      this.tableLen = Math.max(this.tableLen - this.size, this.size);
    },
    showAll () {
      this.$store.commit('SET_RENDERING_TABLE', true);
      setTimeout(() => { // need settimeout for rendering to take effect
        this.tableLen = this.tableData.length;
      }, 100);
    }
  },
  updated () { // data is rendered
    this.$nextTick(() => {
      this.$store.commit('SET_RENDERING_TABLE', false);
    });
  }
};
</script>
