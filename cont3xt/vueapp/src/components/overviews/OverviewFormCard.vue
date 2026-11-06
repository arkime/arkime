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
          <v-icon
            icon="mdi-share"
            class="me-1 cursor-help"
            v-tooltip="$t('common.sharedTip', { creator: localOverview.creator })" />
          {{ localOverview.name }}
        </div>
        <v-btn
          size="small"
          color="success"
          variant="outlined"
          :disabled="isSetAsDefault"
          @click="setAsDefaultOverview">
          <span v-if="isSetAsDefault">
            {{ $t('cont3xt.overviews.defaultForItype', { itype: overview.iType }) }} <v-icon icon="mdi-check-bold" />
          </span>
          <span v-else>
            {{ $t('cont3xt.overviews.setDefaultForItype', { itype: overview.iType }) }}
          </span>
        </v-btn>
        <div>
          <small>
            {{ $t('cont3xt.overviews.viewOnly') }}
          </small>
          <v-btn
            class="ms-1"
            size="small"
            color="secondary"
            @click="rawEditMode = !rawEditMode"
            v-tooltip="rawEditMode ? $t('cont3xt.overviews.viewFormTip') : $t('cont3xt.overviews.viewRawTip')">
            <v-icon :icon="`${rawEditMode ? 'mdi-list-box' : 'mdi-text-box'} mdi-fw`" />
          </v-btn>
        </div>
      </h6>
    </template>
    <v-card-text>
      <template v-if="!rawEditMode">
        <div class="d-flex flex-row align-center">
          <h6>{{ $t('cont3xt.overviews.titleLabel') }}</h6>
          <span class="ms-1">{{ localOverview.title }}</span>
        </div>
        <div class="d-flex flex-row align-center">
          <h6>{{ $t('cont3xt.overviews.itypeLabel') }}</h6>
          <span class="ms-1">{{ localOverview.iType }}</span>
        </div>
        <div class="d-flex flex-row align-center">
          <h6>{{ $t('cont3xt.overviews.fieldsLabel') }}</h6>
          <c3-badge
            v-if="!localOverview.fields.length"
            class="ms-1">
            {{ $t('common.none') }}
          </c3-badge>
        </div>
        <div class="d-flex flex-column">
          <v-card
            v-for="(field, i) in localOverview.fields"
            :key="i"
            class="mb-1 ps-2 pe-2 pt-1 pb-1">
            <span class="text-warning bold">{{ field.from }}&nbsp;</span>
            <template v-if="field.type === 'custom'">
              <span class="text-primary">{{ $t('common.custom') }}</span>:<span class="text-info">"{{ normalizeCardField(field.custom).label }}"</span>
            </template>
            <template v-else>
              <span class="text-primary">{{ field.field }}</span>
              <span v-if="field.alias">&nbsp;{{ $t('cont3xt.overviews.alias') }}&nbsp;<span class="text-info">"{{ field.alias }}"</span></span>
            </template>
          </v-card>
        </div>
      </template>
      <overview-form
        v-else
        :no-edit="true"
        :modified-overview="localOverview"
        :raw-edit-mode="rawEditMode"
        :is-default-overview="isDefaultOverview"
        @update-modified-overview="updateOverview" />
    </v-card-text>
  </v-card> <!-- /view -->
  <!-- edit -->
  <v-card
    v-else
    variant="tonal"
    class="w-100">
    <template #title>
      <div class="w-100 d-flex justify-space-between align-center">
        <div class="d-flex ga-1">
          <!-- transfer button -->
          <v-btn
            size="small"
            color="info"
            v-tooltip="$t('cont3xt.overviews.transferTip')"
            :title="$t('cont3xt.overviews.transferTip')"
            v-if="canTransfer(localOverview) && !isDefaultOverview"
            @click="$emit('open-transfer-resource', localOverview)">
            <v-icon icon="mdi-share mdi-fw" />
          </v-btn> <!-- /transfer button -->
          <!-- delete button -->
          <transition name="buttons">
            <v-btn
              size="small"
              color="error"
              v-if="!confirmDelete && !isDefaultOverview"
              @click="confirmDelete = true"
              v-tooltip="$t('cont3xt.overviews.deleteTip')">
              <v-icon icon="mdi-trash-can mdi-fw" />
            </v-btn>
          </transition> <!-- /delete button -->
          <!-- cancel confirm delete button -->
          <transition name="buttons">
            <v-btn
              size="small"
              color="warning"
              v-tooltip="$t('common.cancel')"
              :title="$t('common.cancel')"
              :disabled="isDefaultOverview"
              v-if="confirmDelete && !isDefaultOverview"
              @click="confirmDelete = false">
              <v-icon icon="mdi-cancel mdi-fw" />
            </v-btn>
          </transition> <!-- /cancel confirm delete button -->
          <!-- confirm delete button -->
          <transition name="buttons">
            <v-btn
              size="small"
              color="error"
              v-tooltip="$t('common.areYouSure')"
              :title="$t('common.areYouSure')"
              :disabled="isDefaultOverview"
              v-if="confirmDelete && !isDefaultOverview"
              @click="deleteOverview">
              <v-icon icon="mdi-check-bold mdi-fw" />
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
            {{ $t('cont3xt.overviews.defaultForItype', { itype: overview.iType }) }} <v-icon icon="mdi-check-bold" />
          </span>
          <span v-else>
            {{ $t('cont3xt.overviews.setDefaultForItype', { itype: overview.iType }) }}
          </span>
        </v-btn>
        <div class="d-flex ga-1">
          <transition name="buttons">
            <v-btn
              size="small"
              color="secondary"
              @click="rawEditMode = !rawEditMode"
              v-tooltip="rawEditMode ? $t('cont3xt.overviews.editFormTip') : $t('cont3xt.overviews.editRawTip')">
              <v-icon :icon="`${rawEditMode ? 'mdi-list-box' : 'mdi-text-box'} mdi-fw`" />
            </v-btn>
          </transition>
          <transition name="buttons">
            <v-btn
              size="small"
              color="warning"
              v-if="changesMade"
              @click="cancelOverviewModification"
              v-tooltip="$t('cont3xt.overviews.cancelChangesTip')">
              <v-icon icon="mdi-cancel mdi-fw" />
            </v-btn>
          </transition>
          <transition name="buttons">
            <v-btn
              size="small"
              color="success"
              v-if="changesMade"
              @click="saveOverview"
              v-tooltip="$t('cont3xt.overviews.saveTip')">
              <v-icon icon="mdi-content-save mdi-fw" />
            </v-btn>
          </transition>
        </div>
      </div>
    </template>
    <v-card-text class="d-flex flex-column">
      <overview-form
        class="pt-2"
        :modified-overview="localOverview"
        :raw-edit-mode="rawEditMode"
        :is-default-overview="isDefaultOverview"
        @update-modified-overview="updateOverview" />
    </v-card-text>
  </v-card> <!-- /edit -->
</template>

<script>
import { iTypes } from '@/utils/iTypes';
import { mapGetters } from 'vuex';
import OverviewService from '@/components/services/OverviewService';
import UserService from '@/components/services/UserService';
import OverviewForm from '@/components/overviews/OverviewForm.vue';
import { normalizeCardField } from '@/utils/normalizeCardField.js';

export default {
  name: 'OverviewFormCard',
  emits: ['open-transfer-resource', 'update-modified-overview', 'overview-deleted'],
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
