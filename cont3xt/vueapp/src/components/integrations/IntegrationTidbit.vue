<template>
  <span style="display: contents" class="cursor-help" id="testtest">
    <!--  tooltip  -->
    <b-tooltip :target="id" noninteractive>
      <span class="text-primary">{{ tidbit.integration }}</span><span v-if="tidbit.tooltip">: {{ tidbit.tooltip }}</span>

    </b-tooltip><!--  /tooltip  -->

    <label v-if="labeled" :for="id" class="text-warning">
      {{ tidbit.label }}
    </label>

    <template v-if="tidbit.display === 'badge'">
      <h5 class="mr-1 align-self-end" :id="id">
        <b-badge variant="light">
          {{ tidbit.displayValue || tidbit.value }}
        </b-badge>
      </h5>
    </template>
    <template v-else-if="tidbit.display === 'cont3xtField'">
      <cont3xt-field class="mr-1 align-self-center" :id="id"
        :value="tidbit.value"
        :display="tidbit.displayValue"
      />
    </template>
    <template v-else-if="tidbit.display === 'cont3xtCopyLink'">
      <cont3xt-field class="mr-1 align-self-center" :id="id"
          :options="{ copy: 'copy link' }"
          :value="tidbit.value"
          :display="tidbit.displayValue"
      />
    </template>
    <template v-else-if="tidbit.display === 'warningEnums'">
      <h5 :id="id" class="align-self-end mr-1">
        <b-badge variant="light" class="d-inline-flex flex-wrap bg-warning" style="padding: 2px;">
          <b-badge v-for="(element, index) in (tidbit.displayValue || tidbit.value)"
                   :key="index" :class="{ 'ml-1': index > 0 }" variant="light">
            {{ element }}
          </b-badge>
        </b-badge>
      </h5>
    </template>
    <template v-else>
      No display for {{ tidbit.display }} ({{ tidbit.value }})
    </template>
  </span>
</template>

<script>
import Cont3xtField from '@/utils/Field';

export default {
  name: 'IntegrationTidbit',
  components: {
    Cont3xtField
  },
  props: {
    tidbit: {
      type: Object,
      required: true
    },
    id: {
      type: String,
      required: true
    }
  },
  computed: {
    labeled () {
      return this.tidbit.label?.length;
    }
  }
};
</script>

<style scoped>

</style>
