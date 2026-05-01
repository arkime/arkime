<!--
Copyright Yahoo Inc.
SPDX-License-Identifier: Apache-2.0
-->
<template>
  <div class="d-flex flex-row text-start gap-1 align-items-start flex-wrap">
    <!-- # packets -->
    <span>
      <select
        :value="params.packets"
        :disabled="params.gzip || params.image"
        class="form-control form-control-sm me-1"
        :class="{'disabled':params.gzip}"
        @change="$emit('updateNumPackets', Number($event.target.value))">
        <option
          v-for="opt in [50, 200, 500, 1000, 2000]"
          :key="opt"
          :value="opt">
          {{ $t('common.packetCount', opt) }}
        </option>
      </select>
      <v-tooltip
        v-if="params.gzip || params.image"
        activator="parent">
        {{ $t('sessions.packetOptions.noPacketSelector') }}
      </v-tooltip>
    </span> <!-- /# packets -->

    <!-- packet display type -->
    <select
      :value="params.base"
      class="form-control form-control-sm me-1"
      @change="$emit('updateBase', $event.target.value)">
      <option value="natural">
        {{ $t('sessions.packetOptions.natural') }}
      </option>
      <option value="ascii">
        {{ $t('sessions.packetOptions.ascii') }}
      </option>
      <option value="utf8">
        {{ $t('sessions.packetOptions.utf8') }}
      </option>
      <option value="hex">
        {{ $t('sessions.packetOptions.hex') }}
      </option>
    </select> <!-- /packet display type -->

    <!-- toggle options menu -->
    <v-menu location="bottom start">
      <template #activator="{ props: activatorProps }">
        <button
          v-bind="activatorProps"
          type="button"
          class="btn btn-sm btn-secondary me-1">
          {{ $t('sessions.packetOptions.packetOptions') }}
          <v-icon end>
            fa-caret-down
          </v-icon>
        </button>
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
    <div class="btn-group me-1">
      <button
        type="button"
        @click="$emit('toggleShowSrc')"
        :class="{'active':params.showSrc}"
        class="btn btn-sm btn-secondary btn-checkbox">
        {{ $t('common.src') }}
        <v-tooltip
          activator="parent"
          location="bottom">
          {{ $t('sessions.packetOptions.srcVisTip') }}
        </v-tooltip>
      </button>
      <button
        type="button"
        @click="$emit('toggleShowDst')"
        :class="{'active':params.showDst}"
        class="btn btn-sm btn-secondary btn-checkbox">
        {{ $t('common.dst') }}
        <v-tooltip
          activator="parent"
          location="bottom">
          {{ $t('sessions.packetOptions.dstVisTip') }}
        </v-tooltip>
      </button>
    </div> <!-- /src/dst packets -->

    <!-- decodings -->
    <div
      class="btn-group"
      v-if="decodingsClone">
      <button
        v-for="(value, key) in decodingsClone"
        :key="key"
        type="button"
        @click="toggleDecoding(key)"
        :disabled="params.showFrames"
        :class="{'active':decodingsClone[key].active}"
        class="btn btn-sm btn-secondary btn-checkbox">
        {{ value.name }}
        <v-tooltip
          activator="parent"
          location="bottom">
          {{ $t('sessions.packetOptions.toggleDecodingTip', value.name) }}
        </v-tooltip>
      </button>
    </div> <!-- /decodings -->

    <!-- decoding form -->
    <div
      class="text-start well well-sm mt-2 pt-2 d-flex flex-row gap-1 flex-wrap w-100"
      v-if="decodingForm">
      <template
        v-for="field in decodingsClone[decodingForm].fields"
        :key="field.name">
        <div v-if="!field.disabled">
          <div class="input-group input-group-sm">
            <span class="input-group-text">
              {{ field.name }}
            </span>
            <input
              :type="field.type"
              class="form-control"
              v-model="field.value"
              :placeholder="field.name">
          </div>
        </div>
      </template>
      <div class="btn-group btn-group-sm">
        <button
          type="button"
          :aria-label="$t('common.cancel')"
          class="btn btn-warning"
          @click="closeDecodingForm(false)">
          <span class="fa fa-ban" />
          <v-tooltip activator="parent">
            {{ $t('common.cancel') }}
          </v-tooltip>
        </button>
        <button
          type="button"
          :aria-label="$t('common.apply')"
          class="btn btn-theme-primary"
          @click="applyDecoding(decodingForm)">
          <span class="fa fa-check" />
          <v-tooltip activator="parent">
            {{ $t('common.apply') }}
          </v-tooltip>
        </button>
      </div>
      <div class="help-block ms-2">
        <span class="fa fa-info-circle" />&nbsp;
        {{ decodingsClone[decodingForm].title }}
      </div>
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
