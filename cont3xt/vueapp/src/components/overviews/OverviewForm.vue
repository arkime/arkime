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
            <b-input-group
                size="sm">
              <template #prepend>
                <b-input-group-text>
                  Integration Name
                </b-input-group-text>
              </template>
              <b-form-input
                  trim
                  v-model="fieldRef.from"
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
                  Field Label
                </b-input-group-text>
              </template>
              <b-form-input
                  trim
                  v-model="fieldRef.field"
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
                size="sm"
                class="ml-2">
              <template #prepend>
                <b-input-group-text>
                  Alias
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

let timeout;

export default {
  name: 'OverviewForm',
  components: {
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
        title: 'Enter the <code>name</code> of the Integration you would like to show a field from.'
      },
      fieldRefFieldTip: {
        title: 'Enter the <code>label</code> of the field you would like to show from the given Integration.'
      },
      fieldRefAliasTip: {
        title: 'Optionally, change the label that will be displayed with this field.'
      },
      iTypes
    };
  },
  computed: {
    ...mapGetters(['getIntegrations', 'getRoles', 'getUser'])
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
        from: '', field: ''
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
    validateFieldRefFrom (fieldRef) {
      return Object.keys(this.getIntegrations)?.includes(fieldRef.from);
    },
    validateFieldRef (fieldRef) {
      if (!this.validateFieldRefFrom(fieldRef)) { return false; }

      const integrationFields = this.getIntegrations[fieldRef.from]?.card?.fields;
      const matchingField = integrationFields?.find(field => field.label === fieldRef.field);

      return matchingField != null;
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
