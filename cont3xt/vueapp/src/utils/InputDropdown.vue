<template>
  <b-dropdown>
    <b-input-group size="sm">
      <b-input-group-text>
        <span class="fa fa-search" />
      </b-input-group-text>
      <b-form-input
          ref="integrationViewDropdownSearch"
          @keydown.enter="selectTopOption"
          v-model="filter"
      />
    </b-input-group>

    <!--  TODO: implement as auto-complete dropdown (for use in OverviewFormCard)  -->
  </b-dropdown>
</template>

<script>
export default {
  name: 'InputDropdown',
  props: {
    options: {
      type: Array,
      required: true
    }
  },
  data () {
    return {
      filter: ''
    };
  },
  computed: {
    filteredOptions () {
      return this.options.filter(option => option.includes(this.filter));
    }
  },
  methods: {
    selectTopOption () {
      if (!this.filteredOptions) { return; }
    }
  }
};
</script>
