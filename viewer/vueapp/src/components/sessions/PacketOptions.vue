<template>
  <span>
    <div class="form-group">
      <!-- # packets -->
      <b-form-select
        size="sm"
        class="mr-1"
        :value="params.packets"
        :options="[
          { value: 50, text: '50 packets' },
          { value: 200, text: '200 packets' },
          { value: 500, text: '500 packets' },
          { value: 1000, text: '1,000 packets' },
          { value: 2000, text: '2,000 packets' }
        ]"
        @change="$emit('updateNumPackets', $event)"
      /> <!-- /# packets -->
      <!-- packet display type -->
      <b-form-select
        size="sm"
        class="mr-1"
        :value="params.base"
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
        text="Packet Options">
        <b-dropdown-item
          @click="$emit('toggleShowFrames')">
          {{ params.showFrames ? 'Show Reassembled Packets' : 'Show Raw Packets' }}
        </b-dropdown-item>
        <b-dropdown-item
          @click="$emit('toggleTimestamps')">
          {{ params.ts ? 'Hide' : 'Show' }}
          Packet Info
        </b-dropdown-item>
        <b-dropdown-item
          v-if="params.base === 'hex'"
          @click="$emit('toggleLineNumbers')">
          {{ params.line ? 'Hide' : 'Show'}}
          Line Numbers
        </b-dropdown-item>
        <b-dropdown-item
          v-if="!params.showFrames"
          @click="$emit('toggleCompression')">
          {{ params.gzip ? 'Disable Uncompressing' : 'Enable Uncompressing' }}
        </b-dropdown-item>
        <b-dropdown-item
          v-if="!params.showFrames"
          @click="$emit('toggleImages')">
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
        <button type="button"
          class="btn btn-sm btn-secondary btn-checkbox btn-sm"
          :class="{'active':params.showSrc}"
          @click="$emit('toggleShowSrc')"
          v-b-tooltip
          title="Toggle source packet visibility">
          Src
        </button>
        <button type="button"
          class="btn btn-secondary btn-checkbox btn-sm"
          :class="{'active':params.showDst}"
          @click="$emit('toggleShowDst')"
          v-b-tooltip
          title="Toggle destination packet visibility">
          Dst
        </button>
      </div> <!-- /src/dst packets -->
      <!-- decodings -->
      <div class="btn-group">
        <button
          v-for="(value, key) in decodings"
          :key="key"
          type="button"
          v-b-tooltip.hover
          @click="toggleDecoding(key)"
          :disabled="params.showFrames"
          :title="`Toggle ${value.name} Decoding`"
          :class="{'active':activeDecodings[key]}"
          class="btn btn-secondary btn-checkbox btn-sm">
          {{ value.name }}
        </button>
      </div> <!-- /decodings -->
    </div>
    <!-- decoding form -->
    <div v-if="decodingForm">
      <form class="form-inline well well-sm mt-1">
        <span v-for="field in decodings[decodingForm].fields"
          :key="field.name">
          <div class="form-group mr-1 mt-1"
            v-if="!field.disabled">
            <div class="input-group input-group-sm">
              <span class="input-group-prepend">
                <span class="input-group-text">
                  {{ field.name }}
                </span>
              </span>
              <input v-model="field.value"
                class="form-control"
                type="field.type"
              />
            </div>
          </div>
        </span>
        <div class="btn-group btn-group-sm pull-right mt-1">
          <button type="button"
            class="btn btn-warning"
            title="cancel"
            v-b-tooltip.hover
            @click="closeDecodingForm(false)">
            <span class="fa fa-ban">
            </span>
          </button>
          <button type="button"
            class="btn btn-theme-primary"
            title="apply"
            v-b-tooltip.hover
            @click="applyDecoding(decodingForm)">
            <span class="fa fa-check">
            </span>
          </button>
        </div>
      </form>
      <div class="help-block">
        <span class="fa fa-info-circle">
        </span>&nbsp;
        {{ decodings[decodingForm].title }}
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
      activeDecodings: {}
    };
  },
  methods: {
    /**
     * Toggles a decoding on or off
     * If a decoding needs more input, shows form
     * @param {string} key Identifier of the decoding to toggle
     */
    toggleDecoding (key) {
      const decoding = this.decodings[key];

      const isActive = !this.activeDecodings[key];
      this.$set(this.activeDecodings, key, isActive);

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
        this.$set(this.activeDecodings, this.decodingForm, active);
      }

      this.decodingForm = false;
    },
    /**
     * Sets the decode param, issues query, and closes form if necessary
     * @param {key} key Identifier of the decoding to apply
     */
    applyDecoding (key) {
      const decodeClone = JSON.parse(JSON.stringify(this.params.decode));
      decodeClone[key] = {};

      const decoding = this.decodings[key];

      if (this.activeDecodings[key]) {
        if (decoding.fields) {
          for (const field of decoding.fields) {
            this.$set(decodeClone[key], field.key, field.value);
          }
        }
      } else {
        this.$delete(decodeClone, key);
      }

      this.$emit('updateDecodings', decodeClone);
      this.closeDecodingForm(this.activeDecodings[key]);

      // update local storage
      localStorage['moloch-decodings'] = JSON.stringify(decodeClone);
    }
  }
};
</script>
