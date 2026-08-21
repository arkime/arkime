<!--
Copyright Yahoo Inc.
SPDX-License-Identifier: Apache-2.0
-->
<!--
  Shared expanded-expression editor modal. Used by:
    - search/ExpressionTypeahead.vue (the main Sessions search bar)
    - sessions/ModifyView.vue (view create/edit form)

  Owns the v-dialog + textarea ExpressionAutocompleteInput. Two-way binds
  the open state (v-model) and the expression text (v-model:expression).
  Emits `apply` when the user accepts the expression (Apply button or
  Enter via the autocomplete) so the parent can run a search / save.
-->
<template>
  <v-dialog
    :model-value="modelValue"
    @update:model-value="onOpenUpdate"
    @after-enter="focusInput"
    max-width="1140">
    <v-card density="compact">
      <v-card-title>
        <v-icon
          icon="mdi-magnify"
          size="large"
          class="me-2" />
      </v-card-title>
      <v-card-text class="big-expression-card-text">
        <ExpressionAutocompleteInput
          ref="autocomplete"
          textarea
          rows="5"
          :model-value="expression"
          @update:model-value="$emit('update:expression', $event)"
          :placeholder="placeholder || $t('common.search')"
          @apply="onApply" />
      </v-card-text>
      <v-card-actions>
        <div class="d-flex w-100 justify-space-between">
          <div>
            <v-btn
              size="large"
              variant="flat"
              color="secondary"
              @click="onClose">
              {{ $t('common.close') }}
            </v-btn>
            <v-btn
              size="large"
              variant="flat"
              color="warning"
              class="ms-2"
              @click="$emit('update:expression', '')">
              {{ $t('common.clear') }}
            </v-btn>
          </div>
          <v-btn
            size="large"
            variant="flat"
            :style="tertiaryBtnStyle"
            @click="onApply">
            {{ applyLabel || $t('common.apply') }}
          </v-btn>
        </div>
      </v-card-actions>
    </v-card>
  </v-dialog>
</template>

<script>
import ExpressionAutocompleteInput from './ExpressionAutocompleteInput.vue';

export default {
  name: 'BigExpressionModal',
  components: { ExpressionAutocompleteInput },
  emits: ['update:modelValue', 'update:expression', 'apply'],
  props: {
    modelValue: { type: Boolean, default: false },
    expression: { type: String, default: '' },
    placeholder: { type: String, default: '' },
    applyLabel: { type: String, default: '' }
  },
  data () {
    return {
      // Arkime theme-color v-btn style. Vuetify :color can't take CSS vars.
      tertiaryBtnStyle: {
        backgroundColor: 'rgb(var(--v-theme-tertiary))',
        color: 'rgb(var(--v-theme-button-fg))'
      }
    };
  },
  methods: {
    onOpenUpdate (val) {
      if (!val) { this.onClose(); }
    },
    onClose () {
      this.$emit('update:modelValue', false);
    },
    onApply () {
      this.$emit('apply');
      this.$emit('update:modelValue', false);
    },
    focusInput () {
      setTimeout(() => {
        if (this.$refs.autocomplete) {
          this.$refs.autocomplete.focus();
        }
      }, 100);
    }
  }
};
</script>

<style>
/* ExpressionAutocompleteInput renders a bare <textarea
   class="arkime-input-control"> here -- no arkime-input-group parent
   means the base "transparent bg + border:none" style leaves it
   invisible on the v-card surface. Paint a proper bordered field. */
.big-expression-card-text textarea.arkime-input-control {
  width: 100%;
  border: 1px solid rgb(var(--v-theme-neutral)) !important;
  border-radius: 4px;
  background-color: rgb(var(--v-theme-background)) !important;
  padding: 8px 10px;
}
.big-expression-card-text textarea.arkime-input-control:focus {
  border-color: rgb(var(--v-theme-primary)) !important;
  box-shadow: 0 0 0 1px rgb(var(--v-theme-primary));
  outline: none;
}
</style>
