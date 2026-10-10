<!--
Copyright Yahoo Inc.
SPDX-License-Identifier: Apache-2.0
-->
<template>
  <div class="session-info">
    <div
      v-for="(infoField, index) in infoFieldsClone"
      :key="infoField.dbField + index">
      <div
        v-if="session[infoField.dbField]"
        class="info-field-row">
        <!-- label dropdown menu -->
        <v-menu location="bottom end">
          <template #activator="{ props: activatorProps }">
            <button
              v-bind="activatorProps"
              type="button"
              class="clickable-label me-1">
              {{ infoField.friendlyName }}<v-icon icon="mdi-menu-down" />
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
        <span
          v-if="Array.isArray(session[infoField.dbField])">
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
/* one row per field — give a tiny vertical gap so adjacent rows don't
   smoosh together when the info column gets dense */
.session-info .info-field-row {
  margin-bottom: 4px;
}
/* Field-label dropdown trigger — same visual treatment as the session
   detail's .clickable-label (SessionDetail.vue). */
.session-info button.clickable-label {
  display: inline-block;
  height: 21px;
  background-color: transparent;
  color: rgb(var(--v-theme-foreground));
  font-size: 11px;
  font-weight: 600;
  line-height: 21px;
  padding: 0 0 0 5px;
  border: 1px solid rgb(var(--v-theme-neutral));
  border-radius: 0.25rem;
  cursor: pointer;
  max-width: 220px;
  white-space: nowrap;
  overflow: hidden;
  text-overflow: ellipsis;
  vertical-align: baseline;
}
.session-info button.clickable-label:hover {
  color: #333;
  background-color: rgb(var(--v-theme-neutral));
  border-color: rgb(var(--v-theme-neutral));
}
/* shrink the dropdown chevron to text size and strip its surrounding
   whitespace (default v-icon is a 24px box, taller than the button) */
.session-info button.clickable-label .v-icon {
  font-size: 16px;
  margin: 0;
}
</style>
