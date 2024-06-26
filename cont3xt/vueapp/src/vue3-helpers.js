import { computed } from 'vue';

export function useGetters (store) {
  return Object.fromEntries(
    Object.keys(store.getters)
      .map(getterKey => [getterKey, computed(() => store.getters[getterKey])])
  );
}
