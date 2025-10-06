<template>
  <div class="connections-popup">
    <div class="mb-2 mt-2">
      <strong>Link</strong>
      <a
        class="pull-right cursor-pointer no-decoration"
        @click="$emit('close')"
      >
        <span class="fa fa-close" />
      </a>
    </div>
    <div>
      <arkime-session-field
        :value="dataLink.source.id"
        :session="dataLink"
        :expr="dataLink.srcExp"
        :field="fields[dataLink.srcDbField]"
        :pull-left="true"
      />
    </div>
    <div class="mb-2">
      <arkime-session-field
        :value="dataLink.target.id"
        :session="dataLink"
        :expr="dataLink.dstExp"
        :field="fields[dataLink.dstDbField]"
        :pull-left="true"
      />
    </div>

    <dl class="dl-horizontal">
      <dt>{{ $t('common.sessions') }}</dt>
      <dd>{{ dataLink.value }}&nbsp;</dd>

      <span
        v-for="field in linkFields"
        :key="field"
      >
        <template v-if="fields[field]">
          <dt>
            {{ fields[field].friendlyName }}
          </dt>
          <dd>
            <span v-if="!Array.isArray(dataLink[field])">
              <arkime-session-field
                :value="dataLink[field]"
                :session="dataLink"
                :expr="fields[field].exp"
                :field="fields[field]"
                :pull-left="true"
              />
            </span>
            <span
              v-else
              v-for="value in dataLink[field]"
              :key="`${field}-${value}`"
            >
              <arkime-session-field
                :value="value"
                :session="dataLink"
                :expr="fields[field].exp"
                :field="fields[field]"
                :pull-left="true"
              />
            </span>&nbsp;
          </dd>
        </template>
      </span>
    </dl>

    <a
      class="cursor-pointer no-decoration"
      href="javascript:void(0)"
      @click="$emit('hideLink')"
    >
      <span class="fa fa-eye-slash me-2" />
      {{ $t('connections.hideLink') }}
    </a>
  </div>
</template>

<script setup>
import ArkimeSessionField from '../sessions/SessionField.vue';

// Define Props
defineProps({
  dataLink: {
    type: Object,
    required: true
  },
  fields: {
    type: Object,
    required: true
  },
  linkFields: {
    type: Array,
    required: true
  }
});

// Define Emits
defineEmits(['hideLink', 'close']);
</script>
