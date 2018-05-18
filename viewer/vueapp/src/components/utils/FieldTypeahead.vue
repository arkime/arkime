<template>

  <span>
    <input v-once
      type="text"
      ref="typeahead"
      v-model="value"
      @focus="showDropdown = true"
      @blur="closeTypeaheadResults"
      @input="filterFields($event.target.value)"
      @keyup="keyup($event)"
      @keyup.enter.stop="enterClick"
      @keyup.esc.stop="closeTypeaheadResults"
      class="form-control form-control-sm"
      placeholder="Begin typing to search for fields"
    />
    <div class="dropdown-menu field-typeahead"
      :class="{'show':showDropdown}">
      <a v-for="field in filteredFields"
        :key="field.exp"
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
    fields: Array,
    initialValue: String
  },
  data: function () {
    return {
      showDropdown: false,
      filteredFields: this.fields,
      value: this.initialValue
    };
  },
  methods: {
    // TODO up/down arrows should navigate typeahead results
    /* wait for changeField click event before closing results */
    closeTypeaheadResults: function () {
      setTimeout(() => { this.showDropdown = false; }, 250);
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
      this.$refs.typeahead.value = this.value;
      this.$emit('fieldSelected', field);
    },
    /**
     * selects the first field in the dropdown if it exists
     * then closes the dropdown
     */
    enterClick: function () {
      if (this.showDropdown && this.filteredFields.length) {
        this.changeField(this.filteredFields[0]);
      }
      this.closeTypeaheadResults();
    },
    /* shows the dropdown results if the user is typing (except enter/esc) */
    keyup: function (event) {
      if (event.keyCode === 13 || event.keyCode === 27) {
        return;
      }
      this.showDropdown = true;
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
