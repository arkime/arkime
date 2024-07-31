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
  const escapedSpecialChars = id.replaceAll(/[:.*>~,$| ]/g, '\\$&');

  return (/^\d/.test(escapedSpecialChars))
    ? `\\3${escapedSpecialChars}` // prepend \3 as a unicode escape when we have a leading number, as id-selectors cannot start with a number
    : escapedSpecialChars;
}

export function idSelector (id) {
  return `#${escapeSelectorId(id)}`;
}
