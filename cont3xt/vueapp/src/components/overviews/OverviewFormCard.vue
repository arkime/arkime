<!--
Copyright Yahoo Inc.
SPDX-License-Identifier: Apache-2.0
-->
<template>
  <!-- view (for settings page users who can view but not edit) -->
  <v-card
      v-if="!(getUser && (getUser.userId === localOverview.creator || localOverview._editable || (getUser.roles && getUser.roles.includes('cont3xtAdmin'))))"
      variant="tonal"
      class="w-100">
    <template #title>
      <h6 class="mb-0 d-flex justify-space-between">
        <div class="overview-header">
          <span
              class="fa fa-share-alt mr-1 cursor-help"
              v-tooltip="`Shared with you by ${localOverview.creator}`"
          />
          {{ localOverview.name }}
        </div>
        <v-btn
            size="small"
            color="success"
            variant="outlined"
            :disabled="isSetAsDefault"
            @click="setAsDefaultOverview">
              <span v-if="isSetAsDefault">
                Default for {{ overview.iType }} iType <span class="fa fa-fw fa-check"/>
              </span>
          <span v-else>
                Set as default for {{ overview.iType }} iType
              </span>
        </v-btn>
        <div>
          <small>
            You can only view this Overview
          </small>
          <v-btn
              class="ml-1"
              size="small"
              color="secondary"
              @click="rawEditMode = !rawEditMode"
              v-tooltip="`View ${rawEditMode ? 'form' : 'raw'} configuration for this overview`">
            <span class="fa fa-fw" :class="{'fa-file-text-o': rawEditMode, 'fa-pencil-square-o': !rawEditMode}" />
          </v-btn>
        </div>
      </h6>
    </template>
    <v-card-text>
      <template v-if="!rawEditMode">
        <div class="d-flex flex-row">
          <h6>Title:</h6><span class="ml-1">{{ localOverview.title }}</span>
        </div>
        <div class="d-flex flex-row">
          <h6>iType:</h6><span class="ml-1">{{ localOverview.iType }}</span>
        </div>
        <div class="d-flex flex-row">
          <h6>Fields:</h6><c3-badge v-if="!localOverview.fields.length" class="ml-1">None</c3-badge>
        </div>
        <div class="d-flex flex-column">
          <v-card v-for="(field, i) in localOverview.fields" :key="i"
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
          </v-card>
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
    </v-card-text>
  </v-card> <!-- /view -->
  <!-- edit -->
  <v-card v-else variant="tonal" class="w-100">
    <template #title>
      <div class="w-100 d-flex justify-space-between align-center">
        <div class="d-flex ga-1">
          <!-- transfer button -->
          <v-btn
            size="small"
            color="info"
            v-tooltip="'Transfer ownership of this link group'"
            title="Transfer ownership of this link group"
            v-if="canTransfer(localOverview) && !isDefaultOverview"
            @click="$emit('open-transfer-resource', localOverview)">
            <span class="fa fa-share fa-fw" />
          </v-btn> <!-- /transfer button -->
          <!-- delete button -->
          <transition name="buttons">
            <v-btn
                size="small"
                color="error"
                v-if="!confirmDelete && !isDefaultOverview"
                @click="confirmDelete = true"
                v-tooltip="'Delete this overview'">
              <span class="fa fa-trash" />
            </v-btn>
          </transition> <!-- /delete button -->
          <!-- cancel confirm delete button -->
          <transition name="buttons">
            <v-btn
                size="small"
                color="warning"
                v-tooltip="'Cancel'"
                title="Cancel"
                :disabled="isDefaultOverview"
                v-if="confirmDelete && !isDefaultOverview"
                @click="confirmDelete = false">
              <span class="fa fa-ban" />
            </v-btn>
          </transition> <!-- /cancel confirm delete button -->
          <!-- confirm delete button -->
          <transition name="buttons">
            <v-btn
                size="small"
                color="error"
                v-tooltip="'Are you sure?'"
                title="Are you sure?"
                :disabled="isDefaultOverview"
                v-if="confirmDelete && !isDefaultOverview"
                @click="deleteOverview">
              <span class="fa fa-check" />
            </v-btn>
          </transition> <!-- /confirm delete button -->
        </div>
        <v-btn
            size="small"
            color="success"
            variant="outlined"
            :disabled="isSetAsDefault"
            @click="setAsDefaultOverview">
              <span v-if="isSetAsDefault">
                Default for {{ overview.iType }} iType <span class="fa fa-fw fa-check"/>
              </span>
          <span v-else>
                Set as default for {{ overview.iType }} iType
              </span>
        </v-btn>
        <div class="d-flex ga-1">
          <transition name="buttons">
            <v-btn
                size="small"
                color="secondary"
                @click="rawEditMode = !rawEditMode"
                v-tooltip="`Edit ${rawEditMode ? 'form' : 'raw'} configuration for this overview`">
              <span class="fa fa-fw" :class="{'fa-file-text-o': rawEditMode, 'fa-pencil-square-o': !rawEditMode}" />
            </v-btn>
          </transition>
          <transition name="buttons">
            <v-btn
                size="small"
                color="warning"
                v-if="changesMade"
                @click="cancelOverviewModification"
                v-tooltip="'Cancel unsaved updates'">
              <span class="fa fa-ban" />
            </v-btn>
          </transition>
          <transition name="buttons">
            <v-btn
                size="small"
                color="success"
                v-if="changesMade"
                @click="saveOverview"
                v-tooltip="'Save this overview'">
              <span class="fa fa-save" />
            </v-btn>
          </transition>
        </div>
      </div>
    </template>
    <v-card-text class="d-flex flex-column">
      <overview-form
        class="pt-2"
        :modifiedOverview="localOverview"
        :raw-edit-mode="rawEditMode"
        :is-default-overview="isDefaultOverview"
        @update-modified-overview="updateOverview"
      />
    </v-card-text>
  </v-card> <!-- /edit -->
</template>

<script>
import { iTypes } from '@/utils/iTypes';
import { mapGetters } from 'vuex';
import OverviewService from '@/components/services/OverviewService';
import UserService from '@/components/services/UserService';
import OverviewForm from '@/components/overviews/OverviewForm.vue';
import { normalizeCardField } from '@/badcopy/normalizeCardField.js';

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
