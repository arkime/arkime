<template>
  <b-card class="w-100">
    <template #header>
      <div class="w-100 d-flex justify-content-between">
        <div>
          <!-- delete button -->
          <transition name="buttons">
            <b-button
                size="sm"
                variant="danger"
                v-if="!confirmDelete"
                @click="confirmDelete = true"
                v-b-tooltip.hover="'Delete this link group'">
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
          Card Title
        </b-input-group-text>
      </template>
      <b-form-input
          trim
          required
          autofocus
          v-model="localOverviewCard.title"
          :state="!!localOverviewCard.title"
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

    <reorder-list
        v-for="(fieldRef, i) in localOverviewCard.fields"
        :key="i"
        :index="i"
        :list="localOverviewCard.fields"
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

export default {
  name: 'OverviewCardForm',
  components: {
    ReorderList
  },
  props: {
    overviewCard: {
      type: Object
    },
    modifiedOverviewCard: {
      type: Object,
      required: true
    }
  },
  data () {
    return {
      localOverviewCard: JSON.parse(JSON.stringify(this.modifiedOverviewCard)),
      changesMade: false,
      rawEditMode: false,
      confirmDelete: false,
      titleTip: {
        title: 'Set the title to display for this card. <code>%{query}</code> will be replaced by the queried indicator.'
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
    ...mapGetters(['getIntegrations'])
  },
  methods: {
    normalizeOverviewCard (unNormalizedOverviewCard) {
      const normalizedFields = (unNormalizedOverviewCard.fields || []).map(fieldRef => {
        const { from, field, alias } = fieldRef;
        const normalizedFieldRef = { from, field };
        if (alias) { Object.assign(normalizedFieldRef, { alias }); }
        return normalizedFieldRef;
      });

      return {
        title: unNormalizedOverviewCard.title || '',
        fields: normalizedFields
      };
    },
    updateOverview () {
      if (!this.overviewCard) {
        this.changesMade = true;
      } else {
        const normalizedInitial = this.normalizeOverviewCard(this.overviewCard);
        const normalizedFinal = this.normalizeOverviewCard(this.localOverviewCard);
        this.changesMade = JSON.stringify(normalizedInitial) !== JSON.stringify(normalizedFinal);
      }

      this.$emit('update-modified-overview', this.localOverviewCard);
    },
    saveOverview () {
      this.$emit('save-overview');
      this.changesMade = false;
    },
    deleteOverview () {
      this.$emit('delete-overview');
    },
    cancelOverviewModification () {
      this.localOverviewCard = JSON.parse(JSON.stringify(this.overviewCard));
      this.updateOverview();
    },
    insertFieldRef (index) {
      this.localOverviewCard.fields.splice(index, 0, {
        from: '', field: ''
      });
      this.updateOverview();
    },
    deleteFieldRef (index) {
      this.localOverviewCard.fields.splice(index, 1);
      this.updateOverview();
    },
    appendFieldRef () {
      this.insertFieldRef(this.localOverviewCard.fields.length);
    },
    moveFieldRef (fromIndex, toIndex) {
      const fieldRef = this.localOverviewCard.fields[fromIndex];
      this.localOverviewCard.fields.splice(fromIndex, 1);
      this.localOverviewCard.fields.splice(toIndex, 0, fieldRef);
      this.updateOverview();
    },
    sendToTop (fromIndex) {
      this.moveFieldRef(fromIndex, 0);
    },
    sendToBottom (fromIndex) {
      this.moveFieldRef(fromIndex, this.localOverviewCard.fields.length - 1);
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
      this.localOverviewCard.fields = list;
      this.updateOverview();
    }
  }
};
</script>
