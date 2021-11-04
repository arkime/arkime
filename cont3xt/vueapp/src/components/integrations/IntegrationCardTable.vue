<template>
  <table class="table table-sm table-striped table-bordered">
    <tr>
      <th
        v-for="field in fields"
        :key="`${field.label}-header`">
        {{ field.label }}
      </th>
    </tr>
    <tr
      :key="index"
      v-for="(data, index) in tableData">
      <td
        v-for="field in fields"
        :key="`${field.label}-${index}-cell`">
        {{ getFieldValue(field, data) }}
      </td>
    </tr>
  </table>
</template>

<script>
export default {
  name: 'IntegrationCardTable',
  props: {
    fields: Array,
    tableData: Array
  },
  methods: {
    getFieldValue (field, row) { // TODO combine this with parent function?
      let value = row;

      for (const p of field.path) {
        value = value[p];
      }

      return value;
    }
  }
};
</script>
