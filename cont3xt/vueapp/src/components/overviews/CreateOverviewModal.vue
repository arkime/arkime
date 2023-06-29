<template>
  <b-modal
      size="xl"
      scrollable
      id="overview-form">
    <!-- header -->
    <template #modal-title>
      <h4 class="mb-0">
        Create New Overview
      </h4>
    </template> <!-- /header -->
    <!-- form -->
    <overview-form
        :modifiedOverview="overview"
        :raw-edit-mode="rawEditMode"
        @update-modified-overview="updateOverview"
    /> <!-- /form -->
    <!-- footer -->
    <template #modal-footer>
      <div class="w-100 d-flex justify-content-between align-items-start">
        <b-button
            @click="close"
            variant="warning">
          Cancel
        </b-button>
        <b-alert
            variant="danger"
            :show="!!error.length"
            class="mb-0 alert-sm mr-1 ml-1">
          {{ error }}
        </b-alert>
        <b-button
            variant="warning"
            @click="rawEditMode = !rawEditMode"
            v-b-tooltip.hover="'Edit the raw config for this link group'">
          <span class="fa fa-pencil-square-o" />
        </b-button>
        <b-button
            @click="create"
            variant="success">
          Create
        </b-button>
      </div>
    </template> <!-- /footer -->
  </b-modal>
</template>

<script>

import OverviewForm from './OverviewForm.vue';
import OverviewService from '../services/OverviewService';

const defaultOverview = {
  name: '',
  title: 'Overview of %{query}',
  iType: '',
  fields: [],
  viewRoles: [],
  editRoles: []
};

export default {
  name: 'CreateOverviewModal',
  components: { OverviewForm },
  data () {
    return {
      error: '',
      overview: defaultOverview,
      rawEditMode: false
    };
  },
  mounted () {
    // reset fields when hidden
    this.$root.$on('bv::modal::hide', () => {
      this.error = '';
      this.overview = defaultOverview;
      this.rawEditMode = false;
    });
  },
  methods: {
    updateOverview (updatedOverview) {
      this.overview = updatedOverview;
    },
    create () {
      OverviewService.createOverview(this.overview).then(() => {
        this.close();
      }).catch(error => {
        this.error = error.text || error;
      });
    },
    close () {
      this.$bvModal.hide('overview-form');
    }
  }
};
</script>
