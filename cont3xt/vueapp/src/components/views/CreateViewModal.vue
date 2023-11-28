<!--
Copyright Yahoo Inc.
SPDX-License-Identifier: Apache-2.0
-->
<template>
  <b-modal
    size="xl"
    scrollable
    id="view-form">
    <!-- header -->
    <template #modal-title>
      <h4 class="mb-0">
        Create New View
      </h4>
    </template> <!-- /header -->
    <!-- form -->
    <ViewForm
      :view="view"
      :focus="focus"
      @update-view="update"
    />
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
          @click="create"
          variant="success">
          Create
        </b-button>
      </div>
    </template> <!-- /footer -->
  </b-modal>
</template>

<script>
import ViewForm from '@/components/views/ViewForm';
import UserService from '@/components/services/UserService';

const defaultView = {
  name: '',
  editRoles: [],
  viewRoles: [],
  integrations: []
};

export default {
  name: 'CreateViewModal',
  components: { ViewForm },
  data () {
    return {
      error: '',
      focus: false,
      view: defaultView
    };
  },
  mounted () {
    this.$root.$on('bv::modal::show', (bvEvent, modalId) => {
      setTimeout(() => { this.focus = true; }, 200);
    });
  },
  methods: {
    /* page functions ------------------------------------------------------ */
    update (view) {
      this.view = view;
    },
    close () {
      this.error = '';
      this.focus = false;
      this.$set(this.view, 'name', '');
      this.$set(this.view, 'editRoles', []);
      this.$set(this.view, 'viewRoles', []);
      this.$set(this.view, 'integrations', []);
      this.$bvModal.hide('view-form');
    },
    create () {
      this.error = '';

      if (!this.view.name) {
        this.error = 'Name required';
        return;
      }

      // NOTE: this function handles fetching the updated view list and storing it
      UserService.saveIntegrationsView(this.view).then((response) => {
        this.close();
      }).catch((error) => {
        this.error = error.text || error;
      });
    }
  }
};
</script>

<style scoped>
.alert.alert-sm {
  padding: 0.4rem 0.8rem;
}
</style>
