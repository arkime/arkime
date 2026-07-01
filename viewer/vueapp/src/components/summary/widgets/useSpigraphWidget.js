/*
Copyright Yahoo Inc.
SPDX-License-Identifier: Apache-2.0

Shared self-fetch lifecycle for SPIGraph-backed field widgets (heatmap/treemap):
fetches /api/spigraph for the widget on mount, on global reload (nonce), and when
the widget's field/expression/length change; aborts the in-flight request on
change and on unmount. The caller supplies onResult(res) to store the response.
*/
import { ref, watch, onMounted, onBeforeUnmount } from 'vue';
import { useRoute } from 'vue-router';
import { useStore } from 'vuex';
import { fetchSpigraph } from './widgetData';

/**
 * @param {() => object} getWidget       returns the current widget definition
 * @param {() => number} getReloadNonce  returns the dashboard reload nonce
 * @param {(res: object) => void} onResult  handles a successful response
 * @param {(route, store, widget, opts) => Promise<object>} [fetcher]  endpoint
 *        fetcher (defaults to fetchSpigraph; pass fetchHierarchy for Intersection)
 * @returns {{ loading: import('vue').Ref<boolean>, error: import('vue').Ref<string>, fetchData: () => Promise<void> }}
 */
export function useSpigraphWidget (getWidget, getReloadNonce, onResult, fetcher = fetchSpigraph) {
  const route = useRoute();
  const store = useStore();
  const loading = ref(true);
  const error = ref('');
  let controller;

  const fetchData = async () => {
    loading.value = true;
    error.value = '';
    if (controller) { controller.abort(); }
    controller = new AbortController();
    try {
      const res = await fetcher(route, store, getWidget(), { signal: controller.signal });
      loading.value = false;
      onResult(res);
    } catch (err) {
      if (err.name === 'AbortError') { return; }
      error.value = err.text || err.message || String(err);
      loading.value = false;
    }
  };

  watch(() => {
    const w = getWidget();
    return [getReloadNonce(), w.field, (w.fields || []).join(','), w.expression, w.length, w.view, w.metricType, w.order, (w.metrics || []).join(','), w.sortMetric];
  }, fetchData);

  onMounted(fetchData);
  onBeforeUnmount(() => { if (controller) { controller.abort(); } });

  return { loading, error, fetchData };
}
