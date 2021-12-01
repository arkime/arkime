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
          v-if="data[index - 1]"
          :data="data[index - 1]"
        />
      </td>
    </tr>
    <tr v-if="data.length > tableLen || tableLen > size">
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
            :class="{'disabled':tableLen >= data.length}">
            show ALL
            <span v-if="data.length > 2000">
              (careful)
            </span>
          </a>
          <a
            @click="showMore"
            class="btn btn-link btn-xs"
            :class="{'disabled':tableLen >= data.length}">
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
      type: [Array, Object], // if object, turns the object into an array of length 1
      required: true
    },
    size: { // the rows of data to display initially and increment or
      type: Number, // decrement thereafter (by clicking more/less)
      default: 50
    }
  },
  data () {
    return {
      data: Array.isArray(this.tableData) ? this.tableData : [this.tableData],
      tableLen: Math.min(this.tableData.length || 1, this.size)
    };
  },
  methods: {
    showMore () {
      this.tableLen = Math.min(this.tableLen + this.size, this.data.length);
    },
    showLess () {
      this.tableLen = Math.max(this.tableLen - this.size, this.size);
    },
    showAll () {
      this.$store.commit('SET_RENDERING_TABLE', true);
      setTimeout(() => { // need settimeout for rendering to take effect
        this.tableLen = this.data.length;
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
