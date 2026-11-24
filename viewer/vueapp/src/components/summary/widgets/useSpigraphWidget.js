/*
Copyright Yahoo Inc.
SPDX-License-Identifier: Apache-2.0

Shared self-fetch lifecycle for SPIGraph-backed field widgets (heatmap/treemap):
fetches /api/spigraph for the widget on mount, on global reload (nonce), and when
a setting it is fetched with changes; aborts the in-flight request on
change and on unmount. The caller supplies onResult(res) to store the response.
*/
import { ref, watch, onMounted, onBeforeUnmount } from 'vue';
import { useRoute } from 'vue-router';
import { useStore } from 'vuex';
import { useI18n } from 'vue-i18n';
import { resolveMessage } from '@common/resolveI18nMessage';
import { fetchSpigraph } from './widgetData';

// The settings that only change how a widget is drawn, and what the dashboard
// keeps beside a definition. A change of any other setting fetches again, so
// one missing from here costs a fetch, never stale data
const DRAWN_KEYS = new Set(['id', 'title', 'width', 'height', 'text', 'barMode', 'orientation', 'seriesType', 'yScale', 'gaugeMax', 'gaugeWarn', 'gaugeCrit',
  'sparkline', 'treeRoot', 'treeDepth', 'maxNodes', 'data', 'otherData', 'loading', 'error']);

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
  const { t } = useI18n();
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
      error.value = resolveMessage(err, t) || String(err);
      loading.value = false;
    }
  };

  // a string, so a rebuilt widget with the same settings doesn't fetch again
  watch(() => JSON.stringify([getReloadNonce(), Object.entries(getWidget()).filter(([key]) => !DRAWN_KEYS.has(key))]), fetchData);

  onMounted(fetchData);
  onBeforeUnmount(() => { if (controller) { controller.abort(); } });

  return { loading, error, fetchData };
}
