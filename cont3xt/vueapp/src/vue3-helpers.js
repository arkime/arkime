import { computed } from 'vue';

export function useGetters (store) {
  return Object.fromEntries(
    Object.keys(store.getters)
      .map(getterKey => [getterKey, computed(() => store.getters[getterKey])])
  );
}

// TODO: toby - maybe a better file location for this?

// prefix any [:.*>~,$| ] characters with backslash to escape them for use in a DOM selector
// TODO: toby - comment better, eg
export function escapeSelectorId (id) {
  return id.replaceAll(/[:.*>~,$| ]/g, '\\$&');
}

export function idSelector (id) {
  return `#${escapeSelectorId(id)}`;
}
