<!--
Copyright Yahoo Inc.
SPDX-License-Identifier: Apache-2.0
-->
<template>
  <template v-if="rawEditMode">
    <textarea
      rows="20"
      v-if="!noEdit"
      :value="rawEditText"
      @input="e => debounceRawEdit(e)"
      class="cont3xt-textarea" />
    <pre v-else>{{ rawEditText }}</pre>
  </template>
  <v-form v-else>
    <v-text-field
      class="mb-3"
      :label="$t('cont3xt.overviews.cardName')"
      trim
      required
      autofocus
      v-model="localOverview.name"
      :state="!!localOverview.name"
      @input="updateOverview">
      <template #append-inner>
        <v-icon
          icon="mdi-information"
          class="cursor-help" />
        <html-tooltip :html="nameTip" />
      </template>
    </v-text-field>

    <v-text-field
      class="mb-3"
      :label="$t('cont3xt.overviews.cardTitle')"
      trim
      required
      autofocus
      v-model="localOverview.title"
      :state="!!localOverview.title"
      @input="updateOverview">
      <template #append-inner>
        <v-icon
          icon="mdi-information"
          class="cursor-help" />
        <html-tooltip :html="titleTip" />
      </template>
    </v-text-field>

    <v-select
      class="mb-2"
      v-model="localOverview.iType"
      :items="iTypes"
      :rules="[isDefaultOverview ? true : iTypes.includes(localOverview.iType)]"
      :disabled="isDefaultOverview"
      @update:model-value="updateOverview"
      :label="$t('cont3xt.overviews.itype')">
      <template #append-inner>
        <v-icon
          icon="mdi-information"
          class="cursor-help" />
        <html-tooltip :html="iTypeTip" />
      </template>
    </v-select>

    <!-- overview roles -->
    <RoleDropdown
      :roles="getRoles"
      :display-text="$t('cont3xt.whoCanView')"
      class="me-1"
      :selected-roles="localOverview.viewRoles"
      @selected-roles-updated="updateViewRoles"
      :disabled="isDefaultOverview" />
    <RoleDropdown
      :roles="getRoles"
      :display-text="$t('cont3xt.whoCanEdit')"
      :selected-roles="localOverview.editRoles"
      @selected-roles-updated="updateEditRoles" />
    <v-icon
      size="large"
      icon="mdi-information"
      class="cursor-help ms-2 me-1"
      v-tooltip="$t('cont3xt.overviews.rolesTip')" />
    <span v-if="!localOverview.creator || (getUser && localOverview.creator === getUser.userId)">
      {{ $t('cont3xt.overviews.creatorNote') }}
    </span>
    <div
      class="mt-2"
      v-if="localOverview.creator">
      {{ $t('cont3xt.createdBy') }}
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
      @click="prependFieldRef">
      {{ $t('cont3xt.overviews.addField') }}
    </v-btn>
    <drag-update-list
      class="d-flex flex-column ga-3 mt-3"
      :value="localOverview.fields"
      @update="updateOverviewFieldsList">
      <div
        v-for="(fieldRef, i) in localOverview.fields"
        :key="i"
        class="position-relative">
        <v-icon
          icon="mdi-menu"
          class="d-inline link-handle drag-handle" />
        <v-card
          :key="i"
          class="d-flex flex-column pa-2"
          variant="tonal">
          <v-form class="w-100 d-flex flex-row align-center">
            <ToggleBtn
              class="overview-toggle-btn me-2"
              @toggle="toggleExpanded(fieldRef)"
              :opened="fieldRef.expanded"
              :class="{expanded: fieldRef.expanded, invisible: !isCustom(fieldRef)}" />
            <v-select
              :label="$t('cont3xt.overviews.source')"
              trim
              :value="fieldRef.from"
              :dirty="!!fieldRef.from"
              @update:model-value="e => setFrom(fieldRef, e)"
              :items="sourceOptions"
              :rules="[validateFieldRefFrom(fieldRef)]">
              <template #append-inner>
                <v-icon
                  icon="mdi-information"
                  class="cursor-help" />
                <html-tooltip :html="fieldRefFromTip" />
              </template>
            </v-select>
            <v-select
              class="ms-2 flex-grow-1"
              :label="$t('cont3xt.overviews.field')"
              trim
              :no-data-text="$t('cont3xt.overviews.noFieldOptions')"
              :value="getField(fieldRef)"
              :dirty="!!getField(fieldRef)"
              :disabled="!fieldRef.from"
              @update:model-value="e => setField(fieldRef, e)"
              :items="fieldOptionsFor(fieldRef)"
              :rules="[validateFieldRef(fieldRef)]">
              <template #append-inner>
                <v-icon
                  icon="mdi-information"
                  class="cursor-help" />
                <html-tooltip :html="fieldRefFieldTip" />
              </template>
            </v-select>
            <v-text-field
              v-if="!isCustom(fieldRef)"
              class="ms-2"
              :label="$t('cont3xt.overviews.label')"
              trim
              v-model="fieldRef.alias"
              @input="updateOverview">
              <template #append-inner>
                <v-icon
                  icon="mdi-information"
                  class="cursor-help" />
                <html-tooltip :html="fieldRefAliasTip" />
              </template>
            </v-text-field>
            <action-dropdown
              :actions="createFieldActions(i)"
              color="primary"
              size="small"
              class="ms-2"
              v-tooltip="$t('cont3xt.overviews.actions')" />
          </v-form>
          <template v-if="fieldRef.expanded">
            <textarea
              rows="5"
              size="sm"
              :value="getOrInitCustomText(fieldRef)"
              @input="e => debounceCustomRawEdit(fieldRef, e)"
              class="cont3xt-textarea mt-2" />
            <v-alert
              color="warning"
              v-if="!!fieldRef._error"
              class="mt-2 mb-0">
              <v-icon
                icon="mdi-alert"
                class="me-2" />
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
      @click="appendFieldRef">
      {{ $t('cont3xt.overviews.addField') }}
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
      iTypes
    };
  },
  computed: {
    ...mapGetters(['getIntegrations', 'getRoles', 'getUser']),
    nameTip () {
      return { title: this.$t('cont3xt.overviews.nameTipHtml') };
    },
    titleTip () {
      return { title: this.$t('cont3xt.overviews.titleTipHtml') };
    },
    iTypeTip () {
      return { title: this.$t('cont3xt.overviews.itypeTipHtml') };
    },
    fieldRefFromTip () {
      return { title: this.$t('cont3xt.overviews.fieldFromTipHtml') };
    },
    fieldRefFieldTip () {
      return { title: this.$t('cont3xt.overviews.fieldTipHtml') };
    },
    fieldRefAliasTip () {
      return { title: this.$t('cont3xt.overviews.fieldAliasTipHtml') };
    },
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
          // nothing to parse (initial load)
          if (this.rawEditText == null) { return; }

          try { // need to update local overview from json input
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
            this.$store.commit('SET_OVERVIEWS_ERROR', this.$t('cont3xt.invalidJson'));
          }
          // clear rawEditText to be parsed again if rawEditMode triggered
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
          text: this.$t('cont3xt.overviews.pushTop'),
          icon: 'mdi-arrow-up-circle',
          action: () => this.sendToTop(i)
        },
        {
          text: this.$t('cont3xt.overviews.pushBottom'),
          icon: 'mdi-arrow-down-circle',
          action: () => this.sendToBottom(i)
        },
        {
          text: this.$t('cont3xt.overviews.addFieldAfter'),
          icon: 'mdi-plus-circle',
          action: () => this.insertFieldRef(i + 1)
        },
        {
          text: this.$t('cont3xt.overviews.removeField'),
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
        fieldRef._error = this.$t('cont3xt.overviews.invalidJsonError');
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

        this.$emit('update-modified-overview', {
          ...this.localOverview,
          name: overviewFromRaw.name,
          title: overviewFromRaw.title,
          iType: overviewFromRaw.iType,
          fields: overviewFromRaw.fields,
          viewRoles: overviewFromRaw.viewRoles,
          editRoles: overviewFromRaw.editRoles
        });
      } catch (err) {
        console.warn('Invalid JSON for raw overview', err);
        this.$store.commit('SET_OVERVIEWS_ERROR', this.$t('cont3xt.invalidJson'));
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
