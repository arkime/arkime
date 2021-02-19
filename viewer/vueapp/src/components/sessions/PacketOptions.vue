<template>
  <span>
    <div class="form-group">
      <!-- # packets -->
      <b-form-select
        size="sm"
        class="mr-1"
        @change="getPackets"
        v-model="params.packets"
        :options="[
          { value: 50, text: '50 packets' },
          { value: 200, text: '200 packets' },
          { value: 500, text: '500 packets' },
          { value: 1000, text: '1,000 packets' },
          { value: 2000, text: '2,000 packets' }
        ]"
      /> <!-- /# packets -->
      <!-- packet display type -->
      <b-form-select
        size="sm"
        class="mr-1"
        @change="getPackets"
        v-model="params.base"
        :options="[
          { value: 'natural', text: 'natural' },
          { value: 'ascii', text: 'ascii' },
          { value: 'utf8', text: 'utf8' },
          { value: 'hex', text: 'hex' }
        ]"
      /> <!-- /packet display type -->
      <!-- src/dst packets -->
      <div class="btn-group mr-1">
        <button type="button"
          class="btn btn-sm btn-secondary btn-checkbox btn-sm"
          :class="{'active':params.showSrc}"
          @click="toggleShowSrc"
          v-b-tooltip
          title="Toggle source packet visibility">
          Src
        </button>
        <button type="button"
          class="btn btn-secondary btn-checkbox btn-sm"
          :class="{'active':params.showDst}"
          @click="toggleShowDst"
          v-b-tooltip
          title="Toggle destination packet visibility">
          Dst
        </button>
      </div> <!-- /src/dst packets -->
      <!-- toggle options -->
      <b-dropdown
        size="sm"
        class="mr-1"
        text="Packet Display Options">
        <b-dropdown-item
          @click="toggleShowFrames">
          {{ params.showFrames ? 'Show Reassambled Packets' : 'Show Packet Flow' }}
        </b-dropdown-item>
        <b-dropdown-item
          v-if="params.base === 'hex'"
          @click="toggleLineNumbers">
          {{ params.line ? 'Hide' : 'Show'}}
          Line Numbers
        </b-dropdown-item>
        <b-dropdown-item
          v-if="!params.showFrames"
          @click="toggleCompression">
          {{ params.gzip ? 'Disable Uncompressing' : 'Enable Uncompressing' }}
        </b-dropdown-item>
        <b-dropdown-item
          v-if="!params.showFrames"
          @click="toggleImages">
          {{ params.image ? 'Hide' : 'Show'}}
          Images &amp; Files
        </b-dropdown-item>
        <b-dropdown-item
          @click="toggleTimestamps">
          {{ params.ts ? 'Hide' : 'Show' }}
          Packet Info
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
      <!-- decodings -->
      <!-- TODO this isn't showing active -->
      <div class="btn-group">
        <button
          v-for="(value, key) in decodings"
          :key="key"
          type="button"
          v-b-tooltip.hover
          @click="toggleDecoding(key)"
          :disabled="params.showFrames"
          :title="value.name + 'Decoding'"
          :class="{'active':params.decode[key]}"
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
        <div class="btn-group btn-group-sm pull-right mt-1 mb-1">
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
      decodingForm: false
    };
  },
  methods: {
    getPackets () {
      this.$emit('getPackets');
    },
    toggleImages () {
      this.$emit('toggleImages');
    },
    toggleShowSrc () {
      this.$emit('toggleShowSrc');
    },
    toggleShowDst () {
      this.$emit('toggleShowDst');
    },
    toggleTimestamps () {
      this.$emit('toggleTimestamps');
    },
    toggleShowFrames () {
      this.$emit('toggleShowFrames');
    },
    toggleLineNumbers () {
      this.$emit('toggleLineNumbers');
    },
    toggleCompression () {
      this.$emit('toggleCompression');
    },
    /**
     * Toggles a decoding on or off
     * If a decoding needs more input, shows form
     * @param {string} key Identifier of the decoding to toggle
     */
    toggleDecoding: function (key) {
      const decoding = this.decodings[key];

      decoding.active = !decoding.active;

      if (decoding.fields && decoding.active) {
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
    closeDecodingForm: function (active) {
      if (this.decodingForm) {
        this.decodings[this.decodingForm].active = active;
      }

      this.decodingForm = false;
    },
    /**
     * Sets the decode param, issues query, and closes form if necessary
     * @param {key} key Identifier of the decoding to apply
     */
    applyDecoding: function (key) {
      this.params.decode[key] = {};
      const decoding = this.decodings[key];

      if (decoding.active) {
        if (decoding.fields) {
          for (let i = 0, len = decoding.fields.length; i < len; ++i) {
            let field = decoding.fields[i];
            this.params.decode[key][field.key] = field.value;
          }
        }
      } else {
        this.params.decode[key] = null;
        delete this.params.decode[key];
      }

      console.log('apply decoding', this.params.decode); // TODO ECR remove
      this.$emit('updateDecodings', this.params.decode);
      this.closeDecodingForm(decoding.active);

      // update local storage
      localStorage['moloch-decodings'] = JSON.stringify(this.params.decode);
    }
  }
};
</script>
