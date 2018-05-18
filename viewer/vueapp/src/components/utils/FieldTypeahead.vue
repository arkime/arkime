<template>

  <span>
    <input v-once
      type="text"
      ref="typeahead"
      v-model="value"
      @focus="showDropdown = true"
      @blur="closeTypeaheadResults"
      @input="filterFields($event.target.value)"
      @keydown.enter.stop.prevent
      class="form-control form-control-sm"
      placeholder="Begin typing to search for fields"
    />
    <div class="dropdown-menu field-typeahead"
      :class="{'show':showDropdown && filteredFields && filteredFields.length}">
      <a v-for="field in filteredFields"
        :key="field.exp"
        class="dropdown-item cursor-pointer"
        @click.stop="changeField(field)">
        {{ field.friendlyName }}
        <small>({{ field.exp }})</small>
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
    // TODO enter should select typeahead result
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
      }, 400);
    },
    changeField: function (field) {
      this.value = field.friendlyName;
      this.$refs.typeahead.value = this.value;
      this.$emit('fieldSelected', field);
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
