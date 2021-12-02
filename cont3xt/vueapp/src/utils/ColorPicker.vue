<template>
  <span
    ref="colorpicker"
    @keyup.esc="hidePicker"
    class="color-picker-input">
    <div class="input-group-append cursor-pointer color"
      @click="togglePicker">
      <span
        style="width: 60px;"
        class="input-group-text"
        :style="{'background-color':colorValue}">
        &nbsp;&nbsp;
        <span v-if="displayPicker"
          class="fa fa-check">
        </span>
        &nbsp;&nbsp;
      </span>
    </div>
    <chrome-picker v-model="colorValue"
      v-if="displayPicker"
      class="color-picker"
      @input="changeColor">
    </chrome-picker>
  </span>
</template>

<script>
import VueColor from 'vue-color';

export default {
  name: 'Settings',
  components: {
    'chrome-picker': VueColor.Chrome
  },
  props: {
    color: {
      type: String,
      default: '#00AAC5'
    },
    linkName: {
      type: String,
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
    },
    hidePicker: function () {
      this.displayPicker = false;
      document.removeEventListener('click', this.documentClick);
      if (this.colorValue !== this.color) {
        this.$emit('colorSelected', {
          linkName: this.linkName,
          color: this.colorValue
        });
      }
    },
    showPicker: function () {
      this.displayPicker = true;
      document.addEventListener('click', this.documentClick);
    },
    togglePicker: function () {
      this.displayPicker ? this.hidePicker() : this.showPicker();
    },
    documentClick: function (e) {
      const el = this.$refs.colorpicker;
      const target = e.target;
      if (el !== target && !el.contains(target)) {
        this.hidePicker();
      }
    }
  },
  beforeDestroy: function () {
    document.removeEventListener('click', this.documentClick);
  }
};
</script>

<style scoped>
.color {
  height: 31px;
}

.color-picker {
  right: 0;
  z-index: 3;
  position: absolute;
}

.color-picker-input .input-group-prepend > .input-group-text {
  color: #333333 !important;
  background-color: #F1F1F1 !important;
}
</style>
