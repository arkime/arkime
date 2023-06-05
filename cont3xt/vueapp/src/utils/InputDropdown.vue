<template>
  <b-dropdown>
    <b-input-group size="sm">
      <b-input-group-text>
        <span class="fa fa-search" />
      </b-input-group-text>
      <b-form-input
          ref="integrationViewDropdownSearch"
          @keydown.enter="selectTopOption"
          @focusin="setBarFocused"
          @focusout="setBarUnfocused"
          v-model="filter"
      />
    </b-input-group>

<!--    <b-dropdown-item v-for="option in options">-->

<!--    </b-dropdown-item>-->
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
