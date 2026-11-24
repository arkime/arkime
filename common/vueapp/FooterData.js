/*
Copyright Yahoo Inc.
SPDX-License-Identifier: Apache-2.0
*/
import { commaString } from '@common/vueFilters.js';
import { shownBatches, batchLongest } from './queryTimes.js';

// This is the vue instance logic for the footer template that is returned from the server
// The template is provided as an HTML string constant in FOOTER_CONFIG, used here as "template"
export default {
  // components: the ones the app lets its template use (the viewer's <query-times>)
  getVueInstance (template, store, components = {}) {
    const vueInstance = {
      template,
      components,
      methods: {
        commaString
      },
      computed: {
        // the longest query of the latest search, for templates written before <query-times>
        responseTime () {
          const batch = shownBatches()[0];
          return batch ? batchLongest(batch) : undefined;
        }
      }
    };

    // Add computed properties if store is provided
    if (store) {
      vueInstance.computed.loadingData = () => store.state.loadingData;
    }

    return vueInstance;
  }
};
