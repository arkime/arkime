import { commaString } from '@common/vueFilters.js';

// This is the vue instance logic for the footer template that is returned from the server
// The template is provided as an HTML string constant in FOOTER_CONFIG, used here as "template"
export default {
  getVueInstance (template, store) {
    const vueInstance = {
      template,
      methods: {
        commaString
      }
    };

    // Add computed properties if store is provided
    if (store) {
      vueInstance.computed = {
        responseTime () {
          return store.state.responseTime;
        },
        loadingData () {
          return store.state.loadingData;
        }
      };
    }

    return vueInstance;
  }
};
