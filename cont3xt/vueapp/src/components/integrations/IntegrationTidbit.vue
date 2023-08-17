<template>
  <span style="display: contents" class="cursor-help d-flex flex-row align-items-center overflow-hidden">

    <b-tooltip :target="id" noninteractive>
      <span class="text-primary">{{ tidbit.integration }}</span><span v-if="tidbit.tooltip">: {{ tidbit.tooltip }}</span>
    </b-tooltip>

    <label v-if="labeled" :for="id" class="text-warning m-0">
      {{ tidbit.label }}
    </label>

    <template v-if="tidbit.display === 'badge'">
      <b-badge variant="light" class="mw-100 overflow-hidden mr-1 mb-1" style="font-size:100%" :id="id">
        {{ tidbit.displayValue || tidbit.value }}
      </b-badge>
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

    <template v-else-if="groupColorNames.includes(tidbit.display)">
      <b-badge
        :id="id"
        variant="light"
        style="font-size:100%"
        class="d-inline-flex flex-wrap group-container mw-100 overflow-auto text-wrap mb-1"
        :class="groupClassMap(tidbit.display)">
        <b-badge v-for="(element, index) in (tidbit.displayValue || tidbit.value)"
          :key="index" class="group-member" variant="light">
          {{ element }}
        </b-badge>
      </b-badge>
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
  data () {
    return {
      groupColorNames: ['dangerGroup', 'warningGroup', 'successGroup', 'primaryGroup', 'secondaryGroup', 'infoGroup']
    };
  },
  computed: {
    labeled () {
      return this.tidbit.label?.length;
    }
  },
  methods: {
    groupClassMap (display) {
      return {
        'bg-danger': display === 'dangerGroup',
        'bg-warning': display === 'warningGroup',
        'bg-success': display === 'successGroup',
        'bg-primary': display === 'primaryGroup',
        'bg-secondary': display === 'secondaryGroup',
        'bg-info': display === 'infoGroup'
      };
    }
  }
};
</script>

<style scoped>
.group-container {
  padding: 2px 2px 0 0;
}

.group-member {
  margin: 0 0 2px 2px;
}
</style>
