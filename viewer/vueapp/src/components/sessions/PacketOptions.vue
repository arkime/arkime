<!--
Copyright Yahoo Inc.
SPDX-License-Identifier: Apache-2.0
-->
<template>
  <span>
    <div class="form-group">
      <!-- # packets -->
      <span v-b-tooltip.hover
        :title="numPacketsInfo">
        <b-form-select
          size="sm"
          role="listbox"
          :value="params.packets"
          :disabled="params.gzip || params.image"
          class="mr-1 form-control"
          :class="{'disabled':params.gzip}"
          :options="[
            { value: 50, text: '50 packets' },
            { value: 200, text: '200 packets' },
            { value: 500, text: '500 packets' },
            { value: 1000, text: '1,000 packets' },
            { value: 2000, text: '2,000 packets' }
          ]"
          @change="$emit('updateNumPackets', $event)"
        />
      </span> <!-- /# packets -->
      <!-- packet display type -->
      <b-form-select
        size="sm"
        role="listbox"
        :value="params.base"
        class="mr-1 form-control"
        :options="[
          { value: 'natural', text: 'natural' },
          { value: 'ascii', text: 'ascii' },
          { value: 'utf8', text: 'utf8' },
          { value: 'hex', text: 'hex' }
        ]"
        @change="$emit('updateBase', $event)"
      /> <!-- /packet display type -->
      <!-- toggle options -->
      <b-dropdown
        size="sm"
        class="mr-1"
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
          v-if="!params.showFrames"
          @click="$emit('toggleCompression')"
          v-b-tooltip.hover.right="{ disabled: params.gzip }"
          :title="params.gzip ? 'Disable Uncompressing' : 'Enable Uncompressing (Note: all packets will be requested)'">
          {{ params.gzip ? 'Disable Uncompressing' : 'Enable Uncompressing' }}
        </b-dropdown-item>
        <b-dropdown-item
          v-if="!params.showFrames"
          @click="$emit('toggleImages')"
          v-b-tooltip.hover.right="{ disabled: params.image }"
          :title="params.image ? 'Hide Images & Files' : 'Show Images & Files (Note: all packets will be requested)'">
          {{ params.image ? 'Hide' : 'Show'}}
          Images &amp; Files
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
      <!-- src/dst packets -->
      <div class="btn-group mr-1">
        <button
          v-b-tooltip
          type="button"
          @click="$emit('toggleShowSrc')"
          :class="{'active':params.showSrc}"
          title="Toggle source packet visibility"
          class="btn btn-sm btn-secondary btn-checkbox btn-sm">
          Src
        </button>
        <button
          v-b-tooltip
          type="button"
          @click="$emit('toggleShowDst')"
          :class="{'active':params.showDst}"
          title="Toggle destination packet visibility"
          class="btn btn-secondary btn-checkbox btn-sm">
          Dst
        </button>
      </div> <!-- /src/dst packets -->
      <!-- decodings -->
      <div class="btn-group">
        <button
          v-for="(value, key) in decodingsClone"
          :key="key"
          type="button"
          v-b-tooltip.hover
          @click="toggleDecoding(key)"
          :disabled="params.showFrames"
          :title="`Toggle ${value.name} Decoding`"
          :class="{'active':decodingsClone[key].active}"
          class="btn btn-secondary btn-checkbox btn-sm">
          {{ value.name }}
        </button>
      </div> <!-- /decodings -->
    </div>
    <!-- decoding form -->
    <div v-if="decodingForm">
      <form class="form-inline well well-sm mt-1">
        <span v-for="field in decodingsClone[decodingForm].fields"
          :key="field.name">
          <div class="form-group mr-1 mt-1"
            v-if="!field.disabled">
            <div class="input-group input-group-sm">
              <span class="input-group-prepend">
                <span class="input-group-text">
                  {{ field.name }}
                </span>
              </span>
              <input
                type="field.type"
                class="form-control"
                v-model="field.value"
                :placeholder="field.name"
              />
            </div>
          </div>
        </span>
        <div class="btn-group btn-group-sm pull-right mt-1">
          <button
            type="button"
            title="cancel"
            v-b-tooltip.hover
            class="btn btn-warning"
            @click="closeDecodingForm(false)">
            <span class="fa fa-ban">
            </span>
          </button>
          <button
            type="button"
            title="apply"
            v-b-tooltip.hover
            class="btn btn-theme-primary"
            @click="applyDecoding(decodingForm)">
            <span class="fa fa-check">
            </span>
          </button>
        </div>
      </form>
      <div class="help-block ml-2">
        <span class="fa fa-info-circle">
        </span>&nbsp;
        {{ decodingsClone[decodingForm].title }}
      </div>
    </div> <!-- /decoding form -->
  </span>
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
    decodings (newVal) {
      this.decodingsClone = JSON.parse(JSON.stringify(newVal));
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
            this.$set(paramsClone[key], field.key, field.value);
          }
        }
      } else {
        this.$delete(paramsClone, key);
      }

      this.$emit('applyDecodings', paramsClone);
      this.closeDecodingForm();

      // update local storage
      localStorage['moloch-decodings'] = JSON.stringify(paramsClone);
    }
  }
};
</script>
