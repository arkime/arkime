<template>

  <span>
    <input
      type="text"
      ref="typeahead"
      v-model="value"
      @click="openTypeaheadResults"
      @blur="closeTypeaheadResults"
      @input="filterFields($event.target.value)"
      @keyup.stop="keyup($event)"
      @keydown.down.stop="down"
      @keydown.up.stop="up"
      @keyup.enter.stop="enterClick"
      @keyup.esc.stop="closeTypeaheadResults"
      class="form-control form-control-sm"
      placeholder="Begin typing to search for fields"
    />
    <div class="dropdown-menu field-typeahead"
      :class="{'show':showDropdown}">
      <a v-for="(field, index) in filteredFields"
        :key="field.exp"
        :class="{'active': index === current}"
        class="dropdown-item cursor-pointer"
        @click.stop="changeField(field)">
        {{ field.friendlyName }}
        <small>({{ field.exp }})</small>
      </a>
      <a v-if="!filteredFields || !filteredFields.length"
        class="dropdown-item">
        No fields match your query
      </a>
    </div>
  </span>

</template>

<script>
let inputTimeout;

export default {
  name: 'MolochFieldTypeahead',
  props: {
    fields: {
      type: Array,
      required: true
    },
    initialValue: String,
    queryParam: String
  },
  data: function () {
    return {
      showDropdown: false,
      filteredFields: this.fields,
      value: this.initialValue,
      current: 0 // select first field
    };
  },
  watch: {
    // update the value when the url query param
    'routeParams': function (newVal, oldVal) {
      if (!newVal) { this.value = this.constantInitialVal; }
      for (let field of this.fields) {
        if (field.dbField === newVal) {
          this.value = field.friendlyName;
          continue;
        }
      }
    }
  },
  computed: {
    // compute the route param becuase watcher only accepts dot-delimited paths
    routeParams: function () {
      return this.$route.query[this.queryParam];
    }
  },
  created: function () {
    // save initial value for later use (specifically if the route param
    // is undefined and we need to fill the input with the initial value)
    this.constantInitialVal = this.initialValue;
  },
  methods: {
    /* wait for changeField click event before closing results */
    closeTypeaheadResults: function () {
      setTimeout(() => {
        this.current = 0; // reset
        this.showDropdown = false;
      }, 250);
    },
    openTypeaheadResults: function () {
      this.showDropdown = true;
    },
    filterFields: function (searchFilter) {
      if (inputTimeout) { clearTimeout(inputTimeout); }

      inputTimeout = setTimeout(() => {
        if (!searchFilter) { this.filteredFields = this.fields; }
        this.filteredFields = this.fields.filter((field) => {
          let sfl = searchFilter.toLowerCase();
          return field.friendlyName.toLowerCase().includes(sfl) ||
            field.exp.toLowerCase().includes(sfl) ||
            (field.aliases && field.aliases.some(item => {
              return item.toLowerCase().includes(sfl);
            }));
        });
      }, 250);
    },
    changeField: function (field) {
      this.value = field.friendlyName;
      this.$emit('fieldSelected', field);
    },
    /**
     * selects the active field in the dropdown if it exists
     * then closes the dropdown
     */
    enterClick: function () {
      if (this.showDropdown && this.filteredFields.length) {
        this.changeField(this.filteredFields[this.current]);
      }
      this.closeTypeaheadResults();
    },
    /* shows the dropdown results if the user is typing (except enter/esc) */
    keyup: function (event) {
      if (event.keyCode === 13 || event.keyCode === 27) {
        return;
      }
      this.showDropdown = true;
    },
    /* navigates up through field results */
    up: function () {
      if (this.current > 0) {
        this.current--;
      } else if (this.current === -1) {
        this.current = this.filteredFields.length - 1;
      } else {
        this.current = -1;
      }
    },
    /* navigates down through field results */
    down: function () {
      if (this.current < this.filteredFields.length - 1) {
        this.current++;
      } else {
        this.current = -1;
      }
    }
  }
};
</script>

<style scoped>
.field-typeahead {
  max-height: 300px;
  overflow-y: auto;
}
</style>
