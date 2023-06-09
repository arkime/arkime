<template>
  <!-- view (for settings page users who can view but not edit) -->
  <b-card
      v-if="!(getUser && (getUser.userId === localOverview.creator || localOverview._editable || (getUser.roles && getUser.roles.includes('cont3xtAdmin'))))"
      class="w-100">
    <template #header>
      <h6 class="mb-0 link-header">
        <span
            class="fa fa-share-alt mr-1 cursor-help"
            v-b-tooltip.hover="`Shared with you by ${localOverview.creator}`"
        />
        {{ localOverview.name }}
        <small class="pull-right">
          You can only view this Link Group
        </small>
      </h6>
    </template>
    <b-card-body>
      <div class="d-flex flex-row">
        <h6>Title:</h6><span class="ml-1">{{ localOverview.title }}</span>
      </div>
      <div class="d-flex flex-row">
        <h6>iType:</h6><span class="ml-1">{{ localOverview.iType }}</span>
      </div>
      <div class="d-flex flex-row">
        <h6>Fields:</h6><b-badge v-if="!localOverview.fields.length" class="ml-1">None</b-badge>
      </div>
      <div class="d-flex flex-column">
        <b-card v-for="(field, i) in localOverview.fields" :key="i"
                class="mb-1"
        >
          <span class="text-warning bold">{{ field.from }}</span> <span class="text-primary">{{ field.field }}</span>
          <span v-if="field.alias">as <span class="text-info">"{{ field.alias }}"</span></span>
        </b-card>
      </div>
    </b-card-body>
  </b-card> <!-- /view -->
  <b-card v-else class="w-100">
    <template #header>
      <div class="w-100 d-flex justify-content-between">
        <div>
          <!-- delete button -->
          <transition name="buttons">
            <b-button
                size="sm"
                variant="danger"
                v-if="!confirmDelete && !isDefaultOverview"
                @click="confirmDelete = true"
                v-b-tooltip.hover="'Delete this overview'">
              <span class="fa fa-trash" />
            </b-button>
          </transition> <!-- /delete button -->
          <!-- cancel confirm delete button -->
          <transition name="buttons">
            <b-button
                size="sm"
                title="Cancel"
                variant="warning"
                v-b-tooltip.hover
                :disabled="isDefaultOverview"
                v-if="confirmDelete"
                @click="confirmDelete = false">
              <span class="fa fa-ban" />
            </b-button>
          </transition> <!-- /cancel confirm delete button -->
          <!-- confirm delete button -->
          <transition name="buttons">
            <b-button
                size="sm"
                variant="danger"
                v-b-tooltip.hover
                title="Are you sure?"
                :disabled="isDefaultOverview"
                v-if="confirmDelete"
                @click="deleteOverview">
              <span class="fa fa-check" />
            </b-button>
          </transition> <!-- /confirm delete button -->
        </div>
        <div>
          <transition name="buttons">
            <b-button
                size="sm"
                variant="secondary"
                @click="rawEditMode = !rawEditMode"
                v-b-tooltip.hover="'Toggle raw configuration for this link group'">
              <span class="fa fa-pencil-square-o" />
            </b-button>
          </transition>
          <transition name="buttons">
            <b-button
                size="sm"
                variant="warning"
                v-if="changesMade"
                @click="cancelOverviewModification"
                v-b-tooltip.hover="'Cancel unsaved updates'">
              <span class="fa fa-ban" />
            </b-button>
          </transition>
          <transition name="buttons">
            <b-button
                size="sm"
                variant="success"
                v-if="changesMade"
                @click="saveOverview"
                v-b-tooltip.hover="'Save this link group'">
              <span class="fa fa-save" />
            </b-button>
          </transition>
        </div>
      </div>
    </template>

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
          autofocus
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
          :options="['domain', 'ip', 'url', 'email', 'phone', 'hash', 'text']"
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
        v-b-tooltip.hover="'Creators will always be able to view and edit their link groups regardless of the roles selected here.'"
    />
    <span v-if="!localOverview.creator || (getUser && localOverview.creator === getUser.userId)">
      As the creator, you can always view and edit your link groups.
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
  </b-card>
</template>

<script>
import ReorderList from '@/utils/ReorderList.vue';
import { mapGetters } from 'vuex';
import RoleDropdown from '@../../../common/vueapp/RoleDropdown';
import { iTypes } from '@/utils/iTypes';

export default {
  name: 'OverviewCardForm',
  components: {
    ReorderList,
    RoleDropdown
  },
  props: {
    overview: {
      type: Object
    },
    modifiedOverview: {
      type: Object,
      required: true
    }
  },
  data () {
    return {
      localOverview: JSON.parse(JSON.stringify(this.modifiedOverview)),
      changesMade: false,
      rawEditMode: false,
      confirmDelete: false,
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
      }
    };
  },
  computed: {
    ...mapGetters(['getIntegrations', 'getRoles', 'getUser']),
    isDefaultOverview () {
      return iTypes.includes(this.localOverview._id);
    }
  },
  methods: {
    normalizeOverview (unNormalizedOverview) {
      const normalizedOverview = JSON.parse(JSON.stringify(unNormalizedOverview));
      // sort roles, as their order should not matter
      normalizedOverview.viewRoles.sort();
      normalizedOverview.editRoles.sort();

      return normalizedOverview;
    },
    updateViewRoles (roles) {
      this.$set(this.localOverview, 'viewRoles', roles);
      this.updateOverview();
    },
    updateEditRoles (roles) {
      this.$set(this.localOverview, 'editRoles', roles);
      this.updateOverview();
    },
    updateOverview () {
      const normalizedFinal = this.normalizeOverview(this.localOverview);

      if (!this.overview) {
        this.changesMade = true;
      } else {
        const normalizedInitial = this.normalizeOverview(this.overview);
        this.changesMade = JSON.stringify(normalizedInitial) !== JSON.stringify(normalizedFinal);
      }

      this.$emit('update-modified-overview', JSON.parse(JSON.stringify(normalizedFinal)));
    },
    saveOverview () {
      this.$emit('save-overview');
      this.changesMade = false;
    },
    deleteOverview () {
      this.$emit('delete-overview');
    },
    cancelOverviewModification () {
      this.localOverview = JSON.parse(JSON.stringify(this.overview));
      this.updateOverview();
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
    }
  }
};
</script>
