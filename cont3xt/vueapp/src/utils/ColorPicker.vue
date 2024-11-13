<!--
Copyright Yahoo Inc.
SPDX-License-Identifier: Apache-2.0
-->
<template>
  <v-btn
    class="skinny-search-row-btn color-picker-btn"
    :color="colorValue"
    :active="false"
    :ripple="false"
    selected-class=""
    data-testid="picker-btn"
  >
    <v-icon v-if="displayPicker" icon="mdi-check-bold" />

    <v-menu activator="parent" v-model="displayPicker" :close-on-content-click="false">
      <chrome-picker
        :model-value="colorValue"
        @update:model-value="changeColor"
        class="color-picker"
        data-testid="picker">
      </chrome-picker>
    </v-menu>
  </v-btn>
</template>

<script>
import { Chrome } from '@ckpack/vue-color';

export default {
  name: 'ColorPicker',
  components: {
    ChromePicker: Chrome
  },
  props: {
    color: {
      type: String,
      default: '#00AAC5'
    },
    linkName: {
      type: String,
      required: true
    },
    index: {
      type: Number,
      required: true
    }
  },
  watch: {
    color (newVal, oldVal) {
      this.setColor(this.color);
    }
  },
  data: function () {
    return {
      colorValue: '',
      displayPicker: false
    };
  },
  mounted: function () {
    this.setColor(this.color);
  },
  methods: {
    setColor: function (color) {
      this.colorValue = color;
    },
    changeColor: function (color) {
      this.setColor(color.hex);
    }
  }
};
</script>

<style>
/* do not lighten button when active, so that we can see actual color */
.color-picker-btn .v-btn__overlay {
  opacity: 0 !important;
}
</style>
