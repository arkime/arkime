<!--
Copyright Yahoo Inc.
SPDX-License-Identifier: Apache-2.0
-->
<template>
  <span class="field">
    <a style="color: revert !important;">
      <v-menu activator="parent" location="bottom end">
        <v-sheet class="d-flex flex-column mw-fit-content" data-testid="field-dropdown">
          <template v-for="option in options">
            <v-btn
              :class="[btnPullClass]"
              size="small"
              variant="text"
              target="_blank"
              :key="option.name"
              v-if="option.href"
              :href="formatUrl(option)">
              {{ option.name }}
            </v-btn>
          </template>
          <v-btn
            :class="[btnPullClass]"
            size="small"
            variant="text"
            key="copy"
            v-if="options.copy"
            @click="doCopy(value)">
            {{ options.copy }}
          </v-btn>
          <v-btn
            :class="[btnPullClass]"
            size="small"
            variant="text"
            key="pivot"
            target="_blank"
            :href="pivotHref"
            v-if="options.pivot">
            {{ options.pivot }}
          </v-btn>
        </v-sheet>
      </v-menu>

      <template v-if="highlights">
        <highlightable-text :content="display || value" :highlights="highlights"/>
      </template>
      <template v-else>
        {{display || value}}
        <template v-if="decodedValue">
          <span class="text-muted">({{decodedValue}})</span>
        </template>
      </template>
      <v-icon icon="mdi-menu-down" />
    </a>
    <!-- clickable field menu -->
  </span>
</template>

<script>
import HighlightableText from '@/utils/HighlightableText.vue';
import { formatPostProcessedValue } from '@/utils/formatValue';
import { clipboardCopyText } from '@/utils/clipboardCopyText';

export default {
  name: 'Cont3xtField',
  components: {
    HighlightableText
  },
  props: {
    data: { // the parent data row for replacing values in urls
      // only necessary for pivot fields that have a url
      type: Object,
      default: () => { return {}; }
    },
    value: { // the value to be used in copy and display if no display value
      type: String,
      required: true
    },
    decodedValue: { // the decoded value to be displayed next to searched value
      type: String,
      required: false
    },
    display: { // the value to display (uses value is this is missing)
      type: String
    },
    pullLeft: { // whether the dropdown should drop down from the left
      type: Boolean,
      default: false
    },
    options: { // which options to display in the dropdown
      type: Object,
      default: () => { return { copy: 'copy', pivot: 'pivot' }; }
    },
    highlights: { // optional highlight span array for use in table highlighting
      type: Array,
      default () {
        return null;
      }
    }
  },
  computed: {
    btnPullClass () {
      return this.pullLeft ? 'justify-start' : 'justify-end';
    },
    pivotHref () {
      const params = new URLSearchParams(window.location.search);
      params.set('b', window.btoa(this.value));
      return `?${params.toString()}`;
    }
  },
  methods: {
    formatUrl (option) {
      const value = formatPostProcessedValue(this.data, option.field);
      return option.href.replace('%{value}', value);
    },
    /**
     * Triggered when a the Copy menu item is clicked for a field
     * Copies the value provided to the user's clipboard and closes the menu
     * @param {string} value The field value
     */
    doCopy (value) {
      clipboardCopyText(value);
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

.field:hover {
  background-color: rgb(var(--v-theme-light)) !important;
  color: rgb(var(--v-theme-secondary));
}

/* .field a {
  color: rgb(var(--v-theme-primary)) !important;
  text-decoration: none;
} */

.field:hover {
  z-index: 4;
  background-color: var(--color-light);
}

.field:hover a {
  color: rgb(var(--v-theme-primary)) !important;
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
  color: rgb(var(--v-theme-dark)) !important;
}

.field-dropdown a.dropdown-item:hover {
  text-decoration: none;
  color: rgb(var(--v-theme-primary)) !important;
  background-color: var(--color-gray-light);
}
</style>
