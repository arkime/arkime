<template>
  <Teleport
    to="body"
    :disabled="bigTypeahead">
    <div
      id="typeahead-results"
      class="arkime-dropdown-menu"
      :class="{'big-typeahead-results': bigTypeahead, 'typeahead-results': !bigTypeahead}"
      :style="dropdownStyle"
      v-show="expression && results && results.length">
      <template v-if="autocompletingField">
        <template
          v-for="(value, key) in fieldHistoryResults"
          :key="key + 'history'">
          <a
            :id="key + 'history'"
            class="arkime-dropdown-item cursor-pointer"
            :class="{'active':key === activeIdx,'last-history-item':key === fieldHistoryResults.length - 1}"
            @click="addToQuery(value)">
            <v-icon icon="mdi-history" />&nbsp;
            <strong v-if="value.exp">{{ value.exp }}</strong>
            <strong v-if="!value.exp">{{ value }}</strong>
            <span v-if="value.friendlyName"> - {{ value.friendlyName }}</span>
            <v-icon
              icon="mdi-close"
              class="float-right mt-1"
              :title="`Remove ${value.exp} from your field history`"
              @click.stop.prevent="removeFromFieldHistory(value)" />
            <v-tooltip
              v-if="value.help"
              :activator="`[id='${key}history']`">
              {{ value.help.substring(0, 100) }}
              <span v-if="value.help.length > 100">
                ...
              </span>
            </v-tooltip>
          </a>
        </template>
      </template>
      <template
        v-for="(value, key) in results"
        :key="value + 'item'">
        <a
          :id="key + 'item'"
          class="arkime-dropdown-item cursor-pointer"
          :title="value.help"
          :class="{'active':key + fieldHistoryResults.length === activeIdx}"
          @click="addToQuery(value)">
          <strong v-if="value.exp">{{ value.exp }}</strong>
          <strong v-if="!value.exp">{{ value }}</strong>
          <span v-if="value.friendlyName"> - {{ value.friendlyName }}</span>
          <v-tooltip
            v-if="value.help"
            :activator="`[id='${key}item']`">
            {{ value.help.substring(0, 100) }}
            <span v-if="value.help.length > 100">
              ...
            </span>
          </v-tooltip>
        </a>
      </template>
    </div> <!-- /results dropdown -->
  </Teleport>
</template>

<script setup>

defineProps({
  expression: {
    type: String,
    default: ''
  },
  results: {
    type: Array,
    default: () => []
  },
  activeIdx: {
    type: Number,
    default: -1
  },
  fieldHistoryResults: {
    type: Array,
    default: () => []
  },
  autocompletingField: Boolean,
  addToQuery: {
    type: Function,
    default: () => {}
  },
  removeFromFieldHistory: {
    type: Function,
    default: () => {}
  },
  bigTypeahead: Boolean,
  // Inline position style supplied by the parent typeahead when the
  // dropdown teleports to <body> (escapes any overflow:hidden container
  // like the page navbar). Passes a {position, top, left, width, ...}
  // object computed from the input's bounding rect.
  dropdownStyle: {
    type: Object,
    default: () => ({})
  }
});
</script>

<style>
.typeahead-results {
  top: initial;
  left: initial;
  display: block;
  overflow-y: auto;
  overflow-x: hidden;
  max-height: 500px;
  margin-left: 35px;
}
.big-typeahead-results {
  top: initial;
  left: initial;
  display: block;
  overflow-y: auto;
  overflow-x: hidden;
  max-height: 500px;
}

.typeahead-results a.last-history-item {
  border-bottom: 1px solid rgb(var(--v-theme-neutral));
}

@media screen and (max-height: 600px) {
  .typeahead-results {
    max-height: 250px;
  }
}
</style>
