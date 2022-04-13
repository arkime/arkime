<template>
  <b-form>
    <!-- view name -->
    <b-input-group
      size="sm"
      class="mb-2">
      <template #prepend>
        <b-input-group-text>
          Name
        </b-input-group-text>
      </template>
      <b-form-input
        trim
        required
        v-focus="focus"
        v-model="localView.name"
        @keydown.enter.stop.prevent
        :state="localView.name.length > 0"
        @change="$emit('update-view', { view: localView, index: viewIndex })"
      />
    </b-input-group> <!-- /view name -->
    <!-- group roles -->
    <b-dropdown
      size="sm"
      text="Who Can View"
      class="roles-dropdown mb-2">
      <b-dropdown-form>
        <b-form-checkbox-group
          v-model="localView.viewRoles"
          @change="$emit('update-view', { view: localView, index: viewIndex })">
          <b-form-checkbox
            v-for="role in getRoles"
            :value="role.value"
            :key="role.value">
            {{ role.text }}
            <span
              v-if="role.userDefined"
              class="fa fa-user cursor-help ml-2"
              v-b-tooltip.hover="'User defined role'"
            />
          </b-form-checkbox>
          <template v-for="role in localView.viewRoles">
            <b-form-checkbox
              :key="role"
              :value="role"
              v-if="!getRoles.find(r => r.value === role)">
              {{ role }}
              <span
                class="fa fa-times-circle cursor-help ml-2"
                v-b-tooltip.hover="'This role no longer exists'"
              />
            </b-form-checkbox>
          </template>
        </b-form-checkbox-group>
      </b-dropdown-form>
    </b-dropdown>
    <b-dropdown
      size="sm"
      text="Who Can Edit"
      class="mb-2 roles-dropdown">
      <b-dropdown-form>
        <b-form-checkbox-group
          v-model="localView.editRoles"
          @change="$emit('update-view', { view: localView, index: viewIndex })">
          <b-form-checkbox
            v-for="role in getRoles"
            :value="role.value"
            :key="role.value">
            {{ role.text }}
            <span
              v-if="role.userDefined"
              class="fa fa-user cursor-help ml-2"
              v-b-tooltip.hover="'User defined role'"
            />
          </b-form-checkbox>
          <template v-for="role in localView.editRoles">
            <b-form-checkbox
              :key="role"
              :value="role"
              v-if="!getRoles.find(r => r.value === role)">
              {{ role }}
              <span
                class="fa fa-times-circle cursor-help ml-2"
                v-b-tooltip.hover="'This role no longer exists'"
              />
            </b-form-checkbox>
          </template>
        </b-form-checkbox-group>
      </b-dropdown-form>
    </b-dropdown>
    <span
      class="fa fa-info-circle fa-lg cursor-help ml-2 mr-1"
      v-b-tooltip.hover="'Creators will always be able to view and edit their views regardless of the roles selected here.'"
    />
    <span v-if="!localView.creator">
      As the creator, you can always view and edit your views.
    </span>
    <!-- selected integrations -->
    <div>
      <b-form-checkbox
        role="checkbox"
        @change="toggleAll"
        v-model="allSelected"
        :indeterminate="indeterminate">
        <strong>Select All</strong>
      </b-form-checkbox>
      <b-form-checkbox-group
        class="wrap-checkboxes"
        v-model="localView.integrations"
        @change="$emit('update-view', { view: localView, index: viewIndex })">
        <template
          v-for="integration in getSortedIntegrations">
          <b-form-checkbox
            :key="integration.key"
            :value="integration.key"
            v-if="integration.doable">
            {{ integration.key }}
          </b-form-checkbox>
        </template>
      </b-form-checkbox-group>
      <b-form-checkbox
        role="checkbox"
        @change="toggleAll"
        v-model="allSelected"
        :indeterminate="indeterminate">
        <strong>Select All</strong>
      </b-form-checkbox>
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
        v-b-tooltip.hover="'As the creator, you can always view and edit your views.'"
      />
    </div>
  </b-form>
</template>

<script>
import { mapGetters } from 'vuex';

import Focus from '@/utils/Focus';

export default {
  name: 'ViewForm',
  directives: { Focus },
  props: {
    view: {
      type: Object,
      required: true
    },
    viewIndex: {
      type: Number
    },
    focus: {
      type: Boolean
    }
  },
  data () {
    return {
      localView: this.view,
      allSelected: false,
      indeterminate: false
    };
  },
  created () {
    if (!this.localView.integrations.length) {
      this.localView.integrations = this.getSelectedIntegrations;
    }
  },
  computed: {
    ...mapGetters([
      'getRoles', 'getUser', 'getDoableIntegrations', 'getSortedIntegrations',
      'getSelectedIntegrations'
    ])
  },
  methods: {
    /* page functions ------------------------------------------------------ */
    toggleAll (checked) {
      this.localView.integrations = checked ? Object.keys(this.getDoableIntegrations) : [];
      this.$emit('update-view', { view: this.localView, index: this.viewIndex });
    },
    /* helpers ------------------------------------------------------------- */
    calculateSelectAll (list) {
      if (list.length === 0) {
        this.allSelected = false;
        this.indeterminate = false;
      } else if (list.length === Object.keys(this.getDoableIntegrations).length) {
        this.allSelected = true;
        this.indeterminate = false;
      } else {
        this.allSelected = false;
        this.indeterminate = true;
      }
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
