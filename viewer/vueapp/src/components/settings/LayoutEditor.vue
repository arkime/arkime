<!--
Copyright Yahoo Inc.
SPDX-License-Identifier: Apache-2.0
-->
<!--
  Editor for the layout shareables shown in Settings: column layouts, info
  field layouts and spiview layouts. They only differ in what their field list
  means, so one dialog covers all three. Sharing is not edited here, the
  Shareables tab owns that.
-->
<template>
  <v-dialog
    :model-value="modelValue"
    max-width="900"
    @update:model-value="$emit('update:modelValue', $event)">
    <v-card>
      <v-card-title>
        {{ $t('settings.layoutEditor.title') }}
      </v-card-title>
      <v-card-text>
        <div class="arkime-input-group arkime-input-group--fluid mb-3">
          <span class="arkime-input-label">{{ $t('settings.layoutEditor.name') }}</span>
          <input
            type="text"
            class="arkime-input-control"
            v-model="name"
            :placeholder="$t('settings.layoutEditor.namePlaceholder')">
        </div>

        <!-- fields -->
        <div class="d-flex align-center mb-1">
          <strong class="flex-grow-1">{{ $t('settings.layoutEditor.fields') }}</strong>
          <FieldSelectDropdown
            :selected-fields="selectedFields"
            :tooltip-text="$t('settings.layoutEditor.addFields')"
            :search-placeholder="$t('settings.layoutEditor.searchFields')"
            :exclude-filename="true"
            field-id-key="dbField"
            @toggle="toggleField"
            @clear="selectedFields = []" />
        </div>

        <div class="layout-editor-fields mb-3">
          <label
            v-for="field in selectedFields"
            :key="field"
            class="arkime-badge arkime-badge--grey me-1 mb-1">
            {{ friendlyName(field) }}
            <v-icon
              icon="mdi-close"
              size="x-small"
              class="cursor-pointer ms-1"
              @click="toggleField({ dbField: field })" />
          </label>
          <span
            v-if="!selectedFields.length"
            class="text-medium-emphasis small">
            {{ $t('settings.layoutEditor.noFields') }}
          </span>
        </div>

        <!-- sort order, column layouts only -->
        <template v-if="hasOrder">
          <div class="d-flex align-center mb-1">
            <strong class="flex-grow-1">{{ $t('settings.layoutEditor.sort') }}</strong>
            <v-btn
              variant="text"
              size="small"
              density="comfortable"
              :disabled="!selectedFields.length"
              @click="addSort">
              <v-icon
                icon="mdi-plus"
                class="me-1" />
              {{ $t('settings.layoutEditor.addSort') }}
            </v-btn>
          </div>
          <div
            v-for="(entry, i) in order"
            :key="`sort-${i}`"
            class="d-flex align-center mb-1">
            <select
              class="arkime-input-control me-2"
              :value="entry[0]"
              @change="order[i][0] = $event.target.value">
              <option
                v-for="field in selectedFields"
                :key="field"
                :value="field">
                {{ friendlyName(field) }}
              </option>
            </select>
            <select
              class="arkime-input-control me-2"
              :value="entry[1]"
              @change="order[i][1] = $event.target.value">
              <option value="asc">
                {{ $t('settings.layoutEditor.asc') }}
              </option>
              <option value="desc">
                {{ $t('settings.layoutEditor.desc') }}
              </option>
            </select>
            <v-btn
              color="error"
              variant="flat"
              size="small"
              density="comfortable"
              icon
              @click="order.splice(i, 1)">
              <v-icon icon="mdi-trash-can-outline" />
            </v-btn>
          </div>
          <span
            v-if="!order.length"
            class="text-medium-emphasis small">
            {{ $t('settings.layoutEditor.noSort') }}
          </span>
        </template>

        <v-alert
          v-if="error"
          type="error"
          variant="tonal"
          density="compact"
          class="mt-3">
          {{ error }}
        </v-alert>
      </v-card-text>
      <v-card-actions>
        <v-spacer />
        <v-btn
          variant="text"
          @click="$emit('update:modelValue', false)">
          {{ $t('common.cancel') }}
        </v-btn>
        <v-btn
          color="success"
          variant="flat"
          @click="save">
          {{ $t('common.save') }}
        </v-btn>
      </v-card-actions>
    </v-card>
  </v-dialog>
</template>

<script>
import FieldSelectDropdown from '../utils/FieldSelectDropdown.vue';

export default {
  name: 'LayoutEditor',
  emits: ['update:modelValue', 'save'],
  components: { FieldSelectDropdown },
  props: {
    modelValue: {
      type: Boolean,
      default: false
    },
    // the layout being edited, flattened by createLayoutService
    layout: {
      type: Object,
      default: undefined
    },
    // 'columns' keeps its field list in `columns` plus a sort `order`,
    // 'fields' is a plain array, 'spiview' is a "field:count,..." string
    kind: {
      type: String,
      default: 'fields'
    },
    fieldsMap: {
      type: Object,
      default: () => ({})
    }
  },
  data () {
    return {
      name: '',
      selectedFields: [],
      order: [],
      // spiview carries a per field value count, preserved across an edit
      counts: {},
      error: ''
    };
  },
  computed: {
    hasOrder () {
      return this.kind === 'columns';
    }
  },
  watch: {
    modelValue (isOpen) {
      if (isOpen) { this.reset(); }
    }
  },
  methods: {
    reset () {
      const layout = this.layout || {};
      this.error = '';
      this.name = layout.name ?? '';
      this.counts = {};
      this.order = JSON.parse(JSON.stringify(layout.order ?? []));

      if (this.kind === 'spiview') {
        this.selectedFields = String(layout.fields ?? '').split(',').filter(Boolean).map((param) => {
          const [id, count] = param.split(':');
          if (count !== undefined) { this.counts[id] = count; }
          return id;
        });
      } else if (this.hasOrder) {
        this.selectedFields = [...(layout.columns ?? [])];
      } else {
        this.selectedFields = [...(layout.fields ?? [])];
      }
    },
    friendlyName (dbField) {
      return this.fieldsMap[dbField]?.friendlyName ?? dbField;
    },
    toggleField (field) {
      const id = field.dbField ?? field;
      const i = this.selectedFields.indexOf(id);
      if (i > -1) {
        this.selectedFields.splice(i, 1);
        // a field that is gone cannot be sorted on
        this.order = this.order.filter(o => o[0] !== id);
      } else {
        this.selectedFields.push(id);
      }
    },
    addSort () {
      this.order.push([this.selectedFields[0], 'asc']);
    },
    save () {
      if (!this.name) {
        this.error = this.$t('settings.layoutEditor.nameRequired');
        return;
      }
      if (!this.selectedFields.length) {
        this.error = this.$t('settings.layoutEditor.fieldsRequired');
        return;
      }

      let data;
      if (this.kind === 'spiview') {
        // put the counts back so loading the layout shows the same value counts
        data = { fields: this.selectedFields.map(f => `${f}:${this.counts[f] ?? 100}`).join(',') };
      } else if (this.hasOrder) {
        data = { columns: [...this.selectedFields], order: JSON.parse(JSON.stringify(this.order)) };
      } else {
        data = { fields: [...this.selectedFields] };
      }

      this.$emit('save', { name: this.name, ...data });
    }
  }
};
</script>

<style scoped>
.layout-editor-fields {
  max-height: 220px;
  overflow-y: auto;
}
</style>
