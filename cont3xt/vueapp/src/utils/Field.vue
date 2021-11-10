<template>
  <span class="field">
    <a @click="toggleDropdown">
      {{ display || value }}
      <span class="fa fa-caret-down" />
    </a>
    <!-- clickable field menu -->
    <div v-if="isOpen"
      class="field-dropdown"
      :class="{'pull-right':!pullLeft,'pull-left':pullLeft}">
      <b-dropdown-item
        v-if="options.copy"
        @click="doCopy(value)">
        {{ options.copy }}
      </b-dropdown-item>
      <b-dropdown-item
        target="_blank"
        v-if="options.pivot"
        :href="`?q=${value}`">
        {{ options.pivot }}
      </b-dropdown-item>
    </div>
  </span>
</template>

<script>
export default {
  name: 'Cont3xtField',
  props: {
    value: { // the value to be used in copy and display is no display value
      type: String,
      required: true
    },
    display: { // the value to display (uses value is this is mssing)
      type: String
    },
    pullLeft: { // whether the dropdown should drop down from the left
      type: Boolean,
      default: false
    },
    options: { // which options to display in the dropdown
      type: Object,
      default: () => { return { copy: 'copy', pivot: 'pivot' }; }
    }
  },
  data () {
    return {
      isOpen: false
    };
  },
  methods: {
    /** Toggles the dropdown menu options for a field */
    toggleDropdown () {
      this.isOpen = !this.isOpen;
    },
    /**
     * Triggered when a the Copy menu item is clicked for a field
     * Copies the value provided to the user's clipboard and closes the menu
     * @param {string} value The field value
     */
    doCopy (value) {
      this.$copyText(value);
      this.isOpen = false;
    }
  }
};
</script>

<style>
.field {
  position: relative;
  cursor: pointer;
  z-index: 1;
  display: inline-block;
  padding: 0 1px;
  margin: 0 6px;
  border-radius: 3px;
  border: 1px solid transparent;
  max-width: 98%;
  line-height: 1.3;
}

/* .field a {
  color: var(--primary) !important;
  text-decoration: none;
} */

.field:hover {
  z-index: 4;
  background-color: var(--color-light);
}

.field:hover a {
  color: var(--primary) !important;
}

.field:hover ul.field-dropdown {
  opacity: 1;
  visibility: visible;
}

/* custom field dropdown styles because we can't use the dropdown-menu
 * class as it is specific to bootstraps dropdown implementation
 * this class is the same as dropdown-menu, but LESS whitespace */
.field-dropdown {
  opacity: 0;
  font-size: 12px;
  position: absolute;
  visibility: hidden;
  max-width: 700px;
  min-width: 80px;
  max-height: 300px;
  overflow-y: auto;
  position: absolute;
  z-index: 1000;
  display: block;
  padding: 5px 0;
  text-align: left;
  list-style: none;
  border-radius: 4px;
  background-color: var(--color-light);
  border: 1px solid var(--color-gray);
  margin-top: 0;
  margin-left: -2px;

          background-clip: padding-box;
  -webkit-background-clip: padding-box;

          box-shadow: 0 6px 12px -3px #333;
  -webkit-box-shadow: 0 6px 12px -3px #333;
}

.field:hover .field-dropdown {
  opacity: 1;
  visibility: visible;
}

.field-dropdown.pull-right {
  right: 0;
  left: auto;
}
.field-dropdown.pull-left {
  left: 0;
  right: auto;
}

.field-dropdown a.dropdown-item {
  overflow: hidden;
  text-overflow: ellipsis;
  display: block;
  padding: 2px 8px;
  clear: both;
  font-weight: normal;
  line-height: 1.42857143;
  white-space: nowrap;
  color: var(--color-dark) !important;
}

.field-dropdown a.dropdown-item:hover {
  text-decoration: none;
  color: var(--primary) !important;
  background-color: var(--color-gray-light);
}
</style>
