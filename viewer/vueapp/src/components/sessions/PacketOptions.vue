<!--
Copyright Yahoo Inc.
SPDX-License-Identifier: Apache-2.0
-->
<template>
  <div class="d-flex flex-row text-start align-center flex-wrap packet-options-row">
    <!-- packet count + display type (one group) -->
    <div class="tb-group">
      <v-menu location="bottom start">
        <template #activator="{ props: activatorProps }">
          <v-btn
            v-bind="activatorProps"
            variant="text"
            :disabled="params.gzip || params.image"
            class="packet-options-select-btn">
            {{ packetsLabel }}
            <v-icon
              icon="mdi-menu-down"
              class="ms-1" />
          </v-btn>
        </template>
        <v-list density="compact">
          <v-list-item
            v-for="item in packetCountItems"
            :key="item.value"
            @click="$emit('updateNumPackets', item.value)">
            {{ item.title }}
          </v-list-item>
        </v-list>
      </v-menu>
      <v-tooltip
        v-if="params.gzip || params.image"
        activator="parent">
        {{ $t('sessions.packetOptions.noPacketSelector') }}
      </v-tooltip>

      <v-menu location="bottom start">
        <template #activator="{ props: activatorProps }">
          <v-btn
            v-bind="activatorProps"
            variant="text"
            class="packet-options-select-btn">
            {{ baseLabel }}
            <v-icon
              icon="mdi-menu-down"
              class="ms-1" />
          </v-btn>
        </template>
        <v-list density="compact">
          <v-list-item
            v-for="item in baseItems"
            :key="item.value"
            @click="$emit('updateBase', item.value)">
            {{ item.title }}
          </v-list-item>
        </v-list>
      </v-menu>
    </div>

    <!-- src/dst packets -->
    <div class="tb-group">
      <v-checkbox
        :model-value="params.showSrc"
        :label="$t('common.src')"
        density="compact"
        hide-details
        @update:model-value="$emit('toggleShowSrc')">
        <v-tooltip
          activator="parent"
          location="bottom">
          {{ $t('sessions.packetOptions.srcVisTip') }}
        </v-tooltip>
      </v-checkbox>
      <v-checkbox
        :model-value="params.showDst"
        :label="$t('common.dst')"
        density="compact"
        hide-details
        @update:model-value="$emit('toggleShowDst')">
        <v-tooltip
          activator="parent"
          location="bottom">
          {{ $t('sessions.packetOptions.dstVisTip') }}
        </v-tooltip>
      </v-checkbox>
    </div>

    <!-- options menu -->
    <div class="tb-group">
      <v-menu
        v-model="optionsMenuOpen"
        :close-on-content-click="false"
        location="bottom start">
        <template #activator="{ props: activatorProps }">
          <v-btn
            v-bind="activatorProps"
            variant="text"
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
          <!-- submenu: toggle decodings (tooltip lives on the wrapper because
               a disabled v-list-item has pointer-events: none) -->
          <template v-if="hasDecodings">
            <v-divider />
            <div>
              <v-list-item
                :disabled="params.showFrames"
                append-icon="mdi-chevron-right">
                <v-list-item-title>{{ $t('sessions.packetOptions.decodings') }}</v-list-item-title>
                <v-menu
                  activator="parent"
                  :close-on-content-click="false"
                  location="end"
                  open-on-click
                  :open-on-hover="false">
                  <v-list density="compact">
                    <v-list-item
                      v-for="(value, key) in decodingsClone"
                      :key="key"
                      :active="value.active"
                      @click="toggleDecoding(key)">
                      {{ value.name }}
                      <v-tooltip
                        activator="parent"
                        location="end">
                        {{ $t('sessions.packetOptions.toggleDecodingTip', value.name) }}
                      </v-tooltip>
                    </v-list-item>
                  </v-list>
                </v-menu>
              </v-list-item>
              <v-tooltip
                v-if="params.showFrames"
                activator="parent"
                location="end">
                {{ $t('sessions.packetOptions.decodingsDisabledTip') }}
              </v-tooltip>
            </div>
          </template>
          <v-divider />
          <v-list-item
            target="_blank"
            :href="cyberChefSrcUrl"
            @click="optionsMenuOpen = false">
            {{ $t('sessions.packetOptions.openCyberChefSrc') }}
          </v-list-item>
          <v-list-item
            target="_blank"
            :href="cyberChefDstUrl"
            @click="optionsMenuOpen = false">
            {{ $t('sessions.packetOptions.openCyberChefDst') }}
          </v-list-item>
        </v-list>
      </v-menu>
    </div> <!-- /options menu group -->
  </div> <!-- /packet-options-row -->

  <!-- decoding form: second root so it's a direct flex child of the
       toolbar and w-100 wraps it onto its own full-width row -->
  <div
    v-if="decodingForm"
    class="decoding-form mt-2 pt-2 px-3 d-flex flex-row gap-2 flex-wrap align-center w-100">
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
      optionsMenuOpen: false,
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
    packetsLabel () {
      const item = this.packetCountItems.find(i => i.value === this.params.packets);
      return item ? item.title : String(this.params.packets);
    },
    baseLabel () {
      const item = this.baseItems.find(i => i.value === this.params.base);
      return item ? item.title : this.params.base;
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
        this.optionsMenuOpen = false; // close the menu so the form is visible
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
.packet-options-row,
.decoding-form {
  --row-h: 28px;
  font-size: 12px;
}

/* Decoding-form v-text-field internals stay; v-select was replaced
   by v-menu+v-btn so the field-input overrides are no longer needed
   for the main row. */
.decoding-form :deep(.v-field),
.decoding-form :deep(.v-field__input) {
  min-height: var(--row-h);
  font-size: 12px;
}

/* Every v-btn in the row -- the menu trigger and the decoding-form
   action buttons share the same height + typographic reset
   (no uppercase, no extra letter-spacing). */
.packet-options-row :deep(.v-btn),
.decoding-form :deep(.v-btn) {
  height: var(--row-h);
  min-height: var(--row-h);
  font-size: 12px;
  text-transform: none;
  letter-spacing: normal;
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
