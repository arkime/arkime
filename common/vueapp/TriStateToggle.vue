<!--
Copyright Yahoo Inc.
SPDX-License-Identifier: Apache-2.0
-->
<template>
  <div class="tri-state-toggle">
    <div class="tri-state-label mb-1">
      {{ label }}
    </div>
    <v-btn-toggle
      mandatory
      density="compact"
      divided
      variant="outlined"
      color="secondary"
      :model-value="internalValue"
      @update:model-value="setValue">
      <v-btn
        size="small"
        value="inherit">
        {{ $t('users.inherit') }}
        <v-tooltip
          activator="parent"
          location="bottom">
          {{ $t('users.inheritTip') }}
        </v-tooltip>
      </v-btn>
      <v-btn
        size="small"
        value="no">
        {{ $t('users.no') }}
        <v-tooltip
          activator="parent"
          location="bottom">
          {{ $t('users.forceOffTip') }}
        </v-tooltip>
      </v-btn>
      <v-btn
        size="small"
        value="yes">
        {{ $t('users.yes') }}
        <v-tooltip
          activator="parent"
          location="bottom">
          {{ $t('users.forceOnTip') }}
        </v-tooltip>
      </v-btn>
    </v-btn-toggle>
  </div>
</template>

<script>
/**
 * Three-state toggle for user and role permissions.
 * States: Inherit (undefined), No (false), Yes (true)
 *
 * For negated fields (like emailSearch where false = disabled),
 * set negated=true to invert the display logic.
 *
 * Example: emailSearch field with "Disable Email Search" label
 * - emailSearch = undefined → displays "Inherit"
 * - emailSearch = true (enabled) → displays "No" (not disabled)
 * - emailSearch = false (disabled) → displays "Yes" (disabled)
 */
export default {
  name: 'TriStateToggle',
  props: {
    modelValue: {
      type: Boolean,
      default: undefined
    },
    label: {
      type: String,
      required: true
    },
    negated: {
      type: Boolean,
      default: false
    }
  },
  emits: ['update:modelValue'],
  computed: {
    // Convert external value (undefined/true/false) to internal string ('inherit'/'yes'/'no')
    internalValue () {
      if (this.modelValue === undefined || this.modelValue === null) {
        return 'inherit';
      }
      const boolVal = this.negated ? !this.modelValue : this.modelValue;
      return boolVal ? 'yes' : 'no';
    }
  },
  methods: {
    // Convert internal string to external value and emit
    setValue (internalVal) {
      let emitVal;
      if (internalVal === 'inherit') {
        emitVal = undefined;
      } else {
        const boolVal = internalVal === 'yes';
        emitVal = this.negated ? !boolVal : boolVal;
      }
      this.$emit('update:modelValue', emitVal);
    }
  }
};
</script>

<style scoped>
.tri-state-toggle {
  min-width: 200px;
}

.tri-state-label {
  font-size: 0.85rem;
  font-weight: 500;
}
</style>
