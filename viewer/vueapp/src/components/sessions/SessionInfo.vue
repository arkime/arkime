<!--
Copyright Yahoo Inc.
SPDX-License-Identifier: Apache-2.0
-->
<template>
  <div class="session-info">
    <div
      v-for="(infoField, index) in infoFieldsClone"
      :key="infoField.dbField + index">
      <div v-if="session[infoField.dbField]">
        <!-- label dropdown menu -->
        <v-menu location="bottom end">
          <template #activator="{ props: activatorProps }">
            <button
              v-bind="activatorProps"
              type="button"
              class="btn btn-xs btn-outline-secondary field-info-trigger me-1">
              {{ infoField.friendlyName }}
              <span class="fa fa-caret-down ms-1" />
            </button>
          </template>
          <v-list density="compact">
            <v-list-item @click="exportUnique(infoField.rawField || infoField.exp, 0)">
              {{ $t('sessions.exportUnique', {name: infoField.friendlyName}) }}
            </v-list-item>
            <v-list-item @click="exportUnique(infoField.rawField || infoField.exp, 1)">
              {{ $t('sessions.exportUniqueCounts', {name: infoField.friendlyName}) }}
            </v-list-item>
            <template v-if="infoField.portField">
              <v-list-item @click="exportUnique(infoField.rawField || infoField.exp + ':' + infoField.portField, 0)">
                {{ $t('sessions.exportUniquePort', {name: infoField.friendlyName}) }}
              </v-list-item>
              <v-list-item @click="exportUnique(infoField.rawField || infoField.exp + ':' + infoField.portField, 1)">
                {{ $t('sessions.exportUniquePortCounts', {name: infoField.friendlyName}) }}
              </v-list-item>
            </template>
            <v-list-item @click="openSpiGraph(infoField.dbField)">
              {{ $t('sessions.openSpiGraph', {name: infoField.friendlyName}) }}
            </v-list-item>
          </v-list>
        </v-menu> <!-- /label dropdown menu -->
        <span v-if="Array.isArray(session[infoField.dbField])">
          <span
            v-for="(value, idx) in limitArrayLength(session[infoField.dbField], infoField.limit)"
            :key="value + idx">
            <arkime-session-field
              :value="value"
              :session="session"
              :expr="infoField.exp"
              :field="infoField" />
          </span>
          <a
            class="cursor-pointer"
            href="javascript:void(0)"
            style="text-decoration:none;"
            v-if="session[infoField.dbField].length > initialLimit"
            @click="toggleShowAll(infoField)">
            <span v-if="!infoField.showAll">
              {{ $t('common.more') }}
            </span>
            <span v-else>
              {{ $t('common.less') }}
            </span>
          </a>
        </span>
        <span v-else>
          <arkime-session-field
            :value="session[infoField.dbField]"
            :session="session"
            :expr="infoField.exp"
            :field="infoField" />
        </span>
      </div>
    </div>
  </div>
</template>

<script>
import SessionsService from './SessionsService';

export default {
  name: 'ArkimeSessionInfo',
  props: {
    session: {
      type: Object,
      default: () => ({})
    }, // the session object
    infoFields: {
      type: Array,
      default: () => []
    } // the fields to display as info
  },
  data: function () {
    return {
      initialLimit: 3,
      infoFieldsClone: JSON.parse(JSON.stringify(this.infoFields))
    };
  },
  watch: {
    infoFields: {
      deep: true,
      handler (newVal, oldVal) {
        this.infoFieldsClone = JSON.parse(JSON.stringify(newVal));
      }
    }
  },
  methods: {
    /**
     * Sets the limit of the length of the list of session field values so
     * a user can toggle viewing all of the array or just the first few items
     * @param {object} infoField The field to toggle
     */
    toggleShowAll: function (infoField) {
      infoField.showAll = !infoField.showAll;

      if (infoField.showAll) {
        infoField.limit = 9999;
      } else {
        infoField.limit = this.initialLimit;
      }
    },
    /**
     * Limits the length an array of session field values
     * @param {array} array   The array to limit the length of
     * @param {number} len The desired length of the array
     */
    limitArrayLength: function (array, len) {
      if (!len) { len = this.initialLimit; }

      const limitCount = parseInt(len, 10);

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

<style scoped>
/* clickable field labels — small, outlined, sits inline with field values */
.session-info .field-info-trigger {
  margin-top: 1px;
  margin-bottom: 1px;
  padding: 0 6px;
  font-size: 0.75rem;
  font-weight: 500;
  line-height: 1.4;
}
</style>
