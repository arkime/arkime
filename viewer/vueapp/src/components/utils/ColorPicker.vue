<template>

  <div class="color-picker-input"
    ref="colorpicker"
    @keyup.esc="hidePicker">
    <div class="input-group input-group-sm">
      <span class="input-group-prepend">
        <span class="input-group-text">
          {{ fieldName }}
        </span>
      </span>
      <div class="input-group-append cursor-pointer"
        @click="togglePicker">
        <span class="input-group-text"
          :style="{'background-color':colorValue}">
          &nbsp;&nbsp;
          <span v-if="displayPicker"
            class="fa fa-check">
          </span>
          &nbsp;&nbsp;
        </span>
      </div>
    </div>
    <chrome-picker v-model="colorValue"
      v-if="displayPicker"
      class="color-picker"
      @input="changeColor">
    </chrome-picker>
  </div>

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
      required: true
    },
    colorName: {
      type: String,
      required: true
    },
    fieldName: {
      type: String,
      default: 'Color'
    }
  },
  watch: {
    color: function (newVal, oldVal) {
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
          name: this.colorName,
          value: this.colorValue
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
      let el = this.$refs.colorpicker;
      let target = e.target;
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
.color-picker {
  position: absolute;
  left: 15px;
  z-index: 3;
}

.color-picker-input .input-group-prepend > .input-group-text {
  color: #333333 !important;
  background-color: #F1F1F1 !important;
}
</style>
