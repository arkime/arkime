<template>

  <span :class="{'dropup':dropup}">
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
      <a v-for="(field, index) in filteredFieldHistory"
        :key="field.exp+'-history'"
        :class="{'active': index === current,'last-history-item':index === filteredFieldHistory.length-1}"
        class="dropdown-item cursor-pointer"
        @click.stop="changeField(field)">
        <span class="fa fa-history"></span>&nbsp;
        {{ field.friendlyName }}
        <small>({{ field.exp }})</small>
        <span class="fa fa-close pull-right mt-1"
          @click.stop.prevent="removeFromFieldHistory(field)">
        </span>
      </a>
      <div v-if="filteredFieldHistory.length"
        class="dropdown-divider">
      </div>
      <a v-for="(field, index) in filteredFields"
        :key="field.exp"
        :class="{'active':index+filteredFieldHistory.length === current}"
        class="dropdown-item cursor-pointer"
        @click.stop="changeField(field)">
        {{ field.friendlyName }}
        <small>({{ field.exp }})</small>
      </a>
      <a v-if="(!filteredFields || !filteredFields.length)"
        class="dropdown-item">
        No fields match your query
      </a>
    </div>
  </span>

</template>

<script>
import UserService from '../users/UserService';

let inputTimeout;

export default {
  name: 'MolochFieldTypeahead',
  props: {
    fields: {
      type: Array,
      required: true
    },
    initialValue: String,
    queryParam: String,
    page: String,
    dropup: Boolean
  },
  data: function () {
    return {
      showDropdown: false,
      filteredFields: this.fields,
      value: this.initialValue,
      current: 0, // select first field
      fieldHistory: [],
      filteredFieldHistory: []
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

    if (this.page) { // get the field history for this page
      UserService.getState(`fieldHistory${this.page}`)
        .then((response) => {
          this.fieldHistory = response.data.fields || [];
          this.filteredFieldHistory = response.data.fields || [];
        });
    }
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
        this.filteredFields = this.$options.filters.searchFields(searchFilter, this.fields, true);
        this.filteredFieldHistory = this.$options.filters.searchFields(searchFilter, this.fieldHistory, true);
      }, 250);
    },
    changeField: function (field) {
      this.value = field.friendlyName;
      this.addFieldToHistory(field);
      this.$emit('fieldSelected', field);
    },
    /**
     * selects the active field in the dropdown if it exists
     * then closes the dropdown
     */
    enterClick: function () {
      if (this.showDropdown && this.filteredFields.length) {
        let selectedField;
        if (this.current < this.filteredFieldHistory.length) {
          selectedField = this.filteredFieldHistory[this.current];
        } else {
          selectedField = this.filteredFields[this.current - this.filteredFieldHistory.length];
        }
        this.changeField(selectedField);
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
    /* navigates up through field and field history results */
    up: function () {
      this.current = (this.current > 0 ? this.current
        : (this.filteredFieldHistory.length + this.filteredFields.length)) - 1;
    },
    /* navigates down through field and field history results */
    down: function () {
      this.current = (this.current + 1) %
        (this.filteredFieldHistory.length + this.filteredFields.length);
    },
    /**
     * Adds an item to the beginning of the field history
     * @param {object} field The field to add to the history
     */
    addFieldToHistory: function (field) {
      let found = false;

      if (!field) { return found; }

      let index = 0;
      for (let historyField of this.fieldHistory) {
        if (historyField.exp === field.exp) {
          found = true;
          break;
        }
        index++;
      }

      if (found) { // if the field was found, remove it
        this.fieldHistory.splice(index, 1);
      }

      // add the field to the beginning of the list
      this.fieldHistory.unshift(field);

      // if the list is larger than 10 items
      if (this.fieldHistory.length > 10) {
        // remove the last item in the history
        this.fieldHistory.splice(this.fieldHistory.length - 1, 1);
      }

      // save the field history for the user
      UserService.saveState({ fields: this.fieldHistory }, `fieldHistory${this.page}`);

      return found;
    },
    /**
     * Removes an item to the field history (and results)
     * @param {object} field The field to remove from the history
     */
    removeFromFieldHistory: function (field) {
      let index = 0;
      for (let historyField of this.fieldHistory) {
        if (historyField.exp === field.exp) {
          break;
        }
        index++;
      }

      // remove the item from the history
      this.fieldHistory.splice(index, 1);

      // find it in the field history results (displayed in the typeahead)
      index = 0;
      for (let historyField of this.filteredFieldHistory) {
        if (historyField.exp === field.exp) {
          break;
        }
        index++;
      }

      // remove the item from the field history resutls
      this.filteredFieldHistory.splice(index, 1);

      // save the field history for the user
      UserService.saveState({ fields: this.fieldHistory }, `fieldHistory${this.page}`);
    }
  }
};
</script>

<style scoped>
.dropup .field-typeahead {
  margin-bottom: 34px;
}
.field-typeahead {
  max-height: 300px;
  overflow-y: auto;
}
.input-group input {
  border-radius: 0 .2rem .2rem 0;
}
.field-typeahead div.dropdown-divider {
  margin: .15rem 0;
  border-color: var(--color-gray);
}
</style>
