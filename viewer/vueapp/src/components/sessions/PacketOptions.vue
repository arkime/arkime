<!--
Copyright Yahoo Inc.
SPDX-License-Identifier: Apache-2.0
-->
<template>
  <div class="d-flex flex-row text-start gap-2 align-center flex-wrap packet-options-row">
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
          class="packet-options-btn">
          {{ $t('sessions.packetOptions.packetOptions') }}
          <v-icon
            end
            icon="mdi-menu-down" />
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
      v-if="hasDecodings"
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
      class="decoding-form mt-2 pt-2 d-flex flex-row gap-2 flex-wrap align-center w-100">
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
        @click="closeDecodingForm">
        <v-icon icon="mdi-cancel" />
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
        <v-icon icon="mdi-check" />
        <v-tooltip activator="parent">
          {{ $t('common.apply') }}
        </v-tooltip>
      </v-btn>
      <span class="decoding-form-help text-medium-emphasis ms-2">
        <v-icon
          icon="mdi-information"
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
    },
    hasDecodings () {
      return this.decodingsClone && Object.keys(this.decodingsClone).length > 0;
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
    /* Closes the additional-input form for the active decoding. */
    closeDecodingForm () {
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
/* Tighten everything to a single row height -- smaller than Vuetify's
   compact default (~38px). --row-h is the shared height/line-height
   that v-field, v-btn, and v-btn-toggle all snap to. */
.packet-options-row {
  --row-h: 28px;
  font-size: 12px;
}

/* Cap v-select widths so flex doesn't stretch them. */
.packet-options-select {
  display: inline-block;
  min-width: 130px;
  max-width: 170px;
}

/* v-select / v-text-field internals */
.packet-options-row :deep(.v-field),
.packet-options-row :deep(.v-field__input) {
  min-height: var(--row-h);
  font-size: 12px;
}
.packet-options-row :deep(.v-field__input) {
  padding-top: 0;
  padding-bottom: 0;
  display: flex;
  align-items: center;
  line-height: var(--row-h);
}
.packet-options-row :deep(.v-select__selection),
.packet-options-row :deep(.v-select__selection-text) {
  line-height: var(--row-h);
  margin-top: 0;
  margin-bottom: 0;
}
.packet-options-row :deep(.v-field__append-inner) {
  padding-top: 0;
  align-items: center;
}

/* Every v-btn in the row -- the menu trigger, both v-btn-toggle groups,
   and the decoding-form action buttons all share the same height +
   typographic reset (no uppercase, no extra letter-spacing). */
.packet-options-row :deep(.v-btn) {
  height: var(--row-h);
  min-height: var(--row-h);
  font-size: 12px;
  text-transform: none;
  letter-spacing: normal;
}
.packet-options-row :deep(.v-btn-toggle) {
  height: var(--row-h);
}
.packet-options-row :deep(.v-btn-toggle .v-btn) {
  padding: 0 10px;
}
.packet-options-btn.v-btn {
  font-weight: 500;
  padding: 0 12px;
}

/* Decoding form */
.decoding-form {
  border-top: 1px solid rgb(var(--v-theme-neutral));
}
.decoding-form-field {
  min-width: 150px;
  max-width: 220px;
}
.decoding-form-help {
  font-size: 12px;
}
</style>
