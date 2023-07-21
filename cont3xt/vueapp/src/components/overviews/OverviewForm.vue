<template>
  <textarea
      rows="20"
      size="sm"
      v-if="rawEditMode"
      :value="rawEditText"
      @input="e => debounceRawEdit(e)"
      class="form-control form-control-sm"
  />
  <b-form v-else>
    <b-input-group
        size="sm"
        class="mb-2">
      <template #prepend>
        <b-input-group-text>
          Card Name
        </b-input-group-text>
      </template>
      <b-form-input
          trim
          required
          autofocus
          v-model="localOverview.name"
          :state="!!localOverview.name"
          @input="updateOverview"
      />
      <template #append>
        <b-input-group-text
            class="cursor-help"
            v-b-tooltip.hover.html="nameTip">
          <span class="fa fa-info-circle" />
        </b-input-group-text>
      </template>
    </b-input-group>

    <b-input-group
        size="sm"
        class="mb-2">
      <template #prepend>
        <b-input-group-text>
          Card Title
        </b-input-group-text>
      </template>
      <b-form-input
          trim
          required
          v-model="localOverview.title"
          :state="!!localOverview.title"
          @input="updateOverview"
      />
      <template #append>
        <b-input-group-text
            class="cursor-help"
            v-b-tooltip.hover.html="titleTip">
          <span class="fa fa-info-circle" />
        </b-input-group-text>
      </template>
    </b-input-group>

    <b-input-group
        size="sm"
        class="mb-2">
      <template #prepend>
        <b-input-group-text>
          iType
        </b-input-group-text>
      </template>
      <b-form-select
          v-model="localOverview.iType"
          :options="iTypes"
          :state="isDefaultOverview ? undefined : iTypes.includes(localOverview.iType)"
          :disabled="isDefaultOverview"
          @input="updateOverview"
      />
      <template #append>
        <b-input-group-text
            class="cursor-help"
            v-b-tooltip.hover.html="iTypeTip">
          <span class="fa fa-info-circle" />
        </b-input-group-text>
      </template>
    </b-input-group>

    <!-- overview roles -->
    <RoleDropdown
        :roles="getRoles"
        display-text="Who Can View"
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
    <span
        class="fa fa-info-circle fa-lg cursor-help ml-2 mr-1"
        v-b-tooltip.hover="'Creators will always be able to view and edit their overviews regardless of the roles selected here.'"
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

    <reorder-list
        v-for="(fieldRef, i) in localOverview.fields"
        :key="i"
        :index="i"
        :list="localOverview.fields"
        @update="updateOverviewFieldsList"
    >
      <template #handle>
        <span class="fa fa-bars d-inline link-handle" />
      </template>
      <template #default>
        <b-card
            :key="i"
            class="d-flex"
        >
          <b-form inline>
            <ToggleBtn
                class="overview-toggle-btn mr-2"
                @toggle="toggleExpanded(fieldRef)"
                :opened="fieldRef.expanded"
                :class="{expanded: fieldRef.expanded, invisible: !isCustom(fieldRef)}"
            />

            <b-input-group
                size="sm">
              <template #prepend>
                <b-input-group-text>
                  Source
                </b-input-group-text>
              </template>
              <b-form-select
                  trim
                  :value="fieldRef.from"
                  @change="e => setFrom(fieldRef, e)"
                  :options="sourceOptions"
                  :state="validateFieldRefFrom(fieldRef)"
                  @input="updateOverview"
              />
              <template #append>
                <b-input-group-text
                    class="cursor-help"
                    v-b-tooltip.hover.html="fieldRefFromTip">
                  <span class="fa fa-info-circle" />
                </b-input-group-text>
              </template>
            </b-input-group>
            <b-input-group
                size="sm"
                class="ml-2 flex-grow-1">
              <template #prepend>
                <b-input-group-text>
                  Field
                </b-input-group-text>
              </template>
              <b-form-select
                  trim
                  :value="getField(fieldRef)"
                  @change="e => setField(fieldRef, e)"
                  :options="fieldOptionsFor(fieldRef)"
                  :state="validateFieldRef(fieldRef)"
                  @input="updateOverview"
              />
              <template #append>
                <b-input-group-text
                    class="cursor-help"
                    v-b-tooltip.hover.html="fieldRefFieldTip">
                  <span class="fa fa-info-circle" />
                </b-input-group-text>
              </template>
            </b-input-group>
            <b-input-group
                v-if="!isCustom(fieldRef)"
                size="sm"
                class="ml-2">
              <template #prepend>
                <b-input-group-text>
                  Label
                </b-input-group-text>
              </template>
              <b-form-input
                  trim
                  v-model="fieldRef.alias"
                  :state="fieldRef.alias ? true : undefined"
                  @input="updateOverview"
              />
              <template #append>
                <b-input-group-text
                    class="cursor-help"
                    v-b-tooltip.hover.html="fieldRefAliasTip">
                  <span class="fa fa-info-circle" />
                </b-input-group-text>
              </template>
            </b-input-group>
            <b-dropdown
                right
                size="sm"
                variant="primary"
                class="ml-2"
                v-b-tooltip.hover="'Actions'">
              <b-dropdown-item
                  class="small"
                  @click="sendToTop(i)">
                <span class="fa fa-arrow-circle-up fa-fw" />
                Push to the TOP
              </b-dropdown-item>
              <b-dropdown-item
                  class="small"
                  @click="sendToBottom(i)">
                <span class="fa fa-arrow-circle-down fa-fw" />
                Push to the BOTTOM
              </b-dropdown-item>
              <b-dropdown-item
                  class="small"
                  @click="insertFieldRef(i + 1)">
                <span class="fa fa-plus-circle fa-fw" />
                Add a field after this one
              </b-dropdown-item>
              <b-dropdown-item
                  class="small"
                  @click="deleteFieldRef(i)">
                <span class="fa fa-times-circle fa-fw" />
                Remove this field
              </b-dropdown-item>
            </b-dropdown>
          </b-form>
          <template v-if="fieldRef.expanded">
            <textarea
                rows="5"
                size="sm"
                :value="getOrInitCustomText(fieldRef)"
                @input="e => debounceCustomRawEdit(fieldRef, e)"
                class="form-control form-control-sm mt-2"
            />
            <b-alert
                variant="warning"
                :show="!!fieldRef._error"
                class="alert-sm mt-2 mb-0">
              <span class="fa fa-exclamation-triangle fa-fw pr-2" />
              {{ fieldRef._error }}
            </b-alert>
          </template>
        </b-card>
      </template>
    </reorder-list>
    <b-button
        variant="outline-primary"
        class="mt-4 w-100"
        @click="appendFieldRef"
    >
      Add Field
    </b-button>
  </b-form>
