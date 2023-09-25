<template>
  <!-- view (for settings page users who can view but not edit) -->
  <b-card
      v-if="!(getUser && (getUser.userId === localOverview.creator || localOverview._editable || (getUser.roles && getUser.roles.includes('cont3xtAdmin'))))"
      class="w-100">
    <template #header>
      <h6 class="mb-0 d-flex justify-content-between">
        <div class="overview-header">
          <span
              class="fa fa-share-alt mr-1 cursor-help"
              v-b-tooltip.hover="`Shared with you by ${localOverview.creator}`"
          />
          {{ localOverview.name }}
        </div>
        <b-button
            size="sm"
            variant="outline-success"
            :disabled="isSetAsDefault"
            @click="setAsDefaultOverview">
              <span v-if="isSetAsDefault">
                Default for {{ overview.iType }} iType <span class="fa fa-fw fa-check"/>
              </span>
          <span v-else>
                Set as default for {{ overview.iType }} iType
              </span>
        </b-button>
        <div>
          <small>
            You can only view this Overview
          </small>
          <b-button
              class="ml-1"
              size="sm"
              variant="secondary"
              @click="rawEditMode = !rawEditMode"
              v-b-tooltip.hover="`View ${rawEditMode ? 'form' : 'raw'} configuration for this overview`">
            <span class="fa fa-fw" :class="{'fa-file-text-o': rawEditMode, 'fa-pencil-square-o': !rawEditMode}" />
          </b-button>
        </div>
      </h6>
    </template>
    <b-card-body>
      <template v-if="!rawEditMode">
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
            <span class="text-warning bold">{{ field.from }}</span>
            <template v-if="field.type === 'custom'">
              <span class="text-primary">Custom</span>:<span class="text-info">"{{ normalizeCardField(field.custom).label }}"</span>
            </template>
            <template v-else>
              <span class="text-primary">{{ field.field }}</span>
              <span v-if="field.alias">as <span class="text-info">"{{ field.alias }}"</span></span>
            </template>
          </b-card>
        </div>
      </template>
      <overview-form
          v-else
          :no-edit="true"
          :modifiedOverview="localOverview"
          :raw-edit-mode="rawEditMode"
          :is-default-overview="isDefaultOverview"
          @update-modified-overview="updateOverview"
      />
    </b-card-body>
  </b-card> <!-- /view -->
  <!-- edit -->
  <b-card v-else class="w-100">
    <template #header>
      <div class="w-100 d-flex justify-content-between">
        <div>
          <!-- transfer button -->
          <b-button
            size="sm"
            variant="info"
            v-b-tooltip.hover
            v-if="canTransfer(localOverview) && !isDefaultOverview"
            title="Transfer ownership of this link group"
            @click="$emit('open-transfer-resource', localOverview)">
            <span class="fa fa-share fa-fw" />
          </b-button> <!-- /transfer button -->
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
                v-if="confirmDelete && !isDefaultOverview"
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
                v-if="confirmDelete && !isDefaultOverview"
                @click="deleteOverview">
              <span class="fa fa-check" />
            </b-button>
          </transition> <!-- /confirm delete button -->
        </div>
        <b-button
            size="sm"
            variant="outline-success"
            :disabled="isSetAsDefault"
            @click="setAsDefaultOverview">
              <span v-if="isSetAsDefault">
                Default for {{ overview.iType }} iType <span class="fa fa-fw fa-check"/>
              </span>
          <span v-else>
                Set as default for {{ overview.iType }} iType
              </span>
        </b-button>
        <div>
          <transition name="buttons">
            <b-button
                size="sm"
                variant="secondary"
                @click="rawEditMode = !rawEditMode"
                v-b-tooltip.hover="`Edit ${rawEditMode ? 'form' : 'raw'} configuration for this overview`">
              <span class="fa fa-fw" :class="{'fa-file-text-o': rawEditMode, 'fa-pencil-square-o': !rawEditMode}" />
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
                v-b-tooltip.hover="'Save this overview'">
              <span class="fa fa-save" />
            </b-button>
          </transition>
        </div>
      </div>
    </template>
    <overview-form
      :modifiedOverview="localOverview"
      :raw-edit-mode="rawEditMode"
      :is-default-overview="isDefaultOverview"
      @update-modified-overview="updateOverview"
    />
  </b-card> <!-- /edit -->

</template>

<script>
import { iTypes } from '@/utils/iTypes';
import { mapGetters } from 'vuex';
import OverviewService from '@/components/services/OverviewService';
import UserService from '@/components/services/UserService';
import OverviewForm from '@/components/overviews/OverviewForm';
import normalizeCardField from '../../../../normalizeCardField';

export default {
  name: 'OverviewFormCard',
  components: {
    OverviewForm
  },
  props: {
    overview: {
      type: Object,
      required: true
    },
    modifiedOverview: {
      type: Object,
      required: true
    }
  },
  data () {
    return {
      localOverview: JSON.parse(JSON.stringify(this.modifiedOverview)),
      confirmDelete: false,
      rawEditMode: false
    };
  },
  computed: {
    ...mapGetters(['getUser', 'getCorrectedSelectedOverviewIdMap']),
    isDefaultOverview () {
      return iTypes.includes(this.localOverview._id);
    },
    changesMade () {
      const normalizedInitial = this.normalizeOverview(this.overview);
      const normalizedFinal = this.normalizeOverview(this.localOverview);

      return JSON.stringify(normalizedInitial) !== JSON.stringify(normalizedFinal);
    },
    isSetAsDefault () {
      return this.getCorrectedSelectedOverviewIdMap[this.overview.iType] === this.overview._id;
    }
  },
  methods: {
    canTransfer (overview) {
      return this.getUser.roles.includes('cont3xtAdmin') ||
        (overview.creator && overview.creator === this.getUser.userId);
    },
    normalizeCardField,
    normalizeOverview (unNormalizedOverview) {
      const normalizedOverview = JSON.parse(JSON.stringify(unNormalizedOverview));
      // sort roles, as their order should not matter
      normalizedOverview.viewRoles.sort();
      normalizedOverview.editRoles.sort();

      normalizedOverview.fields ??= [];
      for (const field of normalizedOverview.fields) { // delete temporary field properties
        delete field.expanded;
        delete field._customRawEdit;
      }

      return normalizedOverview;
    },
    updateOverview (updatedOverview) {
      this.localOverview = JSON.parse(JSON.stringify(updatedOverview));

      this.$emit('update-modified-overview', this.normalizeOverview(updatedOverview));
    },
    saveOverview () {
      OverviewService.updateOverview(this.localOverview);
    },
    deleteOverview () {
      OverviewService.deleteOverview(this.overview._id).then(() => {
        this.$emit('overview-deleted');
      });
    },
    cancelOverviewModification () {
      this.localOverview = JSON.parse(JSON.stringify(this.overview));
      this.updateOverview(this.localOverview);
    },
    setAsDefaultOverview () {
      this.$store.commit('SET_SELECTED_OVERVIEW_ID_FOR_ITYPE',
        { iType: this.overview.iType, id: this.overview._id });
      UserService.setUserSettings({ selectedOverviews: this.getCorrectedSelectedOverviewIdMap });
    }
  }
};
</script>

<style scoped>
.overview-header {
  overflow: hidden;
  white-space: nowrap;
  text-overflow: ellipsis;
  max-width: 33%;
}
</style>
