<!--
Copyright Yahoo Inc.
SPDX-License-Identifier: Apache-2.0
-->
<template>
  <div class="tri-state-toggle">
    <div class="tri-state-label mb-1">
      {{ label }}
    </div>
    <BButtonGroup size="sm">
      <BButton
        :variant="displayValue === undefined ? 'secondary' : 'outline-secondary'"
        :title="$t('users.inheritTip')"
        @click="setValue(undefined)">
        {{ $t('users.inherit') }}
      </BButton>
      <BButton
        :variant="displayValue === false ? 'secondary' : 'outline-secondary'"
        :title="$t('users.forceOffTip')"
        @click="setValue(false)">
        {{ $t('users.no') }}
      </BButton>
      <BButton
        :variant="displayValue === true ? 'secondary' : 'outline-secondary'"
        :title="$t('users.forceOnTip')"
        @click="setValue(true)">
        {{ $t('users.yes') }}
      </BButton>
    </BButtonGroup>
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
    displayValue () {
      if (this.modelValue === undefined || this.modelValue === null) {
        return undefined;
      }
      return this.negated ? !this.modelValue : this.modelValue;
    }
  },
  methods: {
    setValue (displayVal) {
      let emitVal;
      if (displayVal === undefined) {
        emitVal = undefined;
      } else {
        emitVal = this.negated ? !displayVal : displayVal;
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

.tri-state-toggle .btn {
  padding: 0.15rem 0.5rem;
  font-size: 0.75rem;
}
</style>