</template>

<script>
import ReorderList from '@/utils/ReorderList.vue';
import { mapGetters } from 'vuex';
import RoleDropdown from '@../../../common/vueapp/RoleDropdown';
import { iTypes } from '@/utils/iTypes';
import ToggleBtn from '../../../../../common/vueapp/ToggleBtn.vue';

let timeout;

export default {
  name: 'OverviewForm',
  components: {
    ToggleBtn,
    ReorderList,
    RoleDropdown
  },
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
    rawEditMode (newVal) {
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
    }
  },
  methods: {
    updateViewRoles (roles) {
      this.$set(this.localOverview, 'viewRoles', roles);
      this.updateOverview();
    },
    updateEditRoles (roles) {
      this.$set(this.localOverview, 'editRoles', roles);
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

      return (this.getIntegrations[fieldRef.from]?.card?.fields?.map(field => field.label) ?? []).concat(['Custom']);
    },
    isCustom (fieldRef) {
      return fieldRef.type === 'custom';
    },
    getOrInitCustomText (fieldRef) {
      if (fieldRef._customRawEdit == null) {
        this.$set(fieldRef, '_customRawEdit', JSON.stringify(fieldRef.custom ?? {}, null, 2));
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
        this.$set(fieldRef, '_error', 'ERROR: Invalid JSON');
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
        this.$set(fieldRef, 'custom', { field: '', label: fieldRef.alias ?? '' });
        this.$set(fieldRef, 'expanded', true);
        delete fieldRef.field;
      } else {
        fieldRef.type = 'linked';
        this.$set(fieldRef, 'field', field);
        delete fieldRef.custom;
        delete fieldRef._customRawEdit;
        delete fieldRef.expanded;
      }
      this.updateOverview();
    },
    toggleExpanded (fieldRef) {
      this.$set(fieldRef, 'expanded', !fieldRef.expanded);
      this.updateOverview();
    },
    updateOverviewFieldsList ({ list }) {
      this.localOverview.fields = list;
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
