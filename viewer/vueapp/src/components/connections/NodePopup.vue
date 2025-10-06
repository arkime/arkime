<template>
  <div class="connections-popup">
    <div class="mb-2 mt-2">
      <strong>
        <arkime-session-field
          :value="dataNode.id"
          :session="dataNode"
          :expr="dataNode.exp"
          :field="fields[dataNode.dbField]"
          :pull-left="true"
        />
      </strong>
      <a
        class="pull-right cursor-pointer no-decoration"
        @click="$emit('close')"
      >
        <span class="fa fa-close" />
      </a>
    </div>

    <dl class="dl-horizontal">
      <dt>{{ $t('connections.type') }}</dt>
      <dd>{{ ['','Source','Target','Both'][dataNode.type] }}</dd>
      <dt>{{ $t('connections.links') }}</dt>
      <dd>{{ dataNode.weight || dataNode.cnt }}&nbsp;</dd>
      <dt>{{ $t('common.sessions') }}</dt>
      <dd>{{ dataNode.sessions }}&nbsp;</dd>

      <span
        v-for="fieldKey in nodeFields"
        :key="fieldKey"
      >
        <template v-if="fields[fieldKey]">
          <dt>
            {{ fields[fieldKey].friendlyName }}
          </dt>
          <dd>
            <span v-if="!Array.isArray(dataNode[fieldKey])">
              <arkime-session-field
                :value="dataNode[fieldKey]"
                :session="dataNode"
                :expr="fields[fieldKey].exp"
                :field="fields[fieldKey]"
                :pull-left="true"
              />
            </span>
            <span v-else>
              <arkime-session-field
                v-for="(value, index) in dataNode[fieldKey]"
                :key="`${fieldKey}-${index}`"
                :value="value"
                :session="dataNode"
                :expr="fields[fieldKey].exp"
                :field="fields[fieldKey]"
                :pull-left="true"
              />
            </span>&nbsp;
          </dd>
        </template>
      </span>

      <div v-if="baselineDate !== '0'">
        <dt>{{ $t('connections.resultSet') }}</dt>
        <dd>{{ ['','✨Actual','🚫 Baseline','Both'][dataNode.inresult] }}</dd>
      </div>
    </dl>

    <a
      class="cursor-pointer no-decoration"
      href="javascript:void(0)"
      @click.stop.prevent="$emit('hideNode')"
    >
      <span class="fa fa-eye-slash me-2" />
      {{ $t('connections.hideNode') }}
    </a>
  </div>
</template>

<script setup>
import ArkimeSessionField from '../sessions/SessionField.vue';

// Define Props
defineProps({
  dataNode: {
    type: Object,
    required: true
  },
  fields: {
    type: Object,
    required: true
  },
  baselineDate: {
    type: String,
    default: '0'
  },
  nodeFields: {
    type: Array,
    required: true
  }
});

// Define Emits
defineEmits(['hideNode', 'close']);
</script>
