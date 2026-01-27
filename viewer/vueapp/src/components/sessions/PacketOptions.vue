<!--
Copyright Yahoo Inc.
SPDX-License-Identifier: Apache-2.0
-->
<template>
  <BRow
    gutter-x="1"
    class="text-start"
    align-h="start">
    <BCol cols="auto">
      <!-- # packets -->
      <span ref="numPackets">
        <b-form-select
          size="sm"
          role="listbox"
          :model-value="params.packets"
          :disabled="params.gzip || params.image"
          class="me-1 form-control"
          :class="{'disabled':params.gzip}"
          :options="[
            { value: 50, text: $t('common.packetCount', 50) },
            { value: 200, text: $t('common.packetCount', 200) },
            { value: 500, text: $t('common.packetCount', 500) },
            { value: 1000, text: $t('common.packetCount', 1000) },
            { value: 2000, text: $t('common.packetCount', 2000) },
          ]"
          @update:model-value="$emit('updateNumPackets', $event)" />
        <BTooltip
          :target="getTarget('numPackets')"
          v-if="params.gzip || params.image">{{ $t('sessions.packetOptions.noPacketSelector') }}</BTooltip>
      </span> <!-- /# packets -->
    </BCol>
    <BCol cols="auto">
      <!-- packet display type -->
      <b-form-select
        size="sm"
        role="listbox"
        :model-value="params.base"
        class="me-1 form-control"
        :options="[
          { value: 'natural', text: $t('sessions.packetOptions.natural') },
          { value: 'ascii', text: $t('sessions.packetOptions.ascii') },
          { value: 'utf8', text: $t('sessions.packetOptions.utf8') },
          { value: 'hex', text: $t('sessions.packetOptions.hex') }
        ]"
        @update:model-value="$emit('updateBase', $event)" /> <!-- /packet display type -->
    </BCol>
    <BCol cols="auto">
      <!-- toggle options -->
      <b-dropdown
        size="sm"
        class="me-1"
        variant="checkbox"
        :text="$t('sessions.packetOptions.packetOptions')">
        <b-dropdown-item
          @click="$emit('toggleShowFrames')">
          {{ $t(params.showFrames ? 'sessions.packetOptions.showReassembled' : 'sessions.packetOptions.showRaw') }}
        </b-dropdown-item>
        <b-dropdown-item
          @click="$emit('toggleTimestamps')">
          {{ $t(params.ts ? 'sessions.packetOptions.hideInfo' : 'sessions.packetOptions.showInfo') }}
        </b-dropdown-item>
        <b-dropdown-item
          ref="toggleLineNumbers"
          v-if="params.base === 'hex'"
          @click="$emit('toggleLineNumbers')">
          {{ $t(params.line ? 'sessions.packetOptions.hideLineNumbers' : 'sessions.packetOptions.showLineNumbers') }}
        </b-dropdown-item>
        <b-dropdown-item
          ref="toggleCompression"
          v-if="!params.showFrames"
          @click="$emit('toggleCompression')">
          {{ $t(params.gzip ? 'sessions.packetOptions.disableUncompressing' : 'sessions.packetOptions.enableUncompressing') }}
          <BTooltip
            :target="getTarget('toggleCompression')"
            noninteractive
            placement="right">
            {{ $t(params.gzip ? 'sessions.packetOptions.disableUncompressing' : 'sessions.packetOptions.enableUncompressingTip') }}
          </BTooltip>
        </b-dropdown-item>
        <b-dropdown-item
          ref="toggleImages"
          v-if="!params.showFrames"
          @click="$emit('toggleImages')">
          {{ $t(params.image ? 'sessions.packetOptions.hideFiles' : 'sessions.packetOptions.showFiles') }}
          <BTooltip
            :target="getTarget('toggleImages')"
            noninteractive
            placement="right">
            {{ $t(params.image ? 'sessions.packetOptions.hideFiles' : 'sessions.packetOptions.showFilesTip') }}
          </BTooltip>
        </b-dropdown-item>
        <b-dropdown-divider />
        <b-dropdown-item
          target="_blank"
          :href="cyberChefSrcUrl">
          {{ $t('sessions.packetOptions.openCyberChefSrc') }}
        </b-dropdown-item>
        <b-dropdown-item
          target="_blank"
          :href="cyberChefDstUrl">
          {{ $t('sessions.packetOptions.openCyberChefDst') }}
        </b-dropdown-item>
      </b-dropdown> <!-- /toggle options -->
    </BCol>
    <BCol cols="auto">
      <!-- src/dst packets -->
      <div class="btn-group me-1">
        <button
          ref="toggleSrc"
          type="button"
          @click="$emit('toggleShowSrc')"
          :class="{'active':params.showSrc}"
          class="btn btn-sm btn-secondary btn-checkbox btn-sm">
          {{ $t('common.src') }}
          <BTooltip
            :target="getTarget('toggleSrc')"
            noninteractive
            placement="bottom">
            {{ $t('sessions.packetOptions.srcVisTip') }}
          </BTooltip>
        </button>
        <button
          ref="toggleDst"
          type="button"
          @click="$emit('toggleShowDst')"
          :class="{'active':params.showDst}"
          class="btn btn-secondary btn-checkbox btn-sm">
          {{ $t('common.dst') }}
          <BTooltip
            :target="getTarget('toggleDst')"
            noninteractive
            placement="bottom">
            {{ $t('sessions.packetOptions.dstVisTip') }}
          </BTooltip>
        </button>
      </div> <!-- /src/dst packets -->
    </BCol>
    <BCol cols="auto">
      <!-- decodings -->
      <div
        class="btn-group"
        v-if="decodingsClone">
        <button
          v-for="(value, key) in decodingsClone"
          :ref="`decodings${key}`"
          :id="`decodings${key}`"
          :key="key"
          type="button"
          @click="toggleDecoding(key)"
          :disabled="params.showFrames"
          :class="{'active':decodingsClone[key].active}"
          class="btn btn-secondary btn-checkbox btn-sm">
          {{ value.name }}
          <BTooltip
            :target="`decodings${key}`"
            noninteractive
            placement="bottom">
            {{ $t('sessions.packetOptions.toggleDecodingTip', value.name) }}
          </BTooltip>
        </button>
      </div> <!-- /decodings -->
    </BCol>
    <!-- decoding form -->
    <BRow
      gutter-x="1"
      class="text-start well well-sm mt-2 pt-2"
      align-h="start"
      v-if="decodingForm">
      <template
        v-for="field in decodingsClone[decodingForm].fields"
        :key="field.name">
        <BCol
          cols="auto"
          v-if="!field.disabled">
          <div class="input-group input-group-sm">
            <span class="input-group-text">
              {{ field.name }}
            </span>
            <input
              type="field.type"
              class="form-control"
              v-model="field.value"
              :placeholder="field.name">
          </div>
        </BCol>
      </template>
      <BCol cols="auto">
        <div class="btn-group btn-group-sm pull-right">
          <button
            ref="cancelDecoding"
            type="button"
            class="btn btn-warning"
            @click="closeDecodingForm(false)">
            <span class="fa fa-ban" />
            <BTooltip :target="getTarget('cancelDecoding')">
              {{ $t('common.cancel') }}
            </BTooltip>
          </button>
          <button
            ref="applyDecoding"
            type="button"
            class="btn btn-theme-primary"
            @click="applyDecoding(decodingForm)">
            <span class="fa fa-check" />
            <BTooltip :target="getTarget('applyDecoding')">
              {{ $t('common.apply') }}
            </BTooltip>
          </button>
        </div>
      </BCol>
      <div class="help-block ms-2">
        <span class="fa fa-info-circle" />&nbsp;
        {{ decodingsClone[decodingForm].title }}
      </div>
    </BRow> <!-- /decoding form -->
  </BRow>
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
    getTarget (ref) {
      return this.$refs[ref];
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
