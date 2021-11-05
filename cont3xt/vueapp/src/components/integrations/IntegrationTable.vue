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
        class="break-word"
        v-for="field in fields"
        :key="`${field.label}-${index}-cell`">
        <integration-value
          :data="data"
          :field="field"
          :hide-label="true"
        />
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
  components: {
    // NOTE: need async import here because there's a circular dependency
    // between IntegrationValue and IntegrationTable
    // see: vuejs.org/v2/guide/components.html#Circular-References-Between-Components
    IntegrationValue: () => import('@/components/integrations/IntegrationValue')
  }
};
</script>
