<!--
Copyright Yahoo Inc.
SPDX-License-Identifier: Apache-2.0
-->
<template>
  <BRow gutter-x="1" class="text-start" align-h="start">
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
            { value: 50, text: '50 packets' },
            { value: 200, text: '200 packets' },
            { value: 500, text: '500 packets' },
            { value: 1000, text: '1,000 packets' },
            { value: 2000, text: '2,000 packets' }
          ]"
          @update:model-value="$emit('updateNumPackets', $event)"
        />
        <BTooltip :target="getTarget('numPackets')" v-if="numPacketsInfo">{{ numPacketsInfo }}</BTooltip>
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
          { value: 'natural', text: 'natural' },
          { value: 'ascii', text: 'ascii' },
          { value: 'utf8', text: 'utf8' },
          { value: 'hex', text: 'hex' }
        ]"
        @update:model-value="$emit('updateBase', $event)"
      /> <!-- /packet display type -->
    </BCol>
    <BCol cols="auto">
      <!-- toggle options -->
      <b-dropdown
        size="sm"
        class="me-1"
        variant="checkbox"
        text="Packet Options"
        title="Packet Options">
        <b-dropdown-item
          @click="$emit('toggleShowFrames')"
          :title="params.showFrames ? 'Show Reassembled Packets' : 'Show Raw Packets'">
          {{ params.showFrames ? 'Show Reassembled Packets' : 'Show Raw Packets' }}
        </b-dropdown-item>
        <b-dropdown-item
          @click="$emit('toggleTimestamps')"
          :title="params.ts ? 'Hide Packet Info' : 'Show Packet Info'">
          {{ params.ts ? 'Hide' : 'Show' }}
          Packet Info
        </b-dropdown-item>
        <b-dropdown-item
          v-if="params.base === 'hex'"
          @click="$emit('toggleLineNumbers')"
          :title="params.line ? 'Hide Line Numbers' : 'Show Line Numbers'">
          {{ params.line ? 'Hide' : 'Show'}}
          Line Numbers
        </b-dropdown-item>
        <b-dropdown-item
          ref="toggleCompression"
          v-if="!params.showFrames"
          @click="$emit('toggleCompression')">
          {{ params.gzip ? 'Disable Uncompressing' : 'Enable Uncompressing' }}
          <BTooltip :target="getTarget('toggleCompression')">{{ params.gzip ? 'Disable Uncompressing' : 'Enable Uncompressing (Note: all packets will be requested)' }}</BTooltip>
        </b-dropdown-item>
        <b-dropdown-item
          ref="toggleImages"
          v-if="!params.showFrames"
          @click="$emit('toggleImages')">
          {{ params.image ? 'Hide' : 'Show'}}
          Images &amp; Files
          <BTooltip :target="getTarget('toggleImages')">{{ params.image ? 'Hide Images & Files' : 'Show Images & Files (Note: all packets will be requested)' }}</BTooltip>
        </b-dropdown-item>
        <b-dropdown-divider></b-dropdown-divider>
        <b-dropdown-item
          target="_blank"
          :href="cyberChefSrcUrl">
          Open <strong>src</strong> packets with CyberChef
        </b-dropdown-item>
        <b-dropdown-item
          target="_blank"
          :href="cyberChefDstUrl">
          Open <strong>dst</strong> packets with CyberChef
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
          Src
          <BTooltip :target="getTarget('toggleSrc')">Toggle source packet visibility</BTooltip>
        </button>
        <button
          ref="toggleDst"
          type="button"
          @click="$emit('toggleShowDst')"
          :class="{'active':params.showDst}"
          class="btn btn-secondary btn-checkbox btn-sm">
          Dst
          <BTooltip :target="getTarget('toggleDst')">Toggle destination packet visibility</BTooltip>
        </button>
      </div> <!-- /src/dst packets -->
    </BCol>
    <BCol cols="auto">
      <!-- decodings -->
      <div class="btn-group">
        <button
          v-for="(value, key) in decodingsClone"
          :ref="`decodings${key}`"
          :key="key"
          type="button"
          @click="toggleDecoding(key)"
          :disabled="params.showFrames"
          :class="{'active':decodingsClone[key].active}"
          class="btn btn-secondary btn-checkbox btn-sm">
          {{ value.name }}
          <BTooltip :target="getTarget(`decodings${key}`)">Toggle {{ value.name}} Decoding</BTooltip>
        </button>
      </div> <!-- /decodings -->
    </BCol>
    <!-- decoding form -->
    <BRow gutter-x="1" class="text-start well well-sm mt-2 pt-2" align-h="start" v-if="decodingForm">
      <template v-for="field in decodingsClone[decodingForm].fields"
        :key="field.name">
        <BCol cols="auto"
          v-if="!field.disabled">
          <div class="input-group input-group-sm">
            <span class="input-group-text">
              {{ field.name }}
            </span>
            <input
              type="field.type"
              class="form-control"
              v-model="field.value"
              :placeholder="field.name"
            />
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
            <span class="fa fa-ban"></span>
            <BTooltip :target="getTarget('cancelDecoding')">Cancel</BTooltip>
          </button>
          <button
            ref="applyDecoding"
            type="button"
            class="btn btn-theme-primary"
            @click="applyDecoding(decodingForm)">
            <span class="fa fa-check"></span>
            <BTooltip :target="getTarget('applyDecoding')">Apply</BTooltip>
          </button>
        </div>
      </BCol>
      <div class="help-block ms-2">
        <span class="fa fa-info-circle">
        </span>&nbsp;
        {{ decodingsClone[decodingForm].title }}
      </div>
    </BRow> <!-- /decoding form -->
  </BRow>
</template>

<script>
export default {
  name: 'PacketOptions',
  props: {
    params: Object,
    decodings: Object,
    cyberChefSrcUrl: String,
    cyberChefDstUrl: String
  },
  data () {
    return {
      decodingForm: false,
      decodingsClone: JSON.parse(JSON.stringify(this.decodings))
    };
  },
  computed: {
    numPacketsInfo () {
      let toggle;

      if (this.params.gzip && this.params.image) {
        toggle = 'uncompress and images & files';
      } else if (this.params.gzip) {
        toggle = 'uncompress';
      } else if (this.params.image) {
        toggle = 'images and files';
      } else {
        return '';
      }

      return `
        Displaying all packets for this session.
        You cannot select the number of packets because ${toggle} might need them all.
        To select the number of packets returned, disable ${toggle} from the Packet Options menu.
      `;
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
