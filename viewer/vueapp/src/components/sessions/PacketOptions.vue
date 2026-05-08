<!--
Copyright Yahoo Inc.
SPDX-License-Identifier: Apache-2.0
-->
<template>
  <div class="d-flex flex-row text-start gap-2 align-items-center flex-wrap packet-options-row">
    <!-- # packets -->
    <span class="packet-options-select">
      <v-select
        :model-value="params.packets"
        :items="packetCountItems"
        :disabled="params.gzip || params.image"
        density="compact"
        variant="outlined"
        hide-details
        @update:model-value="$emit('updateNumPackets', $event)" />
      <v-tooltip
        v-if="params.gzip || params.image"
        activator="parent">
        {{ $t('sessions.packetOptions.noPacketSelector') }}
      </v-tooltip>
    </span> <!-- /# packets -->

    <!-- packet display type -->
    <v-select
      class="packet-options-select"
      :model-value="params.base"
      :items="baseItems"
      density="compact"
      variant="outlined"
      hide-details
      @update:model-value="$emit('updateBase', $event)" /> <!-- /packet display type -->

    <!-- toggle options menu -->
    <v-menu location="bottom start">
      <template #activator="{ props: activatorProps }">
        <v-btn
          v-bind="activatorProps"
          variant="tonal"
          size="small"
          class="packet-options-btn">
          {{ $t('sessions.packetOptions.packetOptions') }}
          <v-icon end>
            fa-caret-down
          </v-icon>
        </v-btn>
      </template>
      <v-list density="compact">
        <v-list-item @click="$emit('toggleShowFrames')">
          {{ $t(params.showFrames ? 'sessions.packetOptions.showReassembled' : 'sessions.packetOptions.showRaw') }}
        </v-list-item>
        <v-list-item @click="$emit('toggleTimestamps')">
          {{ $t(params.ts ? 'sessions.packetOptions.hideInfo' : 'sessions.packetOptions.showInfo') }}
        </v-list-item>
        <v-list-item
          v-if="params.base === 'hex'"
          @click="$emit('toggleLineNumbers')">
          {{ $t(params.line ? 'sessions.packetOptions.hideLineNumbers' : 'sessions.packetOptions.showLineNumbers') }}
        </v-list-item>
        <v-list-item
          v-if="!params.showFrames"
          @click="$emit('toggleCompression')">
          {{ $t(params.gzip ? 'sessions.packetOptions.disableUncompressing' : 'sessions.packetOptions.enableUncompressing') }}
          <v-tooltip
            activator="parent"
            location="end">
            {{ $t(params.gzip ? 'sessions.packetOptions.disableUncompressing' : 'sessions.packetOptions.enableUncompressingTip') }}
          </v-tooltip>
        </v-list-item>
        <v-list-item
          v-if="!params.showFrames"
          @click="$emit('toggleImages')">
          {{ $t(params.image ? 'sessions.packetOptions.hideFiles' : 'sessions.packetOptions.showFiles') }}
          <v-tooltip
            activator="parent"
            location="end">
            {{ $t(params.image ? 'sessions.packetOptions.hideFiles' : 'sessions.packetOptions.showFilesTip') }}
          </v-tooltip>
        </v-list-item>
        <v-divider />
        <v-list-item
          target="_blank"
          :href="cyberChefSrcUrl">
          {{ $t('sessions.packetOptions.openCyberChefSrc') }}
        </v-list-item>
        <v-list-item
          target="_blank"
          :href="cyberChefDstUrl">
          {{ $t('sessions.packetOptions.openCyberChefDst') }}
        </v-list-item>
      </v-list>
    </v-menu> <!-- /toggle options menu -->

    <!-- src/dst packets -->
    <v-btn-toggle
      :model-value="srcDstValue"
      multiple
      density="compact"
      variant="outlined"
      divided
      @update:model-value="onSrcDstToggle">
      <v-btn value="src">
        {{ $t('common.src') }}
        <v-tooltip
          activator="parent"
          location="bottom">
          {{ $t('sessions.packetOptions.srcVisTip') }}
        </v-tooltip>
      </v-btn>
      <v-btn value="dst">
        {{ $t('common.dst') }}
        <v-tooltip
          activator="parent"
          location="bottom">
          {{ $t('sessions.packetOptions.dstVisTip') }}
        </v-tooltip>
      </v-btn>
    </v-btn-toggle> <!-- /src/dst packets -->

    <!-- decodings -->
    <v-btn-toggle
      v-if="decodingsClone && Object.keys(decodingsClone).length"
      :model-value="activeDecodingKeys"
      multiple
      density="compact"
      variant="outlined"
      divided
      :disabled="params.showFrames">
      <v-btn
        v-for="(value, key) in decodingsClone"
        :key="key"
        :value="key"
        :disabled="params.showFrames"
        @click="toggleDecoding(key)">
        {{ value.name }}
        <v-tooltip
          activator="parent"
          location="bottom">
          {{ $t('sessions.packetOptions.toggleDecodingTip', value.name) }}
        </v-tooltip>
      </v-btn>
    </v-btn-toggle> <!-- /decodings -->

    <!-- decoding form -->
    <div
      v-if="decodingForm"
      class="decoding-form mt-2 pt-2 d-flex flex-row gap-2 flex-wrap align-items-center w-100">
      <template
        v-for="field in decodingsClone[decodingForm].fields"
        :key="field.name">
        <v-text-field
          v-if="!field.disabled"
          v-model="field.value"
          :label="field.name"
          :type="field.type"
          density="compact"
          variant="outlined"
          hide-details
          class="decoding-form-field" />
      </template>
      <v-btn
        size="small"
        variant="tonal"
        color="warning"
        :aria-label="$t('common.cancel')"
        @click="closeDecodingForm(false)">
        <v-icon icon="fa-ban" />
        <v-tooltip activator="parent">
          {{ $t('common.cancel') }}
        </v-tooltip>
      </v-btn>
      <v-btn
        size="small"
        variant="tonal"
        color="primary"
        :aria-label="$t('common.apply')"
        @click="applyDecoding(decodingForm)">
        <v-icon icon="fa-check" />
        <v-tooltip activator="parent">
          {{ $t('common.apply') }}
        </v-tooltip>
      </v-btn>
      <span class="decoding-form-help text-medium-emphasis ms-2">
        <v-icon
          icon="fa-info-circle"
          size="x-small"
          class="me-1" />
        {{ decodingsClone[decodingForm].title }}
      </span>
    </div> <!-- /decoding form -->
  </div>
