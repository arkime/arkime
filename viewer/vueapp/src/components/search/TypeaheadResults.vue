<template>
  <div
    id="typeahead-results"
    class="dropdown-menu"
    :class="{'big-typeahead-results': bigTypeahead, 'typeahead-results': !bigTypeahead}"
    v-show="expression && results && results.length">
    <template v-if="autocompletingField">
      <template
        v-for="(value, key) in fieldHistoryResults"
        :key="key + 'history'">
        <a
          :id="key + 'history'"
          class="dropdown-item cursor-pointer"
          :class="{'active':key === activeIdx,'last-history-item':key === fieldHistoryResults.length - 1}"
          @click="addToQuery(value)">
          <span class="fa fa-history" />&nbsp;
          <strong v-if="value.exp">{{ value.exp }}</strong>
          <strong v-if="!value.exp">{{ value }}</strong>
          <span v-if="value.friendlyName">- {{ value.friendlyName }}</span>
          <span
            class="fa fa-close pull-right mt-1"
            :title="`Remove ${value.exp} from your field history`"
            @click.stop.prevent="removeFromFieldHistory(value)" />
          <BTooltip
            v-if="value.help"
            :target="key + 'history'">
            {{ value.help.substring(0, 100) }}
            <span v-if="value.help.length > 100">
              ...
            </span>
          </BTooltip>
        </a>
      </template>
    </template>
    <template
      v-for="(value, key) in results"
      :key="value + 'item'">
      <a
        :id="key + 'item'"
        class="dropdown-item cursor-pointer"
        :title="value.help"
        :class="{'active':key + fieldHistoryResults.length === activeIdx}"
        @click="addToQuery(value)">
        <strong v-if="value.exp">{{ value.exp }}</strong>
        <strong v-if="!value.exp">{{ value }}</strong>
        <span v-if="value.friendlyName">- {{ value.friendlyName }}</span>
        <BTooltip
          v-if="value.help"
          :target="key + 'item'">
          {{ value.help.substring(0, 100) }}
          <span v-if="value.help.length > 100">
            ...
          </span>
        </BTooltip>
      </a>
    </template>
  </div> <!-- /results dropdown -->
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
  bigTypeahead: Boolean
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
  border-bottom: 1px solid var(--color-gray);
}

@media screen and (max-height: 600px) {
  .typeahead-results {
    max-height: 250px;
  }
}
</style>
