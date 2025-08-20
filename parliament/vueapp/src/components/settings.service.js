import { fetchWrapper } from '@common/fetchWrapper.js';

export default {
  saveSettings: async function (settings) {
    return await fetchWrapper({ url: 'api/settings', method: 'PUT', data: { settings } });
  },

  restoreDefaults: async function (type) {
    return await fetchWrapper({ url: 'api/settings/restoreDefaults', method: 'PUT', data: { type } });
  }
};
