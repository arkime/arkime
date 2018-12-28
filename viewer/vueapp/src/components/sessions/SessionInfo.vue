<template>

  <div class="session-info">

    <div v-for="(infoField, index) in infoFieldsClone"
      :key="infoField.dbField + index">
      <div v-if="session[infoField.dbField]">
        <!-- label dropdown menu -->
        <b-dropdown
          right
          size="sm"
          toggle-class="rounded"
          class="field-dropdown"
          variant="default"
          :text="infoField.friendlyName">
          <b-dropdown-item
            @click="exportUnique(infoField.rawField || infoField.exp, 0)">
            Export Unique {{ infoField.friendlyName }}
          </b-dropdown-item>
          <b-dropdown-item
            @click="exportUnique(infoField.rawField || infoField.exp, 1)">
            Export Unique {{ infoField.friendlyName }} with counts
          </b-dropdown-item>
          <template v-if="infoField.portField">
            <b-dropdown-item
              @click="exportUnique(infoField.rawField || infoField.exp + ':' + infoField.portField, 0)">
              Export Unique {{ infoField.friendlyName }}:Ports
            </b-dropdown-item>
            <b-dropdown-item
              @click="exportUnique(infoField.rawField || infoField.exp + ':' + infoField.portField, 1)">
              Export Unique {{ infoField.friendlyName }}:Ports with counts
            </b-dropdown-item>
          </template>
          <b-dropdown-item
            @click="openSpiGraph(infoField.dbField)">
            Open {{ infoField.friendlyName }} in SPI Graph
          </b-dropdown-item>
        </b-dropdown> <!-- /label dropdown menu -->
        <span v-if="Array.isArray(session[infoField.dbField])">
          <span v-for="(value, index) in limitArrayLength(session[infoField.dbField], infoField.limit)"
            :key="value + index">
            <moloch-session-field
              :value="value"
              :session="session"
              :expr="infoField.exp"
              :field="infoField.dbField">
            </moloch-session-field>
          </span>
          <a class="cursor-pointer"
            href="javascript:void(0)"
            style="text-decoration:none;"
            v-if="session[infoField.dbField].length > initialLimit"
            @click="toggleShowAll(infoField)">
            <span v-if="!infoField.showAll">
              more...
            </span>
            <span v-else>
              ...less
            </span>
          </a>
        </span>
        <span v-else>
          <moloch-session-field
            :value="session[infoField.dbField]"
            :session="session"
            :expr="infoField.exp"
            :field="infoField.dbField">
          </moloch-session-field>
        </span>
      </div>
    </div>

  </div>

</template>

<script>
import SessionsService from './SessionsService';

export default {
  name: 'MolochSessionInfo',
  props: [
    'session', // the session object
    'infoFields' // the fields to display as info
  ],
  data: function () {
    return {
      initialLimit: 3,
      infoFieldsClone: JSON.parse(JSON.stringify(this.infoFields))
    };
  },
  watch: {
    infoFields: function (newVal, oldVal) {
      this.infoFieldsClone = JSON.parse(JSON.stringify(newVal));
    }
  },
  methods: {
    /**
     * Sets the limit of the length of the list of session field values so
     * a user can toggle viewing all of the array or just the first few items
     * @param {object} infoField The field to toggle
     */
    toggleShowAll: function (infoField) {
      this.$set(infoField, 'showAll', !infoField.showAll);

      if (infoField.showAll) {
        this.$set(infoField, 'limit', 9999);
      } else {
        this.$set(infoField, 'limit', this.initialLimit);
      }
    },
    /**
     * Limits the length an array of session field values
     * @param {array} array   The array to limit the length of
     * @param {number} length The desired length of the array
     */
    limitArrayLength: function (array, length) {
      if (!length) { length = this.initialLimit; }

      let limitCount = parseInt(length, 10);

      if (limitCount <= 0) { return array; }

      return array.slice(0, limitCount);
    },
    /**
     * Opens the spi graph page in a new browser tab
     * @param {string} fieldID The field id (dbField) to display spi graph data for
     */
    openSpiGraph: function (fieldID) {
      SessionsService.openSpiGraph(fieldID, this.$route.query);
    },
    /**
     * Open a page to view unique values for different fields
     * @param {string} exp    The field to get unique values for
     * @param {number} counts 1 or 0 whether to include counts of the values
     */
    exportUnique: function (exp, counts) {
      SessionsService.exportUniqueValues(exp, counts, this.$route.query);
    }
  }
};
</script>

<style>
/* clickable field labels */
.session-info .btn-group.dropdown.field-dropdown > button {
  margin-top: 1px;
  margin-bottom: 1px;
  padding: 0 4px;
  font-size: .75rem;
  font-weight: 500;
}
</style>
