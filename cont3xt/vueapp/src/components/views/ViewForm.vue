<!--
Copyright Yahoo Inc.
SPDX-License-Identifier: Apache-2.0
-->
<template>
  <v-form>
    <!-- view name -->
    <!-- TODO: toby required??? input? -->
    <v-text-field
      label="Name"
      v-model.trim="localView.name"
      v-focus="focus"
      required
      :rules="[localView.name.length > 0]"
      @keydown.enter.stop.prevent
      @update:model-value="val => $emit('update-view', { ...localView, name: val })"
    />
    <div class="my-1">
      <RoleDropdown
        class="mr-1"
        :roles="getRoles"
        display-text="Who Can View"
        :selected-roles="localView.viewRoles"
        @selected-roles-updated="updateViewRoles"
      />
      <RoleDropdown
        :roles="getRoles"
        display-text="Who Can Edit"
        :selected-roles="localView.editRoles"
        @selected-roles-updated="updateEditRoles"
      />
      <span
        class="fa fa-info-circle fa-lg cursor-help ml-2 mr-1"
        v-tooltip="'Creators will always be able to view and edit their views regardless of the roles selected here.'"
      />
      <span v-if="!localView.creator">
        As the creator, you can always view and edit your views.
      </span>
      </div>
    <!-- selected integrations -->
    <div>
      <v-checkbox
        class="mt-2"
        tabindex="-1"
        @click="toggleAll"
        :model-value="allSelected"
        color="secondary"
        :indeterminate="indeterminate"
      >
        <template #label><strong>Select All</strong></template>
      </v-checkbox>
      <div class="wrap-checkboxes">
        <template v-for="integration in getSortedIntegrations" :key="integration.key">
          <v-checkbox
            v-if="integration.doable"
            class="custom-checkbox"
            v-model="localView.integrations"
            @update:model-value="val => $emit('update-view', { ...localView, integrations: val })"
            :value="integration.key"
            :label="integration.key"
            color="secondary"
          />
        </template>
      </div>
      <v-checkbox
        @click="toggleAll"
        :model-value="allSelected"
        color="secondary"
        :indeterminate="indeterminate"
      >
        <template #label><strong>Select All</strong></template>
      </v-checkbox>
    </div> <!-- /selected integrations -->
    <!-- /group roles -->
    <div
      class="mt-2"
      v-if="localView.creator">
      Created by
      <span class="text-info">
        {{ localView.creator }}
      </span>
      <span class="fa fa-question-circle ml-2 cursor-help"
        v-if="!localView.creator || (getUser && localView.creator === getUser.userId)"
        v-tooltip="'As the creator, you can always view and edit your views.'"
      />
    </div>
  </v-form>
</template>

<script>
import { mapGetters } from 'vuex';

import Focus from '@common/Focus.vue';
import RoleDropdown from '@common/RoleDropdown.vue';

export default {
  name: 'ViewForm',
  directives: { Focus },
  components: { RoleDropdown },
  props: {
    view: {
      type: Object,
      required: true
    },
    focus: {
      type: Boolean
    }
  },
  data () {
    return {
      localView: this.view
    };
  },
  created () {
    if (!this.localView?.integrations?.length) {
      this.localView.integrations = this.getSelectedIntegrations;
    }
  },
  watch: {
    view (newVal) {
      this.localView = JSON.parse(JSON.stringify(newVal));
    }
  },
  computed: {
    ...mapGetters([
      'getRoles', 'getUser', 'getDoableIntegrations', 'getSortedIntegrations',
      'getSelectedIntegrations'
    ]),
    allSelected () {
      return this.localView?.integrations?.length >= Object.keys(this.getDoableIntegrations).length;
    },
    indeterminate () {
      const integrationsLength = this.localView?.integrations?.length;
      return integrationsLength !== 0 && integrationsLength <= Object.keys(this.getDoableIntegrations).length;
    }
  },
  methods: {
    /* page functions ------------------------------------------------------ */
    toggleAll (checked) {
      // TODO: toby
      this.localView.integrations = !this.allSelected ? Object.keys(this.getDoableIntegrations) : [];
      this.$emit('update-view', this.localView);
    },
    updateViewRoles (roles) {
      this.localView.viewRoles = roles;
      this.$emit('update-view', this.localView);
    },
    updateEditRoles (roles) {
      this.localView.editRoles = roles;
      this.$emit('update-view', this.localView);
    }
  }
};
</script>

<style>
.wrap-checkboxes {
  display: flex;
  flex-wrap: wrap;
}
.wrap-checkboxes .custom-checkbox {
  flex-basis: 150px;
}
</style>