</template>

<script>
export default {
  name: 'PacketOptions',
  props: {
    params: {
      type: Object,
      default: () => ({})
    },
    decodings: {
      type: Object,
      default: () => ({})
    },
    cyberChefSrcUrl: {
      type: String,
      default: ''
    },
    cyberChefDstUrl: {
      type: String,
      default: ''
    }
  },
  emits: [
    'updateNumPackets',
    'updateBase',
    'toggleShowFrames',
    'toggleTimestamps',
    'toggleLineNumbers',
    'toggleCompression',
    'toggleImages',
    'toggleShowSrc',
    'toggleShowDst',
    'updateDecodings',
    'applyDecodings'
  ],
  data () {
    return {
      decodingForm: false,
      decodingsClone: JSON.parse(JSON.stringify(this.decodings))
    };
  },
  computed: {
    packetCountItems () {
      return [50, 200, 500, 1000, 2000].map(opt => ({
        value: opt,
        title: this.$t('common.packetCount', opt)
      }));
    },
    baseItems () {
      return [
        { value: 'natural', title: this.$t('sessions.packetOptions.natural') },
        { value: 'ascii', title: this.$t('sessions.packetOptions.ascii') },
        { value: 'utf8', title: this.$t('sessions.packetOptions.utf8') },
        { value: 'hex', title: this.$t('sessions.packetOptions.hex') }
      ];
    },
    srcDstValue () {
      const v = [];
      if (this.params.showSrc) v.push('src');
      if (this.params.showDst) v.push('dst');
      return v;
    },
    activeDecodingKeys () {
      return Object.keys(this.decodingsClone || {}).filter(k => this.decodingsClone[k].active);
    }
  },
  watch: {
    decodings: {
      deep: true,
      handler (newVal) {
        if (newVal) {
          this.decodingsClone = JSON.parse(JSON.stringify(newVal));
        }
      }
    }
  },
  methods: {
    onSrcDstToggle (selected) {
      const wantSrc = selected.includes('src');
      const wantDst = selected.includes('dst');
      if (wantSrc !== this.params.showSrc) this.$emit('toggleShowSrc');
      if (wantDst !== this.params.showDst) this.$emit('toggleShowDst');
    },
    /**
     * Toggles a decoding on or off
     * If a decoding needs more input, shows form
     * @param {string} key Identifier of the decoding to toggle
     */
    toggleDecoding (key) {
      const decoding = this.decodingsClone[key];

      const isActive = !decoding.active;
      decoding.active = isActive;
      this.$emit('updateDecodings', this.decodingsClone);

      if (decoding.fields && isActive) {
        this.decodingForm = key;
      } else {
        this.decodingForm = false;
        this.applyDecoding(key);
      }
    },
    /**
     * Closes the form for additional decoding input
     * @param {bool} active The active state of the decoding
     */
    closeDecodingForm (active) {
      if (this.decodingForm) {
        this.$emit('updateDecodings', this.decodingsClone);
      }

      this.decodingForm = false;
    },
    /**
     * Sets the decode param, issues query, and closes form if necessary
     * @param {key} key Identifier of the decoding to apply
     */
    applyDecoding (key) {
      const paramsClone = JSON.parse(JSON.stringify(this.params.decode));
      paramsClone[key] = {};

      const decoding = this.decodingsClone[key];

      if (decoding.active) {
        if (decoding.fields) {
          for (const field of decoding.fields) {
            paramsClone[key][field.key] = field.value;
          }
        }
      } else {
        delete paramsClone[key];
      }

      this.$emit('applyDecodings', paramsClone);
      this.closeDecodingForm();

      // update local storage
      localStorage['moloch-decodings'] = JSON.stringify(paramsClone);
    }
  }
};
</script>

<style scoped>
.packet-options-row {
  /* compact rhythm matching the session-options toolbar above */
  font-size: 13px;
}

/* Hold the v-select width down -- otherwise compact density still
   stretches the field to fill flex space. */
.packet-options-select {
  display: inline-block;
  min-width: 140px;
  max-width: 180px;
}

.packet-options-btn {
  text-transform: none;
  letter-spacing: normal;
  font-weight: 500;
}

.decoding-form {
  border-top: 1px solid var(--color-gray);
}

.decoding-form-field {
  min-width: 160px;
  max-width: 240px;
}

.decoding-form-help {
  font-size: 12px;
}
</style>
