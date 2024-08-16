import { computed } from 'vue';

export function useGetters (store) {
  return Object.fromEntries(
    Object.keys(store.getters)
      .map(getterKey => [getterKey, computed(() => store.getters[getterKey])])
  );
}

export function escapeSelectorId (id) {
  const escapedSpecialChars = id.replaceAll(/[:.*>~,$|/\\ ]/g, '\\$&');

  return (/^\d/.test(escapedSpecialChars))
    ? `\\3${escapedSpecialChars}` // prepend \3 as a unicode escape when we have a leading number, as id-selectors cannot start with a number
    : escapedSpecialChars;
}

export function idSelector (id) {
  return `#${escapeSelectorId(id)}`;
}
