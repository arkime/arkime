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
      <div class="w-100 d-flex justify-space-between align-start">
        <v-btn
          @click="close"
          color="warning">
          Cancel
        </v-btn>
        <v-alert
          color="error"
          v-if="!!error.length"
          class="mb-0 alert-sm mr-1 ml-1">
          {{ error }}
        </v-alert>
        <v-btn
          @click="create"
          color="success">
          Create
        </v-btn>
      </div>
    </template> <!-- /footer -->
  </b-modal>
</template>

<script>
import ViewForm from '@/components/views/ViewForm.vue';
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
    // TODO: toby
    // this.$root.$on('bv::modal::show', (bvEvent, modalId) => {
    //   setTimeout(() => { this.focus = true; }, 200);
    // });
  },
  methods: {
    /* page functions ------------------------------------------------------ */
    update (view) {
      this.view = view;
    },
    close () {
      this.error = '';
      this.focus = false;
      this.view.name = '';
      this.view.editRoles = [];
      this.view.viewRoles = [];
      this.view.integrations = [];
      this.$root.$emit('bv::hide::modal', 'view-form');
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
