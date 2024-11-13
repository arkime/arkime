<!--
Copyright Yahoo Inc.
SPDX-License-Identifier: Apache-2.0
-->
<template>
  <template v-if="rawEditMode">
    <v-textarea
      rows="20"
      v-if="!noEdit"
      variant="outlined"
      :value="rawEditText"
      @input="e => debounceRawEdit(e)">
    </v-textarea>
    <pre v-else>{{ rawEditText }}</pre>
  </template>
  <v-form v-else>
    <v-text-field
      class="mb-3"
      label="Card Name"
      trim
      required
      autofocus
      v-model="localOverview.name"
      :state="!!localOverview.name"
      @input="updateOverview"
    >
      <template #append-inner>
        <v-icon icon="mdi-information" class="cursor-help" />
        <html-tooltip :html="nameTip"/>
      </template>
    </v-text-field>

    <v-text-field
      class="mb-3"
      label="Card Title"
      trim
      required
      autofocus
      v-model="localOverview.title"
      :state="!!localOverview.title"
      @input="updateOverview"
    >
      <template #append-inner>
        <v-icon icon="mdi-information" class="cursor-help" />
        <html-tooltip :html="titleTip"/>
      </template>
    </v-text-field>

    <v-select
        class="mb-2"
        v-model="localOverview.iType"
        :items="iTypes"
        :rules="[isDefaultOverview ? true : iTypes.includes(localOverview.iType)]"
        :disabled="isDefaultOverview"
        @update:model-value="updateOverview"
        label="iType"
    >
      <template #append-inner>
        <v-icon icon="mdi-information" class="cursor-help" />
        <html-tooltip :html="iTypeTip"/>
      </template>
    </v-select>

    <!-- overview roles -->
    <RoleDropdown
        :roles="getRoles"
        display-text="Who Can View"
        class="mr-1"
        :selected-roles="localOverview.viewRoles"
        @selected-roles-updated="updateViewRoles"
        :disabled="isDefaultOverview"
    />
    <RoleDropdown
        :roles="getRoles"
        display-text="Who Can Edit"
        :selected-roles="localOverview.editRoles"
        @selected-roles-updated="updateEditRoles"
    />
    <v-icon
      size="large"
      icon="mdi-information"
      class="cursor-help ml-2 mr-1"
      v-tooltip="'Creators will always be able to view and edit their overviews regardless of the roles selected here.'"
    />
    <span v-if="!localOverview.creator || (getUser && localOverview.creator === getUser.userId)">
      As the creator, you can always view and edit your overviews.
    </span>
    <div
        class="mt-2"
        v-if="localOverview.creator">
      Created by
      <span class="text-info">
        {{ localOverview.creator }}
      </span>
    </div>
    <!-- /overview roles -->

    <v-btn
        v-if="localOverview.fields.length"
        variant="outlined"
        color="primary"
        class="mt-4 w-100"
        @click="prependFieldRef"
    >
      Add Field
    </v-btn>
    <drag-update-list
      class="d-flex flex-column ga-3 mt-3"
      :value="localOverview.fields"
      @update="updateOverviewFieldsList"
    >
      <div
        v-for="(fieldRef, i) in localOverview.fields"
        :key="i"
        class="position-relative"
      >
        <v-icon icon="mdi-menu" class="d-inline link-handle drag-handle" />
        <v-card
          :key="i"
          class="d-flex flex-column pa-2"
          variant="tonal"
        >
          <v-form class="w-100 d-flex flex-row align-center">
            <ToggleBtn
              class="overview-toggle-btn mr-2"
              @toggle="toggleExpanded(fieldRef)"
              :opened="fieldRef.expanded"
              :class="{expanded: fieldRef.expanded, invisible: !isCustom(fieldRef)}"
            />
            <v-select
              label="Source"
              trim
              :value="fieldRef.from"
              :dirty="!!fieldRef.from"
              @update:model-value="e => setFrom(fieldRef, e)"
              :items="sourceOptions"
              :rules="[validateFieldRefFrom(fieldRef)]"
            >
              <template #append-inner>
                <v-icon icon="mdi-information" class="cursor-help" />
                <html-tooltip :html="fieldRefFromTip"/>
              </template>
            </v-select>
            <v-select
              class="ml-2 flex-grow-1"
              label="Field"
              trim
              no-data-text="For field options, select a valid source"
              :value="getField(fieldRef)"
              :dirty="!!getField(fieldRef)"
              :disabled="!fieldRef.from"
              @update:model-value="e => setField(fieldRef, e)"
              :items="fieldOptionsFor(fieldRef)"
              :rules="[validateFieldRef(fieldRef)]"
            >
              <template #append-inner>
                <v-icon icon="mdi-information" class="cursor-help" />
                <html-tooltip :html="fieldRefFieldTip"/>
              </template>
            </v-select>
            <v-text-field
              v-if="!isCustom(fieldRef)"
              class="ml-2"
              label="Label"
              trim
              v-model="fieldRef.alias"
              @input="updateOverview"
            >
              <template #append-inner>
                <v-icon icon="mdi-information" class="cursor-help" />
                <html-tooltip :html="fieldRefAliasTip"/>
              </template>
            </v-text-field>
            <action-dropdown
              :actions="createFieldActions(i)"
              color="primary"
              size="small"
              class="ml-2 square-btn-sm"
              v-tooltip="'Actions'"
            />
          </v-form>
          <template v-if="fieldRef.expanded">
            <textarea
                rows="5"
                size="sm"
                :value="getOrInitCustomText(fieldRef)"
                @input="e => debounceCustomRawEdit(fieldRef, e)"
                class="form-control form-control-sm mt-2"
            />
            <v-alert
              color="warning"
              v-if="!!fieldRef._error"
              class="alert-sm mt-2 mb-0">
              <v-icon icon="mdi-alert" class="mr-2" />
              {{ fieldRef._error }}
            </v-alert>
          </template>
        </v-card>
      </div>
    </drag-update-list>
    <v-btn
        variant="outlined"
        color="primary"
        class="mt-4 w-100"
        @click="appendFieldRef"
    >
      Add Field
    </v-btn>
  </v-form>
</template>

<script>
import ActionDropdown from '@/utils/ActionDropdown.vue';
import DragUpdateList from '@/utils/DragUpdateList.vue';
import { mapGetters } from 'vuex';
import RoleDropdown from '@common/RoleDropdown.vue';
import { iTypes } from '@/utils/iTypes';
import ToggleBtn from '@common/ToggleBtn.vue';
import HtmlTooltip from '@common/HtmlTooltip.vue';

let timeout;

export default {
  name: 'OverviewForm',
  components: {
    ToggleBtn,
    DragUpdateList,
    RoleDropdown,
    HtmlTooltip,
    ActionDropdown
  },
  emits: ['update-modified-overview'],
  props: {
    modifiedOverview: {
      type: Object,
      required: true
    },
    rawEditMode: {
      type: Boolean,
      required: true
    },
    isDefaultOverview: {
      type: Boolean,
      default: false
    },
    noEdit: { // for use of disabled raw edit (for view-only privileged users)
      type: Boolean,
      default: false
    }
  },
  data () {
    return {
      localOverview: JSON.parse(JSON.stringify(this.modifiedOverview)),
      confirmDelete: false,
      rawEditText: undefined,
      nameTip: {
        title: 'This name will be used to identify this overview in the Overview Selector and will be viewable by those you share this with.'
      },
      titleTip: {
        title: 'Set the title to display for this card. <code>%{query}</code> will be replaced by the queried indicator.'
      },
      iTypeTip: {
        title: 'The indicator type to display this overview for... Can be either: <code>domain</code>, <code>ip</code>, <code>url</code>, <code>email</code>, <code>phone</code>, <code>hash</code>, or <code>text</code>.'
      },
      fieldRefFromTip: {
        title: 'Select the <code>name</code> of the Integration you would like to show a field from.'
      },
      fieldRefFieldTip: {
        title: 'Select the <code>label</code> of the field you would like to show from the given Integration, or <code>Custom</code> to make your own.'
      },
      fieldRefAliasTip: {
        title: 'Optionally, change the label that will be displayed with this field.'
      },
      iTypes
    };
  },
  computed: {
    ...mapGetters(['getIntegrations', 'getRoles', 'getUser']),
    sourceOptions () {
      const sources = Object.keys(this.getIntegrations);
      sources.sort();
      return sources;
    }
  },
  watch: {
    modifiedOverview () {
      if (this.modifiedOverview) { // sync with parent (important for change cancellation)
        this.localOverview = JSON.parse(JSON.stringify(this.modifiedOverview));
      }
    },
    rawEditMode: {
      handler (newVal) {
        if (!newVal) {
          this.rawEditText = undefined;
          return;
        }

        // remove un-editable fields
        const clone = JSON.parse(JSON.stringify(this.localOverview));
        delete clone._id;
        delete clone.creator;
        delete clone._editable;
        delete clone._viewable;

        clone.fields ??= [];
        for (const fieldRef of clone.fields) {
          if (!this.isCustom(fieldRef) || !fieldRef.expanded) {
            // expanded is only necessary to keep open currently expanded custom fields
            delete fieldRef.expanded;
          }
          delete fieldRef._customRawEdit;
          // type can be inferred from other fields (field vs custom existence)
          //     so we don't need it for raw edit to avoid verbosity
          delete fieldRef.type;
        }
        this.rawEditText = JSON.stringify(clone, null, 2);
      },
      immediate: true
    }
  },
  methods: {
    createFieldActions (i) {
      return [
        {
          text: 'Push to the TOP',
          icon: 'mdi-arrow-up-circle',
          action: () => this.sendToTop(i)
        },
        {
          text: 'Push to the BOTTOM',
          icon: 'mdi-arrow-down-circle',
          action: () => this.sendToBottom(i)
        },
        {
          text: 'Add a field after this one',
          icon: 'mdi-plus-circle',
          action: () => this.insertFieldRef(i + 1)
        },
        {
          text: 'Remove this field',
          icon: 'mdi-close-circle',
          action: () => this.deleteFieldRef(i)
        }
      ];
    },
    updateViewRoles (roles) {
      this.localOverview.viewRoles = roles;
      this.updateOverview();
    },
    updateEditRoles (roles) {
      this.localOverview.editRoles = roles;
      this.updateOverview();
    },
    updateOverview () {
      this.$emit('update-modified-overview', JSON.parse(JSON.stringify(this.localOverview)));
    },
    insertFieldRef (index) {
      this.localOverview.fields.splice(index, 0, {
        type: 'linked', from: '', field: ''
      });
      this.updateOverview();
    },
    deleteFieldRef (index) {
      this.localOverview.fields.splice(index, 1);
      this.updateOverview();
    },
    prependFieldRef () {
      this.insertFieldRef(0);
    },
    appendFieldRef () {
      this.insertFieldRef(this.localOverview.fields.length);
    },
    moveFieldRef (fromIndex, toIndex) {
      const fieldRef = this.localOverview.fields[fromIndex];
      this.localOverview.fields.splice(fromIndex, 1);
      this.localOverview.fields.splice(toIndex, 0, fieldRef);
      this.updateOverview();
    },
    sendToTop (fromIndex) {
      this.moveFieldRef(fromIndex, 0);
    },
    sendToBottom (fromIndex) {
      this.moveFieldRef(fromIndex, this.localOverview.fields.length - 1);
    },
    fieldOptionsFor (fieldRef) {
      if (!this.validateFieldRefFrom(fieldRef)) { return []; }

      const integrationFields = this.getIntegrations[fieldRef.from]?.card?.fields?.map(field => field.label) ?? [];
      integrationFields.sort();

      return integrationFields.concat(['Custom']);
    },
    isCustom (fieldRef) {
      return fieldRef.type === 'custom';
    },
    getOrInitCustomText (fieldRef) {
      if (fieldRef._customRawEdit == null) {
        fieldRef._customRawEdit = JSON.stringify(fieldRef.custom ?? {}, null, 2);
      }
      return fieldRef._customRawEdit;
    },
    debounceCustomRawEdit (fieldRef, e) {
      fieldRef._customRawEdit = e.target.value;
      if (timeout) { clearTimeout(timeout); }
      // debounce the textarea so that it only updates the overview after keyups cease for 400ms
      timeout = setTimeout(() => {
        timeout = null;
        this.updateCustomRawEdit(fieldRef);
      }, 400);
    },
    updateCustomRawEdit (fieldRef) {
      try {
        fieldRef.custom = JSON.parse(fieldRef._customRawEdit);
        delete fieldRef._error;
      } catch (err) {
        fieldRef._error = 'ERROR: Invalid JSON';
      }
      this.updateOverview();
    },
    validateFieldRefFrom (fieldRef) {
      return this.sourceOptions.includes(fieldRef.from);
    },
    validateFieldRef (fieldRef) {
      return this.validateFieldRefFrom(fieldRef) &&
          (this.fieldOptionsFor(fieldRef)?.includes(fieldRef.field) || this.isCustom(fieldRef));
    },
    setFrom (fieldRef, from) {
      fieldRef.from = from;
      this.updateOverview();
    },
    getField (fieldRef) {
      return this.isCustom(fieldRef) ? 'Custom' : fieldRef.field;
    },
    setField (fieldRef, field) {
      if (field === 'Custom') {
        fieldRef.type = 'custom';
        fieldRef.custom = { field: '', label: fieldRef.alias ?? '' };
        fieldRef.expanded = true;
        delete fieldRef.field;
      } else {
        fieldRef.type = 'linked';
        fieldRef.field = field;
        delete fieldRef.custom;
        delete fieldRef._customRawEdit;
        delete fieldRef.expanded;
      }
      this.updateOverview();
    },
    toggleExpanded (fieldRef) {
      fieldRef.expanded = !fieldRef.expanded;
      this.updateOverview();
    },
    updateOverviewFieldsList ({ newList }) {
      this.localOverview.fields = newList;
      this.updateOverview();
    },
    debounceRawEdit (e) {
      this.rawEditText = e.target.value;
      if (timeout) { clearTimeout(timeout); }
      // debounce the textarea so that it only updates the overview after keyups cease for 400ms
      timeout = setTimeout(() => {
        timeout = null;
        this.updateRawOverview();
      }, 400);
    },
    updateRawOverview () {
      try {
        const overviewFromRaw = JSON.parse(this.rawEditText);
        overviewFromRaw.fields ??= [];
        for (const fieldRef of overviewFromRaw.fields) {
          fieldRef.type = (fieldRef.custom == null) ? 'linked' : 'custom';
        }

        this.localOverview = {
          ...this.localOverview,
          name: overviewFromRaw.name,
          title: overviewFromRaw.title,
          iType: overviewFromRaw.iType,
          fields: overviewFromRaw.fields,
          viewRoles: overviewFromRaw.viewRoles,
          editRoles: overviewFromRaw.editRoles
        };
        this.updateOverview();
      } catch (err) {
        console.warn('Invalid JSON for raw overview', err);
        this.$store.commit('SET_OVERVIEWS_ERROR', 'Invalid JSON');
      }
    }
  }
};
</script>

<style scoped>
.overview-toggle-btn {
  font-size: 1rem;
  padding: 0.1rem 0.5rem;
}
</style>
